; extern void tss_flush();
global tss_flush:function (tss_flush.end - tss_flush)
tss_flush:
	mov ax, 0x2B ; gdt offset to the tss
	ltr ax
	ret
.end:
