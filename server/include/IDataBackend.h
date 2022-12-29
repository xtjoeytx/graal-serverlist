#ifndef IDATABACKEND_H
#define IDATABACKEND_H

#pragma once

#include <optional>
#include <string>
#include "DeviceIdentity.h"
#include "PlayerProfile.h"
#include "ServerHQ.h"

enum class AccountStatus
{
	BackendError		= -1,
	Normal				= 0,
	NotActivated		= 1,
	Banned				= 2,
	InvalidPassword		= 3,
	NotFound			= 4
};

enum class GuildStatus
{
	BackendError		= -1,
	Valid				= 0,
	Invalid				= 1,
};

class IDataBackend
{
	public:
		IDataBackend() = default;
		virtual ~IDataBackend() = default;

		virtual int Initialize() = 0;
		virtual void Cleanup() = 0;
		virtual int Ping() = 0;

		virtual bool isConnected() const = 0;
		virtual std::string getLastError() const = 0;

		/// Methods for interfacing with the backend.
		// 
		virtual bool isIpBanned(const std::string& ipAddress) = 0;

		virtual bool updateDeviceIdTime(int64_t deviceId) = 0;
		virtual std::optional<int64_t> getDeviceId(const DeviceIdentity& ident) = 0;

		virtual AccountStatus verifyAccount(const std::string& account, const std::string& password) = 0;
		virtual GuildStatus verifyGuild(const std::string& account, const std::string& nickname, const std::string& guild) = 0;

		virtual std::optional<PlayerProfile> getProfile(const std::string& account) = 0;
		virtual bool setProfile(const PlayerProfile& profile) = 0;

		virtual bool addBuddy(const std::string& account, const std::string& buddyAccount) = 0;
		virtual bool removeBuddy(const std::string& account, const std::string& buddyAccount) = 0;
		virtual std::optional<std::vector<std::string>> getBuddyList(const std::string& account) = 0;

		// Server HQ
		virtual ServerHQResponse verifyServerHQ(const std::string& serverName, const std::string& token) = 0;
		virtual bool updateServerUpTime(const std::string& serverName, size_t uptime) = 0;
};

#endif
