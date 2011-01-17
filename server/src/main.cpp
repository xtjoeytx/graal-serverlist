#include <stdlib.h>
#include <signal.h>
#include "main.h"
#include "TPlayer.h"
#include "TServer.h"
#include "CLog.h"
#include "CFileSystem.h"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <unistd.h>
	#include <dirent.h>
#endif

// Function pointer for signal handling.
typedef void (*sighandler_t)(int);

bool running = true;
#ifndef NO_MYSQL
	CMySQL *mySQL = NULL;
	CMySQL *vBmySQL = NULL;
#endif
CSettings *settings = NULL;
std::vector<TPlayer *> playerList;
std::vector<TServer *> serverList;

CLog serverlog( "serverlog.txt" );
CLog clientlog( "clientlog.txt" );

std::vector<CString> ipBans;
std::vector<CString> serverTypes;

// Home path of the serverlist.
CString homepath;
static void getBasePath();

// Filesystem.
CFileSystem filesystem[5];

int main(int argc, char *argv[])
{
	// Shut down the server if we get a kill signal.
	signal( SIGINT, (sighandler_t) shutdownServer );
	signal( SIGTERM, (sighandler_t) shutdownServer );

	// Grab the base path to the server executable.
	getBasePath();

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
	settings = new CSettings("settings.ini");
	if (!settings->isOpened())
	{
		serverlog.out( "[Error] Could not load settings.\n" );
		return ERR_SETTINGS;
	}

	// Load ip bans.
	ipBans = CString::loadToken("ipbans.txt", "\n", true);
	serverlog.out("Loaded following IP bans:\n");
	for (std::vector<CString>::iterator i = ipBans.begin(); i != ipBans.end(); ++i)
		serverlog.out("\t%s\n", i->text());

	// Load server types.
	serverTypes = CString::loadToken("servertypes.txt", "\n", true);

	// Server sock.
	serverSock.setType( SOCKET_TYPE_SERVER );
	serverSock.setProtocol( SOCKET_PROTOCOL_TCP );
	serverSock.setOptions( SOCKET_OPTION_NONBLOCKING );
	serverSock.setDescription( "serverSock" );
	CString empty;
	if ( serverSock.init( empty, settings->getStr("gserverPort") ) )
	{
		serverlog.out( "[Error] Could not initialize sockets.\n" );
		return ERR_LISTEN;
	}

	// Player sock.
	playerSock.setType( SOCKET_TYPE_SERVER );
	playerSock.setProtocol( SOCKET_PROTOCOL_TCP );
	playerSock.setOptions( SOCKET_OPTION_NONBLOCKING );
	playerSock.setDescription( "playerSock" );
	if ( playerSock.init( empty, settings->getStr("clientPort") ) )
	{
		serverlog.out( "[Error] Could not initialize sockets.\n" );
		return ERR_LISTEN;
	}

	// Player sock.
	playerSockOld.setType( SOCKET_TYPE_SERVER );
	playerSockOld.setProtocol( SOCKET_PROTOCOL_TCP );
	playerSockOld.setOptions( SOCKET_OPTION_NONBLOCKING );
	playerSockOld.setDescription( "playerSockOld" );
	if ( playerSockOld.init( empty, settings->getStr("clientPortOld") ) )
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
	mySQL = new CMySQL(settings->getStr("server").text(), settings->getStr("user").text(), settings->getStr("password").text(), settings->getStr("database").text(), settings->getStr("sockfile").text());
	vBmySQL =  new CMySQL(settings->getStr("server").text(), settings->getStr("vbuser").text(), settings->getStr("vbpassword").text(), settings->getStr("vbdatabase").text(), settings->getStr("sockfile").text());
	if (!mySQL->ping())
	{
		serverlog.out( "[Error] No response from MySQL.\n" );
		return ERR_MYSQL;
	}

	// Truncate servers table from MySQL.
	CString query;
	query << "TRUNCATE TABLE " << settings->getStr("serverlist");
	mySQL->query(query);
	query = CString("TRUNCATE TABLE ") << settings->getStr("securelogin");
	mySQL->query(query);
#endif

	// Create Packet-Functions
	createPLFunctions();
	createSVFunctions();

	// Flavor text
	serverlog.out( CString() << "Graal Reborn - List Server V2\n"
					<< "List server started..\n"
					<< "Client port: " << CString(settings->getInt("clientport")) << "\n"
					<< "GServer port: " << CString(settings->getInt("gserverport")) << "\n" );
#ifdef NO_MYSQL
	// Notify the user that they are running in No MySQL mode
	serverlog.out( CString() << "Running in No MySQL mode, all account and guild checks are disabled.\n");
#endif

	// Main Loop
	time_t t5min = time(0);
	while (running)
	{
		// Make sure MySQL is active
#ifndef NO_MYSQL
		if (!mySQL->ping())
		{
			serverlog.out( "[Error] No response from MySQL.\n" );
			return ERR_MYSQL;
		}
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
		for ( std::vector<TServer*>::iterator iter = serverList.begin(); iter != serverList.end() ; )
		{
			TServer* server = (TServer*)*iter;
			if (server == 0)
			{
				serverlog.out(CString() << "Server disconnected: [Orphaned server]\n");
				iter = serverList.erase(iter);
				continue;
			}

			if ((int)server->getLastData() >= 300 || server->doMain() == false)
			{
				serverlog.out(CString() << "Server disconnected: " << server->getName() << "\n");
				server->sendCompress();
				delete server;
				iter = serverList.erase( iter );
			}
			else
				++iter;
		}

		// Every 5 minutes...
		// Reload ip bans.
		// Resync the file system.
		if ((int)difftime(time(0), t5min) > (5*60))
		{
			ipBans = CString::loadToken("ipbans.txt", "\n", true);
			serverTypes = CString::loadToken("servertypes.txt", "\n", true);
			for (int i = 0; i < 5; ++i)
				filesystem[i].resync();
			t5min = time(0);
		}

		// Wait
		wait(100);
	}

	// Remove all servers.
	// This guarantees the server deconstructors are called.
	for ( std::vector<TServer*>::iterator iter = serverList.begin(); iter != serverList.end() ; )
	{
		TServer* server = (TServer*)*iter;
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
		CString ip(newSock->tcpIp());
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
	serverlog.out(CString() << "New Connection: " << CString(newSock->tcpIp()) << " -> " << ((pType == SOCK_PLAYER) ? "Player" : "Server") << "\n");
	if (pType == SOCK_PLAYER || pType == SOCK_PLAYEROLD)
		playerList.push_back(new TPlayer(newSock, (pType == SOCK_PLAYEROLD ? true : false)));
	else serverList.push_back(new TServer(newSock));
}

/*
	Extra-Cool Functions :D
*/
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
	for (std::vector<TServer*>::iterator i = serverList.begin(); i != serverList.end(); ++i)
	{
		TServer* server = (TServer*)*i;
		if (server == 0) continue;

		if (server->getName().length() != 0)
			packet << server->getServerPacket(PLVER, pIp);
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

CString getBuddies(CString& pAccount)
{
#ifdef NO_MYSQL
	return "";
#else
	CString query;
	std::vector<std::vector<CString> > result;
	query << "SELECT user2.username as buddy FROM user LEFT JOIN userlist ON (user.userid=userlist.userid) LEFT JOIN user as user2 ON  (userlist.relationid=user2.userid) WHERE user.username LIKE '" << pAccount.escape() << "' AND userlist.type LIKE 'buddy' AND userlist.friend LIKE 'yes';";
	vBmySQL->query_rows(query, &result);

	CString buddies;
	// does the player have any vBulletin friends?
	if (result.size() == 0)
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
	std::vector<CString> result;
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
		query = CString() << "SELECT activated, banned, account FROM `" << settings->getStr("userlist") << "` WHERE account='" << pAccount.escape() << "' AND transaction='" << transaction.escape() << "' AND password2='" << md5password.escape() << "' LIMIT 1";
		mySQL->query(query, &result);

		// account/password correct?
		if (result.size() == 0)
			ret = ACCSTAT_INVALID;
		else if (result.size() >= 1 && result[0] == "0")
			ret = ACCSTAT_NONREG;
		else if (result.size() >= 2 && result[1] == "1")
			ret = ACCSTAT_BANNED;
		else
			ret = ACCSTAT_NORMAL;

		// Should we expire the password now?
		if (ret != ACCSTAT_INVALID)
		{
			unsigned char login_type = (char)(atoi(transaction.text()) & 0xFF);

			// Password expires after one server login.  Remove it.
			if (login_type == SECURELOGIN_ONEUSE && fromServer == true)
			{
				query.clear();
				query << "UPDATE `" << settings->getStr("userlist") << "` SET "
					<< "transaction='0',"
					<< "salt2='',"
					<< "password2='' "
					<< "WHERE account='" << pAccount.escape() << "'";
				mySQL->query(query);
			}
		}

		// Get the correct account name.
		if (result.size() == 3)
			pAccount = result[2];

		return ret;
	}

	// Either the secure login failed or we didn't try a secure login.
	// Try the old login method.
	if (ret == ACCSTAT_INVALID)
	{
		result.clear();
		query = CString() << "SELECT password, salt, activated, banned, account FROM `" << settings->getStr("userlist") << "` WHERE account='" << pAccount.escape() << "' AND password=" << "MD5(CONCAT(MD5('" << pPassword.escape() << "'), `salt`)) LIMIT 1";
		mySQL->query(query, &result);

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
	std::vector<CString> result;

	// Check parameters.
	if (pAccount.length() < 1 || pGuild.length() < 1)
		return GUILDSTAT_DISALLOWED;

	// Construct the query.
	query << "SELECT * FROM " << settings->getStr("guild_names") << " WHERE name='" << pGuild.escape() << "' AND status='1'";

	// Send the query.
	if (mySQL->query(query, &result) != 0)
	{
		// Get the nick restriction.
		int restrictNick = 0;
		if (result.size() == 3)
			restrictNick = atoi(result[2].text());

		query = CString() << "SELECT * FROM " << settings->getStr("guild_members") << " WHERE guild='" << pGuild.escape() << "' AND account='" << pAccount.escape() << "'";
		if (restrictNick != 0)
			query << " AND nickname='" << pNickname.escape() << "'";

		return (mySQL->query(query, &result) == 0 ? GUILDSTAT_DISALLOWED : GUILDSTAT_ALLOWED);
	}

	return GUILDSTAT_DISALLOWED;
#endif
}

void shutdownServer( int signal )
{
	serverlog.out( "Server is now shutting down...\n" );
	running = false;
}

void getBasePath()
{
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
}
