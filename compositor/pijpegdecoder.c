#include "ilclient/ilclient.h"
#include "stdio.h"
#include "bcm_host.h"
#include "piresizer.h"
#include "pijpegdecoder.h"
#include "tricks.h"

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
};

int             bufferIndex = 0;	// index to buffer array

OMX_ERRORTYPE OMX_GetDebugInformation (OMX_OUT OMX_STRING debugInfo, OMX_INOUT OMX_S32 *pLen);

#define DEBUG_LEN 4096
OMX_STRING debug_info;
void print_debug() {
    int len = DEBUG_LEN;
    if(debug_info == NULL)
    	debug_info = malloc(DEBUG_LEN);
    debug_info[0] = 0;
    OMX_GetDebugInformation(debug_info, &len);
    fprintf(stderr, debug_info);
}

void printPort(OMX_HANDLETYPE componentHandle, OMX_U32 portno){
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

void eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
}

void FillBufferDoneCallback(
  void* data,
  COMPONENT_T *comp)
{
}

char *err2str(int err) {
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
    printf("OMX error %s\n", err2str(data));
}

//Called from decodeImage once the decoder has read the file header and
//changed the output port settings for the image
int
portSettingsChanged(OPENMAX_JPEG_DECODER * decoder)
{
    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    //Allocated the buffers and enables the port
    int ret = ilclient_enable_port_buffers(decoder->imageDecoder->component, decoder->imageDecoder->outPort, NULL, NULL, NULL);
    if(ret != OMX_ErrorNone)
    	printf("portSettingsChanged: Error %x enabling buffers.\n",ret);

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
    	printf("jpegdecoder: Failed to allocate imgBuf\n");
    	//TODO error
    }

    //printPort(decoder->imageDecoder->handle,decoder->imageDecoder->outPort);

    return OMXJPEG_OK;
}

int
prepareImageDecoder(OPENMAX_JPEG_DECODER * decoder)
{
    decoder->imageDecoder = malloc(sizeof(COMPONENT_DETAILS));
    if (decoder->imageDecoder == NULL) {
		perror("malloc image decoder");
		return OMXJPEG_ERROR_MEMORY;
    }

    int             ret = ilclient_create_component(decoder->client,
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
		perror("image decode");
		return OMXJPEG_ERROR_CREATING_COMP;
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
    // move to idle
    if(ilclient_change_component_state(decoder->imageDecoder->component,
				    OMX_StateIdle) != 0)
    	printf("startupImageDecoder: Failed to transition to idle.\n");

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

    decoder->inputBufferCount = 2;
    portdef.nBufferCountActual = 2;

    OMX_SetParameter(decoder->imageDecoder->handle,
    		OMX_IndexParamPortDefinition, &portdef);

    // enable the port and setup the buffers
    OMX_SendCommand(decoder->imageDecoder->handle,
		    OMX_CommandPortEnable,
		    decoder->imageDecoder->inPort, NULL);

    decoder->ppInputBufferHeader = malloc(sizeof(void)*decoder->inputBufferCount);
    uint32_t thisBufsize;
    for(int i = 0; i < decoder->inputBufferCount; i++){
    	thisBufsize = i == decoder->inputBufferCount-1 ?
				   pis_jpegdecoder.srcSize - i*bufSize : bufSize;

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

    // wait for port enable to complete - which it should once buffers are
    // assigned
    int ret =
	ilclient_wait_for_event(decoder->imageDecoder->component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				decoder->imageDecoder->inPort, 0,
				0, TIMEOUT_MS);
    if (ret != 0) {
		fprintf(stderr, "Did not get port enable %d\n", ret);
		return OMXJPEG_ERROR_EXECUTING;
    }

    // start executing the decoder
    ret = OMX_SendCommand(decoder->imageDecoder->handle,
			  OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (ret != 0) {
		fprintf(stderr, "Error starting image decoder %x\n", ret);
		return OMXJPEG_ERROR_EXECUTING;
    }

    ret = ilclient_wait_for_event(decoder->imageDecoder->component,
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
setupOpenMaxJpegDecoder(OPENMAX_JPEG_DECODER ** pDecoder)
{
    *pDecoder = malloc(sizeof(OPENMAX_JPEG_DECODER));
    if (pDecoder[0] == NULL) {
		perror("malloc decoder");
		return OMXJPEG_ERROR_MEMORY;
    }
    memset(*pDecoder, 0, sizeof(OPENMAX_JPEG_DECODER));

    if ((pDecoder[0]->client = ilclient_init()) == NULL) {
		perror("ilclient_init");
		return OMXJPEG_ERROR_ILCLIENT_INIT;
    }

    ilclient_set_error_callback(pDecoder[0]->client,
 			       error_callback,
 			       NULL);
    ilclient_set_eos_callback(pDecoder[0]->client,
 			     eos_callback,
 			     NULL);
    ilclient_set_fill_buffer_done_callback(pDecoder[0]->client,
    			FillBufferDoneCallback,
    			NULL);

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

OMX_BUFFERHEADERTYPE *temp;

// this function passed the jpeg image buffer in, and returns the decoded
// image
int
decodeImage(OPENMAX_JPEG_DECODER * decoder, char *sourceImage,
	    size_t imageSize)
{

    size_t          toread = 0;	// bytes left to read from buffer
    toread += imageSize;
    int gotEos = 0;
    bufferIndex = 0;

    while (toread > 0 || gotEos == 0) {
    	//TODO: figure out better structure for this
    	if(toread > 0)
    	{
			// get next buffer from array
			OMX_BUFFERHEADERTYPE *pBufHeader =
				decoder->ppInputBufferHeader[bufferIndex];

			// step index
			bufferIndex++;

			toread = toread - pBufHeader->nFilledLen;

			// update the buffer pointer and set the input flags

			pBufHeader->nOffset = 0;
			pBufHeader->nFlags = 0;
			if (toread <= 0) {
				pBufHeader->nFlags = OMX_BUFFERFLAG_EOS;
			}

			// empty the current buffer
			int             ret =
				OMX_EmptyThisBuffer(decoder->imageDecoder->handle,
						pBufHeader);

			if (ret != OMX_ErrorNone) {
				perror("Empty input buffer");
				fprintf(stderr, "return code %x\n", ret);
				return OMXJPEG_ERROR_MEMORY;
			}

			// wait for input buffer to empty or port changed event
			int             done = 0;
			while (done == 0) {
				ret =
					ilclient_wait_for_event
					(decoder->imageDecoder->component,
					 OMX_EventPortSettingsChanged,
					 decoder->imageDecoder->outPort, 0, 0, 1, ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 5);

				if (ret == 0) {
					ret = portSettingsChanged(decoder);
					if (ret != OMXJPEG_OK)
					return ret;
				}

				// check to see if buffer is now empty
				if (pBufHeader->nFilledLen == 0){
					done = 1;
				}
			}
    	}

		OMX_BUFFERHEADERTYPE *buff_header = ilclient_get_output_buffer(decoder->imageDecoder->component, decoder->imageDecoder->outPort, 0);
		if(buff_header != NULL){
			//TODO: find a better way to store this
			temp=buff_header;

			//Copy output port buffer to output image buffer
		    memcpy(&pis_jpegdecoder.imgBuf[pis_jpegdecoder.decodedAt],buff_header->pBuffer + buff_header->nOffset,buff_header->nFilledLen);
		    pis_jpegdecoder.decodedAt += buff_header->nFilledLen;

		    //See if we've reached the end of the stream
		    //TODO: handle case when we never get EOS
		    if (buff_header->nFlags & OMX_BUFFERFLAG_EOS) {
		    	gotEos = 1;
		    }else{
				int ret = OMX_FillThisBuffer(decoder->imageDecoder->handle,
						buff_header);
				if (ret != OMX_ErrorNone) {
					perror("Filling output buffer");
					fprintf(stderr, "Error code %x\n", ret);
					return OMXJPEG_ERROR_MEMORY;
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
    // flush everything through
    OMX_SendCommand(decoder->imageDecoder->handle,
		    OMX_CommandFlush, decoder->imageDecoder->outPort,
		    NULL);
    int ret = ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandFlush, 0,
			    decoder->imageDecoder->outPort, 0, 0,
			    TIMEOUT_MS);
    if(ret != 0)
    	printf("Error flushing decoder commands: %d\n", ret);

    ret = OMX_SendCommand(decoder->imageDecoder->handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(ret != OMX_ErrorNone){
    	printf("Error transitioning to idle: %d\n",ret);
    }

    ret = OMX_SendCommand(decoder->imageDecoder->handle, OMX_CommandPortDisable,
		    decoder->imageDecoder->inPort, NULL);
    if(ret != 0)
    	printf("Error disabling image decoder input port: %d\n",ret);


    for(int i = 0;i<decoder->inputBufferCount;i++)
    	OMX_FreeBuffer(decoder->imageDecoder->handle,
    			decoder->imageDecoder->inPort, decoder->ppInputBufferHeader[i]);

    free(decoder->ppInputBufferHeader);

    ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, decoder->imageDecoder->inPort, 0, 0,
			    TIMEOUT_MS);

    ret = OMX_SendCommand(decoder->imageDecoder->handle, OMX_CommandPortDisable,
		    decoder->imageDecoder->outPort, NULL);
    if(ret != OMX_ErrorNone){
    	printf("1 Error disabling output port %d\n",ret);
    }

    OMX_FreeBuffer(decoder->imageDecoder->handle,
		   decoder->imageDecoder->outPort,
		   temp);

    ret =  ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, decoder->imageDecoder->outPort, 0, 0,
			    TIMEOUT_MS);
    if(ret != 0) printf("2 Error disabling output port %d\n",ret);


    //Once ports are disabled the component will go to idle
    ret = ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateIdle, 0, 0, TIMEOUT_MS);

    if(ret != OMX_ErrorNone)
    	printf("Error %d transitioning to idle.\n", ret);


    //Change the components state to loaded. the ilclient will also wait to confirm the event
    ret = ilclient_change_component_state(decoder->imageDecoder->component,
				    OMX_StateLoaded);

    if(ret != 0)
    	printf("Transition to loaded failed\n");

    OMX_Deinit();

    if (decoder->client != NULL) {
    	ilclient_destroy(decoder->client);
    }
}

sImage *decodeJpgImage(char *img)
{
    OPENMAX_JPEG_DECODER *pDecoder;
    char           *sourceImage;
    size_t          imageSize;
    int             s;

    pis_jpegdecoder.decodedAt = 0;

    FILE           *fp = fopen(img, "rb");
    if (!fp) {
	printf("File %s not found.\n", img);
    }
    fseek(fp, 0L, SEEK_END);
    imageSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    sourceImage = malloc(imageSize);

    assert(sourceImage != NULL);
    s = fread(sourceImage, 1, imageSize, fp);

    if(s != imageSize){
    	printf("decodeJpgImage: File read error.\n");
    	return NULL;
    }

    pis_jpegdecoder.srcSize = imageSize;
    pis_jpegdecoder.inputBuf = (uint8_t *)sourceImage;

    fclose(fp);
    bcm_host_init();
    s = setupOpenMaxJpegDecoder(&pDecoder);
    if(s != 0){
    	printf("decodeJpgImage: Error initializing decoder.\n");
    	return NULL;
    }

    s = decodeImage(pDecoder, sourceImage, imageSize);

    if(s != 0){
    	printf("decodeJpgImage: Error decoding image.\n");
    	return NULL;
    }

    cleanup(pDecoder);
    free(pDecoder);
    free(sourceImage);

    sImage *ret = malloc(sizeof(sImage));
    if(ret == NULL){
    	printf("jpegdecoder: Failed to malloc ret.\n");
    }

    //TODO: get colorSpace from the port settings
    ret->colorSpace = OMX_COLOR_FormatYUV420PackedPlanar;
    ret->imageBuf = pis_jpegdecoder.imgBuf;
    ret->imageSize = pis_jpegdecoder.decodedAt;
    ret->imageWidth = pis_jpegdecoder.width;
    ret->imageHeight = pis_jpegdecoder.height;
    ret->stride = pis_jpegdecoder.stride;
    ret->sliceHeight = pis_jpegdecoder.sliceHeight;

    return ret;
}

