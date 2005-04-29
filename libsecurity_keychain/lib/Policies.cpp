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
// Policy.cpp - Working with Policies
//
#include <security_keychain/Policies.h>
#include <security_utilities/debugging.h>

using namespace KeychainCore;

Policy::Policy(TP supportingTp, const CssmOid &policyOid)
    : mTp(supportingTp),
      mOid(Allocator::standard(), policyOid),
      mValue(Allocator::standard())
{
    // value is as yet unimplemented
	secdebug("policy", "Policy() this %p", this);
}

Policy::~Policy() throw()
{
	secdebug("policy", "~Policy() this %p", this);
}

bool Policy::operator < (const Policy& other) const
{
    //@@@ inefficient
    return oid() < other.oid() ||
        oid() == other.oid() && value() < other.value();
}

bool Policy::operator == (const Policy& other) const
{
    return oid() == other.oid() && value() == other.value();
}
