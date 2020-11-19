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

#ifndef HEIMDAL_HEIMCRED_H
#define HEIMDAL_HEIMCRED_H 1

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

#define HEIMCRED_CONST(_t,_c) extern const _t _c
#include <heimcred-const.h>
#undef HEIMCRED_CONST

/*
 *
 */

typedef struct HeimCred_s *HeimCredRef;

HeimCredRef
HeimCredCreate(CFDictionaryRef attributes, CFErrorRef *error);

CFUUIDRef
HeimCredGetUUID(HeimCredRef);

CFDictionaryRef
HeimCredGetAttributes(HeimCredRef);

HeimCredRef
HeimCredCopyFromUUID(CFUUIDRef);

bool
HeimCredSetAttribute(HeimCredRef cred, CFTypeRef key, CFTypeRef value, CFErrorRef *error);

bool
HeimCredSetAttributes(HeimCredRef cred, CFDictionaryRef attributes, CFErrorRef *error);

CFDictionaryRef
HeimCredCopyAttributes(HeimCredRef cred, CFSetRef attributes, CFErrorRef *error);

CFTypeRef
HeimCredCopyAttribute(HeimCredRef cred, CFTypeRef attribute);

CFArrayRef
HeimCredCopyQuery(CFDictionaryRef query);

bool
HeimCredDeleteQuery(CFDictionaryRef query, CFErrorRef *error);

void
HeimCredDelete(HeimCredRef item);

void
HeimCredDeleteByUUID(CFUUIDRef uuid);

void
HeimCredRetainTransient(HeimCredRef cred);

void
HeimCredReleaseTransient(HeimCredRef cred);

bool
HeimCredMove(CFUUIDRef from, CFUUIDRef to);

CFUUIDRef
HeimCredCopyDefaultCredential(CFStringRef mech, CFErrorRef *error);

CFDictionaryRef
HeimCredCopyStatus(CFStringRef mech);

CFDictionaryRef
HeimCredDoAuth(HeimCredRef cred, CFDictionaryRef attributes, CFErrorRef *error);

bool
HeimCredDeleteAll(CFStringRef altDSID, CFErrorRef *error);

/*
 * Only valid client side
 */

void
HeimCredSetImpersonateBundle(CFStringRef bundle);

const char *
HeimCredGetImpersonateBundle(void);

void
HeimCredSetImpersonateAuditToken(CFDataRef auditToken) API_AVAILABLE(macos(10.16));

CFDataRef
HeimCredGetImpersonateAuditToken(void) API_AVAILABLE(macos(10.16));


// Use for automated tests only, not for normal use.
void
_HeimCredResetLocalCache(void);

/*
 * Only valid server side side
 */
typedef CFDictionaryRef (*HeimCredAuthCallback)(HeimCredRef, CFDictionaryRef);
typedef CFTypeRef (*HeimCredStatusCallback)(HeimCredRef);
typedef void (*HeimCredNotifyCaches)(void);

void
_HeimCredRegisterMech(CFStringRef mech,
		      CFSetRef publicAttributes,
		      HeimCredStatusCallback statusCallback,
		      HeimCredAuthCallback authCallback,
		      HeimCredNotifyCaches notifyCaches,
		      bool readRestricted,
		      CFArrayRef readOnlyCommands);

void
_HeimCredRegisterKerberos(void);

void
_HeimCredRegisterNTLM(void);

void
_HeimCredRegisterKerberosAcquireCred(void);

CFMutableDictionaryRef
_HeimCredCreateBaseSchema(CFStringRef objectType);

/*
typedef struct HeimAuth_s *HeimAuthRef;

HeimAuthRef
HeimCreateAuthetication(CFDictionaryRef input);

bool
HeimAuthStep(HeimAuthRef cred, CFTypeRef input, CFTypeRef *output, CFErrorRef *error);
*/

#endif /* HEIMDAL_HEIMCRED_H */
