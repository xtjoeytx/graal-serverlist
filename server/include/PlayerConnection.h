#ifndef LISTSERVER_PLAYERCONNECTION_H
#define LISTSERVER_PLAYERCONNECTION_H

#include "CSocket.h"
#include "CString.h"
//#include "codec.h"

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
	PLV_PRE22			= 0,
	PLV_POST22			= 1,
	PLV_22				= 2,
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
	PlayerConnection(ListServer *listServer, CSocket *pSocket, bool pIsOld = false);
	~PlayerConnection();

	// main loop
	bool doMain();

	// send packets
	void sendCompress();
	void sendPacket(CString pPacket, bool pSendNow = false);

	// packet-functions;
	bool parsePacket(CString& pPacket);
	bool msgPLI_NULL(CString& pPacket);
	bool msgPLI_V1VER(CString& pPacket);
	bool msgPLI_SERVERLIST(CString& pPacket);
	bool msgPLI_V2VER(CString& pPacket);
	bool msgPLI_V2SERVERLISTRC(CString& pPacket);
	bool msgPLI_V2ENCRYPTKEYCL(CString& pPacket);
	bool msgPLI_GRSECURELOGIN(CString& pPacket);

private:
	ListServer *_listServer;
	CSocket *sock;
	CString sendBuffer, sockBuffer, outBuffer;
//	codec in_codec;
//	codec out_codec;
	int key;
	int version;
	bool isOld;
};

#endif //LISTSERVER_PLAYERCONNECTION_H
