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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <xpc/xpc.h>

#include "common.h"



#define HEIMCRED_CONST(_t,_c) \
    const _t _c = (_t)CFSTR(#_c); \
    const char *_c##xpc = #_c

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
typedef struct HeimCredEventContext_s *HeimCredEventContextRef;

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

CFTypeRef
HeimCredMessageCopyAttributes(xpc_object_t object, const char *key, CFTypeID type)
{
    xpc_object_t xpcattrs = xpc_dictionary_get_value(object, key);
    CFTypeRef item;
    if (xpcattrs == NULL)
	return NULL;
    item = _CFXPCCreateCFObjectFromXPCObject(xpcattrs);
    if (item && CFGetTypeID(item) != type) {
	CFRELEASE_NULL(item);
    }
    return item;	
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
    if (cred->attributes) {
	CFTypeRef client = CFDictionaryGetValue(cred->attributes, kHEIMAttrClientName);
	CFTypeRef server = CFDictionaryGetValue(cred->attributes, kHEIMAttrServerName);
	CFTypeRef parent = CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential);
	CFTypeRef group = CFDictionaryGetValue(cred->attributes, kHEIMAttrLeadCredential);
	CFTypeRef altDSID = CFDictionaryGetValue(cred->attributes, kHEIMAttrAltDSID);
	CFTypeRef uid = CFDictionaryGetValue(cred->attributes, kHEIMAttrUserID);
	CFTypeRef asid = CFDictionaryGetValue(cred->attributes, kHEIMAttrASID);
	
	int lead = group ? CFBooleanGetValue(group) : false;
	CFTypeRef acl = CFDictionaryGetValue(cred->attributes, kHEIMAttrBundleIdentifierACL);
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("HeimCred<%@ group: %@ parent: %@ client: %@ server: %@ lead: %s ACL: %@, altDSID: %@, Uid: %@, asid: %@>"),
					cred->uuid, group, parent, client, server, lead ? "yes" : "no", acl ? acl : CFSTR(""), altDSID ? : CFSTR(""), uid ? : CFSTR(""), asid ? : CFSTR(""));
    } else {
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("HeimCred<%@>"), cred->uuid);
    }
}

static void
HeimCredReleaseItem(CFTypeRef item)
{
    HeimCredRef cred = (HeimCredRef)item;
    CFRELEASE_NULL(cred->uuid);
    CFRELEASE_NULL(cred->attributes);
#if HEIMCRED_SERVER
    HEIMDAL_MUTEX_lock(&cred->event_mutex);
    if (cred->expireEventContext) {
	HEIMDAL_MUTEX_lock(&cred->expireEventContext->cred_mutex);
	cred->expireEventContext->cred = NULL;
	HEIMDAL_MUTEX_unlock(&cred->expireEventContext->cred_mutex);
	CFRELEASE_NULL(cred->expireEventContext);
    }
    if (cred->renewEventContext) {
	HEIMDAL_MUTEX_lock(&cred->renewEventContext->cred_mutex);
	cred->renewEventContext->cred = NULL;
	HEIMDAL_MUTEX_unlock(&cred->renewEventContext->cred_mutex);
	CFRELEASE_NULL(cred->renewEventContext);
    }
    if (cred->renew_event) {
	heim_ipc_event_cancel(cred->renew_event);
	heim_ipc_event_free(cred->renew_event);
	cred->renew_event = NULL;
    }
    if (cred->expire_event) {
	heim_ipc_event_cancel(cred->expire_event);
	heim_ipc_event_free(cred->expire_event);
	cred->expire_event = NULL;
    }
    HEIMDAL_MUTEX_unlock(&cred->event_mutex);
    HEIMDAL_MUTEX_destroy(&cred->event_mutex);
#endif
}

#if HEIMCRED_SERVER
static void
HeimCredEventContextReleaseItem(CFTypeRef item)
{
    HeimCredEventContextRef ctx = (HeimCredEventContextRef)item;
    HEIMDAL_MUTEX_destroy(&ctx->cred_mutex);
}


CFTypeID
HeimCredEventContextGetTypeID(void)
{
    _HeimCredInitCommon();
    return HeimCredCTX.heid;
}

HeimCredEventContextRef
HeimCredEventContextCreateItem(HeimCredRef cred)
{
    HeimCredEventContextRef ctx = (HeimCredEventContextRef)_CFRuntimeCreateInstance(NULL, HeimCredCTX.heid, sizeof(struct HeimCredEventContext_s) - sizeof(CFRuntimeBase), NULL);
    if (ctx == NULL)
	return NULL;

    // cred is a "weak" reference to break the retain cycle.  cred->event->event_context->cred
    // the cred will set this value to NULL when it is released and the event handlers retain the cred during execution.
    ctx->cred = cred;
    HEIMDAL_MUTEX_init(&ctx->cred_mutex);
    return ctx;
}
#endif


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
#if HEIMCRED_SERVER
	    static const CFRuntimeClass HeimCredEventContextClass = {
		0,
		"HeimCredEventContext",
		NULL,
		NULL,
		HeimCredEventContextReleaseItem,
		NULL,
		NULL,
		NULL,
		NULL
	    };
	    HeimCredCTX.heid = _CFRuntimeRegisterClass(&HeimCredEventContextClass);
#endif

	    HeimCredCTX.queue = dispatch_queue_create("HeimCred", NULL);

#if HEIMCRED_SERVER
	    HeimCredCTX.sessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
#else
	    HeimCredCTX.items = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
#endif
	});
}

CFTypeID
HeimCredGetTypeID(void)
{
    _HeimCredInitCommon();
    return HeimCredCTX.haid;
}


HeimCredRef
HeimCredCreateItem(CFUUIDRef uuid)
{
    HeimCredRef cred = (HeimCredRef)_CFRuntimeCreateInstance(NULL, HeimCredCTX.haid, sizeof(struct HeimCred_s) - sizeof(CFRuntimeBase), NULL);
    if (cred == NULL)
	return NULL;
    
    CFRetain(uuid);
    cred->uuid = uuid;
#if HEIMCRED_SERVER
    cred->acquire_status = CRED_STATUS_ACQUIRE_INITIAL;
    cred->expire_event = NULL;
    cred->renew_event = NULL;
    HEIMDAL_MUTEX_init(&cred->event_mutex);
    cred->is_acquire_cred = false;
#endif
    return cred;
}

