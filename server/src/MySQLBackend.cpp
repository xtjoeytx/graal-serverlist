#include <vector>
#include "MySQLBackend.h"
#if defined(__MINGW32__) || defined(__MINGW64__)
extern "C" {
	int strerror_r(int errno, char *buf, size_t len) {
		return strerror_s(buf, len, errno);
	}
}
#endif

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

std::optional<int64_t> MySQLBackend::getDeviceId(const DeviceIdentity & ident)
{
	const std::string query = "SELECT id FROM graal_pcids WHERE " \
		"`platform` = ? AND `hdd-md5hash` = ? AND `nic-md5hash` = ? AND `android-id` = ? LIMIT 1";

	try {
		int64_t pcid;

		// search for an existing identity in the database
		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(ident.platform, ident.hdd_hash, ident.nic_hash, ident.android_id);
		stmt.bind_result(pcid);
		if (stmt.execute() && stmt.fetch())
			return pcid;

		// insert hashes into pcid list and grab insert id as the pc-id
		const std::string insertQuery = "INSERT INTO graal_pcids(`platform`, `hdd-md5hash`, `nic-md5hash`, `android-id`, `firstconnected`) values (?, ?, ?, ?, NOW())";

		daotk::mysql::prepared_stmt insertStmt(_connection, insertQuery);
		insertStmt.bind_param(ident.platform, ident.hdd_hash, ident.nic_hash, ident.android_id);
		if (insertStmt.execute() && _connection.affected_rows() == 1)
			return _connection.last_insert_id();
	}
	catch (std::exception& exp) {
		printf("getDeviceId Exception: %s\n", exp.what());
		return std::nullopt;
	}

	return std::optional<int64_t>();
}

bool MySQLBackend::updateDeviceIdTime(int64_t deviceId)
{
	const std::string query = "UPDATE graal_pcids SET `lastconnected` = NOW() WHERE `id` = ? COLLATE 'utf8mb4_general_ci' COLLATE 'utf8mb4_general_ci' LIMIT 1";

	try {
		daotk::mysql::prepared_stmt stmt(_connection, query);
		stmt.bind_param(deviceId);

		if (stmt.execute())
			return (_connection.affected_rows() == 1);
	}
	catch (std::exception& exp) {
		printf("updateDeviceIdTime Exception: %s\n", exp.what());
	}

	return false;
}

AccountStatus MySQLBackend::verifyAccount(const std::string& account, const std::string& password)
{
	const std::string query = "SELECT account, activated, banned FROM graal_users WHERE " \
		"`account` = ? AND password=MD5(CONCAT(MD5(?),`salt`)) COLLATE 'utf8mb4_general_ci' LIMIT 1";

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
		"WHERE gg.status = 1 and gg.name = ? and gm.account = ? COLLATE 'utf8mb4_general_ci' LIMIT 1";

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
		"WHERE `account` = ? COLLATE 'utf8mb4_general_ci' LIMIT 1";

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
		"WHERE `account` = ? COLLATE 'utf8mb4_general_ci' LIMIT 1";

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

bool MySQLBackend::addBuddy(const std::string& account, const std::string& buddyAccount)
{
	const std::string query = "SELECT id FROM `graal_users` WHERE `account` = '%s' COLLATE 'utf8mb4_general_ci' LIMIT 1";

	try {
		auto user_id = _connection.query(query, escapeStr(account).c_str()).get_value<std::optional<int>>();
		auto buddy_id = _connection.query(query, escapeStr(buddyAccount).c_str()).get_value<std::optional<int>>();

		if (user_id && buddy_id)
		{
			const std::string insertQuery = "INSERT INTO graal_buddies(users_id, friend_id) values (?, ?)";

			daotk::mysql::prepared_stmt stmt(_connection, insertQuery);
			stmt.bind_param(user_id.value(), buddy_id.value());

			if (stmt.execute())
				return (_connection.affected_rows() == 1);
		}
	}
	catch (std::exception& exp) {
		printf("addBuddy Exception: %s\n", exp.what());
	}

	return false;
}

bool MySQLBackend::removeBuddy(const std::string& account, const std::string& buddyAccount)
{
	const std::string query = "SELECT id FROM `graal_users` WHERE `account` = '%s' COLLATE 'utf8mb4_general_ci' LIMIT 1";

	try {
		auto user_id = _connection.query(query, escapeStr(account).c_str()).get_value<std::optional<int>>();
		auto buddy_id = _connection.query(query, escapeStr(buddyAccount).c_str()).get_value<std::optional<int>>();

		if (user_id && buddy_id)
		{
			const std::string insertQuery = "DELETE FROM `graal_buddies` WHERE `users_id` = ? AND `friend_id` = ? COLLATE 'utf8mb4_general_ci' LIMIT 1";

			daotk::mysql::prepared_stmt stmt(_connection, insertQuery);
			stmt.bind_param(user_id.value(), buddy_id.value());

			if (stmt.execute())
				return (_connection.affected_rows() == 1);
		}
	}
	catch (std::exception& exp) {
		printf("removeBuddy Exception: %s\n", exp.what());
	}

	return false;
}

std::optional<std::vector<std::string>> MySQLBackend::getBuddyList(const std::string& account)
{
	const std::string query = "SELECT id FROM `graal_users` WHERE `account` = '%s' COLLATE 'utf8mb4_general_ci' LIMIT 1";

	try {
		auto user_id = _connection.query(query, escapeStr(account).c_str()).get_value<std::optional<int>>();
		if (user_id)
		{
			const std::string getBuddiesQuery = "select gu.account from graal_buddies gb join graal_users gu on gu.id = gb.friend_id where gb.users_id = ? COLLATE 'utf8mb4_general_ci'";

			std::string buddy;

			daotk::mysql::prepared_stmt stmt(_connection, getBuddiesQuery);
			stmt.bind_param(user_id.value());
			stmt.bind_result(buddy);
			if (stmt.execute())
			{
				std::vector<std::string> buddies;
				while (stmt.fetch())
					buddies.push_back(buddy);

				return buddies;
			}

		}
	}
	catch (std::exception& exp) {
		printf("getBuddyList Exception: %s\n", exp.what());
	}

	return std::nullopt;
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

std::string MySQLBackend::escapeStr(const std::string& input)
{
	if (input.length() < 512)
	{
		char sbuf[1024];
		mysql_real_escape_string(_connection.get_raw_connection(), sbuf, input.c_str(), input.length());
		return std::string{ sbuf };
	}

	char* buf = new char[input.length() * 2 + 1];
	mysql_real_escape_string(_connection.get_raw_connection(), buf, input.c_str(), input.length());
	std::string res(buf);
	delete[] buf;
	return res;
}
