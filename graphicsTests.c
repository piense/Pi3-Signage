#include <sys/resource.h>
#include <stdio.h>

#include "graphicsTests.h"
#include "compositor/pijpegdecoder.h"
#include "compositor/piresizer.h"
#include "compositor/compositor.h"
#include "dispmanx.h"
#include "compositor/tricks.h"

void printGPUMemory()
{
	char response[200];
	/*
	vc_gencmd(&response[0],200,"get_mem malloc_total");
	printf("%s ",response);
	vc_gencmd(&response[0],200,"get_mem malloc");
	printf("%s ",response);
	vc_gencmd(&response[0],200,"get_mem reloc_total");
	printf("%s ",response);*/
	//Seems to be the important one
	vc_gencmd(&response[0],200,"get_mem reloc");
	printf("%s\n",response);
}

void printCPUMemory()
{
	  struct rusage r_usage;
	  getrusage(RUSAGE_SELF,&r_usage);
	  printf("Memory usage: %f megabytes\n",((float)r_usage.ru_maxrss)/1000.0);
}

float peakCPUMemoryUsage;

void printCPUandGPUMemory()
{
	  struct rusage r_usage;
	  getrusage(RUSAGE_SELF,&r_usage);
	  char response[200];
	  vc_gencmd(&response[0],200,"get_mem reloc");

	  if(r_usage.ru_maxrss/1000 > peakCPUMemoryUsage)
	  {
		  peakCPUMemoryUsage = ((float)r_usage.ru_maxrss)/1000.0;
	  }

	  printf("Memory: CPU: %f megabytes, CPU Peak: %f, GPU: %s\n",((float)r_usage.ru_maxrss)/1000.0,peakCPUMemoryUsage,response);
}

void dispmanXResourceTest()
{
	uint32_t resource[100];
	uint32_t nativeHandle[100];

	bcm_host_init();

	for(int i = 0;i<1000;i++)
	{
		printf("Attempt %d\n",i);
		printGPUMemory();
		for(int x = 0;x<5;x++){
		resource[x] = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
									1920,1080,
									&nativeHandle[x]);
		}

		printGPUMemory();

		for(int x = 0;x<5;x++){
			vc_dispmanx_resource_delete(resource[x]);
		}

	}
}


void decoderTest()
{
	char* filename = "/mnt/data/images/small.jpg";
	sImage *ret1;

  struct rusage r_usage;
  getrusage(RUSAGE_SELF,&r_usage);
  printf("Memory usage: %ld bytes\n",r_usage.ru_maxrss/1000);

	for(int i = 0;i<500;i++)
	{
		printGPUMemory();
		getrusage(RUSAGE_SELF,&r_usage);
		printf("Memory usage: %f megabytes\n",((float)r_usage.ru_maxrss)/1000.0);

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
  printf("Memory usage: %f bytes\n",((float)r_usage.ru_maxrss)/1000.0);

	for(int i = 0;i<500;i++)
	{
		printGPUMemory();

		getrusage(RUSAGE_SELF,&r_usage);
		  printf("Memory usage: %f megabytes\n",((float)r_usage.ru_maxrss)/1000.0);

		printf("attempt: %d\n",i);

		ret1 = resizeImage2((char*)testImage,
				1270, 847,
				1628160, 20,
				1280,848,
				1920,1080,0,1);

		free(ret1->imageBuf);

		free (ret1);
	}

	free(testImage);
}


