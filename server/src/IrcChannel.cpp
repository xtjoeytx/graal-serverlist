#include "IrcChannel.h"
#include "IrcConnection.h"
#include "ServerConnection.h"

void IrcChannel::sendMessage(const std::string& from, const std::string& message)
{
	for (auto it = _ircSubscribers.begin(); it != _ircSubscribers.end(); ++it)
	{

	}

	for (auto it = _serverSubscribers.begin(); it != _serverSubscribers.end(); ++it)
	{

	}
}
