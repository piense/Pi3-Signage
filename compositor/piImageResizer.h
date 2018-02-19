#pragma once

extern "C"
{
#include "bcm_host.h"
#include "ilclient/ilclient.h"
}

#include <stdint.h>
#include "tricks.h"

#define TIMEOUT_MS 200

sImage *decodeJpgImage(char *img);

class PiImageResizer
{
public:
	//Starts up the decoder component
	PiImageResizer();

	int ResizeImage(char *img,
			uint32_t srcWidth, uint32_t srcHeight, //pixels
			size_t srcSize, //bytes
			OMX_COLOR_FORMATTYPE imgCoding,
			uint16_t srcStride,
			uint16_t srcSliceHeight,
			uint32_t outputWidth,
			uint32_t outputHeight,
			pis_mediaSizing scaling,
			sImage **ret
			);

	~PiImageResizer();

	//Callback from ilclient
	static void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data);

	//Callback from ilclient, data = class pointer
	static void EmptyBufferDoneCB(void *data,COMPONENT_T *comp);

	//Callback from ilclient, data = class pointer
	static void FillBufferDoneCB(void* data,COMPONENT_T *comp);

	void printState();

private:

	int startupImageResizer();
	int prepareImageResizer();
	int doResize();

	//this function run the boilerplate to setup the openmax components;
	int setupOpenMaxImageResizer();

	//this function passed the jpeg image buffer in, and returns the decoded image
	int resizeImage(char *img,
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

	//this function cleans up the decoder.
	void cleanup();

	int portSettingsChanged();

    COMPONENT_T    *component;
    OMX_HANDLETYPE  handle;
    int             inPort;
    int             outPort;

    ILCLIENT_T     *client;

    //Input stuff
	uint8_t *imgBuf;
	uint32_t srcWidth;
	uint32_t srcHeight;
	uint32_t srcSize;
	uint32_t srcStride;
	uint32_t srcSliceHeight;
	OMX_COLOR_FORMATTYPE srcColorSpace; //TODO switch to enum
	OMX_BUFFERHEADERTYPE *ibBufferHeader;

	uint32_t cropTop;
	uint32_t cropLeft;
	uint32_t cropWidth;
	uint32_t cropHeight;

    //Output Buffer stuff
    OMX_BUFFERHEADERTYPE *obHeader;
    uint8_t obGotEOS; // 0 = no
    uint32_t obDecodedAt;
    uint32_t outputWidth;
    uint32_t outputHeight;
    uint32_t outputStride;
    uint32_t outputSliceHeight;
    uint8_t *outputPic;
    uint8_t outputColorSpace; //TODO switch to enum


};
