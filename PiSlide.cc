#include "PiSlide.h"

#include <string>
#include <list>

#include "PiSignageLogging.h"
#include "compositor/piSlideTypes.h"


//TODO Figure out how to manage Z-Order on slide media elements

pis_Slide::pis_Slide(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::pis_Slide()\n");

	HoldTime = 5000;
	DissolveTime = 1000;
	GUID = 0; //Might have this randomly generated for new objects
}

pis_Slide::~pis_Slide()
{
	//TODO: free memory from media elements std::list
}

int pis_Slide::FromXML(std::string slideTxt){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::FromXML\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::FromXML(std::string slideTxt, std::list<pis_Slide> **slides)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::FromXML\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::ToXML(std::string **slideTxt)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::ToXML\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::ToXML(std::list<pis_Slide> *slides, std::string **slideTxt)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::ToXML\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::FromXMLFile (const char *file, std::list<pis_Slide> **slides)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::FromXMLFile\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::ToXMLFile (const char *file)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::ToXMLFile\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::ToXMLFile (std::list<pis_Slide> *slides, const char *file)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::ToXMLFile\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::AddImage(const char *filename, float x, float y, float width, float height, pis_mediaSizing scaling, void * appData)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::AddImage\n");

	pis_mediaElement element;
	pis_mediaImage *imageData = new pis_mediaImage;

	if(imageData == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_Slide::AddImage: Error: unable to allocate resources.\n");
		if(imageData != NULL)
			free(imageData);
		return -1;
	}

	element.data = imageData;
	element.name = filename;
	element.z = 0;
	element.mediaType = pis_MEDIA_IMAGE;
	element.appData = appData;

	imageData->filename = filename;
	imageData->maxHeight = height;
	imageData->maxWidth = width;
	imageData->x = x;
	imageData->y = y;
	imageData->sizing = scaling;

	imageData->cache.img = NULL;
	imageData->cache.width = 0;
	imageData->cache.height = 0;

	mediaElements.push_back(element);

	return 0;
}

int pis_Slide::AddText(const char *text, float x, float y, float width, float height, float fontHeight, const char *font, uint32_t color, void *appData)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::AddText\n");

	//TODO: height & width not handled at the moment
	//TODO: try, catch std::string allocations

	pis_mediaElement element;
	pis_mediaText *textData = new pis_mediaText;

	if(textData == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_Slide::AddImage: Error: unable to allocate resources.\n");
		if(textData != NULL)
			free(textData);
		return -1;
	}

	element.data = textData;
	element.name = std::string("Text");
	element.z = 0;
	element.mediaType = pis_MEDIA_TEXT;
	element.appData = appData;

	textData->color = color;
	textData->fontHeight = fontHeight;
	textData->fontName = std::string(font);
	textData->text = std::string(text);
	textData->x = x;
	textData->y = y;

	mediaElements.push_back(element);
	return 0;
}

int pis_Slide::AddVideo(const char *filename, float x, float y, float width, float height, pis_mediaSizing scaling, void * appData)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::AddVideo\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_Slide::AddVideo Error Not implemented\n");
	return -1;
}

int pis_Slide::AddAudio(const char *filename, float volume, void * appData)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::AddAudio\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_Slide::AddAudio Error not implemented\n");
	return -1;
}

int pis_Slide::SetTransition(uint32_t dissolveTime, uint32_t holdTime)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::SetTransition\n");

	DissolveTime = dissolveTime;
	HoldTime = holdTime;

	pis_logMessage(PIS_LOGLEVEL_ALL, "pis_Slide::SF_SlideSetTransition: Dissolve: %d Hold: %d\n", dissolveTime, holdTime);

	return 0;
}

void pis_Slide::SetDissolveTime(uint32_t millis)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::setDissolveTime %d\n", millis);
	DissolveTime = millis;
}

void pis_Slide::SetHoldTime(uint32_t millis)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::setHoldTime %d\n", millis);
	HoldTime = millis;
}

uint32_t pis_Slide::GetDissolveTime()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::getDissolveTime %d\n", DissolveTime);
	return DissolveTime;
}

uint32_t pis_Slide::GetHoldTime()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::getHoldTime %d\n", HoldTime);
	return HoldTime;
}

uint64_t pis_Slide::getGUID()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::getGUID: %d\n", GUID);
	return GUID;
}

void pis_Slide::newGUID()
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::setGUID()\n", GUID);
	pis_logMessage(PIS_LOGLEVEL_ERROR, "setGUID() not implemented\n");
}

void pis_Slide::setGUID(uint64_t guid)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::setGUID: %d\n", guid);
	GUID = guid;
}
