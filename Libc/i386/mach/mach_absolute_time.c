/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#if !defined(__ppc__)
#include <stdint.h>
#include <mach/clock.h>
#include <mach/mach_time.h>

extern mach_port_t clock_port;

inline static uint64_t
fast_get_nano_from_abs(int scale)
{
    uint64_t value;
    asm (
        "rdtsc                  \n\t"
        "movl   %%edx,%%esi     \n\t"
        "mull   %%ecx           \n\t"
        "movl   %%edx,%%edi     \n\t"
        "movl   %%esi,%%eax     \n\t"
        "mull   %%ecx           \n\t"
        "xorl   %%ecx,%%ecx     \n\t"
        "addl   %%edi,%%eax     \n\t"
        "adcl   %%ecx,%%edx         "
                : "=A"(value) : "c"(scale) : "%esi", "%edi");
        return value;
}

uint64_t
mach_absolute_time(void) {
        static int scale = 0;

        if (__builtin_expect(scale == 0, 0)) {
                mach_timebase_info_data_t info;
                mach_timebase_info(&info);
                scale = info.numer;
        }
        if (__builtin_expect(scale == 1, 0)) {
                mach_timespec_t now;
                (void)clock_get_time(clock_port, &now);
                return (uint64_t)now.tv_sec * NSEC_PER_SEC + now.tv_nsec;
	}
	return fast_get_nano_from_abs(scale);
}
#endif
