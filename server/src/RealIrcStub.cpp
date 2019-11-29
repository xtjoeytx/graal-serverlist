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
	// Password enabled channels require the proper key
	// sendPacket(":" + _listServerAddress + " 475 " + params[1] + " " + params[1] + " :Cannot join channel (+k) - bad key");

	CString usersString;
	std::vector<CString> usersStrList;
	for (auto user : users)
	{
		size_t len = user->getNickName().size();
		if (usersString.length() + len > 400)
		{
			usersStrList.push_back(usersString);
			usersString.clear(400);
		}

		usersString << user->getNickName() << " ";
	}
	usersStrList.push_back(usersString);

	// Send join
	_connection->sendPacket(":" + getNickName() + " JOIN " + channel);

	// Send topic
	_connection->sendPacket(":" + _ircServer->getHostName() + " 332 " + getNickName() + " " + channel + " :Welcome to " + channel + ", " + getNickName() + "!");
	_connection->sendPacket(":" + _ircServer->getHostName() + " 333 " + getNickName() + " " + channel + " " + _ircServer->getHostName() + " 1560487838"); // last two params is user who set the topic, and unixtime when the topic was set

	// Send users, if list of nicks is too long, repeat 353
	for (const auto& usersStr : usersStrList)
		_connection->sendPacket(":" + _ircServer->getHostName() + " 353 " + getNickName() + " = " + channel + " :" + usersStr.trim());
	_connection->sendPacket(":" + _ircServer->getHostName() + " 366 " + getNickName() + " " + channel + " :End of /NAMES list.");
}

void RealIrcStub::sendLeftChannel(const std::string &channel, const std::string &message)
{
	_connection->sendPacket(":" + getNickName() + " PART " + channel + " :" + message);
}
