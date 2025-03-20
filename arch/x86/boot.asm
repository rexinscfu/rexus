; REXUS Kernel x86 Boot Code
; Define multiboot header for GRUB

MBOOT_PAGE_ALIGN    equ 1<<0
MBOOT_MEM_INFO      equ 1<<1
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

KERNEL_VIRTUAL_BASE equ 0xC0000000                  ; 3GB
KERNEL_PAGE_NUMBER  equ (KERNEL_VIRTUAL_BASE >> 22) ; Page dir index of kernel's 4MB PTE

extern _kernel_phys_start
extern _kernel_phys_end
extern kmain

global _start
global gdt_flush
global idt_load
global tss_flush

section .multiboot
align 4
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM
    dd 0
    dd 0
    dd 0
    dd 0
    dd _start

section .data
align 4096
boot_page_directory:
    ; Map the first 4MB of memory
    dd 0x00000083
    times (KERNEL_PAGE_NUMBER - 1) dd 0
    ; Map 3GB+ to 0+
    dd 0x00000083
    times (1024 - KERNEL_PAGE_NUMBER - 1) dd 0

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

section .boot
align 4
_start:
    ; Load the physical address of our page directory
    mov ecx, (boot_page_directory - KERNEL_VIRTUAL_BASE)
    mov cr3, ecx

    ; Enable 4MB pages
    mov ecx, cr4
    or ecx, 0x00000010
    mov cr4, ecx

    ; Enable paging
    mov ecx, cr0
    or ecx, 0x80000000
    mov cr0, ecx

    ; Jump to higher half with an absolute jump
    lea ecx, [higher_half]
    jmp ecx

higher_half:
    ; Unmap the identity mapping as it's now unnecessary
    mov dword [boot_page_directory], 0
    invlpg [0]

    ; Set up the stack
    mov esp, stack_top

    ; Reset EFLAGS
    push 0
    popf

    ; Push the multiboot info structure address and magic value
    push ebx
    push eax

    ; Call the C kernel entry point
    call kmain

    ; Kernel should never return, but if it does:
halt:
    cli
    hlt
    jmp halt

; GDT functions
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10      ; 0x10 is the data segment offset
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush   ; 0x08 is the code segment offset
.flush:
    ret

; IDT function
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret

; TSS function
tss_flush:
    mov ax, 0x2B      ; TSS segment is at offset 0x28 from GDT base, plus 3 (RPL 3)
    ltr ax
    ret 