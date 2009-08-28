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
#include <Security/oidsalg.h>

using namespace KeychainCore;

Policy::Policy(TP supportingTp, const CssmOid &policyOid)
    : mTp(supportingTp),
      mOid(Allocator::standard(), policyOid),
      mValue(Allocator::standard()),
      mAuxValue(Allocator::standard())
{
    // value is as yet unimplemented
	secdebug("policy", "Policy() this %p", this);
}

Policy::~Policy() throw()
{
	secdebug("policy", "~Policy() this %p", this);
}

void Policy::setValue(const CssmData &value)
{
	StLock<Mutex>_(mMutex);
    mValue = value;
    mAuxValue.reset();

    // Certain policy values may contain an embedded pointer. Ask me how I feel about that.
    if (mOid == CSSMOID_APPLE_TP_SSL ||
        mOid == CSSMOID_APPLE_TP_EAP ||
        mOid == CSSMOID_APPLE_TP_IP_SEC)
    {
        CSSM_APPLE_TP_SSL_OPTIONS *opts = (CSSM_APPLE_TP_SSL_OPTIONS *)value.data();
        if (opts->Version == CSSM_APPLE_TP_SSL_OPTS_VERSION)
        {
			if (opts->ServerNameLen > 0)
			{
				// Copy auxiliary data, then update the embedded pointer to reference our copy
				mAuxValue.copy(const_cast<char*>(opts->ServerName), opts->ServerNameLen);
				mValue.get().interpretedAs<CSSM_APPLE_TP_SSL_OPTIONS>()->ServerName =
					reinterpret_cast<char*>(mAuxValue.data());
			}
			else
			{
				// Clear the embedded pointer!
				mValue.get().interpretedAs<CSSM_APPLE_TP_SSL_OPTIONS>()->ServerName =
					reinterpret_cast<char*>(NULL);
			}
		}
    }
    else if (mOid == CSSMOID_APPLE_TP_SMIME ||
             mOid == CSSMOID_APPLE_TP_ICHAT)
    {
        CSSM_APPLE_TP_SMIME_OPTIONS *opts = (CSSM_APPLE_TP_SMIME_OPTIONS *)value.data();
        if (opts->Version == CSSM_APPLE_TP_SMIME_OPTS_VERSION)
		{
			if (opts->SenderEmailLen > 0)
			{
				// Copy auxiliary data, then update the embedded pointer to reference our copy
				mAuxValue.copy(const_cast<char*>(opts->SenderEmail), opts->SenderEmailLen);
				mValue.get().interpretedAs<CSSM_APPLE_TP_SMIME_OPTIONS>()->SenderEmail =
					reinterpret_cast<char*>(mAuxValue.data());
			}
			else
			{
				// Clear the embedded pointer!
				mValue.get().interpretedAs<CSSM_APPLE_TP_SMIME_OPTIONS>()->SenderEmail =
					reinterpret_cast<char*>(NULL);
			}
		}
    }
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

bool Policy::equal(SecCFObject &other)
{
    return (*this) == (const Policy &)other;
}
