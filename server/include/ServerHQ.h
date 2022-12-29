#ifndef LISTSERVER_SERVERHQ_H
#define LISTSERVER_SERVERHQ_H

#pragma once

#include <string>

enum class ServerHQStatus
{
	BackendError	= -1,
	Valid			= 0,
	NotActivated	= 1,
	InvalidPassword	= 2,
	Unregistered	= 3
};

enum class ServerHQLevel
{
	Hidden =	0,
	Bronze =	1,
	Classic =	2,
	Gold =		3,
	G3D =		4,
};

class ServerHQ
{
public:
	std::string serverName;
	ServerHQLevel maxLevel;
	size_t uptime;

	ServerHQ() : maxLevel(ServerHQLevel::Bronze), uptime(0) { }
	ServerHQ(ServerHQ&& o) = default;
	ServerHQ(const ServerHQ& o) = delete;
};

struct ServerHQResponse
{
	ServerHQStatus status;
	ServerHQ serverHq;
};

inline ServerHQLevel getServerHQLevel(int level)
{
	switch (level)
	{
		case 0: return ServerHQLevel::Hidden;
		case 1: return ServerHQLevel::Bronze;
		case 2: return ServerHQLevel::Classic;
		case 3: return ServerHQLevel::Gold;
		case 4: return ServerHQLevel::G3D;

		default: return ServerHQLevel::Bronze;
	}
}

#endif //LISTSERVER_SERVERHQ_H
