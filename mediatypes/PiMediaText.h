#pragma once

#include <string>

#include <libxml/parser.h>

#include "PiMediaItem.h"
#include "../PiSlide.h"

extern "C"
{
//External libs
#include "bcm_host.h"
}

class pis_MediaText : pis_MediaItem
{
public:
	pis_MediaText();
	~pis_MediaText();

	//Gives the media item access to the Slides off screen buffer region
	//Assumption is that it needs to render to the top layer of this
	//when DoComposite() is called
	int SetGraphicsHandles(uint32_t offscreenBufferHandle, uint32_t offscreenWidth,
			uint32_t offscreenHeight);

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
	static int FromXML(xmlNodePtr node, pis_MediaItem **outItem);

	//Exports the media structure as XML
	// XML: Out, pointer to pointer to point to new string object
	int ToXML(std::string **XML);

	int ToXML(xmlNodePtr slideNode);

	//Returns the state of the media item
	pis_MediaState GetState();

	static int NewText(const char *text, float x, float y,
			float width, float height, float fontHeight,
			const char *font, uint32_t color, pis_MediaItem **item);

	float X, Y, FontHeight;
	std::string FontName;
	std::string Text;
	uint32_t Color;

	uint32_t ScreenWidth, ScreenHeight;

	static const char* MediaType;

private:
	static const pis_MediaItemRegistrar r;

	//Pi specific graphics resources
	DISPMANX_RESOURCE_HANDLE_T res;
	DISPMANX_ELEMENT_HANDLE_T element;
	DISPMANX_DISPLAY_HANDLE_T offscreenDisplay;
	uint32_t imgHandle;
	VC_DISPMANX_ALPHA_T alpha;
	VC_RECT_T screenRect;
};
