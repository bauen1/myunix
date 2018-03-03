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
	push ebx
	push esi
	push edi
	push ebp
	call _switch_task
.done:
	pop ebp
	pop edi
	pop esi
	pop ebx
	popf
	ret
.end:
