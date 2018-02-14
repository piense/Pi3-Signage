#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "piSlideTypes.h"

extern "C"
{
//External Libs
#include "bcm_host.h"
#include "vgfont/vgfont.h"
}

#include "piSlideRenderer.h"


struct pis_compositor_s
{
	uint32_t screenWidth, screenHeight;

	pis_slides_s *slides;

	pis_slides_s *nextSlide;
	pis_slides_s *currentSlide;
	uint8_t currentSlideRenderer;
	uint8_t nextSlideRenderer;

	uint8_t numOfRenderers;
	PiSlideRenderer *slideRenderers;

	PlaybackState pbState;

};

extern pis_compositor_s pis_compositor;

pis_compositorErrors pis_initializeCompositor();
pis_compositorErrors pis_compositorcleanup();
pis_compositorErrors pis_doCompositor(); //Might end up as a thread from init

pis_compositorErrors pis_AddTextToSlide(pis_slides_s *slide, const char* text, const char* fontName, float height, float x, float y, uint32_t color);

