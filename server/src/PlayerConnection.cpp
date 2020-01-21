#include <time.h>
#include "ListServer.h"
#include "PlayerConnection.h"
#include "ServerConnection.h"
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
	Constructor
*/
PlayerConnection::PlayerConnection(ListServer *listServer, CSocket *pSocket)
	: _listServer(listServer), sock(pSocket), _fileQueue(pSocket)
{
	static bool _setupPlayerPackets = false;
	if (!_setupPlayerPackets)
	{
		createPlayerPtrTable();
		_setupPlayerPackets = true;
	}

	_fileQueue.setCodec(ENCRYPT_GEN_2, 0);
	_inCodec.setGen(ENCRYPT_GEN_2);
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
	if (sock == NULL)
		return false;

	// Grab the data from the socket and put it into our receive buffer.
	unsigned int size = 0;
	char* data = sock->getData(&size);
	if (size != 0)
		sockBuffer.write(data, size);
	else if (sock->getState() == SOCKET_STATE_DISCONNECTED)
		return false;

	if (!sockBuffer.isEmpty())
	{
		// definitions
		CString unBuffer;

		// parse data
		sockBuffer.setRead(0);
		while (sockBuffer.length() >= 2)
		{
			// packet length
			unsigned short len = (unsigned short)sockBuffer.readShort();
			if ((unsigned int)len > (unsigned int)sockBuffer.length() - 2)
				break;

			unBuffer = sockBuffer.readChars(len);
			sockBuffer.removeI(0, len + 2);

			switch (_inCodec.getGen())
			{
				case ENCRYPT_GEN_1:		// Gen 1 is not encrypted or compressed.
					break;

					// Gen 2 and 3 are zlib compressed.  Gen 3 encrypts individual packets
					// Uncompress so we can properly decrypt later on.
				case ENCRYPT_GEN_2:
				case ENCRYPT_GEN_3:
					unBuffer.zuncompressI();
					break;

					// Gen 4 and up encrypt the whole combined and compressed packet.
					// Decrypt and decompress.
				default:
					decryptPacket(unBuffer);
					break;
			}

			// well theres your buffer
			if (!parsePacket(unBuffer))
				return false;
		}
	}

	// send out buffer
	_fileQueue.sendCompress();
	return true;
}

/*
	Packet-Functions
*/
void PlayerConnection::decryptPacket(CString& pPacket)
{
	// Version 1.41 - 2.18 encryption
	// Was already decompressed so just decrypt the packet.
	if (_inCodec.getGen() == ENCRYPT_GEN_3)
		_inCodec.decrypt(pPacket);

	// Version 2.19+ encryption.
	// Encryption happens before compression and depends on the compression used so
	// first decrypt and then decompress.
	if (_inCodec.getGen() == ENCRYPT_GEN_4)
	{
		// Decrypt the packet.
		_inCodec.limitFromType(COMPRESS_BZ2);
		_inCodec.decrypt(pPacket);

		// Uncompress packet.
		pPacket.bzuncompressI();
	}
	else if (_inCodec.getGen() >= ENCRYPT_GEN_5)
	{
		// Find the compression type and remove it.
		int pType = pPacket.readChar();
		pPacket.removeI(0, 1);

		// Decrypt the packet.
		_inCodec.limitFromType(pType);		// Encryption is partially related to compression.
		_inCodec.decrypt(pPacket);

		// Uncompress packet
		if (pType == COMPRESS_ZLIB)
			pPacket.zuncompressI();
		else if (pType == COMPRESS_BZ2)
			pPacket.bzuncompressI();
		else if (pType != COMPRESS_UNCOMPRESSED)
			printf("** [ERROR] Client gave incorrect packet compression type! [%d]\n", pType);
	}
}

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

void PlayerConnection::sendPacket(CString pPacket, bool pSendNow)
{
	// empty buffer?
	if (pPacket.isEmpty())
		return;

	// append '\n'
	if (pPacket[pPacket.length() - 1] != '\n')
		pPacket.writeChar('\n');

	// append buffer
	_fileQueue.addPacket(pPacket);

	// send buffer now?
	if (pSendNow)
		_fileQueue.sendCompress();
}

int PlayerConnection::sendServerList()
{
	auto conn = _listServer->getConnections();

	CString dataBuffer;
	dataBuffer.writeGChar(PLO_SVRLIST);
	
	int serverCount = 0;
	CString serverPacket;
	for (auto it = conn.begin(); it != conn.end(); ++it)
	{
		ServerConnection *serverObject = *it;
		if (!serverObject->getName().isEmpty())
		{
			serverPacket << (*it)->getServerPacket(1, sock->getRemoteIp());
			serverCount++;
		}
	}
	
	dataBuffer.writeGChar((unsigned char)serverCount);
	dataBuffer.write(serverPacket);
	sendPacket(dataBuffer);

	return serverCount;
}

/*
	Incoming Packets
*/
bool PlayerConnection::msgPLI_NULL(CString& pPacket)
{
	pPacket.setRead(0);
	printf("Unknown Player Packet: %d (%s)\n", pPacket.readGUChar(), pPacket.text()+1 );
	return true;
}

bool PlayerConnection::msgPLI_V1VER(CString& pPacket)
{
	// TODO(joey): not sure what client this is for

	/*
	// definitions
	std::vector<CString>::iterator result;
	std::vector<CString> versions = CString("newmain").tokenize(",");

	// search for version
	if ((result = std::find(versions.begin(), versions.end(), pPacket.text())) == versions.end())
		return;
	*/

	_version = pPacket.readString("").text();
	return true;
}

bool PlayerConnection::msgPLI_SERVERLIST(CString& pPacket)
{
	// read data
	CString account = pPacket.readChars(pPacket.readGUChar());
	CString password = pPacket.readChars(pPacket.readGUChar());

	// Verify account
	AccountStatus status = _listServer->verifyAccount(account.text(), password.text());

	switch (status)
	{
		case AccountStatus::Normal: {
            CSettings& settings = _listServer->getSettings();

            int availableServers = sendServerList();
            sendPacket(CString() >> (char) PLO_STATUS << "Welcome to " << settings.getStr("name") << ", " << account
                                 << ".\r" << "There are " << CString(availableServers) << " server(s) online.");
            sendPacket(CString() >> (char) PLO_SITEURL << settings.getStr("url"));
            sendPacket(CString() >> (char) PLO_UPGURL << settings.getStr("donateUrl"));
            break;
        }

		default:
			sendPacket(CString() >> (char)PLO_ERROR << getAccountError(status));
			return false;
	}

	return true;
}

bool PlayerConnection::msgPLI_V2VER(CString& pPacket)
{
	// TODO(joey): No idea what client actually uses this, but leaving it in for compatibility.
	unsigned char key = pPacket.readGUChar();
	_version = pPacket.readString("").text(); // == newmain

	_fileQueue.setCodec(ENCRYPT_GEN_4, key);
	_inCodec.reset(key);
	_inCodec.setGen(ENCRYPT_GEN_4);
	return true;
}

bool PlayerConnection::msgPLI_V2SERVERLISTRC(CString& pPacket)
{
	unsigned char key = pPacket.readGChar();
	_version = pPacket.readString("").text();

	_fileQueue.setCodec(ENCRYPT_GEN_5, key);
	_inCodec.setGen(ENCRYPT_GEN_5);
	_inCodec.reset(key);
	return true;
}

bool PlayerConnection::msgPLI_V2ENCRYPTKEYCL(CString& pPacket)
{
	unsigned char key = pPacket.readGUChar();
	_version = pPacket.readString("").text();

	_fileQueue.setCodec(ENCRYPT_GEN_5, key);
	_inCodec.setGen(ENCRYPT_GEN_5);
	_inCodec.reset(key);
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
	// TODO(joey): unsure if this still works.

	/*
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
*/

	return true;
}
