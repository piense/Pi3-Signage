#include <string>

#include "PiMediaText.h"
#include "../compositor/tricks.h"
#include "../compositor/fontTest.h"
#include "../PiSignageLogging.h"

using namespace std;

pis_MediaText::pis_MediaText()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaText::pis_MediaText\n");

	X = 0;
	Y = 0;
	FontHeight = 1;
	FontName = "";
	Text = "";
	Color = 0xFFFFFFFF;

	ScreenWidth = 0;
	ScreenHeight = 0;
	res = 0;
	element = 0;
	offscreenDisplay = 0;
	imgHandle = 0;

	alpha.flags = (DISPMANX_FLAGS_ALPHA_T)(DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX);
	alpha.opacity = 255; //alpha 0->255
	alpha.mask = 0;
}

pis_MediaText::~pis_MediaText()
{

}

//Gives the media item access to the Slides off screen buffer region
//Assumption is that it needs to render to the top layer of this
//when DoComposite() is called
int pis_MediaText::SetGraphicsHandles(DISPMANX_DISPLAY_HANDLE_T offscreenBufferHandle, uint32_t offscreenWidth, uint32_t offscreenHeight)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaText::SetGraphicsHandles\n");

	offscreenDisplay = offscreenBufferHandle;
	ScreenWidth = offscreenWidth;
	ScreenHeight = offscreenHeight;

	vc_dispmanx_rect_set(&screenRect,0,0,offscreenWidth,offscreenHeight);

	return 0;
}

//Called when a slide is loading resources
int pis_MediaText::Load()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaText::Load()\n");

	float startTime = linuxTimeInMs();

	//TODO: range check width & height

	//TODO Need to factor in stride for dispmanx to be happy
	//at all resolutions
	res = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
			ScreenWidth,ScreenHeight,
			&imgHandle);

	if(res == 0)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaText::Load Error: Couldn't allocate resource for text.\n");
		return -1;
	}

	//Might just put this 'new' somewhere globally to avoid time allocating every time
	uint32_t *temp= new uint32_t[ScreenWidth*ScreenHeight];

	if(temp == NULL){
		//Should we free dispmanx_resource here? Should be done in slide cleanup even if this didn't work
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaText::Load Error: Couldn't allocate memory for text.\n");
		return -1;
	}

	memset(temp,0,ScreenWidth*ScreenHeight*4);

	renderTextToScreen(temp,
			ScreenWidth, ScreenHeight,
			(uint32_t)(X * ScreenWidth),
			(uint32_t)(Y * ScreenHeight),
			(uint32_t)(FontHeight * ScreenHeight),
			Text.c_str(),
			(Color & 0x00FF0000)>>16,
			(Color & 0x0000FF00)>>8,
			Color & 0x000000FF,
			(Color & 0xFF000000)>>24
			);

	VC_RECT_T rectTxt = {0,0,(int32_t)ScreenWidth,(int32_t)ScreenHeight};
	int ret = vc_dispmanx_resource_write_data(res,
			VC_IMAGE_ARGB8888, ScreenWidth*4,temp,
			&rectTxt
			);

	if(ret != 0)
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaText::Load: Error writing image data to dispmanx resource\n");

	delete [] temp;

	pis_logMessage(PIS_LOGLEVEL_INFO, "pis_MediaText::Load: Loaded Text: %f\n",linuxTimeInMs()-startTime);

	return 0;
}

//Called when a slide is unloading resources
int pis_MediaText::Unload(uint32_t update)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaText::UnLoad()\n");

	if(element != 0)
		vc_dispmanx_element_remove(update, element);

	element = 0;

	if(res != 0){
		vc_dispmanx_resource_delete(res);
		res = 0;
	}

	return 0;
}

//Called when a slide begins it's transition
int pis_MediaText::Go()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaText::Go()\n");
	return -1;
}

//Called in the SlideRenderer's render loop
//Responsible for compositing on top layer of
//off screen buffer (if it has graphics content)
void pis_MediaText::DoComposite(uint32_t update, uint32_t layer)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaText::DoComposite()\n");

	float startTime = linuxTimeInMs();

	if(res == 0)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaText::DoComposite: Error: Text not loaded\n");
	}else{
		if(element == 0){
			pis_logMessage(PIS_LOGLEVEL_ALL, "pis_MediaText::DoComposite: Adding text element\n");
			VC_RECT_T rectImg = {0,0,
					(int32_t)(ScreenWidth<<16),(int32_t)(ScreenHeight<<16)};

			element = vc_dispmanx_element_add(update,
					offscreenDisplay,layer++,
					&screenRect,
					res,
					&rectImg,
					DISPMANX_PROTECTION_NONE,
					&alpha,
					NULL,
					DISPMANX_NO_ROTATE
					);
			if(element == 0)
				pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaText::DoComposite: Error compositing text.\n");
		}else{
			//TODO: Update text data, possibly for movement or dynamic text
		}
	}
	pis_logMessage(PIS_LOGLEVEL_ALL, "pis_MediaText::DoComposite: Text: %f ms\n",linuxTimeInMs()-startTime);
}

//Returns the state of the media item
pis_MediaState pis_MediaText::GetState()
{
	//TODO
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaText::GetState()\n");
	return pis_MediaUnloaded;
}

