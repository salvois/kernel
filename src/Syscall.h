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
#ifndef SYSCALL_H_INCLUDED
#define SYSCALL_H_INCLUDED

#include "Types.h"

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

int Syscall_allocateIpcBuffer(Task *task, uintptr_t virtualAddress);
int Syscall_createChannel(Task *task);
int Syscall_deleteCapability(Task *task, PhysicalAddress index);
int Syscall_sendMessage(Cpu *cpu, uintptr_t socketCapIndex, uintptr_t endpointCapIndex);
int Syscall_receiveMessage(Thread *thread, uintptr_t endpointCapIndex, uint8_t *buffer, size_t size);
int Syscall_readMessage(Thread *thread, uintptr_t messageCapIndex, size_t offset, uint8_t *buffer, size_t size);

#endif
