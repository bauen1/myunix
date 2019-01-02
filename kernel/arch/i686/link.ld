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
	__start_shared_data = .;
	shared_data : { /* data shared between kernel and userspace */
		*(shared_data)
	}
	. = ALIGN(4K);
	__stop_shared_data = .;

	. = ALIGN(4K);
	__start_shared_text = .;
	shared_text : { /* text shared between kernel and userspace */
		*(shared_text)
	}
	. = ALIGN(4K);
	__stop_shared_text = .;

	_end = .;

	/DISCARD/ : {
		*(.comment)
	}
}
