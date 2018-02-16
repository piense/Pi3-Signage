#include "piSlideRenderer.h"

#include <stdint.h>
#include <stdio.h>

#include "tricks.h"
#include "pijpegdecoder.h"
#include "piImageResizer.h"
#include "fontTest.h"


PiSlideRenderer::PiSlideRenderer()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::PiSlideRenderer()\n");

    mainDisplay = vc_dispmanx_display_open( 0 );

    if(mainDisplay == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"PiSlideRenderer: Error, unable to open main display\n");
    	//TODO Fail harder
    	//return;
    	//DON'T RETURN: FLAG ERROR AND INITIALIZE OTHER VARIABLES
    }else
    	pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer: main display opened.\n");


    int ret = vc_dispmanx_display_get_info( mainDisplay, &mainDisplayInfo);
    if(ret != 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"PiSlideRenderer: Error, unable to get info from main dispslay\n");
    	//TODO Fail harder
    	//return;
    	//DON'T RETURN: FLAG ERROR AND INITIALIZE OTHER VARIABLES
    }else
    	pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer: Main display is %dx%d\n",
    			mainDisplayInfo.width, mainDisplayInfo.height );

    vc_dispmanx_rect_set(&screenRect,0,0,mainDisplayInfo.width,mainDisplayInfo.height);
    vc_dispmanx_rect_set(&srcRect,0,0,mainDisplayInfo.width<<16,mainDisplayInfo.height<<16);

    offscreenDispResourceHandle = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
    		(int32_t)mainDisplayInfo.width, (int32_t)mainDisplayInfo.height,
			&offscreenDispBufferHandle);

    if(offscreenDispResourceHandle == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer: Unable to allocate off screen resource.\n");
    	//TODO: Cleanup and damage control
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"PiSlideRenderer: Off screen display resource created.\n");
    }

    offscreenDisplay = vc_dispmanx_display_open_offscreen(offscreenDispResourceHandle,DISPMANX_NO_ROTATE);

    if(offscreenDisplay == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer: Unable to open display off screen.\n");
    	//TODO: Cleanup and damage control
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"PiSlideRenderer: Off screen display opened.\n");
    }

    alpha.flags = (DISPMANX_FLAGS_ALPHA_T)(DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX);
    alpha.opacity = 0; //alpha 0->255
    alpha.mask = 0;

    uint32_t noHandle;
    DISPMANX_RESOURCE_HANDLE_T blackRes = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
    		(int32_t)mainDisplayInfo.width, (int32_t)mainDisplayInfo.height,
			&noHandle);

    if(offscreenDispResourceHandle == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer: Unable to allocate off screen black resource.\n");
    	//TODO: Cleanup and damage control
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"PiSlideRenderer: Off screen display black resource created.\n");
    }

    uint32_t stride = ALIGN_UP(mainDisplayInfo.width,16);
    uint32_t *black = new uint32_t[stride*mainDisplayInfo.height];

    for(int x = 0;x<mainDisplayInfo.width;x++)
    	for(int y = 0;y<mainDisplayInfo.height;y++)
    		black[x+y*stride] = 0xFF<<24;

	ret = vc_dispmanx_resource_write_data(blackRes,
			VC_IMAGE_ARGB8888, stride*4,black,
			&screenRect
			);

	if(ret != 0)
		pis_logMessage(PIS_LOGLEVEL_ERROR,"PiSlideRenderer: Unable to copy black slide to resource\n");

	free(black);

    uint32_t update = vc_dispmanx_update_start(10);



	mainDisplayElementHandle = vc_dispmanx_element_add(update,
			mainDisplay,
			0,
			&screenRect,
			offscreenDispResourceHandle,
			&srcRect,
			DISPMANX_PROTECTION_NONE,
			&alpha,
			NULL,
			DISPMANX_NO_ROTATE
			);

    if(mainDisplayElementHandle == 0){
    	pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer: Unable to add slide element to main display.\n");
    	//TODO: Cleanup and damage control
    }else{
    	pis_logMessage(PIS_LOGLEVEL_ALL,"PiSlideRenderer: Added slide element to .\n");
    }

	alpha.opacity = 255;

    //Create a solid black background for the slide
	vc_dispmanx_element_add(update,
			offscreenDisplay,
		5,
		&screenRect,
		blackRes,
		&srcRect,
		DISPMANX_PROTECTION_NONE,
		&alpha,
		NULL,
		DISPMANX_NO_ROTATE
		);

	vc_dispmanx_update_submit_sync( update );


	slideStartTime = 0;

	dispmanxLayer = 0;
	slide = NULL;
	state = SLIDE_STATE_IDLE;
	opacity = 0;
}

PiSlideRenderer::~PiSlideRenderer()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::~PiSlideRenderer()\n");
	if(mainDisplayElementHandle != 0)
	{
		uint32_t update = vc_dispmanx_update_start(10);
		vc_dispmanx_element_remove(update,mainDisplayElementHandle);
		vc_dispmanx_update_submit_sync( update );
	}

	if(offscreenDisplay != 0)
		vc_dispmanx_display_close(offscreenDisplay);

	if(offscreenDispResourceHandle != 0)
		vc_dispmanx_resource_delete(offscreenDispResourceHandle);
}

void PiSlideRenderer::setDispmanxLayer(uint32_t layer)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::setDispmanxLayer(%d)\n",layer);
	dispmanxLayer = layer;
}


sImage * loadImage(char *filename, uint16_t maxWidth, uint16_t maxHeight, pis_mediaSizing scaling)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::loadImage: %s\n",filename);

	sImage *ret1 = NULL;
	sImage *ret2 = NULL;
	sImage *ret3 = NULL;
	PiImageDecoder decoder;
	PiImageResizer resize;
	int err;
	bool extraLine = false;
	bool extraColumn = false;
	double endMs;
	double startMs = linuxTimeInMs();

	//TODO: Create a component with the decoding, color space, and resize tunneled together

	err = decoder.DecodeJpegImage(filename, &ret1);

	if(err != 0 || ret1 == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "loadImage: Error loading JPEG %s\n", filename);
		goto error;
	}

	pis_logMessage(PIS_LOGLEVEL_ALL,"Compositor: Returned from decoder: %dx%d Size: %d Stride: %d Slice Height: %d\n",
			ret1->imageWidth,ret1->imageHeight,
			ret1->imageSize, ret1->stride, ret1->sliceHeight
			);

	//Decoder buffer logic makes this save, sliceHeight is always rounded up to an even number
	if(ret1->imageHeight%2 != 0) { ret1->imageHeight += 1; extraLine = true; }
	if(ret1->imageWidth%2 != 0) { ret1->imageWidth += 1; extraColumn = true; }

	//Resizer does odd things with YUV so
	//Convert to ARGB8888 first
	err =  resize.ResizeImage (
			(char *)ret1->imageBuf,
			ret1->imageWidth, ret1->imageHeight,
			ret1->imageSize,
			(OMX_COLOR_FORMATTYPE) ret1->colorSpace,
			ret1->stride,
			ret1->sliceHeight,
			ret1->imageWidth, ret1->imageHeight,
			pis_SIZE_STRETCH,
			&ret2
			);

	if(extraLine == true) ret2->imageHeight--;
	if(extraColumn == true) ret2->imageWidth--;

	if(err != 0 || ret2 == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "loadImage: Error converting color space\n");
		goto error;
	}

	err =  resize.ResizeImage (
			(char *)ret2->imageBuf,
			ret2->imageWidth, ret2->imageHeight,
			ret2->imageSize,
			(OMX_COLOR_FORMATTYPE) ret2->colorSpace,
			ret2->stride,
			ret2->sliceHeight,
			maxWidth,maxHeight,
			scaling,
			&ret3
			);

	if(err != 0 || ret3 == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "loadImage: Error resizing image.\n");
		goto error;
	}

	free(ret2->imageBuf);
	free(ret2);

    free(ret1->imageBuf);

    free(ret1);

    endMs = linuxTimeInMs();

    pis_logMessage(PIS_LOGLEVEL_INFO, "Resized image in %f seconds.\n",(endMs-startMs)/1000.0);
	return ret3;

	error:
	if(ret1 != NULL && ret1->imageBuf != NULL)
		free(ret1->imageBuf);
	if(ret1 != NULL)
		free(ret1);

	if(ret2 != NULL && ret2->imageBuf != NULL)
		free(ret2->imageBuf);
	if(ret2 != NULL)
		free(ret2);

	if(ret3 != NULL && ret3->imageBuf != NULL)
		free(ret3);
	if(ret3 != NULL)
		free(ret3);

	return NULL;
}

void PiSlideRenderer::freeSlideResources()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::freeSlideResources()\n");

	state = SLIDE_STATE_UNLOADING;

	double endTime;
	double startTime2 = linuxTimeInMs();

	if(slide == NULL)
	{
		state = SLIDE_STATE_IDLE;
		return;
	}

	pis_mediaElementList_s *current = slide->mediaElementsHead;

	pis_mediaImage* dataImg = NULL;
	pis_mediaText* dataTxt = NULL;

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				dataImg = ((pis_mediaImage *)current->mediaElement.data);

				if(dataImg->element != 0)
					pis_logMessage(PIS_LOGLEVEL_WARN, "PiSlideRenderer::freeSlideResources(): Deleting a resource in use, this will probably end poorly.\n");

				if(dataImg->cache.img != NULL)
					free(dataImg->cache.img);

				dataImg->cache.img = NULL;
				dataImg->cache.width =  0;
				dataImg->cache.height =  0;
				dataImg->cache.stride = 0;

				if(dataImg->res != 0){
					vc_dispmanx_resource_delete(dataImg->res);
					dataImg->res = 0;
				}

				break;

			//TODO:
			case pis_MEDIA_VIDEO:

				break;

			case pis_MEDIA_TEXT:
				dataTxt = ((pis_mediaText *)current->mediaElement.data);

				if(dataTxt->element != 0)
					pis_logMessage(PIS_LOGLEVEL_WARN,"PiSlideRenderer::freeSlideResources(): Deleting a resource in use, this will probably end poorly.\n");

				if(dataTxt->res != 0){
					vc_dispmanx_resource_delete(dataTxt->res);
					dataTxt->res = 0;
				}
				break;

			//TODO:
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	endTime = linuxTimeInMs();
	pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::freeSlideResources(): Freed All: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));

	state = SLIDE_STATE_IDLE;
}

void PiSlideRenderer::startSlide()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::startSlide()\n");
	slideStartTime = linuxTimeInMs();
	state = SLIDE_STATE_APPEARING;
}

void PiSlideRenderer::loadSlideResources()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::loadSlideResources()\n");
	state = SLIDE_STATE_LOADING;

	if(slide == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"PiSlideRenderer::loadSlideResources(): Error: no slide to load.\n");
		return;
	}

	pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::loadSlideResources: Loading slide resources\n");
	double startTime, endTime;
	double startTime2 = linuxTimeInMs();
	int ret;

	pis_mediaImage* dataImg = NULL;
	pis_mediaText* dataTxt = NULL;

	pis_mediaElementList_s *current = slide->mediaElementsHead;

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				pis_logMessage(PIS_LOGLEVEL_ALL,"Adding image to slide buffer\n");
				//TODO: range check width & height
				startTime = linuxTimeInMs();

				dataImg = ((pis_mediaImage *)current->mediaElement.data);

				if(dataImg->res == 0)
				{
					sImage *img = loadImage(dataImg->filename,
							dataImg->maxWidth * mainDisplayInfo.width,
							dataImg->maxHeight * mainDisplayInfo.height,
							dataImg->sizing);

					if(img == NULL || img->imageBuf == NULL)
					{
						pis_logMessage(PIS_LOGLEVEL_ERROR,"loadSlideResources: Error loading image\n");
						break;
					}

					dataImg->cache.img = (uint32_t *) img->imageBuf;
					dataImg->cache.width =  img->imageWidth;
					dataImg->cache.height =  img->imageHeight;
					dataImg->cache.stride = img->stride;

					free(img);

					dataImg->res = 0;
					dataImg->res = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888, //ARGB for pngs later on
							dataImg->cache.width,dataImg->cache.height,
							&dataImg->imgHandle);

					if(dataImg->res == 0){
						free(dataImg->cache.img);
						dataImg->cache.img = NULL;
						pis_logMessage(PIS_LOGLEVEL_ERROR,"loadSlideResources: Error creating image resource\n");
						break;
					}

					VC_RECT_T rect = {0,0,(int32_t)dataImg->cache.width,(int32_t)dataImg->cache.height };
					ret = vc_dispmanx_resource_write_data(dataImg->res,
							VC_IMAGE_ARGB8888, dataImg->cache.stride,dataImg->cache.img,
							&rect
							);
					if(ret != 0){
						pis_logMessage(PIS_LOGLEVEL_ERROR,"loadSlideResources: Error writing image data to dispmanx resource %d\n", ret);
					}else{
						pis_logMessage(PIS_LOGLEVEL_ALL,"loadSlideResources: Resource loaded.\n");
					}

					free(dataImg->cache.img);
					dataImg->cache.img = NULL;
				}else{
					pis_logMessage(PIS_LOGLEVEL_ALL,"loadSlideResources: Image resource != 0\n");
				}
				pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::loadSlideResources: Load Image: %f ms\n",linuxTimeInMs()-startTime);
				break;

			//TODO:
			case pis_MEDIA_VIDEO:
				//TODO: range check width & height
				startTime = linuxTimeInMs();

				pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::loadSlideResources: Load Video: %f ms\n",linuxTimeInMs()-startTime);
				break;

			case pis_MEDIA_TEXT:
				pis_logMessage(PIS_LOGLEVEL_ALL,"Adding text to slide buffer\n");
				startTime = linuxTimeInMs();

				//TODO: range check width & height
				dataTxt = ((pis_mediaText *)current->mediaElement.data);

				if(dataTxt->res == 0)
				{
					//TODO Need to factor in stride for dispmanx to be happy
					//at all resolutions
					dataTxt->res = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
							mainDisplayInfo.width,mainDisplayInfo.height,
							&((pis_mediaText *)current->mediaElement.data)->imgHandle);

					if(dataTxt->res == 0)
					{
						pis_logMessage(PIS_LOGLEVEL_ERROR,"loadSlideResources Error: Couldn't allocate resource for text.\n");
						break;
					}

					//Might just put this malloc somewhere globally to avoid time allocating every time
					uint32_t *temp= new uint32_t[mainDisplayInfo.width*mainDisplayInfo.height];

					if(temp == NULL){
						//Should we free dispmanx_resource here? Should be done in slide cleanup even if this didn't work
						pis_logMessage(PIS_LOGLEVEL_ERROR,"loadSlideResources Error: Couldn't allocate memory for text.\n");
						break;
					}

					memset(temp,0,mainDisplayInfo.width*mainDisplayInfo.height*4);

					renderTextToScreen(temp,
							mainDisplayInfo.width, mainDisplayInfo.height,
							dataTxt->x * mainDisplayInfo.width,
							dataTxt->y * mainDisplayInfo.height,
							dataTxt->fontHeight * mainDisplayInfo.height,
							dataTxt->text,
							(dataTxt->color & 0x00FF0000)>>16,
							(dataTxt->color & 0x0000FF00)>>8,
							dataTxt->color & 0x000000FF,
							(dataTxt->color & 0xFF000000)>>24
							);

					VC_RECT_T rectTxt = {0,0,(int32_t)mainDisplayInfo.width,(int32_t)mainDisplayInfo.height};
					ret = vc_dispmanx_resource_write_data(dataTxt->res,
							VC_IMAGE_ARGB8888, mainDisplayInfo.width*4,temp,
							&rectTxt
							);

					if(ret != 0)
						pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer::loadSlideResources: Error writing image data to dispmanx resource\n");

					free(temp);
				}

				pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::loadSlideResources: Loaded Text: %f\n",linuxTimeInMs()-startTime);
				break;

			//TODO:
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	endTime = linuxTimeInMs();
	pis_logMessage(PIS_LOGLEVEL_INFO, "PiSlideRenderer::loadSlideResources: Loaded All: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));


	state = SLIDE_STATE_LOADED;
}


void PiSlideRenderer::doUpdate(uint32_t update)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::doUpdate(%d)\n",update);

	float ms=linuxTimeInMs();

	//TODO Put states into switch statement

	if(state == SLIDE_STATE_APPEARING){
		opacity = ((float)(ms-slideStartTime))/slide->dissolve;
		if(opacity > 1)	{
			opacity = 1;
			state = SLIDE_STATE_LIVE;
		}
	}

	if(slide != NULL && state == SLIDE_STATE_LIVE &&
			ms > slideStartTime + slide->dissolve + slide->duration)
	{
		state = SLIDE_STATE_EXPIRED;
	}

	if(state == SLIDE_STATE_DISSAPPEARING)
	{
		opacity = 0;
		state = SLIDE_STATE_DONE;
	}

	//Setting the layer every time, might change to as-needed
	vc_dispmanx_element_change_attributes(update,
			mainDisplayElementHandle,
			1 , //To change: Layer
			dispmanxLayer, //Layer
			0, //Opacity
			0, //Dest rect
			0, //Src rect
			0, //mask
			DISPMANX_NO_ROTATE); //transform

	vc_dispmanx_element_change_attributes(update,
			mainDisplayElementHandle,
			2, //To change: Opacity
			0, //Layer
			opacity*255, //Opacity
			0, //Dest rect
			0, //Src rect
			0, //mask
			DISPMANX_NO_ROTATE); //transform

	compositeSlide(update);

	return;
}

void PiSlideRenderer::compositeSlide(uint32_t update)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::compositeSlide(%d)\n",update);

	double startTime, endTime;
	double startTime2 = linuxTimeInMs();

	if(slide == NULL)
		return;

	pis_mediaElementList_s *current = slide->mediaElementsHead;
	pis_mediaImage *dataImg = NULL;
    pis_mediaText *dataTxt = NULL;

	startTime = linuxTimeInMs();

	int layer = 2000;

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				startTime = linuxTimeInMs();
			    dataImg = (pis_mediaImage *)current->mediaElement.data;

				if(dataImg->res == 0)
				{
					pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer::compositeSlide: Error: Image not loaded\n");
				}else{
					if(dataImg->element == 0){
						pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer::compositeSlide: Adding image element\n");
						VC_RECT_T rectImg = {0,0,(int32_t)dataImg->cache.width<<16,(int32_t)dataImg->cache.height << 16};
						VC_RECT_T rectImg2 = {dataImg->x*mainDisplayInfo.width - dataImg->cache.width/2,
								dataImg->y*mainDisplayInfo.height - dataImg->cache.height/2,
								dataImg->cache.width,dataImg->cache.height};
						dataImg->element = vc_dispmanx_element_add(update,
								offscreenDisplay,
							layer++,
							&rectImg2,
							dataImg->res,
							&rectImg,
							DISPMANX_PROTECTION_NONE,
							&alpha,
							NULL,
							DISPMANX_NO_ROTATE
							);
						if(dataImg->element == 0)
							pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer::compositeSlide: Error: Unable to composite image.\n");
					}else{
						//Update element properties(?) for animation stuff
					}
				}
				pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer::compositeSlide: Image: %f\n",linuxTimeInMs()-startTime);
				break;

			//TODO:
			case pis_MEDIA_VIDEO:
				break;

			case pis_MEDIA_TEXT:
				startTime = linuxTimeInMs();
			    dataTxt = (pis_mediaText*)current->mediaElement.data;

				if(dataTxt->res == 0)
				{
					pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer::compositeSlide: Error: Text not loaded\n");
				}else{
					if(dataTxt->element == 0){
						pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer::compositeSlide: Adding text element\n");
						VC_RECT_T rectImg = {0,0,
								mainDisplayInfo.width<<16,mainDisplayInfo.height<<16};

						dataTxt->element = vc_dispmanx_element_add(update,
								offscreenDisplay,layer++,
								&screenRect,
								dataTxt->res,
								&rectImg,
								DISPMANX_PROTECTION_NONE,
								&alpha,
								NULL,
								DISPMANX_NO_ROTATE
								);
						if(dataTxt->element == 0)
							pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer::compositeSlide: Error compositing text.\n");
					}else{
						//TODO: Update text data, possibly for movement or dynamic text
					}
				}
				pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer::compositeSlide: Text: %f ms\n",linuxTimeInMs()-startTime);
				break;

			//TODO:
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	endTime = linuxTimeInMs();
	pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer::compositeSlide: Composited all: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));
}

void PiSlideRenderer::removeSlideFromDisplay()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"PiSlideRenderer::removeSlideFromDisplay()\n");
	double endTime;
	double startTime2 = linuxTimeInMs();

	if(slide == NULL)
		return;

	pis_mediaElementList_s *current = slide->mediaElementsHead;
	pis_mediaImage* dataImg = NULL;
	pis_mediaText* dataTxt = NULL;

	uint32_t update = vc_dispmanx_update_start(10);

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				dataImg = ((pis_mediaImage *)current->mediaElement.data);

				if(dataImg->element != 0){
					vc_dispmanx_element_remove(update, dataImg->element);
				}

				dataImg->element = 0;
				break;

			//TODO:
			case pis_MEDIA_VIDEO:

				break;

			case pis_MEDIA_TEXT:
				dataTxt = ((pis_mediaText *)current->mediaElement.data);

				if(dataTxt->element != 0)
					vc_dispmanx_element_remove(update, dataTxt->element);

				dataTxt->element = 0;
				break;

			//TODO:
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	endTime = linuxTimeInMs();
	pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer::removeSlideFromDisplay: Removed All: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));

	vc_dispmanx_update_submit_sync(update);
}

float PiSlideRenderer::getOpacity()
{
	return opacity;
}
