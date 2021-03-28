#include <vector>
#include "MySQLBackend.h"

MySQLBackend::MySQLBackend(const std::string& host, unsigned int port, const std::string& socket,
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
	MySQLBackend::Cleanup();
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
	if (_connection.is_open())
		_connection.close();
}

int MySQLBackend::Ping()
{
	return (isConnected() ? 0 : -1);
}

bool MySQLBackend::isIpBanned(const std::string& ipAddress)
{
	const std::string query = "SELECT id FROM graal_ipban WHERE " \
		"`server_ip` = ? LIMIT 1";

	int id;

	try {
		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(ipAddress);
		stmt.bind_result(id);

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

AccountStatus MySQLBackend::verifyAccount(const std::string& account, const std::string& password)
{
	const std::string query = "SELECT account, activated, banned FROM graal_users WHERE " \
		"`account` = ? AND password=MD5(CONCAT(MD5(?),`salt`)) LIMIT 1";

	// results
	std::string acct;
	int activated, banned;

	try {
		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(account, password);
		stmt.bind_result(acct, activated, banned);

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

GuildStatus MySQLBackend::verifyGuild(const std::string& account, const std::string& nickname, const std::string& guild)
{
	const std::string query = "SELECT gm.nickname, gg.restrictnick " \
		"FROM `graal_guilds_members` gm JOIN `graal_guilds` gg on gg.name = gm.guild " \
		"WHERE gg.status = 1 and gg.name = ? and gm.account = ? LIMIT 1";
	
	try {
		std::string guildNick;
		int restrictNick;

		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(guild, account);
		stmt.bind_result(guildNick, restrictNick);

		if (stmt.execute())
		{
			if (stmt.fetch())
			{
				if (restrictNick && nickname != guildNick)
					return GuildStatus::Invalid;

				return GuildStatus::Valid;
			}
		}
	}
	catch (std::exception& exp) {
		printf("VerifyGuild Exception: %s\n", exp.what());
	}

	return GuildStatus::Invalid;
}

std::optional<PlayerProfile> MySQLBackend::getProfile(const std::string& account)
{
	const std::string query = "SELECT profile_name, profile_age, profile_sex, profile_country, profile_icq, profile_email, profile_url, profile_hangout, profile_quote " \
		"FROM graal_users " \
		"WHERE `account` = ? LIMIT 1";

	// results
	std::string profile_name, profile_sex, profile_country, profile_icq, profile_email, profile_url, profile_hangout, profile_quote;
	int profile_age;

	try {
		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(account);
		stmt.bind_result(profile_name, profile_age, profile_sex, profile_country, profile_icq, profile_email, profile_url, profile_hangout, profile_quote);

		if (stmt.execute())
		{
			if (stmt.fetch())
			{
				PlayerProfile profile(account);
				profile.setName(profile_name);
				profile.setAge(profile_age);
				profile.setGender(profile_sex);
				profile.setCountry(profile_country);
				profile.setMessenger(profile_icq);
				profile.setEmail(profile_email);
				profile.setWebsite(profile_url);
				profile.setHangout(profile_hangout);
				profile.setQuote(profile_quote);
				return profile;
			}
		}
	} catch (std::exception& exp) {
		printf("GetProfile Exception: %s\n", exp.what());
	}

	return std::nullopt;
}

bool MySQLBackend::setProfile(const PlayerProfile& profile)
{
	const std::string query = "UPDATE graal_users SET profile_name = ?, profile_age = ?, profile_sex = ?, profile_country = ?, " \
		"profile_icq = ?, profile_email = ?, profile_url = ?, profile_hangout = ?, profile_quote = ? " \
		"WHERE `account` = ? LIMIT 1";

	try {
		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(profile.getName(), profile.getAge(), profile.getGender(), profile.getCountry(),
						profile.getMessenger(), profile.getEmail(), profile.getWebsite(), profile.getHangout(), profile.getQuote(),
						profile.getAccountName());

		if (stmt.execute())
			return (_connection.affected_rows() == 1);
	}
	catch (std::exception& exp) {
		printf("SetProfile Exception: %s\n", exp.what());
	}

	return false;
}

ServerHQResponse MySQLBackend::verifyServerHQ(const std::string& serverName, const std::string& token)
{
	ServerHQResponse response{};

	const std::string query = "SELECT name, maxlevel, activated, uptime, password, MD5(CONCAT(MD5(?), `salt`)) " \
		"FROM graal_serverhq " \
		"WHERE `name` = ? LIMIT 1";

	try {
		std::string name, password, tokenHashed;
		int activated, maxlevel;
		size_t uptime;

		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(token, serverName);
		stmt.bind_result(name, maxlevel, activated, uptime, password, tokenHashed);

		if (stmt.execute())
		{
			if (stmt.fetch())
			{
				if (tokenHashed == password)
				{
					response.status = (activated ? ServerHQStatus::Valid : ServerHQStatus::NotActivated);
					response.serverHq.serverName = name;
					response.serverHq.uptime = uptime;

					response.serverHq.maxLevel = getServerHQLevel(maxlevel);
					if (response.serverHq.maxLevel < ServerHQLevel::Bronze)
						response.serverHq.maxLevel = ServerHQLevel::Bronze;
				}
				else response.status = ServerHQStatus::InvalidPassword;
			}
			else response.status = ServerHQStatus::Unregistered;
		}
	}
	catch (std::exception& exp) {
		printf("verifyServerHQ Exception: %s\n", exp.what());
		response.status = ServerHQStatus::BackendError;
	}

	return response;
}

bool MySQLBackend::updateServerUpTime(const std::string& serverName, size_t uptime)
{
	const std::string query = "UPDATE graal_serverhq SET `uptime` = ?, `lastconnected` = NOW() WHERE `name` = ? AND `uptime` < ? LIMIT 1";

	try {
		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(uptime, serverName, uptime);

		if (stmt.execute())
			return (_connection.affected_rows() == 1);
	}
	catch (std::exception& exp) {
		printf("updateServerUpTime Exception: %s\n", exp.what());
	}

	return false;
}
