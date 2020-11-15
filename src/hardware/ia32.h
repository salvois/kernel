#ifndef HARDWARE_IA32_H_INCLUDED
#define HARDWARE_IA32_H_INCLUDED

#include "kernel_asm.h"

/** Halts the current processor until the next interrupt. */
static inline void Cpu_halt() {
    asm volatile("hlt" : : : "memory");
}

/** Spin loop hint for modern processors, no-operation on older processors. */
static inline void Cpu_relax() {
    asm volatile("pause" : : : "memory");
}

/** Enables interrupts on the current processor. */
static inline void Cpu_enableInterrupts() {
    asm volatile("sti" : : : "memory");
}

/** Disables interrupts on the current processor. */
static inline void Cpu_disableInterrupts() {
    asm volatile("cli" : : : "memory");
}

/** Invokes the CPUID instruction. */
static inline void Cpu_cpuid(uint32_t level, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    asm volatile("cpuid" : "=a" (*a), "=b" (*b), "=c" (*c), "=d" (*d) : "0" (level));
}

/** Reads the specified Local APIC register of the current CPU. */
static inline uint32_t Cpu_readLocalApic(size_t offset) {
    return *(volatile uint32_t *) (CPU_LAPIC_VIRTUAL_ADDRESS + offset);
}
/** Writes to the specified Local APIC register of the current CPU. */
static inline void Cpu_writeLocalApic(size_t offset, uint32_t value) {
    *(volatile uint32_t *) (CPU_LAPIC_VIRTUAL_ADDRESS + offset) = value;
}

/** Reads the specified Model Specific Register of the current CPU. */
static inline uint64_t Cpu_readMsr(uint32_t msr) {
    uint32_t a, d;
    asm volatile("rdmsr" : "=a" (a), "=d" (d) : "c" (msr));
    return (uint64_t) (a) | ((uint64_t) (d) << 32);
}

/** Writes to the specified Model Specific Register of the current CPU. */
static inline void Cpu_writeMsr(uint32_t msr, uint64_t value) {
    asm volatile("wrmsr" : : "a" ((uint32_t) value), "d" ((uint32_t) (value >> 32)), "c" (msr));
}

static inline uint32_t Cpu_readFs() {
    uint32_t v = 0;
    asm volatile("mov %%fs, %0" : "=r" (v));
    return v;
}

static inline void Cpu_writeFs(uint32_t value) {
    asm volatile("mov %0, %%fs" : : "r" (value));
}

static inline uint32_t Cpu_readGs() {
    uint32_t v;
    asm volatile("mov %%gs, %0" : "=r" (v));
    return v;
}

static inline void Cpu_writeGs(uint32_t value) {
    asm volatile("mov %0, %%gs" : : "r" (value));
}

static inline void Cpu_loadLdt(uint32_t value) {
    asm volatile("lldt %0" : : "r" ((uint16_t) value));
}

/** Gets the address that caused a page fault. */
static inline void *Cpu_getFaultingAddress() {
    void *addr;
    asm volatile("mov %%cr2, %0" : "=r" (addr) : : "memory");
    return addr;
}

/** Gets the current CPU structure masking the per-CPU kernel stack pointer. */
static inline Cpu *Cpu_getCurrent() {
    #if WORD_SIZE == 32
    uint32_t sp;
    asm volatile("mov %%esp, %0" : "=r" (sp) : : "memory");
    #elif WORD_SIZE == 64
    uint64_t sp;
    asm volatile("mov %%rsp, %0" : "=r" (sp) : : "memory");
    #else
    #error Unsupported word size
    #endif
    return (Cpu *) (sp & ~(CPU_STRUCT_SIZE - 1));
}

/** Switches the current processor to the specified address space. */
static inline void AddressSpace_activate(AddressSpace *as) {
    Log_printf("Switching to address space with root %p.\n", as->root);
    asm volatile("mov %0, %%cr3" : : "r" (as->root) : "memory");
}

/** Invalidates all non-global TLB entries. */
static inline void AddressSpace_invalidateTlb() {
    asm volatile(
    "    mov %%cr3, %%eax\n"
    "    mov %%eax, %%cr3\n"
    : : : "eax", "memory");
}

/** Invalidates a single TLB entry. */
static inline void AddressSpace_invalidateTlbAddress(uintptr_t a) {
    asm volatile("invlpg %0" : : "m" ((void *) a) : "memory");
}

#endif
