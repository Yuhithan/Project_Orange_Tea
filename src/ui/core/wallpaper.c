#include "ui/wallpaper.h"
#include "framebuffer.h"
#include <stdint.h>
#include <stddef.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int wallpaper_load(const char* filname);
void wallpaper_draw(void);
void wallpaper_unload(void);