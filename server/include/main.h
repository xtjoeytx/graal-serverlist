#ifndef MAIN_H
#define MAIN_H

#include "CString.h"

bool parseArgs(int argc, char* argv[]);
void printHelp(const char* pname);
std::string getBaseHomePath();
void shutdownServer(int signal);
const char * getErrorString(InitializeError error);

#endif // MAIN_H
