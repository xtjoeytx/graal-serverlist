#ifndef LISTSERVER_H
#define LISTSERVER_H

#pragma once

#include <string>
#include <vector>
#include "CFileSystem.h"
#include "CLog.h"
#include "CSettings.h"
#include "CSocket.h"
#include "CString.h"

class IDataBackend;
class TPlayer;
class TServer;

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

private:
	bool _initialized;
	CFileSystem _fileSystem[5];
	CLog _clientLog;
	CLog _serverLog;
	CSettings _settings;
	CSocket _playerSock, _serverSock;
	std::string _homePath;
	std::vector<TPlayer *> _playerConnections;
	std::vector<TServer *> _serverConnections;

	IDataBackend *_dataStore;
	std::chrono::high_resolution_clock::time_point _lastTimer;

	// TBD:
	std::vector<CString> _ipBans;
	std::vector<CString> _serverTypes;
};

#endif
