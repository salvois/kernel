/*
FreeDOS-32 kernel
Copyright (C) 2008-2018  Salvatore ISAJA

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
#include "kernel_asm.h"

/**
 * Interrupt trampolines.
 *
 * We set up trampolines that provide a valid C language environment for
 * Cpu_handleSyscallOrInterrupt() that does the real work.
 * All trampolines share most of the code, except a small portion on the
 * beginning that pushes the vector number to the stack and a dummy error code
 * for vectors that do not provide one.
 */
vector=0
.rept 255 # skip vector 0xFF, that is used for LAPIC spurious interrupts (must be consistent with IrqVector.spuriousInterruptVector in kernel.h)
.balign 16
.if vector == 0
.global CSYMBOL(Cpu_firstInterruptTrampoline)
CSYMBOL(Cpu_firstInterruptTrampoline):
.endif
.if vector == 1
.global CSYMBOL(Cpu_secondInterruptTrampoline)
CSYMBOL(Cpu_secondInterruptTrampoline):
.endif
.if (vector != 8) && (vector != 10) && (vector != 11) && (vector != 12) && (vector != 13) && (vector != 14) && (vector != 17)
    push $0 # dummy error code
.endif
    push $vector
    jmp commonInterruptTrampoline
vector = vector + 1
.endr

/**
 * This is the portion of the interrupt trampoline common to all vector numbers.
 * It saves the current context, prepares a "sane" kernel environment if coming
 * from user mode, and calls Cpu_handleSyscallOrInterrupt() to do the real work.
 * The order of pushed registers must be kept consistent with ThreadRegisters.
 */
commonInterruptTrampoline:
    testl $3, 12(%esp)
    jnz commonEntry
    # If we arrive here, we have interrupted a kernel-mode thread.
    push %eax
    push %ecx
    push %edx
    push %ebx
    push %ebp
    push %esi
    push %edi
    mov %gs:(CPU_THISCPU_OFFSET), %ecx # the cpu argument for Cpu_handleSyscallOrInterrupt()
    mov CPU_CURRENTTHREAD_OFFSET(%ecx), %edx # pointer to the current thread
    lea -THREADREGISTERS_EDI_OFFSET(%esp), %edx # pointer to the beginning of struct ThreadRegisters
    mov %edx, THREAD_REGS_OFFSET(%edx)
    jmp callHandler

/**
 * Entry point for sysenter.
 * ecx = user mode stack pointer for sysexit.
 * edx = user mode instruction pointer for sysexit.
 * esp = pointer to the ring 0 stack in the TSS of the current CPU.
 * eax = bits 7..0: system call number; other bits are system call specific.
 */
.global CSYMBOL(Cpu_sysenter)
CSYMBOL(Cpu_sysenter):
    mov (%esp), %esp
    sub $24, %esp # skip ss, esp, eflags, cs, eip and error code in ThreadRegisters
    push $THREADREGISTERS_VECTOR_SYSENTER

/** This is the common entry code for interrupts and sysenter. */
commonEntry:
    push %eax
    push %ecx
    push %edx
    push %ebx
    push %ebp
    push %esi
    push %edi
    mov %ds, %eax
    mov %es, %ebx
    mov (-THREADREGISTERS_EDI_OFFSET - THREAD_REGSBUF_OFFSET + THREAD_CPU_OFFSET)(%esp), %ecx # cpu argument for Cpu_handleSyscallOrInterrupt()
    cmp $CPU_FLAT_USER_DS, %eax
    jne customDsEsEntry
    cmp $CPU_FLAT_USER_DS, %ebx
    jne customDsEsEntry
callHandler:
    lea CPU_STRUCT_SIZE(%ecx), %esp # top of the kernel stack for the current CPU
    cld
    call CSYMBOL(Cpu_handleSyscallOrInterrupt)
/** Returns to user mode, either with sysexit or iret. Besides the surrounding code, this is jumped in by bsp/apHigherHalfEntry in Boot_asm.S. */
.global CSYMBOL(Cpu_returnToUserMode)
CSYMBOL(Cpu_returnToUserMode):
    # Here eax points the ThreadRegisters structure for the (possibly new) current thread
    testl $THREADREGISTERS_VECTOR_CUSTOMDSES, THREADREGISTERS_VECTOR_OFFSET(%eax)
    jnz customDsEsExit
    lea THREADREGISTERS_EDI_OFFSET(%eax), %esp
commonExit:
    pop %edi
    pop %esi    
    pop %ebp
    pop %ebx
    pop %edx
    pop %ecx
    pop %eax
    cmpl $THREADREGISTERS_VECTOR_SYSENTER, (%esp)
    jb interruptExit
    sti
    sysexit
interruptExit:
    add $8, %esp # pop vector and error code (either real or dummy)
    iret
customDsEsEntry:
    orl $THREADREGISTERS_VECTOR_CUSTOMDSES, 32(%esp)
    push %eax
    push %ebx
    mov $CPU_FLAT_USER_DS, %eax
    mov %eax, %ds
    mov %eax, %es
    jmp callHandler
customDsEsExit:
    andl $~THREADREGISTERS_VECTOR_CUSTOMDSES, THREADREGISTERS_VECTOR_OFFSET(%eax)
    lea THREADREGISTERS_ES_OFFSET(%eax), %esp
    pop %es
    pop %ds
    jmp commonExit

/** Handler for spurious interrupts from the Local APIC, just IRET. */
.global CSYMBOL(Cpu_spuriousInterruptHandler)
CSYMBOL(Cpu_spuriousInterruptHandler):
    iret

/** The idle thread just halts in a loop without consuming stack. */
.global CSYMBOL(Cpu_idleThreadFunction)
CSYMBOL(Cpu_idleThreadFunction):
    sti
    hlt
    jmp Cpu_idleThreadFunction
