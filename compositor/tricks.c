#include "tricks.h"
#include <stdlib.h>
#include <stdio.h>

sImage *generateYUV420PackedTestImage(uint32_t width, uint32_t height)
{
	return NULL;
}

//Never got this to work correctly, not sure where problem is
sImage *colorspaceYUV420PackedToRGBA32Slice(sImage *src, uint32_t strideHeight){

	//UV are subsampled twice in the vertical and twice in the horizontal direction
	/*
		For each of these uncompressed formats, each buffer payload contains a slice of
		the Y, U, and V planes. The slices are always ordered Y, U, and V or Y, V and U
		- depending on their color format definition.. The nSliceHeight refers to the
		slice height of the Y plane. The slice height of the U and V planes are derived
		from the slice height for the Y plane based upon for the format. For example, for
		OMX_COLOR_FormatYUV420PackedPlanar with an nSliceHeight of
		16, a buffer payload shall contain 16 spans of Y followed by 8 spans of U (half
		the stride) and 8 spans of V (half the stride).
	 */

	sImage *ret = malloc(sizeof(sImage));
	//ret->colorSpace =
	ret->imageHeight = src->imageHeight;
	ret->imageWidth = src->imageWidth;
	ret->imageBuf = malloc(src->imageHeight*src->imageWidth*4);

	uint8_t Y;
	uint8_t U;
	uint8_t V;
	uint8_t R;
	uint8_t G;
	uint8_t B;

	uint32_t strideStart;

	//TODO this will be substantially quicker to scan the YUV image and expand to the RGB frame. eh.
	for(uint32_t y = 0; y<src->imageHeight; y++){
		for(uint32_t x = 0;x<src->imageWidth;x++){
			strideStart = ((y * src->imageWidth + x)/strideHeight)*(3*strideHeight)/2;

	/*		printf("stridestart: %d X: %d Y:%d Y: %d U: %d V: %d \n",strideStart, x, y,
					(uint32_t)(strideStart + (y*src->imageWidth + x)%strideHeight),
					(uint32_t)(strideStart + (y*(src->imageWidth/2) + x/2)%(strideHeight/2) + strideHeight),
					(uint32_t)(strideStart + (y*(src->imageWidth/2) + x/2)%(strideHeight/2) + (5*strideHeight)/4)
					);*/

			Y = src->imageBuf[strideStart + (y*src->imageWidth + x)%strideHeight];
			U = src->imageBuf[strideStart + (y*(src->imageWidth/2) + x/2)%(strideHeight/2) + strideHeight];
			V = src->imageBuf[strideStart + (y*(src->imageWidth/2) + x/2)%(strideHeight/2) + (5*strideHeight)/4];

			//TODO: Evaluate reworking this for integer math
			R = Y+1.403*V;
			G = Y - .344*U - .714*V;
			B = Y + 1.77*U;

			ret->imageBuf[(y*src->imageWidth + x) * 4 + 0] = B;
			ret->imageBuf[(y*src->imageWidth + x) * 4 + 1] = G;
			ret->imageBuf[(y*src->imageWidth + x) * 4 + 2] = R;
			ret->imageBuf[(y*src->imageWidth + x) * 4 + 3] = 255;

			/*printf("stridestart: %d Y: %d U: %d V: %d R: %d G: %d B: %d\n",strideStart,Y,U,V,R,G,B);*/

		}
	}

	return ret;

}
