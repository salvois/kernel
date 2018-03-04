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
#ifndef KERNEL_ASM_H_INCLUDED
#define	KERNEL_ASM_H_INCLUDED

#define CPU_LAPIC_VIRTUAL_ADDRESS 0xFEE00000 /* The page just before page tables */

// These constants represents the enum Selector for use in assembly
#define CPU_FLAT_KERNEL_DS (2 << 3)
#define CPU_FLAT_USER_DS ((4 << 3) | 3)
#define CPU_KERNEL_GS 0x30

/**
 * Offsets and size of the Cpu, Thread and ThreadRegisters structs.
 * @{
 */
#define CPU_LAPICID_OFFSET 0
#define CPU_THISCPU_OFFSET 8
#define CPU_CURRENTTHREAD_OFFSET 16
#define CPU_IDLE_THREAD_STACK_SIZE 512
#define CPU_IDLE_THREAD_STACK_OFFSET 596
#define CPU_STACK_OFFSET (CPU_IDLE_THREAD_STACK_OFFSET + CPU_IDLE_THREAD_STACK_SIZE)
#define CPU_STRUCT_LOG_SIZE 12
#define CPU_STRUCT_SIZE (1 << CPU_STRUCT_LOG_SIZE)
#define CPU_STACK_SIZE (CPU_STRUCT_SIZE - CPU_STACK_OFFSET)
#define THREAD_CPU_OFFSET 0
#define THREAD_REGS_OFFSET 76
#define THREAD_REGSBUF_OFFSET 80
#define THREADREGISTERS_ES_OFFSET (3 * 4)
#define THREADREGISTERS_EDI_OFFSET (5 * 4)
#define THREADREGISTERS_VECTOR_OFFSET (12 * 4)
#define THREADREGISTERS_VECTOR_MASK 0x3FF
#define THREADREGISTERS_VECTOR_SYSENTER 1000
#define THREADREGISTERS_VECTOR_CUSTOMDSES (1 << 10)
// @}

#define BOOT_STACK_SIZE 4096

#ifdef __ELF__
#define CSYMBOL(s) s
#else
#define CSYMBOL(s) _ ## s
#endif

#endif
