#ifndef TSERVER_H
#define TSERVER_H

#include <time.h>

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
	SVI_VERIACC			= 8,
	SVI_VERIGLD			= 9,
	SVI_GETFILE			= 10,
	SVI_NICKNAME		= 11,
	SVI_GETPROF			= 12,
	SVI_SETPROF			= 13,
	SVI_PLYRADD			= 14,
	SVI_PLYRREM			= 15,
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
};

enum
{
	SVO_VERIACC			= 0,
	SVO_VERIGLD			= 1,
	SVO_FILESTART		= 2,
	SVO_FILEEND			= 3,
	SVO_FILEDATA		= 4,
	SVO_NULL2			= 5,
	SVO_NULL3			= 6,
	SVO_PROFILE			= 7,
	SVO_ERRMSG			= 8,
	SVO_NULL4			= 9,
	SVO_NULL5			= 10,
	SVO_VERIACC2		= 11,
	SVO_FILESTART2		= 12,
	SVO_FILEDATA2		= 13,
	SVO_FILEEND2		= 14,
	SVO_FILESTART3		= 15,
	SVO_FILEDATA3		= 16,
	SVO_FILEEND3		= 17,
	SVO_SERVERINFO		= 18,
	SVO_REQUESTTEXT		= 19,
	SVO_PING			= 99,
	SVO_RAWDATA			= 100,
};

enum
{
	VERSION_1		= 0,
	VERSION_2		= 1,
};

enum
{
	TYPE_HIDDEN		= 0,
	TYPE_BRONZE		= 1,
	TYPE_HOSTED		= 1,
	TYPE_SILVER		= 2,
	TYPE_CLASSIC	= 2,
	TYPE_GOLD		= 3,
	TYPE_3D			= 4,
};

class TServer;
void createSVFunctions();
typedef bool (TServer::*TSVSock)(CString&);

struct player
{
	CString account, nick, level;
	float x, y;
	unsigned char ap, type;
};

class TServer
{
	public:
		// constructor-destructor
		TServer(CSocket *pSocket);
		~TServer();

		// main loop
		bool doMain();

		// kill client
		void kill();

		void SQLupdate(CString tblval, const CString& newVal);
		void SQLupdateHQ(CString tblval, const CString& newVal);
		void updatePlayers();

		// get-value functions
		const CString& getDescription();
		const CString getIp(const CString& pIp = "");
		const CString& getLanguage();
		const CString& getName();
		const CString getPlayers();
		const int getPCount();
		const CString& getPort();
		const CString getType(int PLVER);
		int getTypeVal();
		const CString& getUrl();
		const CString& getVersion();
		const CString getServerPacket(int PLVER, const CString& pIp = "");
		int getLastData()	{ return (int)difftime( time(0), lastData ); }
		CSocket* getSock()	{ return sock; }

		// send-packet functions
		void sendCompress();
		void sendPacket(CString pPacket);

		// packet-functions;
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
		bool msgSVI_REQUESTLIST2(CString& pPacket);
		bool msgSVI_REQUESTBUDDIES(CString& pPacket);

	private:
		CSocket *sock;
		CString sendBuffer, sockBuffer, outBuffer;

		CString description, ip, language, name, port, url, version, localip;
		std::vector<player *> playerList;
		time_t lastPing, lastData, lastPlayerCount, lastUptimeCheck;
		bool addedToSQL;
		bool isServerHQ;
		CString serverhq_pass;
		unsigned char serverhq_level;
		int server_version;
};

#endif // TSERVER_H
