#ifndef LISTSERVER_PLAYERCONNECTION_H
#define LISTSERVER_PLAYERCONNECTION_H

#include "CSocket.h"
#include "CString.h"
#include "CEncryption.h"
#include "CFileQueue.h"

enum
{
	PLI_V1VER			= 0,
	PLI_SERVERLIST		= 1,
	PLI_V2VER			= 4,
	PLI_V2SERVERLISTRC	= 5,
	PLI_V2ENCRYPTKEYCL	= 7,
	PLI_GRSECURELOGIN	= 223, // GR created function.
	PLI_PACKETCOUNT
};

enum
{
	PLO_SVRLIST			= 0,
	PLO_NULL			= 1,
	PLO_STATUS			= 2,
	PLO_SITEURL			= 3,
	PLO_ERROR			= 4,
	PLO_UPGURL			= 5,
	PLO_GRSECURELOGIN	= 223
};

enum
{
	SECURELOGIN_NOEXPIRE	= 0,
	SECURELOGIN_ONEUSE		= 1,
};

class ListServer;

class PlayerConnection
{
public:
	// constructor-destructor
	PlayerConnection(ListServer *listServer, CSocket *pSocket);
	~PlayerConnection();

	// main loop
	bool doMain();

	// send packet
	void sendPacket(CString pPacket, bool pSendNow = false);
	int sendServerList();

	// packet-functions;
	bool msgPLI_NULL(CString& pPacket);
	bool msgPLI_V1VER(CString& pPacket);
	bool msgPLI_SERVERLIST(CString& pPacket);
	bool msgPLI_V2VER(CString& pPacket);
	bool msgPLI_V2SERVERLISTRC(CString& pPacket);
	bool msgPLI_V2ENCRYPTKEYCL(CString& pPacket);
	bool msgPLI_GRSECURELOGIN(CString& pPacket);

private:
	// Packet functions.
	bool parsePacket(CString& pPacket);
	void decryptPacket(CString& pPacket);

	ListServer *_listServer;
	CSocket *sock;
	CString sendBuffer, sockBuffer, outBuffer;
	CEncryption _inCodec;
	CFileQueue _fileQueue;
	std::string _version;
};

#endif //LISTSERVER_PLAYERCONNECTION_H
