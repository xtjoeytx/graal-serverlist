#ifndef NO_MYSQL
#include <vector>
#include "CString.h"
#include "CMySQL.h"

CMySQL::CMySQL(const char *pServer, const char *pUsername, const char *pPassword, const char *pDatabase, const char *pExternal)
{
	mysql = NULL;
	res   = NULL;

	connect(pServer, pUsername, pPassword, pDatabase, pExternal);
}

CMySQL::~CMySQL()
{
	mysql_close(mysql);
}

bool CMySQL::connect(const char *pServer, const char *pUsername, const char *pPassword, const char *pDatabase, const char *pExternal)
{
	database = pDatabase;
	external = pExternal;
	password = pPassword;
	server   = pServer;
	username = pUsername;

	if ((mysql = mysql_init(mysql)) == NULL)
		return false;

	if (!mysql_real_connect(mysql, server, username, password, database, 0, external, 0))
		return false;

	return ping();
}

bool CMySQL::ping()
{
	return (mysql_ping(mysql) == 0);
}

int CMySQL::query(const CString& pQuery, std::vector<CString> *pResult)
{
	// run query
	if (mysql_query(mysql, pQuery.text()))
		return 0;

	// no result wanted?
	if (pResult != 0)
	{
		// store result
		if (!(res = mysql_store_result(mysql)))
			return 0;

		// fetch row
		MYSQL_ROW row = mysql_fetch_row(res);
		unsigned long* lengths = mysql_fetch_lengths(res);
		if (lengths == 0 || row == 0)
		{
			mysql_free_result(res);
			return 0;
		}

		// empty old result
		pResult->clear();

		// Grab the full value and add it to the vector.
		for (unsigned int i = 0; i < mysql_num_fields(res); i++)
		{
			char* temp = new char[lengths[i] + 1];
			memcpy(temp, row[i], lengths[i]);
			temp[lengths[i]] = '\0';
			pResult->push_back(CString(temp));
			delete temp;
		}

		// cleanup
		mysql_free_result(res);
		return pResult->size();
	}
	else
		return 0;
}

const char* CMySQL::error()
{
	return mysql_error(mysql);
}
#endif
