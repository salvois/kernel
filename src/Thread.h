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
#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include "Types.h"

/**
 * Dummy union to check that offsets and size of the ThreadRegisters struct are consistent with constants used in assembly.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union ThreadRegistersChecker {
    char wrongEsOffset[offsetof(ThreadRegisters, es) == THREADREGISTERS_ES_OFFSET];
    char wrongEdiOffset[offsetof(ThreadRegisters, edi) == THREADREGISTERS_EDI_OFFSET];
    char wrongVectorOffset[offsetof(ThreadRegisters, vector) == THREADREGISTERS_VECTOR_OFFSET];
}; 

/**
 * Dummy union to check that offsets and size of the Thread struct are consistent with constants used in assembly.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union ThreadChecker {
    char wrongSize[(sizeof(Thread) & 0xF) == 0];
    char wrongCpuOffset[offsetof(Thread, cpu) == THREAD_CPU_OFFSET];
    char wrongRegsOffset[offsetof(Thread, regs) == THREAD_REGS_OFFSET];
    char wrongRegsBufOffset[offsetof(Thread, regsBuf) == THREAD_REGSBUF_OFFSET];
}; 

#define THREAD_IDLE_PRIORITY 255

static inline bool Thread_isHigherPriority(const Thread *thread, const Thread *other) {
    return thread->queueNode.key < other->queueNode.key;
}

static inline bool Thread_isSamePriority(const Thread *thread, const Thread *other) {
    return thread->queueNode.key == other->queueNode.key;
}

/** Returns the thread object for embedding the specified PriorityQueueNode as queueNode member. */
static inline Thread *Thread_fromQueueNode(PriorityQueueNode *n) {
    return (Thread *) ((uint8_t *) n - offsetof(Thread, queueNode));
}

int Thread_initialize(Task *task, Thread *thread, unsigned priority, unsigned nice, uintptr_t entry, uintptr_t stackPointer);

#endif
