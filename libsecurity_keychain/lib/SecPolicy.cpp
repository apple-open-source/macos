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

#include <CoreFoundation/CFString.h>
#include <Security/SecPolicy.h>
#include <Security/SecPolicyPriv.h>
#include <security_keychain/Policies.h>
#include <security_keychain/PolicyCursor.h>
#include "SecBridge.h"

//
// CF boilerplate
//
CFTypeID
SecPolicyGetTypeID(void)
{
	BEGIN_SECAPI
	return gTypes().Policy.typeID;
	END_SECAPI1(_kCFRuntimeNotATypeID)
}


//
// Sec API bridge functions
//
OSStatus
SecPolicyGetOID(SecPolicyRef policyRef, CSSM_OID* oid)
{
    BEGIN_SECAPI
    Required(oid) = Policy::required(policyRef)->oid();
	END_SECAPI
}


OSStatus
SecPolicyGetValue(SecPolicyRef policyRef, CSSM_DATA* value)
{
    BEGIN_SECAPI
    Required(value) = Policy::required(policyRef)->value();
	END_SECAPI
}

OSStatus
SecPolicySetValue(SecPolicyRef policyRef, const CSSM_DATA *value)
{
	BEGIN_SECAPI
    Required(value);
    const CssmData newValue(value->Data, value->Length);
	Policy::required(policyRef)->setValue(newValue);
	END_SECAPI
}


OSStatus
SecPolicyGetTPHandle(SecPolicyRef policyRef, CSSM_TP_HANDLE* tpHandle)
{
    BEGIN_SECAPI
    Required(tpHandle) = Policy::required(policyRef)->tp()->handle();
	END_SECAPI
}

OSStatus
SecPolicyCopyAll(CSSM_CERT_TYPE certificateType, CFArrayRef* policies)
{
    BEGIN_SECAPI
	Required(policies);
	CFMutableArrayRef currPolicies = NULL;
	currPolicies = CFArrayCreateMutable(NULL, 0, NULL);
	if ( currPolicies )
	{
		SecPointer<PolicyCursor> cursor(new PolicyCursor(NULL, NULL));
		SecPointer<Policy> policy;
		while ( cursor->next(policy) ) /* copies the next policy */
		{
			CFArrayAppendValue(currPolicies, policy->handle());  /* 'SecPolicyRef' appended */
			CFRelease(policy->handle()); /* refcount bumped up when appended to array */
		}
		*policies = CFArrayCreateCopy(NULL, currPolicies);
		CFRelease(currPolicies);
		CFRelease(cursor->handle());
	}
	END_SECAPI
}

OSStatus
SecPolicyCopy(CSSM_CERT_TYPE certificateType, const CSSM_OID *policyOID, SecPolicyRef* policy)
{
	Required(policy);
	Required(policyOID);
	
	SecPolicySearchRef srchRef = NULL;
	OSStatus ortn;
	
	ortn = SecPolicySearchCreate(certificateType, policyOID, NULL, &srchRef);
	if(ortn) {
		return ortn;
	}
	ortn = SecPolicySearchCopyNext(srchRef, policy);
	CFRelease(srchRef);
	return ortn;
}

/* new in 10.6 */
SecPolicyRef
SecPolicyCreateBasicX509(void)
{
    // return a SecPolicyRef object for the X.509 Basic policy
    SecPolicyRef policy = nil;
    SecPolicySearchRef policySearch = nil;
    OSStatus status = SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &policySearch);
    if (!status) {
        status = SecPolicySearchCopyNext(policySearch, &policy);
    }
	if (policySearch) {
		CFRelease(policySearch);
	}
    return policy;
}

/* new in 10.6 */
SecPolicyRef
SecPolicyCreateSSL(Boolean server, CFStringRef hostname)
{
    // return a SecPolicyRef object for the SSL policy, given hostname and client options
    SecPolicyRef policy = nil;
    SecPolicySearchRef policySearch = nil;
    OSStatus status = SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_TP_SSL, NULL, &policySearch);
    if (!status) {
        status = SecPolicySearchCopyNext(policySearch, &policy);
    }
    if (!status && policy) {
        // set options for client-side or server-side policy evaluation
		char *strbuf = NULL;
		const char *hostnamestr = NULL;
		if (hostname) {
			CFIndex strbuflen = 0;
			hostnamestr = CFStringGetCStringPtr(hostname, kCFStringEncodingUTF8);
			if (hostnamestr == NULL) {
				strbuflen = CFStringGetLength(hostname)*6;
				strbuf = (char *)malloc(strbuflen+1);
				if (CFStringGetCString(hostname, strbuf, strbuflen, kCFStringEncodingUTF8)) {
					hostnamestr = strbuf;
				}
			}
		}
        uint32 hostnamelen = (hostnamestr) ? strlen(hostnamestr) : 0;
        uint32 flags = (!server) ? CSSM_APPLE_TP_SSL_CLIENT : 0;
        CSSM_APPLE_TP_SSL_OPTIONS opts = {CSSM_APPLE_TP_SSL_OPTS_VERSION, hostnamelen, hostnamestr, flags};
        CSSM_DATA data = {sizeof(opts), (uint8*)&opts};
        SecPolicySetValue(policy, &data);
		
		if (strbuf) {
			free(strbuf);
		}
    }
	if (policySearch) {
		CFRelease(policySearch);
	}
    return policy;
}

