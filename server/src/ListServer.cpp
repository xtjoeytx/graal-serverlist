#include <chrono>
#include <thread>
#include <assert.h>
#include "ListServer.h"
#include "IrcConnection.h"
#include "PlayerConnection.h"
#include "ServerConnection.h"
#include "ServerPlayer.h"
#include "IrcChannel.h"

#ifndef NO_MYSQL
#include "MySQLBackend.h"
#endif

ListServer::ListServer(const std::string& homePath)
	: _initialized(false), _running(false), _homePath(homePath), _dataStore(nullptr)
{
	_clientLog.setFilename(_homePath + "clientlog.txt");
	_serverLog.setFilename(_homePath + "serverlog.txt");
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

	// Load server types
	//_serverTypes = CString::loadToken("servertypes.txt", "\n", true);

	// Bind the irc socket
	_ircSock.setType(SOCKET_TYPE_SERVER);
	_ircSock.setProtocol(SOCKET_PROTOCOL_TCP);
	_ircSock.setDescription("ircSock");

	if (_ircSock.init(0, "6667"))
		return InitializeError::IrcSock_Init;
	if (_ircSock.connect())
		return InitializeError::IrcSock_Listen;

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

	// TODO(joey): Old player sock?? Unsure what its for, leaving out for now



	// TODO(joey): Create different data backends (likely do a text-based one as well)
	_dataStore = new MySQLBackend(_settings.getStr("server").text(), _settings.getInt("port"), _settings.getStr("sockfile").text(),
		_settings.getStr("user").text(), _settings.getStr("password").text(), _settings.getStr("database").text());

	// Connect to backend
	if (_dataStore->Initialize())
		return InitializeError::Backend_Error;

	_initialized = true;
	return InitializeError::None;
}

void ListServer::Cleanup()
{
	if (!_initialized)
		return;

	// Delete the players
	for (auto it = _playerConnections.begin(); it != _playerConnections.end(); ++it)
		delete *it;
	_playerConnections.clear();

	// Delete the servers
	for (auto it = _serverConnections.begin(); it != _serverConnections.end(); ++it)
		delete *it;
	_serverConnections.clear();

	// Delete the irc clients
	for (auto it = _ircConnections.begin(); it != _ircConnections.end(); ++it)
		delete *it;
	_ircConnections.clear();

	// Disconnect sockets
	_serverSock.disconnect();
	_playerSock.disconnect();
	_ircSock.disconnect();

	if (_dataStore)
	{
		_dataStore->Cleanup();
		delete _dataStore;
		_dataStore = nullptr;
	}

	// Stop running
	setRunning(false);
	_initialized = false;
}

void ListServer::acceptSock(CSocket& socket, SocketType socketType)
{
	CSocket* newSock = socket.accept();
	if (newSock == 0)
		return;

	std::string ipAddress(newSock->getRemoteIp());

	if (_dataStore->isIpBanned(ipAddress))
	{
		getServerLog().append("New connection from %s was rejected due to an ip ban!\n", newSock->getRemoteIp());
		newSock->disconnect();
		delete newSock;
		return;
	}

	getServerLog().append("New Connection from %s -> %s\n", ipAddress.c_str(), (socketType == SocketType::Server ? "Server" : "Player"));

	switch (socketType)
	{
		case SocketType::PlayerOld:
		case SocketType::Player:
		{
			PlayerConnection *newPlayer = new PlayerConnection(this, newSock);
			_playerConnections.push_back(newPlayer);

			if (socketType == SocketType::PlayerOld)
				newPlayer->sendServerList();
			break;
		}

		case SocketType::Server:
			_serverConnections.push_back(new ServerConnection(this, newSock));
			break;

		case SocketType::IRC:
			_ircConnections.push_back(new IrcConnection(this, newSock));
			break;

		default:
			newSock->disconnect();
			delete newSock;
			break;
	}
}

bool ListServer::Main()
{
	if (!_initialized || _running)
		return false;

	setRunning(true);

	auto currentTimer = std::chrono::high_resolution_clock::now();
	std::chrono::high_resolution_clock::time_point _lastTimer;

	while (_running)
	{
		// accept sockets
		acceptSock(_playerSock, SocketType::Player);
		acceptSock(_serverSock, SocketType::Server);
		acceptSock(_ircSock, SocketType::IRC);

		// iterate player connections
		for (auto it = _playerConnections.begin(); it != _playerConnections.end();)
		{
			PlayerConnection *conn = *it;
			if (conn->doMain())
				++it;
			else
			{
				delete conn;
				it = _playerConnections.erase(it);
			}
		}

		// iterate server connections
		for (auto it = _serverConnections.begin(); it != _serverConnections.end();)
		{
			ServerConnection *conn = *it;
			if (conn->doMain())
				++it;
			else
			{
				delete conn;
				it = _serverConnections.erase(it);
			}
		}

		// iterate irc connections
		for (auto it = _ircConnections.begin(); it != _ircConnections.end();)
		{
			IrcConnection *conn = *it;
			if (conn->doMain())
				++it;
			else
			{
				delete conn;
				it = _ircConnections.erase(it);
			}
		}

		// do whatever

		// ping datastore (flush updates to db, or text whatever)
		_dataStore->Ping();

		_lastTimer = currentTimer;
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	return true;
}

void ListServer::addPlayerToChannel(const std::string& channel, ServerPlayer *player)
{
	assert(player);

	IrcChannel *channelObject = getChannel(channel);
	if (channelObject == nullptr)
	{
		channelObject = new IrcChannel(channel);
		_ircChannels[channel] = channelObject;
	}

	channelObject->addUser(player);
}

void ListServer::removePlayerFromChannel(const std::string& channel, ServerPlayer *player)
{
	assert(player);

	IrcChannel *channelObject = getChannel(channel);
	if (channelObject == nullptr)
		return;

	channelObject->removeUser(player);
	if (channelObject->getUserCount() == 0)
	{
		_ircChannels.erase(channel);
		delete channelObject;
	}
}

void ListServer::sendTextToChannel(const std::string& channel, const std::string& from, const std::string& message, ServerConnection *sender)
{
	// TODO(joey): gtokenize for std::string?
	CString forwardPacket;
	forwardPacket.writeGChar(SVO_SENDTEXT);
	forwardPacket << "GraalEngine,irc,privmsg,";
	forwardPacket << CString(from).gtokenize() << "," << CString(channel).gtokenize() << "," << CString(message).gtokenize();

	// TODO(joey): Only send to servers that have a player in the channel
	auto serverList = getConnections();
	for (auto it = serverList.begin(); it != serverList.end(); ++it)
	{
		ServerConnection *server = *it;
		if (server != sender)
			server->sendPacket(forwardPacket);
	}
}

void ListServer::sendPacketToServers(const CString & packet, ServerConnection * sender) const
{
	for (auto it = _serverConnections.begin(); it != _serverConnections.end(); ++it)
	{
		ServerConnection *conn = *it;
		if (conn != sender)
			conn->sendPacket(packet);
	}
}

void ListServer::sendMessage(std::string channel, ServerPlayer *from, std::string message) {
	auto sendChannel = getChannel(channel);
	if (sendChannel != nullptr)
		sendChannel->sendMessage(from->getAccountName(), message);

}
