/*
 * Copyright (c) 2006,2011,2013-2014 Apple Inc. All Rights Reserved.
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

/*    $NetBSD: xdr_array.c,v 1.12 2000/01/22 22:19:18 mycroft Exp $    */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid = "@(#)xdr_array.c 1.10 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)xdr_array.c    2.1 88/07/29 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>

/*
 * xdr_array.c, Generic XDR routines impelmentation.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * arrays.  See xdr.h for more info on the interface to xdr.
 */

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sec_xdr.h"

/*
 * XDR an array of arbitrary elements
 * *addrp is a pointer to the array, *sizep is the number of elements.
 * If addrp is NULL (*sizep * elsize) bytes are allocated.
 * elsize is the size (in bytes) of each element, and elproc is the
 * xdr procedure to call to handle each element of the array.
 */
bool_t
sec_xdr_array(XDR *xdrs, uint8_t **addrp, u_int *sizep, u_int maxsize, u_int elsize, xdrproc_t elproc)
{
    u_int i;
    bool_t stat = TRUE;
    
    u_int c = sizep ? *sizep : 0;  /* the actual element count */
    /* like strings, arrays are really counted arrays */
    if (!xdr_u_int(xdrs, &c))
        return (FALSE);

    if (sizep && (xdrs->x_op == XDR_DECODE))
        *sizep = c;
        
    // XXX/cs on decode if c == 0 return

    if ((c > maxsize || UINT_MAX/elsize < c) && (xdrs->x_op != XDR_FREE))
        return (FALSE);

    if (elsize > 1024) {
        // Structure suspiciously large: 1024 is arbitrary upper bound
        // for struct sizes (non-nested size)
        assert(FALSE);
        return (FALSE);
    }
    
    u_int nodesize = c * elsize;
    uint8_t *target = addrp ? *addrp : NULL;

    uint8_t obj[elsize];

    bool_t sizeof_alloc = sec_xdr_arena_size_allocator(xdrs);

    /*
     * if we are deserializing, we may need to allocate an array.
     * We also save time by checking for a null array if we are freeing.
     */
    if (target == NULL) {
        switch (xdrs->x_op) {
        case XDR_DECODE:
            if (c == 0)
                return (TRUE);
			if (!sec_mem_alloc(xdrs, nodesize, &target))
				return (FALSE);
			if (!target)
				target = &obj[0];
            if (!sizeof_alloc)
				*addrp = target;
            break;

        case XDR_FREE:
            return (TRUE);

        case XDR_ENCODE:
            break;
        }
    }
	
    /*
     * now we xdr each element of array
     */
    for (i = 0; (i < c) && stat; i++) {
        if ((xdrs->x_op == XDR_DECODE) && sizeof_alloc)
            memset(obj, 0, elsize);
        stat = (*elproc)(xdrs, target, 0);
        if ((xdrs->x_op == XDR_ENCODE) || !sizeof_alloc)
            target += elsize;
    }

    /*
     * the array may need freeing
     */
    if (xdrs->x_op == XDR_FREE) {
        sec_mem_free(xdrs, *addrp, nodesize);
        *addrp = NULL;
    }
    return (stat);
}

/**
 * This is almost a straight copy of the standard implementation, except
 * that all calls made that allocate memory can defer to an alternate 
 * mechanism, with the purpose to allocate from one block of memory on
 * *decode*
 */
