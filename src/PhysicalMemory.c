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
#include "kernel.h"

#if 0
#define PHYSICALMEMORY_LOG_PRINTF(...) Log_printf(__VA_ARGS__)
#else
#define PHYSICALMEMORY_LOG_PRINTF(...)
#endif

#define FRAME_FREE_METADATA_AVAILABLE    (1 << 0)
#define FRAME_FREE_METADATA_REGION_MASK  (3 << 1)
#define FRAME_FREE_METADATA_REGION_SHIFT 1
#define FRAME_FREE_METADATA_SIZE_SHIFT   3
#define PHYSICALMEMORYREGION_FREE_LIST_COUNT 22

/** Linked list of unused physical memory frames. */
typedef struct PhysicalMemory_FreeList {
    Frame *front;
} PhysicalMemory_FreeList;

/**
 * A region of physical memory that is managed independently.
 * Each region has different qualities, see enum PhysicalMemoryRegionType.
 */
typedef struct PhysicalMemoryRegion {
    //The free list of order k (k=0..countFreeList-1) holds free memory chunk whose size is >=2^k and <2^(k+1)
    PhysicalMemory_FreeList freeLists[PHYSICALMEMORYREGION_FREE_LIST_COUNT];
    uintptr_t base;
    uintptr_t end;
    size_t freeSpace;
    PhysicalMemoryRegionType regionType;
} PhysicalMemoryRegion;

/** Semi-closed interval of physical addresses to delimit regions. */
typedef struct AddressInterval {
    uintptr_t begin;
    uintptr_t end;
} AddressInterval;

/** Boundaries for regions specified by enum PhysicalMemoryRegionType. */
static AddressInterval PhysicalMemory_regionBoundaries[] = {
    { .begin = 0, .end = ISADMA_MEMORY_REGION_FRAME_END },
    { .begin = ISADMA_MEMORY_REGION_FRAME_END, .end = PERMAMAP_MEMORY_REGION_FRAME_END },
    { .begin = PERMAMAP_MEMORY_REGION_FRAME_END, .end = 0xFFFFFFFF }
};

/** Table of descriptors for each frame of all installed physical memory. */
Frame *PhysicalMemory_frameDescriptors;
/** Number of frames of all installed physical memory. */
size_t PhysicalMemory_totalMemoryFrames;
/** Descriptors for each region specified by enum PhysicalMemoryRegionType. */
static PhysicalMemoryRegion PhysicalMemory_regions[physicalMemoryRegionCount];

static inline bool Frame_freeFrameIsAvailable(const Frame *f) {
    return (f->metadata & FRAME_FREE_METADATA_AVAILABLE) != 0;
}

static inline size_t Frame_freeFrameGetSize(const Frame *ci) {
    return ci->metadata >> FRAME_FREE_METADATA_SIZE_SHIFT;
}

static inline void Frame_freeFrameSetMetadata(Frame *f, size_t size, PhysicalMemoryRegionType regionType, bool isAvailable) {
    f->metadata =  (size << FRAME_FREE_METADATA_SIZE_SHIFT) | (regionType << FRAME_FREE_METADATA_REGION_SHIFT) | (isAvailable ? FRAME_FREE_METADATA_AVAILABLE : 0);
}


/********************************************************************
 * PhysicalMemoryRegion
 ********************************************************************/

/** Returns the index of the free list managing chunks of the specified size. */
static unsigned PhysicalMemoryRegion_sizeToOrder(size_t size) {
    assert(size > 0);
    unsigned k = 1;
    while (k <= PHYSICALMEMORYREGION_FREE_LIST_COUNT && (size >> k) != 0) ++k;
    return k - 1;
}

/** Removes the specified chunk from the specified free list. */
static void PhysicalMemoryRegion_removeFreeChunk(PhysicalMemory_FreeList *fl, Frame *chunk) {
    if (chunk->next) chunk->next->prev = chunk->prev;
    if (chunk->prev) chunk->prev->next = chunk->next;
    else fl->front = chunk->next;
    (chunk + Frame_freeFrameGetSize(chunk) - 1)->metadata &= ~FRAME_FREE_METADATA_AVAILABLE;
    chunk->metadata &= ~FRAME_FREE_METADATA_AVAILABLE;
}

/** Adds the chunk with the specified boundaries to the appropriate free list of the specified region. */
static void PhysicalMemoryRegion_addFreeChunk(PhysicalMemoryRegion *region, uintptr_t base, size_t size) {
    PhysicalMemory_FreeList *fl = &region->freeLists[PhysicalMemoryRegion_sizeToOrder(size)];
    Frame *chunk = &PhysicalMemory_frameDescriptors[base];
    Frame_freeFrameSetMetadata(chunk, size, region->regionType, true);
    (chunk + Frame_freeFrameGetSize(chunk) - 1)->metadata |= FRAME_FREE_METADATA_AVAILABLE;
    chunk->task = NULL;
    chunk->prev = NULL;
    chunk->next = fl->front;
    if (fl->front) fl->front->prev = chunk;
    fl->front = chunk;
}

/**
 * Releases the chunk starting at the specified frame number to the free pool
 * of the specified region.
 */
static void PhysicalMemoryRegion_free(PhysicalMemoryRegion *region, uintptr_t base) {
    Frame *chunk = &PhysicalMemory_frameDescriptors[base];
    assert(!Frame_freeFrameIsAvailable(chunk));
    size_t size = Frame_freeFrameGetSize(chunk);
    PHYSICALMEMORY_LOG_PRINTF("Region_free: [%p..%p) size=%u (%u bytes).\n", base, base + size, size, size << PAGE_SHIFT);
    // Attempt to coalesce with the previous chunk
    if (base > region->base && Frame_freeFrameIsAvailable(chunk - 1)) {
        size_t s = Frame_freeFrameGetSize(chunk - 1);
        base -= s;
        size += s;
        Frame *prevChunk = &PhysicalMemory_frameDescriptors[base];
        assert(prevChunk->metadata == chunk->metadata);
        PhysicalMemoryRegion_removeFreeChunk(&region->freeLists[PhysicalMemoryRegion_sizeToOrder(s)], prevChunk);
    }
    // Attempt to coalesce with the next chunk
    if (base + size < region->end) {
        Frame *nextChunk = &PhysicalMemory_frameDescriptors[base + size];
        size_t s = Frame_freeFrameGetSize(nextChunk);
        if (Frame_freeFrameIsAvailable(nextChunk)) {
            size += s;
            PhysicalMemoryRegion_removeFreeChunk(&region->freeLists[PhysicalMemoryRegion_sizeToOrder(s)], nextChunk);
        }
    }
    // Add the (possibly coalesced) chunk to the free list
    PhysicalMemoryRegion_addFreeChunk(region, base, size);
    region->freeSpace += size;
}

/**
 * Adds the specified block of physical memory to the appropriate region.
 * Panics if parts of the block are already added.
 * Free blocks resulting after adding a block are coalesced together.
 * @param begin First frame number to add.
 * @param end One after the last frame number to add.
 */
__attribute__((section(".boot"))) static void PhysicalMemoryRegion_addBlock(PhysicalMemoryRegion *region, uintptr_t base, size_t size) {
    assert(PhysicalMemory_totalMemoryFrames > 0);
    uintptr_t end = base + size;
    if (end <= base) return;
    PHYSICALMEMORY_LOG_PRINTF("Region_addBlock [%p..%p) size=%u (%u bytes)\n", base, end, size, size << PAGE_SHIFT);
    // The region must not overlap other free regions in the pool
    for (unsigned k = 0; k < PHYSICALMEMORYREGION_FREE_LIST_COUNT; k++) {
        for (Frame *chunk = region->freeLists[k].front; chunk != NULL; chunk = chunk->next) {
            uintptr_t chunkBase = (uintptr_t) (chunk - PhysicalMemory_frameDescriptors);
            uintptr_t chunkEnd = chunkBase + Frame_freeFrameGetSize(chunk);
            if (base <= chunkEnd && end >= chunkBase) {
                panic("Trying to add a physical memory block that overlaps free block in the region.\n"
                        "New block: [%p..%p), overlapped block: [%p..%p).\n"
                        "System halted.", base, end, chunkBase, chunkEnd);
            }
        }
    }
    // Extend the region if needed
    if (region->freeSpace == 0) {
        region->base = base;
        region->end = end;
    } else {
        if (base < region->base) region->base = base;
        if (end > region->end) region->end = end;
    }
    // Add the block to the region
    Frame *chunk = &PhysicalMemory_frameDescriptors[base];
    Frame_freeFrameSetMetadata(chunk, size, region->regionType, false);
    PhysicalMemoryRegion_free(region, base);
}

/**
 * Allocates the specified block of physical memory from the specified region.
 * @param begin First frame number to allocate.
 * @param end One after the last frame number to allocate.
 * @return True if it was possible to allocate the block, false otherwise.
 */
static bool PhysicalMemoryRegion_allocBlock(PhysicalMemoryRegion *region, uintptr_t base, size_t size) {
    uintptr_t end = base + size;
    PHYSICALMEMORY_LOG_PRINTF("Region_allocBlock [%p..%p) size=%u (%u bytes)\n", base, end, size, size << PAGE_SHIFT);
    if (size <= region->freeSpace) {
        for (unsigned k = PhysicalMemoryRegion_sizeToOrder(size); k < PHYSICALMEMORYREGION_FREE_LIST_COUNT; k++) {
            for (Frame *chunk = region->freeLists[k].front; chunk != NULL; chunk = chunk->next) {
                uintptr_t chunkBase = (uintptr_t) (chunk - PhysicalMemory_frameDescriptors);
                uintptr_t chunkEnd = chunkBase + Frame_freeFrameGetSize(chunk);
                PHYSICALMEMORY_LOG_PRINTF("- Chunk [%p..%p) size=%u (%u bytes) - ", chunkBase, chunkEnd, Frame_freeFrameGetSize(chunk), Frame_freeFrameGetSize(chunk) << PAGE_SHIFT);
                if (base >= chunkBase && end <= chunkEnd) {
                    PhysicalMemoryRegion_removeFreeChunk(&region->freeLists[k], chunk);
                    size_t remainder = base - chunkBase;
                    if (remainder) PhysicalMemoryRegion_addFreeChunk(region, chunkBase, remainder);
                    remainder = chunkEnd - end;
                    if (remainder) PhysicalMemoryRegion_addFreeChunk(region, end, chunkEnd - end);
                    region->freeSpace -= size;
                    PHYSICALMEMORY_LOG_PRINTF("Found\n");
                    return true;
                } else {
                    PHYSICALMEMORY_LOG_PRINTF("Not found\n");
                }
            }
        }
    }
    return false;
}

/**
 * Allocates the specified count of frames from the specified region.
 * @return The allocated frame number, or 0 on out of memory.
 */
uintptr_t PhysicalMemoryRegion_alloc(PhysicalMemoryRegion *region, size_t size) {
    if (size <= region->freeSpace) {
        for (unsigned k = PhysicalMemoryRegion_sizeToOrder(size); k < PHYSICALMEMORYREGION_FREE_LIST_COUNT; k++) {
            if (region->freeLists[k].front != NULL) {
                Frame *chunk = region->freeLists[k].front;
                assert(size <= Frame_freeFrameGetSize(chunk));
                uintptr_t base = (uintptr_t) (chunk - PhysicalMemory_frameDescriptors);
                size_t remainder = Frame_freeFrameGetSize(chunk) - size;
                PhysicalMemoryRegion_removeFreeChunk(&region->freeLists[k], chunk);
                if (remainder) PhysicalMemoryRegion_addFreeChunk(region, base + size, remainder);
                PHYSICALMEMORY_LOG_PRINTF("Region_alloc: [%p..%p) size=%u (%u bytes).\n", base, base + size, size, size << PAGE_SHIFT);
                region->freeSpace -= size;
                return base;
            }
        }
    }
    return 0;
}

#if LOG != LOG_NONE
/** Prints the free lists of the specified region for debugging purposes. */
void PhysicalMemoryRegion_dumpFreeList(const PhysicalMemoryRegion *region) {
    PHYSICALMEMORY_LOG_PRINTF("Dumping free memory list\n");
    for (unsigned k = 0; k < PHYSICALMEMORYREGION_FREE_LIST_COUNT; k++) {
        for (Frame *chunk = region->freeLists[k].front; chunk != NULL; chunk = chunk->next) {
            uintptr_t base = (uintptr_t) (chunk - PhysicalMemory_frameDescriptors);
            PHYSICALMEMORY_LOG_PRINTF("Order %d - Chunk [%p..%p) size=%u (%u bytes)\n", k, base, base + Frame_freeFrameGetSize(chunk), Frame_freeFrameGetSize(chunk), Frame_freeFrameGetSize(chunk) << PAGE_SHIFT);
        }
    }
}
#endif

/******************************************************************************
 * PhysicalMemory
 ******************************************************************************/

/**
 * Begins initialization of the physical memory manager.
 *
 * Sets the total amount of memory in bytes, and places the frame descriptor
 * table (for book keeping) at firstFreeFrame (possibly rounded up to be page
 * aligned).
 *
 * The initialization process is as follows:
 * - call initBegin to set up book keeping;
 * - repeatedly call addRegion to add available memory regions as specified
 *   by the bootloader or firmware;
 * - call initEnd to finalize the initialization, marking the book keeping
 *   area as allocated;
 * - repeatedly call allocRegion to mark memory occupied by the kernel
 *   and boot modules as allocated.
 */
__attribute__((section(".boot"))) void PhysicalMemory_initBegin(size_t totalMemoryFrames, uintptr_t firstFreeFrame) {
    PhysicalMemory_totalMemoryFrames = totalMemoryFrames;
    uintptr_t end = firstFreeFrame + ceilToFrame(totalMemoryFrames * sizeof(Frame));
    if (firstFreeFrame < 0x100 || end >= PERMAMAP_MEMORY_REGION_FRAME_END || end >= totalMemoryFrames) {
        panic("Unable to store the frame descriptor table. Aborting.\n");
    }
    PhysicalMemory_frameDescriptors = frame2virt(firstFreeFrame);
    Log_printf("Frame descriptor table at frame number %p (%u bytes) for %u frames of memory.\n",
            firstFreeFrame, totalMemoryFrames * sizeof(Frame), PhysicalMemory_totalMemoryFrames);
    memzero(PhysicalMemory_frameDescriptors, totalMemoryFrames * sizeof(Frame));
}

/**
 * Ends initialization of the physical memory manager.
 *
 * Marks the memory occupied by the frame descriptor table (for book keeping)
 * as allocated. After this call the physical memory manager is set up and all
 * physical memory, with the exception of book keeping, is marked as free.
 */
__attribute__((section(".boot"))) void PhysicalMemory_initEnd() {
    uintptr_t b = virt2frame(PhysicalMemory_frameDescriptors);
    size_t s = ceilToFrame(PhysicalMemory_totalMemoryFrames * sizeof(Frame));
    size_t a = PhysicalMemory_allocBlock(b, b + s);
    if (a != s) {
        panic("Unable to mark memory for the frame descriptor table as allocated. Aborting.\n");
    }
}

/**
 * Initializes the physical memory manager from information returned
 * by a boot loader compliant with the Multiboot specification version 1.
 * @param mbi Higher-half virtual address of the Multiboot Information structure.
 * @param imageBeginPhysicalAddress Physical address where the kernel image begins in memory (inclusive).
 * @param imageEndPhysicalAddress Physical address where the kernel image ends in memory (exclusive).
 */
__attribute__((section(".boot"))) void PhysicalMemory_initializeFromMultibootV1(
        const MultibootMbi *mbi, uintptr_t imageBeginPhysicalAddress, uintptr_t imageEndPhysicalAddress) {
    Log_printf("Initializing the physical memory manager from the MBIv1 at %p.\n", mbi);
    uintptr_t imageBeginFrame = floorToFrame(imageBeginPhysicalAddress);
    // Find the highest occupied memory address to place the chunk info table.
    // While traversing the MBI structure, also get the total amount of RAM.
    uintptr_t occupiedPhysicalMemoryEnd = imageEndPhysicalAddress;
    uint64_t totalMemoryBytes = 0;
    // Skip physical memory occupied by kernel symbols, if present
    if (mbi->flags & (1 << 5)) { // ELF sections provided
        const uint8_t *a = phys2virt(mbi->syms.elf.addr);
        Log_printf("ELF sections loaded at %p, num=%u, size=%u, shndx=%u\n", mbi->syms.elf.addr, mbi->syms.elf.num, mbi->syms.elf.size, mbi->syms.elf.shndx);
        for (size_t i = 0; i < mbi->syms.elf.num; i++, a += mbi->syms.elf.size) {
            const Elf32_Shdr *sh = (const Elf32_Shdr *) a;
            Log_printf("  i=%u flags=%08X [%p..%p)\n", i, sh->sh_flags, sh->sh_addr, sh->sh_addr + sh->sh_size);
            uintptr_t addr = sh->sh_addr;
            if (addr >= HIGH_HALF_BEGIN) addr -= HIGH_HALF_BEGIN;
            if (/*(sh->sh_flags & 2) &&*/ (addr + sh->sh_size > occupiedPhysicalMemoryEnd)) {
                occupiedPhysicalMemoryEnd = addr + sh->sh_size;
            }
        }
    }
    uintptr_t kernelEndFrame = ceilToFrame(occupiedPhysicalMemoryEnd);
    Log_printf("imageBeginFrame=%p kernel image end=%p kernelEndFrame=%p\n",
            imageBeginFrame, imageEndPhysicalAddress, kernelEndFrame);
    // Skip physical memory occupied by boot modules, if present
    if (mbi->flags & (1 << 3)) { // Boot modules provided
        const MultibootModule *m = phys2virt(mbi->mods_addr);
        for (size_t i = 0; i < mbi->mods_count; i++, m++) {
            Log_printf("  Multiboot module %d at [%p-%p) string=\"%s\".\n", i, m->mod_start, m->mod_end, phys2virt(m->string));
            if (m->mod_end > occupiedPhysicalMemoryEnd) occupiedPhysicalMemoryEnd = m->mod_end;
        }
    }
    // Find total memory
    if (mbi->flags & (1 << 6)) { // Memory map provided
        uintptr_t mmapEnd = mbi->mmap_addr + mbi->mmap_length;
        uintptr_t a = mbi->mmap_addr;
        while (a < mmapEnd) {
            const MultibootMemoryMap *m = phys2virt(a);
            Log_printf("  mmap:, base=0x %08X %08X, size=0x %08X %08X, type=%i.\n",
                    m->base_addr_high, m->base_addr_low, m->length_high, m->length_low, m->type);
            if (m->type == 1) {
                uint64_t addr = ((uint64_t) m->base_addr_high << 32) | (uint64_t) m->base_addr_low;
                uint64_t length = ((uint64_t) m->length_high << 32) | (uint64_t) m->length_low;
                uint64_t end = addr + length;
                if (end > totalMemoryBytes) totalMemoryBytes = end;
            }
            a += m->size + sizeof(m->size);
        }
    } else if (mbi->flags & (1 << 0)) { // Memory map not provided, fallback on mem_lower and mem_upper
        uint64_t end = (uint64_t) mbi->mem_lower << 10;
        if (end > totalMemoryBytes) totalMemoryBytes = end;
        end = 0x100000 + ((uint64_t) mbi->mem_upper << 10);
        if (end > totalMemoryBytes) totalMemoryBytes = end;
    }
    if (totalMemoryBytes > MAX_PHYSICAL_ADDRESS) {
        //Video::get().printf("Warning: physical memory beyond 4 GiB will not be used.\n");
        totalMemoryBytes = MAX_PHYSICAL_ADDRESS;
    }
    if (totalMemoryBytes == 0) {
        panic("Memory information not available.\nSystem halted.\n");
    }
    // Initialize physical memory
    PhysicalMemory_initBegin(floorToFrame(totalMemoryBytes), ceilToFrame(occupiedPhysicalMemoryEnd));
    // Find total memory
    if (mbi->flags & (1 << 6)) { // Memory map provided
        uintptr_t mmapEnd = mbi->mmap_addr + mbi->mmap_length;
        uintptr_t a = mbi->mmap_addr;
        while (a < mmapEnd) {
            const MultibootMemoryMap *m = phys2virt(a);
            if (m->type == 1) {
                uint64_t addr = ((uint64_t) m->base_addr_high << 32) | (uint64_t) m->base_addr_low;
                uint64_t length = ((uint64_t) m->length_high << 32) | (uint64_t) m->length_low;
                if (addr < MAX_PHYSICAL_ADDRESS) {
                    if (addr + length > MAX_PHYSICAL_ADDRESS) length = MAX_PHYSICAL_ADDRESS - addr;
                    PhysicalMemory_addBlock(floorToFrame(addr), ceilToFrame(addr + length));
                }
            }
            a += m->size + sizeof(m->size);
        }
    } else if (mbi->flags & (1 << 0)) { // Memory map not provided, fallback on mem_lower and mem_upper
        PhysicalMemory_addBlock(0, ceilToFrame((uintptr_t) mbi->mem_lower << 10));
        uint64_t length = mbi->mem_upper << 10;
        if (0x100000 + length >= MAX_PHYSICAL_ADDRESS) length = MAX_PHYSICAL_ADDRESS - 0x100000;
        PhysicalMemory_addBlock(floorToFrame(0x100000), ceilToFrame(0x100000 + length));
    }
    PhysicalMemory_initEnd();
    // Mark kernel and module memory as allocated
    PhysicalMemory_allocBlock(0, 1);
    PhysicalMemory_allocBlock(imageBeginFrame, kernelEndFrame);
    if (mbi->flags & (1 << 3)) { // Boot modules provided
        const MultibootModule *m = (const MultibootModule *) mbi->mods_addr;
        for (size_t i = 0; i < mbi->mods_count; i++, m++) {
            PhysicalMemory_allocBlock(floorToFrame(m->mod_start), ceilToFrame(m->mod_end));
        }
    }
    #if LOG != LOG_NONE
    PhysicalMemory_dumpFreeList();
    #endif
}

/**
 * Adds the specified block of physical memory to the appropriate regions as part of their initialization.
 * Panics if parts of the block are already added.
 * Free blocks resulting after adding a block are coalesced together.
 * @param begin First frame number to add.
 * @param end One after the last frame number to add.
 */
__attribute__((section(".boot"))) void PhysicalMemory_addBlock(uintptr_t begin, uintptr_t end) {
    for (int i = 0; i < physicalMemoryRegionCount; i++) {
        uintptr_t b = (begin >= PhysicalMemory_regionBoundaries[i].begin) ? begin : PhysicalMemory_regionBoundaries[i].begin;
        uintptr_t e = (end   <= PhysicalMemory_regionBoundaries[i].end)   ? end   : PhysicalMemory_regionBoundaries[i].end;
        if (b < e) {
            PhysicalMemoryRegion_addBlock(&PhysicalMemory_regions[i], b, e - b);
        }
        if (e == end) break;
    }
}

/**
 * Allocates the specified block of physical memory from the appropriate regions.
 * @param begin First frame number to allocate.
 * @param end One after the last frame number to allocate.
 * @return The number of frames actually allocated, that may be less than end-begin on out of memory or if parts of the block are already allocated.
 */
size_t PhysicalMemory_allocBlock(uintptr_t begin, uintptr_t end) {
    size_t a = 0;
    for (int i = 0; i < physicalMemoryRegionCount; i++) {
        uintptr_t b = (begin >= PhysicalMemory_regionBoundaries[i].begin) ? begin : PhysicalMemory_regionBoundaries[i].begin;
        uintptr_t e = (end   <= PhysicalMemory_regionBoundaries[i].end)   ? end   : PhysicalMemory_regionBoundaries[i].end;
        if (b < e) {
            bool r = PhysicalMemoryRegion_allocBlock(&PhysicalMemory_regions[i], b, e - b);
            if (!r) break;
            a += e - b;
        }
        if (e == end) break;
    }
    return a;
}

/**
 * Deallocates the specified block of physical memory from the appropriate regions.
 * Undefined behavior if parts of the block are already free.
 * @param begin First frame number to deallocate.
 * @param end One after the last frame number to deallocate.
 */
void PhysicalMemory_freeBlock(uintptr_t begin, uintptr_t end) {
    for (int i = 0; i < physicalMemoryRegionCount; i++) {
        uintptr_t b = (begin >= PhysicalMemory_regionBoundaries[i].begin) ? begin : PhysicalMemory_regionBoundaries[i].begin;
        uintptr_t e = (end   <= PhysicalMemory_regionBoundaries[i].end)   ? end   : PhysicalMemory_regionBoundaries[i].end;
        if (b < e) {
            PhysicalMemoryRegion_free(&PhysicalMemory_regions[i], b); // , e - b
        }
        if (e == end) break;
    }
}

/**
 * Allocates one frame trying the specified preferred region first.
 * @param task Task to associate the frame to (may be NULL).
 * @param preferredRegion Region type to try to allocate from.
 * @return The allocated frame number, or 0 on out of memory.
 */
uintptr_t PhysicalMemory_alloc(Task *task, PhysicalMemoryRegionType preferredRegion) {
    assert(preferredRegion < physicalMemoryRegionCount);
    for (int i = preferredRegion; i >= 0; i--) {
        uintptr_t f = PhysicalMemoryRegion_alloc(&PhysicalMemory_regions[i], 1);
        if (f != 0) {
            PHYSICALMEMORY_LOG_PRINTF("PhysicalMemory_alloc(%p) allocated at frame %p.\n", task, f);
            PhysicalMemory_frameDescriptors[f].task = task;
            return f;
        }
    }
    return 0;
}

/**
 * Deallocates one frame allocated by PhysicalMemory_alloc to the proper region.
 * @param f The frame number to free.
 */
void PhysicalMemory_free(uintptr_t f) {
    Frame *fd = &PhysicalMemory_frameDescriptors[f];
    fd->task = NULL;
    PhysicalMemoryRegion *r = &PhysicalMemory_regions[(fd->metadata & FRAME_FREE_METADATA_REGION_MASK) >> FRAME_FREE_METADATA_REGION_SHIFT];
    PhysicalMemoryRegion_free(r, f);
}

#if LOG != LOG_NONE
/** Prints the free lists of all regions for debugging purposes. */
void PhysicalMemory_dumpFreeList() {
    for (int i = 0; i < physicalMemoryRegionCount; i++) {
        PhysicalMemoryRegion_dumpFreeList(&PhysicalMemory_regions[i]);
    }
}
#endif
