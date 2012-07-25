/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * CLCachedEntry.cpp - classes representing cached certs and CRLs. 
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */

#include "CLCachedEntry.h"

/*
 * CLCachedEntry base class constructor. Only job here is to cook up 
 * a handle.
 */
CLCachedEntry::CLCachedEntry()
{
	mHandle = reinterpret_cast<CSSM_HANDLE>(this);
}

CLCachedCert::~CLCachedCert()
{
	delete &mCert;
}

CLCachedCRL::~CLCachedCRL()
{
	delete &mCrl;
}

CLQuery::CLQuery(
	CLQueryType		type,
	const CssmOid	&oid,
	unsigned		numFields,
	bool			isFromCache,
	CSSM_HANDLE		cachedObj) :
		mQueryType(type),
		mFieldId(Allocator::standard()),
		mNextIndex(1),
		mNumFields(numFields),
		mFromCache(isFromCache),
		mCachedObject(cachedObj)
{
	mFieldId.copy(oid);
	mHandle = reinterpret_cast<CSSM_HANDLE>(this);
}	

CLQuery::~CLQuery()
{
	/* mFieldId auto frees */
}
