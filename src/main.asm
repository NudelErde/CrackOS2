extern kernel_end
extern kernel_start
extern physical_kernel_start
extern __kernel_main
global start
global page_table_l4
global gdt64.pointer

section .text
bits 64
main_in_long_mode:
    mov ax, 0b10000
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    call __kernel_main
long_end:
    hlt
    jmp long_end

section .text
bits 32
start:
	cli
    mov esp, stack_top
    push ebx

	call printHello

    call check_cpuid
    call check_long_mode

    call setup_paging_table
	pop edi
    call main64

    jmp stop

printHello:
	mov esi, .string
	call print
	ret
.string:
	db "Hello World", 0

check_multiboot:
	cmp eax, 0x36d76289
	jne error
	ret

check_cpuid:
	pushfd
	pop eax
	mov ecx, eax
	xor eax, 1 << 21
	push eax
	popfd
	pushfd
	pop eax
	push ecx
	popfd
	cmp eax, ecx
	je error
	ret

check_long_mode:
	mov eax, 0x80000000
	cpuid
	cmp eax, 0x80000001
	jb error

	mov eax, 0x80000001
	cpuid
	test edx, 1 << 29
	jz error
	
	ret

main64:
	mov eax, page_table_l4
	mov cr3, eax

	; enable PAE
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	; enable paging
	mov eax, cr0
	or eax, 1 << 31
	mov cr0, eax

	lgdt [gdt64.pointer]
	jmp 0b1000:main_in_long_mode

    ret

setup_paging_table: ; identity map
	mov eax, page_table_l3
	or eax, 0b11 ; present, writable
	mov [page_table_l4], eax
	
	mov eax, page_table_l2
	or eax, 0b11 ; present, writable
	mov [page_table_l3], eax

	mov ecx, 0 ; counter
.loop:

	mov eax, 0x200000 ; 2MiB
	mul ecx
	or eax, 0b10000011 ; present, writable, huge page
	mov [page_table_l2 + ecx * 8], eax

	inc ecx ; increment counter
	cmp ecx, 512 ; checks if the whole table is mapped
	jne .loop ; if not, continue

	ret

error:
	mov esi, .string
	call print
.string:
	db "Error", 0

print:
	mov edi, 0xB8000 ; output to edi
.loop:
	mov al, [esi] ; current char in eax
	
	mov byte [edi], al ; char in output address
	mov byte [edi+1], 4 ; color red

	inc ESI
	inc EDI
	inc EDI

	cmp al, 0

	jne .loop
	ret

stop:
    hlt
    jmp stop

section .bss
align 4096
page_table_l4:
	resb 4096
page_table_l3:
	resb 4096
page_table_l2:
	resb 4096
stack_bottom:
	resb 4096 * 4
stack_top:

section .rodata
gdt64:
	dq 0 ; zero entry
.code_segment: equ $ - gdt64
	dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; code segment
.data_segment: equ $ - gdt64
	dq (1 << 41) | (1 << 44) | (1 << 47) ; data segment
.code_segment_ring3: equ $ - gdt64
	dq (1 << 43) | (1 << 44) | (1 << 45) | (1 << 46) | (1 << 47) | (1 << 53) ; ring 3 code segment
.data_segment_ring3: equ $ - gdt64
	dq (1 << 41) | (1 << 44) | (1 << 45) | (1 << 46) | (1 << 47) ; ring 3 data segment
.task_state_segment: equ $ - gdt64
	dq 0
	dq 0
.pointer:
	dw $ - gdt64 - 1 ; length
	dq gdt64 ; address