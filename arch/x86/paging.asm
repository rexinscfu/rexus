; Paging functions for REXUS kernel

global enable_paging
global load_page_directory

; Enable paging
; This function turns on paging by setting the paging bit in CR0
; Parameters:
;   - page_dir: Physical address of page directory
enable_paging:
    ; Get directory address from stack
    mov eax, [esp+4]
    
    ; Load page directory
    mov cr3, eax
    
    ; Enable paging (set PG bit in CR0)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    
    ret

; Load page directory
; This function loads a page directory into CR3
; Parameters:
;   - page_dir: Physical address of page directory
load_page_directory:
    ; Get directory address from stack
    mov eax, [esp+4]
    
    ; Load page directory
    mov cr3, eax
    
    ret 