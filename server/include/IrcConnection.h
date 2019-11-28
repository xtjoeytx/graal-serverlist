#ifndef TIRC_H
#define TIRC_H

#include <time.h>
#include "CSocket.h"
#include "CString.h"
#include "CEncryption.h"
#include "IDataBackend.h" // for AccountStatus
#include "RealIrcStub.h"

class IrcServer;
class ServerConnection;

class IrcConnection
{
	public:
		// constructor-destructor
		IrcConnection(IrcServer *ircServer, CSocket *pSocket);
		~IrcConnection();

		// main loop
		bool doMain();

		// kill client
		void kill();

		// get-value functions
		int getLastData()	{ return (int)difftime( time(0), lastData ); }
		CSocket* getSock()	{ return _socket; }

		// send-packet functions
		void sendCompress();
		void sendPacket(CString pPacket, bool pSendNow = false);

		// packet-functions;
		bool parsePacket(CString& pPacket);
		bool msgIRC_UNKNOWN(CString& pPacket);
		bool msgIRC_USER(CString& pPacket);
		bool msgIRC_PING(CString& pPacket);
		bool msgIRC_NICK(CString& pPacket);
		bool msgIRC_PASS(CString& pPacket);
		bool msgIRC_JOIN(CString& pPacket);
		bool msgIRC_PART(CString& pPacket);
		bool msgIRC_PRIVMSG(CString& pPacket);

		void authenticateUser();

	private:
		IrcServer *_ircServer;
		CSocket *_socket;
		RealIrcStub _ircStub;
		
		// Packet protocol
		CString sendBuffer, sockBuffer, outBuffer;
		AccountStatus _accountStatus;

		CString password, hostname, _listServerAddress;
		time_t lastPing, lastData;

		std::string accountName, nickName;
};

#endif // TIRC_H
