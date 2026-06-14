#include "io/framebuffer.h"
#include "io/kprint.h"
#include "core/types.h"

static fb_info_t fb;

static int valid_vbe_modeinfo(void) {
    /* ModeInfoBlock has no magic. Check ModeAttributes (offset 0x00, u16):
     * bit 0 = mode supported, bit 7 = linear framebuffer available. */
    u16 attrs = *(u16*)0x9000;
    return ((attrs & 0x81) == 0x81);
}

void fb_init(void) {
    /* Guarded read of ModeInfo written by bootloader. Only accept sane values. */
    if (valid_vbe_modeinfo()) {
        u32 phys = *(u32*)(0x9000 + 0x40);
        u16 w = *(u16*)(0x9000 + 0x12);
        u16 h = *(u16*)(0x9000 + 0x14);
        u16 pitch = *(u16*)(0x9000 + 0x0E);
        u8 bpp = *(u8*)(0x9000 + 0x19);

        /* Basic sanity checks to avoid clobbering memory if ModeInfo is bogus */
        if (phys != 0 && w > 0 && h > 0 && w <= 4096 && h <= 2160 && (bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32)) {
            u32 min_pitch = (u32)w * ((bpp + 7) / 8);
            if (pitch == 0) pitch = (u16)min_pitch;
            if ((u32)pitch >= min_pitch && (u32)pitch <= min_pitch * 4) {
                /* Accept this framebuffer mapping */
                fb.addr = (void*)(phys);
                fb.width = w;
                fb.height = h;
                fb.pitch = pitch;
                fb.bpp = bpp;
                fb_fill(0);
                return;
            }
        }
    }

    /* Fallback: VGA mode 13h linear buffer at 0xA0000 (320x200x8). */
    fb.addr = (void*)0xA0000;
    fb.width = 320;
    fb.height = 200;
    fb.pitch = 320;
    fb.bpp = 8;
    fb_fill(0);
}

void fb_put_pixel(u32 x, u32 y, u32 color) {
    if (x >= fb.width || y >= fb.height) return;
    if (fb.bpp == 8) {
        volatile u8* p = (volatile u8*)fb.addr + y * fb.pitch + x;
        *p = (u8)color;
    } else if (fb.bpp == 16) {
        volatile u16* p = (volatile u16*)fb.addr + y * (fb.pitch/2) + x;
        *p = (u16)color;
    } else if (fb.bpp == 24) {
        volatile u8* p = (volatile u8*)fb.addr + y * fb.pitch + x * 3;
        p[0] = (u8)(color & 0xFF);
        p[1] = (u8)((color >> 8) & 0xFF);
        p[2] = (u8)((color >> 16) & 0xFF);
    } else if (fb.bpp == 32) {
        volatile u32* p = (volatile u32*)fb.addr + y * (fb.pitch/4) + x;
        *p = (u32)color;
    }
}

void fb_fill(u32 color) {
    if (!fb.addr) return;
    if (fb.bpp == 8) {
        volatile u8* p = (volatile u8*)fb.addr;
        for (u32 y = 0; y < fb.height; y++) {
            for (u32 x = 0; x < fb.pitch; x++) {
                p[y * fb.pitch + x] = (u8)color;
            }
        }
    } else if (fb.bpp == 16) {
        volatile u16* p = (volatile u16*)fb.addr;
        u32 cols = fb.pitch/2;
        for (u32 y = 0; y < fb.height; y++) {
            for (u32 x = 0; x < cols; x++) {
                p[y * cols + x] = (u16)color;
            }
        }
    } else if (fb.bpp == 24) {
        volatile u8* p = (volatile u8*)fb.addr;
        for (u32 y = 0; y < fb.height; y++) {
            for (u32 x = 0; x < fb.width; x++) {
                u32 off = y * fb.pitch + x * 3;
                p[off+0] = (u8)(color & 0xFF);
                p[off+1] = (u8)((color >> 8) & 0xFF);
                p[off+2] = (u8)((color >> 16) & 0xFF);
            }
        }
    } else if (fb.bpp == 32) {
        volatile u32* p = (volatile u32*)fb.addr;
        u32 cols = fb.pitch/4;
        for (u32 y = 0; y < fb.height; y++) {
            for (u32 x = 0; x < cols; x++) {
                p[y * cols + x] = color;
            }
        }
    }
}

fb_info_t* fb_get_info(void) {
    return &fb;
}
