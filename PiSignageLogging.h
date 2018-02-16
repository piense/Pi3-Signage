#pragma once

#include "compositor/ilclient/ilclient.h"

//-3 = All the things
//-2 = Function Headers
//-1 = Info
//0 = Errors & Warnings only
//1 = Errors Only
//2 = Nada

enum PisLogLevels
{
	PIS_LOGLEVEL_UNUSED,
	PIS_LOGLEVEL_ALL,
	PIS_LOGLEVEL_FUNCTION_HEADER,
	PIS_LOGLEVEL_INFO,
	PIS_LOGLEVEL_WARN,
	PIS_LOGLEVEL_ERROR
};



extern PisLogLevels pis_loggingLevel;

void pis_logMessage(PisLogLevels level, const char *fmt, ...);
const char *OMX_errString(int err);
void printOMXdebug();
void printOMXPort(OMX_HANDLETYPE componentHandle, OMX_U32 portno);
