#!/usr/bin/env python3
"""
make-hdd.py: Build a pre-installed NoanOS HDD image.
Replicates what the installer does:
  1. Copy 700 sectors from disk.img (boot+kernel+initrd)
  2. Keep the FAT32 BPB from disk.img sector 0 (already formatted by mformat)
  3. Install all NARC files from initrd.bin into /bin/ on the FAT32
  4. Install rootfs ELF files at their full paths
  5. Clear the LIVE marker (sector 255)
"""
import sys, os, struct

DISK_IMG  = "build/img/disk.img"
HDD_IMG   = "build/img/hdd.img"
INITRD    = "build/obj/initrd.bin"

SECTOR    = 512
# Must match fat32_format values
RESERVED  = 128
FAT_SIZE  = 160   # sectors per FAT
NUM_FATS  = 2
SPC       = 8     # sectors per cluster
ROOT_CLU  = 2
FAT_START = RESERVED
DATA_START= RESERVED + NUM_FATS * FAT_SIZE  # = 448

def cluster_to_lba(cluster):
    return DATA_START + (cluster - 2) * SPC

def read_sector(img, lba):
    img.seek(lba * SECTOR)
    return bytearray(img.read(SECTOR))

def write_sector(img, lba, data):
    img.seek(lba * SECTOR)
    img.write(bytes(data).ljust(SECTOR, b'\x00')[:SECTOR])

def fat_get(img, cluster):
    fat_lba = FAT_START + (cluster * 4) // SECTOR
    off     = (cluster * 4) % SECTOR
    sec = read_sector(img, fat_lba)
    return struct.unpack_from('<I', sec, off)[0] & 0x0FFFFFFF

def fat_set(img, cluster, value):
    fat_lba = FAT_START + (cluster * 4) // SECTOR
    off     = (cluster * 4) % SECTOR
    sec = read_sector(img, fat_lba)
    struct.pack_into('<I', sec, off, value)
    write_sector(img, fat_lba, sec)
    # Mirror to FAT2
    fat_set2_lba = FAT_START + FAT_SIZE + (cluster * 4) // SECTOR
    sec2 = read_sector(img, fat_set2_lba)
    struct.pack_into('<I', sec2, off, value)
    write_sector(img, fat_set2_lba, sec2)

def fat_find_free(img):
    for c in range(2, 2500):
        if fat_get(img, c) == 0:
            return c
    raise RuntimeError("Disk full")

def name_to_83(name):
    """Convert filename to FAT 8.3 uppercase padded format."""
    name = os.path.basename(name).upper()
    if '.' in name:
        base, ext = name.rsplit('.', 1)
    else:
        base, ext = name, ''
    return (base[:8].ljust(8) + ext[:3].ljust(3)).encode('ascii')

def dir_find_free_slot(img, dir_cluster):
    lba = cluster_to_lba(dir_cluster)
    for s in range(SPC):
        sec = read_sector(img, lba + s)
        for i in range(0, SECTOR, 32):
            if sec[i] == 0x00 or sec[i] == 0xE5:
                return (lba + s, i)
    raise RuntimeError("Directory full")

def dir_create_entry(img, dir_cluster, name83, attr, first_cluster, size):
    lba, off = dir_find_free_slot(img, dir_cluster)
    sec = read_sector(img, lba)
    sec[off:off+11]  = name83
    sec[off+11]      = attr
    sec[off+12:off+20] = b'\x00' * 8
    struct.pack_into('<H', sec, off+20, (first_cluster >> 16) & 0xFFFF)
    sec[off+22:off+26] = b'\x00' * 4  # time/date
    struct.pack_into('<H', sec, off+26, first_cluster & 0xFFFF)
    struct.pack_into('<I', sec, off+28, size)
    write_sector(img, lba, sec)

def dir_find_or_create_subdir(img, parent_cluster, dirname):
    """Return cluster of subdir, creating it if needed."""
    name83 = name_to_83(dirname)
    lba = cluster_to_lba(parent_cluster)
    for s in range(SPC):
        sec = read_sector(img, lba + s)
        for i in range(0, SECTOR, 32):
            if sec[i] == 0x00: break
            if sec[i] == 0xE5: continue
            if sec[i:i+11] == name83 and sec[i+11] & 0x10:
                clu = struct.unpack_from('<H', sec, i+20)[0] << 16
                clu |= struct.unpack_from('<H', sec, i+26)[0]
                return clu
    # Create it
    clu = fat_find_free(img)
    fat_set(img, clu, 0x0FFFFFFF)
    # Zero the cluster
    empty = bytearray(SECTOR)
    for s in range(SPC):
        write_sector(img, cluster_to_lba(clu) + s, empty)
    # Write . and .. entries
    sec = bytearray(SECTOR)
    def make_dir_entry(off, n83, cluster):
        sec[off:off+11] = n83
        sec[off+11] = 0x10
        struct.pack_into('<H', sec, off+20, (cluster>>16)&0xFFFF)
        struct.pack_into('<H', sec, off+26, cluster&0xFFFF)
    make_dir_entry(0,  b'.          ', clu)
    make_dir_entry(32, b'..         ', parent_cluster)
    write_sector(img, cluster_to_lba(clu), sec)
    dir_create_entry(img, parent_cluster, name83, 0x10, clu, 0)
    return clu

def install_file(img, dir_cluster, filename, data):
    name83 = name_to_83(filename)
    size = len(data)
    if size == 0:
        dir_create_entry(img, dir_cluster, name83, 0x20, 0, 0)
        return
    chunks = [data[i:i+SPC*SECTOR] for i in range(0, len(data), SPC*SECTOR)]
    first = prev = 0
    for chunk in chunks:
        clu = fat_find_free(img)
        fat_set(img, clu, 0x0FFFFFFF)
        if prev: fat_set(img, prev, clu)
        else: first = clu
        # Write sectors
        for s in range(SPC):
            off = s * SECTOR
            sec = bytearray(chunk[off:off+SECTOR]).ljust(SECTOR, b'\x00')
            write_sector(img, cluster_to_lba(clu) + s, sec)
        prev = clu
    dir_create_entry(img, dir_cluster, name83, 0x20, first, size)

def noan_wrap(data, entry_offset=16):
    magic = b'NOAN'
    header = struct.pack('<IIII', 0x4E4F414E, entry_offset, len(data), 0)
    return header + data

def main():
    # Read disk.img and hdd.img
    with open(DISK_IMG, 'rb') as f:
        disk = f.read()
    with open(HDD_IMG, 'r+b') as hdd:
        # Step 1: copy 700 sectors from disk.img, preserving HDD BPB (bytes 3-89)
        hdd.seek(0)
        hdd_bpb = bytearray(hdd.read(SECTOR))
        hdd.seek(0)
        for i in range(700):
            sec = bytearray(disk[i*SECTOR:(i+1)*SECTOR])
            if i == 0:
                # Preserve BPB from HDD (bytes 3-89)
                sec[3:90] = hdd_bpb[3:90]
            hdd.seek(i * SECTOR)
            hdd.write(bytes(sec))

        # Step 2: clear LIVE marker at sector 255
        hdd.seek(255 * SECTOR)
        hdd.write(b'\x00' * 4)

        # Zero out the root directory cluster (cluster 2) — it may contain
        # garbage from the disk.img sectors we copied over
        root_lba = cluster_to_lba(ROOT_CLU)
        empty = b'\x00' * SECTOR
        for s in range(SPC):
            hdd.seek((root_lba + s) * SECTOR)
            hdd.write(empty)

        # Step 3: parse NARC initrd
        with open(INITRD, 'rb') as f:
            narc = f.read()
        magic, count = struct.unpack_from('<4sI', narc, 0)
        assert magic == b'NARC', "Bad initrd magic"

        # Ensure /bin exists
        bin_cluster = dir_find_or_create_subdir(hdd, ROOT_CLU, 'bin')

        for i in range(count):
            eoff = 8 + i * 40
            name = narc[eoff:eoff+32].rstrip(b'\x00').decode()
            size, foff = struct.unpack_from('<II', narc, eoff+32)
            data = narc[foff:foff+size]

            if name.startswith('/'):
                # ELF file — install raw at full path
                parts = name.strip('/').split('/')
                dir_clu = ROOT_CLU
                for part in parts[:-1]:
                    dir_clu = dir_find_or_create_subdir(hdd, dir_clu, part)
                install_file(hdd, dir_clu, parts[-1], data)
                print(f"  ELF  {name}")
            else:
                # NOAN binary — wrap and install to /bin/
                wrapped = noan_wrap(data)
                install_file(hdd, bin_cluster, name, wrapped)
                print(f"  NOAN /bin/{name}")

    print("HDD image built successfully.")

if __name__ == '__main__':
    main()
