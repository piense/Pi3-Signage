#include <stdio.h>

#include "piDisplayRender.h"
#include "bcm_host.h"
#include "ilclient/ilclient.h"

extern void printPort(OMX_HANDLETYPE componentHandle, OMX_U32 portno);

void EmptyBufferDoneCallback(
  void* data,
  COMPONENT_T *comp)
{
	printf("Empty buf done\n");
}

void error_callback2(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
    //printf("OMX error %x\n", data);
}

void pis_fillBuf(pis_window *window, uint32_t color)
{
	uint32_t x,y;
	for(x = 0;x<window->screenWidth;x++)
		for(y = 0;y<window->screenHeight;y++)
			window->bufImages[window->lastBufUsed][x+y*window->screenWidth] = color;
}

void pis_bufCopy(pis_window *window, uint32_t *img,
		uint32_t srcX, uint32_t srcY,
		uint32_t srcWidth, uint32_t srcHeight,
		uint32_t dstX, uint32_t dstY)
{
	uint32_t u,v;

	//TODO: Bounds checking

	for(u = srcX;u<srcWidth+srcX;u++)
		for(v = srcY;v<srcHeight+srcY;v++)
			window->bufImages[window->lastBufUsed][u-srcX+dstX + (v-srcY+dstY)*window->screenWidth] = img[u+v*srcWidth];
}

void createComponent(pis_window *window)
{

	if ((window->client = ilclient_init()) == NULL) {
			perror("DR ilclient_init");
			return; //TODO Error codes
	    }

    if (OMX_Init() != OMX_ErrorNone) {
		ilclient_destroy(window->client);
		perror("OMX_Init");
		return; //TODO Error codes
    }

    int ret = ilclient_create_component(window->client,
						    &window->component.component,
						    "video_render",
						    ILCLIENT_DISABLE_ALL_PORTS
						    |
						    ILCLIENT_ENABLE_INPUT_BUFFERS
    						);

    if (ret != 0) {
		perror("piDisplayRender creating component");
		return; //TODO Error codes
    }

    window->component.handle =
    ILC_GET_HANDLE(window->component.component);
}

void initDisplayRegion(pis_window *window)
{
	OMX_CONFIG_DISPLAYREGIONTYPE region;

    region.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
    region.nVersion.nVersion = OMX_VERSION;
    region.nPortIndex = 90;
    region.layer = window->layer;
    region.set = OMX_DISPLAY_SET_LAYER |
    		   OMX_DISPLAY_SET_FULLSCREEN
			   ;

    int ret = OMX_SetConfig(window->component.handle,
    	OMX_IndexConfigDisplayRegion, &region);
    if(ret != 0)
        	printf("Set 1 Error %x\n", ret);

    OMX_PARAM_PORTDEFINITIONTYPE videoParam;

    videoParam.nSize=sizeof(videoParam);
    videoParam.nVersion.nVersion = OMX_VERSION;
    videoParam.nPortIndex = 90;
    ret = OMX_GetParameter(window->component.handle,
    		OMX_IndexParamPortDefinition, &videoParam);
    if(ret != 0)
        	printf("Get 3 Error %x\n", ret);

    videoParam.format.image.eColorFormat = OMX_COLOR_Format32bitARGB8888;
    videoParam.eDomain = OMX_PortDomainImage;
    videoParam.format.image.nFrameHeight = window->screenHeight;
    videoParam.format.image.nFrameWidth = window->screenWidth;
    videoParam.format.image.nStride = window->screenWidth * 4;
    videoParam.format.image.nSliceHeight = 0;

    ret = OMX_SetParameter(window->component.handle,
        		OMX_IndexParamPortDefinition, &videoParam);
        if(ret != 0)
            	printf("Set 3 Error %x\n", ret);

	ret = OMX_GetParameter(window->component.handle,
			OMX_IndexParamPortDefinition, &videoParam);
	if(ret != 0)
			printf("Get 3 Error %x\n", ret);

    if(ilclient_change_component_state(window->component.component,
				    OMX_StateIdle) != 0)
    	printf("startupImageDecoder: Failed to transition to idle.\n");

    // get buffer requirements
    OMX_PARAM_PORTDEFINITIONTYPE portdef;
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = 90;
    OMX_GetParameter(window->component.handle,
		     OMX_IndexParamPortDefinition, &portdef);

    portdef.nBufferCountActual = portdef.nBufferCountMin;

    OMX_SetParameter(window->component.handle,
    		OMX_IndexParamPortDefinition, &portdef);

    // enable the port and setup the buffers
    ret = OMX_SendCommand(window->component.handle,
		    OMX_CommandPortEnable,
		    90, NULL);
    if(ret != 0)
    	printf("Failed to enable input port. %x\n",ret);

    window->inputBufferHeaderCount = portdef.nBufferCountActual;

    window->ppInputBufferHeaders = malloc(sizeof(void)*window->inputBufferHeaderCount);
    window->bufImages = malloc(sizeof(void)*window->inputBufferHeaderCount);
    window->bufSize = window->screenWidth*window->screenHeight*4;

    printf("buf size: %d\n",portdef.nBufferSize);

    for(int i = 0;i<window->inputBufferHeaderCount;i++)
    {
    	window->bufImages[i] = malloc(portdef.nBufferSize);

    	ret = OMX_UseBuffer(window->component.handle,
					   &window->ppInputBufferHeaders[i],
					   90,
					   (void *) NULL,
					   portdef.nBufferSize,
					   (uint8_t*)window->bufImages[i]);
		if(ret != OMX_ErrorNone) {
				printf("Allocate resize input buffer, err: %x\n",ret);
				return;
			}
		window->ppInputBufferHeaders[i]->nFilledLen = window->bufSize;
    }

    window->lastBufUsed = 0;

    // wait for port enable to complete - which it should once buffers are
    // assigned
    ret =
	ilclient_wait_for_event(window->component.component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				90, 0,
				0, 20);
    if (ret != 0) {
		fprintf(stderr, "Did not get port enable %d\n", ret);
		return ;
    }

    // start executing the decoder
    ret = OMX_SendCommand(window->component.handle,
			  OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (ret != 0) {
		fprintf(stderr, "Error starting image decoder %x\n", ret);
		return;
    }

    ret = ilclient_wait_for_event(window->component.component,
				  OMX_EventCmdComplete,
				  OMX_CommandStateSet, 0, OMX_StateExecuting, 0, 0,
				  20);
    if (ret != 0) {
    	fprintf(stderr, "Did not receive executing stat %d\n", ret);
		return ;
    }

}

void pis_setWindowOpacity(pis_window *window, float opacity)
{
	OMX_CONFIG_DISPLAYREGIONTYPE region;

	if(opacity > 1)
		opacity = 1;
	if(opacity < 0)
		opacity = 0;

    region.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
    region.nVersion.nVersion = OMX_VERSION;
    region.nPortIndex = 90;
    region.alpha = opacity * 255;
    region.set = OMX_DISPLAY_SET_ALPHA  ;

    int ret = OMX_SetConfig(window->component.handle,
    	OMX_IndexConfigDisplayRegion, &region);
    if(ret != 0)
        	printf("Set Opacity Error %x\n", ret);
}

void pis_updateScreen(pis_window *window)
{
	window->ppInputBufferHeaders[window->lastBufUsed]->nFilledLen = window->screenWidth*window->screenHeight*4;
	OMX_EmptyThisBuffer(window->component.handle,
				window->ppInputBufferHeaders[window->lastBufUsed]);
	window->lastBufUsed = (window->lastBufUsed + 1) % window->inputBufferHeaderCount;
}

pis_window *pis_NewRenderWindow(uint8_t layer)
{
	pis_window *ret = malloc(sizeof(pis_window));

	ret->layer = layer;

	graphics_get_display_size(0,&ret->screenWidth,&ret->screenHeight);

	createComponent(ret);
	initDisplayRegion(ret);

	return ret;
}

//TODO: Window cleanup function
