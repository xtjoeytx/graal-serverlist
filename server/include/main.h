#ifndef MAIN_H
#define MAIN_H

/*
	Header-Includes
*/
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <vector>

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#endif

/*
	Custom-Class Includes
*/
#ifndef NO_MYSQL
	#include "CMySQL.h"
#endif
#include "CSettings.h"
#include "CSocket.h"
#include "CString.h"

/*
	Defiine Functions
*/
CString getAccountError(int pErrorId);
CString getServerList(int PLVER, const CString& pIp = "");
CString getBuddies(CString& pAccount);
CString getOwnedServers(CString& pAccount);
CString getOwnedServersPM(CString& pAccount);
CString getServerPlayers(CString& servername);
int getPlayerCount();
int getServerCount();
int verifyAccount(CString& pAccount, const CString& pPassword, bool fromServer = false);
int verifyGuild(const CString& pAccount, const CString& pNickname, const CString& pGuild);
void acceptSock(CSocket& pSocket, int pType);
void shutdownServer( int signal );

CString CString_Base64_Decode(const CString& input);
CString CString_Base64_Encode(const CString& input);

/*
	Definitions
*/
enum // Return-Errors
{
	ERR_SUCCESS			= 0,
	ERR_SETTINGS		= -1,
	ERR_SOCKETS			= -2,
	ERR_MYSQL			= -3,
	ERR_LISTEN			= -4,
};

enum // Socket Type
{
	SOCK_PLAYER			= 0,
	SOCK_SERVER			= 1,
	SOCK_PLAYEROLD		= 2,
};

enum // Account Status
{
	ACCSTAT_NORMAL		= 0,
	ACCSTAT_NONREG		= 1,
	ACCSTAT_BANNED		= 2,
	ACCSTAT_INVALID		= 3,
	ACCSTAT_ERROR		= 4,
};

enum // Guild Status
{
	GUILDSTAT_DISALLOWED	= 0,
	GUILDSTAT_ALLOWED		= 1,
};

#endif // MAIN_H
