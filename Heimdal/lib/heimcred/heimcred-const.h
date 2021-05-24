/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 - 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrType); /* kHEIMAttrType */

HEIMCRED_CONST(CFStringRef, kHEIMTypeGeneric);
HEIMCRED_CONST(CFStringRef, kHEIMTypeKerberos);
HEIMCRED_CONST(CFStringRef, kHEIMTypeIAKerb);
HEIMCRED_CONST(CFStringRef, kHEIMTypeNTLM);
HEIMCRED_CONST(CFStringRef, kHEIMTypeConfiguration);
HEIMCRED_CONST(CFStringRef, kHEIMTypeSchema);
HEIMCRED_CONST(CFStringRef, kHEIMTypeKerberosAcquireCred);

/* schema types */
HEIMCRED_CONST(CFStringRef, kHEIMObjectType);
HEIMCRED_CONST(CFStringRef, kHEIMObjectKerberos);
HEIMCRED_CONST(CFStringRef, kHEIMObjectNTLM);
HEIMCRED_CONST(CFStringRef, kHEIMObjectGeneric);
HEIMCRED_CONST(CFStringRef, kHEIMObjectConfiguration);
HEIMCRED_CONST(CFStringRef, kHEIMObjectKerberosAcquireCred);
HEIMCRED_CONST(CFStringRef, kHEIMObjectAny);

HEIMCRED_CONST(CFTypeRef, kHEIMAttrClientName);
HEIMCRED_CONST(CFStringRef, kHEIMNameUserName);
HEIMCRED_CONST(CFStringRef, kHEIMNameMechKerberos);
HEIMCRED_CONST(CFStringRef, kHEIMNameMechIAKerb);
HEIMCRED_CONST(CFStringRef, kHEIMNameMechNTLM);

HEIMCRED_CONST(CFTypeRef, kHEIMAttrServerName); /* CFDict of types generic + mech names */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrUUID);

HEIMCRED_CONST(CFTypeRef, kHEIMAttrDisplayName);

HEIMCRED_CONST(CFTypeRef, kHEIMAttrCredential);	/* CFBooleanRef */
HEIMCRED_CONST(CFStringRef, kHEIMCredentialPassword);
HEIMCRED_CONST(CFStringRef, kHEIMCredentialCertificate);

HEIMCRED_CONST(CFTypeRef, kHEIMAttrLeadCredential); /* CFBooleanRef */
HEIMCRED_CONST(CFTypeRef, kHEIMAttrParentCredential); /* CFUUIDRef */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrData); /* CFDataRef */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrTransient);

HEIMCRED_CONST(CFTypeRef, kHEIMAttrAllowedDomain); /* CFArray[match rules] */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrStatus);
HEIMCRED_CONST(CFStringRef, kHEIMStatusInvalid);
HEIMCRED_CONST(CFStringRef, kHEIMStatusCanRefresh);
HEIMCRED_CONST(CFStringRef, kHEIMStatusValid);

HEIMCRED_CONST(CFTypeRef, kHEIMAttrStoreTime); /* CFDateRef */
HEIMCRED_CONST(CFTypeRef, kHEIMAttrAuthTime); /* CFDateRef */
HEIMCRED_CONST(CFTypeRef, kHEIMAttrExpire); /* CFDateRef */
HEIMCRED_CONST(CFTypeRef, kHEIMAttrRenewTill); /* CFDateRef */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrRetainStatus); /* CFNumberRef */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrBundleIdentifierACL); /* CFArray[bundle-id] */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrDefaultCredential); /* BooleanRef */
HEIMCRED_CONST(CFTypeRef, kHEIMAttrTemporaryCache); /* BooleanRef */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrKerberosTicketGrantingTicket); /* BooleanRef */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrAltDSID); /* Unique User Id for Shared iPad */

HEIMCRED_CONST(CFTypeRef, kHEIMAttrUserID); /*  User Id for use-uid-matching */
HEIMCRED_CONST(CFTypeRef, kHEIMAttrASID); /*  asid for use-uid-matching */

/* NTLM */
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMUsername);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMDomain);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMChannelBinding);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMChallenge);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMType1Data);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMType2Data);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMType3Data);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMClientTargetName);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMClientFlags);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMSessionKey);
HEIMCRED_CONST(CFStringRef, kHEIMAttrNTLMKCMFlags);

#define HEIMCRED_NTLM_FLAG_AV_GUEST 8
