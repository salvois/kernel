#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/src/Acpi.o \
	${OBJECTDIR}/src/AddressSpace.o \
	${OBJECTDIR}/src/Boot.o \
	${OBJECTDIR}/src/Boot_asm.o \
	${OBJECTDIR}/src/Cpu.o \
	${OBJECTDIR}/src/CpuNode.o \
	${OBJECTDIR}/src/Cpu_asm.o \
	${OBJECTDIR}/src/ElfLoader.o \
	${OBJECTDIR}/src/Formatter.o \
	${OBJECTDIR}/src/LapicTimer.o \
	${OBJECTDIR}/src/PhysicalMemory.o \
	${OBJECTDIR}/src/Pic8259.o \
	${OBJECTDIR}/src/PortE9Log.o \
	${OBJECTDIR}/src/PriorityQueue.o \
	${OBJECTDIR}/src/Rs232Log.o \
	${OBJECTDIR}/src/SlabAllocator.o \
	${OBJECTDIR}/src/Syscall.o \
	${OBJECTDIR}/src/Task.o \
	${OBJECTDIR}/src/Thread.o \
	${OBJECTDIR}/src/Tsc.o \
	${OBJECTDIR}/src/Util.o \
	${OBJECTDIR}/src/Video.o


# C Compiler Flags
CFLAGS=-m32 -Wall -std=gnu99

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/kernel

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/kernel: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/kernel ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/src/Acpi.o: src/Acpi.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Acpi.o src/Acpi.c

${OBJECTDIR}/src/AddressSpace.o: src/AddressSpace.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/AddressSpace.o src/AddressSpace.c

${OBJECTDIR}/src/Boot.o: src/Boot.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Boot.o src/Boot.c

${OBJECTDIR}/src/Boot_asm.o: src/Boot_asm.S
	${MKDIR} -p ${OBJECTDIR}/src
	$(AS) $(ASFLAGS) -g -o ${OBJECTDIR}/src/Boot_asm.o src/Boot_asm.S

${OBJECTDIR}/src/Cpu.o: src/Cpu.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Cpu.o src/Cpu.c

${OBJECTDIR}/src/CpuNode.o: src/CpuNode.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/CpuNode.o src/CpuNode.c

${OBJECTDIR}/src/Cpu_asm.o: src/Cpu_asm.S
	${MKDIR} -p ${OBJECTDIR}/src
	$(AS) $(ASFLAGS) -g -o ${OBJECTDIR}/src/Cpu_asm.o src/Cpu_asm.S

${OBJECTDIR}/src/ElfLoader.o: src/ElfLoader.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/ElfLoader.o src/ElfLoader.c

${OBJECTDIR}/src/Formatter.o: src/Formatter.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Formatter.o src/Formatter.c

${OBJECTDIR}/src/LapicTimer.o: src/LapicTimer.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/LapicTimer.o src/LapicTimer.c

${OBJECTDIR}/src/PhysicalMemory.o: src/PhysicalMemory.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/PhysicalMemory.o src/PhysicalMemory.c

${OBJECTDIR}/src/Pic8259.o: src/Pic8259.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Pic8259.o src/Pic8259.c

${OBJECTDIR}/src/PortE9Log.o: src/PortE9Log.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/PortE9Log.o src/PortE9Log.c

${OBJECTDIR}/src/PriorityQueue.o: src/PriorityQueue.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/PriorityQueue.o src/PriorityQueue.c

${OBJECTDIR}/src/Rs232Log.o: src/Rs232Log.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Rs232Log.o src/Rs232Log.c

${OBJECTDIR}/src/SlabAllocator.o: src/SlabAllocator.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/SlabAllocator.o src/SlabAllocator.c

${OBJECTDIR}/src/Syscall.o: src/Syscall.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Syscall.o src/Syscall.c

${OBJECTDIR}/src/Task.o: src/Task.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Task.o src/Task.c

${OBJECTDIR}/src/Thread.o: src/Thread.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Thread.o src/Thread.c

${OBJECTDIR}/src/Tsc.o: src/Tsc.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Tsc.o src/Tsc.c

${OBJECTDIR}/src/Util.o: src/Util.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Util.o src/Util.c

${OBJECTDIR}/src/Video.o: src/Video.c
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -Iinclude -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Video.o src/Video.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
