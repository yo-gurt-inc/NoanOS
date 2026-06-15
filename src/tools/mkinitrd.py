import sys
import struct
import os

# Format:
# [Magic: 'NARC'] (4)
# [Count: u32] (4)
# --- Entries ---
# [Name: 32 bytes]
# [Size: u32]
# [Offset: u32]
# --- Data ---
# [Binary Data...]

def pack_initrd(output_file, shell_bin, user_bins_dir):
    files = []
    # Add the shell first (it's special, used as entry point)
    files.append(('shell', shell_bin))
    
    # Add all user binaries
    for f in os.listdir(user_bins_dir):
        files.append((f, os.path.join(user_bins_dir, f)))

    # Add rootfs files with full paths as names (e.g. '/bin/hello')
    rootfs_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'rootfs')
    rootfs_dir = os.path.normpath(rootfs_dir)
    if os.path.isdir(rootfs_dir):
        for dirpath, dirnames, filenames in os.walk(rootfs_dir):
            for fname in filenames:
                if fname == 'README':
                    continue
                full = os.path.join(dirpath, fname)
                rel  = os.path.relpath(full, rootfs_dir)  # e.g. bin/hello
                fat_path = '/' + rel.replace(os.sep, '/')  # e.g. /bin/hello
                files.append((fat_path, full))

    header_magic = b'NARC'
    count = len(files)
    
    # Calculate offsets
    # Header (8) + (Entry(40) * count)
    current_offset = 8 + (40 * count)
    
    entries = []
    data_blocks = []
    
    for name, path in files:
        with open(path, 'rb') as f:
            data = f.read()
        
        # Ensure name fits in 32 bytes
        name_bytes = name.encode('ascii')[:31].ljust(32, b'\0')
        size = len(data)
        
        entries.append(struct.pack('32sII', name_bytes, size, current_offset))
        data_blocks.append(data)
        
        # Align to 4 bytes
        pad = (4 - (size % 4)) % 4
        data_blocks.append(b'\0' * pad)
        current_offset += size + pad

    with open(output_file, 'wb') as f:
        f.write(header_magic)
        f.write(struct.pack('<I', count))
        for entry in entries:
            f.write(entry)
        for block in data_blocks:
            f.write(block)
    
    print(f"Initrd packed: {output_file} with {count} files.")

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("Usage: python3 mkinitrd.py <output> <shell_bin> <user_bins_dir>")
        sys.exit(1)
    
    pack_initrd(sys.argv[1], sys.argv[2], sys.argv[3])
