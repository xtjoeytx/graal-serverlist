#include <cassert>
#include <cstdlib>
#include <ctime>
#include <array>
#include <CLog.h>
#include <IUtil.h>
#include "ListServer.h"
#include "ServerConnection.h"
#include "ServerPlayer.h"

enum
{
	PLV_PRE22			= 0,
	PLV_POST22			= 1,
	PLV_22				= 2,
};

enum
{
	PLTYPE_AWAIT		= (int)(-1),
	PLTYPE_CLIENT		= (int)(1 << 0),
	PLTYPE_RC			= (int)(1 << 1),
	PLTYPE_NPCSERVER	= (int)(1 << 2),
	PLTYPE_NC			= (int)(1 << 3),
	PLTYPE_CLIENT2		= (int)(1 << 4),
	PLTYPE_CLIENT3		= (int)(1 << 5),
	PLTYPE_RC2			= (int)(1 << 6),
	PLTYPE_EXTERNAL		= (int)(1 << 7),	// IRC client?

	PLTYPE_ANYCLIENT	= (int)(PLTYPE_CLIENT | PLTYPE_CLIENT2 | PLTYPE_CLIENT3),
	PLTYPE_ANYRC		= (int)(PLTYPE_RC | PLTYPE_RC2),
	PLTYPE_ANYNC		= (int)(PLTYPE_NC),
	PLTYPE_ANYCONTROL	= (int)(PLTYPE_ANYRC | PLTYPE_ANYNC),
	PLTYPE_ANYPLAYER	= (int)(PLTYPE_ANYCLIENT | PLTYPE_ANYRC),
	PLTYPE_NONITERABLE	= (int)(PLTYPE_NPCSERVER | PLTYPE_ANYNC | PLTYPE_EXTERNAL)
};

// Version ids that will get forwarded to offline server
const auto forwardingVersions = std::array{
	"GNP1905C"
};

/*
	Pointer-Functions for Packets
*/
typedef bool (ServerConnection::*ServerSocketFunction)(CString&);

ServerSocketFunction serverFunctionTable[SVI_PACKETCOUNT];

void ServerConnection::createServerPtrTable()
{
	for (auto & packetFn : serverFunctionTable)
		packetFn = &ServerConnection::msgSVI_NULL;

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
	serverFunctionTable[SVI_PMPLAYER] = &ServerConnection::msgSVI_PMPLAYER;
	serverFunctionTable[SVI_REGISTERV3] = &ServerConnection::msgSVI_REGISTERV3;
	serverFunctionTable[SVI_SENDTEXT] = &ServerConnection::msgSVI_SENDTEXT;
}

/*
	Constructor - Deconstructor
*/
ServerConnection::ServerConnection(ListServer *listServer, CSocket *pSocket)
	: _listServer(listServer), _socket(pSocket), _isAuthenticated(false), _disconnect(false),
		_isServerHQ(false), _serverLevel(ServerHQLevel::Bronze), _serverMaxLevel(ServerHQLevel::Bronze), _serverUpTime(0),
		_serverProtocol(ProtocolVersion::Version1), _fileQueue(pSocket), new_protocol(false), packetCount(0), nextIsRaw(false), rawPacketSize(0),
		_allowedVersionsMask(uint8_t(ClientType::AllServers))
{
	static bool setupServerPackets = false;
	if (!setupServerPackets)
	{
		ServerConnection::createServerPtrTable();
		setupServerPackets = true;
	}

	_fileQueue.setCodec(ENCRYPT_GEN_1, 0);
	_lastData = _lastPing = _startTime = time(nullptr);
	language = "English";
}

ServerConnection::~ServerConnection()
{
	// Clear Playerlist
	clearPlayerList();

//	// Delete server from SQL serverlist.
//	CString query = CString("DELETE FROM ") << settings->getStr("serverlist") << " WHERE name='" << name.escape() << "'";
//	mySQL->add_simple_query(query.text());
}

/*
	Loops
*/
bool ServerConnection::doMain(const time_t& now)
{
	// sock exist?
	if (_socket == nullptr)
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
		// Update the data timeout.
		_lastData = now;

		if (packetCount == 0)
		{
			packetCount++;

			if (sockBuffer.bytesLeft() >= 8)
			{
				CString potentialVersion = sockBuffer.subString(0, 8);
				if (potentialVersion == "GNP1905C") {
					handleOfflineForwarding();
					return false;
				}
			}
		}

		// definitions
		CString unBuffer;

		if (new_protocol)
		{
			sockBuffer.setRead(0);
			while (sockBuffer.length() >= 2)
			{
				// packet length
				auto len = (unsigned short)sockBuffer.readShort();
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

	// Handle disconnect
	if (_disconnect)
	{
		if (!_disconnectMsg.empty())
			sendPacket(CString() >> (char)SVO_ERRMSG << _disconnectMsg, true);

		return false;
	}

	// Send a ping every 30 seconds.
	auto diff = (int)difftime(now, _lastPing);
	if (diff >= 30)
	{
		_lastPing = now;
		sendPacket(CString() >> (char)SVO_PING);
	}

	// send out buffer
	sendCompress();
	return true;
}

void ServerConnection::handleOfflineForwarding()
{
	_listServer->getServerLog().out("New Protocol client connected! Forwarding client!\n");

	auto server = _listServer->getServer("Offline");

	if (server)
	{
		CString pPacket = CString() << "offline,Offline," << server->getIp() << "," << server->getPort();
		sendGNPPacket(PLO_SERVERWARP, pPacket);
	}
	else
	{
		sendGNPPacket(PLO_DISCMESSAGE, CString() << "No available server! Try again later");
	}
}

void ServerConnection::sendGNPPacket(char packetType, CString &pPacket)
{
	// We ignore appendNL here because new protocol doesn't end with newlines
	CString buf2 = CString() << (char)0 << (char)0/*packetCount*/;
	buf2.writeInt3(pPacket.length() + 6, false);
	buf2.writeChar(packetType, false);
	buf2.write(pPacket.text(), pPacket.length(), true);

	// send buffer
	unsigned int dsize = buf2.length();
	_socket->sendData(buf2.text(), &dsize);
}

bool ServerConnection::canAcceptClient(ClientType clientType)
{
	if (clientType == ClientType::AllServers)
		return true;

	return (_allowedVersionsMask & static_cast<uint8_t>(clientType)) != 0;
}

/*
	Send disconnect message to the server
*/
void ServerConnection::disconnectServer(const std::string& errmsg)
{
	_disconnect = true;
	_disconnectMsg = errmsg;
}

void ServerConnection::enableServerHQ(const ServerHQ& server)
{
	_isServerHQ = true;
	_serverMaxLevel = server.maxLevel;
	_serverUpTime = server.uptime;

	printf("Enabled serverhq for %s\n", server.serverName.c_str());
}

CString ServerConnection::getPlayers() const
{
	// Update our player list.
	CString playerlist;
	for (const auto& player : playerList)
	{
		if ((player->getClientType() & PLTYPE_ANYCLIENT) != 0)
			playerlist << CString(CString() << player->getAccountName() << "\n" << player->getNickName()).gtokenize() << "\n";
	}

	return playerlist;
}

/*
	Get-Value Functions
*/
CString ServerConnection::getIp(const CString& pIp) const
{
	if (pIp == ip)
	{
		if (localip.isEmpty())
			return "127.0.0.1";
		return localip;
	}

	return ip;
}

CString ServerConnection::getType(ClientType clientType) const
{
	if (clientType == ClientType::Version1)
		return "";

	switch (_serverLevel)
	{
		case ServerHQLevel::G3D:
			return "3 ";
		case ServerHQLevel::Gold:
			return "P ";
		case ServerHQLevel::Classic:
			return "";
		case ServerHQLevel::Bronze:
			return "H ";
		case ServerHQLevel::Hidden:
			return "U ";

		default:
			return "";
	}
}

CString ServerConnection::getServerPacket(ClientType clientType, const CString& clientIp) const
{
	CString serverIp = getIp(clientIp);
	CString playerCount((int)playerList.size());
	return CString() >> (char)8 >> (char)(getType(clientType).length() + getName().length()) << getType(clientType) << getName() >> (char)getLanguage().length() << getLanguage() >> (char)getDescription().length() << getDescription() >> (char)getUrl().length() << getUrl() >> (char)getVersion().length() << getVersion() >> (char)playerCount.length() << playerCount >> (char)serverIp.length() << serverIp >> (char)getPort().length() << getPort();
}

ServerPlayer * ServerConnection::getPlayer(unsigned short id) const
{
	for (auto player : playerList)
	{
		if (player->getId() == id)
			return player;
	}

	return nullptr;
}

ServerPlayer * ServerConnection::getPlayer(const std::string & account) const
{
	for (auto player : playerList)
	{
		if (player->getAccountName() == account)
			return player;
	}

	return nullptr;
}

ServerPlayer * ServerConnection::getPlayer(const std::string & account, int type) const
{
	for (auto player : playerList)
	{
		if (player->getClientType() == type && player->getAccountName() == account)
			return player;
	}

	return nullptr;
}

void ServerConnection::clearPlayerList()
{
	// clean playerlist
	IrcServer *ircServer = _listServer->getIrcServer();
	for (auto player : playerList) {
		//ircServer->removePlayer(player->getIrcStub());
		delete player;
	}
	playerList.clear();
}

void ServerConnection::sendText(const CString& data)
{
	CString dataPacket;
	dataPacket.writeGChar(SVO_SENDTEXT);
	dataPacket << data;
	sendPacket(dataPacket);
}

void ServerConnection::sendTextForPlayer(ServerPlayer *player, const CString& data)
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
		if (!outBuffer.isEmpty())
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

		// read id from packet
		uint8_t id = curPacket.readGUChar();
		if (id >= SVI_PACKETCOUNT)
		{
			_listServer->getServerLog().out("Invalid packet received from Server [%d]: %s (%d)\n", id, curPacket.text() + 1, curPacket.length());
			_listServer->getServerLog().out("\tServer Name: %s\n", getName().text());
			_listServer->getServerLog().out("\tServer IP Address: %s:%s\n", getIp().text(), getPort().text());
			return false;
		}

		printf("Server Packet [%d]: %s (%d)\n", id, curPacket.text() + 1, curPacket.length());

		// valid packet, call function
		bool ret = (*this.*serverFunctionTable[id])(curPacket);
		if (!ret)
		{
			_listServer->getServerLog().out("Packet %u failed for server %s.\n", id, name.text());
		}
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

void sanitizeServerName(CString& serverName)
{
	// Remove all server type strings from the name of the server.
	while (serverName.subString(0, 2) == "U " || serverName.subString(0, 2) == "P " || serverName.subString(0, 2) == "H " || serverName.subString(0, 2) == "3 ")
		serverName.removeI(0, 2);
}

bool ServerConnection::msgSVI_SETNAME(CString& pPacket)
{
	CString serverName = pPacket.readString("");
	CString oldName(name);

	sanitizeServerName(serverName);

	// Shouldn't we be checking for blank names?
	if (serverName.isEmpty())
		return false;

	if (!_listServer->updateServerName(this, serverName.text(), _serverAuthToken)) {
		return false;
	}

	_isAuthenticated = true;
	name = serverName;

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
	return true;
}

bool ServerConnection::msgSVI_SETLANG(CString& pPacket)
{
	language = pPacket.readString("");
	return true;
}

bool ServerConnection::msgSVI_SETVERS(CString& pPacket)
{
	CString ver(pPacket.readString(""));
	switch (_serverProtocol)
	{
		case ProtocolVersion::Version1:
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

		case ProtocolVersion::Version2:
		{
			if (ver.match("?.??.?") || ver.match("?.?.?"))
				version = CString("Version: ") << ver;
			else if (ver.match("?.??.?-beta") || ver.match("?.?.?-beta") || ver.match("?.?.?-beta *") || ver.match("?.??.?-beta *"))
				version = CString("Beta Version: ") << ver.subString(0, ver.find('-'));
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

		default:
			version = ver;
			break;
	}
	//SQLupdate("version", version);
	return true;
}

bool ServerConnection::msgSVI_SETURL(CString& pPacket)
{
	url = pPacket.readString("");
	return true;
}

bool ServerConnection::msgSVI_SETIP(CString& pPacket)
{
	ip = pPacket.readString("");
	if (ip == "AUTO")
		ip = _socket->getRemoteIp();

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
			auto playerObject = new ServerPlayer(this, _listServer->getIrcServer());
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
	uint16_t playerId = pPacket.readGUShort();
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString nickname = pPacket.readChars(pPacket.readGUChar());
	CString guild = pPacket.readChars(pPacket.readGUChar());

	// Verify the account.
	GuildStatus status = _listServer->verifyGuild(account.text(), nickname.text(), guild.text());
	if (status == GuildStatus::Valid)
	{
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
	for (const auto& playerObject : playerList)
	{
		if (playerObject->getAccountName() == accountName)
			playerObject->setNickName(nickName);
	}

	return true;
}

bool ServerConnection::msgSVI_GETPROF(CString& pPacket)
{
	uint16_t playerId = pPacket.readGUShort();

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
	std::string accountName = pPacket.readChars(pPacket.readGUChar()).text();

	// Verify this is a player on the server.
	auto player = getPlayer(accountName);
	if (player)
	{
		// Construct profile from packet
		PlayerProfile newProfile(accountName);
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
	}

	return true;
}

bool ServerConnection::msgSVI_PLYRADD(CString& pPacket)
{
	ServerPlayer *playerObject;
	CString propPacket;
	uint8_t clientType;

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
		playerObject = new ServerPlayer(this, _listServer->getIrcServer());
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

	CString identity;
	if (pPacket.bytesLeft())
		identity = pPacket.readChars(pPacket.readGUShort());

	// Validate account credentials
	AccountStatus status = _listServer->verifyAccount(account.text(), password.text());

	// Check if the packet has identity-data to fetch a pc id
	if (status == AccountStatus::Normal && identity.length())
	{
		auto pcId = _listServer->getDataStore()->getDeviceId(identity);
		if (pcId.has_value())
		{
			_listServer->getDataStore()->updateDeviceIdTime(pcId.value());

			// Guest accounts account-names get set to 'pc:id'
			auto pcIdAsString = std::to_string(pcId.value());
			if (account == "guest")
				account = CString("pc:") << pcIdAsString;

			sendPacket(CString() >> (char)SVO_ASSIGNPCID >> (short)id >> (char)type << (char)pcIdAsString.length() << pcIdAsString);
		}
	}

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
	_serverProtocol = ProtocolVersion::Version2;

	CString name = pPacket.readChars(pPacket.readGUChar());
	CString description = pPacket.readChars(pPacket.readGUChar());
	CString language = pPacket.readChars(pPacket.readGUChar());
	CString version = pPacket.readChars(pPacket.readGUChar());
	CString url = pPacket.readChars(pPacket.readGUChar());
	CString ip = pPacket.readChars(pPacket.readGUChar());
	CString port = pPacket.readChars(pPacket.readGUChar());
	CString localip = pPacket.readChars(pPacket.readGUChar());

	// Set up the server.
	msgSVI_SETDESC(description);
	msgSVI_SETLANG(language);
	msgSVI_SETVERS(version);
	msgSVI_SETURL(url);
	msgSVI_SETIP(ip);
	msgSVI_SETLOCALIP(localip);
	msgSVI_SETPORT(port);			// Port last.
	if (!msgSVI_SETNAME(name)) return false;

	// TODO(joey): Send remote ip address
	{
		CString dataPacket;
		dataPacket.writeGChar(SVO_SENDTEXT);
		dataPacket << "Listserver,SetRemoteIp," << _socket->getRemoteIp();
		sendPacket(dataPacket);
	}

	// TODO(joey): temporary
	auto& serverList = _listServer->getConnections();
	for (const auto& server : serverList)
	{
		CString dataPacket;
		dataPacket.writeGChar(SVO_SENDTEXT);
		dataPacket << "Listserver,Modify,Server," << server->getName().gtokenize() << ",players=" << CString(server->getPlayerCount());
		sendPacket(dataPacket);
	}

	return true;
}

bool ServerConnection::msgSVI_SERVERHQPASS(CString& pPacket)
{
	_serverAuthToken = pPacket.readString("").text();
	return true;
}

bool ServerConnection::msgSVI_SERVERHQLEVEL(CString& pPacket)
{
	_serverLevel = getServerHQLevel(pPacket.readGUChar());
	if (_serverLevel > _serverMaxLevel)
		_serverLevel = _serverMaxLevel;

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

	// Fetch the players ip address so we can forward the client to the localip if its on the same host
	CString userIp;
	auto player = getPlayer(playerId);
	if (player)
		userIp = player->getIpAddress();

	auto& serverList = _listServer->getConnections();
	for (const auto& server : serverList)
	{
		if (servername.comparei(server->getName()))
		{
			CString serverPacket = CString(server->getName()) << "\n" << server->getName() << "\n" << server->getIp(userIp) << "\n" << server->getPort();
			sendPacket(CString() >> (char)SVO_SERVERINFO >> (short)playerId << serverPacket.gtokenize());
			break;
		}
	}

	return true;
}

bool ServerConnection::msgSVI_PMPLAYER(CString& pPacket)
{
	uint16_t id = pPacket.readGUShort();
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
			//if (params[0] == "GraalEngine") // Only GraalEngine on v5 and older
			{
				if (params[1] == "irc")
				{
					if (params.size() == 4)
					{
						IrcServer* ircServer = _listServer->getIrcServer();
						if (params[2] == "join") // GraalEngine,irc,join,#channel
						{
							std::string channel = params[3].guntokenize().text();
							ircServer->addPlayerToChannel(channel, player->getIrcStub());
						}
						else if (params[2] == "part") // GraalEngine,irc,part,#channel
						{
							std::string channel = params[3].guntokenize().text();
							ircServer->removePlayerFromChannel(channel, player->getIrcStub());
						}
					}
					else if (params.size() == 6 && params[2] == "privmsg")
					{
						std::string from = params[3].text();
						std::string channel = params[4].text();
						std::string message = params[5].text();

						//ServerPlayer *fromPlayer = getPlayer(from);
						if (player)
							_listServer->getIrcServer()->sendTextToChannel(channel, message, player->getIrcStub());
					}
				}
				else if (params[1] == "pmservers") // GraalEngine,pmservers,""
				{
					CString sendMsg;
					sendMsg << params[0] << "\n" << params[1] << "\n" << params[2] << "\n";

					// Assemble the serverlist.
					auto& serverList = _listServer->getConnections();
					for (auto& server : serverList)
					{
						if (server->getServerLevel() == ServerHQLevel::Hidden)
							continue;

						if (server->canAcceptClient(ClientType::Version4))
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
					sendMsg << params[0] << "\n" << params[1] << "\n" << params[2] << "\n";

					// get servers
					auto& serverList = _listServer->getConnections();
					for (auto & server : serverList)
					{
						if (server->getName() == params[2])
							sendMsg << server->getPlayers();
					}
					sendTextForPlayer(player, sendMsg.gtokenize());
				}


				// This is the one case where params[0] may not be GraalEngine
				if (params[1] == "lister") // -Serverlist,lister,simpleserverlist ----- -Serverlist is the weapon
				{
					// simplelist is what is sent from the client, simpleserverlist is the response but we butchered
					// the initial request so i'm leaving it for older gservers
					if (params[2] == "simplelist" || params[2] == "simpleserverlist")
					{
						CString sendMsg;
						sendMsg << params[0] << "\n" << params[1] << "\n" << "simpleserverlist" << "\n";

						// Assemble the serverlist.
						auto& serverList = _listServer->getConnections();
						for (auto & server : serverList)
						{
							if (server->isAuthenticated() && server->canAcceptClient(ClientType::Version4))
							{
								//if (server->getTypeVal() == TYPE_HIDDEN) continue;

								CString serverData;
								serverData << server->getName() << "\n";
								serverData << server->getType(ClientType::AllServers) << server->getName() << "\n";
								serverData << CString(server->getPlayerCount()) << "\n";
								sendMsg << serverData.gtokenize() << "\n";
							}
						}

						// TODO(joey): Show hidden servers if friends are on them...?
						//p << getOwnedServersPM(account);
						sendMsg.gtokenizeI();
						sendTextForPlayer(player, sendMsg);
					}
					else if (params.size() == 5 && params[2] == "verifybuddies")
					{
						// params[3] -> boolean $pref::Graal::loadbuddylistfromserver
						// params[4] -> checksum (from crc32)

						if (params[3] == "1")
						{
							CString sendMsg;
							sendMsg << params[0] << "\n" << params[1] << "\n" << "buddylist" << "\n";

							auto buddyList = _listServer->getDataStore()->getBuddyList(player->getAccountName());
							if (buddyList)
							{
								for (const auto& buddy : buddyList.value())
									sendMsg << buddy << "\n";
							}

							sendTextForPlayer(player, sendMsg.gtokenize());
						}
					}
					else if (params.size() == 4)
					{
						if (params[2] == "addbuddy") {
							_listServer->getDataStore()->addBuddy(player->getAccountName(), params[3].text());
						}
						else if (params[2] == "deletebuddy") {
							_listServer->getDataStore()->removeBuddy(player->getAccountName(), params[3].text());
						}
					}
				}
			}
		}
	}

	return true;
}

bool ServerConnection::msgSVI_REQUESTSVRINFO(CString& pPacket)
{
	uint16_t playerId = pPacket.readGUShort();
	ServerPlayer *player = getPlayer(playerId);
	CString data = pPacket.readString("");

	CString data2 = data.guntokenize();
	CString weapon = data2.readString("\n");
	CString type = data2.readString("\n");
	CString option = data2.readString("\n");
	CString params = data2.readString("\n");

	// Untokenize our params.
	params.guntokenizeI();

	// Find the server.
	CString servername = params.readString("\n");
	auto& serverList = _listServer->getConnections();
	for (auto& server : serverList)
	{
		if (server == nullptr) continue;
		if (servername.comparei(server->getName()))
		{
			CString p;
			p << weapon << "\n";
			p << type << "\n";
			p << option << "\n";
			p << player->getAccountName() << "\n";
			p << server->getName() << "\n";
			p << server->getType(ClientType::AllServers) << server->getName() << "\n";
			p << server->getPlayerCount() << "\n";
			p << server->getLanguage() << "\n";
			p << server->getDescription() << "\n";
			p << server->getUrl() << "\n";
			p << server->getVersion() << "\n";
			p.gtokenizeI();

			// Send the server info back to the server.
			sendPacket(CString() >> (char)SVO_REQUESTTEXT >> (short)playerId << p);
			return true;
		}
	}

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

					ServerPlayer *player = getPlayer(from);
					_listServer->getIrcServer()->sendTextToChannel(channel, message, player->getIrcStub());
				}
			}
		}
		else if (params[0] == "Listserver")
		{
			if (params[1] == "settings")
			{
				if (params[2] == "allowedversions")
				{
					_allowedVersionsMask = 0;

					auto versionCount = params.size() - 3;
					if (versionCount > 0)
					{
						for (size_t i = 0; i < versionCount; i++)
						{
							_allowedVersionsMask |= getVersionMask(params[3 + i]);
						}
					}
				}
			}
		}
	}

	return true;
}

bool ServerConnection::msgSVI_REQUESTBUDDIES(CString& pPacket)
{
	uint16_t pid = pPacket.readGUShort();
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
