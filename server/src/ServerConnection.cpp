#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "ListServer.h"
#include "ServerConnection.h"
#include "ServerPlayer.h"
#include "CLog.h"

enum
{
	PLV_PRE22			= 0,
	PLV_POST22			= 1,
	PLV_22				= 2,
};

// TODO(joey): Move this somewhere else
CString getAccountError(AccountStatus status)
{
	switch (status)
	{
		case AccountStatus::Normal:
			return "SUCCESS";

		case AccountStatus::NotActivated:
			return "Your account is not activated.";

		case AccountStatus::Banned:
			return "Your account is globally banned.";

		case AccountStatus::InvalidPassword:
			return "Account name or password is invalid.";

		case AccountStatus::BackendError:
			return "There was a problem verifying your account.  The SQL server is probably down.";

		default:
			return "Unknown server error.";
	}
}

/*
	Pointer-Functions for Packets
*/
typedef bool (ServerConnection::*ServerSocketFunction)(CString&);

ServerSocketFunction serverFunctionTable[SVI_PACKETCOUNT];

void createServerPtrTable()
{
	// kinda like a memset-ish thing y'know
	for (int packetId = 0; packetId < SVI_PACKETCOUNT; packetId++)
		serverFunctionTable[packetId] = &ServerConnection::msgSVI_NULL;

	// now set non-nulls
	serverFunctionTable[SVI_SETNAME] = &ServerConnection::msgSVI_SETNAME;
	serverFunctionTable[SVI_SETDESC] = &ServerConnection::msgSVI_SETDESC;
	serverFunctionTable[SVI_SETLANG] = &ServerConnection::msgSVI_SETLANG;
	serverFunctionTable[SVI_SETVERS] = &ServerConnection::msgSVI_SETVERS;
	serverFunctionTable[SVI_SETURL]  = &ServerConnection::msgSVI_SETURL;
	serverFunctionTable[SVI_SETIP]   = &ServerConnection::msgSVI_SETIP;
	serverFunctionTable[SVI_SETPORT] = &ServerConnection::msgSVI_SETPORT;
	serverFunctionTable[SVI_SETPLYR] = &ServerConnection::msgSVI_SETPLYR;
	serverFunctionTable[SVI_VERIACC] = &ServerConnection::msgSVI_VERIACC;
	serverFunctionTable[SVI_VERIGLD] = &ServerConnection::msgSVI_VERIGLD;
	serverFunctionTable[SVI_GETFILE] = &ServerConnection::msgSVI_GETFILE;
	serverFunctionTable[SVI_NICKNAME] = &ServerConnection::msgSVI_NICKNAME;
	serverFunctionTable[SVI_GETPROF] = &ServerConnection::msgSVI_GETPROF;
	serverFunctionTable[SVI_SETPROF] = &ServerConnection::msgSVI_SETPROF;
	serverFunctionTable[SVI_PLYRADD] = &ServerConnection::msgSVI_PLYRADD;
	serverFunctionTable[SVI_PLYRREM] = &ServerConnection::msgSVI_PLYRREM;
	serverFunctionTable[SVI_SVRPING] = &ServerConnection::msgSVI_SVRPING;
	serverFunctionTable[SVI_VERIACC2] = &ServerConnection::msgSVI_VERIACC2;
	serverFunctionTable[SVI_SETLOCALIP] = &ServerConnection::msgSVI_SETLOCALIP;
	serverFunctionTable[SVI_GETFILE2] = &ServerConnection::msgSVI_GETFILE2;
	serverFunctionTable[SVI_UPDATEFILE] = &ServerConnection::msgSVI_UPDATEFILE;
	serverFunctionTable[SVI_GETFILE3] = &ServerConnection::msgSVI_GETFILE3;
	serverFunctionTable[SVI_NEWSERVER] = &ServerConnection::msgSVI_NEWSERVER;
	serverFunctionTable[SVI_SERVERHQPASS] = &ServerConnection::msgSVI_SERVERHQPASS;
	serverFunctionTable[SVI_SERVERHQLEVEL] = &ServerConnection::msgSVI_SERVERHQLEVEL;
	serverFunctionTable[SVI_SERVERINFO] = &ServerConnection::msgSVI_SERVERINFO;
	serverFunctionTable[SVI_REQUESTLIST] = &ServerConnection::msgSVI_REQUESTLIST;
	serverFunctionTable[SVI_REQUESTSVRINFO] = &ServerConnection::msgSVI_REQUESTSVRINFO;
	serverFunctionTable[SVI_REQUESTBUDDIES] = &ServerConnection::msgSVI_REQUESTBUDDIES;
	serverFunctionTable[SVI_REGISTERV3] = &ServerConnection::msgSVI_REGISTERV3;
	serverFunctionTable[SVI_SENDTEXT] = &ServerConnection::msgSVI_SENDTEXT;
}

/*
	Constructor - Deconstructor
*/
ServerConnection::ServerConnection(ListServer *listServer, CSocket *pSocket)
: _listServer(listServer), _socket(pSocket), addedToSQL(false), isServerHQ(false),
serverhq_level(1), server_version(VERSION_1), _fileQueue(pSocket), new_protocol(false), nextIsRaw(false), rawPacketSize(0)
{
	static bool _setupServerPackets = false;
	if (!_setupServerPackets)
	{
		createServerPtrTable();
		_setupServerPackets = true;
	}

	_fileQueue.setCodec(ENCRYPT_GEN_1, 0);
	language = "English";
	lastPing = lastPlayerCount = lastData = lastUptimeCheck = time(0);
}

ServerConnection::~ServerConnection()
{
	// Clear Playerlist
	clearPlayerList();

//	// Update our uptime.
//	if (isServerHQ)
//	{
//		int uptime = (int)difftime(time(0), lastUptimeCheck);
//		CString query = CString("UPDATE `") << settings->getStr("serverhq") << "` SET uptime=uptime+" << CString((int)uptime) << " WHERE name='" << name.escape() << "'";
//		mySQL->add_simple_query(query.text());
//	}
//
//	// Delete server from SQL serverlist.
//	CString query = CString("DELETE FROM ") << settings->getStr("serverlist") << " WHERE name='" << name.escape() << "'";
//	mySQL->add_simple_query(query.text());

	// TODO(joey): very temporary, listserver will be responsible to tell servers when it disconnects
	CString dataPacket;
	dataPacket.writeGChar(SVO_SENDTEXT);
	dataPacket << "Listserver,Modify,Server," << getName().gtokenize() << ",players=-1";
	_listServer->sendPacketToServers(dataPacket, this);

	// delete socket
	delete _socket;
}

bool ServerConnection::sendMessage(const std::string& channel, const std::string& from, const std::string& message)
{
	CString forwardPacket;
	forwardPacket.writeGChar(SVO_SENDTEXT);
	forwardPacket << "GraalEngine,irc,privmsg,";
	forwardPacket << CString(from).gtokenize() << "," << CString(channel).gtokenize() << "," << CString(message).gtokenize();
	sendPacket(forwardPacket);
}
/*
	Loops
*/
bool ServerConnection::doMain()
{
	// sock exist?
	if (_socket == NULL)
		return false;

	// Grab the data from the socket and put it into our receive buffer.
	unsigned int size = 0;
	char* data = _socket->getData(&size);

	if (size != 0)
		sockBuffer.write(data, size);
	else if (_socket->getState() == SOCKET_STATE_DISCONNECTED)
		return false;
	
	if (!sockBuffer.isEmpty())
	{
		// definitions
		CString unBuffer;

		if (new_protocol)
		{
			sockBuffer.setRead(0);
			while (sockBuffer.length() >= 2)
			{
				// packet length
				unsigned short len = (unsigned short)sockBuffer.readShort();
				if ((unsigned int)len > (unsigned int)sockBuffer.length() - 2)
					break;

				unBuffer = sockBuffer.readChars(len);
				sockBuffer.removeI(0, len + 2);
				unBuffer.zuncompressI();

				// well theres your buffer
				if (!parsePacket(unBuffer))
					return false;
			}
		}
		else
		{
			CString line;
			int lineEnd;

			// parse data
			do
			{
				if ((lineEnd = sockBuffer.find('\n')) == -1)
					break;

				line = sockBuffer.subString(0, lineEnd + 1);
				sockBuffer.removeI(0, line.length());
				
				if (!parsePacket(line))
					return false;
			} while (sockBuffer.bytesLeft() && !new_protocol);
		}
	}

	// Send a ping every 30 seconds.
	if ( (int)difftime( time(0), lastPing ) >= 30 )
	{
		lastPing = time(0);
		sendPacket( CString() >> (char)SVO_PING );
	}

	// send out buffer
	sendCompress();
	return true;
}

/*
	Kill Client
*/
void ServerConnection::kill()
{
	// Send Out-Buffer
	sendCompress();
	delete this;
}

const CString ServerConnection::getPlayers()
{
	const int ANY_CLIENT = (int)(1 << 0) | (int)(1 << 4) | (int)(1 << 5);

	// Update our player list.
	CString playerlist;
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		ServerPlayer *player = *it;
		if ((player->getClientType() & ANY_CLIENT) != 0)
			playerlist << CString(CString() << player->getAccountName() << "\n" << player->getNickName() << "\n").gtokenizeI() << "\n";
	}

	playerlist.gtokenizeI();
	return playerlist;
}

/*
	Get-Value Functions
*/
const CString& ServerConnection::getDescription()
{
	return description;
}

const CString ServerConnection::getIp(const CString& pIp)
{
	if (pIp == ip)
	{
		if (localip.length() != 0) return localip;
		return "127.0.0.1";
	}
	return ip;
}

const CString& ServerConnection::getLanguage()
{
	return language;
}

const CString& ServerConnection::getName()
{
	return name;
}

const CString& ServerConnection::getPort()
{
	return port;
}

const CString ServerConnection::getType(int PLVER)
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

const CString ServerConnection::getServerPacket(int PLVER, const CString& pIp)
{
	CString testIp = getIp(pIp);
	CString pcount((int)playerList.size());
	return CString() >> (char)8 >> (char)(getType(PLVER).length() + getName().length()) << getType(PLVER) << getName() >> (char)getLanguage().length() << getLanguage() >> (char)getDescription().length() << getDescription() >> (char)getUrl().length() << getUrl() >> (char)getVersion().length() << getVersion() >> (char)pcount.length() << pcount >> (char)testIp.length() << testIp >> (char)getPort().length() << getPort();
}

ServerPlayer * ServerConnection::getPlayer(unsigned short id) const
{
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		ServerPlayer *player = *it;
		if (player->getId() == id)
			return player;
	}

	return nullptr;
}

ServerPlayer * ServerConnection::getPlayer(const std::string & account, int type) const
{
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		ServerPlayer *player = *it;
		if (player->getClientType() == type && player->getAccountName() == account)
			return player;
	}

	return nullptr;
}

void ServerConnection::clearPlayerList()
{
	// clean playerlist
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
		delete *it;
	playerList.clear();
}

void ServerConnection::sendTextForPlayer(ServerPlayer * player, const CString & data)
{
	assert(player);

	CString dataPacket;
	dataPacket.writeGChar(SVO_REQUESTTEXT);
	dataPacket >> (short)player->getId() << data;
	sendPacket(dataPacket);
}

void ServerConnection::updatePlayers()
{
	CString dataPacket;
	dataPacket.writeGChar(SVO_SENDTEXT);
	dataPacket << "Listserver,Modify,Server," << getName().gtokenize() << ",players=" << CString(getPlayerCount());
	_listServer->sendPacketToServers(dataPacket);
}

/*
	Send-Packet Functions
*/
void ServerConnection::sendCompress()
{
	if (new_protocol)
	{
		_fileQueue.sendCompress();
		return;
	}

	// empty buffer?
	if (sendBuffer.isEmpty())
	{
		// If we still have some data in the out buffer, try sending it again.
		if (outBuffer.isEmpty() == false)
		{
			unsigned int dsize = outBuffer.length();
			outBuffer.removeI(0, _socket->sendData(outBuffer.text(), &dsize));
		}
		return;
	}
	
	// add the send buffer to the out buffer
	outBuffer << sendBuffer;

	// send buffer
	unsigned int dsize = outBuffer.length();
	outBuffer.removeI(0, _socket->sendData(outBuffer.text(), &dsize));

	// clear buffer
	sendBuffer.clear();
}

void ServerConnection::sendPacket(CString pPacket, bool pSendNow)
{
	// empty buffer?
	if (pPacket.isEmpty())
		return;

	// append '\n'
	if (pPacket[pPacket.length()-1] != '\n')
		pPacket.writeChar('\n');

	// append buffer depending on protocol
	if (new_protocol)
		_fileQueue.addPacket(pPacket);
	else
		sendBuffer.write(pPacket);

	// send buffer now?
	if (pSendNow)
		sendCompress();
}

/*
	Packet-Functions
*/
bool ServerConnection::parsePacket(CString& pPacket)
{
	while (pPacket.bytesLeft() > 0)
	{
		CString curPacket;
		if (nextIsRaw)
		{
			nextIsRaw = false;
			curPacket = pPacket.readChars(rawPacketSize);
		}
		else curPacket = pPacket.readString("\n");

		// read id & packet
		unsigned char id = curPacket.readGUChar();

		printf("Server Packet [%d]: %s (%d)\n", id, curPacket.text() + 1, curPacket.length());

		// valid packet, call function
		bool ret = (*this.*serverFunctionTable[id])(curPacket);
		if (!ret) {
			//		serverlog.out("Packet %u failed for server %s.\n", (unsigned int)id, name.text());
		}

		// Update the data timeout.
		lastData = time(0);
	}

	return true;
}

bool ServerConnection::msgSVI_NULL(CString& pPacket)
{
	pPacket.setRead(0);
	unsigned char id = pPacket.readGUChar();
	printf("Unknown Server Packet: %u (%s)\n", (unsigned int)id, pPacket.text()+1);
	return true;
}

bool ServerConnection::msgSVI_SETNAME(CString& pPacket)
{
	CString oldName(name);
	name = pPacket.readString("");
	
	// Remove all server type strings from the name of the server.
	while (name.subString(0, 2) == "U " || name.subString(0, 2) == "P " || name.subString(0, 2) == "H " || name.subString(0, 2) == "3 ")
		name.removeI(0, 2);

	// Shouldn't we be checking for blank names?
	if (name.isEmpty())
		return false;

	// Remove the old server
	if (!oldName.isEmpty())
	{
		CString removeOldServer;
		removeOldServer.writeGChar(SVO_SENDTEXT);
		removeOldServer << "Listserver,Modify,Server," << oldName.gtokenize() << ",players=-1";
		_listServer->sendPacketToServers(removeOldServer);
	}

	// Add the new server name
	CString addNewServer;
	addNewServer.writeGChar(SVO_SENDTEXT);
	addNewServer << "Listserver,Modify,Server," << getName().gtokenize() << ",players=" << CString(getPlayerCount());
	_listServer->sendPacketToServers(addNewServer);

	// Check ServerHQ if we can use this name.
#ifndef NO_MYSQL
//	CString query;
//	std::vector<std::string> result;
//	query = CString() << "SELECT activated FROM `" << settings->getStr("serverhq") << "` WHERE name='" << name.escape() << "' LIMIT 1";
//	int ret = mySQL->try_query(query.text(), result);
//	if (ret == -1) return false;
//
//	if (!result.empty())
//	{
//		result.clear();
//		query = CString() << "SELECT activated FROM `" << settings->getStr("serverhq") << "` WHERE name='" << name.escape() << "' AND activated='1' AND password=" << "MD5(CONCAT(MD5('" << serverhq_pass.escape() << "'), `salt`)) LIMIT 1";
//		ret = mySQL->try_query(query.text(), result);
//		if (ret == -1) return false;
//
//		if (result.size() == 0)
//			name << " (unofficial)";
//	}
#endif

	// check for duplicates
	bool dupFound = false;
	int dupCount = 0;
//dupCheck:
//	for (unsigned int i = 0; i < serverList.size(); i++)
//	{
//		if (serverList[i] == 0) continue;
//		if (serverList[i] != this && serverList[i]->getName() == name)
//		{
//			ServerConnection* server = serverList[i];
//
//			// If the IP addresses are the same, something happened and this server is reconnecting.
//			// Delete the old server.
//			if (server->getSock() == 0 || strcmp(server->getSock()->getRemoteIp(), sock->getRemoteIp()) == 0)
//			{
//				name = server->getName();
//				server->kill();
//				i = serverList.size();
//			}
//			else
//			{
//				// Duplicate found.  Append a random number to the end and start over.
//				// Do this a max of 4 times.  If after 4 tries, fail.
//				if (dupCount++ < 5)
//				{
//					name << CString(rand() % 9);
//					goto dupCheck;
//				}
//				sendPacket(CString() >> (char)SVO_ERRMSG << "Servername is already in use!");
//				dupFound = true;
//				break;
//			}
//		}
//	}
//
//	// In a worst case scenario, attempt to see if we originally had a valid name.
//	// If so, just go back to it.
//	if (dupFound && addedToSQL)
//	{
//		name = oldName;
//		return true;
//	}
//	else if (dupFound)
//		return false;
//
//	// If we aren't in SQL yet, that means we are a new server.  If so, announce it.
//	// If we are in SQL, update the SQL entry with our new name.
//	if (!addedToSQL) serverlog.out(CString() << "New Server: " << name << "\n");
//	else
//	{
//		SQLupdate("name", name);
//	}

	return true;
}

bool ServerConnection::msgSVI_SETDESC(CString& pPacket)
{
	description = pPacket.readString("");
	//SQLupdate("description", description);
	return true;
}

bool ServerConnection::msgSVI_SETLANG(CString& pPacket)
{
	language = pPacket.readString("");
	//SQLupdate("language", language);
	return true;
}

bool ServerConnection::msgSVI_SETVERS(CString& pPacket)
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
	//SQLupdate("version", version);
	return true;
}

bool ServerConnection::msgSVI_SETURL(CString& pPacket)
{
	url = pPacket.readString("");
	//SQLupdate("url", url);
	return true;
}

bool ServerConnection::msgSVI_SETIP(CString& pPacket)
{
	ip = pPacket.readString("");
	ip = (ip == "AUTO" ? _socket->getRemoteIp() : ip);
	//SQLupdate("ip", ip);
	return true;
}

bool ServerConnection::msgSVI_SETPORT(CString& pPacket)
{
	port = pPacket.readString("");

	// This should be the last packet sent when the server is initialized.
	// Add it to the database now.
#ifndef NO_MYSQL
//	if (!addedToSQL)
//	{
//		if (name.length() > 0)
//		{
//			CString query;
//			query << "INSERT INTO " << settings->getStr("serverlist") << " ";
//			query << "(name, ip, port, playercount, description, url, language, type, version) ";
//			query << "VALUES ('"
//				<< name.escape() << "','"
//				<< ip.escape() << "','"
//				<< port.escape() << "','"
//				<< CString((int)playerList.size()) << "','"
//				<< description.escape() << "','"
//				<< url.escape() << "','"
//				<< language.escape() << "','"
//				<< getType(4).escape() << "','"
//				<< version.escape() << "'"
//				<< ")";
//			mySQL->add_simple_query(query.text());
//			addedToSQL = true;
//		}
//	}
//	else SQLupdate("port", port);
#endif

	return true;
}

bool ServerConnection::msgSVI_SETPLYR(CString& pPacket)
{
	// clear list
	if (new_protocol)
	{
		clearPlayerList();
	}
	else
	{
		int oldPlayerCount = getPlayerCount();

		// Clear the playerlist
		clearPlayerList();

		// grab new playercount
		unsigned int count = pPacket.readGUChar();

		for (unsigned int i = 0; i < count; i++)
		{
			CString accountName = pPacket.readChars(pPacket.readGUChar());
			CString nickName = pPacket.readChars(pPacket.readGUChar());
			CString levelName = pPacket.readChars(pPacket.readGUChar());
			char playerX = pPacket.readGChar();
			char playerY = pPacket.readGChar();
			int alignment = pPacket.readGUChar();
			int type = pPacket.readGUChar();

			// Reconstruct the prop packet
			CString propPacket;
			propPacket >> (char)PLPROP_ACCOUNTNAME >> (char)accountName.length() << accountName;
			propPacket >> (char)PLPROP_NICKNAME >> (char)nickName.length() << nickName;
			propPacket >> (char)PLPROP_CURLEVEL >> (char)levelName.length() << levelName;
			propPacket >> (char)PLPROP_X >> (char)playerX;
			propPacket >> (char)PLPROP_Y >> (char)playerY;
			propPacket >> (char)PLPROP_ALIGNMENT >> (char)alignment;

			// Create the player object
			ServerPlayer *playerObject = new ServerPlayer();
			playerObject->setClientType(type);
			playerObject->setProps(propPacket);
			playerList.push_back(playerObject);
		}

		// Update the players.
		if (oldPlayerCount != count)
			updatePlayers();
	}

	return true;
}

bool ServerConnection::msgSVI_VERIACC(CString& pPacket)
{
	// definitions
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString password = pPacket.readChars(pPacket.readGUChar());

	// Verify the account.
	AccountStatus status = _listServer->verifyAccount(account.text(), password.text());

	sendPacket(CString() >> (char)SVO_VERIACC >> (char)account.length() << account << getAccountError(status));
	return true;
}

bool ServerConnection::msgSVI_VERIGLD(CString& pPacket)
{
	unsigned short playerId = pPacket.readGUShort();
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString nickname = pPacket.readChars(pPacket.readGUChar());
	CString guild = pPacket.readChars(pPacket.readGUChar());

	// Verify the account.
	GuildStatus status = _listServer->verifyGuild(account.text(), nickname.text(), guild.text());
	if (status == GuildStatus::Valid)
	{
		// TODO(joey): prune nickname from previous guilds.
		CString newNick = nickname << " (" << guild << ")";
		CString dataBuffer;
		dataBuffer.writeGChar(SVO_VERIGLD);
		dataBuffer.writeGShort(playerId);
		dataBuffer.writeGChar(newNick.length()).write(newNick);
		sendPacket(dataBuffer);
	}

	return true;
}

bool ServerConnection::msgSVI_GETFILE(CString& pPacket)
{
	unsigned short pId = pPacket.readGUShort();
	unsigned char pTy = pPacket.readGUChar();
	CString shortName = pPacket.readChars(pPacket.readGUChar());
//	CFileSystem* fileSystem = &(filesystem[0]);
//	switch (pTy)
//	{
//		case 0: fileSystem = &(filesystem[1]); break;
//		case 1: fileSystem = &(filesystem[2]); break;
//		case 2: fileSystem = &(filesystem[3]); break;
//		case 3: fileSystem = &(filesystem[4]); break;
//	}
//	CString fileData = fileSystem->load(shortName);
//
//	if (fileData.length() != 0)
//	{
//		sendPacket(CString() >> (char)SVO_FILESTART >> (char)shortName.length() << shortName << "\n");
//		fileData = CString_Base64_Encode(fileData);
//
//		while (fileData.length() > 0)
//		{
//			CString temp = fileData.subString(0, (fileData.length() > 0x10000 ? 0x10000 : fileData.length()));
//			fileData.removeI(0, temp.length());
//			sendPacket(CString() >> (char)SVO_FILEDATA >> (char)shortName.length() << shortName << temp << "\n");
//		}
//
//		sendPacket(CString() >> (char)SVO_FILEEND >> (char)shortName.length() << shortName >> (short)pId >> (char)pTy << "\n");
//	}

	return true;
}

bool ServerConnection::msgSVI_NICKNAME(CString& pPacket)
{
	// OBSOLETE
	if (new_protocol)
		return true;

	std::string accountName = pPacket.readChars(pPacket.readGUChar()).text();
	std::string nickName = pPacket.readChars(pPacket.readGUChar()).text();

	// Find the player and adjust his nickname.
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		ServerPlayer *playerObject = *it;
		if (playerObject->getAccountName() == accountName)
			playerObject->setNickName(nickName);
	}

	return true;
}

bool ServerConnection::msgSVI_GETPROF(CString& pPacket)
{
	unsigned short playerId = pPacket.readGUShort();

	// Fix old gservers that sent an incorrect packet.
	pPacket.readGUChar();

	// Read the account name.
	CString accountName = pPacket.readString("");

	std::optional<PlayerProfile> profile = _listServer->getProfile(accountName.text());
	if (profile.has_value())
	{
		const PlayerProfile& prof = profile.value();
		CString age = CString(prof.getAge());

		CString dataBuffer;
		dataBuffer.writeGChar(SVO_PROFILE);
		dataBuffer.writeGShort(playerId);
		dataBuffer.writeGChar(accountName.length()).write(accountName);

		dataBuffer.writeGChar((unsigned char)prof.getName().length()).write(prof.getName());
		dataBuffer.writeGChar((unsigned char)age.length()).write(age);
		dataBuffer.writeGChar((unsigned char)prof.getGender().length()).write(prof.getGender());
		dataBuffer.writeGChar((unsigned char)prof.getCountry().length()).write(prof.getCountry());
		dataBuffer.writeGChar((unsigned char)prof.getMessenger().length()).write(prof.getMessenger());
		dataBuffer.writeGChar((unsigned char)prof.getEmail().length()).write(prof.getEmail());
		dataBuffer.writeGChar((unsigned char)prof.getWebsite().length()).write(prof.getWebsite());
		dataBuffer.writeGChar((unsigned char)prof.getHangout().length()).write(prof.getHangout());
		dataBuffer.writeGChar((unsigned char)prof.getQuote().length()).write(prof.getQuote());
		sendPacket(dataBuffer);
	}

	return true;
}

bool ServerConnection::msgSVI_SETPROF(CString& pPacket)
{
	// Fix old gservers that sent an incorrect packet.
	pPacket.readGUChar();

	// Read the account name in.
	CString accountName = pPacket.readChars(pPacket.readGUChar());

	// TODO(joey): Verify this is a player on the server.

	// Construct profile from packet
	PlayerProfile newProfile(accountName.text());
	newProfile.setName(pPacket.readChars(pPacket.readGUChar()).text());
	newProfile.setAge(strtoint(pPacket.readChars(pPacket.readGUChar())));
	newProfile.setGender(pPacket.readChars(pPacket.readGUChar()).text());
	newProfile.setCountry(pPacket.readChars(pPacket.readGUChar()).text());
	newProfile.setMessenger(pPacket.readChars(pPacket.readGUChar()).text());
	newProfile.setEmail(pPacket.readChars(pPacket.readGUChar()).text());
	newProfile.setWebsite(pPacket.readChars(pPacket.readGUChar()).text());
	newProfile.setHangout(pPacket.readChars(pPacket.readGUChar()).text());
	newProfile.setQuote(pPacket.readChars(pPacket.readGUChar()).text());

	// Set profile
	_listServer->setProfile(newProfile);

	return true;
}

bool ServerConnection::msgSVI_PLYRADD(CString& pPacket)
{
	ServerPlayer *playerObject;
	CString propPacket;
	unsigned char clientType;

	if (new_protocol)
	{
		unsigned short playerId = pPacket.readGUShort();
		clientType = pPacket.readGUChar();
		propPacket = pPacket.readString("");

		// Add player id to prop packet
		propPacket >> (char)PLPROP_ID >> (short)playerId;

		// See if the player is a duplicate
		playerObject = getPlayer(playerId);
	}
	else
	{
		CString accountName = pPacket.readChars(pPacket.readGUChar());
		CString nickName = pPacket.readChars(pPacket.readGUChar());
		CString levelName = pPacket.readChars(pPacket.readGUChar());
		char playerX = pPacket.readGChar();
		char playerY = pPacket.readGChar();
		int alignment = pPacket.readGUChar();
		clientType = pPacket.readGUChar();

		// Reconstruct the prop packet
		propPacket >> (char)PLPROP_ACCOUNTNAME >> (char)accountName.length() << accountName;
		propPacket >> (char)PLPROP_NICKNAME >> (char)nickName.length() << nickName;
		propPacket >> (char)PLPROP_CURLEVEL >> (char)levelName.length() << levelName;
		propPacket >> (char)PLPROP_X >> (char)playerX;
		propPacket >> (char)PLPROP_Y >> (char)playerY;
		propPacket >> (char)PLPROP_ALIGNMENT >> (char)alignment;

		// See if the player is a duplicate
		playerObject = getPlayer(accountName.text(), clientType);
	}

	// Create a new player
	if (playerObject == nullptr)
	{
		playerObject = new ServerPlayer();
		playerList.push_back(playerObject);
	}
	
	// Set properties
	playerObject->setClientType(clientType);
	playerObject->setProps(propPacket);

	// Update the database.
	updatePlayers();

	return true;
}

bool ServerConnection::msgSVI_PLYRREM(CString& pPacket)
{
	if (new_protocol)
	{
		unsigned short id = pPacket.readGUShort();

		for (auto it = playerList.begin(); it != playerList.end();)
		{
			ServerPlayer *player = *it;
			if (player->getId() == id)
			{
				it = playerList.erase(it);
				delete player;
			}
			else ++it;
		}
	}
	else
	{
		unsigned char type = pPacket.readGUChar();
		std::string accountName = pPacket.readString("").text();

		for (auto it = playerList.begin(); it != playerList.end();)
		{
			ServerPlayer *player = *it;
			if (player->getClientType() == type && player->getAccountName() == accountName)
			{
				it = playerList.erase(it);
				delete player;
			}
			else ++it;
		}
	}

	// Update the database.
	updatePlayers();

	return true;
}

bool ServerConnection::msgSVI_SVRPING(CString& pPacket)
{
	if (new_protocol)
	{
		// Response of a PING request from the listserver.
		// Can be used to calculate latency, or something..
	}
	else
	{
		// Server sends this every 30 seconds
	}

	return true;
}

// Secure login password:
//	{transaction}{CHAR \xa7}{password}
bool ServerConnection::msgSVI_VERIACC2(CString& pPacket)
{
	// definitions
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString password = pPacket.readChars(pPacket.readGUChar());
	unsigned short id = pPacket.readGUShort();
	unsigned char type = pPacket.readGUChar();

	// Verify the account.
	AccountStatus status = _listServer->verifyAccount(account.text(), password.text());
	sendPacket(CString() >> (char)SVO_VERIACC2 >> (char)account.length() << account >> (short)id >> (char)type << getAccountError(status));

	return true;
}

bool ServerConnection::msgSVI_SETLOCALIP(CString& pPacket)
{
	localip = pPacket.readString("");
	return true;
}

bool ServerConnection::msgSVI_GETFILE2(CString& pPacket)
{
	unsigned short pId = pPacket.readGUShort();
	unsigned char pTy = pPacket.readGUChar();
	CString shortName = pPacket.readChars(pPacket.readGUChar());
//	CFileSystem* fileSystem = &(filesystem[0]);
//	switch (pTy)
//	{
//		case 0: fileSystem = &(filesystem[1]); break;
//		case 1: fileSystem = &(filesystem[2]); break;
//		case 2: fileSystem = &(filesystem[3]); break;
//		case 3: fileSystem = &(filesystem[4]); break;
//	}
//	CString fileData = fileSystem->load(shortName);
//	time_t modTime = fileSystem->getModTime(shortName);
//
//	if (fileData.length() != 0)
//	{
//		int packetLength = 1 + 1 + shortName.length() + 1;
//
//		// Tell the server that it should expect a file.
//		sendPacket(CString() >> (char)SVO_FILESTART2 << shortName);
//
//		// Save the file length.
//		int fileLength = fileData.length();
//
//		// Compress the file.
//		// Don't compress .png files since they are already zlib compressed.
//		CString ext = shortName.subString(shortName.length() - 4, 4);
//		char doCompress = 0;
//		if (ext.comparei(".png") == false)
//		{
//			doCompress = 1;
//			fileData.zcompressI();
//		}
//
//		// Send the file to the server.
//		while (fileData.length() != 0)
//		{
//			int sendSize = clip(32000, 0, fileData.length());
//			sendPacket(CString() >> (char)SVO_RAWDATA >> (int)(packetLength + sendSize));
//			sendPacket(CString() >> (char)SVO_FILEDATA2 >> (char)shortName.length() << shortName << fileData.subString(0, sendSize));
//			fileData.removeI(0, sendSize);
//		}
//
//		// Tell the gserver that the file send is now finished.
//		sendPacket(CString() >> (char)SVO_FILEEND2 >> (short)pId >> (char)pTy >> (char)doCompress >> (long long)modTime >> (long long)fileLength << shortName);
//	}

	return true;
}

bool ServerConnection::msgSVI_UPDATEFILE(CString& pPacket)
{
	time_t modTime = pPacket.readGUInt5();
	unsigned char pTy = pPacket.readGUChar();
	CString file = pPacket.readString("");
//	CFileSystem* fileSystem = &(filesystem[0]);
//	switch (pTy)
//	{
//		case 0: fileSystem = &(filesystem[1]); break;
//		case 1: fileSystem = &(filesystem[2]); break;
//		case 2: fileSystem = &(filesystem[3]); break;
//		case 3: fileSystem = &(filesystem[4]); break;
//	}
//
//	time_t modTime2 = fileSystem->getModTime(file);
//	if (modTime2 != modTime)
//	{
//		return msgSVI_GETFILE3(CString() >> (short)0 >> (char)pTy >> (char)file.length() << file);
//	}
	return true;
}

// Sigh.  A third one.  Just to add a single byte to the start and data packets.
bool ServerConnection::msgSVI_GETFILE3(CString& pPacket)
{
	unsigned short pId = pPacket.readGUShort();
	unsigned char pTy = pPacket.readGUChar();
	CString shortName = pPacket.readChars(pPacket.readGUChar());
//	CFileSystem* fileSystem = &(filesystem[0]);
//	switch (pTy)
//	{
//		case 0: fileSystem = &(filesystem[1]); break;
//		case 1: fileSystem = &(filesystem[2]); break;
//		case 2: fileSystem = &(filesystem[3]); break;
//		case 3: fileSystem = &(filesystem[4]); break;
//	}
//	CString fileData = fileSystem->load(shortName);
//	time_t modTime = fileSystem->getModTime(shortName);
//
//	if (fileData.length() != 0)
//	{
//		int packetLength = 1 + 1 + 1 + shortName.length() + 1;
//
//		// Tell the server that it should expect a file.
//		sendPacket(CString() >> (char)SVO_FILESTART3 >> (char)pTy >> (char)shortName.length() << shortName);
//
//		// Save the file length.
//		int fileLength = fileData.length();
//
//		// Compress the file.
//		// Don't compress .png files since they are already zlib compressed.
//		CString ext = shortName.subString(shortName.length() - 4, 4);
//		char doCompress = 0;
//		if (ext.comparei(".png") == false)
//		{
//			doCompress = 1;
//			fileData.zcompressI();
//		}
//
//		// Send the file to the server.
//		while (fileData.length() != 0)
//		{
//			int sendSize = clip(32000, 0, fileData.length());
//			sendPacket(CString() >> (char)SVO_RAWDATA >> (int)(packetLength + sendSize));
//			sendPacket(CString() >> (char)SVO_FILEDATA3 >> (char)pTy >> (char)shortName.length() << shortName << fileData.subString(0, sendSize));
//			fileData.removeI(0, sendSize);
//		}
//
//		// Tell the gserver that the file send is now finished.
//		sendPacket(CString() >> (char)SVO_FILEEND3 >> (short)pId >> (char)pTy >> (char)doCompress >> (long long)modTime >> (long long)fileLength << shortName);
//	}

	return true;
}

bool ServerConnection::msgSVI_NEWSERVER(CString& pPacket)
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

	// TODO(joey): temporary
	auto serverList = _listServer->getConnections();
	for (auto it = serverList.begin(); it != serverList.end(); ++it)
	{
		ServerConnection *server = *it;

		CString dataPacket;
		dataPacket.writeGChar(SVO_SENDTEXT);
		dataPacket << "Listserver,Modify,Server," << server->getName().gtokenize() << ",players=" << CString(server->getPlayerCount());
		sendPacket(dataPacket);
	}

	return true;
}

bool ServerConnection::msgSVI_SERVERHQPASS(CString& pPacket)
{
	serverhq_pass = pPacket.readString("");
	return true;
}

bool ServerConnection::msgSVI_SERVERHQLEVEL(CString& pPacket)
{
	serverhq_level = pPacket.readGUChar();

#ifndef NO_MYSQL
//	// Ask what our max level and max players is.
//	CString query;
//	std::vector<std::string> result;
//	query = CString() << "SELECT maxplayers, uptime, maxlevel FROM `" << settings->getStr("serverhq") << "` WHERE name='" << name.escape() << "' AND activated='1' AND password=" << "MD5(CONCAT(MD5('" << serverhq_pass.escape() << "'), `salt`)) LIMIT 1";
//	int ret = mySQL->try_query(query.text(), result);
//	if (ret == -1) return false;
//
//	// Adjust the server level to the max allowed.
//	int maxlevel = settings->getInt("defaultServerLevel", 1);
//	if (result.size() > 2) maxlevel = std::stoi(result[2]);
//	if (serverhq_level > maxlevel) serverhq_level = maxlevel;
//
//	// If we got results, we have server hq support.
//	if (result.size() != 0) isServerHQ = true;
//
//	// If we got valid SQL results, deal with them now.
//	if (isServerHQ)
//	{
//		// Update our uptime.
//		if (result.size() > 1)
//		{
//			// Update our uptime.
//			CString query2;
//			query2 << "UPDATE `" << settings->getStr("serverlist") << "` SET uptime=" << result[1] << " WHERE name='" << name.escape() << "'";
//			mySQL->add_simple_query(query2.text());
//		}
//
//		// If we got max players, update the graal_servers table.
//		if (result.size() != 0)
//		{
//			int maxplayers = std::stoi(result[0]);
//
//			// Check to see if the graal_servers table has a larger max players.
//			std::vector<std::string> result2;
//			CString query2 = CString() << "SELECT maxplayers FROM `" << settings->getStr("serverlist") << "` WHERE name='" << name.escape() << "' LIMIT 1";
//			int ret = mySQL->try_query(query2.text(), result2);
//			if (ret != -1 && result2.size() != 0)
//			{
//				int s_maxp = std::stoi(result2[0]);
//				if (maxplayers > s_maxp) SQLupdate("maxplayers", CString((int)maxplayers));
//				else SQLupdateHQ("maxplayers", CString((int)s_maxp));
//			}
//			else SQLupdate("maxplayers", CString((int)maxplayers));
//		}
//	}
//
//	// Update our current level.
//	SQLupdate("type", getType(4));
//	if (isServerHQ) SQLupdateHQ("curlevel", CString((int)serverhq_level));
#endif

	return true;
}

bool ServerConnection::msgSVI_SERVERINFO(CString& pPacket)
{
	unsigned short playerId = pPacket.readGUShort();
	CString servername = pPacket.readString("");

	auto serverList = _listServer->getConnections();
	for (auto it = serverList.begin(); it != serverList.end(); ++it)
	{
		ServerConnection *server = *it;
		if (servername.comparei(server->getName()))
		{
			CString serverPacket = CString(server->getName()) << "\n" << server->getName() << "\n" << server->getIp() << "\n" << server->getPort();
			sendPacket(CString() >> (char)SVO_SERVERINFO >> (short)playerId << serverPacket.gtokenize());
			break;
		}
	}

	return true;
}

bool ServerConnection::msgSVI_PMPLAYER(CString& pPacket)
{
	unsigned short pid = pPacket.readGUShort();
	CString packet = pPacket.readString("");
	CString data = packet.guntokenize();

	CString servername = data.readString("\n");
	CString account = data.readString("\n");
	CString nick = data.readString("\n");
	CString weapon = data.readString("\n");
	CString type = data.readString("\n");
	CString account2 = data.readString("\n");
	CString message = data.readString("");
	
//	for (std::vector<ServerConnection*>::iterator i = serverList.begin(); i != serverList.end(); ++i)
//	{
//		ServerConnection* server = *i;
//		if (server == 0) continue;
//
//		//p << server->getName() << "\n";
//
//		if (server->getName() == servername)
//		{
//			serverlog.out(CString() << "Sending PM (" << message << ") to " << account2 << " on " << servername << "\n");
//			// Send the pm to the appropriate server.
//			CString pmData = CString(servername << "\n" << account << "\n" << nick << "\n" << weapon << "\n" << type << "\n" << account2 << "\n" << message).gtokenizeI();
//			server->sendPacket(CString() >> (char)SVO_PMPLAYER << pmData << "\n");
//			server->sendCompress();
//
//			return true;
//		}
//	}


	return true;
}

bool ServerConnection::msgSVI_REQUESTLIST(CString& pPacket)
{
	unsigned short playerId = pPacket.readGUShort();
	CString packet = pPacket.readString("");
	std::vector<CString> params = packet.gCommaStrTokens();

	//printf("Test Data: %s\n", packet.text());
	//for (int i = 0; i < params.size(); i++)
	//	printf("Param %d: %s\n", (int)i, params[i].text());

	if (params.size() >= 3)
	{
		// cant find the referenced player?
		ServerPlayer *player = getPlayer(playerId);
		
		if (player != nullptr)
		{
			if (params[0] == "GraalEngine")
			{
				if (params[1] == "irc")
				{
					if (params.size() == 4)
					{
						if (params[2] == "join") // GraalEngine,irc,join,#channel
						{
							CString sendMsg = "GraalEngine,irc,join,";
							sendMsg << params[3].gtokenize();
							sendTextForPlayer(player, sendMsg);

							std::string channel = params[3].guntokenize().text();
							_listServer->addPlayerToChannel(channel, player);
						}
						else if (params[2] == "part") // GraalEngine,irc,part,#channel
						{
							CString sendMsg = "GraalEngine,irc,part,";
							sendMsg << params[3].gtokenize();
							sendTextForPlayer(player, sendMsg);

							std::string channel = params[3].guntokenize().text();
							_listServer->removePlayerFromChannel(channel, player);
						}
					}
				}
			}
		}
		
		// Old stuff, forwards over.
		if (params[1] == "pmservers")
		{
			CString sendMsg;
			// Assemble the serverlist.
			auto serverList = _listServer->getConnections();
			for (auto it = serverList.begin(); it != serverList.end(); ++it)
			{
				ServerConnection *server = *it;
				//if (server->getTypeVal() == TYPE_HIDDEN) continue;

				sendMsg << server->getName() << "\n";
			}

			// TODO(joey): Show hidden servers if friends are on them...?
			//p << getOwnedServersPM(account);
			sendMsg.gtokenizeI();
			sendTextForPlayer(player, sendMsg);
		}
		else if (params[1] == "pmserverplayers")
		{
			CString sendMsg;
			// get servers
			auto serverList = _listServer->getConnections();
			for (auto it = serverList.begin(); it != serverList.end(); ++it)
			{
				ServerConnection *server = *it;
				if (server->getName() == params[2])
					sendMsg << server->getPlayers();
			}
			sendTextForPlayer(player, sendMsg);
		}
	}

	return true;
}

bool ServerConnection::msgSVI_REQUESTSVRINFO(CString& pPacket)
{
	unsigned short pid = pPacket.readGUShort();
	CString data = pPacket.readString("");

	CString data2 = data.guntokenize();
	CString weapon = data2.readString("\n");
	CString type = data2.readString("\n");
	CString option = data2.readString("\n");
	CString params = data2.readString("\n");

	// Untokenize our params.
	params.guntokenizeI();

//	// Find the server.
//	CString servername = params.readString("\n");
//	for (std::vector<ServerConnection*>::iterator i = serverList.begin(); i != serverList.end(); ++i)
//	{
//		ServerConnection* server = *i;
//		if (server == 0) continue;
//		if (servername.comparei(server->getName()))
//		{
//			CString p;
//			p << weapon << "\n";
//			p << type << "\n";
//			p << option << "\n";
//			p << server->getName() << "\n";
//			p << server->getType(PLV_POST22) << server->getName() << "\n";
//			p << server->getDescription() << "\n";
//			p << server->getLanguage() << "\n";
//			p << server->getVersion() << "\n";
//			p << server->getUrl() << "\n";
//			p.gtokenizeI();
//
//			// Send the server info back to the server.
//			sendPacket(CString() >> (char)SVO_REQUESTTEXT >> (short)pid << p);
//			return true;
//		}
//	}

	return true;
}

bool ServerConnection::msgSVI_REGISTERV3(CString& pPacket)
{
	new_protocol = true;
	_fileQueue.setCodec(ENCRYPT_GEN_2, 0);
	version = pPacket.readString("");
	return true;
}

bool ServerConnection::msgSVI_SENDTEXT(CString& pPacket)
{
	CString textData = pPacket.readString("");
	CString data = textData.guntokenize();
	std::vector<CString> params = textData.gCommaStrTokens();

	if (params.size() >= 3)
	{
		if (params[0] == "GraalEngine")
		{
			if (params[1] == "irc")
			{
				if (params.size() == 6 && params[2] == "privmsg")
				{
					std::string from = params[3].text();
					std::string channel = params[4].text();
					std::string message = params[5].text();
					_listServer->sendMessage(channel, from, message);
				}
			}
		}
	}

	return true;
}

bool ServerConnection::msgSVI_REQUESTBUDDIES(CString& pPacket)
{
	unsigned short pid = pPacket.readGUShort();
	CString packet = pPacket.readString("");
	CString data = packet.guntokenize();

	CString account = data.readString("\n");
	CString weapon = data.readString("\n");
	CString type = data.readString("\n");
	CString option = data.readString("\n");

//	CString p;
//	p << weapon << "\n";
//	p << type << "\n";
//	p << "buddylist" << "\n";
//	p.gtokenizeI();
//	p << "," << getBuddies(account);

//	sendPacket(CString() >> (char)SVO_REQUESTTEXT >> (short)pid << p);

	return true;
}
