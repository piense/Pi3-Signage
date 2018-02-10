#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "compositor/compositor.h"
#include "dispmanx.h"
#include "compositor/tricks.h"
#include "graphicsTests.h"
#include "PiSignageLogging.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <net/if.h>

#include <math.h>

int main(int argc, char *argv[])
{

	//decoderTest();
	//resizerTest();
	//dispmanXResourceTest();

	pis_loggingLevel = PIS_LOGLEVEL_ALL;

	//TODO: Catch Ctrl+C for cleanup

	pis_initializeCompositor();

	while(1)
	{
		pis_doCompositor();
	}

	pis_compositorcleanup();

	return 0;
}

