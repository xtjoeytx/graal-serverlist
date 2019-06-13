#ifndef LISTSERVER_H
#define LISTSERVER_H

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

//#include "CFileSystem.h"
#include "CLog.h"
#include "CSettings.h"
#include "CSocket.h"
#include "CString.h"
#include "IDataBackend.h"

//class IDataBackend;
class PlayerConnection;
class ServerConnection;
class IrcConnection;

enum class InitializeError
{
	None,
	InvalidSettings,
	ServerSock_Init,
	ServerSock_Listen,
	PlayerSock_Init,
	PlayerSock_Listen,
	IrcSock_Init,
	IrcSock_Listen,
	Backend_Error
};

enum class SocketType
{
	PlayerOld,
	Player,
	Server,
	IRC
};

class IrcChannel;
class ServerPlayer;
class ListServer
{
public:
	explicit ListServer(const std::string& homePath);
	~ListServer();

	InitializeError Initialize();
	void Cleanup();
	bool Main();

	CLog & getClientLog()				{ return _clientLog; }
	CLog & getServerLog()				{ return _serverLog; }
	CSettings & getSettings()			{ return _settings; }
	
	const CString& getServerlistPacket() const { return CString(); }
	std::vector<ServerConnection *> & getConnections() { return _serverConnections; }
	std::vector<IrcConnection *> & getIrcConnections() { return _ircConnections; }

	// Chatroom Functionality
	IrcChannel * getChannel(const std::string& channel) const;
	void addPlayerToChannel(const std::string& channel, ServerPlayer *player);
	void removePlayerFromChannel(const std::string& channel, ServerPlayer *player);
	void sendTextToChannel(const std::string& channel, const std::string& from, const std::string& message, ServerConnection *sender = nullptr);

	// Backend functionality
	AccountStatus verifyAccount(const std::string& account, const std::string& password) const;
	GuildStatus verifyGuild(const std::string& account, const std::string& nickname, const std::string& guild) const;
	std::optional<PlayerProfile> getProfile(const std::string& account) const;
	bool setProfile(const PlayerProfile& profile);

	//void sendText(const CString& data);
	//void sendText(const std::vector<CString>& stringList);

	void sendPacketToServers(const CString& packet, ServerConnection *sender = nullptr) const;
	
	void setRunning(bool status)
	{
		std::lock_guard<std::mutex> guard(pc);
		_running = status;
	}

private:
	std::mutex pc;

	bool _initialized;
	bool _running;
	CLog _clientLog;
	CLog _serverLog;
	CSettings _settings;
	CSocket _playerSock, _serverSock, _ircSock;
	std::string _homePath;

	std::vector<PlayerConnection *> _playerConnections;
	std::vector<IrcConnection *> _ircConnections;
	std::vector<ServerConnection *> _serverConnections;
	std::unordered_map<std::string, IrcChannel *> _ircChannels;

	IDataBackend *_dataStore;

	// TBD:
	std::vector<CString> _serverTypes;

	//
	void acceptSock(CSocket& socket, SocketType socketType);
};

inline AccountStatus ListServer::verifyAccount(const std::string& account, const std::string& password) const {
	return _dataStore->verifyAccount(account, password);
}

inline GuildStatus ListServer::verifyGuild(const std::string& account, const std::string& nickname, const std::string& guild) const {
	return _dataStore->verifyGuild(account, nickname, guild);
}

inline std::optional<PlayerProfile> ListServer::getProfile(const std::string& account) const {
	return _dataStore->getProfile(account);
}

inline bool ListServer::setProfile(const PlayerProfile& profile) {
	return _dataStore->setProfile(profile);
}

inline IrcChannel * ListServer::getChannel(const std::string& channel) const {
	auto it = _ircChannels.find(channel);
	if (it == _ircChannels.end())
		return nullptr;
	return it->second;
}

#endif
