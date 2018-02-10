#include <stdio.h>
#include <stdarg.h>

#include "PiSignageLogging.h"

#include "compositor/ilclient/ilclient.h"

int pis_loggingLevel = PIS_LOGLEVEL_WARNING;

void pis_logMessage(int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

	if(level >= pis_loggingLevel)
	{
		vprintf(fmt,args);
	}

    va_end(args);
}

char *OMX_errString(int err) {
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

OMX_ERRORTYPE OMX_GetDebugInformation (OMX_OUT OMX_STRING debugInfo, OMX_INOUT OMX_S32 *pLen);

#define DEBUG_LEN 4096
OMX_STRING debug_info;
void printOMXdebug() {
    int len = DEBUG_LEN;
    if(debug_info == NULL)
    	debug_info = malloc(DEBUG_LEN);
    debug_info[0] = 0;
    OMX_GetDebugInformation(debug_info, &len);
    fprintf(stderr, debug_info);
}

void printOMXPort(OMX_HANDLETYPE componentHandle, OMX_U32 portno){
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

    if(portdef.eDomain == OMX_PortDomainVideo){

    	char coding[20];


        switch(portdef.format.image.eCompressionFormat){
        	case OMX_VIDEO_CodingUnused: strcpy(coding,"Unused"); break;
        	case OMX_VIDEO_CodingAutoDetect: strcpy(coding,"Auto Detect"); break;
        	case OMX_VIDEO_CodingMPEG2: strcpy(coding,"MPEG2"); break;
        	case OMX_VIDEO_CodingH263: strcpy(coding,"H.263"); break;
        	case OMX_VIDEO_CodingMPEG4: strcpy(coding,"MPEG4"); break;
        	case OMX_VIDEO_CodingWMV: strcpy(coding,"WMV"); break;
        	case OMX_VIDEO_CodingRV: strcpy(coding,"RV"); break;
        	default: strcpy(coding,"???");
        }

    	printf("\tBitrate: %d\n\tWidth: %d\n\tHeight: %d\n"
    			"\tStride: %d\n\tSlice Height: %d\n\t"
    			"Flag Error Concealment: %s\n\tCompression Format: %s\n\tColor Format: %d\n",
    			portdef.format.video.nBitrate,portdef.format.video.nFrameWidth,portdef.format.video.nFrameHeight,
				portdef.format.video.nStride,portdef.format.video.nSliceHeight,
				portdef.format.video.bFlagErrorConcealment == 1 ? "True" : "False", coding, portdef.format.video.eColorFormat );
    }

}
