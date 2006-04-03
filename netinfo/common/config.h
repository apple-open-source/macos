/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#define NETWORK_CHANGE_NOTIFICATION "com.apple.system.config.network_change"

#ifndef _OS_VERSION_NEXTSTEP_
#ifndef _OS_VERSION_OPENSTEP_
#ifndef _OS_VERSION_MACOS_X_SERVER_
#ifndef _OS_VERSION_MACOS_X_
#include <NetInfo/os.h>
#endif
#endif
#endif
#endif

#ifdef _OS_VERSION_NEXTSTEP_
#define _OS_NEXT_
#endif

#ifdef _OS_VERSION_OPENSTEP_
#define _OS_NEXT_
#endif

#ifdef _OS_NEXT_
#define _THREAD_TYPE_CTHREAD_
#define _IPC_TYPED_
#define _PORT_REGISTRY_NAMESERVER_
#define _NO_SOCKADDR_LENGTH_
#define _UNIX_BSD_43_
#define _WAIT_TYPE_ union wait
#define SO_REUSEPORT SO_REUSEADDR
#include <libc.h>
#define u_int64_t unsigned long long
#define int64_t long long
#define u_int32_t unsigned int
#define int32_t int
#define u_int16_t unsigned short
#define int16_t short
#define u_int8_t unsigned char
#define int8_t char
#define pid_t int
extern void xdr_free();
#ifndef _IP6ADDR_
struct in6_addr {
	union {
		u_int8_t   __u6_addr8[16];
		u_int16_t  __u6_addr16[8];
		u_int32_t  __u6_addr32[4];
	} __u6_addr;			/* 128-bit IP6 address */
};
#define _IP6ADDR_
#endif _IP6ADDR_
#define _W_INT(w) (*(int *)&(w))
#define _WSTATUS(x) (_W_INT(x) & 0177)
#define WEXITSTATUS(x) (_W_INT(x) >> 8)
#define WTERMSIG(x) (_WSTATUS(x))
#endif

#ifdef _OS_VERSION_MACOS_X_SERVER_
#define _OS_APPLE_
#define _THREAD_TYPE_CTHREAD_
#define _IPC_TYPED_
#define _PORT_REGISTRY_NAMESERVER_
#define _SHADOW_
#endif

#ifdef _OS_VERSION_MACOS_X_
#define _OS_APPLE_
#define _THREAD_TYPE_PTHREAD_
#define _IPC_UNTYPED_
#define _PORT_REGISTRY_BOOTSTRAP_
#endif

#ifdef _OS_APPLE_
#define _WAIT_TYPE_ int
#define _UNIX_BSD_44_
#endif
