#ifndef LISTSERVER_H
#define LISTSERVER_H

#pragma once

#include <string>
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

enum class InitializeError
{
	None,
	InvalidSettings,
	ServerSock_Init,
	ServerSock_Listen,
	PlayerSock_Init,
	PlayerSock_Listen,
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

	CLog & getClientLog() { return _clientLog; }
	CLog & getServerLog() { return _serverLog; }
	CSettings & getSettings() { return _settings; }
	//IDataBackend * getDatabase() const { return _dataStore; }

	void addServerConnection(ServerConnection * connection)
	{

	}

	void addPlayerConnect(PlayerConnection * connection)
	{

	}

	const CString& getServerlistPacket() const { return CString(); }

	AccountStatus verifyAccount(const std::string& account, const std::string& password) const;


private:
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

	// TBD:
	std::vector<CString> _serverTypes;

	//
	void acceptSock(CSocket& socket, SocketType socketType);
};

inline AccountStatus ListServer::verifyAccount(const std::string& account, const std::string& password) const {
	return _dataStore->VerifyAccount(account, password);
}

#endif
