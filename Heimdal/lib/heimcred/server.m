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
#import <servers/bootstrap.h>
#import <bsm/audit.h>
#import <bsm/audit_session.h>
#import <bsm/libbsm.h>
#import <xpc/xpc.h>
#import <xpc/private.h>
#import <libproc.h>
#import <syslog.h>
#import <sandbox.h>
#import <launch.h>
#import <launch_priv.h>

#import <dispatch/dispatch.h>
#import <notify.h>
#import <mach/mach.h>
#import <mach/mach_time.h>

#import <roken.h>

#import "HeimCredCoder.h"
#import "heimcred.h"
#import "common.h"
#import "heimbase.h"
#import "hc_err.h"

/*
 *
 */

static bool validateObject(CFDictionaryRef, CFErrorRef *);
static void addErrorToReply(xpc_object_t, CFErrorRef);
static CFTypeRef GetValidatedValue(CFDictionaryRef, CFStringRef, CFTypeID, CFErrorRef *);
static void HCMakeError(CFErrorRef *, CFIndex,  const void *const *, const void *const *, CFIndex);
static CFTypeID getSessionTypeID(void);
static struct HeimSession *HeimCredCopySession(int);
static void handleDefaultCredentialUpdate(struct HeimSession *, HeimCredRef, CFDictionaryRef);


/*
 *
 */
struct HeimSession {
    CFRuntimeBase runtime;
    uid_t session;
    CFMutableDictionaryRef items;
    CFMutableDictionaryRef defaultCredentials;
    int updateDefaultCredential;
};

/*
 *
 */
struct HeimMech {
    CFRuntimeBase runtime;
    CFStringRef name;
    HeimCredStatusCallback statusCallback;
    HeimCredAuthCallback authCallback;
};



static NSString *archivePath = NULL;
static dispatch_queue_t runQueue;

static void
FlattenCredential(const void *key, const void *value, void *context)
{
    NSMutableDictionary *flatten = (NSMutableDictionary *)context;
    HeimCredRef cred = (HeimCredRef)value;
    id nskey = [HeimCredDecoder copyCF2NS:key];
    id attrs = [HeimCredDecoder copyCF2NS:cred->attributes];
    [flatten setObject:attrs forKey:nskey];
    [nskey release];
    [attrs release];
}

static void
FlattenSession(const void *key, const void *value, void *context)
{
    NSMutableDictionary *sf = (NSMutableDictionary *)context;
    id cf = NULL;
    @try {
	struct HeimSession *session = (struct HeimSession *)value;
	cf = [[NSMutableDictionary alloc] init];
	CFDictionaryApplyFunction(session->items, FlattenCredential, cf);
	[sf setObject:cf forKey:[NSNumber numberWithInt:(int)session->session]];
    } @catch(NSException * __unused e) {
    } @finally {
	[cf release];
    }
}


static void
storeCredCache(void)
{
    id sf = NULL;
    @try {
	sf = [[NSMutableDictionary alloc] init];
	CFDictionaryApplyFunction(HeimCredCTX.sessions, FlattenSession, sf);
	[HeimCredDecoder archiveRootObject:sf toFile:archivePath];
    } @catch(NSException * __unused e) {
    } @finally {
	[sf release];
    }
}

static uint64_t
relative_nano_time(void)
{
     static uint64_t factor;
     uint64_t now;

    now = mach_absolute_time();

    if (factor == 0) {
	mach_timebase_info_data_t base;
	(void)mach_timebase_info(&base);
	factor = base.numer / base.denom;
    }

    return now * factor;
}

#define KRB5_KCM_NOTIFY_CACHE_CHANGED "com.apple.Kerberos.cache.changed"

static void
notifyChangedCaches(void)
{
    static uint64_t last_change;
    static int notify_pending;

#define NOTIFY_TIME_LIMIT (NSEC_PER_SEC / 2)

    dispatch_async(dispatch_get_main_queue(), ^{
	    uint64_t now, diff;

	    if (notify_pending)
		return;

	    now = relative_nano_time();
	    if (now < last_change)
		diff = NOTIFY_TIME_LIMIT;
	    else
		diff = now - last_change;

	    if (diff >= NOTIFY_TIME_LIMIT) {
		notify_post(KRB5_KCM_NOTIFY_CACHE_CHANGED);
		last_change = now;
	    } else {
		notify_pending = 1;
		/* wait up to deliver the event */
		dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NOTIFY_TIME_LIMIT - diff), dispatch_get_main_queue(), ^{
			notify_pending = 0;
			last_change = relative_nano_time();
			notify_post(KRB5_KCM_NOTIFY_CACHE_CHANGED);
		    });
	    }
	});
}

static bool
HeimCredAssignMech(HeimCredRef cred, CFDictionaryRef attributes)
{
    heim_assert(cred->mech == NULL, "HeimCredAssignMech already have a mech");

    CFErrorRef error = NULL;
    CFStringRef mechName = GetValidatedValue(attributes, kHEIMAttrType, CFStringGetTypeID(), &error);

    if (mechName) {
	struct HeimMech *mech = (struct HeimMech *)CFDictionaryGetValue(HeimCredCTX.mechanisms, mechName);
	if (mech) {
	    cred->mech = mech;
	    return true;
	}
    }
    CFRELEASE_NULL(error);
    return false;
}

static bool
sessionExists(pid_t asid)
{
    auditinfo_addr_t aia;
    aia.ai_asid = asid;

    if (audit_get_sinfo_addr(&aia, sizeof(aia)) == 0)
	return true;
    return false;
}

static void
readCredCache(void)
{
    @autoreleasepool {
	NSDictionary *sessions = NULL;
	@try {
	    sessions = [HeimCredDecoder copyUnarchiveObjectWithFileSecureEncoding:archivePath];
	    
	    if (!sessions || ![sessions isKindOfClass:[NSDictionary class]])
		return;
	    
	    [sessions enumerateKeysAndObjectsUsingBlock:^(id skey, id svalue, BOOL *sstop) {
		    int sessionID = [(NSNumber *)skey intValue];

		    if (!sessionExists(sessionID))
			return;

		    struct HeimSession *session = HeimCredCopySession(sessionID);
		    if (session == NULL)
			return;

		    NSDictionary *creds = (NSDictionary *)svalue;

		    if (!creds || ![creds isKindOfClass:[NSDictionary class]]) {
			CFRelease(session);
			return;
		    }

		    [creds enumerateKeysAndObjectsUsingBlock:^(id ckey, id cvalue, BOOL *cstop) {
			    CFUUIDRef cfkey = [HeimCredDecoder copyNS2CF:ckey];
			    CFDictionaryRef cfvalue = [HeimCredDecoder copyNS2CF:cvalue];
			    if (cfkey && cfvalue) {
				HeimCredRef cred = HeimCredCreateItem(cfkey);
				
				if (HeimCredAssignMech(cred, cfvalue)) {
				    cred->attributes = CFRetain(cfvalue);
				    CFDictionarySetValue(session->items, cred->uuid, cred);

				    handleDefaultCredentialUpdate(session, cred, cred->attributes);
				} else {
				    /* dropping cred if mech assignment failed */
				    CFRelease(cred);
				}
			    }
			    CFRELEASE_NULL(cfkey);
			    CFRELEASE_NULL(cfvalue);
			}];

		    CFRelease(session);
		}];
	} @catch(NSException *e) {
	    syslog(LOG_ERR, "readCredCache failed with: %s:%s", [[e name] UTF8String], [[e reason] UTF8String]);
	}
    }
}

struct peer {
    xpc_connection_t peer;
    CFStringRef bundleID;
    struct HeimSession *session;
};

/*
 * Check if the credential (uuid) have an ACL and while not having an
 * ACL, walk up the parent chain and check if any other parents have
 * an ACL.
 */

static bool
checkACLInCredentialChain(struct peer *peer, CFUUIDRef uuid, bool *hasACL)
{
    int max_depth = 10;
    bool res = false;

    if (hasACL)
	*hasACL = false;

    while (1) {
	HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
	CFUUIDRef parent;

	if (cred == NULL)
		goto pass;
        
	heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

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

#if !TARGET_OS_EMBEDDED
		if (CFEqual(prefix, CFSTR("*")))
		    goto pass;
#endif
		if (CFEqual(peer->bundleID, prefix))
		    goto pass;

		NSPredicate *wildCardMatch = [NSPredicate predicateWithFormat:@"self like %@", prefix];
		BOOL matchFound = [wildCardMatch evaluateWithObject:(NSString *)peer->bundleID];

		if (matchFound)
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
	CFRELEASE_NULL(a);
    }

    return true;
}

static void
updateStoreTime(HeimCredRef cred, CFMutableDictionaryRef attrs)
{
    CFDateRef date = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    if (date == NULL)
	return;
    CFDictionarySetValue(attrs, kHEIMAttrStoreTime, date);
    CFRelease(date);
}

static void
handleDefaultCredentialUpdate(struct HeimSession *session, HeimCredRef cred, CFDictionaryRef attrs)
{

    heim_assert(cred->mech != NULL, "mech is NULL, schame validation doesn't work ?");
    
    CFUUIDRef oldDefault = CFDictionaryGetValue(session->defaultCredentials, cred->mech->name);

    CFBooleanRef defaultCredential = CFDictionaryGetValue(attrs, kHEIMAttrDefaultCredential);
    if (defaultCredential == NULL || !CFBooleanGetValue(defaultCredential)) {
	if (oldDefault)
	    return;
    } else {	
	if (oldDefault) {
	    /*
	     * Drop marker on old credential
	     */
	    HeimCredRef oldCred = (HeimCredRef)CFDictionaryGetValue(session->items, oldDefault);
	    if (oldCred) {
		CFMutableDictionaryRef oldCredAttrs = CFDictionaryCreateMutableCopy(NULL, 0, oldCred->attributes);
		CFDictionaryRemoveValue(oldCredAttrs, kHEIMAttrDefaultCredential);
		CFRELEASE_NULL(oldCred->attributes);
		oldCred->attributes = oldCredAttrs;
	    }
	}
    }

    CFDictionarySetValue(session->defaultCredentials, cred->mech->name, cred->uuid);

    notifyChangedCaches();
}    
    
static void
handleDefaultCredentialDeletion(struct HeimSession *session, HeimCredRef cred)
{
    CFUUIDRef defaultCredential = CFDictionaryGetValue(session->defaultCredentials, cred->mech->name);

    if (defaultCredential && CFEqual(defaultCredential, cred->uuid))
	CFDictionaryRemoveValue(session->defaultCredentials, cred->mech->name);
    session->updateDefaultCredential = 1;
}

static void
reElectCredential(const void *key, const void *value, void *context)
{
    HeimCredRef cred = (HeimCredRef)value;
    struct HeimSession *session = (struct HeimSession *)context;

    if (CFDictionaryGetValue(session->defaultCredentials, cred->mech->name))
	return;
    if (CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential) != NULL)
	return;
    CFDictionarySetValue(session->defaultCredentials, cred->mech->name, cred->uuid);
}

static void
reElectMechCredential(const void *key, const void *value, void *context)
{
    struct HeimMech *mech = (struct HeimMech *)value;
    struct HeimSession *session = (struct HeimSession *)context;

    if (CFDictionaryGetValue(session->defaultCredentials, mech->name))
	return;

    CFDictionaryApplyFunction(session->items, reElectCredential, session);

    if (CFDictionaryGetValue(session->defaultCredentials, mech->name)) {
	HeimCredCTX.needFlush = 1;
    } else {
	/*
	 * If there is no default credential, make one up
	 */
	CFUUIDRef defcred = CFUUIDCreate(NULL);
	CFDictionarySetValue(session->defaultCredentials, mech->name, defcred);
	CFRelease(defcred);
    }

    notifyChangedCaches();
}

static void
reElectDefaultCredential(struct peer *peer)
{
    if (!peer->session->updateDefaultCredential)
	return;
    peer->session->updateDefaultCredential = 0;

    CFDictionaryApplyFunction(HeimCredCTX.mechanisms, reElectMechCredential, peer->session);
}

#if TARGET_OS_EMBEDDED
static bool
canSetAnyBundleIDACL(struct peer *peer)
{
    return CFStringCompare(CFSTR("com.apple.accountsd"), peer->bundleID, 0) == kCFCompareEqualTo;
}
#endif

static void
do_CreateCred(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFMutableDictionaryRef attrs = NULL;
    HeimCredRef cred = NULL;
    CFUUIDRef uuid = NULL;
    bool hasACL = false;
    CFErrorRef error = NULL;
    CFBooleanRef lead;
    
    CFDictionaryRef attributes = HeimCredMessageCopyAttributes(request, "attributes", CFDictionaryGetTypeID());
    if (attributes == NULL)
	goto out;
    
    if (!validateObject(attributes, &error)) {
	addErrorToReply(reply, error);
	goto out;
    }

    /* check if we are ok to link into this cred-tree */
    uuid = CFDictionaryGetValue(attributes, kHEIMAttrParentCredential);
    if (uuid != NULL && !checkACLInCredentialChain(peer, uuid, &hasACL))
	goto out;
    
    uuid = CFDictionaryGetValue(attributes, kHEIMAttrUUID);
    if (uuid) {
	CFRetain(uuid);

	if (CFGetTypeID(uuid) != CFUUIDGetTypeID())
	    goto out;

	if (CFDictionaryGetValue(peer->session->items, uuid) != NULL)
	    goto out;
	
    } else {
	uuid = CFUUIDCreate(NULL);
	if (uuid == NULL)
	    goto out;
    }
    cred = HeimCredCreateItem(uuid);
    if (cred == NULL)
	goto out;
    
    attrs = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
    if (attrs == NULL)
	goto out;

    bool hasACLInAttributes = addPeerToACL(peer, attrs);

#if TARGET_OS_EMBEDDED
    if (hasACLInAttributes && !canSetAnyBundleIDACL(peer)) {
       CFArrayRef array = CFDictionaryGetValue(attrs, kHEIMAttrBundleIdentifierACL);
       if (array == NULL || CFGetTypeID(array) != CFArrayGetTypeID() || CFArrayGetCount(array) != 1) {
           syslog(LOG_ERR, "peer sent more then one bundle id and is not accountsd");
           goto out;
       }
    }
#endif

    /*
     * make sure this cred-tree as an ACL Set the ACL to be the peer
     * on ios, and on osx, its "*" since that is the default policy.
     *
     * XXX should use the default keychain rule for appsandboxed peers
     * on osx.
     */
    if (!hasACL && !hasACLInAttributes) {
#if TARGET_OS_IPHONE
	const void *values[1] = { (void *)peer->bundleID };
#else
	const void *values[1] = { CFSTR("*") };
#endif
	CFArrayRef array = CFArrayCreate(NULL, values, sizeof(values)/sizeof(values[0]), &kCFTypeArrayCallBacks);
	heim_assert(array != NULL, "out of memory");
	CFDictionarySetValue(attrs, kHEIMAttrBundleIdentifierACL, array);
	CFRELEASE_NULL(array);
    }

    CFDictionarySetValue(attrs, kHEIMAttrUUID, uuid);
    
    if (!validateObject(attrs, &error)) {
	addErrorToReply(reply, error);
	goto out;
    }

    if (!HeimCredAssignMech(cred, attrs)) {
	goto out;
    }

    updateStoreTime(cred, attrs);
    handleDefaultCredentialUpdate(peer->session, cred, attrs);

    cred->attributes = CFRetain(attrs);
    
    CFDictionarySetValue(peer->session->items, cred->uuid, cred);
    notifyChangedCaches();
    HeimCredCTX.needFlush = 1;

    /*
     * If the default credential is unset or doesn't exists, switch to
     * the now just created lead credential.
     */

    heim_assert(cred->mech != NULL, "mech is NULL, schame validation doesn't work ?");

    CFUUIDRef defcred = CFDictionaryGetValue(peer->session->defaultCredentials, cred->mech->name);
    if ((defcred == NULL || CFDictionaryGetValue(peer->session->items, defcred) == NULL)
	&& (lead = CFDictionaryGetValue(cred->attributes, kHEIMAttrLeadCredential))
	&& CFBooleanGetValue(lead))
    {
	CFDictionarySetValue(peer->session->defaultCredentials, cred->mech->name, cred->uuid);
    }
    
    HeimCredMessageSetAttributes(reply, "attributes", cred->attributes);
out:
    CFRELEASE_NULL(attrs);
    CFRELEASE_NULL(attributes);
    CFRELEASE_NULL(cred);
    CFRELEASE_NULL(uuid);
    CFRELEASE_NULL(error);
}

struct MatchCTX {
    struct peer *peer;
    CFMutableArrayRef results;
    CFDictionaryRef query;
    CFIndex numQueryItems;
    CFDictionaryRef attributes;
    CFIndex count;
};

static void
MatchQueryItem(const void *key, const void *value, void *context)
{
    struct MatchCTX * mc = (struct MatchCTX *)context;
    heim_assert(mc->attributes != NULL, "attributes NULL in MatchQueryItem");

    /*
     * Exact matching for each rule, kCFNull matches when key is not set.
     */

    CFTypeRef val = CFDictionaryGetValue(mc->attributes, key);
    if (val == NULL) {
	if (!CFEqual(kCFNull, value))
	    return;
    } else if (!CFEqual(val, value))
	return;
    mc->count++;
}

static void
MatchQueryCred(const void *key, const void *value, void *context)
{
    struct MatchCTX *mc = (struct MatchCTX *)context;
    CFUUIDRef uuid = (CFUUIDRef)key;

    if (!checkACLInCredentialChain(mc->peer, uuid, NULL))
	return;

    HeimCredRef cred = (HeimCredRef)value;
    heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

    if (cred->attributes == NULL)
	return;
    mc->attributes = cred->attributes;
    mc->count = 0;
    CFDictionaryApplyFunction(mc->query, MatchQueryItem, mc);
    heim_assert(mc->numQueryItems >= mc->count, "cant have matched more then number of queries");

    /* found a match */
    if (mc->numQueryItems == mc->count)
	CFArrayAppendValue(mc->results, cred);
}

static void
MatchQuerySchema(const void *key, const void *value, void *context)
{
    struct MatchCTX *mc = (struct MatchCTX *)context;

    CFDictionaryRef schema = (CFDictionaryRef)value;
    heim_assert(CFGetTypeID(schema) == CFDictionaryGetTypeID(), "schema wrong type");

    mc->attributes = schema;
    mc->count = 0;
    CFDictionaryApplyFunction(mc->query, MatchQueryItem, mc);
    heim_assert(mc->numQueryItems >= mc->count, "cant have matched more then number of queries");

    /* found a match */
    if (mc->numQueryItems == mc->count)
	CFArrayAppendValue(mc->results, schema);
}


static CFMutableArrayRef
QueryCopy(struct peer *peer, xpc_object_t request, const char *key)
{
    struct MatchCTX mc = {
	.peer = peer,
	.results = NULL,
	.query = NULL,
	.attributes = NULL,
    };
    CFUUIDRef uuidref = NULL;
    CFErrorRef error = NULL;
    
    mc.query = HeimCredMessageCopyAttributes(request, key, CFDictionaryGetTypeID());
    if (mc.query == NULL)
	goto out;

    mc.numQueryItems = CFDictionaryGetCount(mc.query);

    if (mc.numQueryItems == 1) {
	uuidref = CFDictionaryGetValue(mc.query, kHEIMAttrUUID);
	if (uuidref && CFGetTypeID(uuidref) == CFUUIDGetTypeID()) {
	
	    if (!checkACLInCredentialChain(peer, uuidref, NULL))
		goto out;

	    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuidref);
	    if (cred == NULL)
		goto out;

	    heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

	    mc.results = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    CFArrayAppendValue(mc.results, cred);

	    goto out;
	}
    }
    
    mc.results = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    CFStringRef type = GetValidatedValue(mc.query, kHEIMAttrType, CFStringGetTypeID(), &error);
    if (CFEqual(type, kHEIMTypeSchema)) 
	CFDictionaryApplyFunction(HeimCredCTX.schemas, MatchQuerySchema, &mc);
    else
	CFDictionaryApplyFunction(peer->session->items, MatchQueryCred, &mc);
    
out:
    CFRELEASE_NULL(error);
    CFRELEASE_NULL(mc.query);
    return mc.results;
}

struct child {
    CFUUIDRef parent;
    CFMutableArrayRef array;
    struct HeimSession *session;
};

static void
DeleteChild(const void *key, const void *value, void *context)
{
    struct child *child = (struct child  *)context;

    if (CFEqual(key, child->parent))
	return;

    HeimCredRef cred = (HeimCredRef)value;
    heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

    CFUUIDRef parent = CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential);
    if (parent && CFEqual(child->parent, parent)) {
	CFArrayAppendValue(child->array, cred->uuid);

	handleDefaultCredentialDeletion(child->session, cred);

	/*
	 * Recurse over grandchildren too
	 */
	struct child grandchildren = {
	    .parent = key,
	    .array = child->array,
	    .session = child->session,
	};
	CFDictionaryApplyFunction(child->session->items, DeleteChild, &grandchildren);
    }
}

static void
DeleteChildrenApplier(const void *value, void *context)
{
    struct HeimSession *session = (struct HeimSession *)context;
    heim_assert(CFGetTypeID(value) == CFUUIDGetTypeID(), "Value not an CFUUIDRef");
    CFDictionaryRemoveValue(session->items, value);
}


static void
DeleteChildren(struct HeimSession *session, CFUUIDRef parent)
{
    /*
     * delete all child entries for to UUID
     */
    struct child child = {
	.parent = parent,
	.array = NULL,
	.session = session,
    };
    child.array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFDictionaryApplyFunction(session->items, DeleteChild, &child);
    CFArrayApplyFunction(child.array, CFRangeMake(0, CFArrayGetCount(child.array)), DeleteChildrenApplier, session);
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
    heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

    CFUUIDRef parent = CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential);
    if (parent && CFEqual(parent, fromto->from)) {
	CFMutableDictionaryRef attrs = CFDictionaryCreateMutableCopy(NULL, 0, cred->attributes);
	CFRelease(cred->attributes);
	CFDictionarySetValue(attrs, kHEIMAttrParentCredential, fromto->to);
	cred->attributes = attrs;
    }
}




static void
do_Delete(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFErrorRef error = NULL;
    
    CFArrayRef items = QueryCopy(peer, request, "query");
    if (items == NULL || CFArrayGetCount(items) == 0) {
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(&error, kHeimCredErrorNoItemsMatchesQuery, keys, values, 1);
	goto out;
    }
    
    CFIndex n, count = CFArrayGetCount(items);
    for (n = 0; n < count; n++) {
	HeimCredRef cred = (HeimCredRef)CFArrayGetValueAtIndex(items, n);
	heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");
	
	CFDictionaryRemoveValue(peer->session->items, cred->uuid);
	DeleteChildren(peer->session, cred->uuid);
    }

    notifyChangedCaches();
    HeimCredCTX.needFlush = 1;
    
 out:
    if (error) {
	addErrorToReply(reply, error);
	CFRelease(error);
    }
    CFRELEASE_NULL(items);
}

static void
updateCred(const void *key, const void *value, void *context)
{
    CFMutableDictionaryRef attrs = (CFMutableDictionaryRef)context;
    CFDictionarySetValue(attrs, key, value);
}


static void
do_SetAttrs(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef uuid = HeimCredCopyUUID(request, "uuid");
    CFMutableDictionaryRef attrs;
    CFErrorRef error = NULL;
    
    if (uuid == NULL)
	return;

    if (!checkACLInCredentialChain(peer, uuid, NULL)) {
	CFRelease(uuid);
	return;
    }
    
    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
    CFRelease(uuid);
    if (cred == NULL)
	return;
    
    heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

    if (cred->attributes) {
	attrs = CFDictionaryCreateMutableCopy(NULL, 0, cred->attributes);
	if (attrs == NULL)
	    return;
    } else {
	attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    
    CFDictionaryRef replacementAttrs = HeimCredMessageCopyAttributes(request, "attributes", CFDictionaryGetTypeID());
    if (replacementAttrs == NULL) {
	CFRelease(attrs);
	goto out;
    }

    CFDictionaryApplyFunction(replacementAttrs, updateCred, attrs);
    CFRELEASE_NULL(replacementAttrs);

    if (!validateObject(attrs, &error)) {
	addErrorToReply(reply, error);
	goto out;
    }

    handleDefaultCredentialUpdate(peer->session, cred, attrs);

    /* make sure the current caller is on the ACL list */
    addPeerToACL(peer, attrs);
    
    CFRELEASE_NULL(cred->attributes);
    cred->attributes = attrs;
  out:
    CFRELEASE_NULL(error);
}

static void
do_Fetch(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef uuid = HeimCredCopyUUID(request, "uuid");
    if (uuid == NULL)
	return;

    if (!checkACLInCredentialChain(peer, uuid, NULL)) {
	CFRelease(uuid);
	return;
    }

    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
    CFRelease(uuid);
    if (cred == NULL)
	return;
    
    /* XXX filter the attributes */
    
    HeimCredMessageSetAttributes(reply, "attributes", cred->attributes);
}

static CFComparisonResult
orderLeadFirst(const void *val1, const void *val2, void *context)
{
    HeimCredRef cred1 = (HeimCredRef)val1;
    HeimCredRef cred2 = (HeimCredRef)val2;

    CFTypeRef uuid1 = CFDictionaryGetValue(cred1->attributes, kHEIMAttrParentCredential);
    CFTypeRef uuid2 = CFDictionaryGetValue(cred2->attributes, kHEIMAttrParentCredential);

    if (uuid1 && uuid2 && CFEqual(uuid1, uuid2)) {
	CFBooleanRef lead1 = CFDictionaryGetValue(cred1->attributes, kHEIMAttrLeadCredential);
	CFBooleanRef lead2 = CFDictionaryGetValue(cred2->attributes, kHEIMAttrLeadCredential);

	if (lead1 && CFBooleanGetValue(lead1))
	    return kCFCompareLessThan;
	if (lead2 && CFBooleanGetValue(lead2))
	    return kCFCompareGreaterThan;
    }

    CFErrorRef error = NULL;

    CFDateRef authTime1 = GetValidatedValue(cred1->attributes, kHEIMAttrStoreTime, CFDateGetTypeID(), &error);
    CFRELEASE_NULL(error);
    CFDateRef authTime2 = GetValidatedValue(cred2->attributes, kHEIMAttrStoreTime, CFDateGetTypeID(), &error);
    CFRELEASE_NULL(error);
    
    if (authTime1 && authTime2)
	return CFDateCompare(authTime1, authTime2, NULL);

    CFIndex hash1 = CFHash(cred1->uuid);
    CFIndex hash2 = CFHash(cred2->uuid);

    if (hash1 < hash2)
	return kCFCompareLessThan;
    
    return kCFCompareGreaterThan;
}


static void
do_Query(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFMutableArrayRef array = NULL;
    CFErrorRef error = NULL;

    reElectDefaultCredential(peer);

    CFMutableArrayRef items = QueryCopy(peer, request, "query");
    if (items == NULL) {
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(&error, kHeimCredErrorNoItemsMatchesQuery, keys, values, 1);
	goto out;
    }
    
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (array == NULL) {
	goto out;
    }
    
    CFIndex n, count = CFArrayGetCount(items);

    if (count > 1)
	CFArraySortValues(items, CFRangeMake(0, count), orderLeadFirst, NULL);

    for (n = 0; n < count; n++) {
	HeimCredRef cred = (HeimCredRef)CFArrayGetValueAtIndex(items, n);
	CFArrayAppendValue(array, cred->uuid);
    }
    CFRELEASE_NULL(items);
    
    HeimCredMessageSetAttributes(reply, "items", array);

    out:
    if (error) {
	addErrorToReply(reply, error);
	CFRelease(error);
    }
    CFRELEASE_NULL(array);
    CFRELEASE_NULL(items);
}

static void
do_GetDefault(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFErrorRef error = NULL;

    reElectDefaultCredential(peer);

    CFStringRef mechName = HeimCredMessageCopyAttributes(request, "mech", CFStringGetTypeID());
    if (mechName == NULL) {
	HCMakeError(&error, kHeimCredErrorNoItemsMatchesQuery, NULL, NULL, 0);
	goto out;
    }
    
    CFUUIDRef defCred = CFDictionaryGetValue(peer->session->defaultCredentials, mechName);
    if (defCred == NULL) {
	/*
	 * If there is is no credential, try to elect one
	 */
	peer->session->updateDefaultCredential = 1;
	reElectDefaultCredential(peer);

	defCred = CFDictionaryGetValue(peer->session->defaultCredentials, mechName);
	if (defCred == NULL) {
	    HCMakeError(&error, kHeimCredErrorNoItemsMatchesQuery, NULL, NULL, 0);
	    goto out;
	}
    }

    HeimCredMessageSetAttributes(reply, "default", defCred);
 out:
     if (error) {
	 addErrorToReply(reply, error);
	 CFRelease(error);
     }
}

static void
do_Move(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef from = HeimCredMessageCopyAttributes(request, "from", CFUUIDGetTypeID());
    CFUUIDRef to = HeimCredMessageCopyAttributes(request, "to", CFUUIDGetTypeID());
    
    if (from == NULL || to == NULL) {
	CFRELEASE_NULL(from);
	CFRELEASE_NULL(to);
	return;
    }

    if (!checkACLInCredentialChain(peer, from, NULL) || !checkACLInCredentialChain(peer, to, NULL)) {
	CFRelease(from);
	CFRelease(to);
	return;
    }
    
    HeimCredRef credfrom = (HeimCredRef)CFDictionaryGetValue(peer->session->items, from);
    HeimCredRef credto = (HeimCredRef)CFDictionaryGetValue(peer->session->items, to);
    
    if (credfrom == NULL) {
	CFRelease(from);
	CFRelease(to);
	return;
    }


    CFMutableDictionaryRef newattrs = CFDictionaryCreateMutableCopy(NULL, 0, credfrom->attributes);
    CFDictionaryRemoveValue(peer->session->items, from);
    credfrom = NULL;

    CFDictionarySetValue(newattrs, kHEIMAttrUUID, to);
    
    if (credto == NULL) {
	credto = HeimCredCreateItem(to);
	heim_assert(credto != NULL, "out of memory");

	HeimCredAssignMech(credto, newattrs);

	credto->attributes = newattrs;
	CFDictionarySetValue(peer->session->items, credto->uuid, credto);
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
    DeleteChildren(peer->session, to);
    
    /* 
     * update all child entries for from UUID
     */
    struct fromto fromto = {
	.from = from,
	.to = to,
    };
    CFDictionaryApplyFunction(peer->session->items, UpdateParent, &fromto);

    notifyChangedCaches();
    HeimCredCTX.needFlush = 1;
}

static void
StatusCredential(const void *key, const void *value, void *context)
{
    HeimCredRef cred = (HeimCredRef)value;
    xpc_object_t sc = (xpc_object_t)context;
    char *s;

    if (CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential) != NULL)
	return;

    xpc_object_t xc = xpc_dictionary_create(NULL, NULL, 0);

    CFStringRef us = CFUUIDCreateString(NULL, cred->uuid);
    s = rk_cfstring2cstring(us);
    CFRelease(us);

    xpc_dictionary_set_value(sc, s, xc);
    xpc_release(xc);
    free(s);

    CFStringRef name = CFDictionaryGetValue(cred->attributes, kHEIMAttrClientName);
    if (name) {
	s = rk_cfstring2cstring(name);
	xpc_dictionary_set_string(xc, "name", s);
	free(s);
    }

    s = rk_cfstring2cstring(cred->mech->name);
    xpc_dictionary_set_string(xc, "mech", s);
    free(s);
}

static void
StatusSession(const void *key, const void *value, void *context)
{
    struct HeimSession *session = (struct HeimSession *)value;
    xpc_object_t ss = (xpc_object_t)context;
    xpc_object_t sc = xpc_dictionary_create(NULL, NULL, 0);
    CFDictionaryApplyFunction(session->items, StatusCredential, sc);
    CFStringRef sessionID = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@"), key);
    char *s = rk_cfstring2cstring(sessionID);
    xpc_dictionary_set_value(ss, s, sc);
    free(s);
    xpc_release(sc);
    CFRelease(sessionID);
}

static void
do_Status(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    uid_t uid = xpc_connection_get_euid(peer->peer);
    struct stat sb;

    if (uid == 0) {
	xpc_object_t ss = xpc_dictionary_create(NULL, NULL, 0);

	CFDictionaryApplyFunction(HeimCredCTX.sessions, StatusSession, ss);
	xpc_dictionary_set_value(reply, "items", ss);
	xpc_release(ss);
    }

    if (stat([archivePath UTF8String], &sb) == 0) {
	xpc_dictionary_set_int64(reply, "cache-size", (int64_t)sb.st_size);
    } else {
	xpc_dictionary_set_int64(reply, "cache-size", 0);
    }
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
	syslog(LOG_ERR, "peer sent invalid no command");
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
    } else if (strcmp(cmd, "default") == 0) {
	reply = xpc_dictionary_create_reply(event);
	do_GetDefault(peer, event, reply);
    } else if (strcmp(cmd, "retain-transient") == 0) {
	reply = xpc_dictionary_create_reply(event);
    } else if (strcmp(cmd, "release-transient") == 0) {
	reply = xpc_dictionary_create_reply(event);
    } else if (strcmp(cmd, "status") == 0) {
	reply = xpc_dictionary_create_reply(event);
	do_Status(peer, event, reply);
    } else {
	syslog(LOG_ERR, "peer sent invalid command %s", cmd);
	xpc_connection_cancel(peer->peer);
    }

    if (reply) {
	xpc_connection_send_message(peer->peer, reply);
	xpc_release(reply);
    }

    if (HeimCredCTX.needFlush) {
	if (!HeimCredCTX.flushPending) {
	    HeimCredCTX.needFlush = false;
	} else {
	    HeimCredCTX.flushPending = true;
	    
	    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC), runQueue, ^{
	        @autoreleasepool {
		    HeimCredCTX.flushPending = false;
		    storeCredCache();
		}
	    });
	}
    }
}

static void
peer_final(void *ptr)
{
    struct peer *peer = ptr;
    CFRELEASE_NULL(peer->bundleID);
    CFRELEASE_NULL(peer->session);
}

static CFStringRef
CopySigningIdentitier(xpc_connection_t conn)
{
#if TARGET_IPHONE_SIMULATOR
	char path[MAXPATHLEN];
	CFStringRef ident = NULL;
	const char *str = NULL;

	/* simulator binaries are not codesigned, fake it */
	if (proc_pidpath(getpid(), path, sizeof(path)) > 0) {
		xpc_bundle_t bundle = xpc_bundle_create(path, XPC_BUNDLE_FROM_PATH);
		if (bundle) {
			xpc_object_t xdict = xpc_bundle_get_info_dictionary(bundle);
			if (xdict) {
				str = xpc_dictionary_get_string(xdict, "CFBundleIdentifier");
				if (str)
					ident = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), str);
			}
			xpc_release(bundle);
		}
		/*
		 * If not a bundle, its a command line tool, lets use com.apple.$(basename)
		 */
		if (ident == NULL) {
			str = strrchr(path, '/');
			if (str) {
				str++;
				ident = CFStringCreateWithFormat(NULL, NULL, CFSTR("com.apple.%s"), str);
			}
		}
	}
	if (ident == NULL)
		ident = CFSTR("iphonesimulator");
	return ident;
#else
	CFStringRef res;
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

	uint8_t *buffer = malloc(len);
	if (buffer == NULL)
	    return NULL;
		
	rcent = csops_audittoken(pid, CS_OPS_IDENTITY, buffer, len, &audit_token);
	if (rcent != 0) {
	    free(buffer);
	    return NULL;
	}
	
	char *p = (char *)buffer;
	if (len > 8) {
	    p += 8;
	    len -= 8;
	} else
	    return NULL;

	if (p[len - 1] != '\0') {
	    free(buffer);
	    return NULL;
	}

	res = CFStringCreateWithBytes(NULL, (UInt8 *)p, len - 1, kCFStringEncodingUTF8, false);
	free(buffer);
	return res;
#endif
}

/*
 *
 */

static struct HeimSession *
HeimCredCopySession(int sessionID)
{
    struct HeimSession *session;
    CFNumberRef sid;

    sid = CFNumberCreate(NULL, kCFNumberIntType, &sessionID);
    heim_assert(sid != NULL, "out of memory");


    session = (struct HeimSession *)CFDictionaryGetValue(HeimCredCTX.sessions, sid);
    if (session) {
	CFRelease(sid);
	CFRetain(session);
	return session;
    }
    
    CFTypeID sessionTID = getSessionTypeID();

    heim_assert(sessionTID != _kCFRuntimeNotATypeID, "could not register cftype");

    session = (struct HeimSession *)_CFRuntimeCreateInstance(NULL, sessionTID, sizeof(struct HeimSession) - sizeof(CFRuntimeBase), NULL);
    heim_assert(session != NULL, "out of memory while registering HeimMech instance");

    session->session = sessionID;
    session->items = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    session->defaultCredentials = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    session->updateDefaultCredential = 0;

    CFDictionarySetValue(HeimCredCTX.sessions, sid, session);
    CFRelease(sid);

    return session;
}

/*
 *
 */

static void GSSCred_event_handler(xpc_connection_t peerconn)
{
    struct peer *peer;

    peer = malloc(sizeof(*peer));
    heim_assert(peer != NULL, "out of memory");
    
    peer->peer = peerconn;
    peer->bundleID = CopySigningIdentitier(peerconn);
    if (peer->bundleID == NULL) {
	char path[MAXPATHLEN];

	if (proc_pidpath(getpid(), path, sizeof(path)) <= 0)
	    path[0] = '\0';

	syslog(LOG_ERR, "client[pid-%d] \"%s\" is not signed", (int)xpc_connection_get_pid(peerconn), path);
#if TARGET_OS_EMBEDDED
	free(peer);
	xpc_connection_cancel(peerconn);
	return;
#else
	peer->bundleID = CFStringCreateWithFormat(NULL, NULL, CFSTR("unsigned-binary-path:%s-end"), path);
	heim_assert(peer->bundleID != NULL, "out of memory");
#endif
    }
    peer->session = HeimCredCopySession(xpc_connection_get_asid(peerconn));
    heim_assert(peer->session != NULL, "out of memory");

    xpc_connection_set_context(peerconn, peer);
    xpc_connection_set_finalizer_f(peerconn, peer_final);

    xpc_connection_set_event_handler(peerconn, ^(xpc_object_t event) {
	GSSCred_peer_event_handler(peer, event);
    });
    xpc_connection_resume(peerconn);
}

static void
addErrorToReply(xpc_object_t reply, CFErrorRef error)
{
    if (error == NULL)
	return;

    xpc_object_t xe = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_int64(xe, "error-code", CFErrorGetCode(error));
    xpc_dictionary_set_value(reply, "error", xe);
    xpc_release(xe);
}

/*
 *
 */

static CFStringRef
debug_session(CFTypeRef cf)
{
    return CFSTR("debugsession");
}

static void
release_session(CFTypeRef cf)
{
    struct HeimSession *session = (struct HeimSession *)cf;
    CFRELEASE_NULL(session->items);
    CFRELEASE_NULL(session->defaultCredentials);
}

static CFTypeID
getSessionTypeID(void)
{
    static CFTypeID haid = _kCFRuntimeNotATypeID;
    static dispatch_once_t inited;

    dispatch_once(&inited, ^{
	    static const CFRuntimeClass HeimCredSessionClass = {
		0,
		"HeimCredSession",
		NULL,
		NULL,
		release_session,
		NULL,
		NULL,
		NULL,
		debug_session
	    };
	    haid = _CFRuntimeRegisterClass(&HeimCredSessionClass);
	});
    return haid;
}

/*
 *
 */

static CFStringRef
debug_mech(CFTypeRef cf)
{
    return CFSTR("debugmech");
}

static void
release_mech(CFTypeRef cf)
{
    struct HeimMech *mech = (struct HeimMech *)cf;
    CFRELEASE_NULL(mech->name);
}

static CFTypeID
getMechTypeID(void)
{
    static CFTypeID haid = _kCFRuntimeNotATypeID;
    static dispatch_once_t inited;

    dispatch_once(&inited, ^{
	    static const CFRuntimeClass HeimCredMechanismClass = {
		0,
		"HeimCredMechanism",
		NULL,
		NULL,
		release_mech,
		NULL,
		NULL,
		NULL,
		debug_mech
	    };
	    haid = _CFRuntimeRegisterClass(&HeimCredMechanismClass);
	});
    return haid;
}

/*
 *
 */

static CFTypeRef
GetValidatedValue(CFDictionaryRef object, CFStringRef key, CFTypeID requiredTypeID, CFErrorRef *error)
{
    heim_assert(error != NULL, "error ptr required");

    CFTypeRef value = CFDictionaryGetValue(object, key);
    if (value == NULL)
	return NULL;
    if (CFGetTypeID(value) != requiredTypeID)
	return NULL;
    return value;
}

struct validate {
    CFDictionaryRef schema;
    CFDictionaryRef object;
    CFTypeID subTypeID;
    CFErrorRef *error;
    bool valid;
};

#define kHeimCredErrorDomain CFSTR("com.apple.GSS.credential-store")

static void
HCMakeError(CFErrorRef *error, CFIndex code, 
	    const void *const *keys,
	    const void *const *values,
	    CFIndex num)
{
    if (*error != NULL)
	return;
    *error = CFErrorCreateWithUserInfoKeysAndValues(NULL, kHeimCredErrorDomain, code, keys, values, num);
}

static void
ValidateKey(const void *key, const void *value, void *context)
{
    struct validate *ctx = (struct validate *)context;
    CFStringRef rule;

    rule = GetValidatedValue(ctx->schema, key, CFStringGetTypeID(), ctx->error);
    if (rule == NULL) {
	const void *const keys[] = { CFSTR("Key"), CFSTR("CommonErrorCode") };
	const void *const values[] = { key, kCFBooleanTrue };
	HCMakeError(ctx->error, kHeimCredErrorUnknownKey, keys, values, 2);
	ctx->valid = false;
	syslog(LOG_ERR, "unknown key");
	return;
    }

    return;
}

static bool
StringContains(CFStringRef haystack, CFStringRef needle)
{
    if (CFStringFind(haystack, needle, 0).location == kCFNotFound)
	return false;
    return true;
}

static CFTypeID
GetTypeFromSchemaRule(CFStringRef key, CFStringRef rule, bool typeLevelType)
{
    if (typeLevelType && StringContains(rule, CFSTR("a")))
	return CFArrayGetTypeID();
    else if (StringContains(rule, CFSTR("s")))
	return CFStringGetTypeID();
    else if (StringContains(rule, CFSTR("u")))
	return CFUUIDGetTypeID();
    else if (StringContains(rule, CFSTR("d")))
	return CFDataGetTypeID();
    else if (StringContains(rule, CFSTR("b")))
	return CFBooleanGetTypeID();
    else if (StringContains(rule, CFSTR("t")))
	return CFDateGetTypeID();
    else if (StringContains(rule, CFSTR("n")))
	return CFNumberGetTypeID();

    heim_abort("key %s have a broken rule %s", rk_cfstring2cstring(rule), rk_cfstring2cstring(key));
}

static void
ValidateSubtype(const void *value, void *context)
{
    struct validate *ctx = (struct validate *)context;
    if (CFGetTypeID(value) != ctx->subTypeID) {
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(ctx->error, kHeimCredErrorUnknownKey, keys, values, 1);
	ctx->valid = false;
    }
}

static void
ValidateSchema(const void *key, const void *value, void *context)
{
    struct validate *ctx = (struct validate *)context;
    CFStringRef rule = value;
    CFTypeRef ov;

    if (CFEqual(kHEIMObjectType, key))
	return;

    ov = CFDictionaryGetValue(ctx->object, key);
    if (ov == NULL) {
	if (StringContains(rule, CFSTR("R"))) {
	    const void *const keys[] = { CFSTR("Key"), CFSTR("Rule"), CFSTR("CommonErrorCode") };
	    const void *const values[] = { key, rule, kCFBooleanTrue };
	    HCMakeError(ctx->error, kHeimCredErrorMissingSchemaKey, keys, values, 3);
	    ctx->valid = false;
	    syslog(LOG_ERR, "key missing key");
	}
    } else {
	CFTypeID expectedType = GetTypeFromSchemaRule(key, rule, true);

	if (expectedType != CFGetTypeID(ov)) {
	    const void *const keys[] = { CFSTR("Key"), CFSTR("Rule"), CFSTR("CommonErrorCode") };
	    const void *const values[] = { key, rule, kCFBooleanTrue };
	    HCMakeError(ctx->error, kHeimCredErrorMissingSchemaKey, keys, values, 3);
	    ctx->valid = false;
	    syslog(LOG_ERR, "key have wrong type key");
	}
	if (expectedType == CFArrayGetTypeID()) {
	    ctx->subTypeID = GetTypeFromSchemaRule(key, rule, false);
	    CFArrayApplyFunction(ov, CFRangeMake(0, CFArrayGetCount(ov)), ValidateSubtype, ctx);
	}
    }
}

static bool
validateObject(CFDictionaryRef object, CFErrorRef *error)
{
    heim_assert(error != NULL, "why you bother validating if you wont report the error to the user");

    struct validate ctx = {
	.object = object,
	.valid = true,
	.error = error
    };

    CFStringRef type = GetValidatedValue(object, kHEIMObjectType, CFStringGetTypeID(), error);
    if (type == NULL) {
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(error, kHeimCredErrorMissingSchemaKey, keys, values, 1);
	return false;
    }
    
    ctx.schema = GetValidatedValue(HeimCredCTX.schemas, type, CFDictionaryGetTypeID(), error);
    if (ctx.schema == NULL) {
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(error, kHeimCredErrorNoSuchSchema, keys, values, 1);
	return false;
    }

    /* XXX validate with schema to see that all required attributes are applied */

    CFDictionaryApplyFunction(object, ValidateKey, &ctx);
    CFDictionaryApplyFunction(ctx.schema, ValidateSchema, &ctx);

    return ctx.valid;
}

static void
ValidateSchemaAtRegistration(const void *key, const void *value, void *context)
{
    if (CFEqual(kHEIMObjectType, key))
	return;

    CFTypeID schemaID = GetTypeFromSchemaRule(key, value, true); /* will abort if rule is broken */
    
    CFStringRef globalValue = CFDictionaryGetValue(HeimCredCTX.globalSchema, key);
    if (globalValue == NULL) {
	CFDictionarySetValue(HeimCredCTX.globalSchema, key, value);
    } else {
	CFTypeID globalID = GetTypeFromSchemaRule(key, globalValue, true);
	if (globalID != schemaID) {
	    heim_abort("two schemas have different type for the same key %d != %d (%s)", (int)globalID, (int)schemaID, rk_cfstring2cstring(key));
	}
    }
}

static void
registerSchema(const void *value, void *context)
{
    CFDictionaryRef schema = (CFDictionaryRef)value;
    CFStringRef typeName = CFDictionaryGetValue(schema, kHEIMObjectType);
    CFTypeRef other;

    heim_assert(typeName != NULL, "schema w/o kHEIMObjectType ?");
    
    other = CFDictionaryGetValue(HeimCredCTX.schemas, typeName);
    heim_assert(other == NULL, "schema already registered");
    
    CFDictionaryApplyFunction(schema, ValidateSchemaAtRegistration, NULL);

    CFDictionarySetValue(HeimCredCTX.schemas, typeName, schema);
}

void
_HeimCredRegisterMech(CFStringRef name,
		      CFSetRef schemas,
		      HeimCredStatusCallback statusCallback,
		      HeimCredAuthCallback authCallback)
{
    struct HeimMech *mech;

    mech = (struct HeimMech *)CFDictionaryGetValue(HeimCredCTX.mechanisms, name);
    heim_assert(mech == NULL, "mech already registered");

    CFTypeID mechID = getMechTypeID();

    heim_assert(mechID != _kCFRuntimeNotATypeID, "could not register cftype");

    mech = (struct HeimMech *)_CFRuntimeCreateInstance(NULL, mechID, sizeof(struct HeimMech) - sizeof(CFRuntimeBase), NULL);
    heim_assert(mech != NULL, "out of memory while registering HeimMech instance");

    mech->name = CFRetain(name);
    mech->statusCallback = statusCallback;
    mech->authCallback = authCallback;

    CFDictionarySetValue(HeimCredCTX.mechanisms, name, mech);

    CFSetApplyFunction(schemas, registerSchema, NULL);
}

/*
 * schema rules:
 * R  - required
 * G  - generate
 * s  - string
 * ax - array of x
 * u  - uuid
 * d  - data
 * b  - boolean
 * t  - time/date
 * 
 */


CFMutableDictionaryRef
_HeimCredCreateBaseSchema(CFStringRef objectType)
{
    CFMutableDictionaryRef schema = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(schema, kHEIMAttrType, kHEIMTypeSchema);
    CFDictionarySetValue(schema, kHEIMObjectType, objectType);
    CFDictionarySetValue(schema, kHEIMAttrType, CFSTR("Rs"));
    CFDictionarySetValue(schema, kHEIMAttrClientName, CFSTR("s"));
    CFDictionarySetValue(schema, kHEIMAttrServerName, CFSTR("s"));
    CFDictionarySetValue(schema, kHEIMAttrUUID, CFSTR("Gu"));
    CFDictionarySetValue(schema, kHEIMAttrDisplayName, CFSTR("s"));
    CFDictionarySetValue(schema, kHEIMAttrCredential, CFSTR("b"));
    CFDictionarySetValue(schema, kHEIMAttrLeadCredential, CFSTR("b"));
    CFDictionarySetValue(schema, kHEIMAttrParentCredential, CFSTR("u"));
    CFDictionarySetValue(schema, kHEIMAttrBundleIdentifierACL, CFSTR("as"));
    CFDictionarySetValue(schema, kHEIMAttrDefaultCredential, CFSTR("b"));
    CFDictionarySetValue(schema, kHEIMAttrAuthTime, CFSTR("t"));
    CFDictionarySetValue(schema, kHEIMAttrStoreTime, CFSTR("Gt"));
    CFDictionarySetValue(schema, kHEIMAttrData, CFSTR("d"));
    CFDictionarySetValue(schema, kHEIMAttrRetainStatus, CFSTR("n"));

#if 0
    CFDictionarySetValue(schema, kHEIMAttrTransient, CFSTR("b"));
    CFDictionarySetValue(schema, kHEIMAttrAllowedDomain, CFSTR("as"));
    CFDictionarySetValue(schema, kHEIMAttrStatus, CFSTR(""));
    CFDictionarySetValue(schema, kHEIMAttrExpire, CFSTR("t"));
    CFDictionarySetValue(schema, kHEIMAttrRenewTill, CFSTR("t"));
#endif

    return schema;
}

static void
_HeimCredRegisterGeneric(void)
{
    CFMutableSetRef set;
    CFMutableDictionaryRef schema;

    set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    schema = _HeimCredCreateBaseSchema(kHEIMObjectGeneric);

    CFSetAddValue(set, schema);
    CFRelease(schema);
    _HeimCredRegisterMech(kHEIMTypeGeneric, set, NULL, NULL);
    CFRelease(set);
}

static void
_HeimCredRegisterConfiguration(void)
{
    CFMutableSetRef set;
    CFMutableDictionaryRef schema;

    set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    schema = _HeimCredCreateBaseSchema(kHEIMObjectConfiguration);

    CFSetAddValue(set, schema);
    CFRelease(schema);
    _HeimCredRegisterMech(kHEIMTypeConfiguration, set, NULL, NULL);
    CFRelease(set);
}

#if !TARGET_OS_IPHONE
/*
 *
 */

static void GSSCred_session_handler(xpc_connection_t peerconn)
{
    audit_token_t token;
    uid_t uid;

    xpc_connection_get_audit_token(peerconn, &token);
    uid = xpc_connection_get_euid(peerconn);

    if (HeimCredCTX.session != xpc_connection_get_asid(peerconn) && uid != 0) {
	syslog(LOG_ERR, "client[pid-%d] is not in same session or root", (int)xpc_connection_get_pid(peerconn));
	xpc_connection_cancel(peerconn);
	return;
    }

    /* acquire credential here */

    xpc_connection_cancel(peerconn);
}
#endif

/*
 * We don't need to hold a xpc_transaction over this session, since if
 * we get killed in the middle of deleting credentials, we'll catch
 * that when we start up again.
 */

static void
SessionMonitor(void)
{
    au_sdev_handle_t *h;
    dispatch_queue_t bgq;

    h = au_sdev_open(AU_SDEVF_ALLSESSIONS);
    if (h == NULL)
	return;

    bgq = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0);

    dispatch_async(bgq, ^{
        for (;;) {
	    auditinfo_addr_t aio;
	    int event;
	
	    if (au_sdev_read_aia(h, &event, &aio) != 0)
		continue;

	    /* 
	     * Ignore everything but END. This should relly be
	     * CLOSE but since that is delayed until the credential
	     * is reused, we can't do that 
	     * */
	    if (event != AUE_SESSION_END)
		continue;
		
	    dispatch_async(dispatch_get_main_queue(), ^{
		    int sessionID = aio.ai_asid;
		    CFNumberRef sid = CFNumberCreate(NULL, kCFNumberIntType, &sessionID);
		    heim_assert(sid != NULL, "out of memory");
		    CFDictionaryRemoveValue(HeimCredCTX.sessions, sid);
		    CFRelease(sid);
		});
	}
    });
}


/*
 *
 */

int main(int argc, const char *argv[])
{
    xpc_connection_t conn;

#if TARGET_OS_EMBEDDED
    char *error = NULL;

    if (sandbox_init("com.apple.GSSCred", SANDBOX_NAMED, &error)) {
	syslog(LOG_ERR, "failed to enter sandbox: %s", error);
	exit(EXIT_FAILURE);
    }
#endif

    HeimCredCTX.mechanisms = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(HeimCredCTX.mechanisms != NULL, "out of memory");

    HeimCredCTX.schemas = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(HeimCredCTX.schemas != NULL, "out of memory");

    HeimCredCTX.globalSchema = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(HeimCredCTX.globalSchema != NULL, "out of memory");

    _HeimCredRegisterGeneric();
    _HeimCredRegisterConfiguration();
    _HeimCredRegisterKerberos();
    _HeimCredRegisterNTLM();

    CFRELEASE_NULL(HeimCredCTX.globalSchema);

#if TARGET_IPHONE_SIMULATOR
    archivePath = [[NSString alloc] initWithFormat:@"%@/Library/Caches/com.apple.GSSCred.simulator-archive", NSHomeDirectory()];
#else
    archivePath = @"/var/db/heim-credential-store.archive";
#endif

#if !TARGET_OS_IPHONE
    char *instanceId = getenv(LAUNCH_ENV_INSTANCEID);
    if (instanceId) {
	/*
	 * Pull out uuid and session stored in the sessionUUID
	 */

	uuid_t sessionUUID;
	if (uuid_parse(instanceId, (void *)&sessionUUID) != 0) {
	    syslog(LOG_ERR, "can't parse LAUNCH_ENV_INSTANCEID as a uuid");
	    return 2;
	}

	uid_t auid;
	memcpy(&auid, &sessionUUID, sizeof(auid));
	
	HeimCredCTX.session = ntohl(auid);
	
	if (HeimCredCTX.session == 0) {
	    syslog(LOG_ERR, "0 is not a valid session");
	    return 3;
	}

	/*
	 * Join that session
	 */
	mach_port_name_t session_port;
	int ret = audit_session_port(HeimCredCTX.session, &session_port);
	if (ret) {
	    syslog(LOG_ERR, "could not get audit session port for %d: %s", HeimCredCTX.session, strerror(errno));
	    return 4;
	}
	audit_session_join(session_port);
	mach_port_deallocate(current_task(), session_port);


	conn = xpc_connection_create_mach_service("com.apple.GSSCred",
						  dispatch_get_main_queue(),
						  XPC_CONNECTION_MACH_SERVICE_LISTENER);
    
	xpc_connection_set_event_handler(conn, ^(xpc_object_t object){
		GSSCred_session_handler(object);
	    });

    } else
#endif
    {

	_HeimCredInitCommon();
	SessionMonitor();
	readCredCache();

	runQueue = dispatch_queue_create("com.apple.GSSCred", DISPATCH_QUEUE_SERIAL);
	heim_assert(runQueue != NULL, "dispatch_queue_create failed");

	conn = xpc_connection_create_mach_service("com.apple.GSSCred",
					      runQueue,
					      XPC_CONNECTION_MACH_SERVICE_LISTENER);
    
	xpc_connection_set_event_handler(conn, ^(xpc_object_t object){
		GSSCred_event_handler(object);
	    });
    
    }

    xpc_connection_resume(conn);
	
    dispatch_main();
}
