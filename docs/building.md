# Building REXUS Kernel

This document provides instructions for building and running the REXUS kernel.

## Prerequisites

To build REXUS kernel, you'll need the following tools:

- GCC cross-compiler for i386 target
- NASM (Netwide Assembler)
- GNU Make
- QEMU (for testing)
- GRUB (for creating bootable images)

### Setting up a Cross-Compiler

For proper kernel development, it's recommended to set up a cross-compiler. Follow these steps to build your own cross-compiler or use an existing one that targets i386-elf.

## Building

1. Clone the repository:
   ```
   git clone https://github.com/rexinscfu/rexus.git
   cd rexus
   ```

2. Build the kernel:
   ```
   make
   ```

This will produce `rexus.bin` in the project root directory.

## Running in an Emulator

You can test the kernel using QEMU:

```
qemu-system-i386 -kernel rexus.bin
```

Or with debugging support:

```
qemu-system-i386 -kernel rexus.bin -s -S
```

And then connect with GDB:

```
gdb -ex "target remote localhost:1234" -ex "symbol-file rexus.bin"
```

## Creating a Bootable ISO

To create a bootable ISO with GRUB:

1. Copy the kernel binary to a staging directory:
   ```
   mkdir -p isodir/boot/grub
   cp rexus.bin isodir/boot/
   ```

2. Create a GRUB configuration file:
   ```
   echo 'menuentry "REXUS Kernel" {
       multiboot /boot/rexus.bin
   }' > isodir/boot/grub/grub.cfg
   ```

3. Generate the ISO:
   ```
   grub-mkrescue -o rexus.iso isodir
   ```

## Troubleshooting

If you encounter build errors:

1. Ensure all dependencies are installed
2. Check compiler version compatibility
3. Verify file paths in the Makefile

For runtime issues, check the kernel's debug output via the VGA display. 