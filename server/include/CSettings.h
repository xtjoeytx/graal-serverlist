#ifndef CSETTINGS_H
#define CSETTINGS_H

#include <vector>
#include "CString.h"

struct CKey
{
	CKey(const CString& pName, const CString& pValue)
	{
		name  = pName;
		value = pValue;
	}

	CString name;
	CString value;
};

class CSettings
{
	public:
		// Constructor-Destructor
		CSettings(const CString& pStr);
		~CSettings();

		// File-Loading Functions
		inline bool isOpened();
		bool loadFile(const CString& pStr);
		void clear();

		// Get Type
		CKey *getKey(CString pStr);
		bool getBool(const CString& pStr, bool pDefault = true);
		float getFloat(const CString& pStr, float pDefault = 1.00);
		int getInt(const CString& pStr, int pDefault = 1);
		const CString& getStr(const CString& pStr, const CString& pDefault = "");

	private:
		bool opened;
		std::vector<CKey *> keys;
};

inline bool CSettings::isOpened()
{
	return opened;
}

#endif // CSETTINGS_H
