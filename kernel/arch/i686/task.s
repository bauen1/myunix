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
