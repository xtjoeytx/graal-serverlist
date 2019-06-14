#ifndef IRCCHANNEL_H
#define IRCCHANNEL_H

#pragma once

#include <unordered_set>
#include <string>
#include <vector>

class IrcConnection;
class ServerConnection;
class ServerPlayer;

class IrcChannel
{
public:
	IrcChannel(const std::string& name)
		: _channelName(name) {
	}

	void addUser(ServerPlayer *player)
	{
		_users.insert(player);
	}

	void removeUser(ServerPlayer *player)
	{
		_users.erase(player);
	}

	size_t getUserCount() const {
		return _users.size();
	}

	void sendMessage(ServerPlayer *from, const std::string& message);

	std::unordered_set<ServerPlayer*>* getUsers() { return &_users; };
private:
	std::string _channelName;
	std::unordered_set<ServerPlayer *> _users;
};

#endif
