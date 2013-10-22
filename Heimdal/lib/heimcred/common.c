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

#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFRuntime.h>
#import <CoreFoundation/CFXPCBridge.h>
#import <xpc/xpc.h>

#import "common.h"



#define HEIMCRED_CONST(_t,_c) \
    const _t _c = (_t)CFSTR(#_c); \
    const char *_c##xpc = #_c;

#include "heimcred-const.h"

/*
 * auth 
 */

HEIMCRED_CONST(CFTypeRef, kHEIMTargetName);

#undef HEIMCRED_CONST

/*
 *
 */

HeimCredContext HeimCredCTX;

/*
 *
 */

CFUUIDRef
HeimCredCopyUUID(xpc_object_t object, const char *key)
{
    CFUUIDBytes bytes;
    const void *data = xpc_dictionary_get_uuid(object, key);
    if (data == NULL)
	return NULL;
    memcpy(&bytes, data, sizeof(bytes));
    return CFUUIDCreateFromUUIDBytes(NULL, bytes);
}

CFDictionaryRef
HeimCredMessageCopyAttributes(xpc_object_t object, const char *key)
{
    xpc_object_t xpcattrs = xpc_dictionary_get_value(object, key);
    if (xpcattrs == NULL)
	return NULL;
    return _CFXPCCreateCFObjectFromXPCObject(xpcattrs);
}

void
HeimCredMessageSetAttributes(xpc_object_t object, const char *key, CFTypeRef attrs)
{
    xpc_object_t xpcattrs = _CFXPCCreateXPCObjectFromCFObject(attrs);
    if (xpcattrs == NULL)
	return;
    xpc_dictionary_set_value(object, key, xpcattrs);
    xpc_release(xpcattrs);
}


void
HeimCredSetUUID(xpc_object_t object, const char *key, CFUUIDRef uuid)
{
    CFUUIDBytes bytes = CFUUIDGetUUIDBytes(uuid);
    uuid_t u;
    memcpy(&u, &bytes, sizeof(u));
    xpc_dictionary_set_uuid(object, key, u);
}

static CFStringRef
HeimCredCopyFormatString(CFTypeRef cf, CFDictionaryRef formatOptions)
{
    return CFSTR("format");
}

static CFStringRef
HeimCredCopyDebugName(CFTypeRef cf)
{
    HeimCredRef cred = (HeimCredRef)cf;
    CFTypeRef client = CFDictionaryGetValue(cred->attributes, kHEIMAttrClientName);
    CFTypeRef server = CFDictionaryGetValue(cred->attributes, kHEIMAttrServerName);
    CFTypeRef group = CFDictionaryGetValue(cred->attributes, kHEIMAttrCredentialGroup);
    CFTypeRef parent = CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential);
    CFTypeRef lead = CFDictionaryGetValue(cred->attributes, kHEIMAttrCredentialGroupLead);
    CFTypeRef acl = CFDictionaryGetValue(cred->attributes, kHEIMAttrBundleIdentifierACL);
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("HeimCred<%@ group: %@ parent: %@ client: %@ server: %@ lead: %s ACL: %@>"),
				    cred->uuid, group, parent, client, server, lead ? "yes" : "no", acl ? acl : CFSTR(""));
}

static void
HeimCredReleaseItem(CFTypeRef item)
{
    HeimCredRef cred = (HeimCredRef)item;
    CFRELEASE_NULL(cred->uuid);
    CFRELEASE_NULL(cred->attributes);
}

void
_HeimCredInitCommon(void)
{
    static dispatch_once_t once;

    dispatch_once(&once, ^{
	    static const CFRuntimeClass HeimCredClass = {
		0,
		"HeimCredential",
		NULL,
		NULL,
		HeimCredReleaseItem,
		NULL,
		NULL,
		HeimCredCopyFormatString,
		HeimCredCopyDebugName
	    };
	    HeimCredCTX.haid = _CFRuntimeRegisterClass(&HeimCredClass);

	    HeimCredCTX.queue = dispatch_queue_create("HeimCred", NULL);
	    HeimCredCTX.items = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	});
}


HeimCredRef
HeimCredCreateItem(CFUUIDRef uuid)
{
    HeimCredRef cred = (HeimCredRef)_CFRuntimeCreateInstance(NULL, HeimCredCTX.haid, sizeof(struct HeimCred_s) - sizeof(CFRuntimeBase), NULL);
    if (cred == NULL)
	return NULL;
    
    CFRetain(uuid);
    cred->uuid = uuid;
    return cred;
}

