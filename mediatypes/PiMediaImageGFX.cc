#include <string>

extern "C"
{
//External Libs
#include "bcm_host.h"
}

#include "PiMediaImage.h"
#include "../compositor/piImageResizer.h"
#include "../compositor/pijpegdecoder.h"
#include "../compositor/tricks.h"

using namespace std;

const char* pis_MediaImage::MediaType = "Image";
const pis_MediaItemRegistrar pis_MediaImage::r(MediaType, &FromXML);

pis_MediaImage::pis_MediaImage(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaImage::pis_MediaImage\n");

	X = 0;
	Y = 0;
	MaxWidth = 1;
	MaxHeight = 1;
	ScreenWidth = 0;
	ScreenHeight = 0;
	Scaling = pis_SIZE_SCALE;

	res = 0;
	element = 0;
	offscreenDisplay = 0;
	imgHandle = 0;

	alpha.flags = (DISPMANX_FLAGS_ALPHA_T)(DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX);
	alpha.opacity = 255; //alpha 0->255
	alpha.mask = 0;
}


//Loads an image file and resizes to the appropriate dimensions
//Uses the Pis OpenMAX components. Decodes the file, converts it to ARGB, then resizes it.
//Must use RGB for resizing or there are a bunch of restrictions on odd dimensions
sImage * loadImage(const char *filename, uint16_t maxWidth, uint16_t maxHeight, pis_mediaSizing scaling)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaImage::loadImage: %s\n",filename);

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

	pis_logMessage(PIS_LOGLEVEL_ALL,"pis_MediaImage: Returned from decoder: %dx%d Size: %d Stride: %d Slice Height: %d\n",
			ret1->imageWidth,ret1->imageHeight,
			ret1->imageSize, ret1->stride, ret1->sliceHeight
			);

	//Decoder buffer logic makes this safe, sliceHeight and stride are always rounded up to an even number
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
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaImage::loadImage: Error converting color space\n");
		goto error;
	}

	//Finally get the image into the size we need it
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
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaImage::loadImage: Error resizing image.\n");
		goto error;
	}

	free(ret2->imageBuf);
	free(ret2);

    free(ret1->imageBuf);

    free(ret1);

    endMs = linuxTimeInMs();

    pis_logMessage(PIS_LOGLEVEL_INFO, "pis_MediaImage: Resized image in %f seconds.\n",(endMs-startMs)/1000.0);
	return ret3;

	error:
	if(ret1 != NULL && ret1->imageBuf != NULL)
		delete [] ret1->imageBuf;
	if(ret1 != NULL)
		delete ret1;

	if(ret2 != NULL && ret2->imageBuf != NULL)
		delete [] ret2->imageBuf;
	if(ret2 != NULL)
		delete ret2;

	if(ret3 != NULL)
		delete ret3;

	return NULL;
}

//Gives the media item access to the Slides off screen buffer region
//Assumption is that it needs to render to the top layer of this
//when DoComposite() is called
int pis_MediaImage::SetGraphicsHandles(DISPMANX_DISPLAY_HANDLE_T offscreenBufferHandle, uint32_t offscreenWidth, uint32_t offscreenHeight)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaImage::SetGraphicsHandles\n");

	offscreenDisplay = offscreenBufferHandle;
	ScreenWidth = offscreenWidth;
	ScreenHeight = offscreenHeight;
	return 0;
}

//Called when a slide is loading resources
int pis_MediaImage::Load()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaImage::Load\n");

	//TODO: range check width & height
	float startTime = linuxTimeInMs();


	sImage *img = loadImage(Filename.c_str(),
			MaxWidth * ScreenWidth,
			MaxHeight * ScreenHeight,
			Scaling);

	if(img == NULL || img->imageBuf == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::Load Error loading image\n");
		return -1;
	}

	ImgCache.img = (uint32_t *) img->imageBuf;
	ImgCache.width =  img->imageWidth;
	ImgCache.height =  img->imageHeight;
	ImgCache.stride = img->stride;

	free(img);

	res = 0;
	res = vc_dispmanx_resource_create(VC_IMAGE_ARGB8888, //ARGB for pngs later on
			ImgCache.width,ImgCache.height,
			&imgHandle);

	if(res == 0){
		free(ImgCache.img);
		ImgCache.img = NULL;
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::LoaloadSlideResources:d Error creating image resource\n");
		return -1;
	}

	VC_RECT_T rect = {0,0,(int32_t)ImgCache.width,(int32_t)ImgCache.height };
	int ret = vc_dispmanx_resource_write_data(res,
			VC_IMAGE_ARGB8888, ImgCache.stride,ImgCache.img,
			&rect
			);
	if(ret != 0){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::Load Error writing image data to dispmanx resource %d\n", ret);
	}else{
		pis_logMessage(PIS_LOGLEVEL_ALL,"pis_MediaImage::Load Resource loaded.\n");
	}

	free(ImgCache.img);
	ImgCache.img = NULL;

	pis_logMessage(PIS_LOGLEVEL_INFO, "pis_MediaImage::Load %f millis\n",linuxTimeInMs()-startTime);

	return 0;
}

//Called when a slide is unloading resources
int pis_MediaImage::Unload(uint32_t update)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaImage::Unload\n");

	if(element != 0){
		vc_dispmanx_element_remove(update, element);
	}

	element = 0;

	if(ImgCache.img != NULL)
		delete [] ImgCache.img;

	ImgCache.img = NULL;
	ImgCache.width =  0;
	ImgCache.height =  0;
	ImgCache.stride = 0;

	if(element != 0)
		pis_logMessage(PIS_LOGLEVEL_WARN, "pis_MediaImage::Unload: Deleting a resource in use, this will probably end poorly.\n");
	if(res != 0){
		vc_dispmanx_resource_delete(res);
		res = 0;
	}

	return 0;
}

//Called when a slide begins it's transition
int pis_MediaImage::Go()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaImage::Go\n");
	return -1;
}

//Called in the SlideRenderer's render loop
//Responsible for compositing on top layer of
//off screen buffer (if it has graphics content)
void pis_MediaImage::DoComposite(uint32_t update, uint32_t layer)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaImage::DoComposite\n");
	float startTime = linuxTimeInMs();

	if(element == 0){
		pis_logMessage(PIS_LOGLEVEL_ALL, "PiSlideRenderer::compositeSlide: Adding image element\n");
		VC_RECT_T rectImg = {0,0,(int32_t)ImgCache.width<<16,(int32_t)ImgCache.height << 16};
		VC_RECT_T rectImg2 = {(int32_t)(X*ScreenWidth - ImgCache.width/2),
				(int32_t)(Y*ScreenHeight - ImgCache.height/2),
				(int32_t)ImgCache.width,(int32_t)ImgCache.height};
		element = vc_dispmanx_element_add(update,
			offscreenDisplay,
			layer++,
			&rectImg2,
			res,
			&rectImg,
			DISPMANX_PROTECTION_NONE,
			&alpha,
			NULL,
			DISPMANX_NO_ROTATE
			);
		if(element == 0)
			pis_logMessage(PIS_LOGLEVEL_ERROR, "PiSlideRenderer::compositeSlide: Error: Unable to composite image.\n");
	}else{
		//Update element properties(?) for animation stuff
	}

	pis_logMessage(PIS_LOGLEVEL_ALL, "pis_MediaImage::DoComposite %f millis\n",linuxTimeInMs()-startTime);
}

//Returns the state of the media item
pis_MediaState pis_MediaImage::GetState()
{
	//TODO
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER,"pis_MediaImage::GetState\n");
	return pis_MediaUnloaded;
}
