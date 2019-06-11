#pragma once

#ifndef MYSQLBACKEND_H
#define MYSQLBACKEND_H

#include <string>
#include <mysql+++.h>
#include "IDataBackend.h"

class MySQLBackend : public IDataBackend
{
	public:
		MySQLBackend(const std::string& host, int port, const std::string& socket, const std::string& user, const std::string& password, const std::string& database);
		virtual ~MySQLBackend();

		int Initialize() override;
		void Cleanup() override;
		int Ping() override;

		bool isConnected() const override;
		std::string getLastError() const override;

		/// Methods for interfacing with the backend.
		// 
		bool isIpBanned(const std::string& ipAddress) override;
		AccountStatus verifyAccount(const std::string& account, const std::string& password) override;
		GuildStatus verifyGuild(const std::string& account, const std::string& nickname, const std::string& guild) override;
		std::optional<PlayerProfile> getProfile(const std::string& account) override;
		bool setProfile(const PlayerProfile& profile) override;

	private:
		daotk::mysql::connection _connection;
		daotk::mysql::connect_options _connectionOptions;
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
