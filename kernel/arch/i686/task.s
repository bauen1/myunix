global read_eip:function (read_eip.end - read_eip)
read_eip:
	mov eax, [esp]
	ret
.end:

extern switch_task
global call_switch_task:function (call_switch_task.end - call_switch_task)
call_switch_task:
	push ebp
	mov ebp, esp

	pushf
	pusha

	push DWORD [ebp + 8]
	call switch_task
	add esp, 4

	popa
	popf

	mov esp, ebp
	pop ebp
	ret
.end:

extern __ktask_exit
extern halt
global __call_ktask:function (__call_ktask.end - __call_ktask)
__call_ktask:
	; ensure interrupts are off
	cli

	; call kfunc
	pop eax
	call eax

	; remove name and extra
	add esp, 8

	; call __ktask_exit(eax = status)
	push eax
	; if __ktask_exit tries to return, halt the cpu
	push halt
	jmp __ktask_exit
.end:
