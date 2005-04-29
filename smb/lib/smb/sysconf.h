/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */

/*
**
**  NAME:
**
**      osf/1_mips.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  This file contains all definitions specific to modern BSD platforms
**
**
*/

#ifndef _SYSCONF_H
#define _SYSCONF_H	1	

/******************************************************************************/

#include <dce/pthread_exc.h>
#include <dce/dce.h>
#include <stdio.h>
#include <stdlib.h>
/*
 * Temporarily turn off _POSIX_C_SOURCE so we get all the socket-based
 * errnos - and then turn it back on again, so that we don't get hosed
 * by header A being included with _POSIX_C_SOURCE defined and thus
 * not defining something header B needs if _POSIX_C_SOURCE isn't
 * defined.
 */
#undef _POSIX_C_SOURCE
#include <errno.h>
#define _POSIX_C_SOURCE
#include <sys/file.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
/*
 * We also want "gettimeofday()".
 */
#undef _POSIX_C_SOURCE
#include <sys/time.h>
#define _POSIX_C_SOURCE

#include <unistd.h>

#include <assert.h>
#include <fcntl.h>
#include <string.h>

/*#define NO_VOID_STAR	1*/	/* void * is supported in GCC -ansi mode*/

#define STDARG_PRINTF   1

#define NO_VARARGS_PRINTF 1

/* if SOCKADDR_LEN isn't defined MSG_MAXIOVLEN will not be defined in
 * <sys/socket.h>
 */

#ifndef MSG_MAXIOVLEN
#define MSG_MAXIOVLEN	16
#endif /* MSG_MAXIOVLEN */

/**************************************************************************/

/*
 * Define protocol sequences that are always available in OS X
 */

#ifndef PROT_NCACN
#define PROT_NCACN	1
#endif

#ifndef NAF_NP
#define NAF_NP		1
#endif

#define RPC_DEFAULT_NLSPATH "/usr/share/nls/msg/en_US.ISO8859-1/%s.cat"

/****************************************************************************/

/* define some macros to support atfork handler */

#ifdef PD_BUILD
#define ATFORK_SUPPORTED

#define ATFORK(handler) rpc__cma_atfork(handler)

extern void rpc__cma_atfork(void *);
#else
#define PTHREAD_EXC
#endif

/****************************************************************************/

#ifndef PD_BUILD
/*
 * XXX - "pthread_cancel()" doesn't cancel I/O on Darwin, but if we
 * define NON_CANCELLABLE_IO, we hang in the listener loop doing the
 * test-for-cancel before the select.
 *
 * Yes, this is a bug.  Other UNIXes support cancellable I/O.
 */
#define RPC_C_DG_SOCK_MAX_PRIV_SOCKS 0
#endif

#endif /* _SYSCONF_H */
