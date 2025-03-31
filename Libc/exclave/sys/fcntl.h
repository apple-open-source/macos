/*
 * Copyright (c) 2000-2022 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*-
 * Copyright (c) 1983, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fcntl.h	8.3 (Berkeley) 1/21/94
 */


#ifndef _SYS_FCNTL_H_
#define _SYS_FCNTL_H_

#include <sys/cdefs.h>
#include <_bounds.h>
#include <_compat/sys/_types.h>

_LIBC_SINGLE_BY_DEFAULT()

/*
 * File status flags: these are used by open(2), fcntl(2).
 * They are also used (indirectly) in the kernel file structure f_flags,
 * which is a superset of the open/fcntl flags.  Open flags and f_flags
 * are inter-convertible using OFLAGS(fflags) and FFLAGS(oflags).
 * Open/fcntl flags begin with O_; kernel-internal flags begin with F.
 */
/* open-only flags */
#define O_RDONLY        0x0000          /* open for reading only */
#define O_WRONLY        0x0001          /* open for writing only */
#define O_RDWR          0x0002          /* open for reading and writing */
#define O_ACCMODE       0x0003          /* mask for above modes */

#define O_NONBLOCK      0x00000004      /* no delay */
#define O_APPEND        0x00000008      /* set append mode */

#ifndef O_SYNC
#define O_SYNC          0x0080          /* synch I/O file integrity */
#endif /* O_SYNC */

#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
#define O_SHLOCK        0x00000010      /* open with shared file lock */
#define O_EXLOCK        0x00000020      /* open with exclusive file lock */
#define O_ASYNC         0x00000040      /* signal pgrp when data ready */
#define O_FSYNC         O_SYNC          /* source compatibility: do not use */
#define O_NOFOLLOW      0x00000100      /* don't follow symlinks */
#endif /* (_POSIX_C_SOURCE && !_DARWIN_C_SOURCE) */
#define O_CREAT         0x00000200      /* create if nonexistant */
#define O_TRUNC         0x00000400      /* truncate to zero length */
#define O_EXCL          0x00000800      /* error if already exists */

#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
#define O_EVTONLY       0x00008000      /* descriptor requested for event notifications only */
#endif


#define O_NOCTTY        0x00020000      /* don't assign controlling terminal */


#if __DARWIN_C_LEVEL >= 200809L
#define O_DIRECTORY     0x00100000
#endif

#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
#define O_SYMLINK       0x00200000      /* allow open of a symlink */
#endif

#ifndef O_DSYNC
#define O_DSYNC         0x400000        /* synch I/O data integrity */
#endif /* O_DSYNC */


#if __DARWIN_C_LEVEL >= 200809L
#define O_CLOEXEC       0x01000000      /* implicitly set FD_CLOEXEC */
#endif


#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
#define O_NOFOLLOW_ANY  0x20000000      /* no symlinks allowed in path */
#endif

#if __DARWIN_C_LEVEL >= 200809L
#define O_EXEC          0x40000000               /* open file for execute only */
#define O_SEARCH        (O_EXEC | O_DIRECTORY)   /* open directory for search only */
#endif

/*
 * Constants used for fcntl(2)
 */

/* command values */
#define F_NOCACHE       48              /* turn data caching off/on for this fd */
#define F_GETPATH       50              /* return the full path of the fd */

#if defined(ENABLE_EXCLAVE_STORAGE)

__BEGIN_DECLS

int open(const char *, int, ...);
int fcntl(int, int, ...);

__END_DECLS

#endif /* ENABLE_EXCLAVE_STORAGE */

#endif /* !_SYS_FCNTL_H_ */
