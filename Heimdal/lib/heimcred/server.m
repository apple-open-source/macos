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

#import <TargetConditionals.h>

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFRuntime.h>
#import <System/sys/codesign.h>
#import <xpc/xpc.h>
#import <xpc/private.h>
#import <syslog.h>
#import <sandbox.h>

#import "HeimCredCoder.h"
#import "heimcred.h"
#import "common.h"

/*
 *
 */
struct HeimMech_s {
    CFRuntimeBase runtime;
    CFStringRef name;
    CFSetRef publicAttributes;
    HeimCredAuthCallback callback;
};

static NSString *archivePath = NULL;

static void
FlattenCredential(const void *key, const void *value, void *context)
{
    HeimCredRef cred = (HeimCredRef)value;
    id nskey = [HeimCredDecoder copyCF2NS:key];
    id attrs = [HeimCredDecoder copyCF2NS:cred->attributes];
    NSMutableDictionary *flatten = context;
    [flatten setObject:attrs forKey:nskey];
    [nskey release];
    [attrs release];
}

static void
storeCredCache(void)
{
    id flatten = NULL;
    @try {
	flatten = [[NSMutableDictionary alloc] init];
	CFDictionaryApplyFunction(HeimCredCTX.items, FlattenCredential, flatten);
	[HeimCredDecoder archiveRootObject:flatten toFile:archivePath];
    } @catch(NSException *e) {
    } @finally {
	[flatten release];
    }
}

static void
readCredCache(void)
{
    @autoreleasepool {
	NSDictionary *flatten = NULL;
	@try {
	    flatten = [HeimCredDecoder copyUnarchiveObjectWithFileSecureEncoding:archivePath];
	    
	    if (!flatten || ![flatten isKindOfClass:[NSDictionary class]])
		return;
	    
	    [flatten enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
		CFUUIDRef cfkey = [HeimCredDecoder copyNS2CF:key];
		CFDictionaryRef cfvalue = [HeimCredDecoder copyNS2CF:value];
		if (cfkey && cfvalue) {
		    HeimCredRef cred = HeimCredCreateItem(cfkey);
		    cred->attributes = cfvalue;
		    CFRetain(cfvalue);
		    CFDictionarySetValue(HeimCredCTX.items, cred->uuid, cred);
		}
		CFRELEASE_NULL(cfkey);
		CFRELEASE_NULL(cfvalue);
	     }];
	} @catch(NSException *e) {
	    syslog(LOG_ERR, "readCredCache failed with: %s:%s", [[e name] UTF8String], [[e reason] UTF8String]);
	}
    }
}

struct peer {
    xpc_connection_t peer;
    CFStringRef bundleID;
};

static bool
checkACLInCredentialChain(struct peer *peer, CFUUIDRef uuid, bool *hasACL)
{
    int max_depth = 10;
    bool res = false;

    if (hasACL)
	*hasACL = false;

    while (1) {
	HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(HeimCredCTX.items, uuid);
	CFUUIDRef parent;

	if (cred == NULL)
	    goto pass;

	if (max_depth-- < 0)
	    goto out;
	
	CFArrayRef array = CFDictionaryGetValue(cred->attributes,  kHEIMAttrBundleIdentifierACL);
	if (array) {
	    CFIndex n, count;

	    if (hasACL)
		*hasACL = true;

	    if (CFGetTypeID(array) != CFArrayGetTypeID())
		goto out;

	    count = CFArrayGetCount(array);
	    for (n = 0; n < count; n++) {
		CFStringRef prefix = CFArrayGetValueAtIndex(array, n);

		if (CFGetTypeID(prefix) != CFStringGetTypeID())
		    goto out;

		if (CFStringHasPrefix(peer->bundleID, prefix))
		    goto pass;

		if (CFStringCompare(CFSTR("*"), prefix, 0) == kCFCompareEqualTo)
		    goto pass;
	    }
	    if (n >= count)
		goto out;
	}
	
	parent = CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential);
	if (parent == NULL)
	    goto out;
	
	if (CFEqual(parent, uuid))
	    break;

	uuid = parent;
    }

 pass:
    res = true;

  out:
    return res;
}

static bool
addPeerToACL(struct peer *peer, CFMutableDictionaryRef attrs)
{
    CFArrayRef acl = CFDictionaryGetValue(attrs, kHEIMAttrBundleIdentifierACL);
    if (acl == NULL || CFGetTypeID(acl) != CFArrayGetTypeID())
	return false;

    if (!CFArrayContainsValue(acl, CFRangeMake(0, CFArrayGetCount(acl)), peer->bundleID)) {
	CFMutableArrayRef a = CFArrayCreateMutableCopy(NULL, 0, acl);
	if (a == NULL)
	    return false;
	CFArrayAppendValue(a, peer->bundleID);
	CFDictionarySetValue(attrs, kHEIMAttrBundleIdentifierACL, a);
	acl = a;
	CFRELEASE_NULL(a);
    }

    return true;
}

static void
do_CreateCred(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFMutableDictionaryRef attrs = NULL;
    HeimCredRef cred = NULL;
    CFUUIDRef uuid = NULL;
    bool hasACL = false;
    
    CFDictionaryRef attributes = HeimCredMessageCopyAttributes(request, "attributes");
    if (attributes == NULL)
	return;
    
    /* check if we are ok to link into this cred-tree */
    uuid = CFDictionaryGetValue(attributes, kHEIMAttrParentCredential);
    if (uuid != NULL && !checkACLInCredentialChain(peer, uuid, &hasACL))
	return;

    uuid = CFDictionaryGetValue(attributes, kHEIMAttrUUID);
    if (uuid) {
	CFRetain(uuid);

	if (CFDictionaryGetValue(HeimCredCTX.items, uuid) != NULL)
	    goto out;
	
    } else {
	uuid = CFUUIDCreate(NULL);
	if (uuid == NULL)
	    goto out;
    }
    cred = HeimCredCreateItem(uuid);
    if (cred == NULL)
	goto out;
    
    /* XXX filter attributes */

    attrs= CFDictionaryCreateMutableCopy(NULL, 0, attributes);
    if (attrs == NULL)
	goto out;

    bool hasACLInAttributes = addPeerToACL(peer, attrs);

    /* make sure this cred-tree as an ACL */
    if (!hasACL && !hasACLInAttributes) {
	const void *values[1] = { (void *)peer->bundleID };
	CFArrayRef array = CFArrayCreate(NULL, values, sizeof(values)/sizeof(values[0]), &kCFTypeArrayCallBacks);
	if (array == NULL) abort();
	CFDictionarySetValue(attrs, kHEIMAttrBundleIdentifierACL, array);
	CFRELEASE_NULL(array);
    }

    

    CFDictionarySetValue(attrs, kHEIMAttrUUID, uuid);

    /* make sure credential always are have a parent and group */
    if (CFDictionaryGetValue(attrs, kHEIMAttrParentCredential) == NULL)
	CFDictionarySetValue(attrs, kHEIMAttrParentCredential, uuid);
    if (CFDictionaryGetValue(attrs, kHEIMAttrCredentialGroup) == NULL)
	CFDictionarySetValue(attrs, kHEIMAttrCredentialGroup, uuid);

    cred->attributes = attrs;
    
    CFDictionarySetValue(HeimCredCTX.items, cred->uuid, cred);
    
    HeimCredMessageSetAttributes(reply, "attributes", cred->attributes);
out:
    CFRELEASE_NULL(attributes);
    CFRELEASE_NULL(cred);
    CFRELEASE_NULL(uuid);
}

struct MatchCTX {
    struct peer *peer;
    CFMutableArrayRef results;
    CFDictionaryRef query;
    CFIndex numQueryItems;
    HeimCredRef cred;
    CFIndex count;
};

static void
MatchQueryItem(const void *key, const void *value, void *context)
{
    struct MatchCTX * mc = (struct MatchCTX *)context;
    if (mc->cred->attributes == NULL)
	return;
    CFTypeRef val = CFDictionaryGetValue(mc->cred->attributes, key);
    if (val == NULL)
	return;
    if (!CFEqual(val, value))
	return;
    mc->count++;
}

static void
MatchQuery(const void *key, const void *value, void *context)
{
    struct MatchCTX *mc = (struct MatchCTX *)context;
    CFUUIDRef uuid = (CFUUIDRef)key;

    if (!checkACLInCredentialChain(mc->peer, uuid, NULL))
	return;

    HeimCredRef cred = (HeimCredRef)value;
    if (cred->attributes == NULL)
	return;
    mc->cred = cred;
    mc->count = 0;
    CFDictionaryApplyFunction(mc->query, MatchQueryItem, mc);
    if (mc->numQueryItems < mc->count) abort();
    if (mc->numQueryItems == mc->count)
	CFArrayAppendValue(mc->results, cred);
}


static CFArrayRef
QueryCopy(struct peer *peer, xpc_object_t request, const char *key)
{
    struct MatchCTX mc = {
	.peer = peer,
	.results = NULL,
	.query = NULL,
	.cred = NULL,
    };
    CFUUIDRef uuidref = NULL;
    
    
    mc.query = HeimCredMessageCopyAttributes(request, key);
    if (mc.query == NULL)
	goto out;

    mc.numQueryItems = CFDictionaryGetCount(mc.query);

    if (mc.numQueryItems == 1) {
	uuidref = CFDictionaryGetValue(mc.query, kHEIMAttrUUID);
	if (uuidref && CFGetTypeID(uuidref) == CFUUIDGetTypeID()) {
	
	    if (!checkACLInCredentialChain(peer, uuidref, NULL))
		goto out;

	    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(HeimCredCTX.items, uuidref);
	
	    const void *values[1] = { (void *)cred } ;
	
	    mc.results = (CFMutableArrayRef)CFArrayCreate(NULL, (const void **)&values, cred ? 1 : 0, &kCFTypeArrayCallBacks);
	    goto out;
	}
    }
    
    mc.results = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    CFDictionaryApplyFunction(HeimCredCTX.items, MatchQuery, &mc);
    
out:
    CFRELEASE_NULL(mc.query);
    return mc.results;
}

struct child {
    CFUUIDRef parent;
    CFMutableArrayRef array;
};

static void
DeleteChild(const void *key, const void *value, void *context)
{
    struct child *child = (struct child  *)context;

    if (CFEqual(key, child->parent))
	return;

    HeimCredRef cred = (HeimCredRef)value;
    CFUUIDRef parent = CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential);
    if (parent && CFEqual(child->parent, parent))
	CFArrayAppendValue(child->array, cred->uuid);
}

static void
DeleteChildrenApplier(const void *value, void *context)
{
    if (CFGetTypeID(value) != CFUUIDGetTypeID()) abort();
    CFDictionaryRemoveValue(HeimCredCTX.items, value);
}


static void
DeleteChildren(CFUUIDRef parent)
{
    /*
     * delete all child entries for to UUID
     */
    struct child child = {
	.parent = parent,
	.array = NULL,
    };
    child.array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFDictionaryApplyFunction(HeimCredCTX.items, DeleteChild, &child);
    CFArrayApplyFunction(child.array, CFRangeMake(0, CFArrayGetCount(child.array)), DeleteChildrenApplier, NULL);
    CFRelease(child.array);
}

struct fromto {
    CFUUIDRef from;
    CFUUIDRef to;
};

static void
UpdateParent(const void *key, const void *value, void *context)
{
    struct fromto *fromto = (struct fromto *)context;
    HeimCredRef cred = (HeimCredRef)value;
    CFUUIDRef parent = CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential);
    if (parent && CFEqual(parent, fromto->from)) {
	CFMutableDictionaryRef attrs = CFDictionaryCreateMutableCopy(NULL, 0, cred->attributes);
	CFRelease(cred->attributes);
	CFDictionarySetValue(attrs, kHEIMAttrParentCredential, fromto->to);
	CFDictionarySetValue(attrs, kHEIMAttrCredentialGroup, fromto->to);
	cred->attributes = attrs;
    }
}




static void
do_Delete(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFArrayRef items = QueryCopy(peer, request, "query");
    if (items == NULL)
	return;
    
    CFIndex n, count = CFArrayGetCount(items);
    for (n = 0; n < count; n++) {
	HeimCredRef cred = (HeimCredRef)CFArrayGetValueAtIndex(items, n);
	CFDictionaryRemoveValue(HeimCredCTX.items, cred->uuid);
	DeleteChildren(cred->uuid);
    }
    CFRelease(items);
}

static void
updateCred(const void *key, const void *value, void *context)
{
    CFMutableDictionaryRef attrs = (CFMutableDictionaryRef)context;
    /* XXX filter keys */
    CFDictionarySetValue(attrs, key, value);
}


static void
do_SetAttrs(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef uuid = HeimCredCopyUUID(request, "uuid");
    CFMutableDictionaryRef attrs;
    
    if (uuid == NULL)
	return;

    if (!checkACLInCredentialChain(peer, uuid, NULL))
	return;

    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(HeimCredCTX.items, uuid);
    CFRelease(uuid);
    if (cred == NULL)
	return;
    
    if (cred->attributes) {
	attrs = CFDictionaryCreateMutableCopy(NULL, 0, cred->attributes);
	if (attrs == NULL)
	    return;
    } else {
	attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    
    CFDictionaryRef replacementAttrs = HeimCredMessageCopyAttributes(request, "attributes");
    if (replacementAttrs == NULL) {
	CFRelease(attrs);
	return;
    }
    CFDictionaryApplyFunction(replacementAttrs, updateCred, attrs);
    CFRELEASE_NULL(replacementAttrs);

    /* make sure the current caller is on the ACL list */
    addPeerToACL(peer, attrs);

    CFRELEASE_NULL(cred->attributes);
    cred->attributes = attrs;
}

static void
do_Fetch(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef uuid = HeimCredCopyUUID(request, "uuid");
    if (uuid == NULL)
	return;

    if (!checkACLInCredentialChain(peer, uuid, NULL))
	return;

    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(HeimCredCTX.items, uuid);
    CFRelease(uuid);
    if (cred == NULL)
	return;
    
    /* XXX filter the attributes */
    
    HeimCredMessageSetAttributes(reply, "attributes", cred->attributes);
}

static void
do_Query(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFArrayRef items = QueryCopy(peer, request, "query");
    if (items == NULL)
	return;
    
    CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (array == NULL) {
	CFRELEASE_NULL(items);
	return;
    }
    
    CFIndex n, count = CFArrayGetCount(items);
    for (n = 0; n < count; n++) {
	HeimCredRef cred = (HeimCredRef)CFArrayGetValueAtIndex(items, n);
	CFArrayAppendValue(array, cred->uuid);
    }
    CFRELEASE_NULL(items);
    
    HeimCredMessageSetAttributes(reply, "items", array);
    CFRELEASE_NULL(array);
}

static void
do_Move(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef from = (CFUUIDRef)HeimCredMessageCopyAttributes(request, "from");
    CFUUIDRef to = (CFUUIDRef)HeimCredMessageCopyAttributes(request, "to");

    if (!checkACLInCredentialChain(peer, from, NULL) || !checkACLInCredentialChain(peer, to, NULL))
	return;

    HeimCredRef credfrom = (HeimCredRef)CFDictionaryGetValue(HeimCredCTX.items, from);
    HeimCredRef credto = (HeimCredRef)CFDictionaryGetValue(HeimCredCTX.items, to);
    
    CFMutableDictionaryRef newattrs = CFDictionaryCreateMutableCopy(NULL, 0, credfrom->attributes);
    CFDictionaryRemoveValue(HeimCredCTX.items, from);
    credfrom = NULL;

    CFDictionarySetValue(newattrs, kHEIMAttrUUID, to);
    CFDictionarySetValue(newattrs, kHEIMAttrCredentialGroup, to);
    
    if (credto == NULL) {
	credto = HeimCredCreateItem(to);
	if (credto == NULL) abort();
	credto->attributes = newattrs;
	CFDictionarySetValue(HeimCredCTX.items, credto->uuid, credto);
	CFRelease(credto);

    } else {
	CFUUIDRef parentUUID = CFDictionaryGetValue(credto->attributes, kHEIMAttrParentCredential);
	if (parentUUID)
	    CFDictionarySetValue(newattrs, kHEIMAttrParentCredential, parentUUID);
	CFRELEASE_NULL(credto->attributes);
	credto->attributes = newattrs;
    }
    
    /*
     * delete all child entries for to UUID
     */
    DeleteChildren(to);
    
    /* 
     * update all child entries for from UUID
     */
    struct fromto fromto = {
	.from = from,
	.to = to,
    };
    CFDictionaryApplyFunction(HeimCredCTX.items, UpdateParent, &fromto);
}


static void GSSCred_peer_event_handler(struct peer *peer, xpc_object_t event)
{
    xpc_object_t reply  = NULL;
    xpc_type_t type = xpc_get_type(event);
    if (type == XPC_TYPE_ERROR)
	return;
    
    assert(type == XPC_TYPE_DICTIONARY);
    
    const char *cmd = xpc_dictionary_get_string(event, "command");
    if (cmd == NULL) {
	xpc_connection_cancel(peer->peer);
    } else if (strcmp(cmd, "wakeup") == 0) {

    } else if (strcmp(cmd, "create") == 0) {
	reply = xpc_dictionary_create_reply(event);
	do_CreateCred(peer, event, reply);
    } else if (strcmp(cmd, "delete") == 0) {
	reply = xpc_dictionary_create_reply(event);
	do_Delete(peer, event, reply);
    } else if (strcmp(cmd, "setattributes") == 0) {
	reply = xpc_dictionary_create_reply(event);
	do_SetAttrs(peer, event, reply);
    } else if (strcmp(cmd, "fetch") == 0) {
	reply = xpc_dictionary_create_reply(event);
	do_Fetch(peer, event, reply);
    } else if (strcmp(cmd, "move") == 0) {
	reply = xpc_dictionary_create_reply(event);
	do_Move(peer, event, reply);
    } else if (strcmp(cmd, "query") == 0) {
	reply = xpc_dictionary_create_reply(event);
	do_Query(peer, event, reply);
    } else if (strcmp(cmd, "retain-transient") == 0) {
	reply = xpc_dictionary_create_reply(event);
    } else if (strcmp(cmd, "release-transient") == 0) {
	reply = xpc_dictionary_create_reply(event);
    } else {
	syslog(LOG_ERR, "peer sent invalid command %s", cmd);
	xpc_connection_cancel(peer->peer);
    }

    if (reply) {
	xpc_connection_send_message(peer->peer, reply);
	xpc_release(reply);
    }

    storeCredCache();
}

static void
peer_final(void *ptr)
{
    struct peer *peer = ptr;
    CFRELEASE_NULL(peer->bundleID);
}

static CFStringRef
CopySigningIdentitier(xpc_connection_t conn)
{
	uint8_t header[8] = { 0 };
	uint32_t len;
	audit_token_t audit_token;
	pid_t pid;
	
	pid = xpc_connection_get_pid(conn);
	xpc_connection_get_audit_token(conn, &audit_token);

	int rcent = csops_audittoken(pid, CS_OPS_IDENTITY, header, sizeof(header), &audit_token);
	if (rcent != -1 || errno != ERANGE)
	    return NULL;

		
	memcpy(&len, &header[4], 4);
	len = ntohl(len);
	if (len > 1024 * 1024)
	    return NULL;
	else if (len == 0)
	    return NULL;

	uint8_t buffer[len];
		
	rcent = csops_audittoken(pid, CS_OPS_IDENTITY, buffer, len, &audit_token);
	if (rcent != 0)
	    return NULL;
	
	char *p = (char *)buffer;
	if (len > 8) {
	    p += 8;
	    len -= 8;
	} else
	    return NULL;

	return CFStringCreateWithBytes(NULL, (UInt8 *)p, len, kCFStringEncodingUTF8, false);
}

static void GSSCred_event_handler(xpc_connection_t peerconn)
{
    struct peer *peer;

    peer = malloc(sizeof(*peer));
    if (peer == NULL) abort();
    
    peer->peer = peerconn;
    peer->bundleID = CopySigningIdentitier(peerconn);
    if (peer->bundleID == NULL) {
	free(peer);
	xpc_connection_cancel(peerconn);
	return;
    }

    xpc_connection_set_context(peerconn, peer);
    xpc_connection_set_finalizer_f(peerconn, peer_final);

    xpc_connection_set_event_handler(peerconn, ^(xpc_object_t event) {
	GSSCred_peer_event_handler(peer, event);
    });
    xpc_connection_resume(peerconn);
}

int main(int argc, const char *argv[])
{
#if TARGET_OS_EMBEDDED
    char *error = NULL;

    if (sandbox_init("com.apple.GSSCred", SANDBOX_NAMED, &error)) {
	syslog(LOG_ERR, "failed to enter sandbox: %s", error);
	exit(EXIT_FAILURE);
    }
#endif

#if TARGET_IPHONE_SIMULATOR
    archivePath = [[NSString alloc] initWithFormat:@"%@/Library/Caches/com.apple.GSSCred.simulator-archive", NSHomeDirectory()];
#elif TARGET_OS_EMBEDDED
    archivePath = @"/var/db/heim-credential-store.archive";
#else
    archivePath = [[NSString alloc] initWithFormat:@"%@/Library/Caches/com.apple.GSSCred.archive", NSHomeDirectory()];
#endif

    _HeimCredInitCommon();
    readCredCache();
    xpc_main(GSSCred_event_handler);
    return 0;
}
