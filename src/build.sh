#!/bin/bash
set -e

echo "Building NoanOS with Initrd Support..."

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

make -C "$SCRIPT_DIR" clean
make -C "$SCRIPT_DIR" -j8

INITRD_SRC="$SCRIPT_DIR/build/obj/initrd.bin"

if [ ! -f "$INITRD_SRC" ]; then
    echo "Error: initrd.bin not found at $INITRD_SRC!"
    exit 1
fi

INITRD_SIZE=$(stat -c%s "$INITRD_SRC")
echo "Initrd size: $INITRD_SIZE bytes"

if [ $INITRD_SIZE -lt 51200 ]; then
    echo "Warning: initrd.bin is less than 50KB ($INITRD_SIZE bytes)."
fi

echo "Disk image built successfully: $SCRIPT_DIR/build/img/disk.img"

if [[ "$1" == "run" ]]; then
    make -C "$SCRIPT_DIR" run
fi
