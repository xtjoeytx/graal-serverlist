#ifndef NO_MYSQL
#include <vector>
#include "CString.h"
#include "CMySQL.h"

CMySQL::CMySQL(const char *pServer, const char *pUsername, const char *pPassword, const char *pDatabase, const char *pExternal)
{
	mysql = NULL;
	res   = NULL;

	database = pDatabase;
	external = pExternal;
	password = pPassword;
	server   = pServer;
	username = pUsername;

	isConnected = false;

	connect();
}

CMySQL::~CMySQL()
{
	mysql_close(mysql);
}

bool CMySQL::connect()
{
	isConnected = false;

	if ((mysql = mysql_init(mysql)) == NULL)
		return false;

	if (!mysql_real_connect(mysql, server.text(), username.text(), password.text(), database.text(), 0, external.text(), 0))
		return false;

	isConnected = true;
	return ping();
}

bool CMySQL::ping()
{
	isConnected = (mysql_ping(mysql) == 0);
	return isConnected;
}

const char* CMySQL::error()
{
	return mysql_error(mysql);
}

void CMySQL::update()
{
	while (!queued_commands.empty())
	{
		int ret = mysql_query(mysql, queued_commands.front().text());
		if (ret == 0)
		{
			queued_commands.pop();
			continue;
		}

		isConnected = false;
		break;
	}

	if (!isConnected)
		connect();
}

void CMySQL::add_simple_query(const CString& query)
{
	queued_commands.push(query);
	update();
}

int CMySQL::try_query(const CString& query, std::vector<CString>& result)
{
	if (!isConnected)
		return -1;

	// run query
	if (mysql_query(mysql, query.text()))
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
		result.push_back(CString(temp));
		delete temp;
	}

	// cleanup
	mysql_free_result(res);
	return result.size();
}

int CMySQL::try_query_rows(const CString& query, std::vector<std::vector<CString> >& result)
{
	if (!isConnected)
		return -1;

	// run query
	if (mysql_query(mysql, query.text()))
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
		std::vector<CString> r;
		for (unsigned int i = 0; i < mysql_num_fields(res); i++)
		{
			char* temp = new char[lengths[i] + 1];
			memcpy(temp, row[i], lengths[i]);
			temp[lengths[i]] = '\0';
			r.push_back(CString(temp));
			delete temp;
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
