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

class IrcStub;

class IrcChannel
{
public:
	IrcChannel(const std::string& name)
		: _channelName(name) {
	}

	size_t getUserCount() const;
	const std::string& getChannelName() const;
	const std::unordered_set<IrcStub *>& getUserList() const { return _users; }

	bool addUser(IrcStub *ircUser);
	bool removeUser(IrcStub *ircUser);
	void sendMessage(const std::string& message, IrcStub *from);

private:
	std::string _channelName;
	std::unordered_set<IrcStub *> _users;
};

inline size_t IrcChannel::getUserCount() const
{
	return _users.size();
}

inline const std::string& IrcChannel::getChannelName() const
{
	return _channelName;
}

#endif
