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
#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED

extern int exitCode;

#define ASSERT(condition) \
    if (!(condition)) { \
        __builtin_printf("%%TEST_FAILED%% time=0 testname=%s (%s) message=Assertion %s failed in %s in %s:%u\n", __func__, __FILE__, #condition, __func__, __FILE__, __LINE__); \
        exitCode = -1; \
    }

#define RUN_TEST(name) \
    __builtin_printf("%%TEST_STARTED%%  %s (%s)\n", #name, __FILE__); \
    name(); \
    __builtin_printf("%%TEST_FINISHED%% time=0 %s (%s)\n", #name, __FILE__);

#define RUN_SUITE(name) \
    __builtin_printf("%%SUITE_STARTING%% %s\n", #name); \
    __builtin_printf("%%SUITE_STARTED%%\n"); \
    name(); \
    __builtin_printf("%%SUITE_FINISHED%% time=0\n");

#endif
