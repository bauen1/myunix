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
