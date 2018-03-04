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

/**
 * Formatted output to the video memory.
 * 
 * This singleton class manages output to the screen writing to the text-mode
 * video memory and handles positioning of the hardware cursor.
 */
typedef struct Video {
    uint16_t *videoMemory;
    unsigned ofs;
    unsigned x;
    unsigned y;
    Spinlock spinlock;
} Video;

#define VIDEO_WIDTH  80
#define VIDEO_HEIGHT 25
static Video Video_theInstance;

static void Video_getCursorPos() {
    outp(0x3D4, 0x0F); // cursor position LSB
    Video_theInstance.ofs = inp(0x3D5);
    outp(0x3D4, 0x0E); // cursor position MSB
    Video_theInstance.ofs |= (unsigned) inp(0x3D5) << 8;
    Video_theInstance.y = Video_theInstance.ofs / VIDEO_WIDTH;
    Video_theInstance.x = Video_theInstance.ofs % VIDEO_WIDTH;
}

static void Video_doSetCursorPos() {
    outp(0x3D4, 0x0F); // cursor position LSB
    outp(0x3D5, (uint8_t) Video_theInstance.ofs);
    outp(0x3D4, 0x0E); // cursor position MSB
    outp(0x3D5, (uint8_t) (Video_theInstance.ofs >> 8));
}

static void Video_setCursorPos(unsigned x, unsigned y) {
    Video_theInstance.x = x;
    Video_theInstance.y = y;
    Video_theInstance.ofs = y * VIDEO_WIDTH + x;
    Video_doSetCursorPos();
}

static void Video_scrollUp() {
    unsigned i;
    for (i = 0; i < VIDEO_WIDTH * (VIDEO_WIDTH - 1); i++) {
        Video_theInstance.videoMemory[i] = Video_theInstance.videoMemory[i + VIDEO_WIDTH];
    }
    for (; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++) {
        Video_theInstance.videoMemory[i] = 0x0720;
    }
    Video_setCursorPos(0, VIDEO_HEIGHT - 1);
}

static void Video_put(char c) {
    if (c == '\n' || c == '\r') {
        Video_theInstance.x = 0;
        Video_theInstance.y++;
        Video_theInstance.ofs = Video_theInstance.y * VIDEO_WIDTH;
    } else {
        Video_theInstance.videoMemory[Video_theInstance.ofs] = 0x0700 | (uint8_t) c;
        if (++Video_theInstance.x == VIDEO_WIDTH) {
            Video_theInstance.x = 0;
            Video_theInstance.y++;
        }
        Video_theInstance.ofs++;
    }
    if (Video_theInstance.ofs == VIDEO_WIDTH * VIDEO_HEIGHT) Video_scrollUp();
    else Video_doSetCursorPos();
}

static int Video_appendChar(void *appendable, char c) {
    Video_put(c);
    return 1;
}

static int Video_appendCStr(void *appendable, const char *data) {
    size_t size = 0;
    while (*data) {
        Video_put(*data++);
        size++;
    }
    return size;
}

static int Video_appendCharArray(void *appendable, const char *data, size_t size) {
    for (size_t i = 0; i < size; ++i) Video_put(*data++);
    return size;
}

static AppendableVtbl Video_appendableVtbl = {
    .appendChar = Video_appendChar,
    .appendCStr = Video_appendCStr,
    .appendCharArray = Video_appendCharArray
};

static Appendable Video_appendable = {
    .vtbl = &Video_appendableVtbl,
    .obj = &Video_theInstance
};

void Video_initialize() {
    Spinlock_init(&Video_theInstance.spinlock);
    Video_theInstance.videoMemory = (uint16_t *)(0xC00B8000);
    Video_getCursorPos();
}

void Video_clear() {
    Spinlock_lock(&Video_theInstance.spinlock);
    for (unsigned i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++) Video_theInstance.videoMemory[i] = 0x0720;
    Video_theInstance.ofs = 0;
    Video_setCursorPos(0, 0);
    Spinlock_unlock(&Video_theInstance.spinlock);
}

int Video_vprintf(const char *fmt, va_list args) {
    Spinlock_lock(&Video_theInstance.spinlock);
    int res = Formatter_vprintf(&Video_appendable, fmt, args);
    #if LOG != LOG_VIDEO
    Log_vprintf(fmt, args);
    #endif
    Spinlock_unlock(&Video_theInstance.spinlock);
    return res;
}

int Video_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = Video_vprintf(fmt, args);
    va_end(args);
    return res;
}

#if LOG == LOG_VIDEO
void Log_initialize() {
}

int Log_vprintf(const char *fmt, va_list args) {
    return Video_vprintf(fmt, args);
}

int Log_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = Video_vprintf(fmt, args);
    va_end(args);
    return res;
}
#endif
