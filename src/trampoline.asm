extern page_table_l4
extern gdt64.pointer
extern secondaryCpuMain
extern interruptVectorTable
global trampolineStart
global test_data

section .text
bits 16
align 4096
trampolineStart:
    cli
    mov SP, trampoline_stack_end ; set stack pointer
    sub SP, 0x10
    lgdt [trampolineGDT.pointer]
    mov eax, cr0
    or al, 1 ; enable protected mode
    mov cr0, eax
    jmp 0b1000:trampoline32bit ; jump to 32-bit trampoline

section .text
bits 32
trampoline32bit:
    mov ax, 0b10000
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, trampoline_stack_end
    sub esp, 0x10

    mov eax, page_table_l4
    mov cr3, eax ; set page table
    mov eax, cr4

    or eax, 1 << 5 ; enable PAE
    mov cr4, eax

    mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8 ; enable long mode
	wrmsr
    
    mov eax, cr0
	or eax, 1 << 31 ; enable paging
	mov cr0, eax

    lgdt [gdt64.pointer]
    jmp 0b1000:trampoline64bit ; jump to 64-bit trampoline

section .text
bits 64
trampoline64bit:
    mov ax, 0b10000
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov rsp, trampoline_stack_end
    sub rsp, 0x10
    mov rbp, rsp

    pushfq

    mov word[lidtDescriptor], 0x0FFF; size
    mov rax, interruptVectorTable
    mov qword[lidtDescriptor + 2], rax; offset
    lidt [lidtDescriptor]

    mov rax, secondaryCpuMain

    call rax ; jump to secondary cpu main
.loop:
    cli
    hlt
    jmp .loop

section .rodata
align 4096
trampolineGDT:
    dq 0
.codeSegment:
    dq (0xFFFF) | (0xF << 48) | (1 << 43) | (1 << 44) | (1 << 47) | (1 << 54) | (1 << 55)
.dataSegment:
    dq (0xFFFF) | (0xF << 48) | (1 << 41) | (1 << 44) | (1 << 47) | (1 << 54) | (1 << 55)
.pointer:
	dw $ - trampolineGDT - 1 ; length
	dd trampolineGDT ; address

section .bss
align 4096
trampoline_stack_start:
    resb 4096
trampoline_stack_end:
lidtDescriptor:
    resb 10