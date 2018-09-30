global read_eip:function (read_eip.end - read_eip)
read_eip:
	mov eax, [esp]
	ret
.end:

extern __switch_task
global switch_task:function (switch_task.end - switch_task)
switch_task:
	pushf
	pusha

	call __switch_task
.done:
	popa
	popf
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
