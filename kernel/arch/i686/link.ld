ENTRY(_entry)
SECTIONS
{
	. = 1M;
	phys = .;

	start = .;

	/* read-only code (initialized) */
	.text ALIGN(4K) : {
		code = .;
		*(.mboot) /* make sure the mboot header is at the very beginning of the file */
		*(.text) /* read-only code */
		. = ALIGN(4K);
	}

	/* read-only data (initialized) */
	.rodata ALIGN(4K) : {
		rodata = .;
		*(.rodata) /* read-only data */
		. = ALIGN(4K);
	}

	/* read-write data (initialized) */
	.data ALIGN(4K) : {
		data = .;
		*(.data)
		. = ALIGN(4K);
	}

	/* read-write data (uninitialized) */
	.bss ALIGN(4K) : {
		bss = .;
		*(COMMON)
		*(.bss)
	}

	end = .;
}
