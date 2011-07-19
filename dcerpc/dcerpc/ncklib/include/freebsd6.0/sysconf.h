/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME:
**
**      freebsd6.0/sysconf.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  This file contains all definitions specific to the FreeBSD 6.x platform
**
**
*/

#ifndef _SYSCONF_H
#define _SYSCONF_H	1

/******************************************************************************/

#define DCETHREAD_USE_THROW
#define DCETHREAD_CHECKED
#include <dce/dcethread.h>
#include <dce/dce.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>

/*
*
* IMPORTANT ORDER DEPENDENCY:
*
*       The <cmaxxx.h> include files above do #ifdefs on
*       _POSIX_REENTRANT_FUNCTIONS.  Under OSF/1, this
*       is set in <unistd.h>.  The problem is that for
*       the OSF/1 DCE reference port, we need to build
*       with vanilla CMA threads and no help from the
*       kernel.
*
*       Therefore, the include of <unistd.h> must come
*       after the <cmaxxx.h> files above.
*
*       You have been warned.
*
*/

#include <unistd.h>

#include <assert.h>
#include <fcntl.h>
#include <string.h>

/*#define NO_VOID_STAR	1*/	/* void * is supported in GCC -ansi mode*/

#define USE_PROTOTYPES  1

#define STDARG_PRINTF   1

#define NO_VARARGS_PRINTF 1

#define BSD_4_4_SOCKET 1

/* if SOCKADDR_LEN isn't defined MSG_MAXIOVLEN will not be defined in
 * <sys/socket.h>
 */

#ifndef MSG_MAXIOVLEN
#define MSG_MAXIOVLEN	1024
#endif /* MSG_MAXIOVLEN */

/**************************************************************************/

/*
 * This definition means the ioctl() call to get the interface
 * addresses in ipnaf_bsd.c in enumerate_interfaces() will not be
 * called. This is because Ultrix returns the same internet address
 * for all interfaces.
 */

//#define NO_SIOCGIFADDR 1

/*
 * Define protocol sequences that are always available on
 * OSF/1 platforms
 */

#ifndef PROT_NCACN
#define PROT_NCACN	1
#endif

#ifndef PROT_NCADG
#define PROT_NCADG	1
#endif

#ifndef NAF_IP
#define NAF_IP	1
#endif

#define RPC_DEFAULT_NLSPATH "/usr/lib/nls/msg/en_US.ISO8859-1/%s.cat"

#define RPC_C_PATH_NP_MAX	104

/****************************************************************************/

/* define some macros to support atfork handler */

#define ATFORK_SUPPORTED

#define ATFORK(handler) rpc__atfork(handler)

extern void rpc__cma_atfork (void *);

/****************************************************************************/

#endif /* _SYSCONF_H */
