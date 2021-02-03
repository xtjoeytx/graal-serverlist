#include "ServerPlayer.h"
#include "IrcChannel.h"
#include "IrcConnection.h"
#include "ServerConnection.h"

void IrcChannel::sendMessage(ServerPlayer *from, const std::string& message)
{
	for (auto it = _users.begin(); it != _users.end(); ++it)
	{
	    ServerPlayer* player = *it;
        if (player->getIrcCommunication() != nullptr && player != from)
            player->getIrcCommunication()->sendMessage(_channelName, from, message);
	}

}
