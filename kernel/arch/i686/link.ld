ENTRY(_entry)

SECTIONS
{
	. = 1M;
	phys = .;

	__text_start = .;
	.text : { /* read-only code */
		*(.text)
	}
	. = ALIGN(4K);
	__text_end = .;


	__data_start = .;
	.data : { /* read-write data */
		*(.data)
	}
	. = ALIGN(4K);
	__data_end = .;

	__bss_start = .;
	.bss : { /* read-write data */
		*(COMMON)
		*(.bss)
	}
	. = ALIGN(4K);
	__bss_end = .;

	_end = .;

	/DISCARD/ : {
		*(.comment)
	}
}
