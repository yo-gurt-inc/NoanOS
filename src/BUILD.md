# Building NoanOS

## Quick Start

For first-time setup on a fresh Linux system:

```bash
cd src
make cross    # Install dependencies and setup toolchain
make          # Build NoanOS
make run      # Run in QEMU
```

## Requirements

The `make cross` command will automatically install:
- **nasm** - Assembler for boot code
- **i686-linux-gnu-gcc** - Cross-compiler for 32-bit x86
- **mtools** - FAT32 filesystem utilities
- **python3** - Build scripts
- **qemu-system-x86** - Emulator for testing

Supported distributions:
- Debian/Ubuntu (apt-get)
- Fedora/RHEL (dnf)
- Arch Linux (pacman)

## Manual Setup

If `make cross` doesn't work on your system, install these packages manually:

### Ubuntu/Debian
```bash
sudo apt-get install nasm gcc-multilib g++-multilib gcc-i686-linux-gnu mtools python3 qemu-system-x86
```

### Fedora/RHEL
```bash
sudo dnf install nasm gcc gcc-c++ glibc-devel.i686 mtools python3 qemu-system-x86
```

### Arch Linux
```bash
sudo pacman -S nasm gcc lib32-gcc-libs mtools python qemu-system-x86
```

Then build musl libc:
```bash
cd ..
git clone https://github.com/bminor/musl.git musl-noan
cd musl-noan
./configure --prefix=$(pwd) --target=i686 CFLAGS="-m32 -march=i686" --disable-shared
make -j$(nproc)
make install
cd ../src
```

## Building

```bash
make          # Build disk images
make clean    # Clean build artifacts
make run      # Run in QEMU (installed OS)
```

## Output

Build artifacts:
- `build/img/disk.img` - Live boot disk
- `build/img/hdd.img` - Pre-installed system disk
