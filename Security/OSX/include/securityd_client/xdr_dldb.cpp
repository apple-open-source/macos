/*
 * Copyright (c) 2006,2011,2014 Apple Inc. All Rights Reserved.
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

#include "xdr_cssm.h"
#include "xdr_dldb.h"
#include <security_utilities/mach++.h>

bool_t
xdr_DLDbFlatIdentifier(XDR *xdrs, DataWalkers::DLDbFlatIdentifier *objp)
{
    if (!sec_xdr_pointer(xdrs, reinterpret_cast<uint8_t **>(&objp->uid), sizeof(CSSM_SUBSERVICE_UID), (xdrproc_t)xdr_CSSM_SUBSERVICE_UID))
        return (FALSE);
    if (!sec_xdr_charp(xdrs, reinterpret_cast<char **>(&objp->name), ~0))
        return (FALSE);
    if (!sec_xdr_pointer(xdrs, (uint8_t **)&objp->address, sizeof(CSSM_NET_ADDRESS), (xdrproc_t)xdr_CSSM_NET_ADDRESS))
        return (FALSE);
    return (TRUE);
}

bool_t
xdr_DLDbFlatIdentifierRef(XDR *xdrs, DataWalkers::DLDbFlatIdentifier **objp)
{
    if (!sec_xdr_reference(xdrs, reinterpret_cast<uint8_t **>(objp), sizeof(DataWalkers::DLDbFlatIdentifier), (xdrproc_t)xdr_DLDbFlatIdentifier))
        return (FALSE);
    return (TRUE);
}

CopyOut::~CopyOut()
{
	if (mData) {
		free(mData);
	}
	if(mDealloc && mSource) {
		MachPlusPlus::deallocate(mSource, mSourceLen);
	}
}
