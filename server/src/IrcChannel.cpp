#include <cassert>
#include "IrcChannel.h"
#include "IrcStub.h"

bool IrcChannel::addUser(IrcStub *ircUser)
{
	assert(ircUser);

	auto insertIter = _users.insert(IrcUserData{ ircUser, 0 });

	if (insertIter.second) {
		ircUser->sendJoinedChannel(_channelName, _users);

		for (auto& userData : _users) {
			if (userData.user != ircUser) {
				userData.user->sendUserJoinedChannel(_channelName, ircUser);
			}
		}
	}

	return insertIter.second;
}

bool IrcChannel::removeUser(IrcStub *ircUser)
{
	assert(ircUser);

	IrcUserData userData{ ircUser, 0 };

	size_t count = _users.erase(userData);
	if (count == 0)
		return false;

	ircUser->sendLeftChannel(_channelName, "");

	for (auto& ircStub : _users) {
		ircStub.user->sendUserLeftChannel(_channelName, ircUser);
	}

	return true;
}

void IrcChannel::sendMessage(const std::string& message, IrcStub *sender)
{
	for (auto& userData : _users)
	{
		if (userData.user != sender)
			userData.user->sendMessage(_channelName, message, sender->getNickName());
	}
}

void IrcChannel::setUserMode(const std::string& nickName, char mode)
{
	_userMode[nickName] = mode;

	// update existing user
	for (auto userData : _users)
	{
		if (userData.user->getNickName() == nickName)
		{
			userData.mode = mode;
		}
	}
}
