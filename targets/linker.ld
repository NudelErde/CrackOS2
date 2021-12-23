ENTRY(start)

SECTIONS
{
	. = 1M;
	physical_start = .;
	_start_start = .;
	.boot : {
		KEEP(*(.multiboot_header))
	}
	.start_text : {
		build/asm_object_files/main.o (.text)
		build/cpp_object_files/start.o (.text)
	}
	.start_rodata : {
		build/asm_object_files/main.o (.rodata)
		build/cpp_object_files/start.o (.rodata)
	}
	.start_bss : {
		build/asm_object_files/main.o (.bss)
		build/cpp_object_files/start.o (.bss)
	}
	.start_data : {
		build/asm_object_files/main.o (.data)
		build/cpp_object_files/start.o (.data)
	}
	_start_end = ALIGN(0x1000);
	physical_kernel_start = _start_end;
	. = 0x400000000000; /*64 Ti*/
	kernel_start = .;

	code_start = .;
	.text_main : AT(code_start - kernel_start + physical_kernel_start) {
		*(.text)
		*(.text.*)
	}
	code_end = .;
	. = ALIGN(0x1000);

	rodata_start = .;
	.rodata_main : AT(rodata_start - kernel_start + physical_kernel_start) {
		*(.rodata)
		*(.rodata.*)
	}
	.rodata_end = .;
	. = ALIGN(0x1000);
	
	read_only_end = .;

	bss_start = .;
	.bss_main : AT(bss_start - kernel_start + physical_kernel_start) {
		*(.bss)
		*(.bss.*)
	}
	bss_end = .;
	a = ALIGN(0x1000);

	data_start = .;
	.data_main : AT(data_start - kernel_start + physical_kernel_start) {
		*(.data)
		*(.data.*)
		*(.got)
		*(.got.*)
	}
	data_end = .;
	. = ALIGN(0x1000);
	kernel_end = .;
	physical_end = kernel_end - kernel_start + physical_kernel_start;
}