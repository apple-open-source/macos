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
/*	@(#)sigprocmask.c	1.0	9/26/95	(c) 1995 NeXT	*/

#include <sys/syscall.h>
#include <signal.h>
#include <sys/signal.h>
#include <errno.h>

/*
 *	Translate sigprocmask call to system call interface.
 */

int   sigprocmask (
        int how,
        const sigset_t *nsm,
        sigset_t *osm)
{
    sigset_t	mask;

    if (nsm == 0) {
        how = SIG_BLOCK;
        sigemptyset(&mask);
    } else {
        mask = *nsm;
    }
    mask = (sigset_t) syscall(SYS_sigprocmask, how, mask);
    /* syscall will call cerror if there is an error */
    if (osm)
        *osm = mask;
    return 0;
}
