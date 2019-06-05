#include <vector>
#include "MySQLBackend.h"

MySQLBackend::MySQLBackend(const std::string& host, int port, const std::string& socket,
	const std::string& user, const std::string& password, const std::string& database)
	: IDataBackend()
{
	// TODO(joey): The library doesn't support socket, i'll fork and add it eventually
	_connectionOptions.server = host;
	_connectionOptions.port = port;
	_connectionOptions.username = user;
	_connectionOptions.password = password;
	_connectionOptions.dbname = database;
}

MySQLBackend::~MySQLBackend()
{
	this->Cleanup();
}

int MySQLBackend::Initialize()
{
	// Close any connections
	Cleanup();

	// Auto-reconnect
	_connectionOptions.autoreconnect = true;

	// Initialize mysql
	if (!_connection.open(_connectionOptions))
		return -1;

	return 0;
}

void MySQLBackend::Cleanup()
{
	_connection.close();
}

int MySQLBackend::Ping()
{
	return (IsConnected() ? 0 : -1);
}

bool MySQLBackend::IsIpBanned(const std::string& ipAddress)
{
	return false;
}

int MySQLBackend::VerifyAccount(const std::string& account, const std::string& password)
{
	std::string query = "SELECT account, activated, banned FROM graal_users WHERE " \
		"`account` = ? AND password=MD5(CONCAT(MD5(?),`salt`)) LIMIT 1";

	// results
	std::string acct;
	int activated, banned;

	daotk::mysql::prepared_stmt stmt(_connection, query);
	stmt.bind_param(account, password);
	stmt.bind_result(acct, activated, banned);

	try {
		if (stmt.execute())
		{
			while (stmt.fetch())
			{
				printf("Account: %s\n", acct.c_str());
				printf("Activated: %d\n", activated);
				printf("Banned: %d\n", banned);
			}
		}
	}
	catch (daotk::mysql::mysql_exception exp)
	{
		printf("Mysql Exception (%d): %s\n", exp.error_number(), exp.what());
		return -3;
	}
	catch (daotk::mysql::mysqlpp_exception exp)
	{
		printf("Mysql++ Exception: %s\n", exp.what());
		return -2;
	}
	catch (std::runtime_error exp)
	{
		printf("Runtime Exception: %s\n", exp.what());
		return -1;
	}

	return 0;
}

int MySQLBackend::VerifyGuild(const std::string& account, const std::string& nickname, const std::string& guild)
{
	std::string queryTest = "SELECT account, activated, banned FROM graal_users WHERE " \
	"account = ? AND password=MD5(CONCAT(MD5(?),`salt`)) LIMIT 1";

	// results
	std::string acct;
	int activated, banned;

	// test query 1
	//auto res = my.query("SELECT account, activated, banned FROM graal_users where account = '%s'", account.c_str());
	//res.fetch(acct, activated, banned);

	//printf("Account: %s\n", acct.c_str());
	//printf("Activated: %d\n", activated);
	//printf("Banned: %d\n", banned);

	// test prep statement
	try {
		daotk::mysql::prepared_stmt stmt(_connection, queryTest);
		stmt.bind_param(account, nickname);
		stmt.bind_result(acct, activated, banned);
		if (stmt.execute())
		{
			while (stmt.fetch())
			{
				printf("Account: %s\n", acct.c_str());
				printf("Activated: %d\n", activated);
				printf("Banned: %d\n", banned);
			}
		}
	}
	catch (daotk::mysql::mysql_exception exp)
	{
		printf("Mysql Exception (%d): %s\n", exp.error_number(), exp.what());
		return -3;
	}
	catch (daotk::mysql::mysqlpp_exception exp)
	{
		printf("Mysql++ Exception: %s\n", exp.what());
		return -2;
	}
	catch (std::runtime_error exp)
	{
		printf("Runtime Exception: %s\n", exp.what());
		return -1;
	}

	return 0;
}

PlayerProfile MySQLBackend::GetProfile(const std::string& account)
{
	PlayerProfile profile(account);
	return profile;
}

void MySQLBackend::SetProfile(const PlayerProfile& profile)
{
}
