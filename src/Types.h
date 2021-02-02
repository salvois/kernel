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
#ifndef TYPES_H_INCLUDED
#define	TYPES_H_INCLUDED

//#define NDEBUG
#include "kernel_asm.h"

#define LOG_NONE 0
#define LOG_VIDEO 1
#define LOG_RS232LOG 2
#define LOG_PORTE9LOG 3
#ifndef LOG
    //#define LOG LOG_NONE
    //#define LOG LOG_VIDEO
    #define LOG LOG_RS232LOG
    //#define LOG LOG_PORTE9LOG
#endif
#define SPINLOCK_INSTRUMENTED 0

#include "assert.h"
#include "stddef.h"
#include "stdarg.h"
#include "stdbool.h"
#include "stdint.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"
#include "AtomicWord.h"
#include "NaryTrie.h"
#include "LinkedList.h"

/** Prints the specified message on screen and forcefully halts the system. */
__attribute__((noreturn)) void panic(const char *format, ...);
/** Compiler hint to mark an expected condition. */
#define LIKELY(x)   __builtin_expect((x), 1)
/** Compiler hint to mark an unexpected condition. */
#define UNLIKELY(x) __builtin_expect((x), 0)
/** Max number of pages per task for TLB invalidation of individual pages (invlpg) before switching to full TLB invalidation. */
#define TASK_MAX_TLB_SHOOTDOWN_PAGES 8

typedef struct Task Task;
typedef struct Capability Capability;
typedef struct Thread Thread;
typedef struct Cpu Cpu;
typedef struct CpuNode CpuNode;
typedef struct { uintptr_t v; } CapabilityAddress;
typedef struct { uintptr_t v; } PhysicalAddress;
typedef struct { uintptr_t v; } VirtualAddress;
typedef struct { uintptr_t v; } FrameNumber;


/******************************************************************************
 * Priority queue
 ******************************************************************************/

Trie_header(PriorityQueueImpl, 1, unsigned)

/** Node of a priority queue, storing its effective priority. 24 bytes. */
typedef struct PriorityQueueNode {
    PriorityQueueImpl_Node n;
    unsigned key;
} PriorityQueueNode;

typedef struct PriorityQueue {
    PriorityQueueImpl impl;
    PriorityQueueNode *min;
} PriorityQueue;


/******************************************************************************
 * Utility functions
 ******************************************************************************/

/** Divides a 64-bit unsigned quantity by a 32-bit unsigned quantity. */
static inline uint32_t div(uint64_t dividend, uint32_t divisor) {
    uint32_t quotient;
    asm volatile("divl %2" : "=a" (quotient) : "A" (dividend), "q" (divisor));
    return quotient;
}

/** Divides \a dividend by \a divisor and fills \a quotient and \a remainder. */
static inline void divmod(uint64_t dividend, uint32_t divisor, uint32_t *quotient, uint32_t *remainder) {
    asm volatile("divl %3" : "=a" (quotient), "=d" (remainder) : "A" (dividend), "q" (divisor));
}

/** Multiplies two 32-bit unsigned quantities into a 64-bit unsigned quantity. */
static inline uint64_t mul(uint32_t a, uint32_t b) {
    uint64_t result;
    asm volatile("mull %2" : "=A" (result) : "a" (a), "q" (b));
    return result;
}

/** Reads an uint8_t from I/O port. */
static inline uint8_t inp(uint16_t port) {
    uint8_t res;
    asm volatile("inb %1, %0" : "=a" (res) : "d" (port));
    return res;
}

/** Read an uint16_t from I/O port. */
static inline uint16_t inpw(uint16_t port) {
    uint16_t res;
    asm volatile("inw %1, %0" : "=a" (res) : "d" (port));
    return res;
}

/** Reads an uint32_t from I/O port. */
static inline uint32_t inpd(uint16_t port) {
    uint32_t res;
    asm volatile("inl %1, %0" : "=a" (res) : "d" (port));
    return res;
}

/** Writes an uint8_t to I/O port. */
static inline void outp(uint16_t port, uint8_t value) {
    asm volatile("outb %1, %0" : : "d" (port), "a" (value));
}

/** Writes an uint16_t to I/O port. */
static inline void outpw(uint16_t port, uint16_t value) {
    asm volatile("outw %1, %0" : : "d" (port), "a" (value));
}

/** Writes an uint32_t to I/O port. */
static inline void outpd(uint16_t port, uint32_t value) {
    asm volatile("outl %1, %0" : : "d" (port), "a" (value));
}

/** Read memory barrier, a.k.a. load fence. */
static inline void readBarrier() {
    asm volatile("lfence" : : : "memory");
}

/** Write memory barrier, a.k.a. store fence. */
static inline void writeBarrier() {
    asm volatile("sfence" : : : "memory");
}

/** Read and write memory barrier, a.k.a. load and store fence. */
static inline void readWriteBarrier() {
    asm volatile("mfence" : : : "memory");
}


typedef struct Spinlock {
    #if SPINLOCK_INSTRUMENTED
    volatile uint64_t contendCount;
    #endif
    volatile uint16_t lock1;
    volatile uint16_t lock2;
} Spinlock;

typedef struct SlabAllocator_FreeItem {
    struct SlabAllocator_FreeItem *next;
} SlabAllocator_FreeItem;

typedef struct SlabAllocator {
    SlabAllocator_FreeItem *freeItems;
    size_t itemSize;
    size_t itemsPerSlab;
    uint8_t *brk;
    uint8_t *limit;
    Task    *task;
} SlabAllocator;


/******************************************************************************
 * IPC
 ******************************************************************************/

enum {
    Message_wordCount = 16,
    Message_priorityInheritance = 1 << 0,
    Message_nonBlocking = 1 << 1,
    Message_asynchronous = 1 << 2,
    Message_notification = 1 << 3
};

typedef struct Channel {
    PriorityQueueNode node;
    Thread *sendingThread;
    Task *ownerTask;
    Word tag;
    Word endpointBadge;
    Word padding[6];
    uint8_t data[64];
} Channel;

/**
 * Dummy union to check the size of the ChannelStruct struct.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union ChannelChecker {
    char wrongSize[sizeof(Channel) == 32 * sizeof(Word)];
}; 

typedef struct Endpoint {
    PriorityQueue channels;
    PriorityQueue receivers;
} Endpoint;


/******************************************************************************
 * Thread
 ******************************************************************************/

/** Saved register set of a thread (76 bytes). */
typedef struct ThreadRegisters {
    uint32_t ldt; // undefined for kernel-mode threads, bits 31..16 are reserved
    uint32_t fs; // undefined for kernel-mode threads, bits 31..16 are reserved
    uint32_t gs; // must be kernelGS for kernel-mode threads, bits 31..16 are reserved
    uint32_t es; // undefined for kernel-mode threads, bits 31..16 are reserved
    uint32_t ds; // undefined for kernel-mode threads, bits 31..16 are reserved
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t vector; // interrupt vector number, or THREADREGISTERS_VECTOR_SYSENTER if coming from sysenter
    uint32_t error;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp; // undefined for kernel-mode threads
    uint32_t ss; // undefined for kernel-mode threads, bits 31..16 are reserved
} ThreadRegisters;


/** Mutually exclusive thread states. */
typedef enum ThreadState {
    /** Thread is currently running (thread->cpu->currentThread == thread). */
    threadStateRunning,
    /** Thread is sitting in the ready queue. */
    threadStateReady,
    /** Thread is candidate to be executed as next thread on thread->cpu. */
    threadStateNext,
    /** Thread is blocked. */
    threadStateBlocked
} ThreadState;

/** Type of the function associated with a thread in kernel-mode. */
typedef __attribute__((fastcall)) int (*ThreadFunction)(void *);

struct Thread {
    Cpu *cpu; // The CPU this thread is running on or ran last time
    Task *task;
    ThreadFunction threadFunction;
    void *threadFunctionParam;
    ThreadState state;
    PriorityQueueNode queueNode; // Node to link this thread in a PriorityQueue
    unsigned nice; // The higher the nice level, the more often a thread is executed among threads with the same effective priority
    unsigned priority; // Nominal priority level, the higher the level the higher the priority
    PriorityQueue *threadQueue; // The queue this thread is currently in (NULL if delayed or running)
    unsigned timesliceRemaining; // in nanoseconds
    uint64_t runningTime; // Total CPU time since thread start, in microseconds
    bool cpuAffinity;
    bool kernelThread;
    bool kernelRestartNeeded;
    uint8_t *stack; // for kernel-mode threads
    ThreadRegisters *regs; // for user-mode threads points to regsBuf, for kernel-mode threads points to bottom of the stack
    ThreadRegisters regsBuf; // for user-mode threads and the idle thread of each CPU
    Endpoint endpoint;
    Channel channel;
    uint8_t padding[12]; // sizeof(Thread) must be a multiple of 16 bytes
};

/******************************************************************************
 * Capability
 ******************************************************************************/

/** Types of kernel objects referenced by capabilities. */
typedef enum KobjType {
    /** No kernel object, capability is free. */
    kobjNone,
    /** Capability refers to a memory frame, either mapped or unmapped. */
    kobjFrame,
    /** Capability refers to a Task. */
    kobjTask,
    /** Capability refers to a Thread. */
    kobjThread,
    /** Capability refers to a communication Channel. */
    kobjChannel,
    /** Capability refers to a communication Endpoint. */
    kobjEndpoint
} KobjType;

/**
 * Capability granting a user mode program access to a kernel object.
 * For memory frame capabilities:
 * - obj: bits 63/31..12 frame number
 * - badge: virtual address the frame is mapped into, if any
 * - next, prev: link to other capabilities to the same frame.
 */
struct Capability {
    uintptr_t   obj; // bits 3..0 identify the object type
    uintptr_t   badge;
    Capability *prev;
    Capability *next;
};

/** Returns the kernel object type referenced by the specified capability. */
static inline KobjType Capability_getObjectType(const Capability *cap) {
    return cap->obj & 0xF;
}

/** Returns the pointer to the actual kernel object referenced by the specified capability. */
static inline void *Capability_getObject(const Capability *cap) {
    return (void *) (cap->obj & ~0xF);
}


/******************************************************************************
 * CPU
 ******************************************************************************/

typedef struct Tsc {
    uint32_t nsPerTick; // <<20 on 32-bit
    uint32_t ticksPerNs; // <<23 on 32-bit
    uint32_t usPerTick; // <<32 on 32-bit
} Tsc;

typedef struct LapicTimer {
    uint64_t currentNanoseconds;
    uint64_t nextExpirationNanoseconds;
    uint32_t lastInitialCount;
    uint32_t maxTicks;
    uint32_t nsPerTick; // <<20 on 32-bit
    uint32_t ticksPerNs; // <<23 on 32-bit
} LapicTimer;

typedef uint32_t PageTableEntry;

typedef struct CpuDescriptor {
    uint32_t word0;
    uint32_t word1;
} CpuDescriptor;

/** Supported GDT segment selectors. */
enum CpuSelector {
    nullSelector = 0,
    flatKernelCS = 1 << 3,
    flatKernelDS = CPU_FLAT_KERNEL_DS,
    flatUserCS = (3 << 3) | 3,
    flatUserDS = CPU_FLAT_USER_DS,
    tssSelector = 5 << 3,
    kernelGS = CPU_KERNEL_GS,
    gdtEntryCount = 7
};

/** IA-32 Task State Segment (104 bytes). */
typedef struct Tss {
    uint32_t prevTask; // bits 31..16 are reserved
    uint32_t esp0;
    uint32_t ss0; // bits 31..16 are reserved
    uint32_t esp1;
    uint32_t ss1; // bits 31..16 are reserved
    uint32_t esp2;
    uint32_t ss2; // bits 31..16 are reserved
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es; // bits 31..16 are reserved
    uint32_t cs; // bits 31..16 are reserved
    uint32_t ss; // bits 31..16 are reserved
    uint32_t ds; // bits 31..16 are reserved
    uint32_t fs; // bits 31..16 are reserved
    uint32_t gs; // bits 31..16 are reserved
    uint32_t ldt; // bits 31..16 are reserved
    uint16_t debugTrap; // bits 15..1 are reserved
    uint16_t ioMap;
} Tss;

/**
 * Kernel state of a logical processor.
 * The nextThread member can be set by another CPU, whereas all other members
 * are CPU-local.
 */
struct Cpu {
    size_t        index;
    uint32_t      lapicId;
    bool          active;
    bool          rescheduleNeeded;
    bool          timesliceTimerEnabled;
    uint8_t       padding0;
    uint32_t      kernelEntryCount;
    Cpu          *thisCpu;
    CpuNode      *cpuNode;
    Thread       *currentThread;
    Thread       *nextThread; // thread to switch on fast reschedule, equal to currentThread at rest
    uint64_t      interruptTsc;
    uint64_t      interruptCount;
    uint64_t      lastScheduleTime; // used to compute time elapsed by the current thread
    uint64_t      scheduleArrival; // value of CpuNode.scheduleOrder when this CPU was scheduled
    // Cache line boundary
    LapicTimer    lapicTimer; // 32 bytes
    Tsc           tsc; // 12 bytes
    PriorityQueue readyQueue; // per-CPU ready threads, 12 bytes
    CpuDescriptor gdt[gdtEntryCount]; // 56 bytes
    Tss           tss; // 104 bytes
    Thread        idleThread; // 320 bytes
    uint8_t       stack[CPU_STACK_SIZE];
    uint8_t       padding1[12];
    AtomicWord    initialized; // true when boot is completed
};

/** Kernel state of a set of logical processors sharing scheduling decisions. */
struct CpuNode {
    Cpu         **cpus;
    size_t        cpuCount;
    uint64_t      scheduleArrival; // increases by one every time a CPU of the node is scheduled
    PriorityQueue readyQueue;
    Spinlock      lock;
};

/** Signature for an Interrupt Service Routine. */
typedef __attribute__((fastcall)) void (*Isr)(Cpu *cpu, void *param);

typedef struct IsrTableEntry {
    Isr isr;
    void* param;
} IsrTableEntry;

typedef struct AddressSpace {
    PageTableEntry root;
    size_t tlbShootdownPageCount;
    VirtualAddress tlbShootdownPages[TASK_MAX_TLB_SHOOTDOWN_PAGES];
    LinkedList_Node shootdownFrameListHead;
} AddressSpace;

#endif
