#include <time.h>
#include <stdio.h>
#include <dirent.h>

#include "compositor.h"
#include "bcm_host.h"
#include "vgfont/vgfont.h"

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

	graphics_display_resource(pis_compositor.backImg, 0, 0, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 1, VC_DISPMAN_ROT0, 1);
	graphics_display_resource(pis_compositor.dissolveImg, 0, 1, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 0, VC_DISPMAN_ROT0, 1);

	graphics_update_displayed_resource(pis_compositor.backImg, 0, 0, 0, 0);
	graphics_update_displayed_resource(pis_compositor.dissolveImg, 0, 0, 0, 0);

	pis_compositor.slides = NULL;
	pis_compositor.currentSlide = NULL;

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_cleanup()
{
	graphics_delete_resource(pis_compositor.dissolveImg);

	return pis_COMPOSITOR_ERROR_NONE;
}

pis_compositorErrors pis_doCompositor()
{

	int state = 1;
	double            ms; // Milliseconds
	time_t		s2;
	struct timespec spec;
	float opacity;

	clock_gettime(CLOCK_REALTIME, &spec);
	s2  = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6)+s2*1000; // Convert nanoseconds to milliseconds

	switch(state){
		case 0: opacity = ((float)(ms-pis_compositor.dissolveStartTime))/dissolveTime;
			if(opacity > 1)	{
				opacity = 1;
				state = 1;
				pis_compositor.dissolveStartTime = ms;
			}
			break;

		case 1:
			//graphics_userblt(GRAPHICS_RESOURCE_RGBA32,current->image,0,0,width,height,width*4,backImg,0,0);
			//TODO composite slide to buffer
			graphics_display_resource(pis_compositor.backImg, 0, 0, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 1, VC_DISPMAN_ROT0, 1);
			state = 2;
			break;

		case 2: opacity=0; state = 3; break;

		case 3:
			pis_compositor.slideDissolveTime = current->dissolveToNextTime;
			pis_compositor. holdTime = current->holdTime;

			//TODO: load next slide here

			if(current->next == NULL)
				current = head;
			else
				current = current->next;

			graphics_userblt(GRAPHICS_RESOURCE_RGBA32, current->image,0,0,width,height,width*4,dissolveImg,0,0);
			graphics_display_resource(pis_compositor.dissolveImg, 0, 1, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, 0, VC_DISPMAN_ROT0, 1);
			state = 4;
			break;

		case 4:
			if((ms-startTime) > holdTime){
				state = 0;
				startTime = ms;
			}
			break;
	}

	graphics_display_resource(pis_compositor.backImg, 0, 1, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, opacity, VC_DISPMAN_ROT0, 1);

	return pis_COMPOSITOR_ERROR_NONE;
}
