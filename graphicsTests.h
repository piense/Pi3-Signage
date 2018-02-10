#pragma once

#include <sys/resource.h>

#include "compositor/compositor.h"
#include "dispmanx.h"
#include "compositor/tricks.h"

void printCPUandGPUMemory();
void printCPUMemory();
void printGPUMemory();
void decoderTest();
void resizerTest();
void dispmanXResourceTest();
