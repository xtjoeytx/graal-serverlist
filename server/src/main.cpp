#include <stdlib.h>
#include <signal.h>
#include <chrono>
#include <iostream>
#include <thread>
#include "ListServer.h"
#include "PlayerConnection.h"
#include "ServerConnection.h"

#include "IConfig.h"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <unistd.h>
	#include <dirent.h>
#endif

void shutdownServer(int signal);

// Function pointer for signal handling.
typedef void (*sighandler_t)(int);

// Home path of the serverlist.
std::string getBasePath()
{
	CString homepath;

#if defined(_WIN32) || defined(_WIN64)
	// Get the path.
	char path[ MAX_PATH ];
	GetModuleFileNameA(0, path, MAX_PATH);

	// Find the program exe and remove it from the path.
	// Assign the path to homepath.
	homepath = path;
	int pos = homepath.findl('\\');
	if (pos == -1) homepath.clear();
	else if (pos != (homepath.length() - 1))
		homepath.removeI(++pos, homepath.length());
#elif __APPLE__
	char path[255];
	if (!getcwd(path, sizeof(path)))
		printf("Error getting CWD\n");

	homepath = path;
	if (homepath[homepath.length() - 1] != '/')
		homepath << '/';
#else
	// Get the path to the program.
	char path[260];
	memset((void*)path, 0, 260);
	readlink("/proc/self/exe", path, sizeof(path));

	// Assign the path to homepath.
	char* end = strrchr(path, '/');
	if (end != 0)
	{
		end++;
		if (end != 0) *end = '\0';
		homepath = path;
	}
#endif
	return homepath.text();
}

const char * getErrorString(InitializeError error)
{
	switch (error)
	{
		case InitializeError::None:
			return "Success";

		case InitializeError::InvalidSettings:
			return "Could not read settings";

		case InitializeError::ServerSock_Init:
			return "Could not initialize server socket";

		case InitializeError::ServerSock_Listen:
			return "Could not listen on server socket";

		case InitializeError::PlayerSock_Init:
			return "Could not initialize player socket";

		case InitializeError::PlayerSock_Listen:
			return "Could not listen on player socket";

		case InitializeError::IrcSock_Init:
			return "Could not initialize irc socket";

		case InitializeError::IrcSock_Listen:
			return "Could not listen on irc socket";

		case InitializeError::Backend_Error:
			return "Could not connect to backend";

		default:
			return "";
	}
}

ListServer *listServer = nullptr;
std::thread listThread;

int main(int argc, char *argv[])
{
	// Shut down the server if we get a kill signal.
	signal(SIGINT, (sighandler_t) shutdownServer);
	signal(SIGTERM, (sighandler_t) shutdownServer);

	// Grab the base path to the server executable.
	std::string homePath = getBasePath();

	// Setup listserver
	listServer = new ListServer(homePath);
	InitializeError err = listServer->Initialize();
	if (err != InitializeError::None)
	{
		listServer->getServerLog().out("[Error] %s", getErrorString(err));
		listServer->Cleanup();
		return -1;
	}

	listThread = std::thread(&ListServer::Main, listServer);

	// A CLI interface??? MAYBE... if I have time -joey
	while (true)
	{
		std::string command;
		std::cout << "Input Command: ";
		std::cin >> command;

		printf("Command sent: |%s|\n", command.c_str());
		if (command == "quit")
		{
			listServer->setRunning(false);
			break;
		}
	}

	listThread.join();
	listServer->Cleanup();
	delete listServer;
	return 0;
}

void shutdownServer(int signal)
{
	if (listServer != nullptr)
	{
		listServer->setRunning(false);

		listThread.join();
		listServer->Cleanup();
		delete listServer;
	}
}

/*
	// Initialize data directory.
	filesystem[0].addDir("global");
	filesystem[1].addDir("global/heads");
	filesystem[2].addDir("global/bodies");
	filesystem[3].addDir("global/swords");
	filesystem[4].addDir("global/shields");

	// Definitions
	CSocket playerSock, playerSockOld, serverSock;
	playerList.clear();
	serverList.clear();

	// Load Settings
	settings = new CSettings(homepath + "settings.ini");
	if (!settings->isOpened())
	{
		serverlog.out( "[Error] Could not load settings.\n" );
		return ERR_SETTINGS;
	}

	// Load ip bans.
	ipBans = CString::loadToken(homepath + "ipbans.txt", "\n", true);
	serverlog.out("Loaded following IP bans:\n");
	for (std::vector<CString>::iterator i = ipBans.begin(); i != ipBans.end(); ++i)
		serverlog.out("\t%s\n", i->text());

	// Load server types.
	serverTypes = CString::loadToken("servertypes.txt", "\n", true);

	// Server sock.
	serverSock.setType( SOCKET_TYPE_SERVER );
	serverSock.setProtocol( SOCKET_PROTOCOL_TCP );
	serverSock.setDescription( "serverSock" );
	CString serverInterface = settings->getStr("gserverInterface");
	if (serverInterface == "AUTO") serverInterface.clear();
	if ( serverSock.init( (serverInterface.isEmpty() ? 0 : serverInterface.text()), settings->getStr("gserverPort").text() ) )
	{
		serverlog.out( "[Error] Could not initialize sockets.\n" );
		return ERR_LISTEN;
	}

	// Player sock.
	playerSock.setType( SOCKET_TYPE_SERVER );
	playerSock.setProtocol( SOCKET_PROTOCOL_TCP );
	playerSock.setDescription( "playerSock" );
	CString playerInterface = settings->getStr("clientInterface");
	if (playerInterface == "AUTO") playerInterface.clear();
	if ( playerSock.init( (playerInterface.isEmpty() ? 0 : playerInterface.text()), settings->getStr("clientPort").text() ) )
	{
		serverlog.out( "[Error] Could not initialize sockets.\n" );
		return ERR_LISTEN;
	}

	// Player sock.
	playerSockOld.setType( SOCKET_TYPE_SERVER );
	playerSockOld.setProtocol( SOCKET_PROTOCOL_TCP );
	playerSockOld.setDescription( "playerSockOld" );
	if ( playerSockOld.init( (playerInterface.isEmpty() ? 0 : playerInterface.text()), settings->getStr("clientPortOld").text() ) )
	{
		serverlog.out( "[Error] Could not initialize sockets.\n" );
		return ERR_LISTEN;
	}

	// Connect sockets.
	if ( serverSock.connect() || playerSock.connect() || playerSockOld.connect() )
	{
		serverlog.out( "[Error] Could not connect sockets.\n" );
		return ERR_SOCKETS;
	}

	// MySQL-Connect
#ifndef NO_MYSQL
	mySQL = new CMySQL(settings->getStr("server").text(), settings->getStr("user").text(), settings->getStr("password").text(), settings->getStr("database").text(), settings->getStr("port").text(), settings->getStr("sockfile").text());
	vBmySQL = new CMySQL(settings->getStr("server").text(), settings->getStr("vbuser").text(), settings->getStr("vbpassword").text(), settings->getStr("vbdatabase").text(), settings->getStr("vbport").text(), settings->getStr("sockfile").text());
	if (mySQL->ping() != 0)
	{
		serverlog.out( "[Error] No response from MySQL.\n" );
		return ERR_MYSQL;
	}

	// Truncate servers table from MySQL.
	CString query;
	query << "TRUNCATE TABLE " << settings->getStr("serverlist");
	mySQL->add_simple_query(query.text());
	query = CString("TRUNCATE TABLE ") << settings->getStr("securelogin");
	mySQL->add_simple_query(query.text());
	mySQL->update();
#endif

	// Create Packet-Functions
	createPLFunctions();
	createSVFunctions();

	// Flavor text
	serverlog.out( CString() << "Graal Reborn - List Server v" << LISTSERVER_VERSION <<"\n"
					<< "List server started..\n"
					<< "Client port: " << CString(settings->getInt("clientport")) << "\n"
					<< "GServer port: " << CString(settings->getInt("gserverport")) << "\n" );
#ifdef NO_MYSQL
	// Notify the user that they are running in No MySQL mode
	serverlog.out( CString() << "Running in No MySQL mode, all account and guild checks are disabled.\n");
#endif

	// Main Loop
	time_t t5min = time(0);
	time_t t30s = time(0);
	while (running)
	{
		time_t now = time(0);

		// Make sure MySQL is active
#ifndef NO_MYSQL
		if (mySQL->ping() != 0)
		{
			if ((int)difftime(now, t30s) > 30)
			{
				if (!mySQL->connect()) {
					serverlog.out("[Error] Could not reconnect to MySQL. Trying again in 30 seconds.\n");
				}
				else serverlog.out("Reconnected to MySQL Server\n");

				t30s = now;
			}
		}
		else mySQL->update();
#endif

		// Accept New Connections
		acceptSock(playerSock, SOCK_PLAYER);
		acceptSock(playerSockOld, SOCK_PLAYEROLD);
		acceptSock(serverSock, SOCK_SERVER);

		// Player Sockets
		for ( std::vector<TPlayer*>::iterator iter = playerList.begin(); iter != playerList.end(); )
		{
			TPlayer* player = (TPlayer*)*iter;
			if ( player->doMain() == false )
			{
				player->sendCompress();
				delete player;
				iter = playerList.erase( iter );
			}
			else
				++iter;
		}

		// Server Sockets
		for ( std::vector<ServerConnection*>::iterator iter = serverList.begin(); iter != serverList.end() ; )
		{
			ServerConnection* server = (ServerConnection*)*iter;
			// THIS SHOULD NOT BE CALLED ANYMORE, AND CAN LIKELY BE REMOVED.
			if (server == 0)
			{
				serverlog.out(CString() << "Server disconnected: [Orphaned server]\n");
				iter = serverList.erase(iter);
				continue;
			}

			// Check for a timed out server.
			if ((int)server->getLastData() >= 300)
			{
				serverlog.out(CString() << "Server disconnected: " << server->getName() << " [Timed out]\n");
				server->sendCompress();
				delete server;
				iter = serverList.erase(iter);
				continue;
			}

			// Execute server stuff.
			if (server->doMain() == false)
			{
				serverlog.out(CString() << "Server disconnected: " << server->getName() << "\n");
				server->sendCompress();
				delete server;
				iter = serverList.erase(iter);
				continue;
			}
			
			++iter;
		}

		// Every 5 minutes...
		// Reload ip bans.
		// Resync the file system.
		if ((int)difftime(now, t5min) > 300)
		{
			ipBans = CString::loadToken("ipbans.txt", "\n", true);
			serverTypes = CString::loadToken("servertypes.txt", "\n", true);
			for (int i = 0; i < 5; ++i)
				filesystem[i].resync();
			t5min = now;
		}

		// Wait
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// Remove all servers.
	// This guarantees the server deconstructors are called.
	for ( std::vector<ServerConnection*>::iterator iter = serverList.begin(); iter != serverList.end() ; )
	{
		ServerConnection* server = (ServerConnection*)*iter;
		delete server;
		iter = serverList.erase( iter );
	}

	return ERR_SUCCESS;
}

void acceptSock(CSocket& pSocket, int pType)
{
	CSocket* newSock = pSocket.accept();
	if (newSock == 0)
		return;

	// Server ip bans.
	if (pType == SOCK_SERVER)
	{
		CString ip(newSock->getRemoteIp());
		for (std::vector<CString>::const_iterator i = ipBans.begin(); i != ipBans.end(); ++i)
		{
			if (ip.match(*i))
			{
				printf("Rejected server: %s matched ip ban %s\n", ip.text(), i->text());
				newSock->disconnect();
				delete newSock;
				return;
			}
		}
	}

	//newSock->setOptions( SOCKET_OPTION_NONBLOCKING );
	serverlog.out(CString() << "New Connection: " << CString(newSock->getRemoteIp()) << " -> " << ((pType == SOCK_PLAYER) ? "Player" : "Server") << "\n");
	if (pType == SOCK_PLAYER || pType == SOCK_PLAYEROLD)
		playerList.push_back(new PlayerConnection(nullptr, newSock, (pType == SOCK_PLAYEROLD ? true : false)));
	else serverList.push_back(new ServerConnection(newSock));
}

CString getAccountError(int pErrorId)
{
	switch (pErrorId)
	{
		case ACCSTAT_NORMAL:
		return "SUCCESS";

		case ACCSTAT_NONREG:
		return "Your account is not activated.";

		case ACCSTAT_BANNED:
		return "Your account is globally banned.";

		case ACCSTAT_INVALID:
		return "Account name or password is invalid.";

		case ACCSTAT_ERROR:
		return "There was a problem verifying your account.  The SQL server is probably down.";

		default:
		return "Invalid Error Id";
	}
}

CString getServerList(int PLVER, const CString& pIp)
{
	// definitions
	CString packet;

	// serverlist count
	packet.writeGChar(serverList.size());

	// get servers
	for (std::vector<ServerConnection*>::iterator i = serverList.begin(); i != serverList.end(); ++i)
	{
		ServerConnection* server = (ServerConnection*)*i;
		if (server == 0) continue;

		if (server->getName().length() != 0)
			packet << server->getServerPacket(PLVER, pIp);
	}
	return packet;
}

CString getServerPlayers(CString& servername)
{
	// definitions
	CString packet;

	// serverlist count
	//packet.writeGChar(serverList.size());

	// get servers
	for (std::vector<ServerConnection*>::iterator i = serverList.begin(); i != serverList.end(); ++i)
	{
		ServerConnection* server = (ServerConnection*)*i;
		if (server == 0) continue;

		if (server->getName() == servername)
			packet << server->getPlayers();
	}
	return packet;
}

int getPlayerCount()
{
	return (int)playerList.size();
}

int getServerCount()
{
	return (int)serverList.size();
}

CString getOwnedServers(CString& pAccount)
{
#ifdef NO_MYSQL
	return "";
#else
	CString query;
	std::vector<std::vector<std::string> > result;
	query << "SELECT off.id as id, off.name as name, onn.playercount as playercount, off.curlevel as curlevel, uid.account as account, coalesce(onn.online, 0) as isOnline FROM graal_serverhq as off LEFT JOIN graal_servers as onn ON (off.name=onn.name) LEFT JOIN graal_users as uid ON (off.userid=uid.id) WHERE onn.online = 1 AND curlevel = 0 AND account = '" << pAccount.escape() << "' ORDER BY off.name ASC;";
	int ret = mySQL->try_query_rows(query.text(), result);

	CString servers;
	// does the player have any vBulletin friends?
	if (ret == -1 || result.size() == 0)
		return "";
	else
	{
		ServerConnection * srv;
		CString srv1;
		for (unsigned int i = 0; i < result.size(); i++)
		{
			if (!result[i].empty())
			{
				// TODO(joey): unsure if my change worked, ill have to check back at it
				srv1 = "";
				srv1 << result[i][1] <<"\n";
				srv1 << srv->getType(std::stoi(result[i][3])) << result[i][1] <<"\n";
				srv1 << result[i][2] <<"\n";
				srv1.gtokenizeI();

				servers << srv1 << "\n";
			}
		}

		//buddies.gtokenizeI();

		return servers;
	}
#endif
}

CString getOwnedServersPM(CString& pAccount)
{
#ifdef NO_MYSQL
	return "";
#else
	CString query;
	std::vector<std::vector<std::string> > result;
	query << "SELECT off.id as id, off.name as name, onn.playercount as playercount, off.curlevel as curlevel, uid.account as account, coalesce(onn.online, 0) as isOnline FROM graal_serverhq as off LEFT JOIN graal_servers as onn ON (off.name=onn.name) LEFT JOIN graal_users as uid ON (off.userid=uid.id) WHERE onn.online = 1 AND curlevel = 0 AND account = '" << pAccount.escape() << "' ORDER BY off.name ASC;";
	int ret = mySQL->try_query_rows(query.text(), result);

	CString servers;
	// does the player have any vBulletin friends?
	if (ret == -1 || result.size() == 0)
		return "";
	else
	{
		ServerConnection * srv;
		CString srv1;
		for (unsigned int i = 0; i < result.size(); i++)
		{
			if (!result[i].empty())
			{
				srv1 = "";
				srv1 << result[i][1];

				servers << srv1 << "\n";
			}
		}

		//buddies.gtokenizeI();

		return servers;
	}
#endif
}

CString getBuddies(CString& pAccount)
{
#ifdef NO_MYSQL
	return "";
#else
	CString query;
	std::vector<std::vector<std::string> > result;
	query << "SELECT user2.username as buddy FROM user LEFT JOIN userlist ON (user.userid=userlist.userid) LEFT JOIN user as user2 ON  (userlist.relationid=user2.userid) WHERE user.username LIKE '" << pAccount.escape() << "' AND userlist.type LIKE 'buddy' AND userlist.friend LIKE 'yes';";
	int ret = vBmySQL->try_query_rows(query.text(), result);

	CString buddies;
	// does the player have any vBulletin friends?
	if (ret == -1 || result.size() == 0)
		return "";
	else
	{
		for (unsigned int i = 0; i < result.size(); i++)
		{
			if (!result[i].empty())
				buddies << result[i][0] <<"\n";
		}

		buddies.gtokenizeI();

		return buddies;
	}
#endif
}

int verifyAccount(CString& pAccount, const CString& pPassword, bool fromServer)
{
#ifdef NO_MYSQL
	return ACCSTAT_NORMAL;
#else
	// definitions
	CString query;
	std::vector<std::string> result;
	CString password(pPassword);

	// make sure its not empty.
	if (pAccount.length() == 0)
		return ACCSTAT_INVALID;

	// See if we should try a secure login.
	int ret = ACCSTAT_INVALID;
	if (password.find("\xa7") != -1)
	{
		CString transaction = password.readString("\xa7");
		CString md5password = password.readString("");

		// Try our password.
		result.clear();
		query = CString() << "SELECT activated, banned, account FROM `" << settings->getStr("userlist") << "` WHERE account='" << pAccount.escape() << "' AND transactionnr='" << transaction.escape() << "' AND password2='" << md5password.escape() << "' LIMIT 1";
		int err = mySQL->try_query(query.text(), result);

		// account/password correct?
		if (err == -1)
			ret = ACCSTAT_ERROR;
		else if (result.size() == 0)
			ret = ACCSTAT_INVALID;
		else if (result.size() >= 1 && result[0] == "0")
			ret = ACCSTAT_NONREG;
		else if (result.size() >= 2 && result[1] == "1")
			ret = ACCSTAT_BANNED;
		else
			ret = ACCSTAT_NORMAL;

		// Should we expire the password now?
		if (ret != ACCSTAT_INVALID && ret != ACCSTAT_ERROR)
		{
			unsigned char login_type = (char)(atoi(transaction.text()) & 0xFF);

			// Password expires after one server login.  Remove it.
			if (login_type == SECURELOGIN_ONEUSE && fromServer == true)
			{
				query.clear();
				query << "UPDATE `" << settings->getStr("userlist") << "` SET "
					<< "transactionnr='0',"
					<< "salt2='',"
					<< "password2='' "
					<< "WHERE account='" << pAccount.escape() << "'";
				mySQL->add_simple_query(query.text());
			}
		}

		// Get the correct account name.
		if (result.size() == 3)
			pAccount = result[2];

		return ret;
	}

	// If our ret is ACCSTAT_ERROR, that means there was a SQL error.
	if (ret == ACCSTAT_ERROR)
		return ret;

	// Either the secure login failed or we didn't try a secure login.
	// Try the old login method.
	if (ret == ACCSTAT_INVALID)
	{
		result.clear();
		query = CString() << "SELECT password, salt, activated, banned, account FROM `" << settings->getStr("userlist") << "` WHERE account='" << pAccount.escape() << "' AND password=" << "MD5(CONCAT(MD5('" << pPassword.escape() << "'), `salt`)) LIMIT 1";
		int err = mySQL->try_query(query.text(), result);
		if (err == -1) return ACCSTAT_ERROR;

		// account/password correct?
		if (result.size() == 0)
			return ACCSTAT_INVALID;

		// activated?
		if (result.size() < 3 || result[2] == "0")
			return ACCSTAT_NONREG;

		// banned?
		if (result.size() > 3 && result[3] == "1")
			return ACCSTAT_BANNED;

		// Get the case-sensitive account name.
		if (result.size() > 4)
			pAccount = result[4];

		// passed all tests :)
		return ACCSTAT_NORMAL;
	}

	return ACCSTAT_INVALID;
#endif
}

int verifyGuild(const CString& pAccount, const CString& pNickname, const CString& pGuild)
{
#ifdef NO_MYSQL
	return GUILDSTAT_ALLOWED;
#else
	CString query;
	std::vector<std::string> result;

	// Check parameters.
	if (pAccount.length() < 1 || pGuild.length() < 1)
		return GUILDSTAT_DISALLOWED;

	// Construct the query.
	query << "SELECT * FROM " << settings->getStr("guild_names") << " WHERE name='" << pGuild.escape() << "' AND status='1'";

	// Send the query.
	int err = mySQL->try_query(query.text(), result);
	if (err != -1)
	{
		// Get the nick restriction.
		int restrictNick = 0;
		if (result.size() == 3)
			restrictNick = std::stoi(result[2]);

		query = CString() << "SELECT * FROM " << settings->getStr("guild_members") << " WHERE guild='" << pGuild.escape() << "' AND account='" << pAccount.escape() << "'";
		if (restrictNick != 0)
			query << " AND nickname='" << pNickname.escape() << "'";

		err = mySQL->try_query(query.text(), result);
		return ((err == -1 || err == 0) ? GUILDSTAT_DISALLOWED : GUILDSTAT_ALLOWED);
	}

	return GUILDSTAT_DISALLOWED;
#endif
}
*/

//// 2002-05-07 by Markus Ewald
//CString CString_Base64_Encode(const CString& input)
//{
//	static const char *EncodeTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//
//	CString retVal;
//
//	for (int i = 0; i < input.length(); i++)
//	{
//		char pCode;
//
//		pCode = (input[i] >> 2) & 0x3f;
//		retVal.writeChar(EncodeTable[pCode]);
//
//		pCode = (input[i] << 4) & 0x3f;
//		if (i++ < input.length())
//			pCode |= (input[i] >> 4) & 0x0f;
//		retVal.writeChar(EncodeTable[pCode]);
//
//		if (i < input.length())
//		{
//			pCode = (input[i] << 2) & 0x3f;
//			if (i++ < input.length())
//				pCode |= (input[i] >> 6) & 0x03;
//			retVal.writeChar(EncodeTable[pCode]);
//		}
//		else
//		{
//			i++;
//			retVal.writeChar('=');
//		}
//
//		if (i < input.length())
//		{
//			pCode = input[i] & 0x3f;
//			retVal.writeChar(EncodeTable[pCode]);
//		}
//		else
//		{
//			retVal.writeChar('=');
//		}
//	}
//
//	return retVal;
//}
//
//CString CString_Base64_Decode(const CString& input)
//{
//	static const int DecodeTable[] = {
//		// 0   1   2   3   4   5   6   7   8   9
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  //   0 -   9
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  //  10 -  19
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  //  20 -  29
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  //  30 -  39
//		-1, -1, -1, 62, -1, -1, -1, 63, 52, 53,  //  40 -  49
//		54, 55, 56, 57, 58, 59, 60, 61, -1, -1,  //  50 -  59
//		-1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  //  60 -  69
//		 5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  //  70 -  79
//		15, 16, 17, 18, 19, 20, 21, 22, 23, 24,  //  80 -  89
//		25, -1, -1, -1, -1, -1, -1, 26, 27, 28,  //  90 -  99
//		29, 30, 31, 32, 33, 34, 35, 36, 37, 38,  // 100 - 109
//		39, 40, 41, 42, 43, 44, 45, 46, 47, 48,  // 110 - 119
//		49, 50, 51, -1, -1, -1, -1, -1, -1, -1,  // 120 - 129
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 130 - 139
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 140 - 149
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 150 - 159
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 160 - 169
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 170 - 179
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 180 - 189
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 190 - 199
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 200 - 209
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 210 - 219
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 220 - 229
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 230 - 239
//		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 240 - 249
//		-1, -1, -1, -1, -1, -1				   // 250 - 256
//	};
//
//	CString retVal;
//
//	for (int i = 0; i < input.length(); i++)
//	{
//		unsigned char c1, c2;
//
//		c1 = (char)DecodeTable[(unsigned char)input[i]];
//		i++;
//		c2 = (char)DecodeTable[(unsigned char)input[i]];
//		c1 = (c1 << 2) | ((c2 >> 4) & 0x3);
//		retVal.writeChar(c1);
//
//		if (i++ < input.length())
//		{
//			c1 = input[i];
//			if (c1 == '=')
//				break;
//
//			c1 = (char)DecodeTable[(unsigned char)input[i]];
//			c2 = ((c2 << 4) & 0xf0) | ((c1 >> 2) & 0xf);
//			retVal.writeChar(c2);
//		}
//
//		if (i++ < input.length())
//		{
//			c2 = input[i];
//			if (c2 == '=')
//				break;
//
//			c2 = (char)DecodeTable[(unsigned char)input[i]];
//			c1 = ((c1 << 6) & 0xc0) | c2;
//			retVal.writeChar(c1);
//		}
//	}
//
//	return retVal;
//}

