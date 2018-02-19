#pragma once

#include <stdint.h>

#include "../PiSignageLogging.h"
#include "../PiSlide.h"

extern "C"
{
//External libs
#include "bcm_host.h"
#include "vgfont/vgfont.h"
}

enum SlideRenderState
{
	SLIDE_STATE_IDLE,
	SLIDE_STATE_LOADING,
	SLIDE_STATE_LOADED,
	SLIDE_STATE_APPEARING,
	SLIDE_STATE_LIVE,
	SLIDE_STATE_EXPIRED,
	SLIDE_STATE_DISSAPPEARING,
	SLIDE_STATE_DONE,
	SLIDE_STATE_UNLOADING,
	SLIDE_STATE_ERROR
};

class PiSlideRenderer{
public:
	PiSlideRenderer();
	~PiSlideRenderer();

	//Called each update loop
	void doUpdate(uint32_t update);

	void setDispmanxLayer(uint32_t layer);

	void startSlide(); //Fades up

	void loadSlideResources(); //Reads resources from disk and gets them ready in memory
	void freeSlideResources();

	float getOpacity();

	int32_t dispmanxLayer;

	pis_Slide *slide; //TODO move this out of public and safe a few things

	SlideRenderState state; //TODO: thread-safe getSlideState()
private:
	float opacity;
	void compositeSlide(uint32_t update);

	DISPMANX_DISPLAY_HANDLE_T mainDisplay, offscreenDisplay;
	uint32_t offscreenDispBufferHandle;
	DISPMANX_RESOURCE_HANDLE_T  offscreenDispResourceHandle;
	DISPMANX_ELEMENT_HANDLE_T mainDisplayElementHandle;

	DISPMANX_MODEINFO_T mainDisplayInfo;

	VC_RECT_T srcRect, screenRect; //srcRect is screenRect * 2^16

	VC_DISPMANX_ALPHA_T alpha; //alpha properties of the rendered element

	float slideStartTime;
};
