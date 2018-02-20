#pragma once

extern "C"
{
//External libs
#include "bcm_host.h"
}

#include <string>
#include <list>
#include <libxml/parser.h>
#include "../PiSignageLogging.h"

class pis_MediaItem;

enum pis_MediaState
{
	pis_MediaLoaded,
	pis_MediaUnloaded,
	pis_MediaLive,
	pis_MediaExpired
};


//Represents media on a slide such
//as images, text, audio, videos
class pis_MediaItem
{
public:
	pis_MediaItem() { }
	virtual ~pis_MediaItem() { }

	//Gives the media item access to the Slides off screen buffer region
	//Assumption is that it needs to render to the top layer of this
	//when DoComposite() is called
	//TODO fis handle type
	virtual int SetGraphicsHandles(DISPMANX_DISPLAY_HANDLE_T offscreenBufferHandle,
			uint32_t offscreenWidth, uint32_t offscreenHeight) = 0;

	//Called when a slide is loading resources
	virtual int Load() = 0;

	//Called when a slide is unloading resources
	virtual int Unload(uint32_t update) = 0;

	//Called when a slide begins it's transition
	virtual int Go() = 0;

	//Called in the SlideRenderer's render loop
	//Responsible for compositing on top layer of
	//off screen buffer (if it has graphics content)
	virtual void DoComposite(uint32_t update, uint32_t layer) = 0;

	//Used to identify media type in XML
	virtual std::string GetType() = 0;

	//Recreates the object from XML
	//Must be overriden in super classes
	static int FromXML(xmlNodePtr mediaItemNode, pis_MediaItem **outItem)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "Error: FromXML not implemented.\n");
		return -1;
	}

	//Exports the media structure as XML
	// XML: Out, pointer to pointer to point to new string object
	virtual int ToXML(std::string **XML) = 0;

	virtual int ToXML(xmlNodePtr slideNode) = 0;

	//Returns the state of the media item
	virtual pis_MediaState GetState() = 0;
};

struct pis_MediaItemType
{
	const char* name;
	int (*FromXML)(xmlNodePtr, pis_MediaItem **);
};

typedef int (*xmlFactoryFunc)(xmlNodePtr, pis_MediaItem **);

class pis_MediaItemManager
{
public:
	static pis_MediaItemManager& instance() {
		static pis_MediaItemManager inst; // ensures initialization in the first call
		return inst;
	}

	std::list<pis_MediaItemType> availableMediaTypes;

	xmlFactoryFunc getXMLFunc(const char* type){
		for(std::list<pis_MediaItemType>::iterator it = availableMediaTypes.begin();
				it!=availableMediaTypes.end();++it)
			if(strcmp(it->name,type) == 0)
				return it->FromXML;
		return NULL;
	}

	void reg(pis_MediaItemType type){
		availableMediaTypes.push_back(type);
	}
private:
	pis_MediaItemManager(){
	}

};

struct pis_MediaItemRegistrar
{
	pis_MediaItemRegistrar (const char* name, int (*FromXML)(xmlNodePtr node, pis_MediaItem **newItem)){
		pis_MediaItemType data;
		data.name = name;
		data.FromXML = FromXML;
		pis_MediaItemManager::instance().reg(data);
	}
};


