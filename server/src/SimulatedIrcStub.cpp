#include "SimulatedIrcStub.h"
#include "ListServer.h"
#include "ServerPlayer.h"

SimulatedIrcStub::SimulatedIrcStub(IrcServer *ircServer, ServerConnection *connection, ServerPlayer *player)
	: IrcStub(ircServer), _connection(connection), _player(player)
{
}

SimulatedIrcStub::~SimulatedIrcStub()
{
	_ircServer->removePlayer(this);
}

void SimulatedIrcStub::disconnect()
{
	// TODO(joey): unused
}

void SimulatedIrcStub::sendJoinedChannel(const std::string &channel, const std::unordered_set<IrcStub *> &users)
{
	CString sendMsg = "GraalEngine,irc,join,";
	sendMsg << CString(channel).gtokenize();
	_connection->sendTextForPlayer(_player, sendMsg);
}

void SimulatedIrcStub::sendLeftChannel(const std::string &channel, const std::string &message)
{
	CString sendMsg = "GraalEngine,irc,part,";
	sendMsg << CString(channel).gtokenize();
	_connection->sendTextForPlayer(_player, sendMsg);
}

void SimulatedIrcStub::sendMessage(const std::string& channel, const std::string& message, const std::string& sender)
{
	CString serverPacket;
	serverPacket >> (char)SVO_SENDTEXT << "GraalEngine,irc,privmsg," << sender
				 << "," << CString(channel).gtokenize() << "," << CString(message).gtokenize();
	_connection->sendPacket(serverPacket);
}

void SimulatedIrcStub::sendUserJoinedChannel(const std::string &channel, IrcStub *from)
{

}

void SimulatedIrcStub::sendUserLeftChannel(const std::string &channel, IrcStub *from)
{

}
