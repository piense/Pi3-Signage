#include <string>
#include <libxml/parser.h>
#include <algorithm>

#include "PiMediaText.h"
#include "../PiSlide.h"

using namespace std;

const char* pis_MediaText::MediaType = "text";
const pis_MediaItemRegistrar pis_MediaText::r(MediaType, &FromXML);

int pis_MediaText::NewText(const char *text, float x, float y,
		float width, float height, float fontHeight,
		const char *font, uint32_t color, pis_MediaItem **item)
{
	pis_logMessage(PIS_LOGLEVEL_FUNCTION_HEADER, "pis_MediaText::AddText\n");

	if(item == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::AddToSlide: Error with item pointer\n");
		return -1;
	}

	pis_MediaText *newText = new pis_MediaText();
	if(newText == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR,"pis_MediaImage::AddToSlide: Error allocating new image\n");
		return -1;
	}

	newText->Color = color;
	newText->FontHeight = fontHeight;
	newText->FontName = string(font);
	newText->Text = string(text);
	newText->X = x;
	newText->Y = y;

	(*item) = newText;

	return 0;
}

//Used to identify media type in XML
std::string pis_MediaText::GetType()
{
	return string("text");
}

//Recreates the object from XML
int pis_MediaText::FromXML(xmlNodePtr node, pis_MediaItem **outItem)
{
	pis_MediaText *newText = new pis_MediaText();

	if(newText == NULL){
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaText::FromXML: Error creating new text object\n");
		return -4;
	}

	if(node == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaText::FromXML: Error, NULL node\n");
		return -1;
	}

	if(outItem == NULL)
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaText::FromXML: Error, Null return pointer\n");
		return -2;
	}

	string slideNodeName((char*)node->name);
	transform(slideNodeName.begin(), slideNodeName.end(), slideNodeName.begin(), ::tolower);

	if(slideNodeName != string("mediaitem"))
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaText::FromXML: Error, not a mediaitem node\n");
		delete newText;
		return -3;
	}

	//TODO: Figure out how to make this case insensitive
	xmlChar *returnStr = xmlGetProp(node, BAD_CAST "Type");
	string slideMediaType = string((char*)returnStr);
	xmlFree(returnStr);
	transform(slideMediaType.begin(), slideMediaType.end(), slideMediaType.begin(), ::tolower);

	if(slideMediaType != string("text"))
	{
		pis_logMessage(PIS_LOGLEVEL_ERROR, "pis_MediaText::FromXML: Error, not a text node\n");
		delete newText;
		return -5;
	}

	xmlChar *tempStr;

	for(xmlNodePtr nodePtr = node->children;nodePtr;nodePtr=nodePtr->next)
	{
		if(nodePtr->type == XML_ELEMENT_NODE)
		{
			string nodeName((char*)nodePtr->name);
			transform(nodeName.begin(), nodeName.end(), nodeName.begin(), ::tolower);

			if(nodeName == string("text")){
				tempStr = xmlNodeGetContent(nodePtr);
				newText->Text = string((char*)tempStr);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("fontname")){
				tempStr = xmlNodeGetContent(nodePtr);
				newText->FontName = string((char*)tempStr);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("x")){
				tempStr = xmlNodeGetContent(nodePtr);
				sscanf((char*)tempStr,"%f",&newText->X);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("y")){
				tempStr = xmlNodeGetContent(nodePtr);
				sscanf((char*)tempStr,"%f",&newText->Y);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("fontheight")){
				tempStr = xmlNodeGetContent(nodePtr);
				sscanf((char*)tempStr,"%f",&newText->FontHeight);
				xmlFree(tempStr);
				continue;
			}

			if(nodeName == string("color")){
				tempStr = xmlNodeGetContent(nodePtr);
				sscanf((char*)tempStr,"%x",&newText->Color);
				xmlFree(tempStr);
				continue;
			}
		}
	}

	(*outItem) = newText;
	return 0;
}

int pis_MediaText::ToXML(string ** XML)
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
int pis_MediaText::ToXML(xmlNodePtr slideNode)
{
    xmlNodePtr n2;
    char tempString[Text.length() > 30 ? Text.length()+1 : 31];

    slideNode = xmlNewTextChild(slideNode, NULL, BAD_CAST "MediaItem", NULL);
    xmlNewProp(slideNode, BAD_CAST "Type", BAD_CAST "Text");

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "X", NULL);
    sprintf(&tempString[0],"%f",X);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "Y", NULL);
    sprintf(&tempString[0],"%f",Y);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "FontName", NULL);
    xmlNodeSetContent(n2, BAD_CAST FontName.c_str());

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "Color", NULL);
    sprintf(&tempString[0],"%x",Color);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "FontHeight", NULL);
    sprintf(&tempString[0],"%f",FontHeight);
    xmlNodeSetContent(n2, BAD_CAST &tempString[0]);

    //TODO: Escape text for XML?
    n2 = xmlNewTextChild(slideNode, NULL, BAD_CAST "Text", NULL);
    xmlNodeSetContent(n2, BAD_CAST Text.c_str());

	return 0;
}

