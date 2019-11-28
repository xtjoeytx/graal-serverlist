#include "IrcChannel.h"
#include "IrcConnection.h"
#include "ServerConnection.h"

bool IrcChannel::addUser(IrcStub *ircUser)
{
	assert(ircUser);

	auto it = _users.insert(ircUser);
	if (!it.second) // it.second is true if inserted
		return false;

	ircUser->sendJoinedChannel(_channelName, _users);

	for (auto ircStub : _users) {
		ircStub->sendUserJoinedChannel(_channelName, ircUser);
	}

	return it.second;
}

bool IrcChannel::removeUser(IrcStub *ircUser)
{
	assert(ircUser);

	size_t count = _users.erase(ircUser);
	if (count == 0)
		return false;

	ircUser->sendLeftChannel(_channelName, "");

	for (auto ircStub : _users) {
		ircStub->sendUserLeftChannel(_channelName, ircUser);
	}

	return true;
}

void IrcChannel::sendMessage(const std::string& message, IrcStub *sender)
{
	for (auto ircStub : _users)
	{
		if (ircStub != sender)
			ircStub->sendMessage(_channelName, message, sender->getNickName());
	}
}
