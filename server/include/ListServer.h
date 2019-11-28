#ifndef LISTSERVER_H
#define LISTSERVER_H

#pragma once

#include <assert.h>
#include <string>
#include <unordered_map>
#include <vector>

//#include "CFileSystem.h"
#include "CLog.h"
#include "CSettings.h"
#include "CSocket.h"
#include "CString.h"
#include "IDataBackend.h"
#include "IrcServer.h"

class PlayerConnection;
class ServerConnection;
class IrcConnection;
class IrcChannel;
class ServerPlayer;

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
	Server
};

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
	IrcServer * getIrcServer()  		{ return &_ircServer; }
	CString getServerlistPacket() const { return CString(); }
	std::vector<ServerConnection *> & getConnections() { return _serverConnections; }

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
	CSocket _playerSock, _serverSock;
	std::string _homePath;

	std::vector<PlayerConnection *> _playerConnections;
	std::vector<ServerConnection *> _serverConnections;

	IDataBackend *_dataStore;
	IrcServer _ircServer;

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

#endif
