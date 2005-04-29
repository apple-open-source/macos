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
#ifndef _COMSOC_H
#define _COMSOC_H	1
/*
**
**  NAME:
**
**      comsoc.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  The internal network "socket" object interface.  A very thin veneer
**  over the BSD socket abstraction interfaces.  This makes life a little
**  easier when porting to different environments.
**  
**  All operations return a standard error value of type
**  rpc_socket_error_t, operate on socket handles of type rpc_socket_t
**  and socket addresses of type rpc_socket_addr_t.  These are the types
**  that one should use when coding.
**  
**  Note that there is a distinction between local runtime internal
**  representations of socket addresses and architected (on-the-wire)
**  representations used by location services.  This interface specifies
**  the local runtime internal representation.
**  
**  Operations that return an error value always set the value
**  appropriately.  A value other than rpc_c_socket_ok indicates failure;
**  the values of additional output parameters are undefined.  Other
**  error values are system dependent.
**
**
*/


/*
 * Include platform-specific socket definitions
 */

#include <comsoc_sys.h>


/*
 * Changing anything below will affect other portions of the runtime.
 */

typedef struct {                    /* a BSD UNIX iovec */
    byte_p_t base;
    int len;
} rpc_socket_iovec_t, *rpc_socket_iovec_p_t;

#endif /* _COMSOC_H */
