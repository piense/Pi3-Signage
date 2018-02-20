#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "compositor/tricks.h"
#include "graphicsTests.h"
#include "PiSignageLogging.h"
#include "mediatypes/PiMediaImage.h"
#include "mediatypes/PiMediaItem.h"

#include <signal.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <net/if.h>

#include <math.h>
#include "compositor/PiSlideShow.h"

using namespace std;

int SignageExit = 0;

void stop(int s){
           printf("Caught signal %d\n",s);
           SignageExit = 1;
}

pis_MediaItemManager *MediaItems;

int main(int argc, char *argv[])
{
	MediaItems = &pis_MediaItemManager::instance();
   struct sigaction sigIntHandler;

   sigIntHandler.sa_handler = stop;
   sigemptyset(&sigIntHandler.sa_mask);
   sigIntHandler.sa_flags = 0;

   sigaction(SIGINT, &sigIntHandler, NULL);

   list<pis_Slide*> *slides;

   pis_Slide::FromXMLFile("/mnt/data/test.xml",&slides);

	//decoderTest();
	//resizerTest();
	//dispmanXResourceTest();

	pis_loggingLevel = PIS_LOGLEVEL_INFO;

   for(auto it = MediaItems->availableMediaTypes.begin();it!=MediaItems->availableMediaTypes.end();++it  )
   {
	   pis_logMessage(PIS_LOGLEVEL_INFO,"Loaded media type: %s\n",it->name);
   }

	//TODO: Catch Ctrl+C for cleanup

	pis_SlideShow slideshow;

	//slideshow.PictureTitles = true;
	//slideshow.LoadDirectory("/mnt/data/images");

	slideshow.Slides = *slides;

	while(!SignageExit)
	{
		slideshow.DoSlideShow();
	}

	return 0;
}

