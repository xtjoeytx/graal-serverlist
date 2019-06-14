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
		const CString& getDescription();
		const CString getIp(const CString& pIp = "");
		const CString& getLanguage();
		const CString& getName();
		const CString getPlayers();
		const int getPCount();
		const CString& getPort();
		const CString getType(int PLVER);
		int getTypeVal() { return serverhq_level; }
		const CString& getUrl() { return url; }
		const CString& getVersion() { return version; }
		const CString getServerPacket(int PLVER, const CString& pIp = "");
		int getLastData()	{ return (int)difftime( time(0), lastData ); }
		CSocket* getSock()	{ return _socket; }

		ServerPlayer * getPlayer(unsigned short id) const;
		ServerPlayer * getPlayer(const std::string& account, int type) const;
		void clearPlayerList();
		bool sendMessage(const std::string& channel, ServerPlayer *from, const std::string& message);

    // send-packet functions
		void sendCompress();
		void sendPacket(CString pPacket, bool pSendNow = false);

		// packet-functions;
		bool parsePacket(CString& pPacket);
		bool msgIRCI_NULL(CString& pPacket);
		bool msgIRCI_SENDTEXT(CString& pPacket);

	private:
		ListServer *_listServer;
		CSocket *_socket;
		ServerPlayer *_ircPlayer;
		
		// Packet protocol
		bool nextIsRaw;
		int rawPacketSize;
		bool new_protocol;
		CFileQueue _fileQueue;
		CString sendBuffer, sockBuffer, outBuffer;
		AccountStatus _accountStatus;

		CString description, ip, language, name, port, url, version, localip, _listServerAddress;
		std::vector<ServerPlayer *> playerList;
		time_t lastPing, lastData, lastPlayerCount, lastUptimeCheck;
		bool addedToSQL;
		bool isServerHQ;
		CString password, hostname;
		unsigned char serverhq_level;
};

#endif // TIRC_H
