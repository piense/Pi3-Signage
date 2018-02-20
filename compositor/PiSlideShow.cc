//System headers
#include <stdio.h>
#include <dirent.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string>

extern "C"
{
//External libs
#include "bcm_host.h"
}

//Project includes
#include "PiSlideShow.h"
#include "tricks.h"
#include "fontTest.h"
#include "../graphicsTests.h"
#include "../PiSignageLogging.h"
#include "../PiSlide.h"
#include "../mediatypes/PiMediaImage.h"
#include "../mediatypes/PiMediaText.h"

using namespace std;

int pis_SlideShow::AddNewSlide(pis_Slide ** newSlide)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_SlideShow::addNewSlide()\n");

	*newSlide = new pis_Slide();

	if(*newSlide == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_SlideShow::addNewSlide Error allocating new slide.\n");
		return -1;
	}

	Slides.push_back(*newSlide);

	return 0;
}

int pis_SlideShow::LoadDirectory(string directory)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_SlideShow::LoadDirectory(%s)\n");

	DIR           *d;
	struct dirent *dir;
	d = opendir(directory.c_str());

	if(d == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_SlideShow::LoadDirectory(): sError opening images directory.\n");
		return -1;
	}

	char name[1000];
	char fullpath[1000];
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if(strstr(dir->d_name, ".jpg") != NULL || strstr(dir->d_name, ".jpeg") != NULL ) {

				sprintf(&name[0],"%s",dir->d_name);
				sprintf(&fullpath[0],"%s/%s",directory.c_str(),dir->d_name);

				pis_logMessage(PIS_LOGLEVEL_INFO,"pis_SlideShow::LoadDirectory adding: %s\n",name);

				pis_Slide *newSlide;
				pis_MediaItem *newMedia;

				if(AddNewSlide(&newSlide) != 0){
					pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_SlideShow::LoadDirectory: Failed to get new slide\n");
					continue;
				}

				newSlide->SetTransition(PictureDissolveTime, PictureHoldTime);

				pis_MediaImage::NewImage(
						&fullpath[0],
						.5,.5, //position [0,1]
						1, 1 //size [0,1]
						,pis_SIZE_CROP, &newMedia);

				newSlide->MediaElements.push_back(newMedia);

				if(PictureTitles){
					pis_MediaText::NewText(
					&name[0],.5,.95,1,1,30.0/1080.0,
					"",0xFFFFFFFF, &newMedia);
					newSlide->MediaElements.push_back(newMedia);
				}
			}
		}
	closedir(d);
	}

	pis_Slide::ToXMLFile(&Slides, "/mnt/data/SlideShow.xml");

	return 0;
}

pis_SlideShow::pis_SlideShow()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_SlideShow::pis_SlideShow()\n");

	bcm_host_init();

	CurrentSlide = Slides.begin();

	NumOfRenderers = 3;
	SlideRenderers = new PiSlideRenderer[NumOfRenderers]();

	for(int i = 0;i<NumOfRenderers;i++)
	{
		SlideRenderers[i].setDispmanxLayer(i);
	}

	CurrentSlideRenderer = 0;
	NextSlideRenderer = 0;
	PBState = PB_LOADING_NEXT;
	LastMemPrint = 0;
	PictureTitles = false;
	PictureDissolveTime = 1000;
	PictureHoldTime = 10000;
}

pis_SlideShow::~pis_SlideShow()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "~pis_SlideShow()\n");

	for(int i = 0;i<NumOfRenderers;i++)
	{
		SlideRenderers[i].freeSlideResources();
		SlideRenderers[i].slide = NULL;
		delete [] SlideRenderers;
		SlideRenderers = NULL;
	}

	for(list<pis_Slide *>::iterator it = Slides.begin(); it != Slides.end(); ++it){
		delete *it;
	}

	Slides.clear();
}


int pis_SlideShow::slideRenderersGetIdleRenderer(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_SlideShow::slideRenderersGetIdleRenderer()\n");

	for(int i = 0;i<NumOfRenderers;i++)
	{
		if(SlideRenderers[i].state == SLIDE_STATE_IDLE)
			return i;
	}

	pis_logMessage(PIS_LOGLEVEL_ERROR, "slideRenderersGetIdle: No idle slide renderers.\n");
	return -1;
}



bool pis_SlideShow::slideRenderersAppearing(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_SlideShow::slideRenderesAppearing()\n");

	for(int i = 0;i<NumOfRenderers;i++)
	{
		if(SlideRenderers[i].state == SLIDE_STATE_APPEARING){
			return true;
		}
	}

	return false;
}

//Might be able to find a more wordy function name
void pis_SlideShow::slideRenderersResortDispmanXLayers(int topRenderer){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_SlideShow::slideRenderersResortDispmanXLayers()\n");
	int32_t highestLayer = 0;
	int32_t lowestLayer = 60000;

	for(int i = 0;i<NumOfRenderers;i++)
	{
		if(SlideRenderers[i].dispmanxLayer > highestLayer)
			highestLayer = SlideRenderers[i].dispmanxLayer;
		if(SlideRenderers[i].dispmanxLayer < lowestLayer)
					lowestLayer = SlideRenderers[i].dispmanxLayer;

	}

	SlideRenderers[topRenderer].dispmanxLayer = highestLayer + 1;

	for(int i = 0;i<NumOfRenderers;i++)
	{
		SlideRenderers[i].dispmanxLayer -= lowestLayer - 5;
	}
}

void pis_SlideShow::SlideRenderersCleanup(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "slideRenderersGetIdle()\n");

	for(int i = 0;i<NumOfRenderers;i++)
	{
		if(SlideRenderers[i].state == SLIDE_STATE_DONE){
			SlideRenderers[i].freeSlideResources();
			SlideRenderers[i].slide = NULL;
		}
	}
}

void pis_SlideShow::DoSlideShow()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_SlideShow::DoSlideShow()\n");

	DISPMANX_UPDATE_HANDLE_T    update;

	pis_logMessage(PIS_LOGLEVEL_ALL,"pis_compositor.pbState: %d\n",PBState);

	double ms = linuxTimeInMs();

	if(ms - LastMemPrint > 1000)
	{
		printCPUandGPUMemory();
		LastMemPrint = ms;
	}

	if(		PBState == PB_IDLE ||
			PBState == PB_LOADING_NEXT ||
			PBState == PB_WAITING_FOR_EXPIRED)
		SlideRenderersCleanup();

	switch(PBState){
		case PB_IDLE: break; //Doing nothing waiting for a new slide to appear
		case PB_DISSOLVING:
			if(SlideRenderers[CurrentSlideRenderer].state !=
					SLIDE_STATE_APPEARING)
				PBState = PB_LOADING_NEXT;
			break;
		case PB_LOADING_NEXT:

			pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Loading Next.\n");

			for(int i = 0;i<NumOfRenderers;i++){
				if(SlideRenderers[i].state == SLIDE_STATE_EXPIRED && i != CurrentSlideRenderer)
					SlideRenderers[i].state = SLIDE_STATE_DISSAPPEARING;
			}

			if(CurrentSlide == Slides.end())
			{
				pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: No currentSlide\n");
				if(!Slides.empty()){
					pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Starting the list again.\n");
					NextSlide = Slides.begin();
				}else{
					pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: No slides to use.\n");
					NextSlide = Slides.end();
				}
			}else
			if(CurrentSlide != Slides.end()	&& next(CurrentSlide) != Slides.end()
					)
			{
				pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Going to next slide in the list.\n");
				NextSlide = next(CurrentSlide);
			}else
			if(CurrentSlide != Slides.end()	&& next(CurrentSlide) == Slides.end())
			{
				if(!Slides.empty()){
					NextSlide = Slides.begin();
					pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Resetting to top of the stack.\n");
				}else{
					pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Reached end of show.\n");
					NextSlide = Slides.end();
				}
			}

			if(NextSlide != Slides.end())
			{
				pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Loading up the next slide\n");
				int next = slideRenderersGetIdleRenderer();
				if(next != -1)
				{
					NextSlideRenderer = next;
					SlideRenderers[next].slide = *NextSlide;
					SlideRenderers[next].loadSlideResources();
					PBState = PB_WAITING_FOR_EXPIRED;
				}
			}else{
				pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: nextSlide is NULL\n");
			}

			slideRenderersResortDispmanXLayers(NextSlideRenderer);

			break;
		case PB_WAITING_FOR_EXPIRED:
			if(CurrentSlide == Slides.end() || SlideRenderers[CurrentSlideRenderer].state != SLIDE_STATE_LIVE)
			{
				CurrentSlide = NextSlide;
				CurrentSlideRenderer = NextSlideRenderer;
				SlideRenderers[NextSlideRenderer].startSlide();

				PBState = PB_DISSOLVING;
			}
			break;
	}

	update = vc_dispmanx_update_start(10);

	for(int i = 0;i<NumOfRenderers;i++)
	{
		SlideRenderers[i].doUpdate(update);
	}

	vc_dispmanx_update_submit_sync( update );
}
