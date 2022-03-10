#ifndef SERVERCONNECT_H
#define SERVERCONNECT_H

#pragma once

#include <ctime>
#include <memory>
#include "CSocket.h"
#include "CString.h"
#include "CEncryption.h"
#include "CFileQueue.h"
#include "ServerHQ.h"
#include "ClientType.h"

enum
{
	SVI_SETNAME			= 0,
	SVI_SETDESC			= 1,
	SVI_SETLANG			= 2,

	SVI_SETVERS			= 3,
	SVI_SETURL			= 4,
	SVI_SETIP			= 5,
	SVI_SETPORT			= 6,
	SVI_SETPLYR			= 7,
	SVI_VERIACC			= 8,	// deprecated
	SVI_VERIGLD			= 9,
	SVI_GETFILE			= 10,	// deprecated
	SVI_NICKNAME		= 11,
	SVI_GETPROF			= 12,
	SVI_SETPROF			= 13,
	SVI_PLYRADD			= 14,	// Add player to the servers playerlist
	SVI_PLYRREM			= 15,	// Remove player from the servers playerlist
	SVI_SVRPING			= 16,
	SVI_VERIACC2		= 17,
	SVI_SETLOCALIP		= 18,
	SVI_GETFILE2		= 19,
	SVI_UPDATEFILE		= 20,
	SVI_GETFILE3		= 21,
	SVI_NEWSERVER		= 22,
	SVI_SERVERHQPASS	= 23,
	SVI_SERVERHQLEVEL	= 24,
	SVI_SERVERINFO		= 25,
	SVI_REQUESTLIST		= 26,
	SVI_REQUESTSVRINFO	= 27,
	SVI_REQUESTBUDDIES  = 28,
	SVI_PMPLAYER		= 29,
	SVI_REGISTERV3		= 30,
	SVI_SENDTEXT		= 31,
	SVI_PACKETCOUNT,
};

enum
{
	SVO_VERIACC			= 0,	// deprecated
	SVO_VERIGLD			= 1,
	SVO_FILESTART		= 2,	// deprecated
	SVO_FILEEND			= 3,	// deprecated
	SVO_FILEDATA		= 4,	// deprecated
	SVO_VERSIONOLD		= 5,	// not implemented
	SVO_VERSIONCURRENT	= 6,	// not implemented
	SVO_PROFILE			= 7,
	SVO_ERRMSG			= 8,
	SVO_NULL4			= 9,
	SVO_NULL5			= 10,
	SVO_VERIACC2		= 11,
	SVO_FILESTART2		= 12,	// deprecated
	SVO_FILEDATA2		= 13,	// deprecated
	SVO_FILEEND2		= 14,	// deprecated
	SVO_FILESTART3		= 15,
	SVO_FILEDATA3		= 16,
	SVO_FILEEND3		= 17,
	SVO_SERVERINFO		= 18,
	SVO_REQUESTTEXT		= 19,
	SVO_SENDTEXT		= 20,
	SVO_PMPLAYER		= 29,
	SVO_NEWSERVER		= 30,
	SVO_DELSERVER		= 31,
	SVO_SERVERADDPLYR	= 32,
	SVO_SERVERDELPLYR	= 33,
	SVO_PING			= 99,
	SVO_RAWDATA			= 100,
};

class ListServer;
class ServerPlayer;

class ServerConnection
{
	enum class ProtocolVersion {
		Version1,
		Version2,
		Unknown
	};

	public:
		// constructor-destructor
		ServerConnection(ListServer *listServer, CSocket *pSocket);
		~ServerConnection();

		// main loop
		bool doMain(const time_t& now);
		
		bool canAcceptClient(ClientType clientType);
		void disconnectServer(const std::string& error);
		void enableServerHQ(const ServerHQ& server);

		// get-value functions
		CString getIp(const CString& pIp = "") const;
		CString getType(ClientType clientType) const;
		CString getPlayers() const;
		CString getServerPacket(ClientType clientType, const CString& clientIp = "") const;

		bool isAuthenticated() const			{ return _isAuthenticated; }
		bool isServerHQ() const					{ return _isServerHQ; }
		const CString& getDescription() const	{ return description; }
		const CString& getLanguage() const		{ return language; }
		const CString& getName() const			{ return name; }
		const CString& getPort() const			{ return port; }
		const CString& getUrl() const			{ return url; }
		const CString& getVersion() const		{ return version; }
		int getPlayerCount() const				{ return (int)playerList.size(); };
		int getLastData() const					{ return (int)difftime(time(nullptr), _lastData); }
		int64_t getCurrentUpTime() const		{ return (int64_t)difftime(time(nullptr), _startTime); }
		size_t getUpTime() const				{ return _serverUpTime + getCurrentUpTime(); }
		ServerHQLevel getServerLevel() const	{ return _serverLevel; }
		ServerPlayer * getPlayer(unsigned short id) const;
		ServerPlayer * getPlayer(const std::string& account) const;
		ServerPlayer * getPlayer(const std::string& account, int type) const;
		void clearPlayerList();
		void sendText(const CString& data);
		void sendTextForPlayer(ServerPlayer *player, const CString& data);
		void updatePlayers();

		// send-packet functions
		void sendCompress();
		void sendPacket(CString pPacket, bool pSendNow = false);

	private:
		// packet-functions
		static void createServerPtrTable();
		bool parsePacket(CString& pPacket);

		bool msgSVI_NULL(CString& pPacket);
		bool msgSVI_SETNAME(CString& pPacket);
		bool msgSVI_SETDESC(CString& pPacket);
		bool msgSVI_SETLANG(CString& pPacket);
		bool msgSVI_SETVERS(CString& pPacket);
		bool msgSVI_SETURL(CString& pPacket);
		bool msgSVI_SETIP(CString& pPacket);
		bool msgSVI_SETPORT(CString& pPacket);
		bool msgSVI_SETPLYR(CString& pPacket);
		bool msgSVI_VERIACC(CString& pPacket);
		bool msgSVI_VERIGLD(CString& pPacket);
		bool msgSVI_GETFILE(CString& pPacket);
		bool msgSVI_NICKNAME(CString& pPacket);
		bool msgSVI_GETPROF(CString& pPacket);
		bool msgSVI_SETPROF(CString& pPacket);
		bool msgSVI_PLYRADD(CString& pPacket);
		bool msgSVI_PLYRREM(CString& pPacket);
		bool msgSVI_SVRPING(CString& pPacket);
		bool msgSVI_VERIACC2(CString& pPacket);
		bool msgSVI_SETLOCALIP(CString& pPacket);
		bool msgSVI_GETFILE2(CString& pPacket);
		bool msgSVI_UPDATEFILE(CString& pPacket);
		bool msgSVI_GETFILE3(CString& pPacket);
		bool msgSVI_NEWSERVER(CString& pPacket);
		bool msgSVI_SERVERHQPASS(CString& pPacket);
		bool msgSVI_SERVERHQLEVEL(CString& pPacket);
		bool msgSVI_SERVERINFO(CString& pPacket);
		bool msgSVI_REQUESTLIST(CString& pPacket);
		bool msgSVI_REQUESTSVRINFO(CString& pPacket);
		bool msgSVI_REGISTERV3(CString & pPacket);
		bool msgSVI_SENDTEXT(CString& pPacket);
		bool msgSVI_REQUESTBUDDIES(CString& pPacket);
		bool msgSVI_PMPLAYER(CString& pPacket);

	private:
		ListServer *_listServer;
		std::unique_ptr<CSocket> _socket;

		bool _disconnect;
		std::string _disconnectMsg;
		
		// Packet protocol
		bool nextIsRaw;
		int rawPacketSize;
		bool new_protocol;
		CFileQueue _fileQueue;
		CString sendBuffer, sockBuffer, outBuffer;

		bool _isAuthenticated;
		CString description, ip, language, name, port, url, version, localip;
		std::vector<ServerPlayer *> playerList;
		time_t _lastData, _lastPing, _startTime;
		ProtocolVersion _serverProtocol;
		uint8_t _allowedVersionsMask;

		bool _isServerHQ;
		ServerHQLevel _serverLevel, _serverMaxLevel;
		size_t _serverUpTime;
		std::string _serverAuthToken;
};

#endif // SERVERCONNECT_H
