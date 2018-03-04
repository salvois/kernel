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
OUTPUT_FORMAT("elf32-i386")
ENTRY(Boot_entryPoint)
SECTIONS {
    /DISCARD/ : {
        *(.build-id)
    }
    . = 0x4000;
    Boot_imageBeginPhysicalAddress = . + 0x100000;
    .aplow : AT(ADDR(.aplow) + 0x100000) {
        *(.aplow)
    }
    . += 0x100000;
    .bsplow : AT(ADDR(.bsplow)) {
        *(.bsplow)
    }
    . += 0xC0000000;
    .boot : AT(ADDR(.boot) - 0xC0000000) {
        *(.boot .boot*)
    }
    Boot_bootSectionEnd = ALIGN(4096);
    .text ALIGN(4096) : AT(ADDR(.text) - 0xC0000000) {
        *(.text .text*)
    }
    . = .;
    .data : AT(ADDR(.data) - 0xC0000000) {
        __CTOR_LIST__ = .;
        LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
        *(SORT(.ctors*))
        *(.ctors)
        LONG(0)
        __CTOR_END__ = .;
        __DTOR_LIST__ = .;
        LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
        *(SORT(.dtors*))
        *(.dtors)
        LONG(0)
        __DTOR_END__ = .;
        *(.data)
        *(.rodata*)
    }
    .bss : AT(ADDR(.bss) - 0xC0000000) {
        Boot_bssBegin = .;
        *(COMMON*)
        *(.bss*)
        Boot_bssEnd = ALIGN(4);
    }
    Boot_bssWordSize = SIZEOF(.bss) / 4;
    Boot_imageEndPhysicalAddress = . - 0xC0000000;
    Boot_kernelPageDirectoryPhysicalAddress = Boot_kernelPageDirectory - 0xC0000000;
}
