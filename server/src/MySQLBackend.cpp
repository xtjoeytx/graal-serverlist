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

	// Auto-reconnect
	_connectionOptions.autoreconnect = true;

}

MySQLBackend::~MySQLBackend()
{
	this->Cleanup();
}

int MySQLBackend::Initialize()
{
	// Close any connections
	Cleanup();

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
	const std::string query = "SELECT id graal_ipban WHERE " \
		"`server_ip` = ? LIMIT 1";

	int id;

	daotk::mysql::prepared_stmt stmt(_connection, query);
	stmt.bind_param(ipAddress);
	stmt.bind_result(id);

	try {
		if (stmt.execute())
		{
			if (stmt.fetch())
				return true;
		}
	} catch (std::exception& exp) {
		printf("IsIpBanned Exception: %s\n", exp.what());
	}

	return false;
}

AccountStatus MySQLBackend::VerifyAccount(const std::string& account, const std::string& password)
{
	const std::string query = "SELECT account, activated, banned FROM graal_users WHERE " \
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
			if (stmt.fetch())
			{
				if (banned)
					return AccountStatus::Banned;

				if (!activated)
					return AccountStatus::NotActivated;

				return AccountStatus::Normal;
			}
		}
	} catch (std::exception& exp) {
		printf("VerifyAccount Exception: %s\n", exp.what());
		return AccountStatus::BackendError;
	}

	return AccountStatus::NotFound;
}

GuildStatus MySQLBackend::VerifyGuild(const std::string& account, const std::string& nickname, const std::string& guild)
{
	std::string queryTest = "SELECT account, activated, banned FROM graal_users WHERE " \
	"account = ? AND password=MD5(CONCAT(MD5(?),`salt`)) LIMIT 1";

	return GuildStatus::Invalid;
}

PlayerProfile MySQLBackend::GetProfile(const std::string& account)
{
	PlayerProfile profile(account);
	return profile;
}

void MySQLBackend::SetProfile(const PlayerProfile& profile)
{
}
