ENTRY(_entry)

SECTIONS
{
	. = 1M;
	phys = .;

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

	_end = .;

	/DISCARD/ : {
		*(.comment)
	}
}
