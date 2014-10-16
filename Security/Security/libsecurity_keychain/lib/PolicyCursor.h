/*
 * Copyright (c) 2002-2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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

//
// PolicyCursor.h
//
#ifndef _SECURITY_POLICYCURSOR_H_
#define _SECURITY_POLICYCURSOR_H_

#include <Security/SecPolicySearch.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <Security/mds.h>
#include <Security/mds_schema.h>
#include <security_utilities/seccfobject.h>
#include "SecCFTypes.h"

namespace Security
{

namespace KeychainCore
{

class Policy;

class PolicyCursor : public SecCFObject
{
    NOCOPY(PolicyCursor)
public:
	SECCFFUNCTIONS(PolicyCursor, SecPolicySearchRef, errSecInvalidSearchRef, gTypes().PolicyCursor)

	PolicyCursor(const CSSM_OID* oid, const CSSM_DATA* value);
	virtual ~PolicyCursor() throw();
	bool next(SecPointer<Policy> &policy);

	static void policy(const CSSM_OID* oid, SecPointer<Policy> &policy);

private:
    //CFArrayRef	 mKeychainSearchList;
    //SecKeyUsage  mKeyUsage;
    //SecPolicyRef mPolicy;
    CssmAutoData		mOid;
    bool				mOidGiven;
    // value ignored (for now?)

#if 1	// quick version -- using built-in policy list

    int					mSearchPos;	// next untried table entry

#else	// MDS version -- later
    bool				mFirstLookup;

    //
    // Initialization
    //
	MDS_HANDLE			mMdsHand;
	CSSM_DB_HANDLE		mDbHand;
	//
    // Used for searching (lookups)
    //
	MDS_DB_HANDLE		mObjDlDb;
	MDS_DB_HANDLE		mCdsaDlDb;
	MDS_FUNCS*			mMdsFuncs;
#endif

	Mutex				mMutex;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_POLICYCURSOR_H_
