#pragma once

#include <stdint.h>

uint32_t *packedYUV420toARGB(uint32_t srcWidth, uint32_t srcHeight, uint32_t srcStride, uint32_t sliceHeight, uint8_t *img);
double linuxTimeInMs();

//ARGB 32 bit image buffer
struct pis_img
{
	uint32_t *img;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
};

enum pis_mediaSizing
{
	pis_SIZE_CROP,
	pis_SIZE_SCALE,
	pis_SIZE_STRETCH
};

#ifndef ALIGN_UP
#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))
#endif

typedef struct _sImage sImage;

struct _sImage {
    uint8_t *imageBuf;
    uint32_t imageWidth;
    uint32_t imageHeight;
    uint8_t colorSpace;
    uint32_t imageSize; //in bytes

    uint32_t stride;
    uint32_t sliceHeight;
};
