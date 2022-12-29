#ifndef MYSQLBACKEND_H
#define MYSQLBACKEND_H

#pragma once

#if defined(__MINGW32__) || defined(__MINGW64__)
#define localtime_r(x, y) localtime_s(y, x);
#endif

#include <string>
#include <mysql+++.h>
#include "IDataBackend.h"

class MySQLBackend : public IDataBackend
{
	public:
		MySQLBackend(const std::string& host, unsigned int port, const std::string& socket, const std::string& user, const std::string& password, const std::string& database);
		virtual ~MySQLBackend();

		int Initialize() override;
		void Cleanup() override;
		int Ping() override;

		bool isConnected() const override;
		std::string getLastError() const override;

		/// Methods for interfacing with the backend.
		//
		bool isIpBanned(const std::string& ipAddress) override;
		std::optional<int64_t> getDeviceId(const DeviceIdentity & ident) override;
		bool updateDeviceIdTime(int64_t deviceId) override;
		AccountStatus verifyAccount(const std::string& account, const std::string& password) override;
		GuildStatus verifyGuild(const std::string& account, const std::string& nickname, const std::string& guild) override;

		std::optional<PlayerProfile> getProfile(const std::string& account) override;
		bool setProfile(const PlayerProfile& profile) override;

		bool addBuddy(const std::string& account, const std::string& buddyAccount) override;
		bool removeBuddy(const std::string& account, const std::string& buddyAccount) override;
		std::optional<std::vector<std::string>> getBuddyList(const std::string& account) override;

		ServerHQResponse verifyServerHQ(const std::string& serverName, const std::string& token) override;
		bool updateServerUpTime(const std::string& serverName, size_t uptime) override;

	private:
		daotk::mysql::connection _connection;
		daotk::mysql::connect_options _connectionOptions;

		std::string escapeStr(const std::string& input);
};

inline bool MySQLBackend::isConnected() const
{
	return _connection.is_open();
}

inline std::string MySQLBackend::getLastError() const
{
	return _connection.error_message();
}

#endif
