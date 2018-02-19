#pragma once

#include <string>

extern "C"
{
//External libs
#include "bcm_host.h"
}

#include "PiMediaItem.h"
#include "../compositor/tricks.h"
#include "../PiSlide.h"

class pis_MediaImage : pis_MediaItem
{
public:
	pis_MediaImage();
	~pis_MediaImage();

	//Gives the media item access to the Slides off screen buffer region
	//Assumption is that it needs to render to the top layer of this
	//when DoComposite() is called
	int SetGraphicsHandles(DISPMANX_DISPLAY_HANDLE_T offscreenBufferHandle,
			uint32_t offscreenWidth, uint32_t offscreenHeight);

	//Called when a slide is loading resources
	int Load();

	//Called when a slide is unloading resources
	int Unload(uint32_t update);

	//Called when a slide begins it's transition
	int Go();

	//Called in the SlideRenderer's render loop
	//Responsible for compositing on top layer of
	//off screen buffer (if it has graphics content)
	void DoComposite(uint32_t update, uint32_t layer);

	//Used to identify media type in XML
	std::string GetType();

	//Recreates the object from XML
	static int FromXML(std::string XML, pis_MediaItem **mediaItem);

	//Exports the media structure as XML
	// XML: Out, pointer to pointer to point to new string object
	int ToXML(std::string **XML);

	//Returns the state of the media item
	pis_MediaState GetState();

	//Adds image to a given slide
	static int AddToSlide(const char *filename, float x, float y,
			float width, float height, pis_mediaSizing scaling, pis_Slide *slide);

	float X, Y, MaxWidth, MaxHeight;
	uint32_t ScreenWidth, ScreenHeight;
	std::string Filename;
	pis_mediaSizing Scaling;
	pis_img ImgCache;

	static const char* MediaType;

private:
	static const pis_MediaItemRegistrar r;

	//Pi specific graphics resources
	DISPMANX_RESOURCE_HANDLE_T res;
	DISPMANX_ELEMENT_HANDLE_T element;
	DISPMANX_DISPLAY_HANDLE_T offscreenDisplay;
	uint32_t imgHandle;
	VC_DISPMANX_ALPHA_T alpha;
};
