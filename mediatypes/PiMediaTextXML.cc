#include <string>
#include <libxml/parser.h>

#include "PiMediaText.h"
#include "../PiSlide.h"


using namespace std;

int pis_MediaText::AddText(const char *text, float x, float y,
		float width, float height, float fontHeight,
		const char *font, uint32_t color, pis_Slide *slide)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_MediaText::AddText\n");

	pis_MediaText *newText = new pis_MediaText();
	if(newText == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::AddToSlide: Error allocating new image\n");
	}

	newText->Color = color;
	newText->FontHeight = fontHeight;
	newText->FontName = string(font);
	newText->Text = string(text);
	newText->X = x;
	newText->Y = y;

	string *xml = NULL;
	newText->ToXML(&xml);

	printf("%s\n",xml->c_str());

	delete xml;

	slide->mediaElements.push_back((pis_MediaItem*)newText);
	return 0;
}

//Used to identify media type in XML
std::string pis_MediaText::GetType()
{
	return string("Text");
}

//Recreates the object from XML
int pis_MediaText::FromXML(std::string XML, pis_MediaItem **MediaText)
{
	printf("Created a text media item!\n");
	return -1;
}

//Exports the media structure as XML
// XML: Out, pointer to pointer to point to new string object
int pis_MediaText::ToXML(std::string **XML)
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
    xmlNewProp(n, BAD_CAST "Type", BAD_CAST "Text");
    xmlDocSetRootElement(doc, n);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "X", NULL);
    sprintf(&tempString[0],"%f",X);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "Y", NULL);
    sprintf(&tempString[0],"%f",Y);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "FontName", NULL);
    xmlNodeSetContent(n2, BAD_CAST FontName.c_str());

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "Color", NULL);
    sprintf(&tempString[0],"%x",Color);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(n, NULL, BAD_CAST "FontHeight", NULL);
    sprintf(&tempString[0],"%f",FontHeight);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    //TODO: Escape text for XML?
    n2 = xmlNewTextChild(n, NULL, BAD_CAST "Text", NULL);
    xmlNodeSetContent(n2, BAD_CAST Text.c_str());

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

