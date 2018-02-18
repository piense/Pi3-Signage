#include "stdio.h"
#include <malloc.h>

extern "C"
{
#include "bcm_host.h"
#include "ilclient/ilclient.h"
}

#include "piImageResizer.h"
#include "tricks.h"
#include "piSlideTypes.h"
#include "../PiSignageLogging.h"

PiImageResizer::PiImageResizer(){
	pis_logMessage(PIS_LOGLEVEL_ALL,"New Resizer\n");

    component = NULL;
    handle = NULL;
    inPort = 0;
    outPort = 0;

    client = NULL;

    //Input stuff
	imgBuf = NULL;
	srcWidth = 0;
	srcHeight = 0;
	srcSize = 0;
	srcStride = 0;
	srcSliceHeight = 0;
	srcColorSpace = OMX_COLOR_FormatUnused;
	ibBufferHeader = NULL;

	cropTop = 0;
	cropLeft = 0;
	cropWidth = 0;
	cropHeight = 0;

    //Output Buffer stuff
    obHeader = NULL;
    obGotEOS = 0;
    obDecodedAt = 0 ;
    outputWidth = 0;
    outputHeight = 0;
    outputStride = 0;
    outputSliceHeight = 0;
    outputPic = NULL;
    outputColorSpace = 0;
}

PiImageResizer::~PiImageResizer(){
	pis_logMessage(PIS_LOGLEVEL_ALL,"New Resizer\n");
}

void PiImageResizer::EmptyBufferDoneCB(
		void *data,
		COMPONENT_T *comp)
{
	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Done with input buffer\n");
}

void PiImageResizer::FillBufferDoneCB(
  void* data,
  COMPONENT_T *comp)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"Resizer: FillBufferDoneCB()\n");

	PiImageResizer *decoder = (PiImageResizer *) data;

	//TODO: output directly to the image buffer
	if(decoder->obHeader != NULL){
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Processing received buffer\n");

		if(decoder->obHeader->nFilledLen + decoder->obDecodedAt > decoder->obHeader->nAllocLen){
			pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: ERROR overrun of decoded image buffer\n %d %d %d\n",
					decoder->obHeader->nFilledLen, decoder->obDecodedAt, decoder->obHeader->nAllocLen);
		}else{
			//Copy output port buffer to output image buffer
			//memcpy(&decoder->outputPic[decoder->obDecodedAt],
	    	//	decoder->obHeader->pBuffer + decoder->obHeader->nOffset,decoder->obHeader->nFilledLen);
		}

	    decoder->obDecodedAt += decoder->obHeader->nFilledLen;

	    //See if we've reached the end of the stream
	    if (decoder->obHeader->nFlags & OMX_BUFFERFLAG_EOS) {
	    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Output buffer EOS received\n");
	    	decoder->obGotEOS = 1;
	    }else{
	    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Output buffer asking for more data\n");
			int ret = OMX_FillThisBuffer(decoder->handle,
					decoder->obHeader);
			if (ret != OMX_ErrorNone) {
				pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error in FillThisBuffer: %s",OMX_errString(ret));
				return;
			}
	    }
	}else{
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error: No obHeader\n");
	}
}


void PiImageResizer::error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: OMX error %s\n",OMX_errString(data));
}

//Called from decodeImage once the decoder has read the file header and
//changed the output port settings for the image
int PiImageResizer::portSettingsChanged()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"Resizer: portSettingsChanged()\n");

    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    // Get the image dimensions
    //TODO: Store color format too
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = outPort;
    OMX_GetParameter(handle,
		     OMX_IndexParamPortDefinition, &portdef);

    portdef.format.image.eColorFormat = OMX_COLOR_Format32bitARGB8888;

    portdef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portdef.format.image.bFlagErrorConcealment = OMX_FALSE;
    portdef.format.image.nFrameWidth = outputWidth;
    portdef.format.image.nFrameHeight = outputHeight;
    portdef.format.image.nStride = ALIGN_UP(outputWidth,16)*4;
	portdef.format.image.nSliceHeight = 0;

    int ret = OMX_SetParameter(handle,
		     OMX_IndexParamPortDefinition, &portdef);

    if(ret != OMX_ErrorNone)
        	pis_logMessage(PIS_LOGLEVEL_ERROR, "portSettingsChanged1: Error %x enabling buffers.\n",ret);

    OMX_GetParameter(handle,
		     OMX_IndexParamPortDefinition, &portdef);

    outputStride = portdef.format.image.nStride;
    outputSliceHeight = portdef.format.image.nSliceHeight;
    outputColorSpace = portdef.format.image.eColorFormat;

    pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Resizing to: %dx%d\n",
    		portdef.format.image.nFrameWidth, portdef.format.image.nFrameHeight);

    // enable the port and setup the buffers
    OMX_SendCommand(handle, OMX_CommandPortEnable, outPort, NULL);
    //TODO Error handle
    pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Enabling out port\n");

    outputPic = new uint8_t[portdef.nBufferSize];
	if(outputPic == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Failed to allocated output image.\n");
		return -1;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Allocated output port buffer of: %d with stride %d.\n", portdef.nBufferSize, outputStride);
	}

    ret = OMX_UseBuffer(handle, &obHeader, outPort, NULL, portdef.nBufferSize, outputPic);
    if(ret != OMX_ErrorNone){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: error assigning output port buffer: %s\n",OMX_errString(ret));
    	return -1;
    }
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Output port buffers assigned\n");


    // wait for port enable to complete - which it should once buffers are
    // assigned
    ret =
	ilclient_wait_for_event(component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				outPort, ILCLIENT_PORT_ENABLED,
				0, TIMEOUT_MS);
    if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Output port enabled failed: %d\n", ret);
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Output port enabled.\n");
    }

    //printOMXPort(handle,outPort);

    return 0;
}

int PiImageResizer::prepareImageResizer()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "Resizer: prepareImageResizer()\n");

    int ret = ilclient_create_component(client,
		&component,
		(char*)"resize",
		(ILCLIENT_CREATE_FLAGS_T)(
		ILCLIENT_DISABLE_ALL_PORTS
		|
		ILCLIENT_ENABLE_INPUT_BUFFERS
		|
		ILCLIENT_ENABLE_OUTPUT_BUFFERS
		));

    if (ret != 0) {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error creating component: %d\n",ret);
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Created the component.\n");
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

int PiImageResizer::startupImageResizer()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"Resizer: startupImageResizer()\n");

    // move to idle
    if(ilclient_change_component_state(component, OMX_StateIdle) != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Failed to transition to idle.\n");
    	return -1;
    }

    pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Setting port image format\n");
    // set input image format
    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
    imagePortFormat.nVersion.nVersion = OMX_VERSION;
    imagePortFormat.nPortIndex = inPort;
    imagePortFormat.eColorFormat = srcColorSpace;
    //TODO: Error handler for:
    int ret = OMX_SetParameter(handle,
		     OMX_IndexParamImagePortFormat, &imagePortFormat);

    if(ret != OMX_ErrorNone)
    {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error: Could not set input port color format: %s\n", OMX_errString(ret));
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Set input port color format.\n");
    }

    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = inPort;
    OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &portdef);
    //TODO Error handler & Info

    portdef.format.image.nFrameHeight = srcHeight;
    portdef.format.image.nFrameWidth = srcWidth;
    portdef.nBufferSize = srcSize;
    portdef.format.image.nStride = srcStride ;
    portdef.format.image.nSliceHeight = srcSliceHeight;

    portdef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portdef.format.image.eColorFormat = srcColorSpace;

    ret = OMX_SetParameter(handle,
		     OMX_IndexParamPortDefinition, &portdef);
    if(ret != OMX_ErrorNone){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error configuring input port %s.\n %d %d %d %d %d ",OMX_errString(ret),
    			srcHeight, srcWidth, srcSize, srcStride,srcSliceHeight);
    }


    OMX_CONFIG_RECTTYPE cropConfiguration;

    cropConfiguration.nSize = sizeof(OMX_CONFIG_RECTTYPE);
    cropConfiguration.nVersion.nVersion = OMX_VERSION;
    cropConfiguration.nPortIndex = inPort;
    cropConfiguration.nLeft = cropLeft;
    cropConfiguration.nTop = cropTop;
    cropConfiguration.nHeight = cropHeight;
    cropConfiguration.nWidth = cropWidth;
    ret = OMX_SetConfig(handle, OMX_IndexConfigCommonInputCrop, &cropConfiguration);
    if(ret != OMX_ErrorNone){
        	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error configuring cropping %s.\n ",OMX_errString(ret));
        	return -1;
        }

    // enable the input port
    ret = OMX_SendCommand(handle,
    		OMX_CommandPortEnable, inPort, NULL);
    if(ret != OMX_ErrorNone)
    {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error enabling input port %s.\n",OMX_errString(ret));
    	//TODO: Cleanup?
    	return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Input port enabling\n");
    }

    ret = OMX_UseBuffer(handle,
					   &ibBufferHeader,
					   inPort,
					   (void *) NULL,
					   srcSize,
					   imgBuf);
	if(ret != OMX_ErrorNone) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error using input buffer %s\n",OMX_errString(ret));
		return -1;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Created input port buffer, nFilledLen %d\n",srcSize);
	}
	ibBufferHeader->nFilledLen = srcSize;
	ibBufferHeader->nFlags = OMX_BUFFERFLAG_EOS;

    // wait for port enable to complete - which it should once buffers are
    // assigned
    ret =
	ilclient_wait_for_event(component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				inPort, ILCLIENT_PORT_ENABLED,
				0, TIMEOUT_MS);
    if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Input port enabled failed: %d\n", ret);
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Input port enabled.\n");
    }

    pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Starting image decoder ...\n");
	// start executing the decoder
	ret = OMX_SendCommand(handle,
			  OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Failed starting image decoder: %x\n", ret);
		return -1;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Executing started.\n");
	}

	ret = ilclient_wait_for_event(component,
				  OMX_EventCmdComplete,
				  OMX_CommandStateSet, 0, OMX_StateExecuting, 0, ILCLIENT_STATE_CHANGED,
				  TIMEOUT_MS);
	if (ret != 0) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Did not receive executing state %d\n", ret);
		return -1;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Confirmed executing\n");
	}

    return 0;
}

// this function run the boilerplate to setup the openmax components;
int PiImageResizer::setupOpenMaxImageResizer()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"Resizer: setupOpenMaxJpegDecoder()\n");

    if ((client = ilclient_init()) == NULL) {
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Failed to init ilclient\n");
		return -1;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: ilclient loaded.\n");
    }

    ilclient_set_error_callback(client, error_callback, this);
    //ilclient_set_eos_callback(client, eos_callback, NULL);
    ilclient_set_fill_buffer_done_callback(
    		client, FillBufferDoneCB, this);
    ilclient_set_empty_buffer_done_callback(
    		client, EmptyBufferDoneCB, this);

    if (OMX_Init() != OMX_ErrorNone) {
		ilclient_destroy(client);
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Failed to init OMX\n");
		return -1;
    }

    // prepare the image decoder
    int ret = prepareImageResizer();
    if (ret != 0)
	return ret;
    
    ret = startupImageResizer();
    if (ret != 0)
	return ret;

    return 0;
}



// this function passed the jpeg image buffer in, and returns the decoded
// image
int PiImageResizer::doResize()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"Resizer: doResize()\n");

    obHeader = NULL;
    obGotEOS = 0;

    int ret;
    int outputPortConfigured = 0;

    ret = OMX_EmptyThisBuffer(handle,ibBufferHeader);
	if (ret != OMX_ErrorNone) {
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error in OMX_EmptyThisBuffer: %s\n",OMX_errString(ret));
		return OMX_ErrorUndefined;
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Consuming the input buffer\n");
	}


    //TODO Should use proper semaphores here
    //TODO: handle case when we never get EOS (timeout?)
    //TODO: handle global abort flag if OMX or ilclient throws something
    while (obGotEOS == 0) {

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
				if(ret != 0){
					pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Port settings changed error.\n");
					//TODO: Something to handle th error
					return -1;
				}
				outputPortConfigured = 1;
				ret = OMX_FillThisBuffer(handle, obHeader);
				if (ret != OMX_ErrorNone) {
					pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Error in FillThisBuffer: %s\n",OMX_errString(ret));
					return OMX_ErrorUndefined;
				}else{
					pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Filling the output buffer\n");
				}
			}
		}
    }

    return 0;
}

// this function cleans up the decoder.
// Goal is that no matter what happened to return the
// component to a state to accept a new image
void PiImageResizer::cleanup()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"Resizer: cleanup()\n");

	int ret;

	if(handle == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: No imageDecoder->handle\n");
		return;
	}

	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Continuing cleanup\n");

	// flush everything through
	ret = OMX_SendCommand(handle, OMX_CommandFlush, outPort, NULL);
	if(ret != OMX_ErrorNone)
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Error flushing decoder commands: %s\n", OMX_errString(ret));
	ret = ilclient_wait_for_event(component,
				OMX_EventCmdComplete, OMX_CommandFlush, 0,
				outPort, 0, ILCLIENT_PORT_FLUSH,
				TIMEOUT_MS);
	if(ret != 0)
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Error flushing decoder commands: %d\n", ret);
	else
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Decoder commands flushed\n");


    ret = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Error transitioning to idle: %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Component transitioning to idle\n");

    //Once ports are disabled the component will go to idle
    ret = ilclient_wait_for_event(component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateIdle, 0, ILCLIENT_STATE_CHANGED, TIMEOUT_MS);

    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Component did not enter Idle state: %d\n",ret);
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Component transitioned to idle\n",ret);

    ret = OMX_SendCommand(handle, OMX_CommandPortDisable, inPort, NULL);
    if(ret != 0)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Error disabling image decoder input port: %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Input port disabling\n");

    ret = OMX_FreeBuffer(handle, inPort, ibBufferHeader);
	if(ret != 0)
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Error freeing input buffer: %s\n", OMX_errString(ret));
	else
		pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Freed input buffer.\n");

    ret = ilclient_wait_for_event(component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, inPort, 0, ILCLIENT_PORT_DISABLED,
			    TIMEOUT_MS);
    if(ret != 0)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Error disabling input port %d\n", ret);
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Input port confirmed disabled\n");

    ret = OMX_SendCommand(handle, OMX_CommandPortDisable, outPort, NULL);
    if(ret != OMX_ErrorNone)
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Error disabling output port %s\n", OMX_errString(ret));
    else
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Disabling output port\n");

    ret = OMX_FreeBuffer(handle,outPort, obHeader);
    obHeader = NULL;
    if(ret != OMX_ErrorNone)
        	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Error freeing output port buffer: %s\n", OMX_errString(ret));

    ret =  ilclient_wait_for_event(component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, outPort, 0, ILCLIENT_PORT_DISABLED,
			    TIMEOUT_MS);
    if(ret != 0) pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Output port never disabled %d\n",ret);

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
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Component did not enter Idle state: %d\n",ret);

    if (client != NULL) {
    	ilclient_destroy(client);
    }
}

//TODO: Issue size warnings when input or output is YUV:
//Doesn't like odd sizes on anything or input width not aligned to stride (maybe)
int PiImageResizer::ResizeImage(char *img,
		uint32_t width, uint32_t height, //pixels
		size_t size, //bytes
		OMX_COLOR_FORMATTYPE imgCoding,
		uint16_t stride,
		uint16_t sliceHeight,
		uint32_t maxOutputWidth,
		uint32_t maxOutputHeight,
		pis_mediaSizing scaling,
		sImage **ret
		)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"Resizer: resizeImage()\n");

    int             s;

    obDecodedAt = 0;
    outputPic = NULL;

	float inRatio, outRatio;

	pis_logMessage(PIS_LOGLEVEL_INFO, "Resizing: %dx%d to %dx%d\n",width,height,maxOutputWidth, maxOutputHeight);

	srcHeight = height;
	srcWidth = width;
	srcStride = stride;
	srcSliceHeight = sliceHeight;
	srcSize = size;
	srcColorSpace = imgCoding;

	//Kind of want a 1:1 mode but the renderer is supposed
	//to be very resolution independent
	switch(scaling)
	{
		case pis_SIZE_CROP:
			inRatio = ((float)srcWidth)/srcHeight;
			outRatio = ((float)maxOutputWidth)/maxOutputHeight;
			if(inRatio < outRatio)
			{
				outputWidth = maxOutputWidth;
				outputHeight = maxOutputHeight;
				cropTop = (height - ((float)width)/outRatio) / 2.0;
				cropLeft = 0;
				cropWidth = width;
				cropHeight = ((float)width)/outRatio;
			}else{
				outputHeight = maxOutputHeight;
				outputWidth = maxOutputWidth;
				cropTop = 0;
				cropLeft = (width - ((float)height)*outRatio) / 2.0;
				cropWidth = ((float)height)*outRatio;
				cropHeight = height;
			}
			pis_logMessage(PIS_LOGLEVEL_INFO,"Cropping. Src: %dx%d. New: %dx%d at %d,%d\n", width, height,
					cropWidth, cropHeight, cropLeft, cropTop);
			break;
		case pis_SIZE_SCALE:
			cropTop = 0;
			cropLeft = 0;
			cropWidth = width;
			cropHeight = height;
			inRatio = ((float)srcWidth)/srcHeight;
			outRatio = ((float)maxOutputWidth)/maxOutputHeight;
			if(inRatio < outRatio)
			{
				outputHeight = maxOutputHeight;
				outputWidth = ((float)maxOutputHeight)*inRatio;
			}else{
				outputWidth = maxOutputWidth;
				outputHeight = ((float)maxOutputWidth)/inRatio;
			}
			pis_logMessage(PIS_LOGLEVEL_INFO,"Scaling: %dx%d -> %dx%d\n",
					width, height, outputWidth,outputHeight);
			break;
		case pis_SIZE_STRETCH:
			cropTop = 0;
			cropLeft = 0;
			cropWidth = width;
			cropHeight = height;
			outputHeight = maxOutputHeight;
			outputWidth = maxOutputWidth;
			pis_logMessage(PIS_LOGLEVEL_INFO,"Stretching: %dx%d -> %dx%d\n",
					width, height, outputWidth,outputHeight);
			break;
	}

	if(cropWidth <= 0 || cropHeight <= 0 || outputHeight <= 0 || outputWidth <= 0)
		return -1;

	imgBuf = (uint8_t *)img;
	obDecodedAt = 0;

    bcm_host_init();

    s = setupOpenMaxImageResizer();
    if(s != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Failed to setup OMX image resizer.\n");
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Set up resizer successful.\n");
    }

    s = doResize();

    if(s != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Decode image failed: %s.\n", OMX_errString(s));
    	goto error;
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Image decoder succeeded, cleaning up.\n");
    }

    cleanup();

	*ret = new sImage;
	if(ret == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Failed to allocate sImage ret structure.\n");
		free(ret);
		return -1;
	}

	//TODO: get colorSpace from the port settings
	(*ret)->colorSpace = OMX_COLOR_Format32bitARGB8888;
	(*ret)->imageBuf = outputPic;
	(*ret)->imageSize = obDecodedAt;
	(*ret)->imageWidth = outputWidth;
	(*ret)->imageHeight = outputHeight;
	(*ret)->stride = outputStride;
	(*ret)->sliceHeight = outputSliceHeight;
	(*ret)->colorSpace = outputColorSpace;

	pis_logMessage(PIS_LOGLEVEL_ALL, "Resizer: Returning successfully.\n");
	return 0;

    error:

	pis_logMessage(PIS_LOGLEVEL_ERROR,"Resizer: Exiting with error.\n");

	pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Cleaning up pDecoder\n");
    cleanup();

    pis_logMessage(PIS_LOGLEVEL_ALL,"Resizer: Freeing pDecoder\n");
    //Zero state(?)

    return 0;

}
