CFLAGS = -Wall -O3 -m32 -std=gnu99 -pedantic-errors -nostdinc -nostartfiles -nostdlib -fno-builtin -fno-asynchronous-unwind-tables -Iinclude
ASMSOURCES = src/Boot_asm.S src/Cpu_asm.S
CSOURCES = src/Boot.c \
  src/Acpi.c \
  src/AddressSpace.c \
  src/Cpu.c \
  src/CpuNode.c \
  src/ElfLoader.c \
  src/Formatter.c \
  src/LapicTimer.c \
  src/PhysicalMemory.c \
  src/Pic8259.c \
  src/PortE9Log.c \
  src/PriorityQueue.c \
  src/Rs232Log.c \
  src/SlabAllocator.c \
  src/Syscall.c \
  src/Task.c \
  src/Thread.c \
  src/Tsc.c \
  src/Util.c \
  src/Video.c
OBJECTS = $(patsubst src/%.S, build/%.o, $(ASMSOURCES)) $(patsubst src/%.c, build/%.o, $(CSOURCES))
TESTS_CFLAGS = -m32 -nostdlib -fno-asynchronous-unwind-tables -no-pie -fno-pie -s -Iinclude
TESTS = build/EndlessLoop build/Sysenter

.PHONY: all clean Release cleanRelease

all: build/kernel.bin

Debug: CFLAGS += -DLOG=LOG_RS232LOG
Release: CFLAGS += -DLOG=LOG_NONE -DNDEBUG

Release: all
Debug: all

cleanRelease: clean

clean:
	rm -f build/*

build/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.S include/kernel_asm.h
	$(CC) $(CFLAGS) -c $< -o $@

build/%: test/%.c
	$(CC) $(TESTS_CFLAGS) $< -o $@

build/Sysenter: test/Sysenter.c
	$(CC) $(TESTS_CFLAGS) -O3 $< -o $@

build/kernel.bin: $(OBJECTS) $(TESTS)
	$(LD) -m elf_i386 -T link.ld $(OBJECTS) -o $@
	objdump -D $@ > build/kernel.S
	objdump -M intel -D $@ > build/kernel.asm
	#strip $@
	#sudo mount ~/VirtualBox\ VMs/kerneltest/kerneltest-flat.vmdk /mnt -o loop,offset=1048576 -o umask=000
	#cp $@ /mnt
	#cp $(TESTS) /mnt
	#sudo umount /mnt
