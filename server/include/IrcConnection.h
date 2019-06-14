#ifndef TIRC_H
#define TIRC_H

#include <time.h>
#include "CSocket.h"
#include "CString.h"
#include "CEncryption.h"
#include "CFileQueue.h"
#include "IDataBackend.h" // for AccountStatus
#include "IrcStub.h"

enum
{
	IRCI_SENDTEXT		= 0,
	IRCI_PACKETCOUNT,
};

enum
{
	IRCO_SENDTEXT		= 0,
};

class ListServer;
class ServerConnection;
class ServerPlayer;

class IrcConnection : IrcStub
{
	public:
		// constructor-destructor
		IrcConnection(ListServer *listServer, CSocket *pSocket);
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
		bool sendMessage(const std::string& channel, ServerPlayer *from, const std::string& message);

		// packet-functions;
		bool parsePacket(CString& pPacket);
		bool msgIRCI_NULL(CString& pPacket);
		bool msgIRCI_SENDTEXT(CString& pPacket);

	private:
		ListServer *_listServer;
		CSocket *_socket;
		ServerPlayer *_ircPlayer;
		
		// Packet protocol
		CString sendBuffer, sockBuffer, outBuffer;
		AccountStatus _accountStatus;

		CString _listServerAddress;
		std::vector<ServerPlayer *> playerList;
		time_t lastPing, lastData, lastPlayerCount, lastUptimeCheck;

		CString password, hostname;
};

#endif // TIRC_H
