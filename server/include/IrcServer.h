#ifndef IRCSERVER_H
#define IRCSERVER_H

#pragma once

#include <assert.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "CLog.h"
#include "CSettings.h"
#include "CSocket.h"
#include "IDataBackend.h"
#include "IrcChannel.h"

class ListServer;
class IrcConnection;
class IrcChannel;
class IrcStub;

class IrcServer
{
public:
	IrcServer(ListServer *listServer);
	~IrcServer();

	bool Initialize(IDataBackend *dataStore, const std::string& homePath, int port);
	void Cleanup();
	bool Main();

	CLog & getIrcLog()				{ return _ircLog; }
	CSettings & getSettings();
	std::vector<IrcConnection *> & getIrcConnections() { return _ircConnections; }
	const std::string& getHostName() const { return _serverHost; }

	// Chatroom Functionality
	IrcChannel * getChannel(const std::string& channel) const;

	void removePlayer(IrcStub *ircUser);

	bool addPlayerToChannel(const std::string& channel, IrcStub *ircUser);

	bool removePlayerFromChannel(const std::string& channel, IrcStub *ircUser);

	void sendTextToChannel(const std::string& channel, const std::string& message, IrcStub *ircUser);

	// Backend functionality
	AccountStatus verifyAccount(const std::string& account, const std::string& password) const;

private:
	bool _initialized;
	CLog _ircLog;
	CSocket _ircSock;
	IDataBackend *_dataStore;
	ListServer *_listServer;
	std::string _serverHost;

	std::vector<IrcConnection *> _ircConnections;
	std::unordered_map<std::string, IrcChannel *> _ircChannels;

	void acceptSock();
};

//////////////////
#include "IrcChannel.h"

inline IrcChannel * IrcServer::getChannel(const std::string& channel) const {
	auto it = _ircChannels.find(channel);
	if (it == _ircChannels.end())
		return nullptr;
	return it->second;
}

#endif //LISTSERVER_IRCSERVER_H
