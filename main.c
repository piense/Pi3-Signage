#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <sys/resource.h>


#include "compositor/compositor.h"
#include "dispmanx.h"
#include "compositor/tricks.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <net/if.h>

#include <math.h>

/*
void decoderTest()
{
	char* filename = "/mnt/data/images/small.jpg";
	sImage *ret1;

  struct rusage r_usage;
  getrusage(RUSAGE_SELF,&r_usage);
  printf("Memory usage: %ld bytes\n",r_usage.ru_maxrss/1000);

	for(int i = 0;i<500;i++)
	{
		getrusage(RUSAGE_SELF,&r_usage);
		  printf("Memory usage: %ld megabytes\n",r_usage.ru_maxrss/1000);

		printf("attempt: %d\n",i);

		ret1 = decodeJpgImage(filename);

		free(ret1->imageBuf);

		free (ret1);
	}
}

void resizerTest()
{
	uint32_t *testImage = malloc(1628160);

	sImage *ret1;

  struct rusage r_usage;
  getrusage(RUSAGE_SELF,&r_usage);
  printf("Memory usage: %ld bytes\n",r_usage.ru_maxrss/1000);

	for(int i = 0;i<500;i++)
	{
		getrusage(RUSAGE_SELF,&r_usage);
		  printf("Memory usage: %f megabytes\n",((float)r_usage.ru_maxrss)/1000.0);

		printf("attempt: %d\n",i);

		ret1 = resizeImage2(testImage,
				1270, 847,
				1628160, 20,
				1280,848,
				1920,1080,0,1);

		free(ret1->imageBuf);

		free (ret1);
	}

	free(testImage);
}*/

int main(int argc, char *argv[])
{

	//TODO: Catch Ctrl+C for cleanup

	pis_initializeCompositor();

	while(1)
	{
		pis_doCompositor();
	}

	pis_compositorcleanup();

	return 0;
}

