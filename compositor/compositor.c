//System headers
#include <time.h>
#include <stdio.h>
//#include <dirent.h>
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
#include "piDisplayRender.h"

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

    printf("Resized image in %f seconds.\n\n",(endMs-startMs)/1000.0);
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
	((pis_mediaText *)newMedia->mediaElement.data)->fontHeight = height;
	((pis_mediaText *)newMedia->mediaElement.data)->x = x;
	((pis_mediaText *)newMedia->mediaElement.data)->y = y;
	((pis_mediaText *)newMedia->mediaElement.data)->fontName = fontName;
	((pis_mediaText *)newMedia->mediaElement.data)->text = text;
	((pis_mediaText *)newMedia->mediaElement.data)->color = color;
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

	//TODO range check x,y,width,height

	((pis_mediaImage *)newMedia->mediaElement.data)->filename = file;
	((pis_mediaImage *)newMedia->mediaElement.data)->maxHeight = height;
	((pis_mediaImage *)newMedia->mediaElement.data)->maxWidth = width;
	((pis_mediaImage *)newMedia->mediaElement.data)->x = x;
	((pis_mediaImage *)newMedia->mediaElement.data)->y = y;
	((pis_mediaImage *)newMedia->mediaElement.data)->sizing = sizing;

	sImage *img = loadImage(file, width * pis_compositor.screenWidth, height * pis_compositor.screenHeight);

	((pis_mediaImage *)newMedia->mediaElement.data)->cache.img = (uint32_t *) img->imageBuf;
	((pis_mediaImage *)newMedia->mediaElement.data)->cache.width =  img->imageWidth;
	((pis_mediaImage *)newMedia->mediaElement.data)->cache.height =  img->imageHeight;
	free(img);

	newMedia->next = slide->mediaElementsHead;
	newMedia->mediaElement.name = "Image";
	slide->mediaElementsHead = newMedia;
	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_initializeCompositor()
{

	pis_compositor.screenWidth = 1920;
	pis_compositor.screenHeight = 1080;

	bcm_host_init();

	/*

	vc_gencmd_init();

	bcm_host_init();

	int s;

	s = gx_graphics_init("./");
	if(s != 0){
		printf("Error at gx_graphics_init: %d\nCheck that fonts exist.",s);
	}
	assert(s == 0);

	s = graphics_get_display_size(0, &pis_compositor.screenWidth , &pis_compositor.screenHeight);
	if(s != 0){
		printf("Error at graphics_get_display_size: %d\n",s);
	}
	assert(s == 0);

	printf("width: %d height: %d\n",pis_compositor.screenWidth,pis_compositor.screenHeight);

	s = gx_create_window(0, pis_compositor.screenWidth, pis_compositor.screenHeight, GRAPHICS_RESOURCE_RGBA32, &pis_compositor.backImg);
	if(s != 0){
		printf("gx_create_window 1: %d\n",s);
	}
	assert(s == 0);

	s = gx_create_window(0, pis_compositor.screenWidth, pis_compositor.screenHeight, GRAPHICS_RESOURCE_RGBA32, &pis_compositor.dissolveImg);
	if(s != 0){
		printf("gx_create_window 2: %d\n",s);
	}
	assert(s == 0);

	//TODO: Paint black on back image

	graphics_resource_fill(pis_compositor.backImg, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);
	graphics_resource_fill(pis_compositor.dissolveImg, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);

	graphics_display_resource(pis_compositor.backImg, 0, 0, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 1, VC_DISPMAN_ROT0, 1);
	graphics_display_resource(pis_compositor.dissolveImg, 0, 1, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 0, VC_DISPMAN_ROT0, 1);

	graphics_update_displayed_resource(pis_compositor.backImg, 0, 0, 0, 0);
	graphics_update_displayed_resource(pis_compositor.dissolveImg, 0, 0, 0, 0);
	*/

	//pis_compositor.slides = NULL;
	//pis_compositor.currentSlide = NULL;
	pis_compositor.state = 1;

	pis_compositor.slideHoldTime = 0;
	pis_compositor.slideStartTime = 0;
	pis_compositor.slideDissolveTime = 0;

	//Demo slides

	pis_slides_s *newSlide;

	pis_addNewSlide(&newSlide, 1000,3000,"Slide 1");

	pis_compositor.slides = newSlide;

	pis_AddTextToSlide(newSlide,"Theodore David McVay","",200.0/1920.0,.3,.95,0xFFFF0000);

	pis_AddImageToSlide(newSlide,"/mnt/data/images/small.jpg",.5,.5,
			1,1,pis_SIZE_SCALE);

	pis_addNewSlide(&newSlide, 1000,3000,"Slide 3");

	pis_AddTextToSlide(newSlide,"Ollie!","",200.0/1920.0,.1,.95,0xFFFF0000);

	pis_AddImageToSlide(newSlide,"/mnt/data/images/IMG_6338.jpg",.5,.5,
					1,1,pis_SIZE_SCALE);

	pis_compositor.currentSlide = pis_compositor.slides;

	//-----

	backWindow = pis_NewRenderWindow(1);
	dissolveWindow = pis_NewRenderWindow(2);

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_compositorcleanup()
{
	//graphics_delete_resource(pis_compositor.dissolveImg);

	return pis_COMPOSITOR_ERROR_NONE;
}

int thisNumber = 0;

pis_compositorErrors pis_compositeSlide(pis_window *window, pis_slides_s *slide)
{
	pis_mediaElementList_s *current = slide->mediaElementsHead;

	//graphics_resource_fill(res, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);

	pis_fillBuf(window,0xFF000000);

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE:
				//TODO: range check width & height

				pis_bufCopy(window, ((pis_mediaImage *)current->mediaElement.data)->cache.img,
									0,0,
								((pis_mediaImage *)current->mediaElement.data)->cache.width
								,((pis_mediaImage *)current->mediaElement.data)->cache.height,
								(uint32_t)(((pis_mediaImage *)current->mediaElement.data)->x*pis_compositor.screenWidth
										-((float)((pis_mediaImage *)current->mediaElement.data)->cache.width)/2.0),
								(uint32_t)(((pis_mediaImage *)current->mediaElement.data)->y*pis_compositor.screenHeight
										-((float)((pis_mediaImage *)current->mediaElement.data)->cache.height)/2.0));


				/*graphics_userblt(GRAPHICS_RESOURCE_RGBA32,
						((pis_mediaImage *)current->mediaElement.data)->cache.img,
						0,0,
						((pis_mediaImage *)current->mediaElement.data)->cache.width
						,((pis_mediaImage *)current->mediaElement.data)->cache.height,
						((pis_mediaImage *)current->mediaElement.data)->cache.width*4,
						res,
						(uint32_t)(((pis_mediaImage *)current->mediaElement.data)->x*pis_compositor.screenWidth
								-((float)((pis_mediaImage *)current->mediaElement.data)->cache.width)/2.0)
						,
						(uint32_t)(((pis_mediaImage *)current->mediaElement.data)->y*pis_compositor.screenHeight
														-((float)((pis_mediaImage *)current->mediaElement.data)->cache.height)/2.0)
						);*/
				break;

			//TODO:
			case pis_MEDIA_VIDEO:
				break;

			case pis_MEDIA_TEXT:
				//TODO: Support other fonts
				//TODO: Range check font size and positions
				/*render_subtitle(res,((pis_mediaText *)current->mediaElement.data)->text,0,
						(uint32_t)(((pis_mediaText *)current->mediaElement.data)->fontHeight * pis_compositor.screenHeight),
						(uint32_t)(((pis_mediaText *)current->mediaElement.data)->y * pis_compositor.screenHeight),
						((pis_mediaText *)current->mediaElement.data)->color
						);*/

				break;

			//TODO:
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_doCompositor()
{
	float opacity;

	double ms = linuxTimeInMs();

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
			//graphics_userblt(GRAPHICS_RESOURCE_RGBA32,current->image,0,0,width,height,width*4,backImg,0,0);
			//TODO: Move slide from dissolve layer to background layer

			pis_compositor.slideStartTime = ms;

			if(pis_compositor.currentSlide != NULL){
				pis_compositeSlide(backWindow,pis_compositor.currentSlide);
			}else{
				pis_fillBuf(backWindow,0xFF000000);
				//graphics_resource_fill(pis_compositor.backImg, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);
			}

			pis_updateScreen(backWindow);
			//graphics_display_resource(pis_compositor.backImg, 0, 0, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 1, VC_DISPMAN_ROT0, 1);
			pis_compositor.state = 2;
			break;

		case 2: opacity = 0; pis_compositor.state = 3; break;

		//Render the next slide to the dissolve layer
		case 3:
			//TODO: Handle NULL currentSlide
			if(pis_compositor.currentSlide == NULL)
			{
				//No current slide, fade to black
				//graphics_resource_fill(pis_compositor.dissolveImg, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);
				pis_fillBuf(dissolveWindow,0xFF000000);
				pis_compositor.slideDissolveTime = 1000;
				pis_compositor.slideHoldTime = 1000;
				pis_compositor.state = 4;
				break;
			}else{
				pis_compositor.slideDissolveTime = pis_compositor.currentSlide->dissolve;
				pis_compositor.slideHoldTime = pis_compositor.currentSlide->duration;
			}

			if(pis_compositor.currentSlide != NULL && pis_compositor.currentSlide->next != NULL)
			{
				pis_compositor.currentSlide = pis_compositor.currentSlide->next;
			}else{
				pis_compositor.currentSlide = pis_compositor.slides;
			}

			if(pis_compositor.currentSlide != NULL){
				pis_compositeSlide(dissolveWindow,pis_compositor.currentSlide);
			}else{
				pis_fillBuf(dissolveWindow,0xFF000000);
				//graphics_resource_fill(pis_compositor.dissolveImg, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);
			}

			pis_updateScreen(dissolveWindow);
			//graphics_display_resource(pis_compositor.dissolveImg, 0, 1, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 0, VC_DISPMAN_ROT0, 1);

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

	pis_setWindowOpacity(dissolveWindow, opacity);
	//graphics_display_resource(pis_compositor.dissolveImg, 0, 1, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, opacity, VC_DISPMAN_ROT0, 1);

	return pis_COMPOSITOR_ERROR_NONE;
}


/*
void loadImages(uint16_t width, uint16_t height)
{
	DIR           *d;
	struct dirent *dir;
	d = opendir("/mnt/data/images/");

	if(d == NULL){
		printf("Error opening images directory.\n");
	}

	char fullname[1000];
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if(strstr(dir->d_name, ".jpg") != NULL || strstr(dir->d_name, ".bmp") != NULL || strstr(dir->d_name, ".jpeg") != NULL ) {
				printf("\nAdding: %s\n",dir->d_name);
				sprintf(&fullname[0],"%s%s","/mnt/data/images/",dir->d_name);
				//addImageToList(&fullname[0],width,height);
			}
		}
	closedir(d);
	}
}*/
