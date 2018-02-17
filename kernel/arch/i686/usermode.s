; __attribute__((noreturn))  __jump_to_userspace(void *stack, void *code);
global __jump_to_userspace:function (__jump_to_userspace.end - __jump_to_userspace)
__jump_to_userspace:
	mov ax, 0x23
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	mov eax, [esp + 4] ; stack ptr
	mov ebx, [esp + 8] ; function ptr

	mov esp, eax

	push 0x23
	push eax
	pushf
	push 0x1B
	push ebx
	iret
.end:

align 4096
global __hello_userspace
msg:
	db 'hello world from ring 3!\n', 0
__hello_userspace:
	mov eax, 0xFF ; dump regs
	int 0x80
	mov eax, 0x0
	mov ebx, 'h'
	int 0x80
	mov eax, 0x0
	mov ebx, 'e'
	int 0x80
	mov eax, 0x0
	mov ebx, 'l'
	int 0x80
	mov eax, 0x0
	mov ebx, 'l'
	int 0x80
	mov eax, 0x0
	mov ebx, 'o'
	int 0x80
	mov eax, 0x0
	mov ebx, ' '
	int 0x80
	mov eax, 0x0
	mov ebx, 'f'
	int 0x80
	mov eax, 0x0
	mov ebx, 'r'
	int 0x80
	mov eax, 0x0
	mov ebx, 'o'
	int 0x80
	mov eax, 0x0
	mov ebx, 'm'
	int 0x80
	mov eax, 0x0
	mov ebx, ' '
	int 0x80
	mov eax, 0x0
	mov ebx, 'r'
	int 0x80
	mov eax, 0x0
	mov ebx, 'i'
	int 0x80
	mov eax, 0x0
	mov ebx, 'n'
	int 0x80
	mov eax, 0x0
	mov ebx, 'g'
	int 0x80
	mov eax, 0x0
	mov ebx, ' '
	int 0x80
	mov eax, 0x0
	mov ebx, '3'
	int 0x80
	mov eax, 0x0
	mov ebx, '!'
	int 0x80

.loop:
	mov eax, 0x01
	int 0x80
	mov eax, 0x00
	int 0x80

	jmp .loop
