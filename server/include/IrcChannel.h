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

	size_t getUserCount() const;
	const std::unordered_set<ServerPlayer *>& getUserList() const { return _users; }

	void addUser(ServerPlayer *player);
	void removeUser(ServerPlayer *player);
	void sendMessage(const std::string& from, const std::string& message, void *sender = 0);

	void subscribe(IrcConnection *connection);
	void subscribe(ServerConnection *connection);

	void unsubscribe(IrcConnection *connection);
	void unsubscribe(ServerConnection *connection);

private:
	std::string _channelName;
	std::map<ServerConnection *, size_t> _serverSubscribers;
	std::unordered_set<IrcConnection *> _ircSubscribers;
	std::unordered_set<ServerPlayer *> _users;
};

inline size_t IrcChannel::getUserCount() const
{
	return _users.size();
}

inline void IrcChannel::subscribe(IrcConnection *connection)
{
	_ircSubscribers.insert(connection);
}

inline void IrcChannel::unsubscribe(IrcConnection *connection)
{
	_ircSubscribers.erase(connection);
}

#endif
