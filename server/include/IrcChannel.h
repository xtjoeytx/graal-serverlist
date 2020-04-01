#ifndef IRCCHANNEL_H
#define IRCCHANNEL_H

#pragma once

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

class IrcStub;

struct IrcUserData {
	IrcStub* user;
	char mode;
};

struct IrcUserDataHash {
	std::size_t operator()(const IrcUserData& _node) const {
		return std::hash<IrcStub*>()(_node.user);
	}

	bool operator()(const IrcUserData& user1, const IrcUserData& user2) const {
		return (user1.user == user2.user);
	}
};

using IrcUserDataSet = std::unordered_set<IrcUserData, IrcUserDataHash, IrcUserDataHash>;

class IrcChannel
{
public:
	IrcChannel(const std::string& name)
		: _channelName(name) {
	}

	size_t getUserCount() const;
	const std::string& getChannelName() const;
	const IrcUserDataSet& getUserList() const { return _users; }
	const char getUserMode(const std::string& nickName) const;
	void setUserMode(std::string nickName, char mode);

	bool addUser(IrcStub *ircUser);
	bool removeUser(IrcStub *ircUser);
	void sendMessage(const std::string& message, IrcStub *from);

private:
	std::string _channelName;
	IrcUserDataSet _users;
	std::unordered_map<std::string, char> _userMode;
};

inline size_t IrcChannel::getUserCount() const
{
	return _users.size();
}

inline const std::string& IrcChannel::getChannelName() const
{
	return _channelName;
}

inline const char IrcChannel::getUserMode(const std::string& nickName) const
{
	auto it = _userMode.find(nickName);
	return (it != _userMode.end() ? it->second : ' ');
}

#endif
