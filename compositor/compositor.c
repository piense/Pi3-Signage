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


uint8_t * loadImage(char *filename, uint16_t width, uint16_t height)
{
	time_t		s2;
	struct timespec spec;
	uint8_t *img2;

	clock_gettime(CLOCK_REALTIME, &spec);
	s2  = spec.tv_sec;
	double startMs = round(spec.tv_nsec / 1.0e6)+s2*1000; // Convert nanoseconds to milliseconds

	sImage *ret = decodeJpgImage(filename);

    clock_gettime(CLOCK_REALTIME, &spec);
    s2  = spec.tv_sec;
    double endMs = round(spec.tv_nsec / 1.0e6)+s2*1000; // Convert nanoseconds to milliseconds

    img2 = ret->imageBuf;
    free(ret);

    printf("Loaded image in %f seconds.\n",(endMs-startMs)/1000.0);
	return img2;
}

/*
void addImageToList(char *image, uint16_t width, uint16_t height){
	pis_Img * current;

	if(head == NULL){
		head = malloc(sizeof(img_t));
		//head->image = malloc(width*height*4);
		head->holdTime = 10000;
		head->dissolveToNextTime = 1000;
		head->next = NULL;
		head->image = loadImage(image, width, height);
	}else{
		current = head;
		while(current->next != NULL){
			current = current->next;
		}
		current->next = malloc(sizeof(img_t));
		current = current->next;
		//current->image = malloc(width*height*4);
		current->holdTime = 10000;
		current->dissolveToNextTime = 1000;
		current->next = NULL;
		current->image = loadImage(image, width, height);
	}
}*/

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
}

pis_compositorErrors pis_addNewSlide(uint32_t dissolveTime, uint32_t duration, char *name)
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

pis_compositorErrors pis_initializeCompositor()
{
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

	pis_compositor.slides = NULL;
	pis_compositor.currentSlide = NULL;
	pis_compositor.state = 1;

	pis_compositor.slideHoldTime = 0;
	pis_compositor.slideStartTime = 0;
	pis_compositor.slideDissolveTime = 0;

	pis_addNewSlide(1000,3000,"Slide 1");
	pis_addNewSlide(1000,3000,"Slide 2");

	pis_AddTextToSlide(pis_compositor.slides,"Hello World 1","",50.0/1920.0,.3,.3,0xFFFF0000);
	pis_AddTextToSlide(pis_compositor.slides,"Hello World 2","",50.0/1920.0,.3,.6,0xFFFFFFFF);
	pis_AddTextToSlide(pis_compositor.slides,"Renee 3","",50.0/1920.0,.3,.4,0xFFFFFFFF);

	pis_AddTextToSlide(pis_compositor.slides->next,"Hello World 3","",50.0/1920.0,.3,.2,0xFFFF0000);
	pis_AddTextToSlide(pis_compositor.slides->next,"Hello World 5","",50.0/1920.0,.3,.4,0xFFFFFFFF);
	pis_AddTextToSlide(pis_compositor.slides->next,"Renee","",50.0/1920.0,.3,.45,0xFFFFFFFF);

	pis_compositor.currentSlide = pis_compositor.slides;

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_compositorcleanup()
{
	graphics_delete_resource(pis_compositor.dissolveImg);

	return pis_COMPOSITOR_ERROR_NONE;
}

int thisNumber = 0;

pis_compositorErrors pis_compositeSlide(GRAPHICS_RESOURCE_HANDLE res, pis_slides_s *slide)
{
	pis_mediaElementList_s *current = slide->mediaElementsHead;

	graphics_resource_fill(res, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);

	while(current != NULL){
		switch(current->mediaElement.mediaType){
			case pis_MEDIA_IMAGE: break;
			case pis_MEDIA_VIDEO: break;
			case pis_MEDIA_TEXT:
				//TODO: Support other fonts
				//TODO: Range check font size and positions
				render_subtitle(res,((pis_mediaText *)current->mediaElement.data)->text,0,
						(uint32_t)(((pis_mediaText *)current->mediaElement.data)->fontHeight * pis_compositor.screenHeight),
						(uint32_t)(((pis_mediaText *)current->mediaElement.data)->y * pis_compositor.screenHeight),
						((pis_mediaText *)current->mediaElement.data)->color
						);

				break;
			case pis_MEDIA_AUDIO: break;
		}
		current = current->next;
	}

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_doCompositor()
{
	double            ms; // Milliseconds
	time_t		s2;
	struct timespec spec;
	float opacity;

	clock_gettime(CLOCK_REALTIME, &spec);
	s2  = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6)+s2*1000; // Convert nanoseconds to milliseconds

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
				pis_compositeSlide(pis_compositor.backImg,pis_compositor.currentSlide);
			}else{
				graphics_resource_fill(pis_compositor.backImg, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);
			}

			graphics_display_resource(pis_compositor.backImg, 0, 0, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 1, VC_DISPMAN_ROT0, 1);
			pis_compositor.state = 2;
			break;

		//Forget why this is here but I think it prevents flickering while layers are shuffled
		case 2: opacity=0; pis_compositor.state = 3; break;

		//Render the next slide to the dissolve layer
		case 3:
			//TODO: Handle NULL currentSlide
			if(pis_compositor.currentSlide == NULL)
			{
				//No current slide, fade to black
				graphics_resource_fill(pis_compositor.dissolveImg, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);
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
				pis_compositeSlide(pis_compositor.dissolveImg,pis_compositor.currentSlide);
			}else{
				graphics_resource_fill(pis_compositor.dissolveImg, 0,0,pis_compositor.screenWidth, pis_compositor.screenHeight,PIS_COLOR_ARGB_BLACK);
			}

			graphics_display_resource(pis_compositor.dissolveImg, 0, 1, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 0, VC_DISPMAN_ROT0, 1);

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

	graphics_display_resource(pis_compositor.dissolveImg, 0, 1, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, opacity, VC_DISPMAN_ROT0, 1);

	return pis_COMPOSITOR_ERROR_NONE;
}
