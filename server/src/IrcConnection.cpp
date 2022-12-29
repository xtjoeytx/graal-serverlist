#include <cstdlib>
#include <ctime>
#include "ListServer.h"
#include "IrcConnection.h"
#include "ServerConnection.h"
#include "ServerPlayer.h"

/*
	Pointer-Functions for Packets
*/
typedef bool (IrcConnection::*IrcSocketFunction)(CString&);

std::unordered_map<std::string, IrcSocketFunction> ircFunctionTable;

void createIrcPtrTable()
{
	ircFunctionTable["user"] = &IrcConnection::msgIRC_USER;
	ircFunctionTable["nick"] = &IrcConnection::msgIRC_NICK;
	ircFunctionTable["ping"] = &IrcConnection::msgIRC_PING;
	ircFunctionTable["pong"] = &IrcConnection::msgIRC_PONG;
	ircFunctionTable["join"] = &IrcConnection::msgIRC_JOIN;
	ircFunctionTable["part"] = &IrcConnection::msgIRC_PART;
	ircFunctionTable["pass"] = &IrcConnection::msgIRC_PASS;
	ircFunctionTable["privmsg"] = &IrcConnection::msgIRC_PRIVMSG;
	ircFunctionTable["notice"] = &IrcConnection::msgIRC_PRIVMSG;
	ircFunctionTable["mode"] = &IrcConnection::msgIRC_MODE;
	ircFunctionTable["who"] = &IrcConnection::msgIRC_WHO;
	ircFunctionTable["whois"] = &IrcConnection::msgIRC_WHOIS;
}

/*
	Constructor - Deconstructor
*/
IrcConnection::IrcConnection(IrcServer *ircServer, CSocket *pSocket)
: _ircServer(ircServer), _socket(pSocket), _ircStub(ircServer, this)
{
	static bool _setupServerPackets = false;
	if (!_setupServerPackets)
	{
		createIrcPtrTable();
		_setupServerPackets = true;
	}

	_listServerAddress = _ircServer->getSettings().getStr("listServerAddress");
	_accountStatus = AccountStatus::NotFound;
	lastPing = lastData = time(nullptr);
}

IrcConnection::~IrcConnection()
{
	_ircServer->removePlayer(&_ircStub);

	// delete socket
	delete _socket;
}

/*
	Loops
*/
bool IrcConnection::doMain()
{
	// sock exist?
	if (_socket == nullptr)
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
		} while (sockBuffer.bytesLeft());
	}

	// Send a ping every 30 seconds.
	if ( (int)difftime( time(nullptr), lastPing ) >= 30 )
	{
		lastPing = time(nullptr);
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

/*
	Send-Packet Functions
*/
void IrcConnection::sendCompress()
{
	// empty buffer?
	if (sendBuffer.isEmpty())
	{
		// If we still have some data in the out buffer, try sending it again.
		if (!outBuffer.isEmpty())
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
	sendBuffer.write(pPacket);

	printf("Irc Packet Out: %s (%d)\n", pPacket.trim().text(), pPacket.length());

	// send buffer now?
	if (pSendNow)
		sendCompress();
}

/*
	Packet-Functions
*/
/*
bool IrcConnection::parsePacket(CString& pPacket)
{
	while (pPacket.bytesLeft() > 0)
	{
		CString curPacket = pPacket.readString("\n");

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
			_player.setNickName(params[1].text());
		}
		else if (params[0].toLower() == "pass")
		{
			password = params[1];
		}
		else if (params[0].toLower() == "user")
		{
			_player.setProps(CString() >> (char)PLPROP_ACCOUNTNAME >> (char)params[1].length() << params[1]);

			_accountStatus = _listServer->verifyAccount(_player.getAccountName(), password.text());
			sendPacket(":" + _player.getNickName() + " NICK " + _player.getAccountName());

			switch (_accountStatus)
			{
				case AccountStatus::Normal:
					sendPacket(":" + _listServerAddress + " 001 " + _player.getNickName() + " :Welcome to " + _listServer->getSettings().getStr("name") + ", " + _player.getAccountName() + ".");
					sendPacket(":" + _listServerAddress + " 001 " + _player.getNickName() + " :Your account: " + _player.getAccountName() + ", password: " + password);

					sendPacket(":" + _player.getNickName() + " JOIN #graal");
					_listServer->addPlayerToChannel("#graal", &_player, this);
					break;
				default:
					sendPacket(":" + _listServerAddress + " KILL " + _player.getNickName() + " Unable to identify account: " + (int)AccountStatus::Normal);
					_socket->disconnect();
					break;
			}
		}
	}
	else if (params.size() >= 0 && _accountStatus == AccountStatus::Normal)
	{
		if (params[0].toLower() == "join")
		{
			sendPacket(":" + _player.getNickName() + " JOIN " + params[1]);
			_listServer->addPlayerToChannel(params[1].text(), &_player, this);
		}
		else if (params[0].toLower() == "privmsg")
		{
			CString message = pPacket.subString(pPacket.readString(":").length() + 1);
			//CString forwardPacket;
			//forwardPacket.writeGChar(SVO_SENDTEXT);
			//forwardPacket << "GraalEngine,irc,privmsg," << account.gtokenize() << "," << params[1].gtokenize() << "," << message.gtokenize();
			_listServer->sendTextToChannel(params[1].text(), _player.getAccountName(), message.text(), this);
		}
	}

	return true;
}
*/

/*
	Packet-Functions
*/
bool IrcConnection::parsePacket(CString& pPacket)
{
	while (pPacket.bytesLeft() > 0)
	{
		CString curPacket = pPacket.readString("\n");

		// read id & packet
		std::string packetId = curPacket.readString(" ").toLower().text();
		curPacket.setRead(0);

		// valid packet, call function
		auto it = ircFunctionTable.find(packetId);
		if (it != ircFunctionTable.end()) { (*this.*it->second)(curPacket); }
		else msgIRC_UNKNOWN(curPacket);

		// Update the data timeout.
		lastData = time(nullptr);
	}

	return true;
}

bool IrcConnection::msgIRC_USER(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus != AccountStatus::Normal)
	{
		accountName = params[1].text();

		sendPacket(":" + _ircStub.getNickName() + " NICK " + accountName);
		_ircStub.setNickName(accountName);

		authenticateUser();
	}

	return true;
}

bool IrcConnection::msgIRC_PING(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0)
	{
		sendPacket("PONG " + params[1]);
	}

	return true;
}

bool IrcConnection::msgIRC_NICK(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus != AccountStatus::Normal)
	{
		_ircStub.setNickName(params[1].text());
	}

	return true;
}

bool IrcConnection::msgIRC_PASS(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus != AccountStatus::Normal)
	{
		password = params[1];

		authenticateUser();
	}

	return true;
}

void IrcConnection::authenticateUser()
{
	if (!accountName.empty() && password != "")
	{
		_accountStatus = _ircServer->verifyAccount(accountName, password.text());
		switch (_accountStatus)
		{
		case AccountStatus::Normal:
			sendPacket(":" + _listServerAddress + " 001 " + accountName + " :Welcome to "
				+ _ircServer->getSettings().getStr("name") + ", "
				+ accountName + "!");
			sendPacket(":" + _listServerAddress + " 001 " + accountName + " :Your account: "
				+ accountName + ", password: " + password);
			break;
		default:
			sendPacket(":" + _listServerAddress + " KILL " + accountName
				+ " Unable to identify account: " + (int)_accountStatus, true);
			_socket->disconnect();
			break;
		}
	}
}

bool IrcConnection::msgIRC_JOIN(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus == AccountStatus::Normal)
	{
		_ircServer->addPlayerToChannel(params[1].text(), &_ircStub);
	}

	return true;
}

bool IrcConnection::msgIRC_PART(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus == AccountStatus::Normal)
	{
 		_ircServer->removePlayerFromChannel(params[1].text(), &_ircStub);

		//if (success) {
		//	const CString message = pPacket.subString(pPacket.readString(":").length() + 1);
		//	sendPacket(":" + _ircStub.getNickName() + " PART " + params[1] + " :" + message);
 		//	}
	}

	return true;
}

bool IrcConnection::msgIRC_PRIVMSG(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus == AccountStatus::Normal)
	{
		CString message = pPacket.subString(pPacket.readString(":").length() + 1);

		// Todo(Shitai): Handle when PRIVMSG is sent to player and not a channel. Should send as GraalPM on ServerConnection and as PRIVMSG on IrcConnection
		_ircServer->sendTextToChannel(params[1].text(), message.text(), &_ircStub);
	}

	return true;
}

bool IrcConnection::msgIRC_UNKNOWN(CString& pPacket)
{
	pPacket.setRead(0);
	printf("Unknown IRC Packet: %s\n", pPacket.text());
	return true;
}

bool IrcConnection::msgIRC_PONG(CString &pPacket)
{
	//Todo(Shitai): Calculate latency?

	return true;
}

bool IrcConnection::msgIRC_MODE(CString &pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 3 && _accountStatus == AccountStatus::Normal) // Set mode
	{
		if (params[1].subString(0,1) == "#")
		{
			// Set channel modes
		}
		else if (params[1] == _ircStub.getNickName())
		{
			// Perhaps check for valid modes
			sendPacket(":" + _ircStub.getNickName() + " MODE " + _ircStub.getNickName() + " " + params[2]);
		}
	}
	else if (params.size() == 2 && _accountStatus == AccountStatus::Normal) // Get mode
	{
		if (params[1].subString(0, 1) == "#")
		{
			// Get channel modes
			sendPacket(":" + _listServerAddress + " 324 " + _ircStub.getNickName()  + " " + params[1] + " +cgnst"); // Channel modes
			sendPacket(":" + _listServerAddress + " 329 " + _ircStub.getNickName()  + " " + params[1] + " 1251403546"); // When channel modes was last changed
		}
		else if (params[1] == _ircStub.getNickName())
		{
			sendPacket(":" + _ircStub.getNickName()  + " MODE " + _ircStub.getNickName()  + " :+i");
		}
	}

	return true;
}

bool IrcConnection::msgIRC_WHO(CString &pPacket)
{
	//std::vector<CString> params = pPacket.trim().tokenize(" ");

	return true;
}

bool IrcConnection::msgIRC_WHOIS(CString &pPacket)
{
	//std::vector<CString> params = pPacket.trim().tokenize(" ");
	/*
	 * params[1] == _otherPlayer
	 * All logged in IRC players, no matter if on RC or IrcConnection should be in a list. Initiated on GraalEngine,irc,login,- from RC, and when authenticated on IrcConnection
	 * sendPacket(":" + _listServerAddress + " 311 " + _ircPlayer->getAccountName() + " " + _otherPlayer->getAccountName() + " " + _otherPlayer->getAccountName() + " dummy-host-name * :Real name");
	 * sendPacket(":" + _listServerAddress + " 312 " + _ircPlayer->getAccountName() + " " + _otherPlayer->getAccountName() + " " + _otherPlayer->getServer()->getDashedName() + " :" + _otherPlayer->getServer()->getDescription()); // Server the user is connected to and its description
	 * sendPacket(":" + _listServerAddress + " 671 " + _ircPlayer->getAccountName() + " " + _otherPlayer->getAccountName() + " :is using a secure connection"); // SSL connection
	 * sendPacket(":" + _listServerAddress + " 317 " + _ircPlayer->getAccountName() + " " + _otherPlayer->getAccountName() + " 4442 1560774410 :seconds idle, signon time"); // If user has been idle for a while
	 * sendPacket(":" + _listServerAddress + " 330 " + _ircPlayer->getAccountName() + " " + _otherPlayer->getAccountName() + " " + _otherPlayer->getAccountName() + " :is logged in as"); // In case we start to use nicknames properly, tell which account user is logged in as
	 * sendPacket(":" + _listServerAddress + " 318 " + _ircPlayer->getAccountName() + " " + _otherPlayer->getAccountName() + " :End of /WHOIS list.");
	 */

	return true;
}
