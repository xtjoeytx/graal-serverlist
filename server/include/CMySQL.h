#ifndef HCMYSQL
#define HCMYSQL

#ifndef NO_MYSQL

#include <vector>
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

		bool connect(const char *pServer, const char *pUsername, const char *pPassword, const char *pDatabase, const char *pExternal = "");
		bool ping();
		const char* error();
		int query(const CString& pQuery, std::vector<CString> *pResult = NULL);
		int query_rows(const CString& pQuery, std::vector<std::vector<CString> > *pResult = NULL);

	private:
		const char *database, *external, *password, *server, *username;
		MYSQL *mysql;
		MYSQL_RES *res;
};

#endif

#endif
