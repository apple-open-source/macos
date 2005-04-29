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
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <stdio.h>


#ifdef BUILDING_VARIANT
#include "pthread_internals.h"

extern int __unix_conforming;
extern mach_port_t clock_port;
extern semaphore_t clock_sem;

int
nanosleep(const struct timespec *requested_time, struct timespec *remaining_time) {
    kern_return_t kret;
    int ret;
    mach_timespec_t remain;
    mach_timespec_t current;
   
	if (__unix_conforming == 0)
		__unix_conforming = 1;
	 
    if ((requested_time == NULL) || (requested_time->tv_sec < 0) || (requested_time->tv_nsec >= NSEC_PER_SEC)) {
        errno = EINVAL;
        return -1;
    }

    if (remaining_time != NULL) {
        kret = clock_get_time(clock_port, &current);
        if (kret != KERN_SUCCESS) {
            fprintf(stderr, "clock_get_time() failed: %s\n", mach_error_string(ret));
            return -1;
        }
    }
    ret = __semwait_signal(clock_sem, MACH_PORT_NULL, 1, 1, requested_time->tv_sec, requested_time->tv_nsec);
    if (ret < 0) {
        if (errno == ETIMEDOUT) {
		return 0;
        } else if (errno == EINTR) {
            if (remaining_time != NULL) {
                ret = clock_get_time(clock_port, &remain);
                if (ret != KERN_SUCCESS) {
                    fprintf(stderr, "clock_get_time() failed: %s\n", mach_error_string(ret));
                    return -1;
                }
                /* This depends on the layout of a mach_timespec_t and timespec_t being equivalent */
                ADD_MACH_TIMESPEC(&current, requested_time);
                SUB_MACH_TIMESPEC(&current, &remain);
                remaining_time->tv_sec = current.tv_sec;
                remaining_time->tv_nsec = current.tv_nsec;
            }
        } else {
            errno = EINVAL;
	}
    }
    return -1;
}


#else /* BUILDING_VARIANT */

int
nanosleep(const struct timespec *requested_time, struct timespec *remaining_time) {
    kern_return_t ret;
    mach_timespec_t remain;
    mach_timespec_t current;
    uint64_t end;
    static double ratio = 0.0, rratio;
    
    if ((requested_time == NULL) || (requested_time->tv_sec < 0) || (requested_time->tv_nsec > NSEC_PER_SEC)) {
        errno = EINVAL;
        return -1;
    }

    if (ratio == 0.0) {
        struct mach_timebase_info info;
        ret = mach_timebase_info(&info);
        if (ret != KERN_SUCCESS) {
            fprintf(stderr, "mach_timebase_info() failed: %s\n", mach_error_string(ret));
            errno = EAGAIN;
            return -1;
        }
        ratio = (double)info.numer / ((double)info.denom * NSEC_PER_SEC);
        rratio = (double)info.denom / (double)info.numer;
    }

    /* use rratio to avoid division */
    end = mach_absolute_time() + (uint64_t)(((double)requested_time->tv_sec * NSEC_PER_SEC + (double)requested_time->tv_nsec) * rratio);
    ret = mach_wait_until(end);
    if (ret != KERN_SUCCESS) {
        if (ret == KERN_ABORTED) {
            errno = EINTR;
            if (remaining_time != NULL) {
                uint64_t now = mach_absolute_time();
                double delta;
                if (now > end)
                    now = end;
                delta = (end - now) * ratio;
                remaining_time->tv_sec = delta;
                remaining_time->tv_nsec = NSEC_PER_SEC * (delta - remaining_time->tv_sec);
            }
        } else {
            errno = EINVAL;
        }
        return -1;
    }
    return 0;
}


#endif /* BUILDING_VARIANT */
