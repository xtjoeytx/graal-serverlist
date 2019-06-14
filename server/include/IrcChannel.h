#ifndef IRCCHANNEL_H
#define IRCCHANNEL_H

#pragma once

#include <map>
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

	void subscribe(ServerConnection *connection)
	{
		auto it = _serverSubscribers.find(connection);
		if (it != _serverSubscribers.end())
		{
			it->second++;
			return;
		}

		_serverSubscribers[connection] = 1;
	}

	void unsubscribe(ServerConnection *connection)
	{
		auto it = _serverSubscribers.find(connection);
		if (it != _serverSubscribers.end())
		{
			it->second--;
			if (it->second == 0)
				_serverSubscribers.erase(it);
		}
	}

	void subscribe(IrcConnection *connection) {
		_ircSubscribers.insert(connection);
	}

	void unsubscribe(IrcConnection *connection) {
		_ircSubscribers.erase(connection);
	}

	size_t getUserCount() const {
		return _users.size();
	}

	void sendMessage(const std::string& from, const std::string& message, void *sender = 0);

private:
	std::string _channelName;
	std::map<ServerConnection *, size_t> _serverSubscribers;
	std::unordered_set<IrcConnection *> _ircSubscribers;
	std::unordered_set<ServerPlayer *> _users;
};

#endif
