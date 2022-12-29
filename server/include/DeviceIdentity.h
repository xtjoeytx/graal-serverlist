#ifndef DEVICEIDENTITY_H
#define DEVICEIDENTITY_H

#pragma once

#include <algorithm>
#include "CString.h"

class DeviceIdentity
{
public:
	DeviceIdentity() = default;

	DeviceIdentity(const CString& str) {
		auto tokens = str.gCommaStrTokens();
		if (tokens.size() > 0)
			platform = tokens[0].text();
		if (tokens.size() > 1)
			unknown = tokens[1].text();	
		if (tokens.size() > 2)
			hdd_hash = tokens[2].text();
		if (tokens.size() > 3)
			nic_hash = tokens[3].text();
		if (tokens.size() > 4)
			os_version = tokens[4].text();
		if (tokens.size() > 5)
			android_id = tokens[5].text();
	}

	std::string platform;
	std::string unknown;
	std::string hdd_hash;
	std::string nic_hash;
	std::string os_version;
	std::string android_id;
};

#endif
