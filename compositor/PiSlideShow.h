#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <list>
#include <string>

extern "C"
{
//External Libs
#include "bcm_host.h"
#include "vgfont/vgfont.h"
}

#include "piSlideRenderer.h"
#include "../PiSlide.h"


enum PlaybackState
{
	PB_IDLE,
	PB_DISSOLVING,
	PB_LOADING_NEXT,
	PB_WAITING_FOR_EXPIRED
};


class pis_SlideShow
{
public:
	pis_SlideShow();
	~pis_SlideShow();

	//Loads a directory of images into a slideshow
	//Setup all display options before calling loadDirectory
	// directory: In, directory to load
	// returns: 0 for ok
	int LoadDirectory(std::string directory);

	//Called in a loop by the main function
	//Should probably end up using a threaded model for this
	void DoSlideShow();

	//Overlays picture filename on image
	bool PictureTitles;

	//Transition time between pictures
	uint32_t PictureDissolveTime;

	//Time the picture remains on screen
	uint32_t PictureHoldTime;

	//TODO Might move this back to private
	std::list<pis_Slide*> Slides;

private:

	//Adds a slide to the end of the slide list
	// newSlide: Out, pointer to redirect to new slide object
	// returns: 0 for good
	int AddNewSlide(pis_Slide ** newSlide);

	//Returns the index of the first idle SlideRenderer, negative for error
	int slideRenderersGetIdleRenderer();

	//Determines if any slides are currently fading up
	bool slideRenderersAppearing();

	//Instructs finished renderers to free resources
	void SlideRenderersCleanup();

	//Moves the given renderer index to the foremost layer
	//and lowers the stack
	void slideRenderersResortDispmanXLayers(int topRenderer);

	//Last time in linux millis we printed memory usage.
	//Should move this elsewhere but it's a good check for now
	double LastMemPrint;

	std::list<pis_Slide*>::iterator NextSlide;
	std::list<pis_Slide*>::iterator CurrentSlide;

	uint8_t CurrentSlideRenderer;
	uint8_t NextSlideRenderer;

	//Might change this to a list
	uint8_t NumOfRenderers;
	PiSlideRenderer *SlideRenderers;

	PlaybackState PBState;
};


