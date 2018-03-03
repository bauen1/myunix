align 4096
global __hello_userspace
__hello_userspace:
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
	mov eax, 0
	mov ebx, '#'
	int 0x80
	mov eax, 0x02
	mov ebx, 100
	int 0x80
	jmp .loop
