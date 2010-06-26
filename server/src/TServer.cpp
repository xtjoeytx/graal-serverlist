#include <stdlib.h>
#include <time.h>

#include "main.h"
#include "TServer.h"
#include "TPlayer.h"
#include "CLog.h"
#include "FProfile.h"
#include "CFileSystem.h"

#ifndef NO_MYSQL
	extern CMySQL *mySQL;
#endif
extern CSettings *settings;
extern std::vector<TServer *> serverList;
extern CLog serverlog;
extern std::vector<CString> serverTypes;
extern CFileSystem filesystem[5];

/*
	Pointer-Functions for Packets
*/
std::vector<TSVSock> svfunc;

void createSVFunctions()
{
	// kinda like a memset-ish thing y'know
	for (int i = 0; i < 26; i++)
		svfunc.push_back(&TServer::msgSVI_NULL);

	// now set non-nulls
	svfunc[SVI_SETNAME] = &TServer::msgSVI_SETNAME;
	svfunc[SVI_SETDESC] = &TServer::msgSVI_SETDESC;
	svfunc[SVI_SETLANG] = &TServer::msgSVI_SETLANG;
	svfunc[SVI_SETVERS] = &TServer::msgSVI_SETVERS;
	svfunc[SVI_SETURL]  = &TServer::msgSVI_SETURL;
	svfunc[SVI_SETIP]   = &TServer::msgSVI_SETIP;
	svfunc[SVI_SETPORT] = &TServer::msgSVI_SETPORT;
	svfunc[SVI_SETPLYR] = &TServer::msgSVI_SETPLYR;
	svfunc[SVI_VERIACC] = &TServer::msgSVI_VERIACC;
	svfunc[SVI_VERIGLD] = &TServer::msgSVI_VERIGLD;
	svfunc[SVI_GETFILE] = &TServer::msgSVI_GETFILE;
	svfunc[SVI_NICKNAME] = &TServer::msgSVI_NICKNAME;
	svfunc[SVI_GETPROF] = &TServer::msgSVI_GETPROF;
	svfunc[SVI_SETPROF] = &TServer::msgSVI_SETPROF;
	svfunc[SVI_PLYRADD] = &TServer::msgSVI_PLYRADD;
	svfunc[SVI_PLYRREM] = &TServer::msgSVI_PLYRREM;
	svfunc[SVI_SVRPING] = &TServer::msgSVI_SVRPING;
	svfunc[SVI_VERIACC2] = &TServer::msgSVI_VERIACC2;
	svfunc[SVI_SETLOCALIP] = &TServer::msgSVI_SETLOCALIP;
	svfunc[SVI_GETFILE2] = &TServer::msgSVI_GETFILE2;
	svfunc[SVI_UPDATEFILE] = &TServer::msgSVI_UPDATEFILE;
	svfunc[SVI_GETFILE3] = &TServer::msgSVI_GETFILE3;
	svfunc[SVI_NEWSERVER] = &TServer::msgSVI_NEWSERVER;
	svfunc[SVI_SERVERHQPASS] = &TServer::msgSVI_SERVERHQPASS;
	svfunc[SVI_SERVERHQLEVEL] = &TServer::msgSVI_SERVERHQLEVEL;
	svfunc[SVI_SERVERINFO] = &TServer::msgSVI_SERVERINFO;
}

/*
	Constructor - Deconstructor
*/
TServer::TServer(CSocket *pSocket)
: sock(pSocket), addedToSQL(false), isServerHQ(false),
serverhq_level(1), server_version(VERSION_1)
{
	language = "English";
	lastPing = lastPlayerCount = lastData = lastUptimeCheck = time(0);
}

TServer::~TServer()
{
	// clean playerlist
	for (unsigned int i = 0; i < playerList.size(); i++)
		delete playerList[i];
	playerList.clear();

#ifndef NO_MYSQL
	// Update our uptime.
	if (isServerHQ)
	{
		int uptime = (int)difftime(time(0), lastUptimeCheck);
		CString query;
		query << "UPDATE `" << settings->getStr("serverhq") << "` SET uptime=uptime+" << CString((int)uptime) << " WHERE name='" << name.escape() << "'";
		mySQL->query(query);
	}
#endif

	// Delete server from SQL serverlist.
#ifndef NO_MYSQL
	CString query;
	query << "DELETE FROM " << settings->getStr("serverlist") << " WHERE name='" << name.escape() << "'";
	mySQL->query(query);
#endif

	// delete socket
	delete sock;
}

/*
	Loops
*/
bool TServer::doMain()
{
	// sock exist?
	if (sock == NULL)
		return false;

	// definitions
	CString line, unBuffer;
	int lineEnd;//, size;

	// receive
	if ( sock->getData() == -1 )
		return false;

	// append new data
	sockBuffer.write( sock->getBuffer().text(), sock->getBuffer().length() );
	sock->getBuffer().clear();

	if (!sockBuffer.length())
		return true;

	// parse data
	if ((lineEnd = sockBuffer.findl('\n')) == -1)
		return true;

	line = sockBuffer.subString(0, lineEnd + 1);
	sockBuffer.removeI(0, line.length());

	std::vector<CString> lines = line.tokenize("\n");
	for (unsigned int i = 0; i < lines.size(); i++)
	{
		if (!parsePacket(lines[i]))
			return false;

		// Update the data timeout.
		lastData = time(0);
	}

	// Send a ping every 30 seconds.
	if ( (int)difftime( time(0), lastPing ) >= 30 )
	{
		lastPing = time(0);
		sendPacket( CString() >> (char)SVO_PING );
	}

#ifndef NO_MYSQL
	// Update our uptime every 3 minutes.
	if ((int)difftime(time(0), lastUptimeCheck) >= 180)
	{
		int uptime = (int)difftime(time(0), lastUptimeCheck);
		lastUptimeCheck = time(0);
		CString query;
		query << "UPDATE `" << settings->getStr("serverlist") << "`";
		query << " SET uptime=uptime+" << CString((int)uptime) << " WHERE name='" << name.escape() << "'";
		mySQL->query(query);
		if (isServerHQ)
		{
			query.clear();
			query << "UPDATE `" << settings->getStr("serverhq") << "`";
			query << " SET uptime=uptime+" << CString((int)uptime) << " WHERE name='" << name.escape() << "'";
			mySQL->query(query);
		}
	}
#endif

	// send out buffer
	sendCompress();
	return true;
}

/*
	Kill Client
*/
void TServer::kill()
{
	// Send Out-Buffer
	sendCompress();

	// Delete
	std::vector<TServer*>::iterator iter = std::find(serverList.begin(), serverList.end(), this);
	if (iter != serverList.end()) *iter = 0;
	delete this;
}

void TServer::SQLupdate(CString tblval, const CString& newVal)
{
#ifndef NO_MYSQL
	if (!addedToSQL) return;
	CString query;
	query << "UPDATE `" << settings->getStr("serverlist") << "` SET "
		<< tblval.escape() << "='" << newVal.escape() << "' "
		<< "WHERE name='" << name.escape() << "'";
	mySQL->query(query);
#endif
}

void TServer::SQLupdateHQ(CString tblval, const CString& newVal)
{
#ifndef NO_MYSQL
	CString query;
	query << "UPDATE `" << settings->getStr("serverhq") << "` SET "
		<< tblval.escape() << "='" << newVal.escape() << "' "
		<< "WHERE name='" << name.escape() << "'";
	mySQL->query(query);
#endif
}

void TServer::updatePlayers()
{
#ifndef NO_MYSQL
	// Set our lastconnected.
	CString query = CString() << "UPDATE `" << settings->getStr("serverlist") << "` SET lastconnected=NOW() " << "WHERE name='" << name.escape() << "'";
	mySQL->query(query);
	if (isServerHQ)
	{
		query = CString() << "UPDATE `" << settings->getStr("serverhq") << "` SET lastconnected=NOW() " << "WHERE name='" << name.escape() << "'";
		mySQL->query(query);
	}

	// Update our playercount.
	SQLupdate("playercount", CString((int)playerList.size()));

	// Update our player list.
	CString playerlist;
	for (std::vector<player*>::iterator i = playerList.begin(); i != playerList.end(); ++i)
	{
		int ANY_CLIENT = (int)(1 << 0) | (int)(1 << 4) | (int)(1 << 5);
		player* p = *i;
		if ((p->type & ANY_CLIENT) != 0) playerlist << (*i)->account << ",";
	}
	SQLupdate("playerlist", playerlist);

	// Check to see if we can increase our maxplayers.
	std::vector<CString> result;
	query = CString() << "SELECT maxplayers FROM `" << settings->getStr("serverlist") << "` WHERE name='" << name.escape() << "' LIMIT 1";
	mySQL->query(query, &result);

	// If we got no maxplayers back, wtf.
	if (result.size() == 0) return;

	// Check if we can increase our maxplayers.
	int maxplayers = strtoint(result[0]);
	if (playerList.size() > (unsigned int)maxplayers)
	{
		SQLupdate("maxplayers", CString((int)playerList.size()));
		if (isServerHQ) SQLupdateHQ("maxplayers", CString((int)playerList.size()));
	}
#endif
}

/*
	Get-Value Functions
*/
const CString& TServer::getDescription()
{
	return description;
}

const CString TServer::getIp(const CString& pIp)
{
	if (pIp == ip)
	{
		if (localip.length() != 0) return localip;
		return "127.0.0.1";
	}
	return ip;
}

const CString& TServer::getLanguage()
{
	return language;
}

const CString& TServer::getName()
{
	return name;
}

const int TServer::getPCount()
{
	return playerList.size();
}

const CString& TServer::getPort()
{
	return port;
}

const CString TServer::getType(int PLVER)
{
	CString ret;
	switch (serverhq_level)
	{
		case TYPE_3D:
			ret = "3 ";
			break;
		case TYPE_GOLD:
			ret = "P ";
			break;
		case TYPE_SILVER:
			break;
		case TYPE_BRONZE:
			ret = "H ";
			break;
		case TYPE_HIDDEN:
			ret = "U ";
			break;
	}
	if (PLVER == PLV_PRE22 && serverhq_level == TYPE_BRONZE)
		ret.clear();

	return ret;
}

const CString& TServer::getUrl()
{
	return url;
}

const CString& TServer::getVersion()
{
	return version;
}

const CString TServer::getServerPacket(int PLVER, const CString& pIp)
{
	CString testIp = getIp(pIp);
	CString pcount((int)playerList.size());
	return CString() >> (char)8 >> (char)(getType(PLVER).length() + getName().length()) << getType(PLVER) << getName() >> (char)getLanguage().length() << getLanguage() >> (char)getDescription().length() << getDescription() >> (char)getUrl().length() << getUrl() >> (char)getVersion().length() << getVersion() >> (char)pcount.length() << pcount >> (char)testIp.length() << testIp >> (char)getPort().length() << getPort();
}

/*
	Send-Packet Functions
*/
void TServer::sendCompress()
{
	// empty buffer?
	if (sendBuffer.isEmpty())
	{
		// If we still have some data in the out buffer, try sending it again.
		if (outBuffer.isEmpty() == false)
			sock->sendData(outBuffer);
		return;
	}

	// add the send buffer to the out buffer
	outBuffer << sendBuffer;

	// send buffer
	sock->sendData(outBuffer);

	// clear buffer
	sendBuffer.clear();
}

void TServer::sendPacket(CString pPacket)
{
	// empty buffer?
	if (pPacket.isEmpty())
		return;

	// append '\n'
	if (pPacket[pPacket.length()-1] != '\n')
		pPacket.writeChar('\n');

	// append buffer
	sendBuffer.write(pPacket);
}

/*
	Packet-Functions
*/
bool TServer::parsePacket(CString& pPacket)
{
	// read id & packet
	unsigned char id = pPacket.readGUChar();

	// id exists?
	if (id >= (unsigned char)svfunc.size())
		return true;

	// valid packet, call function
	return (*this.*svfunc[id])(pPacket);
}

bool TServer::msgSVI_NULL(CString& pPacket)
{
	pPacket.setRead(0);
	serverlog.out("Unknown Server Packet: %i (%s)\n", pPacket.readGUChar(), pPacket.text()+1);
	return true;
}

bool TServer::msgSVI_SETNAME(CString& pPacket)
{
	CString oldName(name);
	name = pPacket.readString("");

	// Remove all server type strings from the name of the server.
	while (name.subString(0, 2) == "U " || name.subString(0, 2) == "P " || name.subString(0, 2) == "H " || name.subString(0, 2) == "3 ")
		name.removeI(0, 2);

	// Check ServerHQ if we can use this name.
#ifndef NO_MYSQL
	CString query;
	std::vector<CString> result;
	query = CString() << "SELECT activated FROM `" << settings->getStr("serverhq") << "` WHERE name='" << name.escape() << "' LIMIT 1";
	mySQL->query(query, &result);
	if (result.size() != 0)
	{
		result.clear();
		query = CString() << "SELECT activated FROM `" << settings->getStr("serverhq") << "` WHERE name='" << name.escape() << "' AND activated='1' AND password=" << "MD5(CONCAT(MD5('" << serverhq_pass.escape() << "'), `salt`)) LIMIT 1";
		mySQL->query(query, &result);
		if (result.size() == 0)
			name << " (unofficial)";
	}
#endif

	// check for duplicates
	bool dupFound = false;
	int dupCount = 0;
dupCheck:
	for (unsigned int i = 0; i < serverList.size(); i++)
	{
		if (serverList[i] == 0) continue;
		if (serverList[i] != this && serverList[i]->getName() == name)
		{
			TServer* server = serverList[i];

			// If the IP addresses are the same, something happened and this server is reconnecting.
			// Delete the old server.
			if (server->getSock() == 0 || strcmp(server->getSock()->tcpIp(), sock->tcpIp()) == 0)
			{
				name = server->getName();
				server->kill();
				i = serverList.size();
			}
			else
			{
				// Duplicate found.  Append a random number to the end and start over.
				// Do this a max of 4 times.  If after 4 tries, fail.
				if (dupCount++ < 5)
				{
					name << CString(rand() % 9);
					goto dupCheck;
				}
				sendPacket(CString() >> (char)SVO_ERRMSG << "Servername is already in use!");
				dupFound = true;
				break;
			}
		}
	}

	// In a worst case scenario, attempt to see if we originally had a valid name.
	// If so, just go back to it.
	if (dupFound && addedToSQL)
	{
		name = oldName;
		return true;
	}
	else if (dupFound)
		return false;

	// If we aren't in SQL yet, that means we are a new server.  If so, announce it.
	// If we are in SQL, update the SQL entry with our new name.
	if (!addedToSQL) serverlog.out(CString() << "New Server: " << name << "\n");
	else
	{
		SQLupdate("name", name);
	}

	return true;
}

bool TServer::msgSVI_SETDESC(CString& pPacket)
{
	description = pPacket.readString("");
	SQLupdate("description", description);
	return true;
}

bool TServer::msgSVI_SETLANG(CString& pPacket)
{
	language = pPacket.readString("");
	SQLupdate("language", language);
	return true;
}

bool TServer::msgSVI_SETVERS(CString& pPacket)
{
	CString ver(pPacket.readString(""));
	switch (server_version)
	{
		case VERSION_1:
		{
			int verNum = atoi(ver.text());
			if (verNum == 0)
				version = CString("Custom build: ") << ver;
			else if (verNum <= 52)
				version = CString("Revision ") << CString(abs(verNum));
			else
				version = CString("Build ") << CString(verNum);
			break;
		}

		case VERSION_2:
		{
			if (ver.match("?.??.?") || ver.match("?.?.?"))
				version = CString("Version: ") << ver;
			else if (ver.find("SVN") != -1)
			{
				CString ver2 = ver.removeAll("SVN").trim();
				if (ver2.match("?.??.?") || ver2.match("?.?.?"))
					version = CString("SVN version: ") << ver2;
				else
					version = CString("Custom version: ") << ver;
			}
			else
				version = CString("Custom version: ") << ver;
			break;
		}
	}
	SQLupdate("version", version);
	return true;
}

bool TServer::msgSVI_SETURL(CString& pPacket)
{
	url = pPacket.readString("");
	SQLupdate("url", url);
	return true;
}

bool TServer::msgSVI_SETIP(CString& pPacket)
{
	ip = pPacket.readString("");
	ip = (ip == "AUTO" ? sock->tcpIp() : ip);
	SQLupdate("ip", ip);
	return true;
}

bool TServer::msgSVI_SETPORT(CString& pPacket)
{
	port = pPacket.readString("");

	// This should be the last packet sent when the server is initialized.
	// Add it to the database now.
#ifndef NO_MYSQL
	if (!addedToSQL)
	{
		if (name.length() > 0)
		{
			CString query;
			query << "INSERT INTO " << settings->getStr("serverlist") << " ";
			query << "(name, ip, port, playercount, description, url, language, type, version) ";
			query << "VALUES ('"
				<< name.escape() << "','"
				<< ip.escape() << "','"
				<< port.escape() << "','"
				<< CString((int)playerList.size()) << "','"
				<< description.escape() << "','"
				<< url.escape() << "','"
				<< language.escape() << "','"
				<< getType(4).escape() << "','"
				<< version.escape() << "'"
				<< ")";
			mySQL->query(query);
			addedToSQL = true;
		}
	}
	else SQLupdate("port", port);
#endif

	return true;
}

bool TServer::msgSVI_SETPLYR(CString& pPacket)
{
	// clear list
	for (unsigned int i = 0; i < playerList.size(); i++)
		delete playerList[i];
	playerList.clear();

	// grab new playercount
	unsigned int count = pPacket.readGUChar();

	// remake list
	for (unsigned int i = 0; i < count; i++)
	{
		player *pl = new player();
			pl->account = pPacket.readChars(pPacket.readGUChar());
			pl->nick = pPacket.readChars(pPacket.readGUChar());
			pl->level = pPacket.readChars(pPacket.readGUChar());
			pl->x = (float)pPacket.readGChar() / 2.0f;
			pl->y = (float)pPacket.readGChar() / 2.0f;
			pl->ap = pPacket.readGUChar();
			pl->type = pPacket.readGUChar();
		playerList.push_back(pl);
	}

	// Update the database.
	updatePlayers();

	return true;
}

bool TServer::msgSVI_VERIACC(CString& pPacket)
{
	// definitions
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString password = pPacket.readChars(pPacket.readGUChar());

	// Get the login return code.
	// This will overwrite "account" with the correct case sensitive account name.
	int ret = verifyAccount(account, password, true);

	// send verification
	sendPacket(CString() >> (char)SVO_VERIACC >> (char)account.length() << account << getAccountError(ret));
	return true;
}

bool TServer::msgSVI_VERIGLD(CString& pPacket)
{
	unsigned short playerid = pPacket.readGUShort();
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString nickname = pPacket.readChars(pPacket.readGUChar());
	CString guild = pPacket.readChars(pPacket.readGUChar());

	if (verifyGuild(account, nickname, guild) == GUILDSTAT_ALLOWED)
	{
		nickname << " (" << guild << ")";
		sendPacket(CString() >> (char)SVO_VERIGLD >> (short)playerid >> (char)nickname.length() << nickname);
	}

	return true;
}

bool TServer::msgSVI_GETFILE(CString& pPacket)
{
	unsigned short pId = pPacket.readGUShort();
	unsigned char pTy = pPacket.readGUChar();
	CString shortName = pPacket.readChars(pPacket.readGUChar());
	CFileSystem* fileSystem = &(filesystem[0]);
	switch (pTy)
	{
		case 0: fileSystem = &(filesystem[1]); break;
		case 1: fileSystem = &(filesystem[2]); break;
		case 2: fileSystem = &(filesystem[3]); break;
		case 3: fileSystem = &(filesystem[4]); break;
	}
	CString fileData = fileSystem->load(shortName);

	if (fileData.length() != 0)
	{
		sendPacket(CString() >> (char)SVO_FILESTART >> (char)shortName.length() << shortName << "\n");
		fileData = fileData.B64_Encode();

		while (fileData.length() > 0)
		{
			CString temp = fileData.subString(0, (fileData.length() > 0x10000 ? 0x10000 : fileData.length()));
			fileData.removeI(0, temp.length());
			sendPacket(CString() >> (char)SVO_FILEDATA >> (char)shortName.length() << shortName << temp << "\n");
		}

		sendPacket(CString() >> (char)SVO_FILEEND >> (char)shortName.length() << shortName >> (short)pId >> (char)pTy << "\n");
	}

	return true;
}

bool TServer::msgSVI_NICKNAME(CString& pPacket)
{
	CString accountname = pPacket.readChars( pPacket.readGUChar() );
	CString nickname = pPacket.readChars( pPacket.readGUChar() );

	// Find the player and adjust his nickname.
	for ( unsigned int i = 0; i < playerList.size(); ++i )
	{
		player* pl = playerList[i];
		if ( pl->account == accountname )
			pl->nick = nickname;
	}

	return true;
}

bool TServer::msgSVI_GETPROF(CString& pPacket)
{
	unsigned short playerid = pPacket.readGUShort();

	// Fix old gservers that sent an incorrect packet.
	pPacket.readGUChar();

	// Read the account name.
	CString accountName = pPacket.readString("");

	CString replyPacket;
	if ( getProfile( accountName, replyPacket ) )
		sendPacket(CString() >> (char)SVO_PROFILE >> (short)playerid >> (char)accountName.length() << accountName << replyPacket);
	return true;
}

bool TServer::msgSVI_SETPROF(CString& pPacket)
{
	// Fix old gservers that sent an incorrect packet.
	pPacket.readGUChar();

	// Read the account name in.
	CString accountName = pPacket.readChars( pPacket.readGUChar() );

	// Set profile.
	for (unsigned int i = 0; i < playerList.size(); i++)
	{
		player* pl = playerList[i];
		if ( pl->account == accountName )
		{
			setProfile(accountName, pPacket);
			break;
		}
	}
	return true;
}

bool TServer::msgSVI_PLYRADD(CString& pPacket)
{
	player *pl = new player();
		pl->account = pPacket.readChars(pPacket.readGUChar());
		pl->nick = pPacket.readChars(pPacket.readGUChar());
		pl->level = pPacket.readChars(pPacket.readGUChar());
		pl->x = (float)pPacket.readGChar() / 2.0f;
		pl->y = (float)pPacket.readGChar() / 2.0f;
		pl->ap = pPacket.readGUChar();
		pl->type = pPacket.readGUChar();
	playerList.push_back(pl);

	// Update the database.
	updatePlayers();

	return true;
}

bool TServer::msgSVI_PLYRREM(CString& pPacket)
{
	if (playerList.size() == 0)
		return true;

	unsigned char type = pPacket.readGUChar();
	CString accountname = pPacket.readString("");

	// Find the player and remove him.
	for (std::vector<player*>::iterator i = playerList.begin(); i != playerList.end(); )
	{
		player* pl = *i;
		if (pl->account == accountname && pl->type == type)
		{
			delete pl;
			i = playerList.erase(i);
		}
		else
			++i;
	}

	// Update the database.
	updatePlayers();

	return true;
}

bool TServer::msgSVI_SVRPING(CString& pPacket)
{
	// Do nothing.  It is just a ping.  --  1 per minute.
	return true;
}

// Secure login password:
//	{transaction}{CHAR \xa7}{password}
bool TServer::msgSVI_VERIACC2(CString& pPacket)
{
	// definitions
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString password = pPacket.readChars(pPacket.readGUChar());
	unsigned short id = pPacket.readGUShort();
	unsigned char type = pPacket.readGUChar();

	// Verify the account.
	int ret = verifyAccount(account, password, true);

	// send verification
	sendPacket(CString() >> (char)SVO_VERIACC2
		>> (char)account.length() << account
		>> (short)id >> (char)type
		<< getAccountError(ret));

	return true;
}

bool TServer::msgSVI_SETLOCALIP(CString& pPacket)
{
	localip = pPacket.readString("");
	return true;
}

bool TServer::msgSVI_GETFILE2(CString& pPacket)
{
	unsigned short pId = pPacket.readGUShort();
	unsigned char pTy = pPacket.readGUChar();
	CString shortName = pPacket.readChars(pPacket.readGUChar());
	CFileSystem* fileSystem = &(filesystem[0]);
	switch (pTy)
	{
		case 0: fileSystem = &(filesystem[1]); break;
		case 1: fileSystem = &(filesystem[2]); break;
		case 2: fileSystem = &(filesystem[3]); break;
		case 3: fileSystem = &(filesystem[4]); break;
	}
	CString fileData = fileSystem->load(shortName);
	time_t modTime = fileSystem->getModTime(shortName);

	if (fileData.length() != 0)
	{
		int packetLength = 1 + 1 + shortName.length() + 1;

		// Tell the server that it should expect a file.
		sendPacket(CString() >> (char)SVO_FILESTART2 << shortName);

		// Save the file length.
		int fileLength = fileData.length();

		// Compress the file.
		// Don't compress .png files since they are already zlib compressed.
		CString ext = shortName.subString(shortName.length() - 4, 4);
		char doCompress = 0;
		if (ext.comparei(".png") == false)
		{
			doCompress = 1;
			fileData.zcompressI();
		}

		// Send the file to the server.
		while (fileData.length() != 0)
		{
			int sendSize = clip(32000, 0, fileData.length());
			sendPacket(CString() >> (char)SVO_RAWDATA >> (int)(packetLength + sendSize));
			sendPacket(CString() >> (char)SVO_FILEDATA2 >> (char)shortName.length() << shortName << fileData.subString(0, sendSize));
			fileData.removeI(0, sendSize);
		}

		// Tell the gserver that the file send is now finished.
		sendPacket(CString() >> (char)SVO_FILEEND2 >> (short)pId >> (char)pTy >> (char)doCompress >> (long long)modTime >> (long long)fileLength << shortName);
	}

	return true;
}

bool TServer::msgSVI_UPDATEFILE(CString& pPacket)
{
	time_t modTime = pPacket.readGUInt5();
	unsigned char pTy = pPacket.readGUChar();
	CString file = pPacket.readString("");
	CFileSystem* fileSystem = &(filesystem[0]);
	switch (pTy)
	{
		case 0: fileSystem = &(filesystem[1]); break;
		case 1: fileSystem = &(filesystem[2]); break;
		case 2: fileSystem = &(filesystem[3]); break;
		case 3: fileSystem = &(filesystem[4]); break;
	}

	time_t modTime2 = fileSystem->getModTime(file);
	if (modTime2 != modTime)
	{
		return msgSVI_GETFILE3(CString() >> (short)0 >> (char)pTy >> (char)file.length() << file);
	}
	return true;
}

// Sigh.  A third one.  Just to add a single byte to the start and data packets.
bool TServer::msgSVI_GETFILE3(CString& pPacket)
{
	unsigned short pId = pPacket.readGUShort();
	unsigned char pTy = pPacket.readGUChar();
	CString shortName = pPacket.readChars(pPacket.readGUChar());
	CFileSystem* fileSystem = &(filesystem[0]);
	switch (pTy)
	{
		case 0: fileSystem = &(filesystem[1]); break;
		case 1: fileSystem = &(filesystem[2]); break;
		case 2: fileSystem = &(filesystem[3]); break;
		case 3: fileSystem = &(filesystem[4]); break;
	}
	CString fileData = fileSystem->load(shortName);
	time_t modTime = fileSystem->getModTime(shortName);

	if (fileData.length() != 0)
	{
		int packetLength = 1 + 1 + 1 + shortName.length() + 1;

		// Tell the server that it should expect a file.
		sendPacket(CString() >> (char)SVO_FILESTART3 >> (char)pTy >> (char)shortName.length() << shortName);

		// Save the file length.
		int fileLength = fileData.length();

		// Compress the file.
		// Don't compress .png files since they are already zlib compressed.
		CString ext = shortName.subString(shortName.length() - 4, 4);
		char doCompress = 0;
		if (ext.comparei(".png") == false)
		{
			doCompress = 1;
			fileData.zcompressI();
		}

		// Send the file to the server.
		while (fileData.length() != 0)
		{
			int sendSize = clip(32000, 0, fileData.length());
			sendPacket(CString() >> (char)SVO_RAWDATA >> (int)(packetLength + sendSize));
			sendPacket(CString() >> (char)SVO_FILEDATA3 >> (char)pTy >> (char)shortName.length() << shortName << fileData.subString(0, sendSize));
			fileData.removeI(0, sendSize);
		}

		// Tell the gserver that the file send is now finished.
		sendPacket(CString() >> (char)SVO_FILEEND3 >> (short)pId >> (char)pTy >> (char)doCompress >> (long long)modTime >> (long long)fileLength << shortName);
	}

	return true;
}

bool TServer::msgSVI_NEWSERVER(CString& pPacket)
{
	server_version = VERSION_2;

	CString name = pPacket.readChars(pPacket.readGUChar());
	CString description = pPacket.readChars(pPacket.readGUChar());
	CString language = pPacket.readChars(pPacket.readGUChar());
	CString version = pPacket.readChars(pPacket.readGUChar());
	CString url = pPacket.readChars(pPacket.readGUChar());
	CString ip = pPacket.readChars(pPacket.readGUChar());
	CString port = pPacket.readChars(pPacket.readGUChar());
	CString localip = pPacket.readChars(pPacket.readGUChar());

	// Set up the server.
	if (msgSVI_SETNAME(name) == false) return false;
	msgSVI_SETDESC(description);
	msgSVI_SETLANG(language);
	msgSVI_SETVERS(version);
	msgSVI_SETURL(url);
	msgSVI_SETIP(ip);
	msgSVI_SETLOCALIP(localip);
	msgSVI_SETPORT(port);			// Port last.

	return true;
}

bool TServer::msgSVI_SERVERHQPASS(CString& pPacket)
{
	serverhq_pass = pPacket.readString("");
	return true;
}

bool TServer::msgSVI_SERVERHQLEVEL(CString& pPacket)
{
	serverhq_level = pPacket.readGUChar();

#ifndef NO_MYSQL
	// Ask what our max level and max players is.
	CString query;
	std::vector<CString> result;
	query = CString() << "SELECT maxplayers, uptime, maxlevel FROM `" << settings->getStr("serverhq") << "` WHERE name='" << name.escape() << "' AND activated='1' AND password=" << "MD5(CONCAT(MD5('" << serverhq_pass.escape() << "'), `salt`)) LIMIT 1";
	mySQL->query(query, &result);

	// If the password was wrong, limit ourselves to the bronze tab.
	if (result.size() == 0)
	{
		if (serverhq_level != 0)
			serverhq_level = 1;
	}
	else isServerHQ = true;

	// Limit ourselves to our max level.
	if (isServerHQ)
	{
		int maxlevel = strtoint(result[2]);
		if (serverhq_level > maxlevel) serverhq_level = maxlevel;

		// Update our uptime.
		if (result.size() > 1)
		{
			// Update our uptime.
			CString query2;
			query2 << "UPDATE `" << settings->getStr("serverlist") << "` SET uptime=" << result[1] << " WHERE name='" << name.escape() << "'";
			mySQL->query(query2);
		}

		// If we got max players, update the graal_servers table.
		if (result.size() != 0)
		{
			int maxplayers = strtoint(result[0]);

			// Check to see if the graal_servers table has a larger max players.
			std::vector<CString> result2;
			CString query2 = CString() << "SELECT maxplayers FROM `" << settings->getStr("serverlist") << "` WHERE name='" << name.escape() << "' LIMIT 1";
			mySQL->query(query2, &result2);
			if (result2.size() != 0)
			{
				int s_maxp = strtoint(result2[0]);
				if (maxplayers > s_maxp) SQLupdate("maxplayers", CString((int)maxplayers));
				else SQLupdateHQ("maxplayers", CString((int)s_maxp));
			}
			else SQLupdate("maxplayers", CString((int)maxplayers));
		}
	}

	// Update our current level.
	SQLupdate("type", getType(4));
	if (isServerHQ) SQLupdateHQ("curlevel", CString((int)serverhq_level));
#endif

	return true;
}

bool TServer::msgSVI_SERVERINFO(CString& pPacket)
{
	int pid = pPacket.readGUShort();
	CString servername = pPacket.readString("");

	int id = 0;
	for (std::vector<TServer*>::iterator i = serverList.begin(); i != serverList.end(); ++i)
	{
		TServer* server = *i;
		if (server == 0) continue;
		if (servername.comparei(server->getName()))
		{
			sendPacket(CString() >> (char)SVO_SERVERINFO >> (short)pid << "playerworld" << CString((int)id) << ",\"" << server->getName() << "\"," << server->getIp() << "," << server->getPort());
			return true;
		}
		++id;
	}

	return true;
}
