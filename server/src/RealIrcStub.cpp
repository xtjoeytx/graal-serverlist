#include "RealIrcStub.h"
#include "ListServer.h"
#include "IrcConnection.h"

void RealIrcStub::disconnect()
{

}

void RealIrcStub::sendMessage(const std::string& channel, const std::string& message, const std::string& sender)
{
	CString ircPacket;
	ircPacket << ":" << sender << " PRIVMSG " << channel << " :" << message;
	_connection->sendPacket(ircPacket);
}

void RealIrcStub::sendUserJoinedChannel(const std::string &channel, IrcStub *from)
{
	CString ircPacket;
	ircPacket << ":" << from->getNickName() << " JOIN " << channel;
	_connection->sendPacket(ircPacket);
}

void RealIrcStub::sendUserLeftChannel(const std::string &channel, IrcStub *from)
{
	CString ircPacket;
	ircPacket << ":" << from->getNickName() << " PART " << channel;
	_connection->sendPacket(ircPacket);
}

void RealIrcStub::sendJoinedChannel(const std::string &channel, const std::unordered_set<IrcStub *> &users)
{
	CString usersString;
	for (auto user : users) {
		usersString << user->getNickName() << " ";
	}

	_connection->sendPacket(":" + _ircServer->getHostName() + " 353 " + getNickName() + " = " + channel + " :" + usersString.trim());
	_connection->sendPacket(":" + _ircServer->getHostName() + " 366 " + getNickName() + " " + channel + " :End of /NAMES list.");
}

void RealIrcStub::sendLeftChannel(const std::string &channel, const std::string &message)
{
	_connection->sendPacket(":" + getNickName() + " PART " + channel + " :" + message);
}
