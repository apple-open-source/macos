/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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
// Policies.h
//
#ifndef _SECURITY_POLICY_H_
#define _SECURITY_POLICY_H_

#include <Security/SecPolicy.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_client/tpclient.h>
#include <security_utilities/seccfobject.h>
#include "SecCFTypes.h"

namespace Security
{

namespace KeychainCore
{

using namespace CssmClient;

//
// A Policy[Impl] represents a particular
// CSSM "policy" managed by a particular TP.
//
class Policy : public SecCFObject
{
	NOCOPY(Policy)
public:
	SECCFFUNCTIONS(Policy, SecPolicyRef, errSecInvalidItemRef, gTypes().Policy)

    Policy(TP supportingTp, const CssmOid &policyOid);
    
public:
    virtual ~Policy() throw();
    
    TP &tp()							{ return mTp; }
    const TP &tp() const				{ return mTp; }
    const CssmOid &oid() const			{ return mOid; }
    const CssmData &value() const		{ return mValue; }
	CssmOwnedData &value()				{ return mValue; }
	
    void setValue(const CssmData &value);
    
    bool operator < (const Policy& other) const;
    bool operator == (const Policy& other) const;

    bool equal(SecCFObject &other);

private:
    TP					mTp;			// TP module for this Policy
    CssmAutoData		mOid;			// OID for this policy
    CssmAutoData		mValue;			// value for this policy
    CssmAutoData		mAuxValue;		// variable-length value data for this policy
	Mutex				mMutex;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_POLICY_H_
