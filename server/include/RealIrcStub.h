#ifndef REALIRCSTUB_H
#define REALIRCSTUB_H

#pragma once

#include "IrcStub.h"

class IrcConnection;

class RealIrcStub : public IrcStub
{
public:
	RealIrcStub(IrcServer *ircServer, IrcConnection *connection) : IrcStub(ircServer), _connection(connection) {}

	void disconnect() override;

	void sendJoinedChannel(const std::string& channel, const std::unordered_set<IrcUserData, IrcUserDataHash, IrcUserDataHash> &users) override;
	void sendLeftChannel(const std::string& channel, const std::string& message) override;
	void sendMessage(const std::string& channel, const std::string& message, const std::string& sender) override;
	void sendUserJoinedChannel(const std::string& channel, IrcStub *from) override;
	void sendUserLeftChannel(const std::string& channel, IrcStub *from) override;

private:
	IrcConnection *_connection;

};

#endif //REALIRCSTUB_H
