#pragma once
#include <stdint.h>
#include "ilclient/ilclient.h"
#include "tricks.h"
#include <stdbool.h>

sImage *resizeImage2(char *img,
		uint32_t srcWidth, uint32_t srcHeight, //pixels
		size_t srcSize, //bytes
		OMX_COLOR_FORMATTYPE imgCoding,
		uint16_t srcStride,
		uint16_t srcSliceHeight,
		uint32_t outputWidth,
		uint32_t outputHeight,
		bool crop,
		bool lockAspect
		);
