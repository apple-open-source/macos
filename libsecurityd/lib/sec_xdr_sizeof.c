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
/*
 * xdr_sizeof.c
 *
 * Copyright 1990 Sun Microsystems, Inc.
 *
 * General purpose routine to see how much space something will use
 * when serialized using XDR.
 */
#include <stdlib.h>

#include "sec_xdr.h"

/* ARGSUSED */
static bool_t
sec_x_putlong(xdrs, intp)
        XDR *xdrs;
        int *intp;
{
    xdrs->x_handy += BYTES_PER_XDR_UNIT;
    return (TRUE);
}

/* ARGSUSED */
static bool_t
sec_x_putbytes(xdrs, bp, len)
        XDR *xdrs;
        char  *bp;
        u_int len;
{
    xdrs->x_handy += len;
    return (TRUE);
}

static u_int
sec_x_getpostn(xdrs)
        XDR *xdrs;
{
    return (xdrs->x_handy);
}

/* ARGSUSED */
static bool_t
sec_x_setpostn(xdrs, pos)
        XDR *xdrs;
        u_int pos;
{
    /* This is not allowed */
    return (FALSE);
}

static int32_t *
sec_x_inline(xdrs, len)
        XDR *xdrs;
        u_int len;
{
    intptr_t llen;

    if (len == 0) {
        return (NULL);
    }
    
    if (xdrs->x_op != XDR_ENCODE) {
        return (NULL);
    }

    llen = len;
    
    if (llen < (intptr_t)xdrs->x_base) {
        /* x_private was already allocated */
        xdrs->x_handy += llen;
        return ((int32_t *) xdrs->x_private);
    } else {
        /* Free the earlier space and allocate new area */
        if (xdrs->x_private)
                free(xdrs->x_private);
        if ((xdrs->x_private = (caddr_t) malloc(len)) == NULL) {
                xdrs->x_base = 0;
                return (NULL);
        }
        xdrs->x_base = (caddr_t)llen;
        xdrs->x_handy += llen;
        return ((int32_t *) xdrs->x_private);
    }
}

static int
sec_harmless()
{
    /* Always return FALSE/NULL, as the case may be */
    return (0);
}

static void
sec_x_destroy(xdrs)
        XDR *xdrs;
{
    xdrs->x_handy = 0;
    xdrs->x_base = 0;
    if (xdrs->x_private) {
        free(xdrs->x_private);
        xdrs->x_private = NULL;
    }
    return;
}

u_int
sec_xdr_sizeof_in(func, data)
        xdrproc_t func;
        void *data;
{
    XDR x;
    struct xdr_ops ops;
    bool_t stat;
    /* to stop ANSI-C compiler from complaining */
#ifdef __LP64__
    typedef  bool_t (* dummyfunc1)(XDR *, int *);
#else
    typedef  bool_t (* dummyfunc1)(XDR *, long *);
#endif
    typedef  bool_t (* dummyfunc2)(XDR *, caddr_t, u_int);

    ops.x_putlong = sec_x_putlong;
    ops.x_putbytes = sec_x_putbytes;
    ops.x_inline = sec_x_inline;
    ops.x_getpostn = sec_x_getpostn;
    ops.x_setpostn = sec_x_setpostn;
    ops.x_destroy = sec_x_destroy;

    /* the other harmless ones */
    ops.x_getlong =  (dummyfunc1) sec_harmless;
    ops.x_getbytes = (dummyfunc2) sec_harmless;

    x.x_op = XDR_ENCODE;
    x.x_ops = &ops;
    x.x_handy = 0;
    x.x_public = (caddr_t) NULL; // explicitly unsetting to avoid confusion with custom allocator
    x.x_private = (caddr_t) NULL;
    x.x_base = (caddr_t) 0;

    sec_xdr_arena_allocator_t size_alloc;
    sec_xdr_arena_init_size_alloc(&size_alloc, &x);
    stat = func(&x, data);
    if (x.x_private)
        free(x.x_private);
    return (stat == TRUE ? (unsigned) x.x_handy: 0);
}

u_int
sec_xdr_sizeof_out(copy, size, func, data)
        const void *copy;
        u_int size;
        xdrproc_t func;
	void **data;
{
    XDR x;
    bool_t stat;

    sec_xdrmem_create(&x, (void *)copy, size, XDR_DECODE);

    sec_xdr_arena_allocator_t size_alloc;
    sec_xdr_arena_init_size_alloc(&size_alloc, &x);
    stat = func(&x, data);
    if (size_alloc.data)
        free(size_alloc.data);
    return (stat == TRUE ? (unsigned long)size_alloc.offset : 0);
}
