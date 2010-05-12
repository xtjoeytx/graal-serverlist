#ifndef FPROFILE_H
#define FPROFILE_H

#include "CString.h"

bool getProfile( const CString& pAccountName, CString &pPacket);
bool setProfile( const CString& pAccountName, CString &pPacket);

#endif
