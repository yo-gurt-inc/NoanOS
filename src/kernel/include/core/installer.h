#ifndef INSTALLER_H
#define INSTALLER_H

#include "core/types.h"

/* Boot-mode marker sector on the boot drive: sector 700. On the live disk it
 * is past the initrd (LBA 121-632); on an installed disk it lies inside the
 * FAT32 reserved region, and the installer's 700-sector copy (0-699) never
 * reaches it. Values: "LIVE" (installer menu), "AUTO" (auto-install, written
 * by make test into disk.img), "TEST" (run the /bin test suite, written by
 * the auto-installer onto the target), zeros (normal installed boot). */
#define INSTALLER_MARKER_SECTOR 700

int installer_start(u32 boot_drive);
int installer_auto(u32 boot_drive);

#endif
