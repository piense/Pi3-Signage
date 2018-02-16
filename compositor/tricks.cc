#include "tricks.h"

#include <time.h>
#include <math.h>

//Slow but useful for debugging
//Not 100% correct either, a bit red shifted
uint32_t *packedYUV420toARGB(uint32_t srcWidth, uint32_t srcHeight, uint32_t srcStride, uint32_t sliceHeight, uint8_t *img)
{
	uint32_t *ret = new uint32_t[srcWidth*srcHeight];
	float u, v;
	uint32_t uStart = srcStride*sliceHeight;
	uint32_t vStart = srcStride*sliceHeight*1.25;
	float r,g,b, r1, g1, b1;
	uint8_t r8, b8, g8;
	uint32_t sliceOffset;
	uint32_t y3;
	float yc1, yc2, yc3, yc4, yc;

	for(uint32_t y2 = 0;y2<srcHeight;y2+=2)
	{
		sliceOffset = y2/sliceHeight*sliceHeight*1.5*srcStride;
		y3 = y2%sliceHeight;
		for(uint32_t x = 0;x<srcWidth;x+=2)
		{
			yc1 = img[x+y3*srcStride + sliceOffset];
			yc2 = img[x+y3*srcStride + 1 +sliceOffset];
			yc3 = img[x+(y3+1)*srcStride + sliceOffset];
			yc4 = img[x+(y3+1)*srcStride + 1 + sliceOffset];

			u = img[uStart+x/2+(y3/2)*(srcStride/2) + sliceOffset];
			v = img[vStart+x/2+(y3/2)*(srcStride/2) + sliceOffset];

			//b = 1.164*(y - 16.0) + 2.018*(u - 128.0);
			//g = 1.164*(y - 16.0) - 0.813*(v - 128.0) - 0.391*(u - 128.0);
			//r = 1.164*(y - 16.0) + 1.596*(v - 128.0);

			b = 2.018*(u - 128.0);
			g = 0.813*(v - 128.0) - 0.391*(u - 128.0);
			r = 1.596*(v - 128.0);

			yc = 1.164*(yc1 - 16.0);
			r1 = yc + r;
			g1 = yc - g;
			b1 = yc + b;
			if(r1<0)r1=0;
			if(r1>255)r1=255;
			if(g1<0)g1=0;
			if(g1>255)g1=255;
			if(b1<0)b1=0;
			if(b1>255)b1=255;
			r8 = r1; g8 = g1; b8=b1;
			ret[x+y2*srcWidth] = (0xff<<24)|(r8<<16)|(g8<<8)|(b8);

			yc = 1.164*(yc2 - 16.0);
			r1 = yc + r;
			g1 = yc - g;
			b1 = yc + b;
			if(r1<0)r1=0;
			if(r1>255)r1=255;
			if(g1<0)g1=0;
			if(g1>255)g1=255;
			if(b1<0)b1=0;
			if(b1>255)b1=255;
			r8 = r1; g8 = g1; b8=b1;
			ret[x+y2*srcWidth+1] = (0xff<<24)|(r8<<16)|(g8<<8)|(b8);

			yc = 1.164*(yc3 - 16.0);
			r1 = yc + r;
			g1 = yc - g;
			b1 = yc + b;
			if(r1<0)r1=0;
			if(r1>255)r1=255;
			if(g1<0)g1=0;
			if(g1>255)g1=255;
			if(b1<0)b1=0;
			if(b1>255)b1=255;
			r8 = r1; g8 = g1; b8=b1;
			ret[x+(y2+1)*srcWidth] = (0xff<<24)|(r8<<16)|(g8<<8)|(b8);

			yc = 1.164*(yc4 - 16.0);
			r1 = yc + r;
			g1 = yc - g;
			b1 = yc + b;
			if(r1<0)r1=0;
			if(r1>255)r1 = 255;
			if(g1<0)g1=0;
			if(g1>255)g1=255;
			if(b1<0)b1=0;
			if(b1>255)b1=255;
			r8 = r1; g8 = g1; b8=b1;
			ret[x+(y2+1)*srcWidth+1] = (0xff<<24)|(r8<<16)|(g8<<8)|(b8);
		}
	}
	return ret;
}

double linuxTimeInMs()
{
	struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    time_t s2  = spec.tv_sec;
    return round(spec.tv_nsec / 1.0e6)+s2*1000; // Convert nanoseconds to milliseconds
}
