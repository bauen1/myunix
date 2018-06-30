ENTRY(_entry)

SECTIONS
{
	. = 1M;

	.text : { /* read-only code */
		*(.text)
	}
	. = ALIGN(4K);
	_etext = .;

	.data : { /* read-write data */
		*(.data)
	}
	. = ALIGN(4K);
	_edata = .;

	.bss : { /* read-write data */
		*(COMMON)
		*(.bss)
	}
	. = ALIGN(4K);

	__start_mod_info = .;
	mod_info : { /**/
		*(mod_info)
	}
	. = ALIGN(4K);
	__stop_mod_info = .;

	. = ALIGN(4K);
	__start_isrs = .;
	isrs : { /* isrs code, shared with userspace*/
		*(isrs)
	}
	. = ALIGN(4K);
	__stop_isrs = .;

	. = ALIGN(4K);
	__start_user_shared = .;
	user_shared : { /* other stuff shared with userspace */
		*(user_shared)
	}
	. = ALIGN(4K);
	__stop_user_shared = .;

	_end = .;

	/DISCARD/ : {
		*(.comment)
	}
}
