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

class PiImageDecoder
{
public:

	//Starts up the decoder component
	PiImageDecoder();

	int DecodeJpegImage(const char *img, sImage **ret);

	~PiImageDecoder();

	//Callback from ilclient
	static void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data);

	//Callback from ilclient, data = class pointer
	static void EmptyBufferDoneCB(void *data,COMPONENT_T *comp);

	//Callback from ilclient, data = class pointer
	static void FillBufferDoneCB(void* data,COMPONENT_T *comp);


private:

	int startupImageDecoder();
	int prepareImageDecoder();

	//this function run the boilerplate to setup the openmax components;
	int setupOpenMaxJpegDecoder();

	//this function passed the jpeg image buffer in, and returns the decoded image
	int decodeImage( );

	//this function cleans up the decoder.
	void cleanup();

	int portSettingsChanged();

    COMPONENT_T    *component;
    OMX_HANDLETYPE  handle;
    int             inPort;
    int             outPort;

    ILCLIENT_T     *client;

    //Input Buffer stuff
    uint8_t ibIndex; //Seems like the components won't accept more than 16 buffers
    uint32_t ibToRead; //Bytes remaining in the input image to push through the decoder
    uint16_t ibBufferCount;
    OMX_BUFFERHEADERTYPE **ibBufferHeader;

    //Output Buffer stuff
    OMX_BUFFERHEADERTYPE *obHeader;
    uint8_t obGotEOS; // 0 = no
    uint32_t obDecodedAt;

	uint8_t *imgBuf;
	uint8_t *inputBuf;
	uint32_t width;
	uint32_t height;
	uint32_t srcSize;

	uint8_t colorSpace;
	uint32_t stride;
	uint32_t sliceHeight;
};
