#include "stdio.h"
#include <malloc.h>

extern "C"
{
#include "bcm_host.h"
#include "ilclient/ilclient.h"
}

#include "piresizer.h"
#include "pijpegdecoder.h"
#include "tricks.h"
#include "../PiSignageLogging.h"

//use with nodeID OMX_ALL to start, not getting much data from jpegs (?)
void printChildNodes(uint32_t nodeID, OMX_HANDLETYPE handle)
{
	int ret2;

	OMX_CONFIG_CONTAINERNODECOUNTTYPE containerCount;

	containerCount.nSize = sizeof(OMX_CONFIG_CONTAINERNODECOUNTTYPE);
	containerCount.nVersion.nVersion = OMX_VERSION;
	containerCount.nParentNodeID = nodeID;

	ret2 = OMX_GetConfig(handle,OMX_IndexConfigContainerNodeCount,&containerCount);
    if(ret2 != OMX_ErrorNone)
        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: meta3: %s\n", OMX_errString(ret2));

	printf("Num nodes: %d\n",containerCount.nNumNodes);

	OMX_CONFIG_METADATAITEMTYPE metadataItem;
	OMX_CONFIG_CONTAINERNODEIDTYPE node;
	for(uint32_t i = 0;i<containerCount.nNumNodes;i++){
		node.nNodeIndex = i;
		node.nSize = sizeof(OMX_CONFIG_CONTAINERNODEIDTYPE);
		node.nVersion.nVersion = OMX_VERSION;
		node.nParentNodeID = nodeID;

		ret2 = OMX_GetConfig(handle,OMX_IndexConfigCounterNodeID,&node);

	    if(ret2 != OMX_ErrorNone)
	        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: meta4: %s\n", OMX_errString(ret2));

	    printf("Node Name: %s\n",node.cNodeName);

	    if(node.bIsLeafType == OMX_TRUE){
			metadataItem.eScopeMode = OMX_MetadataScopeNodeLevel;
			metadataItem.eSearchMode = OMX_MetadataSearchItemByIndex;
			metadataItem.nMetadataItemIndex = node.nNodeID;

			metadataItem.nSize = sizeof(OMX_CONFIG_METADATAITEMTYPE);
			metadataItem.nVersion.nVersion = OMX_VERSION;
			ret2 = OMX_GetConfig(handle,OMX_IndexConfigMetadataItem,&metadataItem);

		    if(ret2 != OMX_ErrorNone)
		        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: meta5: %s\n", OMX_errString(ret2));

			printf("Metadata value: %s=%s\n",metadataItem.nKey, metadataItem.nValue);
	    }else{
	    	printf("Printing children of: %d\n",node.nNodeID);
	    	printChildNodes(node.nNodeID,handle);
	    }
	}
}

PiImageDecoder::PiImageDecoder()
{
    //TODO: Need a state variable for if everything is loaded and ready to decode or cleaned up

    component = NULL;
    handle = NULL;
    inPort = 0;
    outPort = 0;

    client = NULL;

    ibIndex = 0;
    ibToRead = 0;
    ibBufferCount = 0;
    ibBufferHeader = 0;

    obHeader = NULL;
    obGotEOS = 0;
    obDecodedAt = 0;

	imgBuf = NULL;
	inputBuf = NULL;
	width = 0;
	height = 0;
	srcSize = 0;

	colorSpace = 0;
	stride = 0;
	sliceHeight = 0;

	omxError = 0;
}

PiImageDecoder::~PiImageDecoder()
{

}

void PiImageDecoder::EmptyBufferDoneCB(
		void *data,
		COMPONENT_T *comp)
{

	PiImageDecoder *decoder = (PiImageDecoder *) data;

	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: EmptyBufferDoneCB()\n");
	//Fire off the next buffer fill

	//Protect callbacks from race conditions with cleanup code
	if(decoder->omxError != 0){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error detected, aborting EmptyBufferDoneCB\n");
		return;
	}

	if(decoder == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error EmptyBufferDoneCB no pDecoder\n");
		//TODO abort the decoding process
		return;
	}

	if(decoder->ibToRead > 0)
	{
		if(decoder->ibIndex >= decoder->ibBufferCount)
		{
			pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error accessing input buffers\n");
			return;
		}else{
			pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: Using input buffer %d\n",decoder->ibIndex);
		}

		// get next buffer from array
		OMX_BUFFERHEADERTYPE *pBufHeader =
				decoder->ibBufferHeader[decoder->ibIndex];

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
			OMX_EmptyThisBuffer(decoder->handle,
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

void PiImageDecoder::FillBufferDoneCB(
  void* data,
  COMPONENT_T *comp)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: FillBufferDoneCB()\n");

	PiImageDecoder *decoder = (PiImageDecoder *) data;

	//Protect callbacks from race conditions with cleanup code
	if(decoder->omxError != 0){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error detected, aborting Fill BufferDoneCB\n");
		return;
	}

	//TODO: output directly to the image buffer
	if(decoder->obHeader != NULL){
		pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: More data put in the buffer\n");

		if(decoder->obHeader->nFilledLen + decoder->obDecodedAt > decoder->obHeader->nAllocLen){
			pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: ERROR overrun of decoded image buffer\n %d %d %d\n",
					decoder->obHeader->nFilledLen, decoder->obDecodedAt, decoder->obHeader->nAllocLen);
		}else{
			//Used to copy into another buffer

			//Copy output port buffer to output image buffer
			//memcpy(&decoder->imgBuf[decoder->obDecodedAt],
	    	//	decoder->obHeader->pBuffer + decoder->obHeader->nOffset,decoder->obHeader->nFilledLen);
			//pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Filled output buf from %d to %d\n",
			//		decoder->obDecodedAt, decoder->obHeader->nFilledLen + decoder->obDecodedAt);
		}

	    decoder->obDecodedAt += decoder->obHeader->nFilledLen;

	    //See if we've reached the end of the stream
	    if (decoder->obHeader->nFlags & OMX_BUFFERFLAG_EOS) {
	    	pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: Output buffer EOS received\n");
	    	decoder->obGotEOS = 1;
	    }else{
	    	//Not sure if this is a valid case with only one buffer
	    	pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: Output buffer asking for more data\n");
			int ret = OMX_FillThisBuffer(decoder->handle,
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


void PiImageDecoder::error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: OMX error %s\n",OMX_errString(data));
	((PiImageDecoder*)userdata)->omxError = 1;
}

//Called from decodeImage once the decoder has read the file header and
//changed the output port settings for the image
int PiImageDecoder::portSettingsChanged()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: portSettingsChanged()\n");

	//Protect callbacks from race conditions with cleanup code
	if(omxError != 0){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error detected, aborting portSettingsChanged\n");
		return -1;
	}

    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    //Allocates the buffers and enables the port
    int ret;

    // Get the image dimensions
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = outPort;
    OMX_GetParameter(handle,
		     OMX_IndexParamPortDefinition, &portdef);

    width = portdef.format.image.nFrameWidth;
    height = portdef.format.image.nFrameHeight;
    colorSpace = portdef.format.image.eColorFormat;
    stride = portdef.format.image.nStride;
    sliceHeight = portdef.format.image.nSliceHeight;

    pis_logMessage(PIS_LOGLEVEL_INFO,"JPEG Decoder: Image: %dx%d Stride: %d, Slice Height: %d\n",
    		portdef.format.image.nFrameWidth,portdef.format.image.nFrameHeight, portdef.format.image.nStride, portdef.format.image.nSliceHeight);

    // enable the port and setup the buffers
    OMX_SendCommand(handle, OMX_CommandPortEnable, outPort, NULL);
    //TODO Error handle
    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Enabling out port\n");

    //Output port buffer size is always the full image size
    imgBuf = new uint8_t[portdef.nBufferSize];
    if(imgBuf == NULL)
    {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error allocating output image buffer.\n");
    	//TODO return error
    	return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Allocated output image buffer %d bytes\n", portdef.nBufferSize);
    }

    ret = OMX_UseBuffer(handle, &obHeader, outPort, NULL, portdef.nBufferSize, imgBuf);
    if(ret != OMX_ErrorNone){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: error assigning output port buffer: %s\n",OMX_errString(ret));
    	return -1;
    }
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Output port buffers assigned\n");

    // wait for port enable to complete - which it should once buffers are
    // assigned
    ret =
	ilclient_wait_for_event(component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				outPort, ILCLIENT_PORT_ENABLED,
				0, TIMEOUT_MS);
    if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Output port enabled failed: %d\n", ret);
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Output port enabled.\n");
    }

    //Ehhh, not sure you ever really want to see this. Maybe LOGLEVEL_ALL_KITCHEN_SINK(?)
    //printPort(decoder->imageDecoder->handle,decoder->imageDecoder->outPort);

    //printChildNodes(OMX_ALL, handle);

    return 0;
}

int PiImageDecoder::prepareImageDecoder()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "JPEG Decoder: prepareImageDecoder()\n");

    int ret = ilclient_create_component(client,
		&component,
		(char*)"image_decode",
		(ILCLIENT_CREATE_FLAGS_T)(
		ILCLIENT_DISABLE_ALL_PORTS
		|
		ILCLIENT_ENABLE_INPUT_BUFFERS
		|
		ILCLIENT_ENABLE_OUTPUT_BUFFERS
		));

    if (ret != 0) {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error creating component: %d\n",ret);
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Created the component.\n");
    }


    // grab the handle for later use in OMX calls directly
    handle = ILC_GET_HANDLE(component);

    // get and store the ports
    OMX_PORT_PARAM_TYPE port;
    port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;

    OMX_GetParameter(handle,
		     OMX_IndexParamImageInit, &port);
    if (port.nPorts != 2) {
    	return -1;
    }

    inPort = port.nStartPortNumber;
    outPort = port.nStartPortNumber + 1;

    return 0;
}

int PiImageDecoder::startupImageDecoder()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: startupImageDecoder()\n");

    // move to idle
    if(ilclient_change_component_state(component, OMX_StateIdle) != 0)
    	printf("startupImageDecoder: Failed to transition to idle.\n");

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Setting port image format\n");
    // set input image format
    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
    imagePortFormat.nVersion.nVersion = OMX_VERSION;
    imagePortFormat.nPortIndex = inPort;
    imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    OMX_SetParameter(handle,
		     OMX_IndexParamImagePortFormat, &imagePortFormat);

    // get buffer requirements
    OMX_PARAM_PORTDEFINITIONTYPE portdef;
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = inPort;
    OMX_GetParameter(handle,
		     OMX_IndexParamPortDefinition, &portdef);

    uint32_t bufSize = portdef.nBufferSize;

    //First buffer has to be short, probably for it to read
    //the file header, rest of the file is in the second and third buffer
    //Tried smaller chunks, doesn't seem to like more than 16 input buffers??
    //TODO Only works for 2 or 3 buffers, not a clue why. Math seems sound
    ibBufferCount = 3;
    portdef.nBufferCountActual = 3;

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Setting port parameters\n");
    OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &portdef);

    // enable the port and setup the buffers
    OMX_SendCommand(handle, OMX_CommandPortEnable, inPort, NULL);
    //TODO Error handle
    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Enabling input port\n");


    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Initializing input buffers\n");
    ibBufferHeader = new OMX_BUFFERHEADERTYPE*[ibBufferCount];
    uint32_t thisBufsize;
    for(int i = 0; i < ibBufferCount; i++){

    	//If this is the last buffer make sure it all fits
    	thisBufsize = i == ibBufferCount-1 ? srcSize - i*bufSize : bufSize;

    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Allocating buffer of %d\n",thisBufsize);

        int ret = OMX_UseBuffer(handle,
    					   &ibBufferHeader[i],
    					   inPort,
    					   (void *) NULL,
						   thisBufsize,
    					   inputBuf+i*bufSize);
    	if(ret != OMX_ErrorNone) {
    			printf("Allocate resize input buffer, err: %x\n",ret);
    			return -1;
    		}
    	ibBufferHeader[i]->nFilledLen = thisBufsize;
    }


    // wait for port enable to complete - which it should once buffers are
    // assigned
    int ret =
	ilclient_wait_for_event(component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				inPort, ILCLIENT_PORT_ENABLED,
				0, TIMEOUT_MS);
    if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Input port enabled failed: %d\n", ret);
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Input port enabled.\n");
    }

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Starting image decoder ...\n");
	// start executing the decoder
	ret = OMX_SendCommand(handle,
			  OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed starting image decoder: %x\n", ret);
		return -1;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Executing started.\n");
	}

	ret = ilclient_wait_for_event(component,
				  OMX_EventCmdComplete,
				  OMX_CommandStateSet, 0, OMX_StateExecuting, 0, ILCLIENT_STATE_CHANGED,
				  TIMEOUT_MS);
	if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Did not receive executing state %d\n", ret);
		return -1;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Confirmed executing\n");
	}

    return 0;
}

// this function run the boilerplate to setup the openmax components;
int PiImageDecoder::setupOpenMaxJpegDecoder()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: setupOpenMaxJpegDecoder()\n");

    if ((client = ilclient_init()) == NULL) {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to init ilclient\n");
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: ilclient loaded.\n");
    }

    ilclient_set_error_callback(client, error_callback, this);
    //ilclient_set_eos_callback(client, eos_callback, NULL);
    ilclient_set_fill_buffer_done_callback(
    		client, FillBufferDoneCB, this);
    ilclient_set_empty_buffer_done_callback(
    		client, EmptyBufferDoneCB, this);

    if (OMX_Init() != OMX_ErrorNone) {
		ilclient_destroy(client);
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to init OMX\n");
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: OMX Initialized\n");
    }

    // prepare the image decoder
    int ret = prepareImageDecoder();
    if (ret != 0)
    {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed at prepareImageDecoder %d\n", ret);
    	return ret;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: prepareImageDecoder suceeded\n");
    }
    
    ret = startupImageDecoder();
    if (ret != 0)
    {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed at startupImageDecoder %d\n", ret);
    	return ret;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: startupImageDecoder suceeded\n");
    }

    return 0;
}

// this function passed the jpeg image buffer in, and returns the decoded
// image
int PiImageDecoder::decodeImage()
{
    ibToRead = srcSize;
    ibIndex = 0;
    obHeader = NULL;
    obGotEOS = 0;

    int ret;
    int outputPortConfigured = 0;

    EmptyBufferDoneCB(this,NULL);

    //TODO Should use proper semaphores here
    //TODO: handle case when we never get EOS (timeout?)
    while ((ibToRead > 0 || obGotEOS == 0) && omxError == 0) {

    	//TODO: See if this can get moved to a callback
		if(!outputPortConfigured)
		{
			ret =
				ilclient_wait_for_event
				(component,
				 OMX_EventPortSettingsChanged,
				 outPort, 0, 0, 1,
				 0, 0);

			if (ret == 0) {
				ret = portSettingsChanged();
				outputPortConfigured = 1;
				int ret = OMX_FillThisBuffer(handle, obHeader);
				if (ret != OMX_ErrorNone) {
					pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Error in FillThisBuffer: %s\n",OMX_errString(ret));
					return OMX_ErrorUndefined;
				}
			}
		}
    }

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Finished decoding\n");

    return 0;
}

// this function cleans up the decoder.
// Goal is that no matter what happened to return the
// component to a state to accept a new image
void PiImageDecoder::cleanup()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: cleanup()\n");

	int ret;

	if(handle == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: No imageDecoder->handle\n");
		return;
	}

	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Continuing cleanup\n");

	// flush everything through
	ret = OMX_SendCommand(handle, OMX_CommandFlush, outPort, NULL);
	if(ret != OMX_ErrorNone)
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error flushing decoder commands: %s\n", OMX_errString(ret));
	ret = ilclient_wait_for_event(component,
				OMX_EventCmdComplete, OMX_CommandFlush, 0,
				outPort, 0, ILCLIENT_PORT_FLUSH,
				TIMEOUT_MS);
	if(ret != 0)
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error flushing decoder commands: %d\n", ret);
	else
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Decoder commands flushed\n");


    ret = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error transitioning to idle: %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component transitioning to idle\n");

    //Once ports are disabled the component will go to idle
    ret = ilclient_wait_for_event(component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateIdle, 0, ILCLIENT_STATE_CHANGED, TIMEOUT_MS);

    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component did not enter Idle state: %d\n",ret);
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component transitioned to idle\n",ret);

    ret = OMX_SendCommand(handle, OMX_CommandPortDisable, inPort, NULL);
    if(ret != 0)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error disabling image decoder input port: %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Input port disabling\n");

    //TODO: Whith buffer >3 it seems soemthing happens to the input buffer headers and faults
    for(int i = 0;i<ibBufferCount;i++){
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freeing buffer header %d\n",i);
    	ret = OMX_FreeBuffer(handle, inPort, ibBufferHeader[i]);
        if(ret != 0)
        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error freeing input buffer %d: %s\n", i, OMX_errString(ret));
        else
        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freed input buffer %d\n", i);
    }

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freed input buffers and headers\n");

    ret = ilclient_wait_for_event(component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, inPort, 0, ILCLIENT_PORT_DISABLED,
			    TIMEOUT_MS);
    if(ret != 0)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error disabling input port %d\n", ret);
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Input port confirmed disabled\n");

    ret = OMX_SendCommand(handle, OMX_CommandPortDisable, outPort, NULL);
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error disabling output port %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Disabling output port\n");


    ret = OMX_FreeBuffer(handle,outPort, obHeader);
    obHeader = NULL;
    if(ret != OMX_ErrorNone)
        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error freeing output port buffer: %s\n", OMX_errString(ret));

    ret =  ilclient_wait_for_event(component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, outPort, 0, ILCLIENT_PORT_DISABLED,
			    TIMEOUT_MS);
    if(ret != 0) pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Output port never disabled %d\n",ret);

    ret = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);

    if(ret != OMX_ErrorNone)
        	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Error moving to loaded state: %s\n", OMX_errString(ret));

    ret =  ilclient_wait_for_event(component,
			    OMX_EventCmdComplete, OMX_CommandStateSet,
			    0, outPort, 0, ILCLIENT_STATE_CHANGED,
			    TIMEOUT_MS);
    if(ret != 0) pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: State never changed to loaded %d\n",ret);


    COMPONENT_T  *list[2];
    list[0] = component;
    list[1] = (COMPONENT_T  *)NULL;
    ilclient_cleanup_components(list);

    ret = OMX_Deinit();
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Component did not enter Deinit(): %d\n",ret);

    if (client != NULL) {
    	ilclient_destroy(client);
    }
}

int PiImageDecoder::DecodeJpegImage(const char *img, sImage **ret)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"JPEG Decoder: decodeJpgImage()\n");

    uint32_t s;

    obDecodedAt = 0;
    omxError = 0;

    FILE           *fp = fopen(img, "rb");
    if (!fp) {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: File %s not found.\n", img);
    	return -1;
    }

    fseek(fp, 0L, SEEK_END);
    srcSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    inputBuf = new uint8_t[srcSize];
    if(inputBuf == NULL)
    {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to allocate buffer for sourceImage\n", img);
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Allocated input image buffer.\n");
    }

    s = (uint32_t)fread(inputBuf, 1, srcSize, fp);

    if(s != srcSize){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to read image file %s into memory %d %d\n",img,s, srcSize);
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Image file read into memory.\n");
    }

    fclose(fp);
    bcm_host_init();

    s = setupOpenMaxJpegDecoder();
    if(s != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to setup OpenMaxJpegDecoder.\n");
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Set up decoder successful.\n");
    }

    s = decodeImage();

    if(s != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Decode image failed: %s.\n", OMX_errString(s));
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Image decoder succeeded, cleaning up.\n");
    }

    if(omxError != 0)
    {
    	goto error;
    }

    cleanup();
    free(inputBuf);
    inputBuf = NULL;

	*ret = new sImage;
	if(ret == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Failed to allocate sImage ret structure.\n");
		free(ret);
		return -1;
	}

	//TODO: get colorSpace from the port setting
	(*ret)->colorSpace = OMX_COLOR_FormatYUV420PackedPlanar;
	(*ret)->imageBuf = imgBuf;
	(*ret)->imageSize = obDecodedAt;
	(*ret)->imageWidth = width;
	(*ret)->imageHeight = height;
	(*ret)->stride = stride;
	(*ret)->sliceHeight = sliceHeight;

	pis_logMessage(PIS_LOGLEVEL_ALL, "JPEG Decoder: Returning successfully.\n");
	return 0;

    error:

	if(imgBuf != NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Cleaning up imgBuf\n");
		free(imgBuf);
	}

	pis_logMessage(PIS_LOGLEVEL_ERROR,"JPEG Decoder: Exiting with error.\n");

	pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Cleaning up pDecoder\n");
    cleanup();

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freeing pDecoder\n");
    //Zero state(?)

    pis_logMessage(PIS_LOGLEVEL_ALL,"JPEG Decoder: Freeing sourceImage\n");
    if(inputBuf != NULL)
    	free(inputBuf);

    return -1;

}

