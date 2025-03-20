OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

KERNEL_VIRT_BASE = 0xC0000000;
KERNEL_PHYS_BASE = 0x00100000;

SECTIONS
{
    . = KERNEL_PHYS_BASE;
    _kernel_phys_start = .;

    .boot : {
        *(.multiboot)
        *(.boot)
    }

    . += KERNEL_VIRT_BASE;
    _kernel_virt_start = .;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRT_BASE) {
        _code = .;
        *(.text)
        *(.text.*)
        _code_end = .;
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRT_BASE) {
        _rodata = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRT_BASE) {
        _data = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRT_BASE) {
        _bss = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }

    .init_array ALIGN(4K) : AT(ADDR(.init_array) - KERNEL_VIRT_BASE) {
        _init_array_start = .;
        *(.init_array)
        *(.init_array.*)
        _init_array_end = .;
    }

    _kernel_end = .;
    _kernel_size = _kernel_end - _kernel_virt_start;
    _kernel_phys_end = _kernel_phys_start + _kernel_size;
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
} 