; boot.s - entry point

MB_HDR_MAGIC equ 0x1BADB002
MB_HDR_FLAGS_PAGE_ALIGN equ 1<<0 ; align on 4kb
MB_HDR_FLAGS_MEMORY_INFO equ 1<<1 ; request mem_info struct
MB_HDR_FLAGS_VIDEO_MODE equ 1<<2 ; respect graphics field
MB_HDR_FLAGS_AOUT_KLUDGE equ 1<<16 ; respect address field
MB_HDR_FLAGS equ (MB_HDR_FLAGS_PAGE_ALIGN | MB_HDR_FLAGS_MEMORY_INFO | MB_HDR_FLAGS_VIDEO_MODE)
MB_HDR_CHECKSUM equ -(MB_HDR_MAGIC + MB_HDR_FLAGS)
STACK_SIZE equ 32768 ; needs to be dividable by 16

section .text
bits 32
;extern __bss_start, __bss_end
extern kmain
global _start
_start:
	; TCC linker compatibility
	jmp _entry

align 4
mboot_hdr:
	dd MB_HDR_MAGIC
	dd MB_HDR_FLAGS
	dd MB_HDR_CHECKSUM

	; address field
	dd 0 ; dd mboot_hdr
	dd 0 ; dd _start
	dd 0 ; dd __bss_start
	dd 0 ; dd __bss_end
	dd 0 ; dd _entry

	; graphics field
	dd 1
	dd 0
	dd 0
	dd 0

global _entry:function _entry.end-_entry
_entry:
	cli
	cld
	mov esp, stack.top
	push DWORD 0
	popf
	push stack.top
	push eax
	push ebx
	call kmain
	jmp _halt
.end:

global _halt:function _halt.end-_halt
_halt:
	cli
.loop:
	hlt
	jmp .loop
.end:

global __stack_chk_guard:data (__stack_chk_guard.end - __stack_chk_guard)
__stack_chk_guard:
	dd 0x0a0dFF00
.end:

section .bss
global stack:data (stack.top - stack)
align 16
stack:
	resb STACK_SIZE
align 16
.top:
