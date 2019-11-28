#include "ServerPlayer.h"

ServerPlayer::ServerPlayer(ServerConnection *serverConnection, IrcServer *ircServer)
	: _id(0), _clientType(0), _x(0.0), _y(0.0), _alignment(0),
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
				ircStub.setNickName(pPacket.readChars(pPacket.readGUChar()).text());
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

			case PLPROP_ACCOUNTNAME: // PLPROP_ACCOUNTNAME
				_accountName = pPacket.readChars(pPacket.readGUChar()).text();
				break;

			default:
				printf("Invalid property: %d\n", propId);
				return;
		}
	}
}