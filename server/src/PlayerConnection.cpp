#include <time.h>
#include "main.h"
#include "PlayerConnection.h"
#include "CLog.h"

/*
	Pointer-Functions for Packets
*/
typedef bool (PlayerConnection::*PlayerSocketFunction)(CString&);

PlayerSocketFunction playerFunctionTable[PLI_PACKETCOUNT];

void createPlayerPtrTable()
{
	// kinda like a memset-ish thing y'know
	for (int packetId = 0; packetId < PLI_PACKETCOUNT; packetId++)
		playerFunctionTable[packetId] = &PlayerConnection::msgPLI_NULL;

	// now set non-nulls
	playerFunctionTable[PLI_V1VER] = &PlayerConnection::msgPLI_V1VER;
	playerFunctionTable[PLI_SERVERLIST] = &PlayerConnection::msgPLI_SERVERLIST;
	playerFunctionTable[PLI_V2VER] = &PlayerConnection::msgPLI_V2VER;
	playerFunctionTable[PLI_V2SERVERLISTRC] = &PlayerConnection::msgPLI_V2SERVERLISTRC;
	playerFunctionTable[PLI_V2ENCRYPTKEYCL] = &PlayerConnection::msgPLI_V2ENCRYPTKEYCL;
	playerFunctionTable[PLI_GRSECURELOGIN] = &PlayerConnection::msgPLI_GRSECURELOGIN;
}

/*
	Constructor - Deconstructor
*/
PlayerConnection::PlayerConnection(ListServer *listServer, CSocket *pSocket, bool pIsOld)
	: _listServer(listServer), sock(pSocket), key(0), version(PLV_PRE22), isOld(pIsOld)
{
	// 1.41 doesn't request a server list.  It assumes the server will just send it out.
//	if (isOld)
//	{
//		if (getServerCount() > 0)
//			sendPacket(CString() >> (char)PLO_SVRLIST << getServerList(version, sock->getRemoteIp()), true);
//	}
}

PlayerConnection::~PlayerConnection()
{
	delete sock;
}

/*
	Loops
*/
bool PlayerConnection::doMain()
{
	// sock exist?
	if (sock == NULL || sock->getState() == SOCKET_STATE_DISCONNECTED)
		return false;

	// definitions
	CString unBuffer;
//
//	// Grab the data from the socket and put it into our receive buffer.
//	unsigned int size = 0;
//	char* data = sock->getData(&size);
//	if (size != 0)
//		sockBuffer.write(data, size);
//	else if (sock->getState() == SOCKET_STATE_DISCONNECTED)
//		return false;
//
//	// parse data
//	sockBuffer.setRead(0);
//	while (sockBuffer.length() >= 2)
//	{
//		// packet length
//		unsigned short len = (unsigned short)sockBuffer.readShort();
//		if ((unsigned int)len > (unsigned int)sockBuffer.length()-2)
//			break;
//
//		// version 2.2+
//		if (version == PLV_POST22)
//		{
//			// Read the compression type.
//			unsigned char compressType = (unsigned char)sockBuffer.readChar();
//
//			// Pull out the entire packet and discard the first three bytes.
//			unBuffer = sockBuffer.subString( 0, len + 2 );
//			unBuffer.removeI( 0, 3 );
//
//			// Decrypt.
//			in_codec.limitfromtype( compressType );
//			in_codec.apply(reinterpret_cast<uint8_t*>(unBuffer.text()), unBuffer.length());
//
//			// Uncompress?
//			if ( compressType == ENCRYPT22_ZLIB )
//				unBuffer.zuncompressI();
//			else if ( compressType == ENCRYPT22_BZ2 )
//				unBuffer.bzuncompressI();
//
//			// Make sure we actually have something.
//			if (unBuffer.length() < 1)
//				return false;
//		}
//			// anything below version 2.2
//		else if (version == PLV_PRE22)
//		{
//			// uncompress
//			unBuffer = sockBuffer.remove(0, 2).zuncompress();
//			if (unBuffer.length() < 1)
//				return false;
//		}
//		else if (version == PLV_22)
//		{
//			unBuffer = sockBuffer.remove(0, 2);
//
//			// Decrypt.
//			in_codec.limitfromtype(ENCRYPT22_BZ2);
//			in_codec.apply(reinterpret_cast<uint8_t*>(unBuffer.text()), unBuffer.length());
//
//			unBuffer.bzuncompressI();
//			if (unBuffer.length() == 0)
//				return false;
//		}
//
//		// remove read-data
//		sockBuffer.removeI(0, len+2);
//
//		// well theres your buffer
//		if (!parsePacket(unBuffer))
//			return false;
//	}

	// send out buffer
	sendCompress();
	return true;
}

/*
	Send-Packet Functions
*/
void PlayerConnection::sendCompress()
{
//	// empty buffer?
//	if (sendBuffer.isEmpty())
//	{
//		// If we still have some data in the out buffer, try sending it again.
//		if (outBuffer.isEmpty() == false)
//		{
//			unsigned int dsize = outBuffer.length();
//			outBuffer.removeI(0, sock->sendData(outBuffer.text(), &dsize));
//		}
//		return;
//	}
//
//	// compress buffer
//	if (version == PLV_POST22)
//	{
//		// Choose which compression to use and apply it.
//		int compressionType = ENCRYPT22_UNCOMPRESSED;
//		if (sendBuffer.length() > 0x2000)	// 8KB
//		{
//			compressionType = ENCRYPT22_BZ2;
//			sendBuffer.bzcompressI();
//		}
//		else if (sendBuffer.length() > 40)
//		{
//			compressionType = ENCRYPT22_ZLIB;
//			sendBuffer.zcompressI();
//		}
//
//		// Encrypt the packet and add it to the out buffer.
//		out_codec.limitfromtype(compressionType);
//		out_codec.apply(reinterpret_cast<uint8_t*>(sendBuffer.text()), sendBuffer.length());
//		outBuffer << (short)(sendBuffer.length() + 1) << (char)compressionType << sendBuffer;
//
//		// Send outBuffer.
//		unsigned int dsize = outBuffer.length();
//		outBuffer.removeI(0, sock->sendData(outBuffer.text(), &dsize));
//	}
//	else if (version == PLV_PRE22)
//	{
//		// Compress the packet and add it to the out buffer.
//		sendBuffer.zcompressI();
//		outBuffer << (short)sendBuffer.length() << sendBuffer;
//
//		// Send outBuffer.
//		unsigned int dsize = outBuffer.length();
//		outBuffer.removeI(0, sock->sendData(outBuffer.text(), &dsize));
//	}
//	else if (version == PLV_22)
//	{
//		sendBuffer.bzcompressI();
//
//		// Encrypt the packet and add it to the out buffer.
//		out_codec.limitfromtype(ENCRYPT22_BZ2);
//		out_codec.apply(reinterpret_cast<uint8_t*>(sendBuffer.text()), sendBuffer.length());
//		outBuffer << (short)sendBuffer.length() << sendBuffer;
//
//		// Send outBuffer.
//		unsigned int dsize = outBuffer.length();
//		outBuffer.removeI(0, sock->sendData(outBuffer.text(), &dsize));
//	}
//
//	// Clear the send buffer.
//	sendBuffer.clear();
}

void PlayerConnection::sendPacket(CString pPacket, bool pSendNow)
{
	// empty buffer?
	if (pPacket.isEmpty())
		return;

	// append '\n'
	if (pPacket[pPacket.length()-1] != '\n')
		pPacket.writeChar('\n');

	// append buffer
	sendBuffer.write(pPacket);

	// send buffer now?
	if (pSendNow)
		sendCompress();
}

/*
	Packet-Functions
*/
bool PlayerConnection::parsePacket(CString& pPacket)
{
	while (pPacket.bytesLeft() > 0)
	{
		// read id & packet
		CString str = pPacket.readString("\n");
		int id = str.readGUChar();

		// check lengths
		if (str.length() < 0 || id >= (int)PLI_PACKETCOUNT)
			continue;

		// valid packet, call function
		if (!(*this.*playerFunctionTable[id])(str))
			return false;
	}

	return true;
}

bool PlayerConnection::msgPLI_NULL(CString& pPacket)
{
	pPacket.setRead(0);
	printf("Unknown Player Packet: %d (%s)\n", pPacket.readGUChar(), pPacket.text()+1 );
	return true;
}

bool PlayerConnection::msgPLI_V1VER(CString& pPacket)
{
	/*
	// definitions
	std::vector<CString>::iterator result;
	std::vector<CString> versions = CString("newmain").tokenize(",");

	// search for version
	if ((result = std::find(versions.begin(), versions.end(), pPacket.text())) == versions.end())
		return;
	*/

	pPacket.readString("");
	return true;
}

bool PlayerConnection::msgPLI_SERVERLIST(CString& pPacket)
{
	// definitions
	CString account, password;
	int res;

	// read data
	account  = pPacket.readChars(pPacket.readGUChar());
	password = pPacket.readChars(pPacket.readGUChar());
	printf("Account: %s\n", account.text());
	//printf("Password: %s\n", password.text());

	// verify account
//	res = verifyAccount(account, password);
	printf( "Verify: %d\n", res );

	switch (res)
	{
//		case ACCSTAT_NORMAL:
//			if (getServerCount() > 0)
//				sendPacket(CString() >> (char)PLO_SVRLIST << getServerList(version, sock->getRemoteIp()), true);
//			sendPacket(CString() >> (char)PLO_STATUS << "Welcome to " << settings->getStr("name") << ", " << account << "." << "\r" << "There are " << CString(getServerCount()) << " server(s) online.");
//			sendPacket(CString() >> (char)PLO_SITEURL << settings->getStr("url"));
//			sendPacket(CString() >> (char)PLO_UPGURL << settings->getStr("donateUrl"));
//			return true;

		default:
//			sendPacket(CString() >> (char)PLO_ERROR << getAccountError(res));
			return false;
	}
}

bool PlayerConnection::msgPLI_V2VER(CString& pPacket)
{
//	//version = PLV_POST22;
//	version = PLV_22;
//	key = pPacket.readGChar();
//	in_codec.reset(key);
//	out_codec.reset(key);
	pPacket.readString("");// == newmain
	return true;
}

bool PlayerConnection::msgPLI_V2SERVERLISTRC(CString& pPacket)
{
//	version = PLV_POST22;
//	key = pPacket.readGChar();
//	in_codec.reset(key);
//	out_codec.reset(key);
	return true;
}

bool PlayerConnection::msgPLI_V2ENCRYPTKEYCL(CString& pPacket)
{
//	version = PLV_POST22;
//	key = pPacket.readGChar();
//	//version = pPacket.readChars(8);
//	//pPacket.readString("") == newmain
//	in_codec.reset(key);
//	out_codec.reset(key);
	return true;
}

/*
Incoming format:
	{CHAR PLI_GRSECURELOGIN}{CHAR account length}{account}{CHAR password length}{password}{CHAR login_type}

Outgoing format:
	{CHAR PLO_GRSECURELOGIN}{INT5 transaction}{salt}
*/
bool PlayerConnection::msgPLI_GRSECURELOGIN(CString& pPacket)
{
#ifndef NO_MYSQL
	// Grab the packet values.
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString password = pPacket.readChars(pPacket.readGUChar());
	unsigned char login_type = pPacket.readGUChar();

	// Try a normal login.  If it fails, send back an empty packet.
//	if (verifyAccount(account, password) != ACCSTAT_NORMAL)
//	{
//		sendPacket(CString() >> (char)PLO_GRSECURELOGIN);
//		return true;
//	}

	// Get our transaction id.
	int transaction = abs(rand() & rand());

	// Encode our login type inside the transaction id.
	transaction = (transaction << 8) | login_type;

	// Construct the salt out of ascii values 32 - 125.
	CString salt;
	salt << (char)(((unsigned char)rand() % 0x5E) + 0x20);
	salt << (char)(((unsigned char)rand() % 0x5E) + 0x20);
	salt << (char)(((unsigned char)rand() % 0x5E) + 0x20);

	// Add the transaction to the database.
//	CString query;
//	query << "UPDATE `" << settings->getStr("userlist") << "` SET "
//		  << "transactionnr='" << CString((int)transaction).escape() << "',"
//		  << "salt2='" << salt.escape() << "',"
//		  << "password2=MD5(CONCAT(MD5('" << password.escape() << "'), '" << salt.escape() << "')) "
//		  << "WHERE account='" << account.escape() << "'";
//	mySQL->add_simple_query(query.text());

	// Send the secure login info back to the player.
	sendPacket(CString() >> (char)PLO_GRSECURELOGIN >> (long long)transaction << salt);
#endif

	return true;
}
