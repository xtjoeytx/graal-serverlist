#include "MySQLBackend.h"

MySQLBackend::MySQLBackend(const std::string& host, int port, const std::string& external,
	const std::string& user, const std::string& password, const std::string& database)
	: IDataBackend(), _isConnected(false), _mysql(nullptr), _mysqlresult(nullptr)
{

}

MySQLBackend::~MySQLBackend()
{
	this->Cleanup();
}

int MySQLBackend::Initialize()
{
	return 0;
}

void MySQLBackend::Cleanup()
{
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
