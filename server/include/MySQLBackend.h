#pragma once

#ifndef MYSQLBACKEND_H
#define MYSQLBACKEND_H

#include <string>
#include "IDataBackend.h"

#ifdef _WIN32
	#define my_socket_defined
	#define my_socket int
#endif
#include "mysql/mysql.h"

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
		bool _isConnected;
		MYSQL *_mysql;
		MYSQL_RES *_mysqlresult;

		int _cfgPort;
		std::string _cfgHost, _cfgSocket;
		std::string _cfgUser, _cfgPass, _cfgDatabase;
};

inline bool MySQLBackend::IsConnected() const
{
	return _isConnected;
}

inline std::string MySQLBackend::GetLastError() const
{
	return mysql_error(_mysql);
}

#endif
