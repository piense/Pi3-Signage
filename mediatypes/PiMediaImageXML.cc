#include <string>
#include <libxml/parser.h>

#include "PiMediaImage.h"
#include "../compositor/tricks.h"
#include "../PiSlide.h"
#include "../PiSignageLogging.h"

using namespace std;

int pis_MediaImage::AddToSlide(const char *filename, float x, float y,
		float width, float height, pis_mediaSizing scaling, pis_Slide *slide)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_MediaImage::AddToSlide\n");

	pis_MediaImage *newImage = new pis_MediaImage();
	if(newImage == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::AddToSlide: Error allocating new image\n");
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

	slide->mediaElements.push_back((pis_MediaItem *) newImage);

	string *xml = NULL;
	newImage->ToXML(&xml);

	printf("%s\n",xml->c_str());

	delete xml;

	return 0;
}

//Used to identify media type in XML
std::string pis_MediaImage::GetType()
{
	return string("Image");
}

//Recreates the object from XML
int pis_MediaImage::FromXML(std::string XML, pis_MediaItem **mediaImage)
{
	printf("Created an image media item!\n");
	return -1;
}

//Exports the media structure as XML
// XML: Out, pointer to pointer to point to new string object
int pis_MediaImage::ToXML(std::string **XML)
{
    xmlNodePtr n;
    xmlNodePtr n2;
    xmlDocPtr doc;
    xmlBufferPtr xmlBuff = xmlBufferCreate();
    string *ret;
    char tempString[2000];

    /*
     * Create the document.
     */
    doc = xmlNewDoc(BAD_CAST "1.0");

    n = xmlNewNode(NULL, BAD_CAST "MediaItem");
    xmlNewProp(n, BAD_CAST "Type", BAD_CAST "Image");
    xmlDocSetRootElement(doc, n);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "X", NULL);
    sprintf(&tempString[0],"%f",X);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "Y", NULL);
    sprintf(&tempString[0],"%f",Y);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "MaxWidth", NULL);
    sprintf(&tempString[0],"%f",MaxWidth);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "MaxHeight", NULL);
    sprintf(&tempString[0],"%f",MaxHeight);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "Filename", NULL);
    sprintf(&tempString[0],"%s",Filename.c_str());
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "Scaling", NULL);
    switch(Scaling){
    	case pis_SIZE_CROP: sprintf(&tempString[0],"CROP");break;
    	case pis_SIZE_SCALE: sprintf(&tempString[0],"SCALE");break;
    	case pis_SIZE_STRETCH: sprintf(&tempString[0],"STRETCH");break;
    }
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    xmlNodeDump(xmlBuff, doc, n,0,1);
    ret = new string((char*)xmlBuff->content);

    /*
     * Free associated memory.
     */
    xmlBufferFree(xmlBuff);
    xmlFreeDoc(doc);

    (*XML) = ret;

	return 0;
}
