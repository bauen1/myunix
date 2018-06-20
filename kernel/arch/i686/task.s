global read_eip:function (read_eip.end - read_eip)
read_eip:
	mov eax, [esp]
	ret
.end:

global kidle:function (kidle.end - kidle)
kidle:
	sti
.loop:
	hlt
	jmp .loop
.end:

extern _switch_task
global switch_task:function (switch_task.end - switch_task)
switch_task:
	pushf
	pusha

	call _switch_task
.done:
	popa
	popf
	ret
.end:

extern _ktask_exit
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

	; call _ktask_exit(eax = status)
	push eax
	; if _ktask_exit tries to return, halt the cpu
	push halt
	jmp _ktask_exit
.end:
