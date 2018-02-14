#pragma once

#include <stdint.h>

double linuxTimeInMs();

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
