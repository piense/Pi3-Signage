#include "ilclient/ilclient.h"
#include "stdio.h"
#include "bcm_host.h"
#include <stdint.h> 
#include "piresizer.h"

//I hate asserts
#include <assert.h>

#define OMXJPEG_OK                  0
#define OMXJPEG_ERROR_ILCLIENT_INIT    -1024
#define OMXJPEG_ERROR_OMX_INIT         -1025
#define OMXJPEG_ERROR_MEMORY         -1026
#define OMXJPEG_ERROR_CREATING_COMP    -1027
#define OMXJPEG_ERROR_WRONG_NO_PORTS   -1028
#define OMXJPEG_ERROR_EXECUTING         -1029
#define OMXJPEG_ERROR_NOSETTINGS   -1030

typedef struct _OPENMAX_RESIZER OPENMAX_RESIZER;

//this function run the boilerplate to setup the openmax components;
int setupOpenMaxResizer(OPENMAX_RESIZER ** resizer, uint16_t srcWidth, uint16_t srcHeight, uint32_t imgSize, uint16_t srcStride, uint16_t srcSliceHeight );

//this function passed the jpeg image buffer in, and returns the decoded image
int resizeImage(OPENMAX_RESIZER* resizer,
              char *sourceImage, size_t imageSize, uint32_t imageWidth, uint32_t imageHeight);

//this function cleans up the decoder.
void resizeCleanup(OPENMAX_RESIZER* decoder);

uint8_t *decodedPic = NULL;
uint32_t decodedPicAt;
uint32_t decodedPicWidth;
uint32_t decodedPicHeight;

OMX_PARAM_RESIZETYPE piResizeType;
OMX_RESIZEMODETYPE piResizeModeType;

#define TIMEOUT_MS 20

typedef struct _COMPONENT_DETAILS {
    COMPONENT_T    *component;
    OMX_HANDLETYPE  handle;
    int             inPort;
    int             outPort;
} COMPONENT_DETAILS;

struct _OPENMAX_RESIZER{
    ILCLIENT_T     *client;
    COMPONENT_DETAILS *imageResizer;
    OMX_BUFFERHEADERTYPE **ppInputBufferHeader;
    int             inputBufferHeaderCount;
    OMX_BUFFERHEADERTYPE *pOutputBufferHeader;
};

OMX_ERRORTYPE OMX_GetDebugInformation (OMX_OUT OMX_STRING debugInfo, OMX_INOUT OMX_S32 *pLen);

uint16_t inputWidth, inputHeight, outputWidth, outputHeight;

#define DEBUG_LEN 4096
OMX_STRING debug_info;
void resize_print_debug() {
    int len = DEBUG_LEN;
    if(debug_info == NULL)
    	debug_info = malloc(DEBUG_LEN);
    debug_info[0] = 0;
    OMX_GetDebugInformation(debug_info, &len);
    fprintf(stderr, debug_info);
}

void resizePrintPort(OMX_HANDLETYPE componentHandle, OMX_U32 portno){
    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = portno;
    OMX_GetParameter(componentHandle,
		     OMX_IndexParamPortDefinition, &portdef);

    char domain[50];
    switch(portdef.eDomain){
    	case OMX_PortDomainAudio: strcpy(domain,"Audio"); break;
    	case OMX_PortDomainVideo: strcpy(domain,"Video"); break;
    	case OMX_PortDomainImage: strcpy(domain,"Image"); break;
    	case OMX_PortDomainOther: strcpy(domain,"Other"); break;
    	case OMX_PortDomainKhronosExtensions: strcpy(domain,"Khronos Extensions"); break;
    	case OMX_PortDomainVendorStartUnused: strcpy(domain,"Vendor Start Unused"); break;
    	case OMX_PortDomainMax: strcpy(domain,"Max"); break;
    	default: strcpy(domain,"Unknown"); break;
    }

    printf("Port Info:\n\tOMX Version: %d\n\tPort Index: %d\n\tDirection: %s\n\tBuffer Count Actual: %d\n\tBuffer Count Min: %d\n\tBuffer Size: %d\n\tEnabled: %s\n\tPopulated: %s\n\tDomain: %s\n",
    		portdef.nVersion.nVersion, portdef.nPortIndex, portdef.eDir == 0 ? "Input" : "Output", portdef.nBufferCountActual, portdef.nBufferCountMin, portdef.nBufferSize, portdef.bEnabled == 1 ? "Yes" : "No",
    				portdef.bPopulated == 1 ? "Yes" : "No",domain);

    //TODO: Detail other types of ports

    if(portdef.eDomain == OMX_PortDomainImage){

    	char coding[20];

        switch(portdef.format.image.eCompressionFormat){
        	case OMX_IMAGE_CodingUnused: strcpy(coding,"Unused"); break;
        	case OMX_IMAGE_CodingAutoDetect: strcpy(coding,"Auto Detect"); break;
        	case OMX_IMAGE_CodingJPEG: strcpy(coding,"JPEG"); break;
        	case OMX_IMAGE_CodingJPEG2K: strcpy(coding,"JPEG2K"); break;
        	case OMX_IMAGE_CodingEXIF: strcpy(coding,"EXIF"); break;
        	case OMX_IMAGE_CodingTIFF: strcpy(coding,"TIFF"); break;
        	case OMX_IMAGE_CodingGIF: strcpy(coding,"GIF"); break;
        	case OMX_IMAGE_CodingPNG: strcpy(coding,"PNG"); break;
        	case  OMX_IMAGE_CodingLZW: strcpy(coding,"LZW"); break;
        	case OMX_IMAGE_CodingBMP: strcpy(coding,"BMP"); break;
        	case OMX_IMAGE_CodingTGA: strcpy(coding,"TGA"); break;
        	case OMX_IMAGE_CodingPPM: strcpy(coding,"PPM"); break;
        	default: strcpy(coding,"???");
        }

    	printf("\tMIME Type: %s\n\tWidth: %d\n\tHeight: %d\n\tStride: %d\n\tSlice Height: %d\n\tFlag Error Concealment: %s\n\tCompression Format: %s\n\tColor Format: %d\n",
    			portdef.format.image.cMIMEType,portdef.format.image.nFrameWidth,portdef.format.image.nFrameHeight,portdef.format.image.nStride,
    			portdef.format.image.nSliceHeight,portdef.format.image.bFlagErrorConcealment == 1 ? "True" : "False", coding, portdef.format.image.eColorFormat );
    }

}

void resize_eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
	//printf("EOS CB\n");
}

void ResizeFillBufferDoneCallback(
  void* data,
  COMPONENT_T *comp)
{
	//printf("Fill buf done\n");
}

char *resizeerr2str(int err) {
    switch (err) {
    case OMX_ErrorInsufficientResources: return "OMX_ErrorInsufficientResources";
    case OMX_ErrorUndefined: return "OMX_ErrorUndefined";
    case OMX_ErrorInvalidComponentName: return "OMX_ErrorInvalidComponentName";
    case OMX_ErrorComponentNotFound: return "OMX_ErrorComponentNotFound";
    case OMX_ErrorInvalidComponent: return "OMX_ErrorInvalidComponent";
    case OMX_ErrorBadParameter: return "OMX_ErrorBadParameter";
    case OMX_ErrorNotImplemented: return "OMX_ErrorNotImplemented";
    case OMX_ErrorUnderflow: return "OMX_ErrorUnderflow";
    case OMX_ErrorOverflow: return "OMX_ErrorOverflow";
    case OMX_ErrorHardware: return "OMX_ErrorHardware";
    case OMX_ErrorInvalidState: return "OMX_ErrorInvalidState";
    case OMX_ErrorStreamCorrupt: return "OMX_ErrorStreamCorrupt";
    case OMX_ErrorPortsNotCompatible: return "OMX_ErrorPortsNotCompatible";
    case OMX_ErrorResourcesLost: return "OMX_ErrorResourcesLost";
    case OMX_ErrorNoMore: return "OMX_ErrorNoMore";
    case OMX_ErrorVersionMismatch: return "OMX_ErrorVersionMismatch";
    case OMX_ErrorNotReady: return "OMX_ErrorNotReady";
    case OMX_ErrorTimeout: return "OMX_ErrorTimeout";
    case OMX_ErrorSameState: return "OMX_ErrorSameState";
    case OMX_ErrorResourcesPreempted: return "OMX_ErrorResourcesPreempted";
    case OMX_ErrorPortUnresponsiveDuringAllocation: return "OMX_ErrorPortUnresponsiveDuringAllocation";
    case OMX_ErrorPortUnresponsiveDuringDeallocation: return "OMX_ErrorPortUnresponsiveDuringDeallocation";
    case OMX_ErrorPortUnresponsiveDuringStop: return "OMX_ErrorPortUnresponsiveDuringStop";
    case OMX_ErrorIncorrectStateTransition: return "OMX_ErrorIncorrectStateTransition";
    case OMX_ErrorIncorrectStateOperation: return "OMX_ErrorIncorrectStateOperation";
    case OMX_ErrorUnsupportedSetting: return "OMX_ErrorUnsupportedSetting";
    case OMX_ErrorUnsupportedIndex: return "OMX_ErrorUnsupportedIndex";
    case OMX_ErrorBadPortIndex: return "OMX_ErrorBadPortIndex";
    case OMX_ErrorPortUnpopulated: return "OMX_ErrorPortUnpopulated";
    case OMX_ErrorComponentSuspended: return "OMX_ErrorComponentSuspended";
    case OMX_ErrorDynamicResourcesUnavailable: return "OMX_ErrorDynamicResourcesUnavailable";
    case OMX_ErrorMbErrorsInFrame: return "OMX_ErrorMbErrorsInFrame";
    case OMX_ErrorFormatNotDetected: return "OMX_ErrorFormatNotDetected";
    case OMX_ErrorContentPipeOpenFailed: return "OMX_ErrorContentPipeOpenFailed";
    case OMX_ErrorContentPipeCreationFailed: return "OMX_ErrorContentPipeCreationFailed";
    case OMX_ErrorSeperateTablesUsed: return "OMX_ErrorSeperateTablesUsed";
    case OMX_ErrorTunnelingUnsupported: return "OMX_ErrorTunnelingUnsupported";
    default: return "unknown error";
    }
}


void resizePrintState(OMX_HANDLETYPE handle) {
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

void resize_error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
    printf("OMX error %s\n", resizeerr2str(data));
}

//Called from resizeImage once the decoder has read the file header and
//changed the output port settings for the image
int
resizePortSettingsChanged(OPENMAX_RESIZER * decoder)
{

    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    // Get the image dimensions
    //TODO: Store color format too
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = decoder->imageResizer->outPort;
    OMX_GetParameter(decoder->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    portdef.format.image.eColorFormat = OMX_COLOR_Format32bitARGB8888;

    portdef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portdef.format.image.bFlagErrorConcealment = OMX_FALSE;
    portdef.format.image.nFrameWidth = outputWidth;
    portdef.format.image.nFrameHeight = outputHeight;
	portdef.format.image.nStride = 0;
	portdef.format.image.nSliceHeight = 0;
	portdef.nBufferSize=outputWidth*outputHeight*4;

    int ret = OMX_SetParameter(decoder->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    if(ret != OMX_ErrorNone)
        	printf("portSettingsChanged: Error %x enabling buffers.\n",ret);

    //Allocated the buffers and enables the port
    ret = ilclient_enable_port_buffers(decoder->imageResizer->component, decoder->imageResizer->outPort, NULL, NULL, NULL);
    if(ret != OMX_ErrorNone)
    	printf("portSettingsChanged: Error %d enabling buffers.\n",ret);

    // Get the image dimensions
    //TODO: Store color format too
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = decoder->imageResizer->outPort;
    OMX_GetParameter(decoder->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    unsigned int    uWidth =
	(unsigned int) portdef.format.image.nFrameWidth;
    unsigned int    uHeight =
	(unsigned int) portdef.format.image.nFrameHeight;

    printf("Image dimensions: %d %d\n",uWidth, uHeight);

    decodedPicWidth = uWidth;
    decodedPicHeight = uHeight;

    //resizePrintPort(decoder->imageResizer->handle,decoder->imageResizer->inPort);
    //resizePrintPort(decoder->imageResizer->handle,decoder->imageResizer->outPort);

    return OMXJPEG_OK;
}

int
prepareImageResizer(OPENMAX_RESIZER * resizer)
{
	resizer->imageResizer = malloc(sizeof(COMPONENT_DETAILS));
    if (resizer->imageResizer == NULL) {
		perror("malloc image decoder");
		return OMXJPEG_ERROR_MEMORY;
    }

    int             ret = ilclient_create_component(resizer->client,
						    &resizer->
						    imageResizer->
						    component,
						    "resize",
						    ILCLIENT_DISABLE_ALL_PORTS
						    |
						    ILCLIENT_ENABLE_INPUT_BUFFERS
						    |
						    ILCLIENT_ENABLE_OUTPUT_BUFFERS //When not using tunneling (maybe)
    						);

    if (ret != 0) {
		perror("image resize");
		return OMXJPEG_ERROR_CREATING_COMP;
    }
    // grab the handle for later use in OMX calls directly
    resizer->imageResizer->handle =
	ILC_GET_HANDLE(resizer->imageResizer->component);

    // get and store the ports
    OMX_PORT_PARAM_TYPE port;
    port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;

    OMX_GetParameter(resizer->imageResizer->handle,
		     OMX_IndexParamImageInit, &port);
    if (port.nPorts != 2) {
    	return OMXJPEG_ERROR_WRONG_NO_PORTS;
    }
    resizer->imageResizer->inPort = port.nStartPortNumber;
    resizer->imageResizer->outPort = port.nStartPortNumber + 1;

    return OMXJPEG_OK;
}

int
startupImageResizer(OPENMAX_RESIZER * resizer, uint16_t srcWidth, uint16_t srcHeight, uint16_t srcSize, uint16_t srcStride, uint16_t srcSliceHeight)
{
    // move to idle
    if(ilclient_change_component_state(resizer->imageResizer->component,
				    OMX_StateIdle) != 0)
    	printf("startupImageDecoder: Failed to transition to idle.\n");

    // set input image format
    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
    imagePortFormat.nVersion.nVersion = OMX_VERSION;
    imagePortFormat.nPortIndex = resizer->imageResizer->inPort;
    imagePortFormat.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
    int ret = OMX_SetParameter(resizer->imageResizer->handle,
		     OMX_IndexParamImagePortFormat, &imagePortFormat);
    if(ret != OMX_ErrorNone)
    	printf("Error %x setting input port color format.\n", ret);

    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = resizer->imageResizer->inPort;
    OMX_GetParameter(resizer->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    portdef.format.image.nFrameHeight = srcHeight;
    portdef.format.image.nFrameWidth = srcWidth;
    portdef.nBufferSize = srcSize;
    portdef.format.image.nStride = srcStride ;
    portdef.format.image.nSliceHeight = srcSliceHeight;

    portdef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portdef.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

    ret = OMX_SetParameter(resizer->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);
    if(ret != OMX_ErrorNone)
    	printf("Error %x setting input port config.\n", ret);

    // get buffer requirements
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = resizer->imageResizer->inPort;
    OMX_GetParameter(resizer->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    // enable the port and setup the buffers
    OMX_SendCommand(resizer->imageResizer->handle,
		    OMX_CommandPortEnable,
		    resizer->imageResizer->inPort, NULL);
    resizer->inputBufferHeaderCount = portdef.nBufferCountActual;
    // allocate pointer array
    resizer->ppInputBufferHeader =
	(OMX_BUFFERHEADERTYPE **) malloc(sizeof(void) *
			resizer->inputBufferHeaderCount);
    // allocate each buffer
    int             i;
    for (i = 0; i < resizer->inputBufferHeaderCount; i++) {
		if (OMX_AllocateBuffer(resizer->imageResizer->handle,
					   &resizer->ppInputBufferHeader[i],
					   resizer->imageResizer->inPort,
					   (void *) NULL,
					   portdef.nBufferSize) != OMX_ErrorNone) {
			perror("Allocate decode buffer");
			return OMXJPEG_ERROR_MEMORY;
		}
    }

    // wait for port enable to complete - which it should once buffers are
    // assigned
                 ret =
	ilclient_wait_for_event(resizer->imageResizer->component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				resizer->imageResizer->inPort, 0,
				0, TIMEOUT_MS);
    if (ret != 0) {
		fprintf(stderr, "Did not get port enable %d\n", ret);
		return OMXJPEG_ERROR_EXECUTING;
    }

    // start executing the resizer
    ret = OMX_SendCommand(resizer->imageResizer->handle,
			  OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (ret != 0) {
		fprintf(stderr, "Error starting image decoder %x\n", ret);
		return OMXJPEG_ERROR_EXECUTING;
    }

    ret = ilclient_wait_for_event(resizer->imageResizer->component,
				  OMX_EventCmdComplete,
				  OMX_CommandStateSet, 0, OMX_StateExecuting, 0, 0,
				  TIMEOUT_MS);
    if (ret != 0) {
    	fprintf(stderr, "Did not receive executing stat %d\n", ret);
		return OMXJPEG_ERROR_EXECUTING;
    }

    return OMXJPEG_OK;
}

// this function run the boilerplate to setup the openmax components;
int
setupOpenMaxResizer(OPENMAX_RESIZER ** resizer, uint16_t srcWidth, uint16_t srcHeight, uint32_t srcSize, uint16_t srcStride, uint16_t srcSliceHeight )
{
    *resizer = malloc(sizeof(OPENMAX_RESIZER));
    if (resizer[0] == NULL) {
		perror("malloc decoder");
		return OMXJPEG_ERROR_MEMORY;
    }
    memset(*resizer, 0, sizeof(OPENMAX_RESIZER));

    if ((resizer[0]->client = ilclient_init()) == NULL) {
		perror("ilclient_init");
		return OMXJPEG_ERROR_ILCLIENT_INIT;
    }


    ilclient_set_error_callback(resizer[0]->client,
 			       resize_error_callback,
 			       NULL);
    ilclient_set_eos_callback(resizer[0]->client,
 			     resize_eos_callback,
 			     NULL);
    ilclient_set_fill_buffer_done_callback(resizer[0]->client,
    			ResizeFillBufferDoneCallback,
    			NULL);

    if (OMX_Init() != OMX_ErrorNone) {
	ilclient_destroy(resizer[0]->client);
	perror("OMX_Init");
	return OMXJPEG_ERROR_OMX_INIT;
    }
    // prepare the image decoder
    int             ret = prepareImageResizer(resizer[0]);
    if (ret != OMXJPEG_OK)
	return ret;
    
    ret = startupImageResizer(resizer[0], srcWidth, srcHeight, srcSize, srcStride, srcSliceHeight);
    if (ret != OMXJPEG_OK)
	return ret;

    return OMXJPEG_OK;
}

OMX_BUFFERHEADERTYPE *temp;


int
resizeImage(OPENMAX_RESIZER* resizer,
              char *sourceImage, size_t imageSize, uint32_t imageWidth, uint32_t imageHeight)
{
    char           *sourceOffset = sourceImage;	// we store a separate
						// buffer to image so we
						// can offset it
    size_t toread = 0;	// bytes left to read from buffer
    toread += imageSize;
    int gotEos = 0;
    int bufferIndex = 0;

    //TODO: figure out better structure for this

	// get next buffer from array
	OMX_BUFFERHEADERTYPE *pBufHeader =
			resizer->ppInputBufferHeader[bufferIndex];

	// step index and reset to 0 if required
	bufferIndex++;
	if (bufferIndex >= resizer->inputBufferHeaderCount)
		bufferIndex = 0;

	// work out the next chunk to load into the decoder
	// (should be the whole image or we'll have a problem)
	if (toread > pBufHeader->nAllocLen)
		pBufHeader->nFilledLen = pBufHeader->nAllocLen;
	else
		pBufHeader->nFilledLen = toread;

	toread = toread - pBufHeader->nFilledLen;

	// pass the bytes to the buffer
	// (Moving too much data around, should use tunneling or the existing buffer)
	memcpy(pBufHeader->pBuffer, sourceOffset, pBufHeader->nFilledLen);

	// update the buffer pointer and set the input flags
	sourceOffset = sourceOffset + pBufHeader->nFilledLen;
	pBufHeader->nOffset = 0;
	pBufHeader->nFlags = 0;
	if (toread <= 0) {
		pBufHeader->nFlags = OMX_BUFFERFLAG_EOS;
	}
	// empty the current buffer
	int             ret =
		OMX_EmptyThisBuffer(resizer->imageResizer->handle,
				pBufHeader);

	if (ret != OMX_ErrorNone) {
		perror("Empty input buffer");
		fprintf(stderr, "return code %x\n", ret);
		return OMXJPEG_ERROR_MEMORY;
	}

	ret =
		ilclient_wait_for_event
		(resizer->imageResizer->component,
		 OMX_EventPortSettingsChanged,
		 resizer->imageResizer->outPort, 0, 0, 1, ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 5);

	if (ret == 0) {
		ret = resizePortSettingsChanged(resizer);
		if (ret != OMXJPEG_OK)
		return ret;
	}else{
		printf("resizeImage: Setting up the input buffer failed.\n");
	}

	while (gotEos == 0) {
		OMX_BUFFERHEADERTYPE *buff_header = ilclient_get_output_buffer(resizer->imageResizer->component, resizer->imageResizer->outPort, 1);
		if(buff_header != NULL){
			//TODO: find a better way to store this
			temp=buff_header;

			//Copy output port buffer to output image buffer
			//TODO: bounds checking, point the output buffer to chunks of the main buffer to avoid memory copying
		    uint32_t i;
		    //printf("Filled: %d\n",buff_header->nFilledLen);
		    for(i=0;i<buff_header->nFilledLen;i++){
		    	decodedPic[decodedPicAt] = *(buff_header->pBuffer + buff_header->nOffset + i);
		    	decodedPicAt++;
		    }

		    //See if we've reached the end of the stream
		    //TODO: handle case when we never get EOS
		    if (buff_header->nFlags & OMX_BUFFERFLAG_EOS) {
		    	gotEos = 1;
		    }else{
				int ret = OMX_FillThisBuffer(resizer->imageResizer->handle,
						buff_header);
				if (ret != OMX_ErrorNone) {
					perror("Filling output buffer");
					fprintf(stderr, "Error code %x\n", ret);
					return OMXJPEG_ERROR_MEMORY;
				}
		    }
		}
    }

    printf("Resizer transfered %d bytes\n",decodedPicAt);

    return OMXJPEG_OK;
}

// this function cleans up the resizer.
void
resizeCleanup(OPENMAX_RESIZER * resizer)
{
    // flush everything through
    OMX_SendCommand(resizer->imageResizer->handle,
		    OMX_CommandFlush, resizer->imageResizer->outPort,
		    NULL);
    int ret = ilclient_wait_for_event(resizer->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandFlush, 0,
			    resizer->imageResizer->outPort, 0, 0,
			    TIMEOUT_MS);
    if(ret != 0)
    	printf("Error flushing decoder commands: %d\n", ret);


    ret = OMX_SendCommand(resizer->imageResizer->handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(ret != OMX_ErrorNone){
    	printf("Error transitioning to idle: %d\n",ret);
    }

    ret = OMX_SendCommand(resizer->imageResizer->handle, OMX_CommandPortDisable,
    		resizer->imageResizer->inPort, NULL);
    if(ret != 0)
    	printf("Error disabling image decoder input port: %d\n",ret);

    int             i = 0;
    for (i = 0; i < resizer->inputBufferHeaderCount; i++) {
	OMX_BUFFERHEADERTYPE *vpBufHeader =
			resizer->ppInputBufferHeader[i];

	OMX_FreeBuffer(resizer->imageResizer->handle,
			resizer->imageResizer->inPort, vpBufHeader);
    }

    ilclient_wait_for_event(resizer->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, resizer->imageResizer->inPort, 0, 0,
			    TIMEOUT_MS);

    ret = OMX_SendCommand(resizer->imageResizer->handle, OMX_CommandPortDisable,
    		resizer->imageResizer->outPort, NULL);
    if(ret != OMX_ErrorNone){
    	printf("1 Error disabling output port %d\n",ret);
    }

    OMX_FreeBuffer(resizer->imageResizer->handle,
    		resizer->imageResizer->outPort,
		   temp);

    ret =  ilclient_wait_for_event(resizer->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, resizer->imageResizer->outPort, 0, 0,
			    TIMEOUT_MS);
    if(ret != 0) printf("2 Error disabling output port %d\n",ret);

    //Once ports are disabled the component will go to idle
    ret = ilclient_wait_for_event(resizer->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateIdle, 0, 0, TIMEOUT_MS);

    if(ret != OMX_ErrorNone)
    	printf("Error %d transitioning to idle.\n", ret);

    //Change the components state to loaded. the ilclient will also wait to confirm the event
    ret = ilclient_change_component_state(resizer->imageResizer->component,
				    OMX_StateLoaded);

    if(ret != 0)
    	printf("Transition to loaded failed\n");

    OMX_Deinit();

    if (resizer->client != NULL) {
    	ilclient_destroy(resizer->client);
    }
}

sImage *resizeImage2(char *img,
		uint32_t srcWidth, uint32_t srcHeight, //pixels
		size_t srcSize, //bytes
		OMX_COLOR_FORMATTYPE imgCoding,
		uint16_t srcStride,
		uint16_t srcSliceHeight,
		uint32_t output2Width,
		uint32_t output2Height,
		_Bool crop,
		_Bool lockAspect
		)
		//TODO Colorspace
{
	//TODO: Handle input size = output size

	//TODO move this to when we get the final image size
	decodedPic = malloc(1920*1080*5);
	memset(decodedPic,0,1920*1080*5);
	decodedPicAt = 0;

	//TODO: Check imgCoding is supported by resizer

	inputWidth = srcWidth;
	inputHeight = srcHeight;
	outputWidth = 1920;
	outputHeight = 1080;

    OPENMAX_RESIZER *pDecoder;
    int             s;

    bcm_host_init();
    s = setupOpenMaxResizer(&pDecoder, srcWidth, srcHeight, srcSize, srcStride, srcSliceHeight);
    if(s != 0){
    	printf("resizeImage: Error initializing resizer.\n");
    	return NULL;
    }

    s = resizeImage(pDecoder, img, srcWidth, srcWidth, srcHeight);

    if(s != 0){
    	printf("resizeImage: Error resizing image.\n");
    	return NULL;
    }

    resizeCleanup(pDecoder);

    sImage *ret = malloc(sizeof(sImage));
    ret->imageBuf = decodedPic;
    ret->imageWidth=decodedPicWidth;
    ret->imageHeight=decodedPicHeight;

    //TODO return struct & value
    return ret;
}

