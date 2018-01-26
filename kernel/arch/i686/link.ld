ENTRY(_entry)

SECTIONS
{
	. = 1M;
	phys = .;
	__start = .;

	__text_start = .;
	.text : { /* read-only code */
		*(.boot)
		*(.text)
	}
	. = ALIGN(4K);
	__text_end = .;

	__rodata_start = .;
	.rodata : { /* read-only data */
		*(.rodata)
	}
	. = ALIGN(4K);
	__rodata_end = .;

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

	__end = .;

	/DISCARD/ : {
		*(.comment)
	}
}
