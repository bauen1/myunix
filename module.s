; assemble using 'nasm -fbin -o kernel/iso/modules/test module.s'
[ORG 0x1000000]
[BITS 32]
_start:
	mov ecx, 0xDEADBEEF

	mov eax, 0x4
	mov ebx, 1
	mov ecx, msg
	mov edx, msg.end - msg
	int 0x80

.loop:
	mov eax, 0x4
	mov ebx, 1
	mov ecx, msg2
	mov edx, msg2.end - msg2
	int $80
	mov eax, 0x2
	mov ebx, 100
	int $80
	jmp .loop

msg:
	db "hello world!", $0A
.end:

msg2:
	db "#"
.end:
