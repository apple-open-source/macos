/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

/*      $NetBSD: xdr_reference.c,v 1.13 2000/01/22 22:19:18 mycroft Exp $ */

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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint) 
static char *sccsid = "@(#)xdr_reference.c 1.11 87/08/11 SMI";
static char *sccsid = "@(#)xdr_reference.c      2.1 88/07/29 4.0 RPCSRC";
static char *rcsid = "$FreeBSD: src/lib/libc/xdr/xdr_reference.c,v 1.11 2002/03/22 21:53:26 obrien Exp $";
#endif
#include <sys/cdefs.h>
#include <assert.h>
/*
 * xdr_reference.c, Generic XDR routines impelmentation.
 *
 * Copyright (C) 1987, Sun Microsystems, Inc.
 *
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * "pointers".  See xdr.h for more info on the interface to xdr.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sec_xdr.h"

/*
 * XDR an indirect pointer
 * xdr_reference is for recursively translating a structure that is
 * referenced by a pointer inside the structure that is currently being
 * translated.  pp references a pointer to storage. If *pp is null
 * the  necessary storage is allocated.
 * size is the sizeof the referneced structure.
 * proc is the routine to handle the referenced structure.
 */
bool_t
sec_xdr_reference(XDR *xdrs, uint8_t **pp, u_int size, xdrproc_t proc)
{
    uint8_t *loc = pp ? *pp : NULL;
    bool_t stat;

    if (size > 1024) {
        // Structure suspiciously large: 1024 is arbitrary upper bound
        // for struct sizes (non-nested size)
        assert(FALSE);
        return (FALSE);
    }
    uint8_t obj[size];

    bool_t sizeof_alloc = sec_xdr_arena_size_allocator(xdrs);

    if (loc == NULL)
            switch (xdrs->x_op) {
            case XDR_FREE:
                return (TRUE);
            case XDR_DECODE:
                {
                    if (!sec_mem_alloc(xdrs, size, &loc))
                        return (FALSE);
					if (!loc) {
                        memset(obj, 0, size);
						loc = &obj[0];
                    }
					if (!sizeof_alloc)
						*pp = loc;
					break;
                }
            case XDR_ENCODE:
                break;
            }

    stat = (*proc)(xdrs, loc, 0);

    if (xdrs->x_op == XDR_FREE) {
        sec_mem_free(xdrs, loc, size);
        *pp = NULL;
    }
    return (stat);
}


bool_t
sec_xdr_pointer(XDR *xdrs, uint8_t **objpp, u_int obj_size, xdrproc_t xdr_obj)
{
    bool_t more_data;

    more_data = (objpp ? (*objpp != NULL) : FALSE);
    if (! xdr_bool(xdrs,&more_data))
        return (FALSE);

    bool_t sizeof_alloc = sec_xdr_arena_size_allocator(xdrs);

    if (! more_data) {
        if ((xdrs->x_op == XDR_DECODE) && !sizeof_alloc)
            *objpp = NULL;
        return (TRUE);
    }
    return (sec_xdr_reference(xdrs,objpp,obj_size,xdr_obj));
}

/**
 * This is almost a straight copy of the standard implementation, except
 * that all calls made that allocate memory can defer to an alternate 
 * mechanism, with the purpose to allocate from one block of memory on
 * *decode*
 */
