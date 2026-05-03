#include "include/rtc.h"
#include "include/io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static int get_update_in_progress_flag() {
    outb(CMOS_ADDR, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static u8 get_rtc_register(int reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

void rtc_get_time(u8* h, u8* m, u8* s) {
    while (get_update_in_progress_flag());
    *s = get_rtc_register(0x00);
    *m = get_rtc_register(0x02);
    *h = get_rtc_register(0x04);

    u8 registerB = get_rtc_register(0x0B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        *s = (*s & 0x0F) + ((*s / 16) * 10);
        *m = (*m & 0x0F) + ((*m / 16) * 10);
        *h = ((*h & 0x0F) + (((*h & 0x70) / 16) * 10)) | (*h & 0x80);
    }

    // Convert 12 hour clock to 24 hour clock if necessary
    if (!(registerB & 0x02) && (*h & 0x80)) {
        *h = ((*h & 0x7F) + 12) % 24;
    }
}

void rtc_get_date(u8* d, u8* mo, u16* y) {
    while (get_update_in_progress_flag());
    *d = get_rtc_register(0x07);
    *mo = get_rtc_register(0x08);
    *y = get_rtc_register(0x09);

    u8 registerB = get_rtc_register(0x0B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        *d = (*d & 0x0F) + ((*d / 16) * 10);
        *mo = (*mo & 0x0F) + ((*mo / 16) * 10);
        *y = (*y & 0x0F) + ((*y / 16) * 10);
    }

    // Assume 2000s
    *y += 2000;
}
