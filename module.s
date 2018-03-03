; assemble using 'nasm -fbin -o kernel/iso/modules/test module.s'
[ORG 0x1000000]
[BITS 32]
_start:
	mov ecx, 0xDEADBEEF

	mov eax, 0
	mov bx, 'm'
	int $80
	mov bx, 'o'
	int $80
	mov bx, 'd'
	int $80
	mov bx, '0'
	int $80
	mov bx, '!'
	int $80
	mov bx, $0A
	int $80
.loop:
	mov eax, 1
	int $80
	mov eax, 0
	int $80
	mov eax, 0
	mov ebx, '@'
	int $80
	jmp .loop
