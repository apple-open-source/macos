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

#include <Security/SecPolicySearch.h>
#include <Security/PolicyCursor.h>
#include <Security/Policies.h>
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
	SecPointer<PolicyCursor> cursor(new PolicyCursor(oid, value));
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
