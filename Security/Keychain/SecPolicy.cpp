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

#include <Security/SecPolicy.h>
#include "SecBridge.h"


static inline Policy *Required(SecPolicyRef policyRef)
{
    return gTypes().policy.required(policyRef);
}


//
// CF boilerplate
//
CFTypeID
SecPolicyGetTypeID(void)
{
	BEGIN_SECAPI
	return gTypes().policy.typeId;
	END_SECAPI1(_kCFRuntimeNotATypeID)
}


//
// Sec API bridge functions
//
OSStatus
SecPolicyGetOID(SecPolicyRef policyRef, CSSM_OID* oid)
{
    BEGIN_SECAPI
    Required(oid) = Required(policyRef)->oid();
	END_SECAPI
}


OSStatus
SecPolicyGetValue(SecPolicyRef policyRef, CSSM_DATA* value)
{
    BEGIN_SECAPI
    Required(value) = Required(policyRef)->value();
	END_SECAPI
}


OSStatus
SecPolicyGetTPHandle(SecPolicyRef policyRef, CSSM_TP_HANDLE* tpHandle)
{
    BEGIN_SECAPI
    Required(tpHandle) = Required(policyRef)->tp()->handle();
	END_SECAPI
}
