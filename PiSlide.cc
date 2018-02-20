#include "PiSlide.h"

#include <string>
#include <list>
#include <libxml/parser.h>
#include <algorithm>

#include "PiSignageLogging.h"

extern pis_MediaItemManager *MediaItems;

using namespace std;

//TODO Figure out how to manage Z-Order on slide media elements

pis_Slide::pis_Slide(){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::pis_Slide()\n");

	HoldTime = 5000;
	DissolveTime = 1000;
	GUID = 0; //Might have this randomly generated for new objects
}

pis_Slide::~pis_Slide()
{

}

int pis_Slide::FromXML(std::string slideTxt){
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::FromXML\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::FromXML(xmlNodePtr node, pis_Slide **newSlide)
{
	if(node == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_Slide::FromXML: Error, NULL node\n");
		return -1;
	}

	if(newSlide == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_Slide::FromXML: Error, Null return pointer\n");
		return -2;
	}

	pis_Slide *retSlide = new pis_Slide();

	string slideNodeName((char*)node->name);
	transform(slideNodeName.begin(), slideNodeName.end(), slideNodeName.begin(), ::tolower);

	if(slideNodeName != string("slide"))
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_Slide::FromXML: Error, not a slide node\n");
		delete retSlide;
		return -3;
	}

	xmlChar* dissolveTimeStr = xmlGetProp(node, BAD_CAST "DissolveTime");
	xmlChar* holdTimeStr = xmlGetProp(node, BAD_CAST "HoldTime");
	sscanf((char*)dissolveTimeStr,"%d",&retSlide->DissolveTime);
	sscanf((char*)holdTimeStr,"%d",&retSlide->HoldTime);
	xmlFree(dissolveTimeStr);
	xmlFree(holdTimeStr);

	for(xmlNodePtr nodePtr = node->children;nodePtr;nodePtr=nodePtr->next)
	{
		if(nodePtr->type == XML_ELEMENT_NODE)
		{
			string nodeName((char*)nodePtr->name);
			transform(nodeName.begin(), nodeName.end(), nodeName.begin(), ::tolower);
			if(nodeName == string("mediaitem")){
				xmlChar* typeXmlStr = xmlGetProp(nodePtr,BAD_CAST "Type");
				string type ((char*)typeXmlStr);
				xmlFree(typeXmlStr);
				transform(type.begin(), type.end(), type.begin(), ::tolower);
				pis_MediaItem *newItem;
				xmlFactoryFunc mediaFactory = MediaItems->getXMLFunc(type.c_str());
				if(mediaFactory == NULL){
					pis_logMessage(PIS_LOGLEVEL_ERROR,"FromXML: Unable to find XML factory for %s\n",type.c_str());
				}else{
					mediaFactory(nodePtr,&newItem);
					if(newItem != NULL){
						retSlide->MediaElements.push_back(newItem);
					}else{
						pis_logMessage(PIS_LOGLEVEL_ERROR,"FromXML: Unable to create type %s\n",type.c_str());
					}
				}
			}
		}
	}

	(*newSlide) = retSlide;

	return 0;
}

int pis_Slide::FromXML(std::string slideTxt, std::list<pis_Slide> **slides)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::FromXML\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::ToXML(string ** XML)
{
    xmlNodePtr n;
    xmlDocPtr doc;
    xmlBufferPtr xmlBuff = xmlBufferCreate();
    doc = xmlNewDoc(BAD_CAST "1.0");

    n = xmlNewNode(NULL, BAD_CAST "Slides");
    xmlDocSetRootElement(doc, n);

    ToXML(n);

    xmlNodeDump(xmlBuff, doc, n,0,1);
    string *ret = new string((char*)xmlBuff->content);
	(*XML) = ret;
    xmlFreeDoc(doc);
    xmlBufferFree(xmlBuff);

    return 0;
}

int pis_Slide::ToXML(xmlNodePtr slidesNode)
{
    char tempString[30];

    slidesNode = xmlNewTextChild(slidesNode, NULL, BAD_CAST "Slide", NULL);
    sprintf(&tempString[0],"%d",DissolveTime);
    xmlNewProp(slidesNode, BAD_CAST "DissolveTime", BAD_CAST &tempString[0]);
    sprintf(&tempString[0],"%d",HoldTime);
    xmlNewProp(slidesNode, BAD_CAST "HoldTime", BAD_CAST &tempString[0]);

    for(list<pis_MediaItem*>::iterator it = MediaElements.begin();
    		it != MediaElements.end();++it)
    {
    	if((*it) != NULL)
    		(*it)->ToXML(slidesNode);
    }

    return 0;
}

int pis_Slide::ToXML(list<pis_Slide*> *slides, xmlDocPtr doc)
{
	xmlNodePtr node;

    node = xmlNewNode(NULL, BAD_CAST "Slides");
    xmlDocSetRootElement(doc, node);

	for(list<pis_Slide*>::iterator it = slides->begin();
			it != slides->end();++it)
	{
		(*it)->ToXML(node);
	}
	return 0;
}

int pis_Slide::ToXML(list<pis_Slide*> *slides, std::string **XML)
{
    xmlNodePtr n;
    xmlDocPtr doc;
    xmlBufferPtr xmlBuff = xmlBufferCreate();
    doc = xmlNewDoc(BAD_CAST "1.0");

    n = xmlNewNode(NULL, BAD_CAST "Slides");
    xmlDocSetRootElement(doc, n);

    ToXML(slides, doc);

    xmlNodeDump(xmlBuff, doc, n,0,1);
    string *ret = new string((char*)xmlBuff->content);
	(*XML) = ret;
    xmlFreeDoc(doc);
    xmlBufferFree(xmlBuff);

    return 0;
}


int pis_Slide::FromXMLFile (const char *file, list<pis_Slide*> **slides)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::FromXMLFile\n");

	if(slides == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_Slide::FromXMLFile: Error NULL slides\n");
		return -3;
	}

	xmlDocPtr doc;
	xmlNodePtr slidesNode;

	list<pis_Slide*> *newSlides = new list<pis_Slide*>();

    doc = xmlReadFile(file, NULL, 0);
    if (doc == NULL) {
        pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_Slide::FromXMLFile: Error 1 reading file %s\n", file);
        return -1;
    }

    slidesNode = xmlDocGetRootElement(doc);
    if(slidesNode == NULL){
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_Slide::FromXMLFile: Error 2 reading file %s\n", file);
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return -2;
    }

    string rootName((char*)slidesNode->name);
    transform(rootName.begin(), rootName.end(), rootName.begin(), ::tolower);

    if(rootName == string("slides")){
    	for(xmlNodePtr slide = slidesNode->children; slide; slide=slide->next){
    		if(slide->type == XML_ELEMENT_NODE){
    			string nodeName((char*)slide->name);
    			transform(nodeName.begin(), nodeName.end(), nodeName.begin(), ::tolower);
    			if(nodeName == string("slide")){
    				pis_Slide *newSlide;
    				pis_Slide::FromXML(slide, &newSlide);
    				if(newSlide != NULL)
    					newSlides->push_back(newSlide);
    			}
    		}
    	}

    }else{
    	pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_Slide::FromXMLFile: No slides in file %s\n", file);
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();

    (*slides) = newSlides;

    return 0;
}

int pis_Slide::ToXMLFile (const char *file)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::ToXMLFile\n");
	pis_logMessage(PIS_LOGLEVEL_ERROR,"Not implemented\n");
	return -1;
}

int pis_Slide::ToXMLFile (list<pis_Slide *> *slides, const char *file)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_Slide::ToXMLFile\n");

	remove(file);

    xmlNodePtr n;
    xmlDocPtr doc;
    doc = xmlNewDoc(BAD_CAST "1.0");

    n = xmlNewNode(NULL, BAD_CAST "Slides");
    xmlDocSetRootElement(doc, n);

    ToXML(slides, doc);

    xmlSaveFormatFile(file, doc,1);

    xmlFreeDoc(doc);

	return 0;
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
