#include "CPUControl/cpu.hpp"
#include "ACPI/APIC.hpp"
#include "BasicOutput/Output.hpp"

[[noreturn]] void stop() {
    while (true) {
        asm("cli");
        asm("hlt");
    }
}
void halt() {
    asm("hlt");
}

asm(R"(
.text
.global getRSP
.type getRSP, @function
getRSP:
    movq %rsp, %rax
    subq $8, %rax
    ret
.global getRBP
.type getRBP, @function
getRBP:
    movq %rbp, %rax
    ret
.global getRIP
.type getRIP, @function
getRIP:
    movq (%rsp), %rax
    ret
.global getR12
.type getR12, @function
getR12:
    movq %r12, %rax
    ret
.global getR13
.type getR13, @function
getR13:
    movq %r13, %rax
    ret
.global getR14
.type getR14, @function
getR14:
    movq %r14, %rax
    ret
.global getR15
.type getR15, @function
getR15:
    movq %r15, %rax
    ret
.global getCR0
.type getCR0, @function
getCR0:
    movq %cr0, %rax
    ret
.global getCR2
.type getCR2, @function
getCR2:
    movq %cr2, %rax
    ret
.global getCR3
.type getCR3, @function
getCR3:
    movq %cr3, %rax
    ret
.global getCR4
.type getCR4, @function
getCR4:
    movq %cr4, %rax
    ret
.global getCR8
.type getCR8, @function
getCR8:
    movq %cr8, %rax
    ret
.global getDR0
.type getDR0, @function
getDR0:
    movq %dr0, %rax
    ret
.global getDR1
.type getDR1, @function
getDR1:
    movq %dr1, %rax
    ret
.global getDR2
.type getDR2, @function
getDR2:
    movq %dr2, %rax
    ret
.global getDR3
.type getDR3, @function
getDR3:
    movq %dr3, %rax
    ret
.global getDR6
.type getDR6, @function
getDR6:
    movq %dr6, %rax
    ret
.global getDR7
.type getDR7, @function
getDR7:
    movq %dr7, %rax
    ret
.global getRAX
.type getRAX, @function
    ret
.global getRBX
.type getRBX, @function
    movq %rbx, %rax
    ret
.global getRCX
.type getRCX, @function
    movq %rcx, %rax
    ret
.global getRDX
.type getRDX, @function
    movq %rdx, %rax
    ret
.global getRSI
.type getRSI, @function
    movq %rsi, %rax
    ret
.global getRDI
.type getRDI, @function
    movq %rdi, %rax
    ret
.global getR8
.type getR8, @function
    movq %r8, %rax
    ret
.global getR9
.type getR9, @function
    movq %r9, %rax
    ret
.global getR10
.type getR10, @function
    movq %r10, %rax
    ret
.global getR11
.type getR11, @function
    movq %r11, %rax
    ret
.global getFlags
.type getFlags, @function
getFlags:
    pushfq
    popq %rax
    ret
.global getPriviledgeLevel
.type getPriviledgeLevel, @function
    movq %cs, %rax
    andq $3, %rax
    ret
.global invalidatePage
.type invalidatePage, @function
invalidatePage:
    invlpg (%rdi)
    ret
)");