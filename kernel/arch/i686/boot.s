; boot.s - entry point

MB_HDR_MAGIC equ 0x1BADB002
MB_HDR_FLAGS_PAGE_ALIGN equ 1<<0 ; align on 4kb
MB_HDR_FLAGS_MEMORY_INFO equ 1<<1 ; request mem_info struct
MB_HDR_FLAGS_VIDEO_MODE equ 1<<2
MB_HDR_FLAGS equ (MB_HDR_FLAGS_PAGE_ALIGN | MB_HDR_FLAGS_MEMORY_INFO | MB_HDR_FLAGS_VIDEO_MODE)
MB_HDR_CHECKSUM equ -(MB_HDR_MAGIC + MB_HDR_FLAGS)
STACK_SIZE equ 32768 ; needs to be dividable by 16

bits 32
extern code, bss, end, kmain

section .mboot
	jmp _entry
align 4
mboot_hdr:
	dd MB_HDR_MAGIC
	dd MB_HDR_FLAGS
	dd MB_HDR_CHECKSUM
	dd mboot_hdr
	dd code
	dd bss
	dd end
	dd _entry
	dd 0
	dd 0
	dd 0
	dd 0

section .text
global _entry:function _entry.end-_entry
_entry:
	cli
	cld
	mov esp, stack.top
	push DWORD 0
	popf
	push esp
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

global __stack_chk_fail:function (__stack_chk_fail.end - __stack_chk_fail)
__stack_chk_fail:
	jmp _halt
.end:

section .rodata
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
