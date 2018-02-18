#pragma once

#include <string>
#include <compositor/piSlideTypes.h>
#include <list>

//TODO Figure out how to manage Z-Order on slide media elements

//This class manages the slides data structure, ie completely platform independent code
class pis_Slide{
public:

	pis_Slide();
	~pis_Slide();

	//Takes a slide formatted as xml and adds it's elements to this
	//slide and sets slide properties to match
	// slideTxt: In, slide as xml std::string
	// return: 0 for good, otherwise error
	int FromXML(std::string slideTxt);

	//Takes slides formatted as xml and returns a collection of slides
	// slideTxt: In, slides as XML in a std::string
	// slides: Out, pointers to slides.
	// return: 0 for good, otherwise error
	static int FromXML(std::string slideTxt, std::list<pis_Slide> **slides);

	//Serializes slide to to XML
	// slide: In, Slide structure to serialize
	// slideTxt: Out, pointer to change to new std::string object containing XML
	// return: 0 for good, otherwise error
	int ToXML(std::string **slideTxt);

	//Takes multiples slides and serializes them to XML
	// slides: In, Slide structures to serialize
	// slideTxt: Out, pointer to change to new std::string object containing XML
	// 			  One std::string containing multiple slide objects
	// return: 0 for good, otherwise error
	static int ToXML(std::list<pis_Slide> *slides, std::string **slideTxt);

	//Reads a file from the filesystem and deserializes it into slide objects
	// file: In, file to read
	// slides: Out, slides read from file
	// return: 0 for good, otherwise error
	static int FromXMLFile (const char *file, std::list<pis_Slide> **slides);

	//Serializes a slide to an XML file
	// file: In, file to write data to
	// return: 0 for good, otherwise error
	int ToXMLFile (const char *file);

	//Serializes slides to an XML file
	// slides: In, slides to serialize
	// file: In, file to write data to
	// return: 0 for good, otherwise error
	static int ToXMLFile (std::list<pis_Slide> *slides,const char *file);


	//Adds an image to the slide
	//Image will be on top all other elements, ie last in the render std::list
	// filename: In, image file to add. Doesn't access file at all.
	// x: In, x position on slide 0 - 1. Position is relative to center of image and top left of slide.
	// y: In, y position on slide 0 - 1. Position is relative to center of image and top left of slide.
	// width: In, 0 - 1, Width of image on slide relative to display width.
	// height: In, 0 - 1, Height of image on slide relative to display height.
	// scaling: In, how the image is scaled to fit the given on-screen dimensions
	// return: 0 for good, otherwise error
	int AddImage(const char *filename, float x, float y, float width, float height, pis_mediaSizing scaling, void * appData);

	//Adds text to the slide
	//Text will be on top all other elements, ie last in the render std::list
	// text: In, text to add
	// x: In, x position on slide 0 - 1. Position is relative to center of text and top left of slide.
	// y: In, y position on slide 0 - 1. Position is relative to center of text and top left of slide.
	// width: In, 0 - 1, Width of text bounds on slide relative to display width.
	// height: In, 0 - 1, Height of text bounds on slide relative to display height.
	// fontHeight: In, 0 - 1, Height of text relative to slide height
	// font: In, font style to use
	// color: In, color in ARGB
	// appData: In, host specific data structure
	// return: 0 for good, otherwise error
	int AddText(const char *text, float x, float y, float width, float height, float fontHeight, const char *font, uint32_t color, void * appData);

	//Adds a video the slide
	//Video will be on top all other elements, ie last in the render std::list
	// filename: In, video file to add. Doesn't access file at all.
	// x: In, x position on slide 0 - 1. Position is relative to center of video and top left of slide.
	// y: In, y position on slide 0 - 1. Position is relative to center of video and top left of slide.
	// width: In, 0 - 1, Width of video on slide relative to display width.
	// height: In, 0 - 1, Height of video on slide relative to display height.
	// scaling: In, how the video is scaled to fit the given on-screen dimensions
	int AddVideo(const char *filename, float x, float y, float width, float height, pis_mediaSizing scaling, void * appData);


	//Adds an audio file to the slide
	// filename: In, audio file to add. Doesn't access file at all.
	// volume: In, 0 to 1
	// return: 0 for good, otherwise error
	int AddAudio(const char *filename, float volume, void * appData);

	//Sets the transition timing of a slide
	// dissolveTime: In, Time in millis for the crossdissolve
	// holdTime: In, Time in millis for the slide to hold before expiring
	// return: 0 for good, otherwise error
	int SetTransition(uint32_t dissolveTime, uint32_t holdTime);

	//Sets the dissolve time in milliseconds
	void SetDissolveTime(uint32_t millis);

	//Sets the hold time in milliseconds
	void SetHoldTime(uint32_t millis);

	//returns dissolve duration in milliseconds
	uint32_t GetDissolveTime();

	//returns slide hold time in milliseconds
	uint32_t GetHoldTime();

	//GUID provides a unique key for referencing this slide object in data
	//files and across the network. Somehow randomly generated to make
	//uniqueness extremely highly probable

	//Returns the slides GUID
	uint64_t getGUID();

	//sets a new random GUID
	void newGUID();

	//Sets a new GUID
	void setGUID(uint64_t guid);

	//Using a std::list of structs for now, might rethink that later
	//public for now so the renderer can work with it
	std::list<pis_mediaElement> mediaElements;

private:
	uint32_t DissolveTime;
	uint32_t HoldTime;
	uint64_t GUID;
};



