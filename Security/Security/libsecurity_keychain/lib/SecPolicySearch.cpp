/*
 * Copyright (c) 2002-2004,2011,2014 Apple Inc. All Rights Reserved.
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

#include <Security/SecPolicySearch.h>
#include <security_keychain/PolicyCursor.h>
#include <security_keychain/Policies.h>
#include "SecBridge.h"


//
// CF Boilerplate
CFTypeID
SecPolicySearchGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().PolicyCursor.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecPolicySearchCreate(
            CSSM_CERT_TYPE certType,
			const CSSM_OID* oid,
            const CSSM_DATA* value,
			SecPolicySearchRef* searchRef)
{
    BEGIN_SECAPI
	Required(searchRef);	// preflight
    PolicyCursor* pc = new PolicyCursor(oid, value);
    if (pc == NULL)
    {
        return errSecPolicyNotFound;
    }
    
	SecPointer<PolicyCursor> cursor(pc);
	*searchRef = cursor->handle();
	END_SECAPI
}


OSStatus
SecPolicySearchCopyNext(
            SecPolicySearchRef searchRef, 
            SecPolicyRef* policyRef)
{
    BEGIN_SECAPI
    
	RequiredParam(policyRef);
	SecPointer<Policy> policy;
	if (!PolicyCursor::required(searchRef)->next(policy))
		return errSecPolicyNotFound;
	*policyRef = policy->handle();
	END_SECAPI
}
