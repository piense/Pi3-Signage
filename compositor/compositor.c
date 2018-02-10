//System headers
#include <time.h>
#include <stdio.h>
#include <dirent.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>

//External libs
#include "bcm_host.h"
#include "vgfont/vgfont.h"

//Project includes
#include "compositor.h"
#include "pijpegdecoder.h"
#include "piresizer.h"
#include "tricks.h"
#include "textoverlay.h"
#include "fontTest.h"
#include "../graphicsTests.h"


//TODO: put this in a struct and out of the global namespace
DISPMANX_DISPLAY_HANDLE_T   display, offDisp1, offDisp2;
DISPMANX_DISPLAY_HANDLE_T dissolveDisp, backDisp, tempDisp;
uint32_t	offDisp1Buf, offDisp2Buf;
DISPMANX_MODEINFO_T         info;
DISPMANX_UPDATE_HANDLE_T    update;
DISPMANX_RESOURCE_HANDLE_T  offDisp1Resource, offDisp2Resource;
DISPMANX_RESOURCE_HANDLE_T dissolveDispRes, backDispRes, tempDispRes;
DISPMANX_ELEMENT_HANDLE_T   dispElement1, dispElement2, dissolveElement, backElement, tempElement;
uint32_t                    vc_image_ptr;
uint32_t					screen;
VC_DISPMANX_ALPHA_T alpha, alphaDissolve;
VC_RECT_T srcRect, screenRect;


double linuxTimeInMs()
{
	time_t		s2;
	struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s2  = spec.tv_sec;
    return round(spec.tv_nsec / 1.0e6)+s2*1000; // Convert nanoseconds to milliseconds
}

sImage * loadImage(char *filename, uint16_t maxWidth, uint16_t maxHeight)
{
	double startMs = linuxTimeInMs();

	sImage *ret1 = decodeJpgImage(filename);

	printf("Loaded image in %f seconds.\n",(linuxTimeInMs()-startMs)/1000.0);

	startMs = linuxTimeInMs();

	printf("Resizing to: %dx%d\n",maxWidth,maxHeight);

    sImage *ret2 = resizeImage2((char *)ret1->imageBuf, ret1->imageWidth, ret1->imageHeight,
    		ret1->imageSize,
			ret1->colorSpace,
			ret1->stride,
			ret1->sliceHeight,
			maxWidth,maxHeight,
			0, 1
			);

    double endMs = linuxTimeInMs();

    free(ret1->imageBuf);

    free(ret1);

    printf("Resized image in %f seconds.\n",(endMs-startMs)/1000.0);
	return ret2;
}

pis_compositorErrors pis_addNewSlide(pis_slides_s **ret, uint32_t dissolveTime, uint32_t duration, char *name)
{

	pis_slides_s *newSlide = malloc(sizeof(pis_slides_s));
	newSlide->dissolve = dissolveTime;
	newSlide->duration = duration;
	newSlide->mediaElementsHead = NULL;
	newSlide->next = NULL;

	if(pis_compositor.slides == NULL)
	{
		pis_compositor.slides = newSlide;
	}else{
		pis_slides_s *current = pis_compositor.slides;
		while(current->next != NULL)
			current = current->next;
		current->next = newSlide;
	}

	*ret = newSlide;

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_AddTextToSlide(pis_slides_s *slide, char* text, char* fontName, float height, float x, float y, uint32_t color)
{
	pis_mediaElementList_s *newMedia = malloc(sizeof(pis_mediaElementList_s));
	newMedia->mediaElement.data = malloc(sizeof(pis_mediaText));
	newMedia->mediaElement.mediaType = pis_MEDIA_TEXT;
	pis_mediaText *data = ((pis_mediaText *)newMedia->mediaElement.data);

	char *tempTxt = malloc(strlen(text)+1);
	strcpy(tempTxt,text);
	data->text = tempTxt;

	tempTxt = malloc(strlen(fontName)+1);
	strcpy(tempTxt,fontName);
	data->fontName = tempTxt;

	data->fontHeight = height;
	data->x = x;
	data->y = y;
	data->color = color;
	data->res = 0;
	data->element = 0;

	newMedia->next = slide->mediaElementsHead;
	newMedia->mediaElement.name = "Text";
	slide->mediaElementsHead = newMedia;
	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_AddImageToSlide(pis_slides_s *slide, char* file,
		float x, float y, float width, float height, pis_mediaSizing sizing)
{
	pis_mediaElementList_s *newMedia = malloc(sizeof(pis_mediaElementList_s));
	newMedia->mediaElement.data = malloc(sizeof(pis_mediaImage));
	newMedia->mediaElement.mediaType = pis_MEDIA_IMAGE;

	pis_mediaImage* data = ((pis_mediaImage *)newMedia->mediaElement.data);

	//TODO range check x,y,width,height

	char *lfile = malloc(strlen(file)+1);
	strcpy(&lfile[0],&file[0]);

	data->filename = lfile;
	data->maxHeight = height;
	data->maxWidth = width;
	data->x = x;
	data->y = y;
	data->sizing = sizing;

	data->cache.img = NULL;
	data->cache.width = 0;
	data->cache.height = 0;
	data->res = 0;
	data->element = 0;

	newMedia->next = slide->mediaElementsHead;
	newMedia->mediaElement.name = "Image";
	slide->mediaElementsHead = newMedia;
	return pis_COMPOSITOR_ERROR_NONE;
}

void pis_removeSlideFromDisplay(pis_slides_s *slide)
{
	printf("Removing slide elements.\n");
	double endTime;
	double startTime2 = linuxTimeInMs();

	pis_mediaElementList_s *current = slide->mediaElementsHead;

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				;
				pis_mediaImage* data = ((pis_mediaImage *)current->mediaElement.data);

				if(data->element != 0){
					vc_dispmanx_element_remove(update, data->element);
				}

				data->element = 0;
				break;

			//TODO:
			case pis_MEDIA_VIDEO:

				break;

			case pis_MEDIA_TEXT:
				;
				pis_mediaText* dataTxt = ((pis_mediaText *)current->mediaElement.data);

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
	printf("Removed All: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));

	printf("-----\n");
}

void pis_freeSlideResource(pis_slides_s *slide)
{
	printf("clearing slide resources.\n");
	double endTime;
	double startTime2 = linuxTimeInMs();

	pis_mediaElementList_s *current = slide->mediaElementsHead;

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				;
				pis_mediaImage* data = ((pis_mediaImage *)current->mediaElement.data);

				if(data->element != 0)
					printf("Deleting a resource in use, this will probably end poorly.\n");

				if(data->cache.img != NULL)
					free(data->cache.img);
				data->cache.img = NULL;
				data->cache.width =  0;
				data->cache.height =  0;
				data->cache.stride = 0;

				if(data->res != 0){
					vc_dispmanx_resource_delete(data->res);
				}

				data->res = 0;
				break;

			//TODO:
			case pis_MEDIA_VIDEO:

				break;

			case pis_MEDIA_TEXT:
				;
				pis_mediaText* dataTxt = ((pis_mediaText *)current->mediaElement.data);

				if(dataTxt->element != 0)
					printf("Deleting a resource in use, this will probably end poorly.\n");

				if(dataTxt->res != 0)
					vc_dispmanx_resource_delete(dataTxt->res);
				dataTxt->res = 0;
				break;

			//TODO:
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	endTime = linuxTimeInMs();
	printf("Freed All: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));

	printf("-----\n");
}

void pis_loadSlideResources(pis_slides_s *slide)
{
	printf("Loading slide resources.\n");
	double startTime, endTime;
	double startTime2 = linuxTimeInMs();
	int ret;

	pis_mediaElementList_s *current = slide->mediaElementsHead;

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				//TODO: range check width & height
				startTime = linuxTimeInMs();

				pis_mediaImage* data = ((pis_mediaImage *)current->mediaElement.data);

				if(data->res == 0)
					{
					sImage *img = loadImage(data->filename,
											data->maxWidth * pis_compositor.screenWidth,
											data->maxHeight * pis_compositor.screenHeight);

					data->cache.img = (uint32_t *) img->imageBuf;
					data->cache.width =  img->imageWidth;
					data->cache.height =  img->imageHeight;
					data->cache.stride = img->stride;

					free(img);

					data->res = 0;
					data->res = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888, //ARGB for pngs later on
							data->cache.width,data->cache.height,
							&data->imgHandle);

					if(data->res == 0)
						printf("Error creating image resource\n");

					VC_RECT_T rect = {0,0,data->cache.width,data->cache.height };
					ret = vc_dispmanx_resource_write_data(data->res,
							VC_IMAGE_ARGB8888, data->cache.stride,data->cache.img,
							&rect
							);
					if(ret != 0)
						printf("Error writing image data to dispmanx resource\n");
					}else{
						printf("Resource already loaded.\n");
					}

					free(data->cache.img);
					data->cache.img = NULL;

				printf("Image: %f\n",linuxTimeInMs()-startTime);
				break;

			//TODO:
			case pis_MEDIA_VIDEO:
				//TODO: range check width & height
				startTime = linuxTimeInMs();

				printf("Video: %f\n",linuxTimeInMs()-startTime);
				break;

			case pis_MEDIA_TEXT:
				startTime = linuxTimeInMs();

				//TODO: range check width & height
				pis_mediaText* dataTxt = ((pis_mediaText *)current->mediaElement.data);

				if(dataTxt->res == 0)
				{
					//TODO Need to factor in stride for dispmanx to be happy
					//at all resolutions
					dataTxt->res = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
							pis_compositor.screenWidth,pis_compositor.screenHeight,
							&((pis_mediaText *)current->mediaElement.data)->imgHandle);

					//Might just put this malloc somewhere globally to avoid time allocating every time
					uint32_t *temp=malloc(pis_compositor.screenWidth*pis_compositor.screenHeight*4);
					memset(temp,0,pis_compositor.screenWidth*pis_compositor.screenHeight*4);

					renderTextToScreen(temp,
							pis_compositor.screenWidth, pis_compositor.screenHeight,
							dataTxt->x * pis_compositor.screenWidth,
							dataTxt->y * pis_compositor.screenHeight,
							dataTxt->fontHeight * pis_compositor.screenHeight,
							dataTxt->text,
							(dataTxt->color & 0x00FF0000)>>16,
							(dataTxt->color & 0x0000FF00)>>8,
							dataTxt->color & 0x000000FF,
							(dataTxt->color & 0xFF000000)>>24
							);

					VC_RECT_T rectTxt = {0,0,pis_compositor.screenWidth,pis_compositor.screenHeight};
					ret = vc_dispmanx_resource_write_data(dataTxt->res,
							VC_IMAGE_ARGB8888, pis_compositor.screenWidth*4,temp,
							&rectTxt
							);

					if(ret != 0)
						printf("Error writing image data to dispmanx resource\n");

					free(temp);
				}

				printf("Text: %f\n",linuxTimeInMs()-startTime);
				break;

			//TODO:
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	endTime = linuxTimeInMs();
	printf("Loaded All: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));

	printf("-----\n");

}

void pis_initDispmanx()
{
	printGPUMemory();

    printf("Open display[%i]...\n", screen );
    display = vc_dispmanx_display_open( 0 );

    int ret = vc_dispmanx_display_get_info( display, &info);
    assert(ret == 0);
    printf( "Display is %dx%d\n", info.width, info.height );

    vc_dispmanx_rect_set(&screenRect,0,0,info.width,info.height);
    vc_dispmanx_rect_set(&srcRect,0,0,info.width<<16,info.height<<16);

    offDisp1Resource = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888, info.width, info.height, &offDisp1Buf);

    if(offDisp1Resource == 0)
    	printf("Unable to allocate offDisp1Resource.\n");

    offDisp2Resource = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888, info.width, info.height, &offDisp2Buf);

    if(offDisp2Resource == 0)
    	printf("Unable to allocate offDisp2Resource.\n");

    offDisp1 = vc_dispmanx_display_open_offscreen(offDisp1Resource,DISPMANX_NO_ROTATE);

    if(offDisp1 == 0)
    	printf("Unable to open offDisp1\n");

    offDisp2 = vc_dispmanx_display_open_offscreen(offDisp2Resource,DISPMANX_NO_ROTATE);

    if(offDisp2 == 0)
        	printf("Unable to open offDisp2\n");

    alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX ;
    alpha.opacity = 255; /*alpha 0->255*/
    alpha.mask = 0;

    alphaDissolve.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX;
    alphaDissolve.opacity = 0; /*alpha 0->255*/
    alphaDissolve.mask = 0;

    backDisp = offDisp1;
    dissolveDisp = offDisp2;

    backDispRes = offDisp1Resource;
    dissolveDispRes = offDisp2Resource;
}

pis_compositorErrors pis_compositeSlide(DISPMANX_DISPLAY_HANDLE_T dispRes, pis_slides_s *slide)
{
	double startTime, endTime;
	double startTime2 = linuxTimeInMs();

	pis_mediaElementList_s *current = slide->mediaElementsHead;

	startTime = linuxTimeInMs();

	printf("Compositing slide resources.\n");

	int layer = 2000;

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				startTime = linuxTimeInMs();
			    pis_mediaImage *dataImg = current->mediaElement.data;

				if(dataImg->res == 0)
				{
					printf("Image not loaded\n");
				}else{
					if(dataImg->element == 0){
						printf("Adding image element\n");
						VC_RECT_T rectImg = {0,0,dataImg->cache.width<<16,dataImg->cache.height << 16};
						VC_RECT_T rectImg2 = {dataImg->x*pis_compositor.screenWidth - dataImg->cache.width/2,
								dataImg->y*pis_compositor.screenHeight - dataImg->cache.height/2,
								dataImg->cache.width,dataImg->cache.height};
						dataImg->element = vc_dispmanx_element_add(update,dispRes,
							layer++,
							&rectImg2,
							dataImg->res,
							&rectImg,
							DISPMANX_PROTECTION_NONE,
							&alpha,
							NULL,
							VC_IMAGE_ROT0
							);
						if(dataImg->element == 0)
							printf("Unable to composite image.\n");
					}else{
						//Update element properties(?) for animation stuff
					}
				}
				printf("Image: %f\n",linuxTimeInMs()-startTime);
				break;

			//TODO:
			case pis_MEDIA_VIDEO:
				break;

			case pis_MEDIA_TEXT:
				startTime = linuxTimeInMs();
			    pis_mediaText *dataTxt = current->mediaElement.data;

				if(dataTxt->res == 0)
				{
					printf("Text not loaded\n");
				}else{
					if(dataTxt->element == 0){
						printf("Adding text element\n");
						VC_RECT_T rectImg = {0,0,
								pis_compositor.screenWidth<<16,pis_compositor.screenHeight<<16};

						dataTxt->element = vc_dispmanx_element_add(update,dispRes,layer++,
								&screenRect,
								dataTxt->res,
								&rectImg,
								DISPMANX_PROTECTION_NONE,
								&alpha,
								NULL,
								VC_IMAGE_ROT0
								);
						if(dataTxt->element == 0)
							printf("Error compositing text.\n");
					}else{
						//TODO: Update text data, possibly for movement or dynamic text
					}
				}
				printf("Text: %f\n",linuxTimeInMs()-startTime);
				break;

			//TODO:
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	endTime = linuxTimeInMs();
	printf("Composited all: %f ms, %f fps.\n",endTime - startTime2,1000/(endTime - startTime2));

	printf("-----\n");

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_loadDirectory(char* directory)
{
	DIR           *d;
	struct dirent *dir;
	d = opendir(directory);

	if(d == NULL){
		printf("Error opening images directory.\n");
	}

	pis_slides_s *newSlide;

	char name[1000];
	char fullpath[1000];
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if(strstr(dir->d_name, ".jpg") != NULL ||strstr(dir->d_name, ".jpeg") != NULL ) {

				sprintf(&name[0],"%s",dir->d_name);
				sprintf(&fullpath[0],"%s/%s",directory,dir->d_name);

				printf("\nAdding: %s %s\n",name,fullpath);

				pis_addNewSlide(&newSlide, 1000,5000,"Image Slide");

				pis_AddTextToSlide(newSlide,&name[0],"",50.0/1080.0,.5,.95,0xFF000000);

				pis_AddImageToSlide(newSlide,&fullpath[0],
						.5,.5, //position [0,1]
						1,1 //size [0,1]
						,pis_SIZE_SCALE);

			}
		}
	closedir(d);
	}

	return 0;
}

pis_compositorErrors pis_initializeCompositor()
{
	bcm_host_init();

//	printFontList();

	pis_initDispmanx();

	graphics_get_display_size(0,&pis_compositor.screenWidth,&pis_compositor.screenHeight);

	pis_compositor.state = 3;

	pis_compositor.slideHoldTime = 0;
	pis_compositor.slideStartTime = 0;
	pis_compositor.slideDissolveTime = 0;

	//Demo slides
	pis_loadDirectory("/mnt/data/images");

	pis_compositor.currentSlide = NULL;
	pis_compositor.lastSlide = NULL;
	pis_compositor.nextSlide = pis_compositor.slides;

	//Create a solid background for the two off screen windows
	uint32_t *temp = malloc(pis_compositor.screenHeight*pis_compositor.screenWidth*4);
	uint32_t x,y;
	for(x = 0;x<pis_compositor.screenWidth;x++)
		for(y = 0;y<pis_compositor.screenHeight;y++)
			temp[x+y*pis_compositor.screenWidth] = 0xFF << 24;

	uint32_t tempBlah1;
	uint32_t back1Res = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888,
			pis_compositor.screenWidth,pis_compositor.screenHeight,&tempBlah1);

	//TODO account for odd pitches to make dispmanx happy
	vc_dispmanx_resource_write_data(back1Res,VC_IMAGE_ARGB8888,pis_compositor.screenWidth*4,temp,&screenRect);

	free(temp);

	update = vc_dispmanx_update_start(10);

	//Add a black background to each offscreen buffer
	vc_dispmanx_element_add(update,dissolveDisp, 2,&screenRect,back1Res,&srcRect,
			DISPMANX_PROTECTION_NONE,&alpha,NULL,VC_IMAGE_ROT0);

	vc_dispmanx_element_add(update,backDisp, 2,&screenRect,back1Res,&srcRect,
				DISPMANX_PROTECTION_NONE,&alpha,NULL,VC_IMAGE_ROT0);

	//Add the offscreen buffers to the main display
	dispElement1 = vc_dispmanx_element_add(update,display,2000,&screenRect,backDispRes,&srcRect,
			DISPMANX_PROTECTION_NONE,&alpha,NULL,VC_IMAGE_ROT0);

	dispElement2 = vc_dispmanx_element_add(update,display,2001,&screenRect,dissolveDispRes,&srcRect,
				DISPMANX_PROTECTION_NONE,&alphaDissolve,NULL,VC_IMAGE_ROT0);

	backElement=dispElement1;
	dissolveElement=dispElement2;

	vc_dispmanx_update_submit_sync( update );

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_compositorcleanup()
{
	//TODO pis_DR cleanup

	return pis_COMPOSITOR_ERROR_NONE;
}

double msLastPrint;

pis_compositorErrors pis_doCompositor()
{
	float opacity;

	double ms = linuxTimeInMs();

	if(ms - msLastPrint > 1000)
	{
		printCPUandGPUMemory();
		msLastPrint = ms;
	}

	update = vc_dispmanx_update_start(10);

	switch(pis_compositor.state){

		//Slide transition Time
		case 0: opacity = ((float)(ms-pis_compositor.dissolveStartTime))/pis_compositor.slideDissolveTime;
			if(opacity > 1)	{
				opacity = 1;
				pis_compositor.state = 1;
				pis_compositor.slideStartTime = ms;
			}
			break;

		//Transition over, copy to opaque background layer
		case 1:
			//TODO: Move slide from dissolve layer to background layer

			tempDispRes = dissolveDispRes;
			dissolveDispRes = backDispRes;
			backDispRes = tempDispRes;

			tempDisp = dissolveDisp;
			dissolveDisp = backDisp;
			backDisp = tempDisp;

			tempElement = dissolveElement;
			dissolveElement = backElement;
			backElement = tempElement;

			pis_compositor.slideStartTime = ms;

			vc_dispmanx_element_change_attributes(update,
					backElement,
					1 , //To change: Layer
					2000, //Layer
					0, //Opacity
					0, //Dest rect
					0, //Src rect
					0, //mask
					DISPMANX_NO_ROTATE); //transform

			vc_dispmanx_element_change_attributes(update,
					dissolveElement,
					1 , //to change: Layer
					2001, //Layer
					0, //Opacity
					0, //Dest rect
					0, //Src rect
					0, //mask
					DISPMANX_NO_ROTATE); //transform


			opacity = 0;

			pis_compositor.state = 2;
			break;

		case 2:
			//At this point: "nextSlide" is sitting on the backImage and "currentSlide" is done
			pis_compositor.lastSlide = pis_compositor.currentSlide;
			pis_compositor.currentSlide = pis_compositor.nextSlide;

			//Figure out what the next slide should be
			if(pis_compositor.currentSlide != NULL && pis_compositor.currentSlide->next == NULL)
			{
				printf("Have a current slide but next is null, setting stack to head\n");
				pis_compositor.nextSlide = pis_compositor.slides;
			}else if(pis_compositor.currentSlide != NULL){
				printf("Have a current slide with a next\n");
				pis_compositor.nextSlide = pis_compositor.currentSlide->next;
			}else {
				printf("No current slide, next is NULL\n");
				pis_compositor.nextSlide = NULL;
			}

			//Now: currentSlide is on the back image, nextSlide needs to be dissolved up
			pis_compositor.state = 3;
			break;

		//Render the next slide to the dissolve layer
		case 3:

			//clean up memory from the last slide if need be
			if(pis_compositor.lastSlide != NULL && pis_compositor.lastSlide != pis_compositor.currentSlide)
			{
				printf("Clearing old slide\n");
				pis_removeSlideFromDisplay(pis_compositor.lastSlide);
				pis_freeSlideResource(pis_compositor.lastSlide);
			}else{
				printf("No slide to clear.\n");
			}

			//Get the next slide ready
			//TODO: Handle NULL currentSlide
			if(pis_compositor.nextSlide == NULL)
			{
				printf("Null next, fading out.\n");
				//No current slide, fade to black
				pis_compositor.slideDissolveTime = 1000;
				pis_compositor.slideHoldTime = 1000;
				pis_compositor.state = 4;
				break;
			}else{
				printf("Loading up the next slide.\n");
				opacity = 0; //For initial conditions
				pis_loadSlideResources(pis_compositor.nextSlide);
				pis_compositeSlide(dissolveDisp,pis_compositor.nextSlide);
				pis_compositor.slideDissolveTime = pis_compositor.nextSlide->dissolve;
				pis_compositor.slideHoldTime = pis_compositor.nextSlide->duration;
			}

			pis_compositor.state = 4;

			break;

		//Wait for the current slide to timeout and then start a dissolve when it's time
		case 4:
			if((ms-pis_compositor.slideStartTime) > pis_compositor.slideHoldTime){
				pis_compositor.state = 0;
				pis_compositor.dissolveStartTime = ms ;
			}
			break;
	}

	vc_dispmanx_element_change_attributes(update,
			backElement,
			2, //To change: Opacity
			0, //Layer
			255, //Opacity
			0, //Dest rect
			0, //Src rect
			0, //mask
			DISPMANX_NO_ROTATE); //transform

	vc_dispmanx_element_change_attributes(update,
			dissolveElement,
			2, //To change: Opacity
			0, //Layer
			opacity*255, //Opacity
			0, //Dest rect
			0, //Src rect
			0, //mask
			DISPMANX_NO_ROTATE); //transform

	vc_dispmanx_update_submit_sync( update );

	return pis_COMPOSITOR_ERROR_NONE;
}
