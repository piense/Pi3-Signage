#pragma once

#include <stdint.h>

void printFontList();
void renderTextToScreen(uint32_t *screenBuf,
		uint32_t screenWidth, uint32_t screenHeight,
		uint32_t xPos, uint32_t yPos,
		uint32_t fontSize, const char* string,
		uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha
		);
