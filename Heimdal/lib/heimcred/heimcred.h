/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
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

/*

attributes:

	type: { generic, ntlm, scram, krb5 }
	name: [ { generic = val , krb5 = val, ntlm = val, scram = val } ] 

	uuid: [cfdata]

	persistant + temporary:
		name
		password / x509 credential

	allowed domains:
		{ mech - domains }

	status: { mech -> { invalid, can refersh, valid-for } , ... }

	retain-status: { false, integer }

    ADD_CRED
	{ generic, krb5, ntlm, scram } name
	type: persistant + temporary
    	password
    	x509-credential
	acl: [ application, bundle ids, developer id prefix ]

    DO_AUTH_NTLM,
	do

    DO_SCRAM_AUTH,
	do

    DO_KRB5_AUTH,
	mk-req
	rd-rep

	as-req-init
	as-req-step

    SEARCH_CREDS,

	by uuid
	by name
	by variable
	
	prefix matching
	subfixfix matching
	type matching

	return subuuids
	return labeluuids
	return attributes and their content

    SET_ATTRIBUTE,
	cache name
	cred name
	default-status
	label-name + label-value

    REMOVE_CRED

    RETAIN_CRED
    RELEASE_CRED

*/

#import <CoreFoundation/CoreFoundation.h>
#import <dispatch/dispatch.h>

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

HeimCredRef
HeimCredCopyFromUUID(CFUUIDRef);

bool
HeimCredSetAttribute(HeimCredRef cred, CFTypeRef key, CFTypeID value, CFErrorRef *error);

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

CFDictionaryRef
HeimCredDoAuth(HeimCredRef cred, CFDictionaryRef input);


/*
 * Only valid XPCService side
 */
typedef CFDictionaryRef (*HeimCredAuthCallback)(HeimCredRef, CFDictionaryRef);

void
_HeimCredRegisterMech(CFStringRef mech,
		      CFSetRef publicAttributes,
		      HeimCredAuthCallback callback);

/*
typedef struct HeimAuth_s *HeimAuthRef;

HeimAuthRef
HeimCreateAuthetication(CFDictionaryRef input);

bool
HeimAuthStep(HeimAuthRef cred, CFTypeRef input, CFTypeRef *output, CFErrorRef *error);
*/
