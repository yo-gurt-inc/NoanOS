#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "core/types.h"

typedef struct {
    void* addr;
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
} fb_info_t;

void fb_init(void);
void fb_put_pixel(u32 x, u32 y, u32 color);
void fb_fill(u32 color);
fb_info_t* fb_get_info(void);

#endif
