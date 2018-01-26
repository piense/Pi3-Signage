#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "compositor/compositor.h"


#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <net/if.h>

#include <math.h>

int main(int argc, char *argv[])
{
	pis_compositorErrors pis_initializeCompositor();

	while(1)
	{
	pis_compositorErrors pis_doCompositor();
	}

	pis_compositorErrors pis_cleanup();

	return 0;
}

