/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/time.h>

extern int nanosleep(const struct timespec *, struct timespec *);

/* We use nanosleep and let it set errno, and compute the residual for us. */
unsigned int
sleep(unsigned int seconds)
{
    struct timespec req, rem;

    if (seconds == 0) {
        return 0;
    }
    req.tv_sec = seconds;
    req.tv_nsec = 0;

    /* It's not clear from the spec whether the remainder will be 0
    ** if we weren't interrupted
    */
    if (nanosleep(&req, &rem) == -1) {
        return (unsigned int)rem.tv_sec;
    }
    return 0;
}
