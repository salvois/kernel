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
#ifndef KERNEL_H_INCLUDED
#define	KERNEL_H_INCLUDED

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

/** Prints the specified message on screen and forcefully halts the system. */
__attribute__((noreturn)) void panic(const char *format, ...);
/** Compiler hint to mark an expected condition. */
#define LIKELY(x)   __builtin_expect((x), 1)
/** Compiler hint to mark an unexpected condition. */
#define UNLIKELY(x) __builtin_expect((x), 0)
/** log2 of the size in bytes of a memory page or page frame. */
#define PAGE_SHIFT 12
/** Size in bytes of a memory page or page frame. */
#define PAGE_SIZE (1UL << PAGE_SHIFT)
/** Virtual address the kernel space begin at. */
#define HIGH_HALF_BEGIN 0xC0000000u
/** Highest possible address of physical memory usable for ISA DMA. */
#define ISADMA_MEMORY_REGION_FRAME_END (16 << 20 >> PAGE_SHIFT)
/** Highest possible address of permanently mapped physical memory. */
#define PERMAMAP_MEMORY_REGION_FRAME_END (896 << 20 >> PAGE_SHIFT)
/** The highest possible address for physical memory. */
#define MAX_PHYSICAL_ADDRESS 0xFFFFF000u // 4 GiB - 4096 bytes
/** Max number of pages per task for TLB invalidation of individual pages (invlpg) before switching to full TLB invalidation. */
#define TASK_MAX_TLB_SHOOTDOWN_PAGES 8
/** Number of nice levels (to weight threads with same priority). */
#define NICE_LEVELS 40
/** Frequency in Hz of the ACPI Power Management timer. */
#define ACPI_PMTIMER_FREQUENCY 3579545

typedef struct Frame Frame;
typedef struct Task Task;
typedef struct Capability Capability;
typedef struct Thread Thread;
typedef struct Cpu Cpu;
typedef struct CpuNode CpuNode;
typedef struct ListNode ListNode;


/******************************************************************************
 * Priority queue
 ******************************************************************************/

Trie_header(PriorityQueueImpl, 1, unsigned)

/** Node of a priority queue, storing its effective priority. 24 bytes. */
typedef struct PriorityQueueNode {
    PriorityQueueImpl_Node n;
    unsigned key;
} PriorityQueueNode;

static inline unsigned PriorityQueueNode_getKey(const PriorityQueueImpl_Node *n) {
    return ((const PriorityQueueNode *) n)->key;
}

typedef struct PriorityQueue {
    PriorityQueueImpl impl;
    PriorityQueueNode *min;
} PriorityQueue;

static inline void PriorityQueue_init(PriorityQueue *queue) {
    PriorityQueueImpl_initialize(&queue->impl, 8);
    queue->min = NULL;
}

static inline bool PriorityQueue_isEmpty(const PriorityQueue *queue) {
    return PriorityQueueImpl_isEmpty(&queue->impl);
}

static inline void PriorityQueue_insert(PriorityQueue *queue, PriorityQueueNode *x) {
    PriorityQueueImpl_insert(&queue->impl, &x->n, false);
    if (queue->min == NULL || x->key < queue->min->key) {
        queue->min = x;
    }
}

static inline void PriorityQueue_insertFront(PriorityQueue *queue, PriorityQueueNode *x) {
    PriorityQueueImpl_insert(&queue->impl, &x->n, true);
    if (queue->min == NULL || x->key <= queue->min->key) {
        queue->min = x;
    }
}

static inline PriorityQueueNode *PriorityQueue_peek(PriorityQueue *queue) {
    assert(!PriorityQueue_isEmpty(queue));
    return queue->min;
}

static inline PriorityQueueNode *PriorityQueue_poll(PriorityQueue *queue) {
    PriorityQueueNode *result = queue->min;
    PriorityQueueImpl_remove(&queue->impl, &result->n);
    queue->min = !PriorityQueueImpl_isEmpty(&queue->impl) ? (PriorityQueueNode *) PriorityQueueImpl_findMin(&queue->impl) : NULL;
    return result;
}

static inline void PriorityQueue_remove(PriorityQueue *queue, PriorityQueueNode *x) {
    PriorityQueueImpl_remove(&queue->impl, &x->n);
    if (x == queue->min) {
        queue->min = !PriorityQueueImpl_isEmpty(&queue->impl) ? (PriorityQueueNode *) PriorityQueueImpl_findMin(&queue->impl) : NULL;
    }
}

static inline PriorityQueueNode *PriorityQueue_pollAndInsert(PriorityQueue *queue, PriorityQueueNode *newNode, bool front) {
    PriorityQueueNode *result = queue->min;
    PriorityQueueImpl_remove(&queue->impl, &result->n);
    PriorityQueueImpl_insert(&queue->impl, &newNode->n, front);
    queue->min = !PriorityQueueImpl_isEmpty(&queue->impl) ? (PriorityQueueNode *) PriorityQueueImpl_findMin(&queue->impl) : NULL;
    return result;
}


/******************************************************************************
 * Doubly linked list
 ******************************************************************************/

struct ListNode {
    ListNode *prev;
    ListNode *next;
};

typedef struct List {
    ListNode *first;
    ListNode *last;
} List;


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


/******************************************************************************
 * Video and log output
 ******************************************************************************/

/** Virtual function table for objects to which characters can be appended. */
typedef struct AppendableVtbl {
    /** Appends a char and returns the number of chars actually appended. */
    int (*appendChar)(void *appendable, char c);
    /** Appends a nul-terminated string and returns the number of chars actually appended. */
    int (*appendCStr)(void *appendable, const char *data);
    /** Appends an array of chars and returns the number of chars actually appended. */
    int (*appendCharArray)(void *appendable, const char *data, size_t size);
} AppendableVtbl;

/** Fat pointer to an object supporting AppendableVtbl operations. */
typedef struct Appendable {
    const AppendableVtbl *vtbl;
    void *obj;
} Appendable;

int  Formatter_vprintf(Appendable *appendable, const char *fmt, va_list args);
int  Formatter_printf(Appendable *appendable, const char *fmt, ...);
void Video_initialize();
int  Video_printf(const char *format, ...);
int  Video_vprintf(const char *fmt, va_list args);
#if LOG == LOG_NONE
static inline void Log_initialize() { }
static inline int Log_printf(const char *format, ...) { return 0; }
static inline int Log_vprintf(const char *fmt, va_list args) { return 0; }
#else
void Log_initialize();
int  Log_printf(const char *format, ...);
int  Log_vprintf(const char *fmt, va_list args);
#endif


/******************************************************************************
 * Multiboot specification version 1, only used during boot.
 ******************************************************************************/

/** Multiboot module. */
typedef struct MultibootModule {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} MultibootModule;

/** Multiboot memory map. */
typedef struct MultibootMemoryMap {
    uint32_t size; // not including 'size' itself
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
} MultibootMemoryMap;

/** Multiboot Information structure. */
typedef struct MultibootMbi {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr; // physical address of an array of Module structures
    union {
        struct AoutSymbolTable {
            uint32_t tabsize;
            uint32_t strsize;
            uint32_t addr;
            uint32_t reserved;
        } aout;

        struct ElfSectionHeaderTable {
            uint32_t num;
            uint32_t size;
            uint32_t addr;
            uint32_t shndx;
        } elf;
    } syms;
    uint32_t mmap_length;
    uint32_t mmap_addr; // physical address of an array of MemoryMap structures
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
} MultibootMbi;


/******************************************************************************
 * Multi-Processor specification, only used during boot.
 ******************************************************************************/

typedef struct MpFloatingPointer {
    uint32_t signature; // "_MP_" = 0x5F504D5F
    uint32_t mpConfigPhysicalAddress; // unused if defaultConfig > 0
    uint8_t mpFloatingPointerSize; // in 16-byte units, must be 1
    uint8_t version; // 1: 1.1, 4: 1.4
    uint8_t checksum; // the sum of all bytes in this structure must be 0
    uint8_t defaultConfig; // 0: no default config, >0: one of the standard defaults
    uint8_t imcrp; // bit 7: PIC mode implemented, bit 6-0: reserved
    uint8_t reserved[3]; // must be 0
} MpFloatingPointer;

typedef struct MpConfigHeader {
    uint32_t signature; // "PCMP"
    uint16_t baseSize; // size in bytes of the base configuration table
    uint8_t version; // 1: 1.1, 4: 1.4
    uint8_t checksum; // the sum of all bytes in the base structure must be 0
    char oemId[8]; // space padded
    char productId[12]; // space padded
    uint32_t oemTablePhysicalAddress; // may be 0
    uint16_t oemTableSize; // may be 0
    uint16_t entryCount; // number of entries in the base configuration table
    uint32_t lapicPhysicalAddress;
    uint16_t extendedSize; // size in bytes of the extended configuration table
    uint8_t extendedChecksum;
    uint8_t reserved;
} MpConfigHeader;

enum MpConfigEntryType {
    mpConfigProcessor = 0,
    mpConfigBus = 1,
    mpConfigIoapic = 2,
    mpConfigIoint = 3,
    mpConfigLocalint = 4
};

struct MpConfigEntry {
    uint8_t entryType; // one of MpConfigEntryType
};

typedef struct MpConfigProcessor { // extends MpConfigEntry
    uint8_t entryType; // mpConfigProcessor
    uint8_t lapicId;
    uint8_t lapicVersion;
    uint8_t flags; // bit 0: CPU enabled, bit 1: is boot processor
    uint32_t cpuSignature;
    uint32_t featureFlags;
    uint32_t reserved[2];
} MpConfigProcessor;

typedef struct MpConfigBus { // extends MpConfigEntry
    uint8_t entryType; // mpConfigBus
    uint8_t busId;
    char busType[6];
} MpConfigBus;

typedef struct MpConfigIoapic { // extends MpConfigEntry
    uint8_t entryType; // mpConfigIoapic
    uint8_t ioapicId;
    uint8_t ioapicVersion;
    uint8_t flags; // bit 0: IOAPIC enabled
    uint32_t ioapicPhysicalAddress;
} MpConfigIoapic;


/******************************************************************************
 * ACPI
 ******************************************************************************/

typedef struct AcpiDescriptionHeader {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemId[6];
    char oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
} __attribute__((packed)) AcpiDescriptionHeader;

typedef struct AcpiAddress {
    uint8_t addressSpaceId; // 0=system memory, 1=system I/O
    uint8_t registerBitWidth;
    uint8_t registerBitOffset;
    uint8_t reserved;
    uint64_t address;
} __attribute__((packed)) AcpiAddress;

typedef struct AcpiFadt {
    AcpiDescriptionHeader header; // signature='FACP'
    uint32_t firmwareCtrlPhysicalAddress;
    uint32_t dsdtPhysicalAddress;
    uint8_t reserved1;
    uint8_t preferredPmProfile;
    uint16_t sciInt;
    uint32_t smiCmd;
    uint8_t acpiEnable;
    uint8_t acpiDisable;
    uint8_t s4biosReq;
    uint8_t pstateCnt;
    uint32_t pm1aEvtBlk;
    uint32_t pm1bEvtBlk;
    uint32_t pm1aCntBlk;
    uint32_t pm1bCntBlk;
    uint32_t pm2CntBlk;
    uint32_t pmTmrBlk; // The 32-bit physical address of the Power Management Timer
    uint32_t gpe0Blk;
    uint32_t gpe1Blk;
    uint8_t pm1EvtLen;
    uint8_t pm1CntLen;
    uint8_t pm2CntLen;
    uint8_t pmTmrLen; // Must be 4
    uint8_t gpe0BlkLen;
    uint8_t gpe1BlkLen;
    uint8_t gpe1Base;
    uint8_t cstCnt;
    uint16_t pLvl2Lat;
    uint16_t pLvl3Lat;
    uint16_t flushSize;
    uint16_t flushStride;
    uint8_t dutyOffset;
    uint8_t dutyWidth;
    uint8_t dayAlrm;
    uint8_t monAlrm;
    uint8_t century;
    uint16_t iapcBootArch;
    uint8_t reserved2;
    uint32_t flags;
    AcpiAddress resetReg;
    uint8_t resetValue;
    uint8_t reserved3[3];
    // Extended fields omitted
} __attribute__((packed)) AcpiFadt;

extern AcpiFadt Acpi_fadt;

typedef struct AcpiPmTimer {
    uint64_t timerMax; // 0x01000000 if timer is 24-bit, 0x100000000 if timer is 32-bit
    uint64_t timerMaxNanoseconds; //
    uint64_t cumulativeNanoseconds;
    uint32_t prevCount;
} AcpiPmTimer;

__attribute__((section(".boot"))) void Acpi_findConfig();

/** Reads the raw value of the ACPI Power Management timer. */
static inline uint32_t Acpi_readPmTimer() {
    return inpd(Acpi_fadt.pmTmrBlk);
}

void AcpiPmTimer_initialize();
unsigned AcpiPmTimer_busyWait(unsigned ticks);


/******************************************************************************
 * Spinlock
 ******************************************************************************/

/**
 * %Spinlock synchronization primitive.
 *
 * A spinlock is a low level mutual exclusion primitive.
 * Instead of making the caller sleep, it runs in a tight loop until the lock
 * can be acquired. This is well suited on SMP to protect <em>very small</em>
 * sections of code. This implementation uses an algorithm that ensures that
 * callers are granted the spinlock in FIFO order.
 *
 * This spinlock design allows FIFO behavior (also called "ticket lock").
 * Every time a caller tries to acquire the lock, it increments lock1.
 * When the lock is released, lock2 is incremented.
 * The lock can be acquired only if lock1 == lock2.
 */
typedef struct Spinlock {
    #if SPINLOCK_INSTRUMENTED
    volatile uint64_t contendCount;
    #endif
    volatile uint16_t lock1;
    volatile uint16_t lock2;
} Spinlock;

/** Initializes a new unlocked spinlock. */
static inline void Spinlock_init(Spinlock *sl) {
    #if SPINLOCK_INSTRUMENTED
    sl->contendCount = 0;
    #endif
    sl->lock1 = 0;
    sl->lock2 = 0;
}

/** Tries to acquire the spinlock without waiting and returns true if successful. */
static inline bool Spinlock_tryLock(Spinlock *sl) {
    uint8_t res;
    asm volatile(
            "	mov $1, %%ax\n"
            "	lock; xadd %%ax, %0\n"
            "	cmp %%ax, %1\n"
            "	sete %2\n"
            "	je 2f\n"
            "	lock; incw %1\n"
            "2:\n"
            : "+m" (sl->lock1), "+m" (sl->lock2), "=a" (res) : : "memory");
    return res;
}

/** Runs in a tight loop until the lock is acquired. */
static inline void Spinlock_lock(Spinlock *sl) {
    #if SPINLOCK_INSTRUMENTED
    uint32_t contention = 0;
    #endif
    asm volatile(
            "	mov $1, %%ax\n"
            "	lock; xadd %%ax, %0\n"
            "1:\n"
            "	cmp %%ax, %1\n"
            "	je 2f\n"
            #if SPINLOCK_INSTRUMENTED
            "   lock; incl %2\n"
            #endif
            "	pause\n"
            "	jmp 1b\n"
            "2:\n"
            : "+m" (sl->lock1), "+m" (sl->lock2)
            #if SPINLOCK_INSTRUMENTED
              , "+m" (contention)
            #endif
            : : "eax", "memory");
    #if SPINLOCK_INSTRUMENTED
    sl->contendCount += contention;
    #endif
}

/** Releases the lock. */
static inline void Spinlock_unlock(Spinlock *sl) {
    asm volatile(
            "	lock; incw %0\n"
            : "+m" (sl->lock2) : : "memory");
}


/******************************************************************************
 * Physical memory
 ******************************************************************************/

/** Types of independently managed physical memory regions. */
typedef enum PhysicalMemoryRegionType {
    /** Memory with low addresses that can be used for ISA DMA. */
    isadmaMemoryRegion,
    /** Memory that is 1:1 permanently mapped in the higher-half virtual address
     * space, with an offset, for data that the kernel needs to access directly. */
    permamapMemoryRegion,
    /** All other memory, for user mode data. */
    otherMemoryRegion,
    /** Number of supported physical memory regions. */
    physicalMemoryRegionCount
} PhysicalMemoryRegionType;

/** Descriptor of a physical memory frame. */
struct Frame {
    /**
     * Frame metadata.
     * For free frame chunks, number of frames in the chunk, flags in the lowest bits.
     * For mapped frames, the virtual address the frame is mapped into the owning task, if any.
     */
    size_t  metadata;
    /** Task owning this frame. */
    Task   *task;
    /** Previous frame in a doubly linked list. */
    Frame  *prev;
    /** Next frame in a doubly linked list. */
    Frame  *next;
};

extern Frame *PhysicalMemory_frameDescriptors;
extern size_t PhysicalMemory_totalMemoryFrames;

/** Rounds the specified physical address down to a frame number. */
static inline uintptr_t floorToFrame(uintptr_t phys) {
    return phys >> PAGE_SHIFT;
}

/** Rounds the specified physical address up to a frame number. */
static inline uintptr_t ceilToFrame(uintptr_t phys) {
    return (phys + PAGE_SIZE - 1) >> PAGE_SHIFT;
}

/** Returns the virtual address of the specified permanently mapped physical address. */
static inline void *phys2virt(uintptr_t phys) {
    assert(phys < PERMAMAP_MEMORY_REGION_FRAME_END << PAGE_SHIFT);
    return (void *) (phys + HIGH_HALF_BEGIN);
}

/** Returns the physical address of the specified permanently mapped virtual address. */
static inline uintptr_t virt2phys(const void *virt) {
    assert((uintptr_t) virt >= HIGH_HALF_BEGIN);
    return (uintptr_t) virt - HIGH_HALF_BEGIN;
}

/** Returns the virtual address of the specified permanently mapped frame number. */
static inline void *frame2virt(uintptr_t frame) {
    assert(frame < PERMAMAP_MEMORY_REGION_FRAME_END);
    return (void *) ((frame << PAGE_SHIFT) + HIGH_HALF_BEGIN);
}

/** Returns the frame number of the specified permanently mapped virtual address. */
static inline uintptr_t virt2frame(const void *virt) {
    assert((uintptr_t) virt >= HIGH_HALF_BEGIN);
    return ((uintptr_t) virt - HIGH_HALF_BEGIN) >> PAGE_SHIFT;
}

__attribute__((section(".boot"))) void PhysicalMemory_initializeFromMultibootV1(
        const MultibootMbi *mbi, uintptr_t imageBeginPhysicalAddress, uintptr_t imageEndPhysicalAddress);
void      PhysicalMemory_addBlock    (uintptr_t begin, uintptr_t end);
size_t    PhysicalMemory_allocBlock  (uintptr_t begin, uintptr_t end);
uintptr_t PhysicalMemory_alloc       (Task *task, PhysicalMemoryRegionType preferredRegion);
void      PhysicalMemory_free        (uintptr_t f);
#if LOG != LOG_NONE
void      PhysicalMemory_dumpFreeList();
#endif


/******************************************************************************
 * Slab allocator
 ******************************************************************************/

typedef struct SlabAllocator_FreeItem SlabAllocator_FreeItem;

struct SlabAllocator_FreeItem {
    SlabAllocator_FreeItem *next;
};

typedef struct SlabAllocator {
    SlabAllocator_FreeItem *freeItems;
    size_t itemSize;
    size_t itemsPerSlab;
    uint8_t *brk;
    uint8_t *limit;
    Task    *task;
} SlabAllocator;

void  SlabAllocator_initialize(SlabAllocator *sa, size_t itemSize, Task *task);
void *SlabAllocator_allocate  (SlabAllocator *sa);
void  SlabAllocator_deallocate(SlabAllocator *sa, void *item);

extern SlabAllocator taskAllocator;
extern SlabAllocator threadAllocator;
extern SlabAllocator channelAllocator;


/******************************************************************************
 * IPC
 ******************************************************************************/

enum SyscallNumber {
    syscallSendSyncRequest,
    syscallSendSyncNotification,
    syscallSendAsyncRequest,
    syscallSendAsyncNotification,
    syscallReceive,
    syscallReply,
    syscallReplyReceive,
    syscallYield
};

enum {
    Channel_dataWords = 16
};

typedef struct Channel {
    PriorityQueueNode node;
    Task *ownerTask;
    Word destinationEndpoint;
    Word responseEndpoint;
    Word badge;
    Word data[Channel_dataWords];
    Word padding[6];
} Channel;

/**
 * Dummy union to check the size of the ChannelStruct struct.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union ChannelChecker {
    char wrongSize[sizeof(Channel) == 32 * sizeof(Word)];
}; 

/** Returns the channel object embedding the specified PriorityQueueNode as node member. */
static inline Channel *Channel_fromQueueNode(PriorityQueueNode *n) {
    return (Channel *) ((uint8_t *) n - offsetof(Channel, node));
}

/** Returns true if the priority inheritance flag has been set in the specified destination endpoint word. */
static inline bool isPriorityInheritanceEnabled(Word endpointWord) {
    return endpointWord & (1 << 0);
}

/** Returns true if the non-blocking flag has been set in the specified destination endpoint word. */
static inline bool isNonBlockingEnabled(Word endpointWord) {
    return endpointWord & (1 << 1);
}

/** Returns true if the non-blocking flag has been set in the specified destination endpoint word. */
static inline Word getEndpointRef(Word endpointWord) {
    return endpointWord & ~0xF;
}

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

/**
 * Dummy union to check that offsets and size of the ThreadRegisters struct are consistent with constants used in assembly.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union ThreadRegistersChecker {
    char wrongEsOffset[offsetof(ThreadRegisters, es) == THREADREGISTERS_ES_OFFSET];
    char wrongEdiOffset[offsetof(ThreadRegisters, edi) == THREADREGISTERS_EDI_OFFSET];
    char wrongVectorOffset[offsetof(ThreadRegisters, vector) == THREADREGISTERS_VECTOR_OFFSET];
}; 

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
    uint8_t *stack; // for kernel-mode threads
    ThreadRegisters *regs; // for user-mode threads points to regsBuf, for kernel-mode threads points to bottom of the stack
    ThreadRegisters regsBuf; // for user-mode threads
    Endpoint endpoint;
    Channel channel;
    uint8_t padding[12]; // sizeof(Thread) must be a multiple of 16 bytes
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
 * TSC
 ******************************************************************************/

/** Kernel state of the Timestamp Counter of a CPU. */
typedef struct Tsc {
    uint32_t nsPerTick; // <<20 on 32-bit
    uint32_t ticksPerNs; // <<23 on 32-bit
    uint32_t usPerTick; // <<32 on 32-bit
} Tsc;

/** Converts the specified count of ticks of the TSC to nanoseconds. */
static inline uint64_t Tsc_convertTicksToNanoseconds(const Tsc *tsc, uint32_t ticks) {
    return mul(ticks, tsc->nsPerTick) >> 20;
}

__attribute__((section(".boot"))) void Tsc_initialize(Tsc *tsc);


/******************************************************************************
 * Local APIC Timer structure (see below for functions)
 ******************************************************************************/

/** Kernel state of the Local APIC timer of a CPU. */
typedef struct LapicTimer {
    uint64_t currentNanoseconds;
    uint64_t nextExpirationNanoseconds;
    uint32_t lastInitialCount;
    uint32_t maxTicks;
    uint32_t nsPerTick; // <<20 on 32-bit
    uint32_t ticksPerNs; // <<23 on 32-bit
} LapicTimer;


/******************************************************************************
 * CPU
 ******************************************************************************/

/** An entry of a page table (any level). */
typedef uint32_t PageTableEntry;

/** Flags of a page table entry (any level). */
enum PageTableFlags {
    ptPresent = 1 << 0,
    ptWriteable = 1 << 1,
    ptUser = 1 << 2,
    ptWriteThrough = 1 << 3,
    ptCacheDisable = 1 << 4,
    ptAccessed = 1 << 5,
    ptDirty = 1 << 6,
    ptLargePage = 1 << 7,
    ptPat = 1 << 7,
    ptGlobal = 1 << 8,
    ptIgnored = 7 << 9
};

/** Segment or gate descriptor of an x86 CPU. */
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

/** Offsets for Local APIC registers. */
enum Lapic {
    lapicEoi = 0xB0,
    lapicSpuriousInterrupt = 0xF0,
    lapicInterruptCommandLow = 0x300, // Interrupt Command Register, low word
    lapicInterruptCommandHigh = 0x310, // Interrupt Command Register, high word (IPI destination)
    lapicTimerLvt = 0x320,
    lapicPerformanceCounterLvt = 0x340,
    lapicLint0Lvt = 0x350,
    lapicLint1Lvt = 0x360,
    lapicTimerInitialCount = 0x380,
    lapicTimerCurrentCount = 0x390,
    lapicTimerDivider = 0x3E0
};

/** CPU Model Specific Registers. */
enum Msr {
    msrSysenterCs = 0x174,
    msrSysenterEsp = 0x175,
    msrSysenterEip = 0x176
};

/** Hardwired interrupt vectors. */
enum IrqVector {
    lapicTimerVector = 0xFC,
    tlbShootdownIpiVector = 0xFD,
    rescheduleIpiVector = 0xFE,
    spuriousInterruptVector = 0xFF
};

/**
 * Kernel state of a logical processor.
 * The nextThread member can be set by another CPU, whereas all other members
 * are CPU-local.
 */
struct Cpu {
    uint32_t      lapicId;
    bool          active;
    bool          rescheduleNeeded;
    bool          timesliceTimerEnabled;
    uint8_t       padding0;
    Cpu          *thisCpu;
    CpuNode      *cpuNode;
    Thread       *currentThread;
    Thread       *nextThread; // thread to switch on fast reschedule, equal to currentThread at rest
    uint64_t      interruptTsc;
    uint64_t      interruptCount;
    uint64_t      lastScheduleTime; // used to compute time elapsed by the current thread
    uint64_t      scheduleArrival; // value of CpuNode.scheduleOrder when this CPU was scheduled
    CpuDescriptor gdt[gdtEntryCount]; // 56 bytes
    Tss           tss; // 104 bytes
    AtomicWord    initialized; // true when boot is completed
    LapicTimer    lapicTimer; // 32 bytes
    Tsc           tsc; // 24 bytes
    PriorityQueue readyQueue; // per-CPU ready threads, 24 bytes
    Thread        idleThread; // offset 268, 320 bytes
    uint8_t       idleThreadStack[CPU_IDLE_THREAD_STACK_SIZE];
    uint8_t       stack[CPU_STACK_SIZE];
};

/**
 * Dummy union to check that offsets and size of the Cpu struct are consistent with constants used in assembly.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union CpuChecker {
    char wrongLapicIdOffset[offsetof(Cpu, lapicId) == CPU_LAPICID_OFFSET];
    char wrongThisCpuOffset[offsetof(Cpu, thisCpu) == CPU_THISCPU_OFFSET];
    char wrongCurrentThreadOffset[offsetof(Cpu, currentThread) == CPU_CURRENTTHREAD_OFFSET];
    char wrongIdleThreadStackOffset[offsetof(Cpu, idleThreadStack) == CPU_IDLE_THREAD_STACK_OFFSET];
    char wrongStackOffset[offsetof(Cpu, stack) == CPU_STACK_OFFSET];
    char wrongCpuSize[sizeof(Cpu) == CPU_STRUCT_SIZE];
}; 

/** Kernel state of a set of logical processors sharing scheduling decisions. */
struct CpuNode {
    Cpu          *cpus;
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

extern CpuNode   CpuNode_theInstance;
extern uint32_t  Cpu_cpuCount;
extern Cpu      *Cpu_cpus;
extern unsigned  Cpu_timesliceLengths[NICE_LEVELS];
extern uint8_t   Boot_kernelPageDirectoryPhysicalAddress;

__attribute__((section(".boot"))) void Cpu_loadCpuTables(Cpu *cpu);
__attribute__((section(".boot"))) void Cpu_setupIdt();
__attribute__((section(".boot"))) Cpu *Cpu_initializeCpuStructs(const MpConfigHeader *mpConfigHeader);
__attribute__((section(".boot"))) void Cpu_startOtherCpus();
__attribute__((fastcall, noreturn)) int Cpu_idleThreadFunction(void *); // from Cpu_asm.S
__attribute__((fastcall)) void Cpu_unhandledException(Cpu *cpu, void *param);
void Cpu_returnToUserMode(uintptr_t stackPointer); // from Cpu_asm.S
void Cpu_setIsr(size_t vector, Isr isr, void *param);
void Cpu_sendRescheduleInterrupt(Cpu *cpu);
void Cpu_sendTlbShootdownIpi(Cpu *cpu);
void Cpu_switchToThread(Cpu *cpu, Thread *next);
void Cpu_requestReschedule(Cpu *cpu);
bool Cpu_accountTimesliceAndCheckExpiration(Cpu *cpu);
Thread *Cpu_findNextThreadAndUpdateReadyQueue(Cpu *cpu, bool timesliced);
void Cpu_setTimesliceTimer(Cpu *cpu);
void Cpu_schedule(Cpu *cpu);
void Cpu_exitKernel(Cpu *cpu);

Cpu *CpuNode_findTargetCpu(const CpuNode *node, Cpu *lastCpu);
void CpuNode_addRunnableThread(CpuNode *node, Thread *thread);

void Pic8259_initialize(uint8_t masterVector, uint8_t slaveVector);


/******************************************************************************
 * Address space
 ******************************************************************************/

typedef struct AddressSpace {
    PageTableEntry root;
    size_t         tlbShootdownPageCount;
    uintptr_t      tlbShootdownPages[TASK_MAX_TLB_SHOOTDOWN_PAGES];
    Frame         *shootdownFrames;
} AddressSpace;

int  AddressSpace_initialize(Task *task);
int  AddressSpace_map(Task *task, uintptr_t virtualAddress, uintptr_t frame);
int  AddressSpace_mapCopy(Task *destTask, uintptr_t destVirt, Task *srcTask, uintptr_t srcVirt);
int  AddressSpace_mapFromNewFrame(Task *task, uintptr_t virtualAddress, PhysicalMemoryRegionType preferredRegion);
void AddressSpace_unmap(Task *task, uintptr_t virtualAddress, uintptr_t payload);

#include "hardware.h"


/******************************************************************************
 * Local APIC Timer functions
 ******************************************************************************/

/** Converts the specified count of ticks of the Local APIC timer to nanoseconds. */
static inline uint32_t LapicTimer_convertTicksToNanoseconds(const LapicTimer *lt, uint32_t ticks) {
    return mul(ticks, lt->nsPerTick) >> 20;
}
/** Converts the specified number of nanoseconds to count of ticks of the Local APIC timer. */
static inline uint32_t LapicTimer_convertNanosecondsToTicks(const LapicTimer *lt, uint32_t ns) {
    return mul(ns, lt->ticksPerNs) >> 23;
}

/** Returns the current count of the Local APIC timer. */
static inline uint32_t LapicTimer_getCurrentCount(LapicTimer *lt) {
    return Cpu_readLocalApic(lapicTimerCurrentCount);
}

__attribute__((section(".boot"))) void LapicTimer_initialize(LapicTimer *lt);


/******************************************************************************
 * Task
 ******************************************************************************/

struct Task {
    Task          *ownerTask;
    AddressSpace   addressSpace;
    SlabAllocator  capabilitySpace;
    size_t         threadCount;
    uint8_t        padding[4]; // sizeof(Task) must be a multiple of 16 bytes
};

/**
 * Dummy union to check the size of the Task struct.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union TaskChecker {
    char wrongSize[(sizeof(Task) & 0xF) == 0];
}; 

Task       *Task_create(Task *ownerTask, uintptr_t *cap);
Capability *Task_allocateCapability(Task *task, uintptr_t obj, uintptr_t badge);
Capability *Task_lookupCapability(Task *task, uintptr_t address);
uintptr_t   Task_getCapabilityAddress(const Capability *cap);
void        Task_deallocateCapability(Task *task, Capability *cap);


/******************************************************************************
 * ELF loader
 ******************************************************************************/

typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;

/** ELF section header, also used to compute memory requirements of kernel and modules. */
typedef struct Elf32_Shdr {
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off  sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
} Elf32_Shdr;

/** Types of ELF sections. */
typedef enum Elf32_Shdr_Type {
    SHT_NULL,
    SHT_PROGBITS,
    SHT_SYMTAB,
    SHT_STRTAB,
    SHT_RELA,
    SHT_HASH,
    SHT_DYNAMIC,
    SHT_NOTE,
    SHT_NOBITS,
    SHT_REL,
    SHT_SHLIB,
    SHT_DYNSYM,
    SHT_INIT_ARRAY = 14,
    SHT_FINI_ARRAY,
    SHT_PREINIT_ARRAY,
    SHT_GROUP,
    SHT_SYMTAB_SHNDX
} Elf32_Shdr_Type;

void ElfLoader_fromExeMultibootModule(Task *task, uintptr_t begin, uint32_t end, const char *name);


/******************************************************************************
 * System calls
 ******************************************************************************/

int Syscall_allocateIpcBuffer(Task *task, uintptr_t virtualAddress);
int Syscall_createChannel(Task *task);
int Syscall_deleteCapability(Task *task, uintptr_t index);
int Syscall_sendMessage(Cpu *cpu, uintptr_t socketCapIndex, uintptr_t endpointCapIndex);
int Syscall_receiveMessage(Thread *thread, uintptr_t endpointCapIndex, uint8_t *buffer, size_t size);
int Syscall_readMessage(Thread *thread, uintptr_t messageCapIndex, size_t offset, uint8_t *buffer, size_t size);

#endif
