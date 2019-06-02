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
		MySQLBackend(const std::string& host, int port, const std::string& external, const std::string& user, const std::string& password, const std::string& database);
		~MySQLBackend();

		int Initialize() override;
		void Cleanup() override;
		int Ping() override;

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
};

#endif
