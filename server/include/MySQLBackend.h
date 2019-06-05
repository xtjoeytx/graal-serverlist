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

		bool IsConnected() const override;
		std::string GetLastError() const override;

		/// Methods for interfacing with the backend.
		// 
		bool IsIpBanned(const std::string& ipAddress) override;
		int VerifyAccount(const std::string& account, const std::string& password) override;
		int VerifyGuild(const std::string& account, const std::string& nickname, const std::string& guild) override;
		PlayerProfile GetProfile(const std::string& account) override;
		void SetProfile(const PlayerProfile& profile) override;

	private:
		daotk::mysql::connection _connection;
		daotk::mysql::connect_options _connectionOptions;
};

inline bool MySQLBackend::IsConnected() const
{
	return _connection.is_open();
}

inline std::string MySQLBackend::GetLastError() const
{
	return _connection.error_message();
}

#endif
