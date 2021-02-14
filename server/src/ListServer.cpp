#include <cassert>
#include <chrono>
#include <thread>
#include "ListServer.h"
#include "IrcConnection.h"
#include "PlayerConnection.h"
#include "ServerConnection.h"
#include "ServerPlayer.h"

#ifndef NO_MYSQL
#include "MySQLBackend.h"
#endif

// TODO(joey): Move this somewhere else
const char * getAccountError(AccountStatus status)
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

ListServer::ListServer(std::string homePath)
		: _initialized(false), _running(false), _homePath(std::move(homePath)), _dataStore(nullptr), _ircServer(this)
{
	_clientLog.setFilename(/*_homePath +*/ "clientlog.txt");
	_serverLog.setFilename(/*_homePath +*/ "serverlog.txt"); // note: CLog does its own homepath check
}

ListServer::~ListServer()
{
	Cleanup();
}

InitializeError ListServer::Initialize()
{
	// Already initialized
	if (_initialized)
		return InitializeError::None;

	// Load settings
	_settings.setSeparator("=");
	if (!_settings.loadFile(_homePath + "settings.ini"))
		return InitializeError::InvalidSettings;

	// Bind the server socket
	CString serverInterface = _settings.getStr("gserverInterface");

	if (serverInterface == "AUTO")
		serverInterface.clear();
	_serverSock.setType(SOCKET_TYPE_SERVER);
	_serverSock.setProtocol(SOCKET_PROTOCOL_TCP);
	_serverSock.setDescription("serverSock");

	if (_serverSock.init((serverInterface.isEmpty() ? 0 : serverInterface.text()), _settings.getStr("gserverPort").text()))
		return InitializeError::ServerSock_Init;
	if (_serverSock.connect())
		return InitializeError::ServerSock_Listen;

	// Bind the player socket
	CString clientInterface = _settings.getStr("clientInterface");
	if (clientInterface == "AUTO")
		clientInterface.clear();

	_playerSock.setType(SOCKET_TYPE_SERVER);
	_playerSock.setProtocol(SOCKET_PROTOCOL_TCP);
	_playerSock.setDescription("playerSock");

	if (_playerSock.init((clientInterface.isEmpty() ? 0 : clientInterface.text()), _settings.getStr("clientPort").text()))
		return InitializeError::PlayerSock_Init;
	if (_playerSock.connect())
		return InitializeError::PlayerSock_Listen;

	// TODO(joey): Old player sock?? Unsure what its for, leaving out for now. (11/25/19) - its for v141


	// TODO(joey): Create different data backends (likely do a text-based one as well)
	_dataStore = std::make_unique<MySQLBackend>(_settings.getStr("server").text(), _settings.getInt("port"), _settings.getStr("sockfile").text(),
												_settings.getStr("user").text(), _settings.getStr("password").text(), _settings.getStr("database").text());

	// Connect to backend
	if (_dataStore->Initialize())
		return InitializeError::Backend_Error;

	// Bind the irc socket
	if (!_ircServer.Initialize(_dataStore.get(), _homePath, 6667))
		return InitializeError::IrcSock_Listen;

	_initialized = true;
	return InitializeError::None;
}

void ListServer::Cleanup()
{
	if (!_initialized)
		return;

	// Sync servers to database
	syncServers();

	// Delete the players
	_playerConnections.clear();

	// Delete the servers
	_serverConnections.clear();

	// Disconnect sockets
	_serverSock.disconnect();
	_playerSock.disconnect();

	// Cleanup the IRC Server
	_ircServer.Cleanup();

	if (_dataStore)
	{
		_dataStore->Cleanup();
		_dataStore.reset();
	}

	// Stop running
	setRunning(false);
	_initialized = false;
}

void ListServer::acceptSock(CSocket& socket, SocketType socketType)
{
	auto newSock = socket.accept();
	if (newSock == nullptr)
		return;

	auto& log = (socketType == SocketType::Server ? getServerLog() : getClientLog());

	std::string ipAddress(newSock->getRemoteIp());
	if (_dataStore->isIpBanned(ipAddress))
	{
		log.out("New connection from %s was rejected due to an ip ban!\n", newSock->getRemoteIp());
		newSock->disconnect();
		delete newSock;
		return;
	}

	log.out("New Connection from %s -> %s\n", ipAddress.c_str(), (socketType == SocketType::Server ? "Server" : "Player"));

	switch (socketType)
	{
		case SocketType::PlayerOld:
		case SocketType::Player:
		{
			auto newPlayer = std::make_unique<PlayerConnection>(this, newSock);

			if (socketType == SocketType::PlayerOld)
				newPlayer->sendServerList();

			_playerConnections.push_back(std::move(newPlayer));
			break;
		}

		case SocketType::Server:
		{
			_serverConnections.push_back(std::make_unique<ServerConnection>(this, newSock));
			break;
		}
	}
}

bool ListServer::Main()
{
	if (!_initialized || _running)
		return false;

	setRunning(true);

	std::vector<std::unique_ptr<PlayerConnection>> removePlayers;
	std::vector<std::unique_ptr<ServerConnection>> removeServers;

	time_t lastSync = time(nullptr);

	while (_running)
	{
		time_t now = time(nullptr);

		// accept sockets
		acceptSock(_playerSock, SocketType::Player);
		acceptSock(_serverSock, SocketType::Server);

		// PLAYER LOOP
		{
			// iterate player connections
			for (auto it = _playerConnections.begin(); it != _playerConnections.end();)
			{
				auto& conn = *it;
				if (conn->doMain())
					++it;
				else
				{
					removePlayers.push_back(std::move(conn));
					it = _playerConnections.erase(it);
				}
			}

			// remove players
			removePlayers.clear();
		}

		// SERVER LOOP
		{
			// iterate server connections
			for (auto it = _serverConnections.begin(); it != _serverConnections.end();)
			{
				auto& conn = *it;
				if (conn->doMain(now))
					++it;
				else
				{
					removeServers.push_back(std::move(conn));
					it = _serverConnections.erase(it);
				}
			}

			// remove servers
			for (auto& conn : removeServers)
				removeServer(conn.get());
			removeServers.clear();
		}

		// IRC Server main loop
		_ircServer.Main();

		// Persist serverhq data
		if (difftime(now, lastSync) >= 300) {
			syncServers();
			lastSync = now;
		}

		// Ping datastore (flush updates to db, or text)
		_dataStore->Ping();

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	return true;
}

void ListServer::setRunning(bool status)
{
	std::lock_guard<std::mutex> guard(pc);
	_running = status;
}

void ListServer::sendPacketToServers(const CString & packet, ServerConnection * sender) const
{
	for (const auto & conn : _serverConnections)
	{
		if (sender != conn.get())
			conn->sendPacket(packet);
	}
}

bool ListServer::updateServerName(ServerConnection *pConnection, const std::string &serverName, const std::string &authToken)
{
	auto response = _dataStore->verifyServerHQ(serverName, authToken);

	switch (response.status)
	{
		case ServerHQStatus::BackendError:
			pConnection->disconnectServer("Could not establish connection to database");
			return false;

		case ServerHQStatus::InvalidPassword:
		case ServerHQStatus::NotActivated:
			pConnection->disconnectServer("Could not authenticate your server");
			return false;

		case ServerHQStatus::Unregistered:
		case ServerHQStatus::Valid:
			break;
	}

	bool authoritative = (response.status == ServerHQStatus::Valid);

	// TODO(joey): non-authoritative servers could technically disconnect each other and hijack names
	if (authoritative)
	{
		for (auto& conn : _serverConnections)
		{
			if (pConnection != conn.get() && conn->getName() == serverName)
			{
				conn->disconnectServer("Servername is already in use!");
			}
		}
	
		pConnection->enableServerHQ(response.serverHq);
	}
	else
	{
		for (auto& conn : _serverConnections)
		{
			if (pConnection != conn.get() && conn->getName() == serverName)
			{
				if (pConnection->getIp() == conn->getIp() && pConnection->getPort() == conn->getPort())
				{
					conn->disconnectServer("A duplicate server has been found, disconnecting server!");
					return true;
				}

				pConnection->disconnectServer("Servername is already in use!");
				return false;
			}
		}
	}

	return true;
}

void ListServer::removeServer(ServerConnection *conn)
{
	_dataStore->updateServerUpTime(conn->getName().text(), conn->getUpTime());

	// Notify other servers this server will be removed
	CString dataPacket;
	dataPacket.writeGChar(SVO_SENDTEXT);
	dataPacket << "Listserver,Modify,Server," << conn->getName().gtokenize() << ",players=-1";
	sendPacketToServers(dataPacket);
}

void ListServer::syncServers()
{
	for (auto& conn : _serverConnections)
	{
		if (conn->isServerHQ())
		{
			_dataStore->updateServerUpTime(conn->getName().text(), conn->getUpTime());
		}
	}
}