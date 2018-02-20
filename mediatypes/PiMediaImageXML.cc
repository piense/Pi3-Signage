#include <string>
#include <libxml/parser.h>
#include <algorithm>

#include "PiMediaImage.h"
#include "../compositor/tricks.h"
#include "../PiSlide.h"
#include "../PiSignageLogging.h"

using namespace std;

const char* pis_MediaImage::MediaType = "image";
const pis_MediaItemRegistrar pis_MediaImage::r(MediaType, &FromXML);

int pis_MediaImage::NewImage(const char *filename, float x, float y,
		float width, float height, pis_mediaSizing scaling, pis_MediaItem **item)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_MediaImage::AddToSlide\n");

	if (item == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::AddToSlide: Bad item ptr\n");
		return -1;
	}

	pis_MediaImage *newImage = new pis_MediaImage();
	if(newImage == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::AddToSlide: Error allocating new image\n");
		return -1;
	}

	newImage->Filename = filename;
	newImage->MaxHeight = height;
	newImage->MaxWidth = width;
	newImage->X = x;
	newImage->Y = y;
	newImage->Scaling = scaling;

	newImage->ImgCache.img = NULL;
	newImage->ImgCache.width = 0;
	newImage->ImgCache.height = 0;

	(*item) = newImage;

	return 0;
}

//Used to identify media type in XML
std::string pis_MediaImage::GetType()
{
	return string("image");
}

//Recreates the object from XML
int pis_MediaImage::FromXML(xmlNodePtr node, pis_MediaItem **outItem)
{
	pis_MediaImage *newImage = new pis_MediaImage();

	if(newImage == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaImage::FromXML: Error creating new text object\n");
		return -4;
	}

	if(node == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaImage::FromXML: Error, NULL node\n");
		return -1;
	}

	if(outItem == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaImage::FromXML: Error, Null return pointer\n");
		return -2;
	}

	string slideNodeName((char*)node->name);
	transform(slideNodeName.begin(), slideNodeName.end(), slideNodeName.begin(), ::tolower);

	if(slideNodeName != string("mediaitem"))
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaImage::FromXML: Error, not a mediaitem node\n");
		delete newImage;
		return -3;
	}

	//TODO: Figure out how to make this case insensitive
	xmlChar *returnStr = xmlGetProp(node, BAD_CAST "Type");
	string slideMediaType = string((char*)returnStr);
	xmlFree(returnStr);
	transform(slideMediaType.begin(), slideMediaType.end(), slideMediaType.begin(), ::tolower);

	if(slideMediaType != string("image"))
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaImage::FromXML: Error, not an image node\n");
		delete newImage;
		return -5;
	}

	xmlChar *tempStr;

	for(xmlNodePtr nodePtr = node->children;nodePtr;nodePtr=nodePtr->next)
	{
		if(nodePtr->type == XML_ELEMENT_NODE)
		{
			string nodeName((char*)nodePtr->name);
			transform(nodeName.begin(), nodeName.end(), nodeName.begin(), ::tolower);

			if(nodeName == string("filename")){
				tempStr = xmlNodeGetContent(nodePtr);
				newImage->Filename = string((char*)tempStr);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("scaling")){
				tempStr = xmlNodeGetContent(nodePtr);
				string scaling = string((char*)tempStr);
				transform(scaling.begin(), scaling.end(), scaling.begin(), ::tolower);
				if(scaling == string("stretch"))
					newImage->Scaling = pis_SIZE_STRETCH;
				if(scaling == string("crop"))
					newImage->Scaling = pis_SIZE_CROP;
				if(scaling == string("scale"))
					newImage->Scaling = pis_SIZE_SCALE;
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("x")){
				tempStr = xmlNodeGetContent(nodePtr);
				sscanf((char*)tempStr,"%f",&newImage->X);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("y")){
				tempStr = xmlNodeGetContent(nodePtr);
				sscanf((char*)tempStr,"%f",&newImage->Y);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("maxwidth")){
				tempStr = xmlNodeGetContent(nodePtr);
				sscanf((char*)tempStr,"%f",&newImage->MaxWidth);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("maxheight")){
				tempStr = xmlNodeGetContent(nodePtr);
				sscanf((char*)tempStr,"%f",&newImage->MaxHeight);
				xmlFree(tempStr);
				continue;
			}

		}
	}

	(*outItem) = newImage;
	return 0;
}

int pis_MediaImage::ToXML(string ** XML)
{
    xmlNodePtr n;
    xmlDocPtr doc;
    xmlBufferPtr xmlBuff = xmlBufferCreate();
    doc = xmlNewDoc(BAD_CAST "1.0");

    n = xmlNewNode(NULL, BAD_CAST "Slide");
    xmlDocSetRootElement(doc, n);

    ToXML(n);

    xmlNodeDump(xmlBuff, doc, n,0,1);
    string *ret = new string((char*)xmlBuff->content);
	(*XML) = ret;
    xmlFreeDoc(doc);
    xmlBufferFree(xmlBuff);

    return 0;
}

//Exports the media structure as XML
// XML: Out, pointer to pointer to point to new string object
int pis_MediaImage::ToXML(xmlNodePtr slideNode)
{
    xmlNodePtr n2;
    char tempString[Filename.length() > 30 ? Filename.length()+1 : 31];

    /*
     * Create the document.
     */

    slideNode = xmlNewTextChild(slideNode, NULL, BAD_CAST "MediaItem", NULL);
    xmlNewProp(slideNode, BAD_CAST "Type", BAD_CAST "Image");

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "X", NULL);
    sprintf(&tempString[0],"%f",X);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "Y", NULL);
    sprintf(&tempString[0],"%f",Y);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "MaxWidth", NULL);
    sprintf(&tempString[0],"%f",MaxWidth);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "MaxHeight", NULL);
    sprintf(&tempString[0],"%f",MaxHeight);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "Filename", NULL);
    sprintf(&tempString[0],"%s",Filename.c_str());
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "Scaling", NULL);
    switch(Scaling){
    	case pis_SIZE_CROP: sprintf(&tempString[0],"CROP");break;
    	case pis_SIZE_SCALE: sprintf(&tempString[0],"SCALE");break;
    	case pis_SIZE_STRETCH: sprintf(&tempString[0],"STRETCH");break;
    }
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

	return 0;
}
