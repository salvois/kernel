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

#if LOG == LOG_RS232LOG

#define RS232LOG_PORT_NUMBER  0x3F8
#define RS232LOG_BAUD_DIVISOR 1 // baud rate = 115200 / baud divisor

static void Rs232Log_put(char c) {
    while ((inp(RS232LOG_PORT_NUMBER + 5) & 0x20) == 0) { }
    outp(RS232LOG_PORT_NUMBER, c);
}

static int Rs232Log_appendChar(void *appendable, char c) {
    Rs232Log_put(c);
    return 1;
}

static int Rs232Log_appendCStr(void *appendable, const char *data) {
    size_t size = 0;
    while (*data) {
        Rs232Log_put(*data++);
        size++;
    }
    return size;
}

static int Rs232Log_appendCharArray(void *appendable, const char *data, size_t size) {
    for (size_t i = 0; i < size; i++) Rs232Log_put(*data++);
    return size;
}

static AppendableVtbl Rs232Log_appendableVtbl = {
    .appendChar = Rs232Log_appendChar,
    .appendCStr = Rs232Log_appendCStr,
    .appendCharArray = Rs232Log_appendCharArray
};

static Appendable Rs232Log_appendable = {
    .vtbl = &Rs232Log_appendableVtbl,
    .obj = NULL
};

static Spinlock Rs232Log_spinlock;

/** Initializes the serial port. Courtesy of http://wiki.osdev.org/Serial_Ports */
void Log_initialize() {
    Spinlock_init(&Rs232Log_spinlock);
    outp(RS232LOG_PORT_NUMBER + 1, 0x00); // Disable all interrupts
    outp(RS232LOG_PORT_NUMBER + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outp(RS232LOG_PORT_NUMBER + 0, (uint8_t) RS232LOG_BAUD_DIVISOR);
    outp(RS232LOG_PORT_NUMBER + 1, (uint8_t) (RS232LOG_BAUD_DIVISOR >> 8));
    outp(RS232LOG_PORT_NUMBER + 3, 0x03); // 8 bits, no parity, one stop bit
    outp(RS232LOG_PORT_NUMBER + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outp(RS232LOG_PORT_NUMBER + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

int Log_vprintf(const char *fmt, va_list args) {
    Spinlock_lock(&Rs232Log_spinlock);
    int res = Formatter_vprintf(&Rs232Log_appendable, fmt, args);
    Spinlock_unlock(&Rs232Log_spinlock);
    return res;
}

int Log_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = Log_vprintf(fmt, args);
    va_end(args);
    return res;
}

#endif // #if LOG == LOG_RS232LOG
