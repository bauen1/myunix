extern kernel_directory
extern handle_isr

; TODO: only reload cr3 when necessary
section isrs
global isrs_start
isrs_start:

global isrs_kernel_directory_ptr:data (isrs_kernel_directory_ptr.end - isrs_kernel_directory_ptr)
isrs_kernel_directory_ptr:
	dd 0
.end:

isr_common_stub:
	pusha

	push ds
	push es
	push fs
	push gs

	mov eax, cr3 ; save the page directory
	push eax
	mov eax, DWORD [ isrs_kernel_directory_ptr ]
	mov cr3, eax

	mov ax, 0x10        ; kernel data segment gdt index
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	cld

	push esp            ; our argument is a pointer to the stacks top
	call handle_isr
	add esp, 4

global return_to_regs
return_to_regs:
	pop eax ; restore the page directory
	mov cr3, eax

	pop gs
	pop fs
	pop es
	pop ds

	popa
	add esp, 8          ; pop isr_num and err_num
	iret

%macro ISR_CUSTOM 2
	global _isr%1
	_isr%1:
		cli
		push %2 ; custom value
		push %1 ; isr number
		jmp isr_common_stub
%endmacro

%macro ISR_NOERR 1
	global _isr%1
	_isr%1:
		cli
		push 0  ; 0 error code
		push %1 ; isr number
		jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
	global _isr%1
	_isr%1:
		cli
		; error code is already pushed by the cpu
		push %1 ; isr number
		jmp isr_common_stub
%endmacro


ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31
ISR_CUSTOM 32,0
ISR_CUSTOM 33,1
ISR_CUSTOM 34,2
ISR_CUSTOM 35,3
ISR_CUSTOM 36,4
ISR_CUSTOM 37,5
ISR_CUSTOM 38,6
ISR_CUSTOM 39,7
ISR_CUSTOM 40,8
ISR_CUSTOM 41,9
ISR_CUSTOM 42,10
ISR_CUSTOM 43,11
ISR_CUSTOM 44,12
ISR_CUSTOM 45,13
ISR_CUSTOM 46,14
ISR_CUSTOM 47,15
ISR_CUSTOM 128,0

global isrs_end
isrs_end:
align 4096
