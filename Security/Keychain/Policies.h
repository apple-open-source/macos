/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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


//
// Policies.h
//
#ifndef _SECURITY_POLICY_H_
#define _SECURITY_POLICY_H_

#include <Security/SecRuntime.h>
#include <Security/cssmdata.h>
#include <Security/tpclient.h>

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
    Policy(TP supportingTp, const CssmOid &policyOid);
    
public:
    virtual ~Policy();
    
    TP &tp()							{ return mTp; }
    const TP &tp() const				{ return mTp; }
    const CssmOid &oid() const			{ return mOid; }
    const CssmData &value() const		{ return mValue; }
    
    bool operator < (const Policy& other) const;
    bool operator == (const Policy& other) const;

private:
    TP					mTp;			// TP module for this Policy
    CssmAutoData		mOid;			// OID for this policy
    CssmAutoData		mValue;			// value for this policy
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_POLICY_H_
