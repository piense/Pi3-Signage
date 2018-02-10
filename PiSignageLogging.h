#pragma once

#include "compositor/ilclient/ilclient.h"

//-3 = All the things
//-2 = Function Headers
//-1 = Info
//0 = Errors & Warnings only
//1 = Errors Only
//2 = Nada

#define PIS_LOGLEVEL_ALL -3
#define PIS_LOGLEVEL_FUNCTION_HEADER -2
#define PIS_LOGLEVEL_INFO -1
#define PIS_LOGLEVEL_WARNING 0
#define PIS_LOGLEVEL_ERROR 1

extern int pis_loggingLevel;

void pis_logMessage(int level, const char *fmt, ...);
char *OMX_errString(int err);
void printOMXdebug();
void printOMXPort(OMX_HANDLETYPE componentHandle, OMX_U32 portno);
