#include "IrcServer.h"
#include "IrcConnection.h"
#include "ListServer.h"

IrcServer::IrcServer(ListServer *listServer)
	: _initialized(false), _listServer(listServer)
{

}

IrcServer::~IrcServer()
{
	Cleanup();
}

bool IrcServer::Initialize(IDataBackend *dataStore, int port)
{
	// Bind the irc socket
	_ircSock.setType(SOCKET_TYPE_SERVER);
	_ircSock.setProtocol(SOCKET_PROTOCOL_TCP);
	_ircSock.setDescription("ircSock");

	if (_ircSock.init(0, "6667"))
		return false; //InitializeError::IrcSock_Init;
	if (_ircSock.connect())
		return false; // InitializeError::IrcSock_Listen;

	_serverHost = getSettings().getStr("listServerAddress").text();
	_dataStore = dataStore;

	return true;
}

void IrcServer::Cleanup()
{
	// Delete the irc clients
	for (auto & _ircConnection : _ircConnections)
		delete _ircConnection;
	_ircConnections.clear();

	// Disconnect binded socket
	_ircSock.disconnect();
}

bool IrcServer::Main()
{
	// Accept sockets
	acceptSock();

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

	return true;
}

void IrcServer::acceptSock()
{
	CSocket *newSock = _ircSock.accept();
	if (newSock == 0)
		return;

	std::string ipAddress(newSock->getRemoteIp());

	if (_dataStore->isIpBanned(ipAddress))
	{
//		getServerLog().append("New connection from %s was rejected due to an ip ban!\n", newSock->getRemoteIp());
		newSock->disconnect();
		delete newSock;
		return;
	}

//	getServerLog().append("New Connection from %s -> %s\n", ipAddress.c_str(), "Player");

	_ircConnections.push_back(new IrcConnection(this, newSock));
}

CSettings & IrcServer::getSettings()
{
	return _listServer->getSettings();
}

AccountStatus IrcServer::verifyAccount(const std::string& account, const std::string& password) const
{
	return _dataStore->verifyAccount(account, password);
}

void IrcServer::removePlayer(IrcStub *ircUser)
{
	assert(ircUser);

	std::vector<IrcChannel *> deadChannels;

	for (auto & _ircChannel : _ircChannels)
	{
		IrcChannel *channel = _ircChannel.second;

		if (channel->removeUser(ircUser))
		{
			size_t channelUserCount = channel->getUserCount();
			if (channelUserCount == 1)
				deadChannels.push_back(channel);
		}
	}

	for (IrcChannel *channel : deadChannels) {
		_ircChannels.erase(channel->getChannelName());
		delete channel;
	}
}

bool IrcServer::addPlayerToChannel(const std::string& channel, IrcStub *ircUser)
{
	assert(ircUser);

	IrcChannel *channelObject = getChannel(channel);
	if (channelObject == nullptr)
	{
		channelObject = new IrcChannel(channel);
		_ircChannels[channel] = channelObject;
	}

	return channelObject->addUser(ircUser);
}

bool IrcServer::removePlayerFromChannel(const std::string& channel, IrcStub *ircUser)
{
	assert(ircUser);

	IrcChannel *channelObject = getChannel(channel);
	if (channelObject == nullptr)
		return false;

	if (channelObject->removeUser(ircUser))
	{
		if (channelObject->getUserCount() == 0)
		{
			_ircChannels.erase(channel);
			delete channelObject;
		}

		return true;
	}

	return false;
}

void IrcServer::sendTextToChannel(const std::string& channel, const std::string& message, IrcStub *ircUser)
{
	IrcChannel *channelObject = getChannel(channel);
	if (channelObject != nullptr)
		channelObject->sendMessage(message, ircUser);
}
