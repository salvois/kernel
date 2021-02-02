/*
FreeDOS-32 kernel
Copyright (C) 2008-2020  Salvatore ISAJA

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifdef __ELF__
#define MULTIBOOT_HEADER_FLAGS 0x00000003
#else
#define MULTIBOOT_HEADER_FLAGS 0x00010003
#endif

#include "kernel_asm.h"

.section .boot, "ax"
.align 4
.globl Boot_multibootHeader
Boot_multibootHeader:
    .long 0x1BADB002 /* magic */
    .long MULTIBOOT_HEADER_FLAGS /* flags */
    .long -(0x1BADB002 + MULTIBOOT_HEADER_FLAGS) /* checksum */
    #ifndef __ELF__
    .long Boot_multibootHeaderPhysicalAddress /* header_addr */
    .long CSYMBOL(Boot_imageBeginPhysicalAddress) /* load_addr */
    .long CSYMBOL(Boot_dataEndPhysicalAddress) /* load_end_addr */
    .long CSYMBOL(Boot_imageEndPhysicalAddress) /* bss_end_addr */
    .long Boot_entryPointPhysicalAddress /* entry_addr */
    #endif
.globl CSYMBOL(Boot_multibootMagic)
CSYMBOL(Boot_multibootMagic): /* uint32_t Boot_multibootMagic */
    .long 0
.globl CSYMBOL(Boot_mbiPhysicalAddress)
CSYMBOL(Boot_mbiPhysicalAddress): /* const MultibootMbi *Boot_mbiPhysicalAddress */
    .long 0

bspHigherHalfEntry:
    movw $16, %cx
    movw %cx, %ds
    movw %cx, %es
    movw %cx, %fs
    movw %cx, %gs
    movw %cx, %ss
    movl $(CSYMBOL(Boot_stack) + BOOT_STACK_SIZE), %esp
    pushl $0
    popf
    movl %esi, CSYMBOL(Boot_multibootMagic)
    movl %edx, CSYMBOL(Boot_mbiPhysicalAddress)
    call CSYMBOL(Boot_entry)
    # Here eax points to the Cpu struct for the bootstrap processor
    lea CPU_TOP_OF_STACK(%eax), %esp
    call CSYMBOL(Boot_bspEntry)
    # Continue pretending that we are returning from an interrupt
    jmp CSYMBOL(Cpu_returnToUserMode)

apHigherHalfEntry:
    movw $16, %cx
    movw %cx, %ds
    movw %cx, %es
    movw %cx, %fs
    movw %cx, %gs
    movw %cx, %ss
    xorl %ecx, %ecx
1:
    cmpl %ecx, CSYMBOL(Cpu_cpuCount)
    je 3f # CPU not found, should never happen
    mov CPU_LAPIC_VIRTUAL_ADDRESS + 0x20, %eax # Read LAPIC ID register
    shrl $24, %eax # eax = LAPIC ID
    movl CSYMBOL(Cpu_cpus)(,%ecx,4), %esi # Cpu *esi = Cpu_cpus[ecx]
    cmpl %eax, CPU_LAPICID_OFFSET(%esi) # Compare the current LAPIC ID against esi->lapicId
    je 2f
    incl %ecx
    jmp 1b
2:
    lea CPU_TOP_OF_STACK(%esi), %esp
    pushl $0
    popf
    call CSYMBOL(Boot_apEntry)
    # Continue pretending that we are returning from an interrupt
    jmp CSYMBOL(Cpu_returnToUserMode)
3:
    cli
    hlt

/* Startup code and GDT for application processors, loaded after 1 MiB and relocated by boot code below 1 MiB */
.code16

.section .aplow, "ax"
Boot_apEntryPoint:
    /* Here we are in real mode */
    cli
    lgdt apGdtLocation
    movl $CSYMBOL(Boot_kernelPageDirectoryPhysicalAddress), %eax
    movl %eax, %cr3
    movl %cr4, %eax
    or $0x00000090, %eax # enable global pages (bit 7) and large pages (bit 4)
    movl %eax, %cr4
    movl %cr0, %eax
    orl $0x80000001, %eax # enable paging (bit 31) and protected mode (bit 0)
    movl %eax, %cr0
    /* Here we are in 32-bit protected mode with paging enabled. Do a 32-bit ljmp $8, $apHigherHalfEntry */
    .byte 0x66
    .byte 0xEA
    .long apHigherHalfEntry
    .word 8

    .align 4
    .word 0 /* padding */
apGdtLocation:
    .word 23 /* sizeof(apHigherHalfGdt) - 1 */
    .long apGdt
apGdt:
    .long 0, 0 /* null segment */
    .long 0x0000FFFF, 0x00CF9A00 /* 4 GiB flat code segment with base 0 */
    .long 0x0000FFFF, 0x00CF9200 /* 4 GiB flat data segment with base 0 */

/* Startup code and GDT for the bootstrap processor, initially running at physical address 0x10???? then jumping at virtual address 0xC010???? */
.code32

.section .bsplow, "ax"
.globl Boot_entryPoint
Boot_entryPoint:
    cli
    lgdt bspGdtLocation
    movl %eax, %esi # save the Multiboot magic
    movl %ebx, %edx # save the Multiboot Information structure physical address
    # Populate the page directory for the permanently mapped physical memory in kernel address space.
    # 896 MiB (224 4-MiB superpages) starting at virtual address 0xC0000000.
    # for (int ecx = 0; ecx < 224; ecx++) {
    #     Boot_kernelPageDirectory[768 + ecx] = (ecx << 22) | ptGlobal(8) | ptLargePage(7) | ptWriteable(1) | ptPresent(0)
    # }
    mov $CSYMBOL(Boot_kernelPageDirectoryPhysicalAddress), %ebx
    xor %ecx, %ecx
1:
    mov %ecx, %eax
    shl $22, %eax
    or $0x183, %eax # ptGlobal(8) | ptLargePage(7) | ptWriteable(1) | ptPresent(0)
    mov %eax, 3072(%ebx, %ecx, 4)
    inc %ecx
    cmp $224, %ecx
    jne 1b
    movl $0x183, 0(%ebx)
    movl %ebx, %cr3
    movl %cr4, %eax
    or $0x00000090, %eax # enable global pages (bit 7) and large pages (bit 4)
    movl %eax, %cr4
    movl %cr0, %eax
    or $0x80000000, %eax # enable paging (bit 31)
    movl %eax, %cr0
    ljmp $8, $bspHigherHalfEntry

/* GDT for the bootstrap processor, located right after 1 MiB */
    .align 4
    .word 0 # padding
bspGdtLocation:
    .word 23 # sizeof(bspHigherHalfGdt) - 1
    .long bspGdt
bspGdt:
    .long 0, 0 # null segment
    .long 0x0000FFFF, 0x00CF9A00 # page granular, 32-bit, non-system, code, r/x, ring 0, 4 GiB limit
    .long 0x0000FFFF, 0x00CF9200 # page granular, 32-bit, non-system, data, r/w, ring 0, 4 GiB limit


.section .bss
.globl CSYMBOL(Boot_kernelPageDirectory)
    .align 4096
    .lcomm CSYMBOL(Boot_kernelPageDirectory), 4096
.globl CSYMBOL(Boot_stack)
    .lcomm CSYMBOL(Boot_stack), BOOT_STACK_SIZE
