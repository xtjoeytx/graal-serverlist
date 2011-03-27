#ifndef HCMYSQL
#define HCMYSQL

#ifndef NO_MYSQL

#include <vector>
#include <queue>
#include "CString.h"
#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#define my_socket_defined
	#define my_socket int
#endif
#include "mysql/mysql.h"

class CMySQL
{
	public:
		CMySQL(const char *pServer, const char *pUsername, const char *pPassword, const char *pDatabase, const char *pExternal = "");
		~CMySQL();

		bool connect();
		bool ping();
		const char* error();

		void update();

		void add_simple_query(const CString& query);
		int try_query(const CString& query, std::vector<CString>& result);
		int try_query_rows(const CString& query, std::vector<std::vector<CString> >& result);

		bool connected() const	{ return isConnected; }

	private:
		MYSQL *mysql;
		MYSQL_RES *res;
		CString database, external, password, server, username;

		bool isConnected;
		std::queue<CString> queued_commands;
};

#endif

#endif
