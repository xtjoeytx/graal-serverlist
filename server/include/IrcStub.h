#ifndef IRCSTUB_H
#define IRCSTUB_H

#pragma once

#include <string>
#include <unordered_set>
#include <vector>

class IrcServer;
class IrcChannel;

class IrcStub
{
	public:
		// may be removed
		virtual void disconnect() = 0;

		virtual void sendJoinedChannel(const std::string& channel, const std::unordered_set<IrcStub *> &users) = 0;
		virtual void sendLeftChannel(const std::string& channel, const std::string& message) = 0;
		virtual void sendMessage(const std::string& channel, const std::string& message, const std::string& sender) = 0;
		virtual void sendUserJoinedChannel(const std::string& channel, IrcStub *from) = 0;
		virtual void sendUserLeftChannel(const std::string& channel, IrcStub *from) = 0;

		const std::string& getNickName() const {
			return _nickName;
		}

		void setNickName(const std::string& nickName) {
			_nickName = nickName;
		}

	protected:
		IrcStub(IrcServer *ircServer)
			: _ircServer(ircServer) { }

		IrcServer *_ircServer;
		std::string _nickName;
//		std::vector<IrcChannel *> channels;
};

#endif //IRCSTUB_H
