#include "piSlideRenderer.h"

#include <stdint.h>
#include <stdio.h>
#include <list>

#include "tricks.h"
#include "pijpegdecoder.h"
#include "piImageResizer.h"
#include "fontTest.h"
#include "mediatypes/PiMediaItem.h"

using namespace std;

//Gets info on the display, sets up offscreen drawing buffer, and adds a window to the main display
//initializes other things too
PiSlideRenderer::PiSlideRenderer()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::PiSlideRenderer()\n");

    mainDisplay = vc_dispmanx_display_open( 0 );

    if(mainDisplay == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"PiSlideRenderer: Error, unable to open main display\n");
    	//TODO Fail harder
    	//return;
    	//DON'T RETURN: FLAG ERROR AND INITIALIZE OTHER VARIABLES
    }else
    	pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer: main display opened.\n");


    int ret = vc_dispmanx_display_get_info( mainDisplay, &mainDisplayInfo);
    if(ret != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"PiSlideRenderer: Error, unable to get info from main display\n");
    	//TODO Fail harder
    	//return;
    	//DON'T RETURN: FLAG ERROR AND INITIALIZE OTHER VARIABLES
    }else
    	pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer: Main display is %dx%d\n",
    			mainDisplayInfo.width, mainDisplayInfo.height );

    vc_dispmanx_rect_set(&screenRect,0,0,mainDisplayInfo.width,mainDisplayInfo.height);
    vc_dispmanx_rect_set(&srcRect,0,0,mainDisplayInfo.width<<16,mainDisplayInfo.height<<16);

    offscreenDispResourceHandle = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
    		(int32_t)mainDisplayInfo.width, (int32_t)mainDisplayInfo.height,
			&offscreenDispBufferHandle);

    if(offscreenDispResourceHandle == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer: Unable to allocate off screen resource.\n");
    	//TODO: Cleanup and damage control
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"PiSlideRenderer: Off screen display resource created.\n");
    }

    offscreenDisplay = vc_dispmanx_display_open_offscreen(offscreenDispResourceHandle,DISPMANX_NO_ROTATE);

    if(offscreenDisplay == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer: Unable to open display off screen.\n");
    	//TODO: Cleanup and damage control
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"PiSlideRenderer: Off screen display opened.\n");
    }

    alpha.flags = (DISPMANX_FLAGS_ALPHA_T)(DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX);
    alpha.opacity = 0; //alpha 0->255
    alpha.mask = 0;

    uint32_t noHandle;
    DISPMANX_RESOURCE_HANDLE_T blackRes = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
    		(int32_t)mainDisplayInfo.width, (int32_t)mainDisplayInfo.height,
			&noHandle);

    if(offscreenDispResourceHandle == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer: Unable to allocate off screen black resource.\n");
    	//TODO: Cleanup and damage control
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"PiSlideRenderer: Off screen display black resource created.\n");
    }

    uint32_t stride = ALIGN_UP(mainDisplayInfo.width,16);
    uint32_t *black = new uint32_t[stride*mainDisplayInfo.height];

    for(int x = 0;x<mainDisplayInfo.width;x++){
    	for(int y = 0;y<mainDisplayInfo.height;y++){
    		black[x+y*stride] = 0xFF<<24;
    	}
    }

	ret = vc_dispmanx_resource_write_data(blackRes,
			VC_IMAGE_ARGB8888, stride*4,black,
			&screenRect
			);

	if(ret != 0)
		pis_logMessage(PIS_LOGLEVEL_ERROR,"PiSlideRenderer: Unable to copy black slide to resource\n");

	delete [] black;

    uint32_t update = vc_dispmanx_update_start(10);

	mainDisplayElementHandle = vc_dispmanx_element_add(update,
			mainDisplay,
			0,
			&screenRect,
			offscreenDispResourceHandle,
			&srcRect,
			DISPMANX_PROTECTION_NONE,
			&alpha,
			NULL,
			DISPMANX_NO_ROTATE
			);

    if(mainDisplayElementHandle == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer: Unable to add slide element to main display.\n");
    	//TODO: Cleanup and damage control
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"PiSlideRenderer: Added slide element to .\n");
    }

	alpha.opacity = 255;

    //Create a solid black background for the slide
	vc_dispmanx_element_add(update,
			offscreenDisplay,
		5,
		&screenRect,
		blackRes,
		&srcRect,
		DISPMANX_PROTECTION_NONE,
		&alpha,
		NULL,
		DISPMANX_NO_ROTATE
		);

	vc_dispmanx_update_submit_sync( update );


	slideStartTime = 0;
	dispmanxLayer = 0;
	slide = NULL;
	state = SLIDE_STATE_IDLE;
	opacity = 0;
}

PiSlideRenderer::~PiSlideRenderer()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::~PiSlideRenderer()\n");
	if(mainDisplayElementHandle != 0)
	{
		uint32_t update = vc_dispmanx_update_start(10);
		vc_dispmanx_element_remove(update,mainDisplayElementHandle);
		vc_dispmanx_update_submit_sync( update );
	}

	if(offscreenDisplay != 0)
		vc_dispmanx_display_close(offscreenDisplay);

	if(offscreenDispResourceHandle != 0)
		vc_dispmanx_resource_delete(offscreenDispResourceHandle);
}

void PiSlideRenderer::setDispmanxLayer(uint32_t layer)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::setDispmanxLayer(%d)\n",layer);
	dispmanxLayer = layer;
}


void PiSlideRenderer::freeSlideResources()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::freeSlideResources()\n");

	state = SLIDE_STATE_UNLOADING;

	double endTime;
	double startTime2 = linuxTimeInMs();

	if(slide == NULL)
	{
		state = SLIDE_STATE_IDLE;
		return;
	}

	uint32_t update = vc_dispmanx_update_start(10);

	for(list<pis_MediaItem*>::iterator it = slide->MediaElements.begin();
			it != slide->MediaElements.end(); ++it)
	{
		(*it)->Unload(update);
	}

	vc_dispmanx_update_submit_sync(update);

	endTime = linuxTimeInMs();
	pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::freeSlideResources(): Freed All: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));

	state = SLIDE_STATE_IDLE;
}

void PiSlideRenderer::startSlide()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::startSlide()\n");
	slideStartTime = linuxTimeInMs();

	//Start all media items

	state = SLIDE_STATE_APPEARING;
}

void PiSlideRenderer::loadSlideResources()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::loadSlideResources()\n");
	state = SLIDE_STATE_LOADING;

	if(slide == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"PiSlideRenderer::loadSlideResources(): Error: no slide to load.\n");
		return;
	}

	pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::loadSlideResources: Loading slide resources\n");

	double startTime2 = linuxTimeInMs();

	for(list<pis_MediaItem*>::iterator it = slide->MediaElements.begin();
			it != slide->MediaElements.end(); ++it)
	{
		//Might be a better place to do this than every load but it's quick
		(*it)->SetGraphicsHandles(offscreenDisplay,mainDisplayInfo.width,mainDisplayInfo.height);
		(*it)->Load();
	}

	float endTime = linuxTimeInMs();
	pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::loadSlideResources: Loaded All: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));

	state = SLIDE_STATE_LOADED;
}


void PiSlideRenderer::doUpdate(uint32_t update)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::doUpdate(%d)\n",update);

	float ms=linuxTimeInMs();

	//TODO Put states into switch statement

	if(state == SLIDE_STATE_APPEARING){
		opacity = ((float)(ms-slideStartTime))/slide->GetDissolveTime();
		if(opacity > 1)	{
			opacity = 1;
			state = SLIDE_STATE_LIVE;
		}
	}

	if(slide != NULL && state == SLIDE_STATE_LIVE &&
			ms > slideStartTime + slide->GetDissolveTime() + slide->GetHoldTime())
	{
		state = SLIDE_STATE_EXPIRED;
	}

	if(state == SLIDE_STATE_DISSAPPEARING)
	{
		opacity = 0;
		state = SLIDE_STATE_DONE;
	}

	//Setting the layer every time, might change to as-needed
	vc_dispmanx_element_change_attributes(update,
			mainDisplayElementHandle,
			1 , //To change: Layer
			dispmanxLayer, //Layer
			0, //Opacity
			0, //Dest rect
			0, //Src rect
			0, //mask
			DISPMANX_NO_ROTATE); //transform

	vc_dispmanx_element_change_attributes(update,
			mainDisplayElementHandle,
			2, //To change: Opacity
			0, //Layer
			opacity*255, //Opacity
			0, //Dest rect
			0, //Src rect
			0, //mask
			DISPMANX_NO_ROTATE); //transform

	compositeSlide(update);

	return;
}

void PiSlideRenderer::compositeSlide(uint32_t update)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::compositeSlide(%d)\n",update);

	double startTime2, endTime;

	if(slide == NULL)
		return;

	startTime2 = linuxTimeInMs();

	int layer = 2000;

	for(list<pis_MediaItem *>::iterator it = slide->MediaElements.begin();
			it != slide->MediaElements.end(); ++it)
	{
		(*it)->DoComposite(update,++layer);
	}

	endTime = linuxTimeInMs();
	pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer::compositeSlide: Composited all: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));
}

float PiSlideRenderer::getOpacity()
{
	return opacity;
}
