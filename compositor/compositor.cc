//System headers
#include <stdio.h>
#include <dirent.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>

extern "C"
{
//External libs
#include "bcm_host.h"
#include "vgfont/vgfont.h"
}

//Project includes
#include "compositor.h"
#include "pijpegdecoder.h"
#include "piImageResizer.h"
#include "tricks.h"
#include "fontTest.h"
#include "../graphicsTests.h"
#include "../PiSignageLogging.h"

pis_compositor_s pis_compositor;

pis_compositorErrors pis_addNewSlide(
		pis_slides_s **ret, uint32_t dissolveTime, uint32_t duration, const char *name)
{

	pis_slides_s *newSlide = new pis_slides_s;
	newSlide->dissolve = dissolveTime;
	newSlide->duration = duration;
	newSlide->mediaElementsHead = NULL;
	newSlide->next = NULL;
	//TODO: Something with the name

	if(pis_compositor.slides == NULL)
	{
		pis_compositor.slides = newSlide;
	}else{
		pis_slides_s *current = pis_compositor.slides;
		while(current->next != NULL)
			current = current->next;
		current->next = newSlide;
	}

	*ret = newSlide;

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_AddTextToSlide(pis_slides_s *slide, const char* text, const char* fontName,
		float height, float x, float y, uint32_t color)
{
	pis_mediaElementList_s *newMedia = new pis_mediaElementList_s;
	newMedia->mediaElement.data = malloc(sizeof(pis_mediaText));
	newMedia->mediaElement.mediaType = pis_MEDIA_TEXT;
	pis_mediaText *data = ((pis_mediaText *)newMedia->mediaElement.data);

	char *tempTxt = new char[strlen(text)+1];
	strcpy(tempTxt,text);
	data->text = tempTxt;

	tempTxt =  new char[strlen(fontName)+1];
	strcpy(tempTxt,fontName);
	data->fontName = tempTxt;

	data->fontHeight = height;
	data->x = x;
	data->y = y;
	data->color = color;
	data->res = 0;
	data->element = 0;

	newMedia->next = slide->mediaElementsHead;
	newMedia->mediaElement.name = "Text";
	slide->mediaElementsHead = newMedia;
	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_AddImageToSlide(pis_slides_s *slide, char* file,
		float x, float y, float width, float height, pis_mediaSizing sizing)
{
	pis_mediaElementList_s *newMedia = new pis_mediaElementList_s;
	newMedia->mediaElement.data = malloc(sizeof(pis_mediaImage));
	newMedia->mediaElement.mediaType = pis_MEDIA_IMAGE;

	pis_mediaImage* data = ((pis_mediaImage *)newMedia->mediaElement.data);

	//TODO range check x,y,width,height

	char *lfile = new char[strlen(file)+1];
	strcpy(&lfile[0],&file[0]);

	data->filename = lfile;
	data->maxHeight = height;
	data->maxWidth = width;
	data->x = x;
	data->y = y;
	data->sizing = sizing;

	data->cache.img = NULL;
	data->cache.width = 0;
	data->cache.height = 0;
	data->res = 0;
	data->element = 0;

	newMedia->next = slide->mediaElementsHead;
	newMedia->mediaElement.name = "Image";
	slide->mediaElementsHead = newMedia;
	return pis_COMPOSITOR_ERROR_NONE;
}

int pis_loadDirectory(const char* directory)
{
	DIR           *d;
	struct dirent *dir;
	d = opendir(directory);

	if(d == NULL){
		printf("Error opening images directory.\n");
	}

	pis_slides_s *newSlide;

	char name[1000];
	char fullpath[1000];
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if(strstr(dir->d_name, ".jpg") != NULL ||strstr(dir->d_name, ".jpeg") != NULL ) {

				sprintf(&name[0],"%s",dir->d_name);
				sprintf(&fullpath[0],"%s/%s",directory,dir->d_name);

				printf("\nAdding: %s %s\n",name,fullpath);


				pis_addNewSlide(&newSlide, 1000,7000,"Image Slide");
				pis_AddTextToSlide(newSlide,&name[0],"",30.0/1080.0,.5,.95,0xFFFFFFFF);

				pis_AddImageToSlide(newSlide,&fullpath[0],
						.5,.5, //position [0,1]
						1,1 //size [0,1]
						,pis_SIZE_CROP);
			}

		}
	closedir(d);
	}

	return 0;
}

pis_compositorErrors pis_initializeCompositor()
{
	bcm_host_init();

//	printFontList();

	//Demo slides
	pis_loadDirectory("/mnt/data/images");

	pis_compositor.currentSlide = pis_compositor.slides;

	pis_compositor.numOfRenderers = 3;
	pis_compositor.slideRenderers = new PiSlideRenderer[pis_compositor.numOfRenderers]();

	for(int i = 0;i<pis_compositor.numOfRenderers;i++)
	{
		pis_compositor.slideRenderers[i].setDispmanxLayer(i);
	}

	pis_compositor.currentSlideRenderer = 0;

	pis_compositor.pbState = PB_LOADING_NEXT;

	return pis_COMPOSITOR_ERROR_NONE;
}

int slideRenderersGetIdle(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "slideRenderersGetIdle()\n");

	for(int i = 0;i<pis_compositor.numOfRenderers;i++)
	{
		if(pis_compositor.slideRenderers[i].state == SLIDE_STATE_IDLE)
			return i;
	}

	pis_logMessage(PIS_LOGLEVEL_ERROR, "slideRenderersGetIdle: No idle slide renderers.\n");
	return -1;
}

void slideRenderersCleanup(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "slideRenderersGetIdle()\n");

	for(int i = 0;i<pis_compositor.numOfRenderers;i++)
	{
		if(pis_compositor.slideRenderers[i].state == SLIDE_STATE_DONE){
			pis_compositor.slideRenderers[i].removeSlideFromDisplay();
			pis_compositor.slideRenderers[i].freeSlideResources();
			pis_compositor.slideRenderers[i].slide = NULL;
		}
	}
}

bool slideRenderersAppearing(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"slideRenderesAppearing()\n");
	for(int i = 0;i<pis_compositor.numOfRenderers;i++)
	{
		if(pis_compositor.slideRenderers[i].state == SLIDE_STATE_APPEARING){
			return true;
		}
	}

	return false;
}

void slideRenderersResort(int topRenderer){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"slideRenderesAppearing()\n");
	int32_t highestLayer = 0;
	int32_t lowestLayer = 60000;

	for(int i = 0;i<pis_compositor.numOfRenderers;i++)
	{
		if(pis_compositor.slideRenderers[i].dispmanxLayer > highestLayer)
			highestLayer = pis_compositor.slideRenderers[i].dispmanxLayer;
		if(pis_compositor.slideRenderers[i].dispmanxLayer < lowestLayer)
					lowestLayer = pis_compositor.slideRenderers[i].dispmanxLayer;

	}

	pis_compositor.slideRenderers[topRenderer].dispmanxLayer = highestLayer + 1;

	for(int i = 0;i<pis_compositor.numOfRenderers;i++)
	{
		pis_compositor.slideRenderers[i].dispmanxLayer -= lowestLayer - 5;
	}
}

pis_compositorErrors pis_compositorcleanup()
{


	return pis_COMPOSITOR_ERROR_NONE;
}

double msLastPrint;

pis_compositorErrors pis_doCompositor()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_doCompositor()\n");

	DISPMANX_UPDATE_HANDLE_T    update;

	pis_logMessage(PIS_LOGLEVEL_ALL,"pis_compositor.pbState: %d\n",pis_compositor.pbState);

	double ms = linuxTimeInMs();

	if(ms - msLastPrint > 1000)
	{
		printCPUandGPUMemory();
		msLastPrint = ms;
	}

	if(pis_compositor.pbState == PB_IDLE ||
			pis_compositor.pbState == PB_LOADING_NEXT ||
			pis_compositor.pbState == PB_WAITING_FOR_EXPIRED)
		slideRenderersCleanup();

	//This could be in another thread eventually
	switch(pis_compositor.pbState){
		case PB_IDLE: break; //Doing nothing waiting for a new slide to appear
		case PB_DISSOLVING:
			if(pis_compositor.slideRenderers[pis_compositor.currentSlideRenderer].state !=
					SLIDE_STATE_APPEARING)
				pis_compositor.pbState = PB_LOADING_NEXT;
			break;
		case PB_LOADING_NEXT:

			pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Loading Next.\n");

			for(int i = 0;i<pis_compositor.numOfRenderers;i++){
				if(pis_compositor.slideRenderers[i].state == SLIDE_STATE_EXPIRED && i != pis_compositor.currentSlideRenderer)
					pis_compositor.slideRenderers[i].state = SLIDE_STATE_DISSAPPEARING;
			}

			if(pis_compositor.currentSlide == NULL)
			{
				pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: No currentSlide\n");
				if(pis_compositor.slides != NULL){
					pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Starting the list again.\n");
					pis_compositor.nextSlide = pis_compositor.slides;
				}else{
					pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: No slides to use.\n");
					pis_compositor.nextSlide = NULL;
				}
			}else
			if(pis_compositor.currentSlide != NULL && pis_compositor.currentSlide->next != NULL)
			{
				pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Going to next slide in the list.\n");
				pis_compositor.nextSlide = pis_compositor.currentSlide->next;
			}else
			if(pis_compositor.currentSlide != NULL && pis_compositor.currentSlide->next == NULL)
			{
				if(pis_compositor.slides != NULL){
					pis_compositor.nextSlide = pis_compositor.slides;
					pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Resetting to top of the stack.\n");
				}else{
					pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Reached end of show.\n");
					pis_compositor.nextSlide = NULL;
				}
			}

			if(pis_compositor.nextSlide != NULL)
			{
				pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Loading up the next slide\n");
				int next = slideRenderersGetIdle();
				if(next != -1)
				{
					pis_compositor.nextSlideRenderer = next;
					pis_compositor.slideRenderers[next].slide = pis_compositor.nextSlide;
					pis_compositor.slideRenderers[next].loadSlideResources();
					pis_compositor.pbState = PB_WAITING_FOR_EXPIRED;
				}
			}else{
				pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: nextSlide is NULL\n");
			}

			slideRenderersResort(pis_compositor.nextSlideRenderer);

			break;
		case PB_WAITING_FOR_EXPIRED:
			if(pis_compositor.currentSlide == NULL ||
					pis_compositor.slideRenderers[pis_compositor.currentSlideRenderer].state != SLIDE_STATE_LIVE
					)
			{
				pis_compositor.currentSlide = pis_compositor.nextSlide;
				pis_compositor.currentSlideRenderer = pis_compositor.nextSlideRenderer;
				pis_compositor.slideRenderers[pis_compositor.nextSlideRenderer].startSlide();

				pis_compositor.pbState = PB_DISSOLVING;
			}
			break;
	}

	update = vc_dispmanx_update_start(10);

	for(int i = 0;i<pis_compositor.numOfRenderers;i++)
	{
		pis_compositor.slideRenderers[i].doUpdate(update);
	}

	vc_dispmanx_update_submit_sync( update );

	return pis_COMPOSITOR_ERROR_NONE;
}
