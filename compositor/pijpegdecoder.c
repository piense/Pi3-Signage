#include "ilclient/ilclient.h"
#include "stdio.h"
#include "bcm_host.h"
#include "piresizer.h"
#include "pijpegdecoder.h"
#include "tricks.h"
#include "../PiSignageLogging.h"

//I hate asserts
#include <assert.h>

struct
{
	uint8_t *imgBuf;
	uint8_t *inputBuf;
	uint32_t decodedAt;
	uint32_t width;
	uint32_t height;
	uint32_t srcSize;

	uint8_t colorSpace;
	uint32_t stride;
	uint32_t sliceHeight;
} pis_jpegdecoder;

#define OMXJPEG_OK                  0
#define OMXJPEG_ERROR_ILCLIENT_INIT    -1024
#define OMXJPEG_ERROR_OMX_INIT         -1025
#define OMXJPEG_ERROR_MEMORY         -1026
#define OMXJPEG_ERROR_CREATING_COMP    -1027
#define OMXJPEG_ERROR_WRONG_NO_PORTS   -1028
#define OMXJPEG_ERROR_EXECUTING         -1029
#define OMXJPEG_ERROR_NOSETTINGS   -1030

typedef struct _OPENMAX_JPEG_DECODER OPENMAX_JPEG_DECODER;

//this function run the boilerplate to setup the openmax components;
int setupOpenMaxJpegDecoder(OPENMAX_JPEG_DECODER** decoder);

//this function passed the jpeg image buffer in, and returns the decoded image
int decodeImage(OPENMAX_JPEG_DECODER* decoder,
              char* sourceImage, size_t imageSize);

//this function cleans up the decoder.
void cleanup(OPENMAX_JPEG_DECODER* decoder);

#define TIMEOUT_MS 200

typedef struct _COMPONENT_DETAILS {
    COMPONENT_T    *component;
    OMX_HANDLETYPE  handle;
    int             inPort;
    int             outPort;
} COMPONENT_DETAILS;

struct _OPENMAX_JPEG_DECODER {
    ILCLIENT_T     *client;
    COMPONENT_DETAILS *imageDecoder;
    OMX_BUFFERHEADERTYPE **ppInputBufferHeader;
    uint16_t inputBufferCount;
    OMX_BUFFERHEADERTYPE *pOutputBufferHeader;

    //Input Buffer stuff
    uint8_t ibIndex; //Seems like the components won't accept more than 16 buffers
    uint32_t ibToRead; //Bytes remaining in the input image to push through the decoder

    //Output Buffer stuff
    OMX_BUFFERHEADERTYPE *obHeader; //Stored in multiple places ATM but it works :/
    uint8_t obGotEOS; // 0 = no
};

OPENMAX_JPEG_DECODER *pDecoder;

void eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
}

void EmptyBufferDoneCB(
		void *data,
		COMPONENT_T *comp)
{
	OPENMAX_JPEG_DECODER *decoder = (OPENMAX_JPEG_DECODER *) data;

	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: EmptyBufferDoneCB()\n");
	//Fire off the next buffer fill

	if(decoder == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error EmptyBufferDoneCB no pDecoder\n");
		//TODO abort the decoding process
		return;
	}

	if(decoder->ibToRead > 0)
	{
		if(decoder->ibIndex >= decoder->inputBufferCount)
		{
			pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error accessing input buffers\n");
			return;
		}else{
			pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: Using input buffer %d\n",decoder->ibIndex);
		}

		// get next buffer from array
		OMX_BUFFERHEADERTYPE *pBufHeader =
				decoder->ppInputBufferHeader[decoder->ibIndex];

		// step index
		decoder->ibIndex++;

		decoder->ibToRead = decoder->ibToRead - pBufHeader->nFilledLen;

		// update the buffer pointer and set the input flags

		pBufHeader->nOffset = 0;
		pBufHeader->nFlags = 0;
		if (decoder->ibToRead <= 0) {
			pBufHeader->nFlags = OMX_BUFFERFLAG_EOS;
			pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Sending EOS on input buffer\n");
		}

		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Sending buffer %d to input port\n",decoder->ibIndex - 1);

		// empty the current buffer
		int             ret =
			OMX_EmptyThisBuffer(decoder->imageDecoder->handle,
					pBufHeader);

		if (ret != OMX_ErrorNone) {
			pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error emptying input buffer %s\n",OMX_errString(ret));
			//TODO abort decode process
			return;
		}
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: No more image data to send\n");
	}
}

void FillBufferDoneCB(
  void* data,
  COMPONENT_T *comp)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: FillBufferDoneCB()\n");

	OPENMAX_JPEG_DECODER *decoder = (OPENMAX_JPEG_DECODER *) data;

	//TODO: output directly to the image buffer
	if(decoder->obHeader != NULL){
		pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: Processing received buffer\n");

		if(decoder->obHeader->nFilledLen + pis_jpegdecoder.decodedAt > decoder->obHeader->nAllocLen){
			pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: ERROR overrun of decoded image buffer\n %d %d %d\n",decoder->obHeader->nFilledLen, pis_jpegdecoder.decodedAt, decoder->obHeader->nAllocLen);
		}else
			//Copy output port buffer to output image buffer
			memcpy(&pis_jpegdecoder.imgBuf[pis_jpegdecoder.decodedAt],
	    		decoder->obHeader->pBuffer + decoder->obHeader->nOffset,decoder->obHeader->nFilledLen);

	    pis_jpegdecoder.decodedAt += decoder->obHeader->nFilledLen;

	    //See if we've reached the end of the stream
	    if (decoder->obHeader->nFlags & OMX_BUFFERFLAG_EOS) {
	    	pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: Output buffer EOS received\n");
	    	decoder->obGotEOS = 1;
	    }else{
	    	pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: Output buffer asking for more data\n");
			int ret = OMX_FillThisBuffer(decoder->imageDecoder->handle,
					decoder->obHeader);
			if (ret != OMX_ErrorNone) {
				pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error in FillThisBuffer: %s",OMX_errString(ret));
				return;
			}
	    }
	}else{
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error: No obHeader\n");
	}
}


void printState(OMX_HANDLETYPE handle) {
    OMX_STATETYPE state;
    OMX_ERRORTYPE err;

    err = OMX_GetState(handle, &state);
    if (err != OMX_ErrorNone) {
        fprintf(stderr, "Error on getting state\n");
        exit(1);
    }
    switch (state) {
		case OMX_StateLoaded:           printf("StateLoaded\n"); break;
		case OMX_StateIdle:             printf("StateIdle\n"); break;
		case OMX_StateExecuting:        printf("StateExecuting\n"); break;
		case OMX_StatePause:            printf("StatePause\n"); break;
		case OMX_StateWaitForResources: printf("StateWait\n"); break;
		case OMX_StateInvalid:          printf("StateInvalid\n"); break;
		default:                        printf("State unknown\n"); break;
    }
}

void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: OMX error %s\n",OMX_errString(data));
	//TODO Global error flag to abort the process
}

//Called from decodeImage once the decoder has read the file header and
//changed the output port settings for the image
int
portSettingsChanged(OPENMAX_JPEG_DECODER * decoder)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: portSettingsChanged()\n");

    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    //Allocates the buffers and enables the port
    int ret = ilclient_enable_port_buffers(decoder->imageDecoder->component, decoder->imageDecoder->outPort, NULL, NULL, NULL);
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: error allocating output port buffer: %d\n",ret);
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Output port buffers allocated\n");

    decoder->obHeader = ilclient_get_output_buffer(decoder->imageDecoder->component, decoder->imageDecoder->outPort, 0);

    // Get the image dimensions
    //TODO: Store color format too
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = decoder->imageDecoder->outPort;
    OMX_GetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    pis_jpegdecoder.width = portdef.format.image.nFrameWidth;
    pis_jpegdecoder.height = portdef.format.image.nFrameHeight;
    pis_jpegdecoder.colorSpace = portdef.format.image.eColorFormat;
    pis_jpegdecoder.stride = portdef.format.image.nStride;
    pis_jpegdecoder.sliceHeight = portdef.format.image.nSliceHeight;

    //Output port buffer size is always the full image size
    pis_jpegdecoder.imgBuf = malloc(portdef.nBufferSize);
    if(pis_jpegdecoder.imgBuf == NULL)
    {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error allocating output image buffer.\n");
    	//TODO return error
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Allocated output image buffer\n");
    }

    //Ehhh, not sure you ever really want to see this. Maybe LOGLEVEL_ALL_KITCHEN_SINK(?)
    //printPort(decoder->imageDecoder->handle,decoder->imageDecoder->outPort);

    return OMXJPEG_OK;
}

int
prepareImageDecoder(OPENMAX_JPEG_DECODER * decoder)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "JPEG Decoder: prepareImageDecoder()\n");

    decoder->imageDecoder = malloc(sizeof(COMPONENT_DETAILS));
    if (decoder->imageDecoder == NULL) {
		perror("malloc image decoder");
		return OMXJPEG_ERROR_MEMORY;
    }

    int ret = ilclient_create_component(decoder->client,
		&decoder->
		imageDecoder->
		component,
		"image_decode",
		ILCLIENT_DISABLE_ALL_PORTS
		|
		ILCLIENT_ENABLE_INPUT_BUFFERS
		|
		ILCLIENT_ENABLE_OUTPUT_BUFFERS //When not using tunneling (maybe)
		);

    if (ret != 0) {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error creating component: %d\n",ret);
		return OMXJPEG_ERROR_CREATING_COMP;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Created the component.\n");
    }


    // grab the handle for later use in OMX calls directly
    decoder->imageDecoder->handle =
	ILC_GET_HANDLE(decoder->imageDecoder->component);

    // get and store the ports
    OMX_PORT_PARAM_TYPE port;
    port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;

    OMX_GetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamImageInit, &port);
    if (port.nPorts != 2) {
    	return OMXJPEG_ERROR_WRONG_NO_PORTS;
    }

    decoder->imageDecoder->inPort = port.nStartPortNumber;
    decoder->imageDecoder->outPort = port.nStartPortNumber + 1;

    return OMXJPEG_OK;
}

int
startupImageDecoder(OPENMAX_JPEG_DECODER * decoder)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: startupImageDecoder()\n");

    // move to idle
    if(ilclient_change_component_state(decoder->imageDecoder->component,
				    OMX_StateIdle) != 0)
    	printf("startupImageDecoder: Failed to transition to idle.\n");

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Setting port image format\n");
    // set input image format
    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
    imagePortFormat.nVersion.nVersion = OMX_VERSION;
    imagePortFormat.nPortIndex = decoder->imageDecoder->inPort;
    imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    OMX_SetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamImagePortFormat, &imagePortFormat);

    // get buffer requirements
    OMX_PARAM_PORTDEFINITIONTYPE portdef;
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = decoder->imageDecoder->inPort;
    OMX_GetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    //It doesn't like large buffers so use the given size

    uint32_t bufSize = portdef.nBufferSize;

    //First buffer has to be short, probably for it to read
    //the file header, rest of the file is in the second buffer
    //Tried smaller chunks, doesn't seem to like more than 16 input buffers??
    //TODO Only works for 2 or 3 buffers, not a clue why.
    decoder->inputBufferCount = 3;
    portdef.nBufferCountActual = 3;

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Setting port parameters\n");
    OMX_SetParameter(decoder->imageDecoder->handle,
    		OMX_IndexParamPortDefinition, &portdef);

    // enable the port and setup the buffers
    OMX_SendCommand(decoder->imageDecoder->handle,
		    OMX_CommandPortEnable,
		    decoder->imageDecoder->inPort, NULL);

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Initializing input buffers\n");
    decoder->ppInputBufferHeader = malloc(sizeof(void)*decoder->inputBufferCount);
    uint32_t thisBufsize;
    for(int i = 0; i < decoder->inputBufferCount; i++){

    	//If this is the last buffer make sure it all fits
    	thisBufsize = i == decoder->inputBufferCount-1 ?
				   pis_jpegdecoder.srcSize - i*bufSize : bufSize;

    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Allocating buffer of %d\n",thisBufsize);

        int ret = OMX_UseBuffer(decoder->imageDecoder->handle,
    					   &decoder->ppInputBufferHeader[i],
    					   decoder->imageDecoder->inPort,
    					   (void *) NULL,
						   thisBufsize,
    					   pis_jpegdecoder.inputBuf+i*bufSize);
    	if(ret != OMX_ErrorNone) {
    			printf("Allocate resize input buffer, err: %x\n",ret);
    			return OMXJPEG_ERROR_MEMORY;
    		}
    	decoder->ppInputBufferHeader[i]->nFilledLen = thisBufsize;
    }

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Enabling input port\n");
    // wait for port enable to complete - which it should once buffers are
    // assigned
    int ret =
	ilclient_wait_for_event(decoder->imageDecoder->component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				decoder->imageDecoder->inPort, ILCLIENT_PORT_ENABLED,
				0, TIMEOUT_MS);
    if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Input port enabled failed: %d\n", ret);
		return OMXJPEG_ERROR_EXECUTING;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Input port enabled.\n");
    }

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Starting image decoder ...\n");
	// start executing the decoder
	ret = OMX_SendCommand(decoder->imageDecoder->handle,
			  OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed starting image decoder: %x\n", ret);
		return OMXJPEG_ERROR_EXECUTING;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Executing started.\n");
	}

	ret = ilclient_wait_for_event(decoder->imageDecoder->component,
				  OMX_EventCmdComplete,
				  OMX_CommandStateSet, 0, OMX_StateExecuting, 0, ILCLIENT_STATE_CHANGED,
				  TIMEOUT_MS);
	if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Did not receive executing state %d\n", ret);
		return OMXJPEG_ERROR_EXECUTING;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Confirmed executing\n");
	}

    return OMXJPEG_OK;
}

// this function run the boilerplate to setup the openmax components;
int
setupOpenMaxJpegDecoder(OPENMAX_JPEG_DECODER ** pDecoder)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: setupOpenMaxJpegDecoder()\n");

    *pDecoder = malloc(sizeof(OPENMAX_JPEG_DECODER));
    if (pDecoder[0] == NULL) {
		perror("malloc decoder");
		return OMXJPEG_ERROR_MEMORY;
    }
    memset(*pDecoder, 0, sizeof(OPENMAX_JPEG_DECODER));

    if ((pDecoder[0]->client = ilclient_init()) == NULL) {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to init ilclient\n");
		return OMXJPEG_ERROR_ILCLIENT_INIT;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: ilclient loaded.\n");
    }

    ilclient_set_error_callback(pDecoder[0]->client,
 			       error_callback,
 			       NULL);
    ilclient_set_eos_callback(pDecoder[0]->client,
 			     eos_callback,
 			     NULL);
    ilclient_set_fill_buffer_done_callback(pDecoder[0]->client,
    		FillBufferDoneCB, *pDecoder);
    ilclient_set_empty_buffer_done_callback(pDecoder[0]->client,
    		EmptyBufferDoneCB, *pDecoder);

    if (OMX_Init() != OMX_ErrorNone) {
		ilclient_destroy(pDecoder[0]->client);
		perror("OMX_Init");
		return OMXJPEG_ERROR_OMX_INIT;
    }

    // prepare the image decoder
    int ret = prepareImageDecoder(pDecoder[0]);
    if (ret != OMXJPEG_OK)
	return ret;
    
    ret = startupImageDecoder(pDecoder[0]);
    if (ret != OMXJPEG_OK)
	return ret;

    return OMXJPEG_OK;
}

// this function passed the jpeg image buffer in, and returns the decoded
// image
int
decodeImage(OPENMAX_JPEG_DECODER * decoder, char *sourceImage,
	    size_t imageSize)
{
    decoder->ibToRead += imageSize;
    decoder->ibIndex = 0;
    decoder->obHeader = NULL;
    decoder->obGotEOS = 0;

    int ret;
    int outputPortConfigured = 0;

    EmptyBufferDoneCB(decoder,NULL);

    //TODO Should use proper semaphores here
    //TODO: handle case when we never get EOS
    //TODO: handle global abort flag if OMX or ilclient throws something
    while (decoder->ibToRead > 0 || decoder->obGotEOS == 0) {
		if(!outputPortConfigured)
		{
			ret =
				ilclient_wait_for_event
				(decoder->imageDecoder->component,
				 OMX_EventPortSettingsChanged,
				 decoder->imageDecoder->outPort, 0, 0, 1,
				 0, 0);

			if (ret == 0) {
				ret = portSettingsChanged(decoder);
				outputPortConfigured = 1;
				int ret = OMX_FillThisBuffer(decoder->imageDecoder->handle,
						decoder->obHeader);
				if (ret != OMX_ErrorNone) {
					pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error in FillThisBuffer: %s",OMX_errString(ret));
					return OMX_ErrorUndefined;
				}
			}
		}
    }

    return OMXJPEG_OK;
}

// this function cleans up the decoder.
void
cleanup(OPENMAX_JPEG_DECODER * decoder)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: cleanup()\n");

	int ret;

	if(decoder == NULL){
		pis_logMessage(PIS_LOGLEVEL_ALL, "JPEG Decoder: No decoder structure\n");
		return;
	}

	if(decoder->imageDecoder == NULL){
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: No imageDecoder\n");
		return;
	}

	if(decoder->imageDecoder->handle == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: No imageDecoder->handle\n");
		return;
	}

	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Continuing cleanup\n");

	// flush everything through
	ret = OMX_SendCommand(decoder->imageDecoder->handle,
			OMX_CommandFlush, decoder->imageDecoder->outPort,
			NULL);
	if(ret != OMX_ErrorNone)
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error flushing decoder commands: %s\n", OMX_errString(ret));
	ret = ilclient_wait_for_event(decoder->imageDecoder->component,
				OMX_EventCmdComplete, OMX_CommandFlush, 0,
				decoder->imageDecoder->outPort, 0, ILCLIENT_PORT_FLUSH,
				TIMEOUT_MS);
	if(ret != 0)
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error flushing decoder commands: %d\n", ret);
	else
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Decoder commands flushed\n");


    ret = OMX_SendCommand(decoder->imageDecoder->handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error transitioning to idle: %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component transitioning to idle\n");

    //Once ports are disabled the component will go to idle
    ret = ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateIdle, 0, ILCLIENT_STATE_CHANGED, TIMEOUT_MS);

    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component did not enter Idle state: %d\n",ret);
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component transitioned to idle\n",ret);

    ret = OMX_SendCommand(decoder->imageDecoder->handle, OMX_CommandPortDisable,
		    decoder->imageDecoder->inPort, NULL);
    if(ret != 0)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error disabling image decoder input port: %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Input port disabling\n");

    for(int i = 0;i<decoder->inputBufferCount;i++){
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freeing buffer header %d\n",i);
    	ret = OMX_FreeBuffer(decoder->imageDecoder->handle,
    			decoder->imageDecoder->inPort, decoder->ppInputBufferHeader[i]);
        if(ret != 0)
        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error freeing input buffer %d: %s\n", i, OMX_errString(ret));
        else
        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freed input buffer %d\n", i);
    }

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freed input buffers\n");

    if(decoder->ppInputBufferHeader != NULL)
    	free(decoder->ppInputBufferHeader);
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: No input buffer header to free.\n");

    ret = ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, decoder->imageDecoder->inPort, 0, ILCLIENT_PORT_DISABLED,
			    TIMEOUT_MS);
    if(ret != 0)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error disabling input port %d\n", ret);
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Input port confirmed disabled\n");

    ret = OMX_SendCommand(decoder->imageDecoder->handle, OMX_CommandPortDisable,
		    decoder->imageDecoder->outPort, NULL);
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error disabling output port %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Disabling output port\n");

    if(decoder->obHeader != NULL && decoder->obHeader->pBuffer != NULL){
    	vcos_free(decoder->obHeader->pBuffer);
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freed output pBuffer\n");
    }else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error freeing temp->pBuffer\n");

    ret = OMX_FreeBuffer(decoder->imageDecoder->handle,
		   decoder->imageDecoder->outPort,
		   decoder->obHeader);
    if(ret != OMX_ErrorNone)
        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error freeing output port buffer: %s\n", OMX_errString(ret));

    ret =  ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, decoder->imageDecoder->outPort, 0, ILCLIENT_PORT_DISABLED,
			    TIMEOUT_MS);
    if(ret != 0) pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Output port never disabled %d\n",ret);





    //Change the components state to loaded. the ilclient will also wait to confirm the event
    ret = ilclient_change_component_state(decoder->imageDecoder->component,
				    OMX_StateLoaded);
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component did not enter Idle state: %d\n",ret);

    COMPONENT_T  *list[2];
    list[0] = decoder->imageDecoder->component;
    list[1] = (COMPONENT_T  *)NULL;
    ilclient_cleanup_components(list);

    ret = OMX_Deinit();
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component did not enter Idle state: %d\n",ret);

    if (decoder->client != NULL) {
    	ilclient_destroy(decoder->client);
    }
}

sImage *decodeJpgImage(char *img)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: decodeJpgImage()\n");

	pDecoder = NULL;


    char           *sourceImage = NULL;
    size_t          imageSize;
    int             s;

    pis_jpegdecoder.decodedAt = 0;

    FILE           *fp = fopen(img, "rb");
    if (!fp) {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: File %s not found.\n", img);
    	return NULL;
    }


    fseek(fp, 0L, SEEK_END);
    imageSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);


    sourceImage = malloc(imageSize);
    if(sourceImage == NULL)
    {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to allocate buffer for sourceImage\n", img);
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Allocated input image buffer.\n");
    }

    assert(sourceImage != NULL);
    s = fread(sourceImage, 1, imageSize, fp);

    if(s != imageSize){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to read image file into memory\n");
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Image file read into memory.\n");
    }

    pis_jpegdecoder.srcSize = imageSize;
    pis_jpegdecoder.inputBuf = (uint8_t *)sourceImage;

    fclose(fp);
    bcm_host_init();

    s = setupOpenMaxJpegDecoder(&pDecoder);
    if(s != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to setup OpenMaxJpegDecoder.\n");
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Set up decoder successful.\n");
    }

    s = decodeImage(pDecoder, sourceImage, imageSize);

    if(s != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Decode image failed: %s.\n", OMX_errString(s));
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Image decoder succeeded, cleaning up.\n");
    }


    cleanup(pDecoder);
    free(pDecoder);
    free(sourceImage);

	sImage *ret = malloc(sizeof(sImage));
	if(ret == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to allocate sImage ret structure.\n");
		free(ret);
		return NULL;
	}

	//TODO: get colorSpace from the port settings
	ret->colorSpace = OMX_COLOR_FormatYUV420PackedPlanar;
	ret->imageBuf = pis_jpegdecoder.imgBuf;
	ret->imageSize = pis_jpegdecoder.decodedAt;
	ret->imageWidth = pis_jpegdecoder.width;
	ret->imageHeight = pis_jpegdecoder.height;
	ret->stride = pis_jpegdecoder.stride;
	ret->sliceHeight = pis_jpegdecoder.sliceHeight;

	pis_logMessage(PIS_LOGLEVEL_ALL, "JPEG Decoder: Returning successfully.\n");
	return ret;

    error:

	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Exiting with error.\n");

	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Cleaning up pDecoder\n");
    cleanup(pDecoder);

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freeing pDecoder\n");
    if(pDecoder != NULL)
    	free(pDecoder);

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freeing sourceImage\n");
    if(sourceImage != NULL)
    	free(sourceImage);

    return NULL;

}

