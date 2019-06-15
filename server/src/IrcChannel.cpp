#include "IrcChannel.h"
#include "IrcConnection.h"
#include "ServerConnection.h"

// TODO(joey): A proper event handling class!

void IrcChannel::addUser(ServerPlayer *player)
{
	_users.insert(player);

	if (!_ircSubscribers.empty())
	{
		CString ircPacket;
		ircPacket << ":" << player->getAccountName() << " JOIN " << _channelName;
		for (auto it = _ircSubscribers.begin(); it != _ircSubscribers.end(); ++it)
		{
			IrcConnection *conn = *it;
			//if (conn != sender)
			conn->sendPacket(ircPacket);
		}
	}
}

void IrcChannel::removeUser(ServerPlayer *player)
{
	_users.erase(player);

	if (!_ircSubscribers.empty())
	{
		CString ircPacket;
		ircPacket << ":" << player->getAccountName() << " PART " << _channelName;
		for (auto it = _ircSubscribers.begin(); it != _ircSubscribers.end(); ++it)
		{
			IrcConnection *conn = *it;
			//if (conn != sender)
			conn->sendPacket(ircPacket);
		}
	}
}

void IrcChannel::sendMessage(const std::string& from, const std::string& message, void *sender)
{
	if (!_ircSubscribers.empty())
	{
		CString ircPacket;
		ircPacket << ":" << from << " PRIVMSG " << _channelName << " :" << message;
		for (auto it = _ircSubscribers.begin(); it != _ircSubscribers.end(); ++it)
		{
			IrcConnection *conn = *it;
			if (conn != sender)
				conn->sendPacket(ircPacket);
		}
	}

	if (!_serverSubscribers.empty())
	{
		CString serverPacket;
		serverPacket >> (char)SVO_SENDTEXT << "GraalEngine,irc,privmsg," << from
					 << "," << CString(_channelName).gtokenize() << "," << CString(message).gtokenize();

		for (auto it = _serverSubscribers.begin(); it != _serverSubscribers.end(); ++it)
		{
			ServerConnection *conn = it->first;
			if (conn != sender)
				conn->sendPacket(serverPacket);
		}
	}
}

void IrcChannel::subscribe(ServerConnection *connection)
{
	auto it = _serverSubscribers.find(connection);
	if (it != _serverSubscribers.end())
	{
		it->second++;
		return;
	}

	_serverSubscribers[connection] = 1;
}

void IrcChannel::unsubscribe(ServerConnection *connection)
{
	auto it = _serverSubscribers.find(connection);
	if (it != _serverSubscribers.end())
	{
		it->second--;
		if (it->second == 0)
			_serverSubscribers.erase(it);
	}
}