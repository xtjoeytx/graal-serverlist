#ifndef HCMYSQL
#define HCMYSQL

#ifndef NO_MYSQL

#include <vector>
#include <queue>
#include <string>

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
		CMySQL(const char *pServer, const char *pUsername, const char *pPassword, const char *pDatabase, const char *pPort, const char *pExternal = "");
		~CMySQL();

		bool connect();
		int ping();
		void update();

		void add_simple_query(const std::string& query);
		int try_query(const std::string& query, std::vector<std::string>& result);
		int try_query_rows(const std::string& query, std::vector<std::vector<std::string> >& result);

		bool connected() const;
		const char * error() const;

private:
		bool isConnected;
		MYSQL *mysql;
		MYSQL_RES *res;
		std::string server, port, external;
		std::string database, username, password;

		std::queue<std::string> queued_commands;
};

inline bool CMySQL::connected() const {
	return isConnected;
}

inline const char * CMySQL::error() const {
	return mysql_error(mysql);
}

#endif

#endif
