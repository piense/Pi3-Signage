#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "compositor/compositor.h"
#include "compositor/tricks.h"
#include "graphicsTests.h"
#include "PiSignageLogging.h"

#include <signal.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <net/if.h>

#include <math.h>

int SignageExit = 0;

void stop(int s){
           printf("Caught signal %d\n",s);
           SignageExit = 1;
}


int main(int argc, char *argv[])
{

   struct sigaction sigIntHandler;

   sigIntHandler.sa_handler = stop;
   sigemptyset(&sigIntHandler.sa_mask);
   sigIntHandler.sa_flags = 0;

   sigaction(SIGINT, &sigIntHandler, NULL);

	//decoderTest();
	//resizerTest();
	//dispmanXResourceTest();

	pis_loggingLevel = PIS_LOGLEVEL_INFO;

	//TODO: Catch Ctrl+C for cleanup

	pis_initializeCompositor();

	while(!SignageExit)
	{
		pis_doCompositor();
	}

	pis_compositorcleanup();



	return 0;
}

