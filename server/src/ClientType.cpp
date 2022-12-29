#include "ClientType.h"

/*
	Quick overview of how each clients connect:

	V1.41 - Just establishes network connection, and expects the listserver to be sent upon connection
	V2.17 - Connects and sends client type "newmain" using PLI_V1VER packet, uses PLI_SERVERLIST for auth/list request
	v2.22 - v2.31 - Uses PLI_V2ENCRYPTKEYCL for authentication, both clients send the same version "GNW30123" and client type "newmain" so cant distinguish
	v4.00 - uses loginserver, not listserver
	RC1 - Uses PLI_V2SERVERLISTRC
	RC2 & RC3 - Uses PLI_V2ENCRYPTKEYCL for authentication, sends version "GNW30123" and client type "rc2"

*/

constexpr uint8_t getVersionMask(int version)
{
	if (version >= CLVER_1_39 && version < CLVER_2_1)
	{
		return static_cast<uint8_t>(ClientType::Version1);
	}
	else if (version >= CLVER_2_1 && version < CLVER_2_2)
	{
		return static_cast<uint8_t>(ClientType::Version2);
	}
	else if (version >= CLVER_2_2 && version < CLVER_4_0211)
	{
		return static_cast<uint8_t>(ClientType::Version3);
	}
	else if (version >= CLVER_4_0211)
	{
		return static_cast<uint8_t>(ClientType::Version4);
	}

	return static_cast<uint8_t>(ClientType::None);
}

uint8_t getVersionMask(const CString& versionStr)
{
	auto pos = versionStr.find(":");
	if (pos < 0)
		return getVersionMask(getVersionID(versionStr));

	// Range-based versions
	uint8_t mask = 0;
	int startRange = getVersionID(versionStr.subString(0, pos));

	if (startRange > 0)
	{
		int endRange = getVersionID(versionStr.subString(pos + 1));

		while (startRange <= endRange)
		{
			mask |= getVersionMask(startRange);
			startRange++;
		}
	}

	return mask;
}