ENTRY(start)

PHDRS
{
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}

SECTIONS
{
	. = 1M;

	.boot ALIGN(8) :
	{
		KEEP(*(.multiboot_header))
	} :text

	.text ALIGN(16) :
	{
		*(.text .text.*)
	} :text

	.rodata ALIGN(16) : { *(.rodata .rodata.*) } :text
	.data ALIGN(16) : { *(.data .data.*) } :data
	.bss ALIGN(16) (NOLOAD) :
	{
		*(.bss .bss.*)
		*(COMMON)
	} :data
}
