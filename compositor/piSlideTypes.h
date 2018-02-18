#pragma once

#include <string>

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

//Media might become class/inheritance based with a base class of "MediaItem"

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
	std::string filename;
	pis_mediaSizing sizing;
	pis_img cache;
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
	std::string fontName;
	std::string text;
	uint32_t color;
};

struct pis_mediaAudio
{

};

struct pis_mediaElement
{
	std::string name;
	pis_mediaTypes mediaType;
	void *data; //struct to match the mediaType
	void *appData; //Host specific rendering data
	uint32_t z; //0 = bottom
};


enum PlaybackState
{
	PB_IDLE,
	PB_DISSOLVING,
	PB_LOADING_NEXT,
	PB_WAITING_FOR_EXPIRED
};
