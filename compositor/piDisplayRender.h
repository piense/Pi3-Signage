#pragma once

#include <stdint.h>
#include "bcm_host.h"
#include "ilclient/ilclient.h"

typedef struct {
    COMPONENT_T    *component;
    OMX_HANDLETYPE  handle;
    int             inPort;
}renderComponent;

typedef struct {
	renderComponent component;
    ILCLIENT_T     *client;
    uint32_t	screenWidth;
    uint32_t	screenHeight;
    uint32_t		**bufImages;
    OMX_BUFFERHEADERTYPE **ppInputBufferHeaders;
    uint8_t inputBufferHeaderCount;
    uint8_t lastBufUsed;
    uint8_t	layer;
    uint32_t bufSize;
}pis_window;

pis_window *pis_NewRenderWindow(uint8_t layer);

void pis_fillBuf(pis_window *window, uint32_t color);

//Copy image into screen buffer
void pis_bufCopy(pis_window *window, uint32_t *img,
		uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight,
		uint32_t dstX, uint32_t dstY);

//Swap in next buffer
void pis_updateScreen(pis_window *window);

//Opacity of window on screen
void pis_setWindowOpacity(pis_window *window, float opacity);
