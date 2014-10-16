/*
 * Copyright (c) 2006,2011-2012,2014 Apple Inc. All Rights Reserved.
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

#include "xdr_auth.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

bool_t
xdr_AuthorizationItem(XDR *xdrs, AuthorizationItem *objp)
{
    if (!sec_xdr_charp(xdrs, (char **)&objp->name, ~0))
		return (FALSE);
		
    u_int valueLength;
	
    if (xdrs->x_op == XDR_ENCODE) {
		if (objp->valueLength > (u_int)~0)
			return (FALSE);
		valueLength = (u_int)objp->valueLength;
    }
	
    if (!sec_xdr_bytes(xdrs, (uint8_t **)&objp->value, &valueLength, ~0))
		return (FALSE);
		
    if (xdrs->x_op == XDR_DECODE)
		objp->valueLength = valueLength;
	
	// This is only ever 32 bits, but prototyped with long on 32 bit and int on 64 bit to fall in line with UInt32
    if (!xdr_u_long(xdrs, &objp->flags))
		return (FALSE);
		
    return (TRUE);
}

bool_t
xdr_AuthorizationItemSet(XDR *xdrs, AuthorizationItemSet *objp)
{
    return sec_xdr_array(xdrs, (uint8_t **)&objp->items, (u_int *)&objp->count, ~0, sizeof(AuthorizationItem), (xdrproc_t)xdr_AuthorizationItem);
}

bool_t
xdr_AuthorizationItemSetPtr(XDR *xdrs, AuthorizationItemSet **objp)
{
	return sec_xdr_reference(xdrs, (uint8_t **)objp,sizeof(AuthorizationItemSet), (xdrproc_t)xdr_AuthorizationItemSet);
}

inline bool_t copyin_AuthorizationItemSet(const AuthorizationItemSet *rights, void **copy, mach_msg_size_t *size)
{
	return copyin((AuthorizationItemSet *)rights, (xdrproc_t)xdr_AuthorizationItemSet, copy, size);
}

inline bool_t copyout_AuthorizationItemSet(const void *copy, mach_msg_size_t size, AuthorizationItemSet **rights)
{
	u_int length = 0;
	void *data = NULL; // allocate data for us
	bool_t ret = copyout(copy, size, (xdrproc_t)xdr_AuthorizationItemSetPtr, &data, &length);
	if (ret)
		*rights = data;
    return ret;
}
