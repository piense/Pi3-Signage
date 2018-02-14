#pragma once

extern "C"
{
//External Libs
#include "bcm_host.h"
#include "vgfont/vgfont.h"
}

#define PIS_COLOR_ARGB_BLACK 0xFF000000
#define PIS_COLOR_ARGB_WHITE 0xFFFFFFFF
#define PIS_COLOR_ARGB_RED   0xFFFF0000
#define PIS_COLOR_ARGB_GREEN 0xFF00FF00
#define PIS_COLOR_ARGB_BLUE  0xFF0000FF

//TODO: The compositor shouldn't be managing all the slides,
//Need to separate that out and find a nice way to manage the image cache in RAM

//ARGB 32 bit image buffer
struct pis_img
{
	uint32_t *img;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
};

enum pis_compositorErrors
{
	pis_COMPOSITOR_ERROR_NONE
};

enum pis_mediaSizing
{
	pis_SIZE_CROP,
	pis_SIZE_SCALE,
	pis_SIZE_STRETCH
};

enum pis_mediaTypes
{
	pis_MEDIA_IMAGE,
	pis_MEDIA_VIDEO,
	pis_MEDIA_TEXT,
	pis_MEDIA_AUDIO
};

//x,y to center of item
//width and height relative to frame size of 1x1

//TODO: Storing media on a per-slide basis is less than efficient
//Slides should point to a media library with cache that can be
//preloaded and reused RAM permitting

struct pis_mediaImage
{
	float x, y, maxWidth, maxHeight;
	char *filename;
	pis_mediaSizing sizing;
	pis_img cache;
	DISPMANX_RESOURCE_HANDLE_T res;
	DISPMANX_ELEMENT_HANDLE_T element;
	uint32_t imgHandle;
};

struct pis_mediaVideo
{
	float x, y, maxWidth, maxHeight;
	char *filename;
	pis_mediaSizing sizing;
	bool loop;
};

struct pis_mediaText
{
	float x, y, fontHeight;
	char *fontName;
	char *text;
	uint32_t color;
	DISPMANX_RESOURCE_HANDLE_T res;
	DISPMANX_ELEMENT_HANDLE_T element;
	uint32_t imgHandle;
};

struct pis_mediaAudio
{

};

struct pis_mediaElement_s
{
	const char *name;
	pis_mediaTypes mediaType;
	void *data; //struct to match the mediaType
	uint32_t z; //0 = bottom
	//
};

struct pis_mediaElementList_s
{
	pis_mediaElement_s mediaElement;
	struct pis_mediaElementList_s *next;
};

//A set of presentation slides
struct pis_slides_s{
	uint32_t duration; //millis
	uint32_t dissolve; //millis for the fade into the slide
//	bool loaded; //media content is ready in memory

	pis_mediaElementList_s *mediaElementsHead;
	pis_slides_s *next;
};

enum PlaybackState
{
	PB_IDLE,
	PB_DISSOLVING,
	PB_LOADING_NEXT,
	PB_WAITING_FOR_EXPIRED
};
