#include "main.h"
#include "FProfile.h"
#include "CString.h"

#ifndef NO_MYSQL
	extern CMySQL *mySQL;
#endif
extern CSettings *settings;

bool getProfile( const CString& pAccountName, CString &pPacket)
{
	pPacket.clear();

#ifndef NO_MYSQL
	CString query;

	query << "SELECT profile_name, profile_age, profile_sex, profile_country, profile_icq, profile_email, profile_url, profile_hangout, profile_quote FROM " << settings->getStr("userlist") << " WHERE account='" << pAccountName.escape().text() << "' LIMIT 1";
	std::vector<CString> result;
	mySQL->query(query, &result);

	if (result.size() != 0)
	{
		for ( unsigned int i = 0; i < 9; ++i )
		{
			if ( i > result.size() - 1 )
			{
				pPacket >> (char)0;
				continue;
			}

			CString rowBuff( result[i] );
			if ( rowBuff.length() > (0xDF + 1) )	// 0xDF = 223
				pPacket >> (char)0xDF << rowBuff.subString( 0, (0xDF + 1) ); // 0xDF = 223
			else
				pPacket >> (char)rowBuff.length() << rowBuff;
		}
		//pPacket << (char)strlen(row[0]) << row[0] << (char)strlen(row[1]) << row[1] << (char)strlen(row[2]) << row[2] << (char)strlen(row[3]) << row[3] << (char)strlen(row[4]) << row[4] << (char)strlen(row[5]) << row[5] << (char)strlen(row[6]) << row[6] << (char)strlen(row[7]) << row[7] << (char)strlen(row[8]) << row[8];
		return true;
	}
#endif

	// Blank profile.
	for (unsigned int i = 0; i < 9; ++i)
		pPacket >> (char)0;

	return true;
}

bool setProfile( const CString& pAccountName, CString &pPacket)
{
#ifdef NO_MYSQL
	return true;
#else
	CString query;
	std::vector<CString> items;

	for (int i = 0; i < 9; i++)
		items.push_back( pPacket.readChars(pPacket.readGUChar()) );

	if (items.size() <= 8)
		return false;

	query << "UPDATE " << settings->getStr("userlist") << " SET"
	<< " profile_name='"	<< items[0].escape() << "',"
	<< " profile_age='"		<< items[1].escape() << "',"
	<< " profile_sex='"		<< items[2].escape() << "',"
	<< " profile_country='"	<< items[3].escape() << "',"
	<< " profile_icq='"		<< items[4].escape() << "',"
	<< " profile_email='"	<< items[5].escape() << "',"
	<< " profile_url='"		<< items[6].escape() << "',"
	<< " profile_hangout='"	<< items[7].escape() << "',"
	<< " profile_quote='"	<< items[8].escape() << "'"
	<< " WHERE account='"	<< pAccountName.escape() << "' LIMIT 1";
	mySQL->query(query);

	return true;
#endif
}
