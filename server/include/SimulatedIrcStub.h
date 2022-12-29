#ifndef SIMULATEDIRCSTUB_H
#define SIMULATEDIRCSTUB_H

#pragma once

#include "IrcStub.h"

class ServerConnection;
class ServerPlayer;

class SimulatedIrcStub : public IrcStub
{
public:
	SimulatedIrcStub(IrcServer *ircServer, ServerConnection *connection, ServerPlayer *player);
	~SimulatedIrcStub();

	void disconnect() override;

	void sendJoinedChannel(const std::string& channel, const std::unordered_set<IrcUserData, IrcUserDataHash, IrcUserDataHash> &users) override;
	void sendLeftChannel(const std::string& channel, const std::string& message) override;
	void sendMessage(const std::string& channel, const std::string& message, const std::string& sender) override;
	void sendUserJoinedChannel(const std::string& channel, IrcStub *from) override;
	void sendUserLeftChannel(const std::string& channel, IrcStub *from) override;

private:
	ServerConnection *_connection;
	ServerPlayer *_player;
};

#endif //SIMULATEDIRCSTUB_H
