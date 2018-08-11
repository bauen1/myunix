global load_page_directory:function (load_page_directory.end - load_page_directory)
extern isrs_kernel_directory_ptr
load_page_directory:
	mov eax, [esp + 4]
	mov cr3, eax
	mov DWORD [ isrs_kernel_directory_ptr ], eax
	ret
.end:

global enable_paging:function (enable_paging.end - enable_paging)
enable_paging:
	mov eax, cr0
	or eax, 0x80000001
	mov cr0, eax
	ret
.end:
