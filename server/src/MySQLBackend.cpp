#include <vector>
#include "MySQLBackend.h"

MySQLBackend::MySQLBackend(const std::string& host, int port, const std::string& socket,
	const std::string& user, const std::string& password, const std::string& database)
	: IDataBackend(), _isConnected(false), _mysql(nullptr), _mysqlresult(nullptr),
	_cfgHost(host), _cfgPort(port), _cfgSocket(socket),
	_cfgUser(user), _cfgPass(password), _cfgDatabase(database)
{

}

MySQLBackend::~MySQLBackend()
{
	this->Cleanup();
}

int MySQLBackend::Initialize()
{
	// Restore this to a clean state, freeing any resources used
	Cleanup();

	// Initialize mysql
	_mysql = mysql_init(_mysql);
	if (!_mysql)
		return -1;

	// Setup auto-reconnect
	//my_bool auto_reconnect = 1;
	//mysql_options(_mysql, MYSQL_OPT_RECONNECT, &auto_reconnect);

	// Unused
	unsigned long clientFlags = 0;

	// Connect to socket
	if (!mysql_real_connect(_mysql, _cfgHost.c_str(), _cfgUser.c_str(), _cfgPass.c_str(), _cfgDatabase.c_str(), _cfgPort, _cfgSocket.c_str(), clientFlags))
		return -2;

	return 0;
}

void MySQLBackend::Cleanup()
{
	if (_mysqlresult)
	{
		mysql_free_result(_mysqlresult);
		_mysqlresult = nullptr;
	}

	if (_mysql)
	{
		mysql_close(_mysql);
		_mysql = nullptr;
	}

	_isConnected = false;
}

int MySQLBackend::Ping()
{
	return 0;
}

bool MySQLBackend::IsIpBanned(const std::string& ipAddress)
{
	return false;
}

int MySQLBackend::VerifyAccount(const std::string& account, const std::string& password)
{
	// temporary testing, may look into the c++ mysql connector
	std::string queryTest = "SELECT activated, banned, account FROM graal_users WHERE ";
	queryTest += "account='" + account + "' AND ";
	queryTest += "password=MD5(CONCAT(MD5('" + password + "'), `salt`)) LIMIT 1";

	printf("Query: %s\n", queryTest.c_str());

	if (mysql_query(_mysql, queryTest.c_str()))
	{
		_isConnected = false;
		return -1;
	}

	// store result
	if (!(_mysqlresult = mysql_store_result(_mysql)))
	{
		_isConnected = false;
		return -2;
	}

	std::vector<std::string> result;

	int row_count = 0;

	// fetch row
	MYSQL_ROW row = 0;
	while ((row = mysql_fetch_row(_mysqlresult)))
	{
		unsigned long* lengths = mysql_fetch_lengths(_mysqlresult);
		if (lengths == 0 || row == 0)
		{
			mysql_free_result(_mysqlresult);
			return -3;
		}

		// Grab the full value and add it to the vector.
		//std::vector<std::string> r;
		for (unsigned int i = 0; i < mysql_num_fields(_mysqlresult); i++)
		{
			char* temp = new char[lengths[i] + 1];
			memcpy(temp, row[i], lengths[i]);
			temp[lengths[i]] = '\0';
			result.emplace_back(std::string(temp));
			delete[] temp;
		}

		// Push back the row.
		//result.push_back(r);
		++row_count;
	}

	for (auto it = result.begin(); it != result.end(); ++it)
	{
		printf("Test: %s\n", it->c_str());
	}

	return 0;
}

int MySQLBackend::VerifyGuild(const std::string& account, const std::string& nickname, const std::string& guild)
{
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
