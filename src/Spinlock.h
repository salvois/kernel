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
#ifndef SPINLOCK_H_INCLUDED
#define SPINLOCK_H_INCLUDED

#include "Types.h"

/*
 * Spinlock synchronization primitive.
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

#endif
