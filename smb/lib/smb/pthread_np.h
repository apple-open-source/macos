/*
 * 
 * (c) Copyright 1990 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1990 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1990 DIGITAL EQUIPMENT CORPORATION
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
**
**  NAME:
**
**      pthread_np.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Non-standard pthread routines on which the NCK runtime depends.
**
*/

#ifndef _PTHREAD_NP_H_
#define _PTHREAD_NP_H_

#ifndef _POSIX_THREADS
# define _POSIX_THREADS				
#endif

extern int
pthread_get_expiration_np 
    (
	struct timespec	*delta,
	struct timespec	*abstime
    );

extern void
pthread_delay_np 
    (
	struct timespec	*interval
    );

extern void
pthread_lock_global_np 
    (
	void
    );

extern void
pthread_unlock_global_np 
    (
	void
    );

#define pthread_equal_np pthread_equal

/* --------------------------------------------------------------------------- */
/*
 * The following types are defined by CMA, and used by the runtime.
 */  

typedef void (*pthread_initroutine_t)(void);
typedef void *(*pthread_startroutine_t)(void * arg);
typedef void (*pthread_destructor_t)(void *arg);
typedef void *pthread_addr_t; 

#endif /* _PTHREAD_NP_H_ */
