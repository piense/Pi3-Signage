#pragma once

#include <sys/resource.h>

#include "compositor/PiSlideShow.h"
#include "compositor/tricks.h"

void printCPUandGPUMemory();
void printCPUMemory();
void printGPUMemory();
void decoderTest();
void resizerTest();
void dispmanXResourceTest();
