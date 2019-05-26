#ifndef NO_MYSQL
#include <vector>
#include "CString.h"
#include "CMySQL.h"

CMySQL::CMySQL(const char *pServer, const char *pUsername, const char *pPassword, const char *pDatabase, const char *pPort, const char *pExternal)
	: mysql(nullptr), res(nullptr), isConnected(false), server(pServer), port(pPort), username(pUsername), password(pPassword), database(pDatabase), external(pExternal)
{
	connect();
}

CMySQL::~CMySQL()
{
	mysql_close(mysql);
}

bool CMySQL::connect()
{
	isConnected = false;

	if (mysql != nullptr)
	{
		mysql_close(mysql);
		mysql = nullptr;
	}

	if ((mysql = mysql_init(mysql)) == nullptr)
		return false;

	unsigned int sqlPort = 0;
	if (!port.empty())
		sqlPort = std::stoi(port);

	if (!mysql_real_connect(mysql, server.c_str(), username.c_str(), password.c_str(), database.c_str(), sqlPort, external.c_str(), 0))
		return false;

	// this will update isConnected
	ping();
	return isConnected;
}

int CMySQL::ping()
{
	int result = mysql_ping(mysql);
	isConnected = (result == 0);
	return result;
}

void CMySQL::update()
{
	while (!queued_commands.empty())
	{
		int ret = mysql_query(mysql, queued_commands.front().c_str());
		if (ret == 0)
		{
			queued_commands.pop();
			continue;
		}

		if (ret == 2006 || ret == 2013)
			isConnected = false;
		else queued_commands.pop();
		break;
	}

	if (!isConnected)
		connect();
}

void CMySQL::add_simple_query(const std::string& query)
{
	queued_commands.push(query);
	update();
}

int CMySQL::try_query(const std::string& query, std::vector<std::string>& result)
{
	if (!isConnected)
	{
		if (!connect())
			return -1;
	}

	// run query
	if (mysql_query(mysql, query.c_str()) != 0)
	{
		isConnected = false;
		return -1;
	}

	// store result
	if (!(res = mysql_store_result(mysql)))
	{
		isConnected = false;
		return -1;
	}

	// fetch row
	MYSQL_ROW row = mysql_fetch_row(res);
	unsigned long* lengths = mysql_fetch_lengths(res);
	if (lengths == 0 || row == 0)
	{
		mysql_free_result(res);
		return 0;
	}

	// empty old result
	result.clear();

	// Grab the full value and add it to the vector.
	for (unsigned int i = 0; i < mysql_num_fields(res); i++)
	{
		char* temp = new char[lengths[i] + 1];
		memcpy(temp, row[i], lengths[i]);
		temp[lengths[i]] = '\0';
		result.emplace_back(std::string(temp));
		delete[] temp;
	}

	// cleanup
	mysql_free_result(res);
	return result.size();
}

int CMySQL::try_query_rows(const std::string& query, std::vector<std::vector<std::string> >& result)
{
	if (!isConnected)
	{
		if (!connect())
			return -1;
	}

	// run query
	if (mysql_query(mysql, query.c_str()))
	{
		isConnected = false;
		return -1;
	}

	// store result
	if (!(res = mysql_store_result(mysql)))
	{
		isConnected = false;
		return -1;
	}

	int row_count = 0;
	result.clear();

	// fetch row
	MYSQL_ROW row = 0;
	while ((row = mysql_fetch_row(res)))
	{
		unsigned long* lengths = mysql_fetch_lengths(res);
		if (lengths == 0 || row == 0)
		{
			mysql_free_result(res);
			return row_count;
		}

		// Grab the full value and add it to the vector.
		std::vector<std::string> r;
		for (unsigned int i = 0; i < mysql_num_fields(res); i++)
		{
			char* temp = new char[lengths[i] + 1];
			memcpy(temp, row[i], lengths[i]);
			temp[lengths[i]] = '\0';
			r.emplace_back(std::string(temp));
			delete[] temp;
		}

		// Push back the row.
		result.push_back(r);
		++row_count;
	}

	// cleanup
	mysql_free_result(res);
	return row_count;
}

#endif
