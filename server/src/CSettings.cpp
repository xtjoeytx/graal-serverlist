#include "CSettings.h"

/*
	Constructor - Deconstructor
*/
CSettings::CSettings(const CString& pStr)
{
	opened = loadFile(pStr);
}

CSettings::~CSettings()
{
	clear();
}

/*
	File-Loading Functions
*/
bool CSettings::loadFile(const CString& pStr)
{
	// definitions
	CString fileData;

	// Clear Keys
	clear();

	// Load File
	if (!fileData.load(pStr))
		return false;

	// Parse Data
	std::vector<CString> strList = fileData.tokenize("\n");
	for (unsigned int i = 0; i < strList.size(); i++)
	{
		// Skip Comments
		if (strList[i][0] == '#' || strList[i].find("=") == -1)
			continue;

		// Tokenize Line && Trim
		std::vector<CString> line = strList[i].tokenize("=");
		for (unsigned int j = 0; j < line.size(); j++)
			line[j].trimI();

		// Fix problem involving settings with an = in the value.
		if (line.size() > 2)
		{
			for (unsigned int j = 2; j < line.size(); ++j)
				line[1] << "=" << line[j];
		}

		// Create Key
		keys.push_back(new CKey(line[0].toLowerI(), line[1]/*.toLowerI()*/));
	}

	return true;
}

void CSettings::clear()
{
	// Clear Keys
	for (unsigned int i = 0; i < keys.size(); i++)
		delete keys[i];
	keys.clear();
}

/*
	Get Settings
*/
CKey * CSettings::getKey(CString pStr)
{
	// Lowercase Name
	pStr.toLowerI();

	// Search for Name
	for (unsigned int i = 0; i < keys.size(); i++)
	{
		if (keys[i]->name == pStr)
			return keys[i];
	}

	// None :(
	return NULL;
}

bool CSettings::getBool(const CString& pStr, bool pDefault)
{
	CKey *key = getKey(pStr);
	return (key == NULL ? pDefault : (key->value == "true" || key->value == "1"));
}

float CSettings::getFloat(const CString& pStr, float pDefault)
{
	CKey *key = getKey(pStr);
	return (key == NULL ? pDefault : (float)strtofloat(key->value));
}

int CSettings::getInt(const CString& pStr, int pDefault)
{
	CKey *key = getKey(pStr);
	return (key == NULL ? pDefault : strtoint(key->value));
}

const CString& CSettings::getStr(const CString& pStr, const CString& pDefault)
{
	CKey *key = getKey(pStr);
	return (key == NULL ? pDefault : key->value);
}

