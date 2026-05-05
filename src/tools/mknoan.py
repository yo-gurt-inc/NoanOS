import sys
import struct
import os

def create_noan(input_file, output_file, entry=0):
    with open(input_file, 'rb') as f:
        data = f.read()

    # NOAN Header: Magic (4), Entry (4), CodeSize (4), DataSize (4)
    # Total 16 bytes.
    # We'll treat the whole binary as code for simplicity in this example.
    magic = 0x4E4F414E
    code_size = len(data)
    data_size = 0
    
    # The entry point in the NOAN header is an offset from the start of the binary.
    # Since our header is 16 bytes, and the code starts right after, 
    # the absolute entry offset in the file is 16 + entry.
    header = struct.pack('<IIII', magic, 16 + entry, code_size, data_size)
    
    with open(output_file, 'wb') as f:
        f.write(header)
        f.write(data)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python3 mknoan.py <input> <output> [entry_offset]")
        sys.exit(1)
    
    entry = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    create_noan(sys.argv[1], sys.argv[2], entry)
