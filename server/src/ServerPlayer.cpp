#include <cstdint>
#include "ServerPlayer.h"

std::string getIpString(uint32_t ipaddr)
{
	uint8_t a1 = (ipaddr >> 24) & 0xFF;
	uint8_t a2 = (ipaddr >> 16) & 0xFF;
	uint8_t a3 = (ipaddr >> 8) & 0xFF;
	uint8_t a4 = ipaddr & 0xFF;

	char buf[16];
	sprintf(buf, "%d.%d.%d.%d", a1, a2, a3, a4);
	return std::string(buf);
}

ServerPlayer::ServerPlayer(ServerConnection *serverConnection, IrcServer *ircServer)
	: _id(0), _clientType(0), _x(0.0), _y(0.0), _alignment(0), _ipAddress(0),
	  ircStub(ircServer, serverConnection, this)
{

}

ServerPlayer::~ServerPlayer()
{

}

void ServerPlayer::setProps(CString& pPacket)
{
	while (pPacket.bytesLeft() > 0)
	{
		unsigned char propId = pPacket.readGUChar();

		switch (propId)
		{
			case PLPROP_NICKNAME: // PLPROP_NICKNAME
				_nickName = pPacket.readChars(pPacket.readGUChar()).text();
				break;

			case PLPROP_ID: // PLPROP_ID
				_id = pPacket.readGUShort();
				break;

			case PLPROP_X: // PLPROP_X
				_x = (float)pPacket.readGUChar() / 2.0f;
				break;

			case PLPROP_Y: // PLPROP_Y
				_y = (float)pPacket.readGUChar() / 2.0f;
				break;

			case PLPROP_CURLEVEL: // PLPROP_CURLEVEL
				_levelName = pPacket.readChars(pPacket.readGUChar()).text();
				break;

			case PLPROP_ALIGNMENT: // PLPROP_ALIGNMENT
				_alignment = pPacket.readGUChar();
				break;

			case PLPROP_IPADDR: // PLPROP_IPADDR
				_ipAddress = pPacket.readGUInt5();
				_ipAddressStr = getIpString(_ipAddress);
				break;

			case PLPROP_ACCOUNTNAME: // PLPROP_ACCOUNTNAME
				_accountName = pPacket.readChars(pPacket.readGUChar()).text();
				ircStub.setNickName(_accountName);
				break;

			default:
				printf("Invalid property: %d\n", propId);
				return;
		}
	}
}