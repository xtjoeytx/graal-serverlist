#include "IrcChannel.h"
#include "IrcConnection.h"
#include "ServerConnection.h"

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
