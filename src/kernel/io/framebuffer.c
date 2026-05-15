#include "io/framebuffer.h"
#include "io/kprint.h"
#include "core/types.h"

static fb_info_t fb;

void fb_init(void) {
    /* Assume VGA mode 13h linear buffer at 0xA0000 (320x200x8). */
    fb.addr = (void*)0xA0000;
    fb.width = 320;
    fb.height = 200;
    fb.pitch = 320;
    fb.bpp = 8;
    fb_fill(0);
}

void fb_put_pixel(u32 x, u32 y, u32 color) {
    if (x >= fb.width || y >= fb.height) return;
    volatile u8* p = (volatile u8*)fb.addr + y * fb.pitch + x;
    *p = (u8)color;
}

void fb_fill(u32 color) {
    volatile u8* p = (volatile u8*)fb.addr;
    for (u32 y = 0; y < fb.height; y++) {
        for (u32 x = 0; x < fb.pitch; x++) {
            p[y * fb.pitch + x] = (u8)color;
        }
    }
}

fb_info_t* fb_get_info(void) {
    return &fb;
}
