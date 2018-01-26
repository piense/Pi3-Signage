#pragma once

#include <stdint.h>
#include <bool.h>

//External Libs
#include "bcm_host.h"
#include "vgfont.h"

//ARGB 32 bit image buffer
typedef struct pis_Img
{
	uint32_t *img;
	uint32_t width;
	uint32_t height;
}pis_Img;

typedef enum pis_compositorErrors
{
	pis_COMPOSITOR_ERROR_NONE
}pis_compositorErrors;

typedef enum pis_mediaSizing
{
	pis_SIZE_CROP,
	pis_SIZE_FIT,
	pis_SIZE_STRETCH
} pis_mediaSizing;

typedef enum pis_mediaTypes
{
	pis_MEDIA_IMAGE,
	pis_MEDIA_VIDEO,
	pis_MEDIA_TEXT,
	pis_MEDIA_AUDIO
} pis_mediaTypes;

//x,y to center of item
//width and height relative to frame size of 1x1

typedef struct pis_mediaImage
{
	float x, y, maxWidth, maxHeight;
	char *filename;
	pis_mediaSizing sizing;
} pis_mediaImage;

typedef struct pis_mediaVideo
{
	float x, y, maxWidth, maxHeight;
	char *filename;
	pis_mediaSizing sizing;
	bool loop;
} pis_mediaVideo;

typedef struct pis_mediaText
{
	float x, y, fontHeight;
	char *fontName;
	char *text;
}pis_mediaText;

typedef struct pis_mediaAudio
{

}pis_mediaAudio;

typedef struct pis_mediaElement_s
{
	char *name;
	pis_mediaTypes mediaType;
	void *data; //struct to match the mediaType
	uint32_t z; //0 = bottom
	//
} pis_mediaElement_s;

typedef struct pis_mediaElementList_s
{
	pis_mediaElement_s mediaElement;
	pis_mediaElementList_s *next;
}pis_mediaElementList_s;

//A set of presentation slides
typedef struct pis_slides_s{
	uint32_t duration; //millis
	uint32_t dissolve; //millis for the fade into the slide
//	bool loaded; //media content is ready in memory

	pis_mediaElementList_s *mediaElementsHead;

} pis_slides_s;

typedef struct pis_compositor_s
{
	uint8_t state;
	uint32_t screenWidth, screenHeight;
	GRAPHICS_RESOURCE_HANDLE backImg, dissolveImg;

	pis_slides_s *slides;

	pis_slides_s *currentSlide;

	uint32_t fps; //as calculated in pis_doCompositor

	double dissolveStartTime;
	double slideStartTime;
	uint32_t slideHoldTime;
	uint32_t slideDissolveTime;
	uint8_t transitionState;
} pis_compositor_s;

pis_compositor_s pis_compositor;

pis_compositorErrors pis_initializeCompositor();
pis_compositorErrors pis_cleanup();
pis_compositorErrors pis_doCompositor(); //Might end up as a thread from init


