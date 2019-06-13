#include <stdlib.h>
#include <time.h>
#include "ListServer.h"
#include "IrcConnection.h"
#include "ServerConnection.h"
#include "ServerPlayer.h"
#include "CLog.h"

/*
	Pointer-Functions for Packets
*/
typedef bool (IrcConnection::*IrcSocketFunction)(CString&);

IrcSocketFunction ircFunctionTable[IRCI_PACKETCOUNT];

void createIrcPtrTable()
{
	// kinda like a memset-ish thing y'know
	for (int packetId = 0; packetId < IRCI_PACKETCOUNT; packetId++)
		ircFunctionTable[packetId] = &IrcConnection::msgIRCI_NULL;

	// now set non-nulls
	ircFunctionTable[IRCI_SENDTEXT] = &IrcConnection::msgIRCI_SENDTEXT;
}

/*
	Constructor - Deconstructor
*/
IrcConnection::IrcConnection(ListServer *listServer, CSocket *pSocket)
: _listServer(listServer), _socket(pSocket), addedToSQL(false), isServerHQ(false),
serverhq_level(1), _fileQueue(pSocket), new_protocol(false), nextIsRaw(false), rawPacketSize(0)
{
	static bool _setupServerPackets = false;
	if (!_setupServerPackets)
	{
		createIrcPtrTable();
		_setupServerPackets = true;
	}
	_listServerAddress = _listServer->getSettings().getStr("listServerAddress");
	_ircPlayer = new ServerPlayer(this);
	_accountStatus = AccountStatus::NotFound;
	_fileQueue.setCodec(ENCRYPT_GEN_1, 0);
	language = "English";
	lastPing = lastPlayerCount = lastData = lastUptimeCheck = time(0);
}

IrcConnection::~IrcConnection()
{
	// Clear Playerlist
	clearPlayerList();


	// delete socket
	delete _socket;
}

/*
	Loops
*/
bool IrcConnection::doMain()
{
	// sock exist?
	if (_socket == NULL)
		return false;

	// Grab the data from the socket and put it into our receive buffer.
	unsigned int size = 0;
	char* data = _socket->getData(&size);

	if (size != 0)
		sockBuffer.write(data, size);
	else if (_socket->getState() == SOCKET_STATE_DISCONNECTED)
		return false;
	
	if (!sockBuffer.isEmpty())
	{
		// definitions
		CString unBuffer;

		if (new_protocol)
		{
			sockBuffer.setRead(0);
			while (sockBuffer.length() >= 2)
			{
				// packet length
				unsigned short len = (unsigned short)sockBuffer.readShort();
				if ((unsigned int)len > (unsigned int)sockBuffer.length() - 2)
					break;

				unBuffer = sockBuffer.readChars(len);
				sockBuffer.removeI(0, len + 2);
				unBuffer.zuncompressI();

				// well theres your buffer
				if (!parsePacket(unBuffer))
					return false;
			}
		}
		else
		{
			CString line;
			int lineEnd;

			// parse data
			do
			{
				if ((lineEnd = sockBuffer.find('\n')) == -1)
					break;

				line = sockBuffer.subString(0, lineEnd + 1);
				sockBuffer.removeI(0, line.length());
				
				if (!parsePacket(line))
					return true;
			} while (sockBuffer.bytesLeft() && !new_protocol);
		}
	}

	// Send a ping every 30 seconds.
	if ( (int)difftime( time(0), lastPing ) >= 30 )
	{
		lastPing = time(0);
		sendPacket( "PING :" + _listServerAddress );
	}

	// send out buffer
	sendCompress();
	return true;
}

/*
	Kill Client
*/
void IrcConnection::kill()
{
	// Send Out-Buffer
	sendCompress();
	delete this;
}

const CString IrcConnection::getPlayers()
{
	const int ANY_CLIENT = (int)(1 << 0) | (int)(1 << 4) | (int)(1 << 5);

	// Update our player list.
	CString playerlist;
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		ServerPlayer *player = *it;
		if ((player->getClientType() & ANY_CLIENT) != 0)
			playerlist << CString(CString() << player->getAccountName() << "\n" << player->getNickName() << "\n").gtokenizeI() << "\n";
	}

	playerlist.gtokenizeI();
	return playerlist;
}

/*
	Get-Value Functions
*/
const CString& IrcConnection::getDescription()
{
	return description;
}

const CString IrcConnection::getIp(const CString& pIp)
{
	if (pIp == ip)
	{
		if (localip.length() != 0) return localip;
		return "127.0.0.1";
	}
	return ip;
}

const CString& IrcConnection::getLanguage()
{
	return language;
}

const CString& IrcConnection::getName()
{
	return name;
}

const int IrcConnection::getPCount()
{
	return (int)playerList.size();
}

const CString& IrcConnection::getPort()
{
	return port;
}

const CString IrcConnection::getType(int PLVER)
{
	CString ret;
	ret.clear();

	return ret;
}

bool IrcConnection::sendMessage(const std::string& channel, const std::string& from, const std::string& message)
{
    sendPacket(":" + from + " PRIVMSG " + channel + " :" + message);
}

const CString IrcConnection::getServerPacket(int PLVER, const CString& pIp)
{
	CString testIp = getIp(pIp);
	CString pcount((int)playerList.size());
	return CString() >> (char)8 >> (char)(getType(PLVER).length() + getName().length()) << getType(PLVER) << getName() >> (char)getLanguage().length() << getLanguage() >> (char)getDescription().length() << getDescription() >> (char)getUrl().length() << getUrl() >> (char)getVersion().length() << getVersion() >> (char)pcount.length() << pcount >> (char)testIp.length() << testIp >> (char)getPort().length() << getPort();
}

ServerPlayer * IrcConnection::getPlayer(unsigned short id) const
{
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		ServerPlayer *player = *it;
		if (player->getId() == id)
			return player;
	}

	return nullptr;
}

ServerPlayer * IrcConnection::getPlayer(const std::string & account, int type) const
{
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		ServerPlayer *player = *it;
		if (player->getClientType() == type && player->getAccountName() == account)
			return player;
	}

	return nullptr;
}

void IrcConnection::clearPlayerList()
{
	// clean playerlist
	for (auto it = playerList.begin(); it != playerList.end(); ++it)
		delete *it;
	playerList.clear();
}

/*
	Send-Packet Functions
*/
void IrcConnection::sendCompress()
{
	if (new_protocol)
	{
		_fileQueue.sendCompress();
		return;
	}

	// empty buffer?
	if (sendBuffer.isEmpty())
	{
		// If we still have some data in the out buffer, try sending it again.
		if (outBuffer.isEmpty() == false)
		{
			unsigned int dsize = outBuffer.length();
			outBuffer.removeI(0, _socket->sendData(outBuffer.text(), &dsize));
		}
		return;
	}
	
	// add the send buffer to the out buffer
	outBuffer << sendBuffer;

	// send buffer
	unsigned int dsize = outBuffer.length();
	outBuffer.removeI(0, _socket->sendData(outBuffer.text(), &dsize));

	// clear buffer
	sendBuffer.clear();
}

void IrcConnection::sendPacket(CString pPacket, bool pSendNow)
{
	// empty buffer?
	if (pPacket.isEmpty())
		return;

	// append '\n'
	if (pPacket[pPacket.length()-1] != '\n')
		pPacket.writeChar('\n');

	// append buffer depending on protocol
	if (new_protocol)
		_fileQueue.addPacket(pPacket);
	else
		sendBuffer.write(pPacket);

	printf("Irc Packet Out: %s (%d)\n", pPacket.trim().text(), pPacket.length());
	// send buffer now?
	if (pSendNow)
		sendCompress();
}

/*
	Packet-Functions
*/
bool IrcConnection::parsePacket(CString& pPacket)
{
	while (pPacket.bytesLeft() > 0)
	{
		CString curPacket;
		if (nextIsRaw)
		{
			nextIsRaw = false;
			curPacket = pPacket.readChars(rawPacketSize);
		}
		else curPacket = pPacket.readString("\n");

		// read id & packet
		//auto id = curPacket.readString(" ").text();

		printf("Irc Packet In: %s (%d)\n", curPacket.trim().text(), curPacket.trim().length());

		// valid packet, call function
		bool ret = (*this.*ircFunctionTable[IRCI_SENDTEXT])(curPacket);
		if (!ret) {
			//		serverlog.out("Packet %u failed for server %s.\n", (unsigned int)id, name.text());
		}

		// Update the data timeout.
		lastData = time(0);
	}

	return true;
}

bool IrcConnection::msgIRCI_SENDTEXT(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params[0].toLower() == "ping")
	{
		sendPacket("PONG " + params[1]);
	}

	if (params.size() >= 0 && _accountStatus != AccountStatus::Normal)
	{
		if (params[0].toLower() == "nick")
		{
            _ircPlayer->setNickName(params[1].text());
		}
		else if (params[0].toLower() == "pass")
		{
			password = params[1];
		}
		else if (params[0].toLower() == "user")
		{
			_ircPlayer->setProps(CString() >> (char)PLPROP_ACCOUNTNAME >> (char)params[1].length() << params[1]);

			sendPacket(":" + _ircPlayer->getNickName() + " NICK " + _ircPlayer->getAccountName());
            _ircPlayer->setNickName(_ircPlayer->getAccountName());
		}

		if (_ircPlayer->getAccountName() != "" && password != "")
        {
            _accountStatus = _listServer->verifyAccount(_ircPlayer->getAccountName(), password.text());
            switch (_accountStatus)
            {
                case AccountStatus::Normal:
                    sendPacket(":" + _listServerAddress + " 001 " + _ircPlayer->getAccountName() + " :Welcome to " + _listServer->getSettings().getStr("name") + ", " + _ircPlayer->getAccountName() + "!");
                    sendPacket(":" + _listServerAddress + " 001 " + _ircPlayer->getAccountName() + " :Your account: " + _ircPlayer->getAccountName() + ", password: " + password);
                    break;
                default:
                    sendPacket(":" + _listServerAddress + " KILL " + _ircPlayer->getAccountName() + " Unable to identify account: " + (int)_accountStatus);
                    _socket->disconnect();
                    break;
            }
        }
	}
	else if (params.size() >= 0 && _accountStatus == AccountStatus::Normal)
	{
		if (params[0].toLower() == "join")
		{
			sendPacket(":" + _ircPlayer->getAccountName() + " JOIN " + params[1]);
			_listServer->addPlayerToChannel(params[1].text(), _ircPlayer);
		}
		else if (params[0].toLower() == "part")
        {
            _listServer->removePlayerFromChannel(params[1].text(), _ircPlayer);
        }
		else if (params[0].toLower() == "privmsg")
		{
			CString message = pPacket.subString(pPacket.readString(":").length()+1);
			_listServer->sendMessage(params[1].text(), _ircPlayer->getAccountName(), message.text());
		}
	}

	return true;
}

bool IrcConnection::msgIRCI_NULL(CString& pPacket)
{
	pPacket.setRead(0);
	unsigned char id = pPacket.readGUChar();
	printf("Unknown Server Packet: %u (%s)\n", (unsigned int)id, pPacket.text() + 1);
	return true;
}
