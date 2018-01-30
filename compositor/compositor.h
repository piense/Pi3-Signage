#pragma once

#include <stdint.h>
#include <stdbool.h>

//External Libs
#include "bcm_host.h"
#include "vgfont/vgfont.h"
#include "piDisplayRender.h"

pis_window *backWindow;
pis_window *dissolveWindow;

#define PIS_COLOR_ARGB_BLACK 0xFF000000
#define PIS_COLOR_ARGB_WHITE 0xFFFFFFFF
#define PIS_COLOR_ARGB_RED   0xFFFF0000
#define PIS_COLOR_ARGB_GREEN 0xFF00FF00
#define PIS_COLOR_ARGB_BLUE  0xFF0000FF

//TODO: The compositor shouldn't be managing all the slides,
//Need to separate that out and find a nice way to manage the image cache in RAM

//ARGB 32 bit image buffer
typedef struct pis_img
{
	uint32_t *img;
	uint32_t width;
	uint32_t height;
}pis_img;

typedef enum pis_compositorErrors
{
	pis_COMPOSITOR_ERROR_NONE
}pis_compositorErrors;

typedef enum pis_mediaSizing
{
	pis_SIZE_CROP,
	pis_SIZE_SCALE,
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

//TODO: Storing media on a per-slide basis is less than efficient
//Slides should point to a media library with cache that can be
//preloaded and reused RAM permitting

typedef struct pis_mediaImage
{
	float x, y, maxWidth, maxHeight;
	char *filename;
	pis_mediaSizing sizing;
	pis_img cache;
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
	uint32_t color;
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
	struct pis_mediaElementList_s *next;
}pis_mediaElementList_s;

//A set of presentation slides
typedef struct pis_slides_s{
	uint32_t duration; //millis
	uint32_t dissolve; //millis for the fade into the slide
//	bool loaded; //media content is ready in memory

	pis_mediaElementList_s *mediaElementsHead;
	struct pis_slides_s *next;

} pis_slides_s;

typedef struct pis_compositor_s
{
	uint8_t state;
	uint32_t screenWidth, screenHeight;
	GRAPHICS_RESOURCE_HANDLE backImg, dissolveImg;

	pis_slides_s *slides;

	pis_slides_s *currentSlide;

	double dissolveStartTime;
	double slideStartTime;
	uint32_t slideHoldTime;
	uint32_t slideDissolveTime;

} pis_compositor_s;

pis_compositor_s pis_compositor;

pis_compositorErrors pis_initializeCompositor();
pis_compositorErrors pis_compositorcleanup();
pis_compositorErrors pis_doCompositor(); //Might end up as a thread from init

pis_compositorErrors pis_AddTextToSlide(pis_slides_s *slide, char* text, char* fontName, float height, float x, float y, uint32_t color);

