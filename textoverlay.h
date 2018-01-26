#pragma once

#include <stdint.h>

#include "bcm_host.h"
#include "vgfont.h"

int32_t render_subtitle(GRAPHICS_RESOURCE_HANDLE img, const char *text, const uint32_t skip, const uint32_t text_size, const uint32_t y_offset);
