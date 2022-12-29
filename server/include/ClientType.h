#ifndef LISTSERVER_CLIENTTYPE_H
#define LISTSERVER_CLIENTTYPE_H

#include <IUtil.h>

enum class ClientType : uint8_t
{
	None = 0x0,
	Version1 = 0x1,		// 1.41 - early 2.00
	Version2 = 0x2,		// >= 2.17 && < 2.22
	Version3 = 0x4,		// >= 2.22 && < 4.00
	Version4 = 0x8,		// >= 4.00
	AllServers = Version1 | Version2 | Version3 | Version4,
};

uint8_t getVersionMask(const CString& versionStr);

#endif //LISTSERVER_CLIENTTYPE_H
