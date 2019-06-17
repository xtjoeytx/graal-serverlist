#include <stdlib.h>
#include <time.h>
#include "ListServer.h"
#include "IrcConnection.h"
#include "ServerConnection.h"
#include "ServerPlayer.h"
#include "CLog.h"
#include <map>
#include "IrcChannel.h"

/*
	Pointer-Functions for Packets
*/
typedef bool (IrcConnection::*IrcSocketFunction)(CString&);

std::unordered_map<std::string,IrcSocketFunction> ircFunctionTable;

void createIrcPtrTable()
{
	ircFunctionTable["user"] = &IrcConnection::msgIRC_USER;
	ircFunctionTable["nick"] = &IrcConnection::msgIRC_NICK;
	ircFunctionTable["ping"] = &IrcConnection::msgIRC_PING;
	ircFunctionTable["join"] = &IrcConnection::msgIRC_JOIN;
	ircFunctionTable["part"] = &IrcConnection::msgIRC_PART;
	ircFunctionTable["pass"] = &IrcConnection::msgIRC_PASS;
	ircFunctionTable["privmsg"] = &IrcConnection::msgIRC_PRIVMSG;
	ircFunctionTable["notice"] = &IrcConnection::msgIRC_PRIVMSG;
	ircFunctionTable["pong"] = &IrcConnection::msgIRC_PONG;
	ircFunctionTable["mode"] = &IrcConnection::msgIRC_MODE;
	ircFunctionTable["who"] = &IrcConnection::msgIRC_WHO;
	ircFunctionTable["whois"] = &IrcConnection::msgIRC_WHOIS;
}

/*
	Constructor - Deconstructor
*/
IrcConnection::IrcConnection(ListServer* listServer, CSocket* pSocket)
	: _listServer(listServer), _socket(pSocket)
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
	lastPing = lastPlayerCount = lastData = lastUptimeCheck = time(0);
}

IrcConnection::~IrcConnection()
{
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
		int lineEnd;

		// parse data
		do
		{
			if ((lineEnd = sockBuffer.find('\n')) == -1)
				break;

			CString line = sockBuffer.subString(0, lineEnd + 1);
			sockBuffer.removeI(0, line.length());

			if (!parsePacket(line))
				return true;
		}
		while (sockBuffer.bytesLeft());
	}

	// Send a ping every 30 seconds.
	if (int(difftime(time(0), lastPing)) >= 30)
	{
		lastPing = time(0);
		sendPacket("PING :" + _listServerAddress);
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
	if (pPacket[pPacket.length() - 1] != '\n')
		pPacket.writeChar('\n');

	// append buffer depending on protocol
	sendBuffer.write(pPacket);

	printf("Irc Packet Out: %s (%d)\n", pPacket.trim().text(), pPacket.length());
	// send buffer now?
	if (pSendNow)
		sendCompress();
}

bool IrcConnection::sendMessage(const std::string& channel, ServerPlayer* from, const std::string& message)
{
	sendPacket(":" + from->getAccountName() + " PRIVMSG " + channel + " :" + message);

	return true;
}

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
		lastData = time(0);
	}

	return true;
}

bool IrcConnection::msgIRC_USER(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus != AccountStatus::Normal)
	{
		_ircPlayer->setProps(CString() >> (char)PLPROP_ACCOUNTNAME >> (char)params[1].length() << params[1]);

		sendPacket(":" + _ircPlayer->getNickName() + " NICK " + _ircPlayer->getAccountName());
		_ircPlayer->setNickName(_ircPlayer->getAccountName());

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

bool IrcConnection::msgIRC_PONG(CString& pPacket)
{
	//Todo(Shitai): Calculate latency?

	return true;
}

bool IrcConnection::msgIRC_NICK(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus != AccountStatus::Normal)
	{
		_ircPlayer->setNickName(params[1].text());
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
	if (!_ircPlayer->getAccountName().empty() && password != "")
	{
		_accountStatus = _listServer->verifyAccount(_ircPlayer->getAccountName(), password.text());
		switch (_accountStatus)
		{
		case AccountStatus::Normal:
			sendPacket(":" + _listServerAddress + " 001 " + _ircPlayer->getAccountName() + " :Welcome to " + _listServer->getSettings().getStr("name") + ", " + _ircPlayer->getAccountName() + "!");
			sendPacket(":" + _listServerAddress + " 001 " + _ircPlayer->getAccountName() + " :Your account: " + _ircPlayer->getAccountName() + ", password: " + password);

			break;
		default:
			sendPacket(":" + _listServerAddress + " KILL " + _ircPlayer->getAccountName() + " Unable to identify account: " + int(_accountStatus));
			sendCompress();
			_socket->disconnect();
			break;
		}
	}
}

bool IrcConnection::msgIRC_MODE(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 3 && _accountStatus == AccountStatus::Normal) // Set mode
	{
		if (params[1].subString(0,1) == "#")
		{
			// Set channel modes
		}
		else if (params[1] == _ircPlayer->getAccountName())
		{
			// Perhaps check for valid modes
			sendPacket(":" + _ircPlayer->getAccountName() + " MODE " + _ircPlayer->getAccountName() + " " + params[2]);
		}
	}
	else if (params.size() == 2 && _accountStatus == AccountStatus::Normal) // Get mode
	{
		if (params[1].subString(0, 1) == "#")
		{
			// Get channel modes
			sendPacket(":" + _listServerAddress + " 324 " + _ircPlayer->getAccountName() + " " + params[1] + " +cgnst"); // Channel modes
			sendPacket(":" + _listServerAddress + " 329 " + _ircPlayer->getAccountName() + " " + params[1] + " 1251403546"); // When channel modes was last changed
		}
		else if (params[1] == _ircPlayer->getAccountName())
		{
			sendPacket(":" + _ircPlayer->getAccountName() + " MODE " + _ircPlayer->getAccountName() + " :+i");
		}
	}

	return true;
}

bool IrcConnection::msgIRC_WHO(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	return true;
}


bool IrcConnection::msgIRC_WHOIS(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");
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


bool IrcConnection::msgIRC_JOIN(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus == AccountStatus::Normal)
	{
		// Password enabled channels require the proper key
		// sendPacket(":" + _listServerAddress + " 475 " + params[1] + " " + params[1] + " :Cannot join channel (+k) - bad key");

		sendPacket(":" + _ircPlayer->getAccountName() + " JOIN " + params[1]);
		_listServer->addPlayerToChannel(params[1].text(), _ircPlayer);

		// Todo(Shitai): Move to IrcChannel.cpp?
		auto channel = _listServer->getChannel(params[1].text());
		CString users;
		for (auto * user: *channel->getUsers())
		{
			users << user->getAccountName() << " ";
		}

		// Send topic
		sendPacket(":" + _listServerAddress + " 332 " + _ircPlayer->getAccountName() + " " + params[1] + " :Welcome to " + params[1] + ", " + _ircPlayer->getAccountName() + "!");
		sendPacket(":" + _listServerAddress + " 333 " + _ircPlayer->getAccountName() + " " + params[1] + " " + _listServerAddress + " 1560487838"); // last two params is user who set the topic, and unixtime when the topic was set

		// Send users, if list of nicks is too long, repeat 353
		sendPacket(":" + _listServerAddress + " 353 " + _ircPlayer->getAccountName() + " = " + params[1] + " :" + users.trim());
		sendPacket(":" + _listServerAddress + " 366 " + params[1] + " " + params[1] + " :End of /NAMES list.");
	}

	return true;
}

bool IrcConnection::msgIRC_PART(CString& pPacket)
{
	std::vector<CString> params = pPacket.trim().tokenize(" ");

	if (params.size() >= 0 && _accountStatus == AccountStatus::Normal)
	{
		const CString message = pPacket.subString(pPacket.readString(":").length() + 1);
		sendPacket(":" + _ircPlayer->getAccountName() + " PART " + params[1] + " :" + message);
		_listServer->removePlayerFromChannel(params[1].text(), _ircPlayer);
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
		_listServer->sendMessage(params[1].text(), _ircPlayer, message.text());
	}

	return true;
}

bool IrcConnection::msgIRC_UNKNOWN(CString& pPacket)
{
	pPacket.setRead(0);
	printf("Unknown IRC Packet: %s\n", pPacket.text());
	return true;
}
