/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

#include <errno.h>
#include <sys/time.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <mach/mach_syscalls.h>
#include <mach/clock.h>
#include <mach/clock_types.h>
#include <stdio.h>

extern mach_port_t clock_port;

int
nanosleep(const struct timespec *requested_time, struct timespec *remaining_time) {
    kern_return_t ret;
    mach_timespec_t remain;
    mach_timespec_t current;
    
    if ((requested_time == NULL) || (requested_time->tv_sec < 0) || (requested_time->tv_nsec > NSEC_PER_SEC)) {
        errno = EINVAL;
        return -1;
    }

    ret = clock_get_time(clock_port, &current);
    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "clock_get_time() failed: %s\n", mach_error_string(ret));
        return -1;
    }
    /* This depends on the layout of a mach_timespec_t and timespec_t being equivalent */
    ret = clock_sleep_trap(clock_port, TIME_RELATIVE, requested_time->tv_sec, requested_time->tv_nsec, &remain);
    if (ret != KERN_SUCCESS) {
        if (ret == KERN_ABORTED) {
            errno = EINTR;
            if (remaining_time != NULL) {
                ret = clock_get_time(clock_port, &remain);
                if (ret != KERN_SUCCESS) {
                    fprintf(stderr, "clock_get_time() failed: %s\n", mach_error_string(ret));
                    return -1;
                }
                ADD_MACH_TIMESPEC(&current, requested_time);
                SUB_MACH_TIMESPEC(&current, &remain);
                remaining_time->tv_sec = current.tv_sec;
                remaining_time->tv_nsec = current.tv_nsec;
            }
        } else {
            errno = EINVAL;
        }
        return -1;
    }
    return 0;
}
