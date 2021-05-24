/*-
* Copyright (c) 2013 Kungliga Tekniska Högskolan
* (Royal Institute of Technology, Stockholm, Sweden).
* All rights reserved.
*
* Portions Copyright (c) 2013,2020 Apple Inc. All rights reserved.
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

#import "gsscred.h"
#import <TargetConditionals.h>

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFRuntime.h>
#import <CoreFoundation/CFXPCBridge.h>
#import <System/sys/codesign.h>
#import <servers/bootstrap.h>
#import <bsm/libbsm.h>
#import <xpc/xpc.h>
#import <xpc/private.h>
#import <libproc.h>
#import <sandbox.h>
#import <launch.h>
#import <launch_priv.h>

#import <dispatch/dispatch.h>
#import <notify.h>
#import <mach/mach.h>
#import <mach/mach_time.h>
#import <os/log.h>
#import <os/log_private.h>

#import <roken.h>

#import "HeimCredCoder.h"
#import "acquirecred.h"
#import "common.h"
#import "heimbase.h"
#import "hc_err.h"

/*
 *
 */

HeimCredGlobalContext HeimCredGlobalCTX;

/*
 *
 */

static bool validateObject(CFDictionaryRef, CFErrorRef *);
static void addErrorToReply(xpc_object_t, CFErrorRef);
static CFTypeRef GetValidatedValue(CFDictionaryRef, CFStringRef, CFTypeID, CFErrorRef *);
static void HCMakeError(CFErrorRef *, CFIndex,  const void *const *, const void *const *, CFIndex);
static CFTypeID getSessionTypeID(void);
static void handleDefaultCredentialUpdate(struct HeimSession *, HeimCredRef, CFDictionaryRef);
static CFMutableArrayRef QueryCopy(struct peer *peer, xpc_object_t request, const char *key);
static void deleteCredInternal(HeimCredRef cred, struct peer *peer);
static bool removeDuplicates(struct peer *peer, CFDictionaryRef attributes, CFErrorRef *error);
static void DeleteChildren(struct HeimSession *session, CFUUIDRef parent);
static void HeimCredRemoveItemsForASID(struct HeimSession *session, au_asid_t asid);
static void addStandardEventsToCred(HeimCredRef cred, struct HeimSession *session);
static bool commandCanReadforMech(struct HeimMech *mech, const char *cmd);
static bool isTGT(HeimCredRef cred);
static bool isAcquireCred(HeimCredRef cred);
static void reElectMechCredential(const void *key, const void *value, void *context);

NSString *archivePath = NULL;

static void
FlattenCredential(const void *key, const void *value, void *context)
{
    NSMutableDictionary *flatten = (__bridge NSMutableDictionary *)context;
    HeimCredRef cred = (HeimCredRef)value;
    CFRetain(cred);
    id nskey = [HeimCredDecoder copyCF2NS:key];
    id attrs;
    
    if (isAcquireCred(cred)) {
	HEIMDAL_MUTEX_lock(&cred->event_mutex);
	CFDateRef expireTime = CFDateCreate(NULL, (CFTimeInterval)cred->expire - kCFAbsoluteTimeIntervalSince1970);
	CFNumberRef status = CFNumberCreate(NULL, kCFNumberIntType, &cred->acquire_status);
	HEIMDAL_MUTEX_unlock(&cred->event_mutex);
	CFMutableDictionaryRef extraAttrs = CFDictionaryCreateMutableCopy(NULL, 0, cred->attributes);
	CFDictionarySetValue(extraAttrs, kHEIMAttrExpire, expireTime);
	CFDictionarySetValue(extraAttrs, kHEIMAttrStatus, status);
	attrs = [HeimCredDecoder copyCF2NS:extraAttrs];
	CFRELEASE_NULL(extraAttrs);
	CFRELEASE_NULL(expireTime);
	CFRELEASE_NULL(status);
    } else {
	attrs = [HeimCredDecoder copyCF2NS:cred->attributes];
    }
    CFRelease(cred);
    [flatten setObject:attrs forKey:nskey];

}

static void
FlattenSession(const void *key, const void *value, void *context)
{
    NSMutableDictionary *sf = (__bridge NSMutableDictionary *)context;
    id cf = NULL;
    @try {
	struct HeimSession *session = (struct HeimSession *)value;
	cf = [[NSMutableDictionary alloc] init];
	CFDictionaryApplyFunction(session->items, FlattenCredential, (__bridge void *)cf);
	[sf setObject:cf forKey:[NSNumber numberWithInt:(int)session->session]];
    } @catch(NSException * __unused e) {
    } @finally {

    }
}

void
storeCredCache(void)
{
    @autoreleasepool {
	id sf = NULL;
	@try {
	    sf = [[NSMutableDictionary alloc] init];
	    CFDictionaryApplyFunction(HeimCredCTX.sessions, FlattenSession, (__bridge void *)sf);
	    [HeimCredDecoder archiveRootObject:sf toFile:archivePath];
	} @catch(NSException * __unused e) {
	} @finally {
	    
	}
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

void
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

	    os_log_debug(GSSOSLog(), "sending cache changed notification");
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

static void updateCredExpireTime(HeimCredRef cred)
{
    time_t expireTime = 0;
    NSDate *expire =  (__bridge NSDate*)CFDictionaryGetValue(cred->attributes, kHEIMAttrExpire);
    expireTime = (time_t)[expire timeIntervalSince1970];
    HEIMDAL_MUTEX_lock(&cred->event_mutex);
    cred->expire = expireTime;
    if (expire!=nil && isTGT(cred)) {
	//only TGTs get an expire event here, to notify caches when they expire
	cred_update_expire_time_locked(cred, cred->expire);
    }
    HEIMDAL_MUTEX_unlock(&cred->event_mutex);
}

static void addStandardEventsToCred(HeimCredRef cred, struct HeimSession *session)
{

    if (!CFEqual(cred->mech->name, kHEIMTypeKerberos) && !CFEqual(cred->mech->name, kHEIMTypeKerberosAcquireCred)) {
	return;
    }
    
    cred->session = session->session;

    HeimCredEventContextRef expireCredContext = HeimCredEventContextCreateItem(cred);
    cred->expireEventContext = (HeimCredEventContextRef)CFRetain(expireCredContext);
    cred->expire_event = heim_ipc_event_cf_create_f(HeimCredGlobalCTX.expireFunction, expireCredContext);
    CFRELEASE_NULL(expireCredContext);

    HeimCredEventContextRef renewCredContext = HeimCredEventContextCreateItem(cred);
    cred->renewEventContext = (HeimCredEventContextRef)CFRetain(renewCredContext);
    cred->renew_event = heim_ipc_event_cf_create_f(HeimCredGlobalCTX.renewFunction, renewCredContext);
    CFRELEASE_NULL(renewCredContext);
    
    heim_ipc_event_set_final_f(cred->renew_event, HeimCredGlobalCTX.finalFunction);
    heim_ipc_event_set_final_f(cred->expire_event, HeimCredGlobalCTX.finalFunction);
    
    //this will also setup an event to notify the caches when the cred expires.
    updateCredExpireTime(cred);
    
#if TARGET_OS_OSX
    if (isAcquireCred(cred)) {
	//if this is an acquire credential request, then set the expire time, type, and status
	HEIMDAL_MUTEX_lock(&cred->event_mutex);
	cred->is_acquire_cred = true;
	HEIMDAL_MUTEX_unlock(&cred->event_mutex);
	cred_acquire_status status;
	CFNumberRef statusNumber = CFDictionaryGetValue(cred->attributes, kHEIMAttrStatus);
	if (statusNumber!=NULL &&
	    CFNumberGetValue(statusNumber, kCFNumberIntType, &status)) {
	    cred_update_acquire_status(cred, status);
	} else {
	    cred_update_acquire_status(cred, CRED_STATUS_ACQUIRE_START);
	}
    } else if (isTGT(cred) && hasRenewTillInAttributes(cred->attributes)) {
	//if the credential is renewable, then start the renewal event
	cred_update_renew_time(cred, false);
    } 
#endif
    
}

static bool
isTemporaryCache(CFDictionaryRef attributes)
{
    bool temporaryCache = false;
    if (CFDictionaryContainsKey(attributes, kHEIMAttrTemporaryCache)) {
	temporaryCache = (CFDictionaryGetValue(attributes, kHEIMAttrTemporaryCache) == kCFBooleanTrue);
    }
    return temporaryCache;
}

cache_read_status
readCredCache(void)
{
    @autoreleasepool {
	NSDictionary *sessions = NULL;
	@try {

#if TARGET_OS_OSX
	    long long maxFileSize = 1024*1024*5; //5 megs
#else
	    long long maxFileSize = 1024*1024*1; //1 meg
#endif
	    NSDictionary *fileAttributes =  [[NSFileManager defaultManager] attributesOfItemAtPath:archivePath error:nil];
	    NSNumber *archiveFileSize = fileAttributes[NSFileSize];
	    if ([archiveFileSize longLongValue] > maxFileSize) {
		os_log_error(GSSOSLog(), "The archive file size %@ exceeds the max limit of %lld. Aborting the load.", archiveFileSize, maxFileSize);
		return READ_SIZE_ERROR;
	    }

	    sessions = [HeimCredDecoder copyUnarchiveObjectWithFileSecureEncoding:archivePath];
	    
	    if (!sessions || ![sessions isKindOfClass:[NSDictionary class]]) {
		return READ_EMPTY;
	    }
	    
	    [sessions enumerateKeysAndObjectsUsingBlock:^(id skey, id svalue, BOOL *sstop) {
		int sessionID = [(NSNumber *)skey intValue];
		
		if (!HeimCredGlobalCTX.useUidMatching) {
		    //the sid is not an asid when using uid matching
		    if (!HeimCredGlobalCTX.sessionExists(sessionID))
			return;
		}
		
		struct HeimSession *session = HeimCredCopySession(sessionID);
		if (session == NULL)
		    return;
		
		NSDictionary *creds = (NSDictionary *)svalue;
		
		if (!creds || ![creds isKindOfClass:[NSDictionary class]]) {
		    CFRELEASE_NULL(session);
		    return;
		}
		
		[creds enumerateKeysAndObjectsUsingBlock:^(id ckey, id cvalue, BOOL *cstop) {
		    CFUUIDRef cfkey = [HeimCredDecoder copyNS2CF:ckey];
		    CFDictionaryRef cfvalue = [HeimCredDecoder copyNS2CF:cvalue];
		    if (cfkey && cfvalue) {
			HeimCredRef cred = HeimCredCreateItem(cfkey);

			if (HeimCredGlobalCTX.useUidMatching) {
			    NSNumber *asid = CFDictionaryGetValue(cfvalue, kHEIMAttrASID);
			    if (asid == nil || (asid && !HeimCredGlobalCTX.sessionExists(asid.intValue))) {
				//do not load this credential if the audit session does not exist.
				CFRELEASE_NULL(cred);
			    }
			}
			
			if (cred && HeimCredAssignMech(cred, cfvalue)) {
			    cred->attributes = CFRetain(cfvalue);
			    CFDictionarySetValue(session->items, cred->uuid, cred);

			    if (!isTemporaryCache(cred->attributes)) {
				handleDefaultCredentialUpdate(session, cred, cred->attributes);
			    }
			    addStandardEventsToCred(cred, session);
			} else {
			    /* dropping cred if mech assignment failed */
			}
			if (cred) {
			    CFRelease(cred);
			}
		    }
		    CFRELEASE_NULL(cfkey);
		    CFRELEASE_NULL(cfvalue);
		}];

		//elect the default credential, if there was not one explicitly set
		CFDictionaryApplyFunction(HeimCredCTX.mechanisms, reElectMechCredential, session);

		CFIndex count = CFDictionaryGetCount(session->items);
		if (count == 0) {
		    //do not add the session if no creds
		    CFNumberRef sid = CFNumberCreate(NULL, kCFNumberIntType, &sessionID);
		    CFDictionaryRemoveValue(HeimCredCTX.sessions, sid);
		    CFRELEASE_NULL(session);
		    CFRELEASE_NULL(sid);
		} else {
		    CFRelease(session); //session is retained after this method
		}
	    }];
	    return READ_SUCCESS;
	} @catch(NSException *e) {
	    os_log_error(GSSOSLog(), "readCredCache failed with: %s:%s", [[e name] UTF8String], [[e reason] UTF8String]);
	    return READ_EXCEPTION;
	}
    }
}


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

    if (CFGetTypeID(uuid) != CFUUIDGetTypeID()) {
	goto out;
    }

    while (1) {
	HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
	CFUUIDRef parent;

	if (cred == NULL)
		goto pass;
        
	heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

	if (max_depth-- < 0)
	    goto out;
#if TARGET_OS_OSX
	NSString *serverName = CFDictionaryGetValue(cred->attributes, kHEIMAttrServerName);
	if ([serverName hasPrefix:@"krb5_ccache_conf_data/password"]) {
	    //This is sensitive data, a special entitlement is required to access it.
	    
	    switch (peer->access_status) {
		case IAKERB_NOT_CHECKED:
		{
		    if (HeimCredGlobalCTX.hasEntitlement(peer, "com.apple.private.gssapi.iakerb-data-access"))
		    {
			peer->access_status = IAKERB_ACCESS_GRANTED;
			break;
		    } else if (([@"com.apple.NetAuthSysAgent" isEqualToString:(__bridge NSString * _Nonnull)peer->bundleID] && HeimCredGlobalCTX.verifyAppleSigned(peer, @"com.apple.NetAuthSysAgent")) ||
			([@"com.apple.gssd" isEqualToString:(__bridge NSString * _Nonnull)peer->bundleID] && HeimCredGlobalCTX.verifyAppleSigned(peer, @"com.apple.gssd")))
		    {
			peer->access_status = IAKERB_ACCESS_GRANTED;
			os_log_debug(GSSOSLog(), "access granted to sensitive data: %@", peer->bundleID);
			break;
		    } else {
			peer->access_status = IAKERB_ACCESS_DENIED;
			os_log_debug(GSSOSLog(), "access denied to sensitive data: %@", peer->bundleID);
			goto out;
		    }
		}

		case IAKERB_ACCESS_DENIED: {
		    goto out;
		    break;
		}
		case IAKERB_ACCESS_GRANTED: {
		    //carry on
		    break;
		}
	    }
	}
#endif
#if TARGET_OS_IOS
	if (HeimCredGlobalCTX.isMultiUser) {
	    uid_t uid = HeimCredGlobalCTX.getUid(peer->peer);
	    if (uid == 0) {
		//root has access to all credentials, regardless of bundleid ACL and altDSID
		goto pass;
	    } else {
		//all other users have to match the saved user
		BOOL dsidMatch = false;
		if (!peer->currentDSID) {
		    NSString *altDSID = HeimCredGlobalCTX.currentAltDSID();
		    peer->currentDSID = CFBridgingRetain(altDSID);  //we save this because if the user is changed, then another instance is started.
		}
		CFStringRef savedDSID = CFDictionaryGetValue(cred->attributes, kHEIMAttrAltDSID);
		
		if (savedDSID && peer->currentDSID) {
		    dsidMatch = (CFStringCompare(savedDSID, peer->currentDSID, 0) == kCFCompareEqualTo);
		}
		if (!dsidMatch) {
		    //The dsid that created the credential doesnt match the current dsid
		    goto out;
		}
	    }
	}
#endif
	if (HeimCredGlobalCTX.useUidMatching) {
	    //check if the asids match
	    au_asid_t asid = HeimCredGlobalCTX.getAsid(peer->peer);
	    CFNumberRef asidNumber = CFNumberCreate(NULL, kCFNumberIntType, &asid);
	    CFNumberRef credASID = CFDictionaryGetValue(cred->attributes, kHEIMAttrASID);
	    BOOL asidMatch = false;
	    if (asidNumber && credASID) {
		asidMatch = CFEqual(asidNumber, credASID);
	    }
	    //check if the uids match
	    uid_t uid = HeimCredGlobalCTX.getUid(peer->peer);
	    CFNumberRef credUid = CFDictionaryGetValue(cred->attributes, kHEIMAttrUserID);
	    CFNumberRef uidNumber = CFNumberCreate(NULL, kCFNumberIntType, &uid);
	    BOOL usersMatch = false;
	    if (credUid && uidNumber) {
	    	usersMatch = CFEqual(credUid, uidNumber);
	    }
	    
	    CFRELEASE_NULL(uidNumber);
	    CFRELEASE_NULL(asidNumber);
	    
	    //if neither matches, then deny access to the cred
	    if (!usersMatch && !asidMatch) {
		//The uid that created the credential doesnt match the uid requesting the credential and the asid for the credential doesnt match the asid of the cred.
		goto out;
	    }
	}
	
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

#if TARGET_OS_OSX
		if (CFEqual(prefix, CFSTR("*")))
		    goto pass;
#endif
		if (CFEqual(peer->bundleID, prefix))
		    goto pass;

		NSPredicate *wildCardMatch = [NSPredicate predicateWithFormat:@"self like %@", prefix];
		BOOL matchFound = [wildCardMatch evaluateWithObject:(__bridge NSString *)peer->bundleID];

		if (matchFound)
		    goto pass;
		
		if (CFEqual(prefix, CFSTR("com.apple.private.gssapi.allowmanagedapps"))) {
		    if (peer->needsManagedAppCheck) {
			os_log_debug(GSSOSLog(), "checking managed app status for: %{private}s", CFStringGetCStringPtr(peer->bundleID, kCFStringEncodingUTF8));
#if TARGET_OS_IOS
			peer->isManagedApp = [HeimCredGlobalCTX.managedAppManager isManagedApp:(__bridge NSString *)peer->bundleID];
#else
			peer->isManagedApp = false;
#endif
			peer->needsManagedAppCheck = false;
			os_log_debug(GSSOSLog(), "app %{private}s %s",  CFStringGetCStringPtr(peer->bundleID, kCFStringEncodingUTF8), (peer->isManagedApp ? "is managed" : "is not managed") );
		    }
		    if (peer->isManagedApp) {
			goto pass;
		    }

		}
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
    CFRELEASE_NULL(date);
}
static void
setDefaultCredentialValue(CFUUIDRef parentUUID, struct HeimSession *session, CFBooleanRef value)
{
    HeimCredRef parentCred = (HeimCredRef)CFDictionaryGetValue(session->items, parentUUID);
    if (parentCred) {
	CFMutableDictionaryRef attrs = CFDictionaryCreateMutableCopy(NULL, 0, parentCred->attributes);
	if (value) {
	    CFDictionarySetValue(attrs, kHEIMAttrDefaultCredential, value);
	} else {
	    CFDictionaryRemoveValue(attrs, kHEIMAttrDefaultCredential);
	}
	CFRELEASE_NULL(parentCred->attributes);
	parentCred->attributes = attrs;
    }
}

static void
handleDefaultCredentialUpdate(struct HeimSession *session, HeimCredRef cred, CFDictionaryRef attrs)
{
    //this method is to handle explicit default cache changes (kHEIMAttrDefaultCredential == true in attrs)

    heim_assert(cred->mech != NULL, "mech is NULL, schame validation doesn't work ?");
    
    //only caches can be default
    CFUUIDRef parent = CFDictionaryGetValue(attrs, kHEIMAttrParentCredential);
    if (parent != NULL) {
	return;
    }
    
    CFUUIDRef oldDefault = CFDictionaryGetValue(session->defaultCredentials, cred->mech->name);

    CFBooleanRef defaultCredential = CFDictionaryGetValue(attrs, kHEIMAttrDefaultCredential);
    if (defaultCredential == NULL || !CFBooleanGetValue(defaultCredential)) {
	//if the cache is not the new default, then exit
	return;
    }

    if (oldDefault) {
	// Drop marker on old cache
	setDefaultCredentialValue(oldDefault, session, NULL);
    }

    //set new cache as default.  The attrs already have the kHEIMAttrDefaultCredential value
    CFDictionarySetValue(session->defaultCredentials, cred->mech->name, cred->uuid);

    if (cred->mech->notifyCaches!=NULL) {
	cred->mech->notifyCaches();
    }



}
    
//this method will be called for each mech
static void
reElectMechCredential(const void *key, const void *value, void *context)
{
    struct HeimMech *mech = (struct HeimMech *)value;
    struct HeimSession *session = (struct HeimSession *)context;

    CFUUIDRef defCred = CFDictionaryGetValue(session->defaultCredentials, mech->name);
    if (defCred) {
	if (CFDictionaryGetValue(session->items, defCred)) {
	    //the default exists, nothing to do
	    return;
	} else {
	    //if the default credential doesn't exist, elect another one.
	    CFDictionaryRemoveValue(session->defaultCredentials, mech->name);
	    os_log_debug(GSSOSLog(), "The default credential for %@ does not exist.", mech->name);
	}
    }
    
    NSDictionary *items = (__bridge NSDictionary*)session->items;
    [items enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull itemkey, id  _Nonnull obj, BOOL * _Nonnull stop) {
	HeimCredRef cred = (__bridge HeimCredRef)obj;
	CFUUIDRef parentUUID =  CFDictionaryGetValue(cred->attributes, kHEIMAttrParentCredential);

	// only caches with unexpired TGTs can be the default, so we look for the first valid TGT and set it's parent to be the default.
	// temporary caches cannot be the default
	if (CFEqual(cred->mech->name, mech->name) &&  //only handle the current type of cred
	    parentUUID != NULL &&
	    !isTemporaryCache(cred->attributes) &&
	    CFDictionaryGetValue(cred->attributes, kHEIMAttrLeadCredential) == kCFBooleanTrue) {

	    NSDate *expire =  (__bridge NSDate*)CFDictionaryGetValue(cred->attributes, kHEIMAttrExpire);
	    if (expire.timeIntervalSinceNow > 0) {
		//the first one found is elected the default
		CFDictionarySetValue(session->defaultCredentials, cred->mech->name, parentUUID);

		//set the default credential attribute on the parent
		setDefaultCredentialValue(parentUUID, session, kCFBooleanTrue);

		HeimCredCTX.needFlush = 1;

		os_log_debug(GSSOSLog(), "The default for %@ credential is now %@",cred->mech->name, cred);
		*stop = YES;
	    }
	}
    }];
    
    //if a default was not identified, create a uuid as the default
    defCred = CFDictionaryGetValue(session->defaultCredentials, mech->name);
    if (defCred == NULL) {
	os_log_debug(GSSOSLog(), "A default credential for %@ could not be identified.", mech->name);
	/*
	 * If there is no default credential, make one up
	 */
	CFUUIDRef defcred = CFUUIDCreate(NULL);
	CFDictionarySetValue(session->defaultCredentials, mech->name, defcred);
	CFRELEASE_NULL(defcred);
    }

    if (mech->notifyCaches!=NULL) {
	mech->notifyCaches();
    }
}

static void
reElectDefaultCredential(struct peer *peer)
{
    if (!peer->session->updateDefaultCredential)
	return;
    peer->session->updateDefaultCredential = 0;

    CFDictionaryApplyFunction(HeimCredCTX.mechanisms, reElectMechCredential, peer->session);
}

static void
handleDefaultCredentialDeletion(struct peer *peer, HeimCredRef cred)
{
    CFUUIDRef defaultCredential = CFDictionaryGetValue(peer->session->defaultCredentials, cred->mech->name);

    //if the default cache is deleted, then remove it and trigger an election
    if (defaultCredential && CFEqual(defaultCredential, cred->uuid)) {
	CFDictionaryRemoveValue(peer->session->defaultCredentials, cred->mech->name);
	peer->session->updateDefaultCredential = 1;
	reElectDefaultCredential(peer);
    }
}

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
static bool
canSetAnyBundleIDACL(struct peer *peer)
{
    return CFStringCompare(CFSTR("com.apple.accountsd"), peer->callingAppBundleID, 0) == kCFCompareEqualTo
    || HeimCredGlobalCTX.hasEntitlement(peer, "com.apple.private.gssapi.allowwildcardacl");

}
#endif

static void
deleteCredInternal(HeimCredRef cred, struct peer *peer)
{
    heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

    CFDictionaryRemoveValue(peer->session->items, cred->uuid);

    DeleteChildren(peer->session, cred->uuid);
    if (cred->mech->notifyCaches!=NULL) {
	cred->mech->notifyCaches();
    }

    handleDefaultCredentialDeletion(peer, cred);
}


static bool
removeDuplicates(struct peer *peer, CFDictionaryRef attributes, CFErrorRef *error)
{
    NSMutableDictionary *query;
    
    //remove duplicate creds ahead of creating a new credential, this is not for parent credentials
    CFStringRef objectType = CFDictionaryGetValue(attributes, kHEIMObjectType);
    CFUUIDRef parentCredential = CFDictionaryGetValue(attributes, kHEIMAttrParentCredential);
    CFStringRef clientName = CFDictionaryGetValue(attributes, kHEIMAttrClientName);
    CFStringRef serverName = CFDictionaryGetValue(attributes, kHEIMAttrServerName);
    
    if (!parentCredential || !clientName || !serverName) {
	//nothing to check for duplicates
	return true;
    }
    
    query = [@{
	(id)kHEIMObjectType:(__bridge id)objectType,
	(id)kHEIMAttrParentCredential:(__bridge id)parentCredential,
	(id)kHEIMAttrClientName:(__bridge id)clientName,
	(id)kHEIMAttrServerName:(__bridge id)serverName
    } mutableCopy];
    
#if TARGET_OS_IOS
    if (HeimCredGlobalCTX.isMultiUser) {
	NSString *altDSID = HeimCredGlobalCTX.currentAltDSID();
	if (altDSID) {
	    query[(id)kHEIMAttrAltDSID] = altDSID;
	} else {
	    os_log_error(GSSOSLog(), "the device is multiuser and is missing the altDSID");
	    const void *const keys[] = { CFSTR("CommonErrorCode") };
	    const void *const values[] = { kCFBooleanTrue };
	    HCMakeError(error, kHeimCredErrorMissingRequiredValue, keys, values, 1);
	    return false;
	}
    }
#endif

    if (HeimCredGlobalCTX.useUidMatching) {
	//the addition of both of these attributes is redundant because it must have the same parent id, but it doesnt hurt to check then too.
	uid_t uid = HeimCredGlobalCTX.getUid(peer->peer);
	CFNumberRef uidNumber = CFNumberCreate(NULL, kCFNumberIntType, &uid);
	if (uid != 0) {
	    query[(id)kHEIMAttrUserID] = (__bridge id _Nullable)(uidNumber);
	}
	CFRELEASE_NULL(uidNumber);
	
	au_asid_t asid = HeimCredGlobalCTX.getAsid(peer->peer);
	CFNumberRef asidNumber = CFNumberCreate(NULL, kCFNumberIntType, &asid);
	query[(id)kHEIMAttrASID] = (__bridge id _Nullable)(asidNumber);
	CFRELEASE_NULL(asidNumber);
    }
    
    xpc_object_t xpcquery = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)(query));
    xpc_object_t xc = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(xc, "command", "removeduplicates");
    xpc_dictionary_set_value(xc, "query", xpcquery);
    CFArrayRef duplicateItems = QueryCopy(peer, xc, "query");
    
    CFIndex n, count = CFArrayGetCount(duplicateItems);
    for (n = 0; n < count; n++) {
	HeimCredRef cred = (HeimCredRef)CFArrayGetValueAtIndex(duplicateItems, n);
	deleteCredInternal(cred, peer);
    }
    
    if (count > 0) {
	HeimCredCTX.needFlush = 1;
    }
    
    CFRELEASE_NULL(duplicateItems);
    return true;
}


static bool
isAcquireCred(HeimCredRef cred)
{
    //this should be executed on the run queue to prevent concurrency issues with the attributes.
    return (CFEqual(cred->mech->name, kHEIMTypeKerberosAcquireCred) && CFDictionaryGetValue(cred->attributes, kHEIMAttrCredential) != NULL);
}

bool hasRenewTillInAttributes(CFDictionaryRef attributes)
{
    return (CFDictionaryGetValue(attributes, kHEIMAttrRenewTill) != NULL);
}

static bool isTGT(HeimCredRef cred)
{
    return (CFEqual(cred->mech->name, kHEIMTypeKerberos) && CFDictionaryGetValue(cred->attributes, kHEIMAttrLeadCredential) == kCFBooleanTrue);
}

void
do_CreateCred(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFMutableDictionaryRef attrs = NULL;
    HeimCredRef cred = NULL;
    CFUUIDRef uuid = NULL;
    bool hasACL = false;
    CFErrorRef error = NULL;
    CFBooleanRef lead;
    BOOL temporaryCache = false;
    os_log_debug(GSSOSLog(), "Begin Create Cred: %@", request);
    
    CFDictionaryRef attributes = HeimCredMessageCopyAttributes(request, "attributes", CFDictionaryGetTypeID());
    if (attributes == NULL)
	goto out;
    
    if (!validateObject(attributes, &error)) {
	addErrorToReply(reply, error);
	goto out;
    }

    temporaryCache = isTemporaryCache(attributes);

    /* check if we are ok to link into this cred-tree */
    
    CFUUIDRef parentUUID = CFDictionaryGetValue(attributes, kHEIMAttrParentCredential);
    if (parentUUID != NULL && !checkACLInCredentialChain(peer, parentUUID, &hasACL))
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
    
    //duplicate check here
    if (!removeDuplicates(peer, attributes, &error)) {
	addErrorToReply(reply, error);
	goto out;
    }
    
    cred = HeimCredCreateItem(uuid);
    if (cred == NULL)
	goto out;
    
    attrs = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
    if (attrs == NULL)
	goto out;

    bool hasACLInAttributes = addPeerToACL(peer, attrs);

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    if (hasACLInAttributes && !canSetAnyBundleIDACL(peer)) {
       CFArrayRef array = CFDictionaryGetValue(attrs, kHEIMAttrBundleIdentifierACL);
       if (array == NULL || CFGetTypeID(array) != CFArrayGetTypeID() || CFArrayGetCount(array) != 1) {
           os_log_error(GSSOSLog(), "peer sent more then one bundle id and is not accountsd");
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
    if (!temporaryCache) {
	handleDefaultCredentialUpdate(peer->session, cred, attrs);
    }
    
#if TARGET_OS_IOS
    if (HeimCredGlobalCTX.isMultiUser) {
	//store the altDSID for the new credential when in shared ipad mode on ios
	CFStringRef altDSID = CFBridgingRetain(HeimCredGlobalCTX.currentAltDSID());
	if (altDSID) {
	    CFDictionarySetValue(attrs, kHEIMAttrAltDSID, altDSID);
	    CFRELEASE_NULL(altDSID);
	} else {
	    os_log_error(GSSOSLog(), "the device is multiuser and is missing the altDSID");
	    const void *const keys[] = { CFSTR("CommonErrorCode") };
	    const void *const values[] = { kCFBooleanTrue };
	    HCMakeError(&error, kHeimCredErrorMissingRequiredValue, keys, values, 1);
	    addErrorToReply(reply, error);
	    goto out;
	}
    }
#endif

    if (HeimCredGlobalCTX.useUidMatching) {
	uid_t uid = HeimCredGlobalCTX.getUid(peer->peer);
	CFNumberRef uidNumber = CFNumberCreate(NULL, kCFNumberIntType, &uid);
	if (uid != 0) {
	    CFDictionarySetValue(attrs, kHEIMAttrUserID, uidNumber);
	}
	CFRELEASE_NULL(uidNumber);
	
	au_asid_t asid = HeimCredGlobalCTX.getAsid(peer->peer);
	CFNumberRef asidNumber = CFNumberCreate(NULL, kCFNumberIntType, &asid);
	CFDictionarySetValue(attrs, kHEIMAttrASID, asidNumber);
	CFRELEASE_NULL(asidNumber);
    }

    if(parentUUID==NULL) {
	//the retain status is only on caches and starts with 1
	CFDictionarySetValue(attrs, kHEIMAttrRetainStatus, @1);
    }

    cred->attributes = CFRetain(attrs);
    
    CFDictionarySetValue(peer->session->items, cred->uuid, cred);
    if (CFDictionaryGetValue(cred->attributes, kHEIMAttrLeadCredential) == kCFBooleanTrue) {
	if (cred->mech->notifyCaches!=NULL) {
	    cred->mech->notifyCaches();
	}
    }
    HeimCredCTX.needFlush = 1;

    /*
     * If the default credential is unset or doesn't exists, switch to
     * the now just created lead credential.
     */

    heim_assert(cred->mech != NULL, "mech is NULL, schame validation doesn't work ?");

    //if there is not a default cred cache, and we just created a TGT then trigger an election
    CFUUIDRef defcred = CFDictionaryGetValue(peer->session->defaultCredentials, cred->mech->name);
    if (!temporaryCache
	&& (defcred == NULL || CFDictionaryGetValue(peer->session->items, defcred) == NULL)
	&& (lead = CFDictionaryGetValue(cred->attributes, kHEIMAttrLeadCredential))
	&& CFBooleanGetValue(lead))
    {
	peer->session->updateDefaultCredential = 1;
	reElectDefaultCredential(peer);
    }

    addStandardEventsToCred(cred, peer->session);

    HeimCredMessageSetAttributes(reply, "attributes", cred->attributes);
    if (cred->mech->statusCallback!=NULL) {
	CFTypeRef status = cred->mech->statusCallback(cred);
	os_log_debug(GSSOSLog(), "End Create Cred: %{private}@", status);
	CFRELEASE_NULL(status);
    }
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
    const char *command;
};

static void
MatchQueryItem(const void *key, const void *value, void *context)
{
    struct MatchCTX * mc = (struct MatchCTX *)context;
    heim_assert(mc->attributes != NULL, "attributes NULL in MatchQueryItem");

    if (CFEqual(key, kHEIMObjectType) && CFEqual(value, kHEIMObjectAny)) {
	mc->count++;
	return;
    }
    
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
    if (mc->numQueryItems == mc->count) {
	if (!commandCanReadforMech(cred->mech, mc->command)) {
	    return;
	}
	CFArrayAppendValue(mc->results, cred);
    }
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

static bool
commandCanReadforMech(struct HeimMech *mech, const char *cmd)
{
    heim_assert(mech != NULL, "mech is required");
    heim_assert(cmd != NULL, "command is required");
         
    NSString *command = [NSString stringWithCString:cmd encoding:NSUTF8StringEncoding];
    if (mech->readRestricted && mech->readOnlyCommands && [(__bridge NSArray *)mech->readOnlyCommands containsObject:command])
    {
	return false;
    }
    return true;
}

static CFMutableArrayRef
QueryCopy(struct peer *peer, xpc_object_t request, const char *key)
{
    struct MatchCTX mc = {
	.peer = peer,
	.results = NULL,
	.query = NULL,
	.attributes = NULL,
	.command = NULL,
    };
    CFErrorRef error = NULL;
    
    mc.command = xpc_dictionary_get_string(request, "command");
    if (!mc.command) {
	os_log_error(GSSOSLog(), "query is missing command");
	goto out;
    }
    
    
    mc.query = HeimCredMessageCopyAttributes(request, key, CFDictionaryGetTypeID());
    if (mc.query == NULL)
	goto out;

    mc.numQueryItems = CFDictionaryGetCount(mc.query);

    if (mc.numQueryItems == 1) {
	CFUUIDRef uuidref = CFDictionaryGetValue(mc.query, kHEIMAttrUUID);
	if (uuidref && CFGetTypeID(uuidref) == CFUUIDGetTypeID()) {
	
	    if (!checkACLInCredentialChain(peer, uuidref, NULL))
		goto out;

	    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuidref);
	    if (cred == NULL)
		goto out;

	    heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");
	    
	    if (!commandCanReadforMech(cred->mech, mc.command)) {
		goto out;
	    }
	    
	    mc.results = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    CFArrayAppendValue(mc.results, cred);

	    goto out;
	}
    }
    
    mc.results = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    CFStringRef objectType = GetValidatedValue(mc.query, kHEIMObjectType, CFStringGetTypeID(), &error);
    CFStringRef type = GetValidatedValue(mc.query, kHEIMAttrType, CFStringGetTypeID(), &error);
    if (objectType && !CFEqual(objectType, kHEIMObjectAny)
	&& type && CFEqual(type, kHEIMTypeSchema))
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
    CFRELEASE_NULL(child.array);
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
	CFRELEASE_NULL(cred->attributes);
	CFDictionarySetValue(attrs, kHEIMAttrParentCredential, fromto->to);
	cred->attributes = attrs;
    }
}

void
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
	deleteCredInternal(cred, peer);
    }

    HeimCredCTX.needFlush = 1;
    
 out:
    if (error) {
	addErrorToReply(reply, error);
	CFRELEASE_NULL(error);
    }
    CFRELEASE_NULL(items);
}

static void
updateCred(const void *key, const void *value, void *context)
{
    CFMutableDictionaryRef attrs = (CFMutableDictionaryRef)context;
    CFDictionarySetValue(attrs, key, value);
}

void
do_SetAttrs(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef uuid = HeimCredCopyUUID(request, "uuid");
    CFMutableDictionaryRef attrs;
    CFErrorRef error = NULL;
    NSArray *readonlyAttributes;
    
    if (uuid == NULL)
	return;

    if (!checkACLInCredentialChain(peer, uuid, NULL)) {
	CFRELEASE_NULL(uuid);
	return;
    }
    
    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
    CFRELEASE_NULL(uuid);
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
	CFRELEASE_NULL(attrs);
	goto out;
    }

    readonlyAttributes = @[
	(id)kHEIMObjectType,
	(id)kHEIMAttrType,
	(id)kHEIMAttrAltDSID,
	(id)kHEIMAttrUserID,
	(id)kHEIMAttrRetainStatus
    ];
    for (NSString *attr in readonlyAttributes) {
	if (CFDictionaryContainsKey(replacementAttrs, (__bridge const void *)(attr))) {
	    const void *const keys[] = { CFSTR("CommonErrorCode") };
	    const void *const values[] = { kCFBooleanTrue };
	    HCMakeError(&error, kHeimCredErrorUpdateNotAllowed, keys, values, 1);
	    addErrorToReply(reply, error);
	    CFRELEASE_NULL(attrs);
	    CFRELEASE_NULL(replacementAttrs);
	    goto out;
	}
    }

    // temporary caches can not be set as default
    CFBooleanRef defaultCache = CFDictionaryGetValue(replacementAttrs, kHEIMAttrDefaultCredential);
    if (isTemporaryCache(cred->attributes) && defaultCache && CFBooleanGetValue(defaultCache)) {
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(&error, kHeimCredErrorUpdateNotAllowed, keys, values, 1);
	addErrorToReply(reply, error);
	CFRELEASE_NULL(attrs);
	CFRELEASE_NULL(replacementAttrs);
	goto out;
    }

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    bool hasACLInAttributes = false;
    CFArrayRef acl = CFDictionaryGetValue(replacementAttrs, kHEIMAttrBundleIdentifierACL);
    if (acl == NULL || CFGetTypeID(acl) != CFArrayGetTypeID()) {
	hasACLInAttributes = false;
    } else {
	hasACLInAttributes = true;
    }
    
    if (hasACLInAttributes && !canSetAnyBundleIDACL(peer)) {
	if (CFArrayGetCount(acl) != 1 || !CFEqual(peer->bundleID, CFArrayGetValueAtIndex(acl, 0))) {
	    os_log_error(GSSOSLog(), "peer sent more then one bundle id and is not allowed if the acl does not match the app bundle id.");
	    const void *const keys[] = { CFSTR("CommonErrorCode") };
	    const void *const values[] = { kCFBooleanTrue };
	    HCMakeError(&error, kHeimCredErrorUpdateNotAllowed, keys, values, 1);
	    addErrorToReply(reply, error);
	    CFRELEASE_NULL(attrs);
	    CFRELEASE_NULL(replacementAttrs);
	    goto out;
	}
    }
#endif
    
#if TARGET_OS_OSX
    //we are only interested in the status if the replacement values include it
    CFNumberRef statusNumber = CFDictionaryGetValue(replacementAttrs, kHEIMAttrStatus);
#endif
    
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
    
    HeimCredCTX.needFlush = 1;
    
    //update the cred expire time and event
    updateCredExpireTime(cred);
    
#if TARGET_OS_OSX
    //if the renew_till is updated, then update the renew event
    //normal flows will not do this.  it will facilitate testing and not waiting 2 hours
    if (isTGT(cred) && hasRenewTillInAttributes(cred->attributes)) {
	//if the credential is renewable, then start the renewal event
	cred_update_renew_time(cred, false);
    }
    
    //if the replacement values include a status, and the value is CRED_STATUS_ACQUIRE_START, trigger acquire.
    //normal flows will not update the status to start.  it will facilitate testing and not waiting 10 hours
    if (isAcquireCred(cred) && statusNumber!=NULL) {
	cred_acquire_status status = CRED_STATUS_ACQUIRE_INITIAL;
	if (CFNumberGetValue(statusNumber, kCFNumberIntType, &status)
	    && status == CRED_STATUS_ACQUIRE_START) {
	    //kick off the acquire function
	    cred_update_acquire_status(cred, CRED_STATUS_ACQUIRE_START);
	}
    }
#endif

  out:
    CFRELEASE_NULL(error);
}

void
do_Auth(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFDictionaryRef outputAttrs = NULL;
    CFUUIDRef uuid = HeimCredCopyUUID(request, "uuid");
    if (uuid == NULL)
	return;

    if (!checkACLInCredentialChain(peer, uuid, NULL)) {
	CFRELEASE_NULL(uuid);
	return;
    }

    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
    CFRELEASE_NULL(uuid);
    if (cred == NULL)
	return;

    CFDictionaryRef inputAttrs = HeimCredMessageCopyAttributes(request, "attributes", CFDictionaryGetTypeID());
    if (inputAttrs == NULL) {
	return;
    }
    
/* call mech authCallback */
    if( cred->mech->authCallback ) {
	outputAttrs = cred->mech->authCallback(cred, inputAttrs);
	if (outputAttrs)
	    HeimCredMessageSetAttributes(reply, "attributes", outputAttrs);
    } else {
	os_log_error(GSSOSLog(), "no HeimCredAuthCallback defined for mech");
    }
    
    if (inputAttrs) {
	CFRELEASE_NULL(inputAttrs);
    }
}

void
do_Fetch(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef uuid = HeimCredCopyUUID(request, "uuid");
    if (uuid == NULL)
	return;

    if (!checkACLInCredentialChain(peer, uuid, NULL)) {
	CFRELEASE_NULL(uuid);
	return;
    }
    
    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
    CFRELEASE_NULL(uuid);
    if (cred == NULL)
	return;
    
    if (!commandCanReadforMech(cred->mech, "fetch")) {
	return;
    }
    
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


void
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
	CFRELEASE_NULL(error);
    }
    CFRELEASE_NULL(array);
    CFRELEASE_NULL(items);
}

void
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
	 CFRELEASE_NULL(error);
     }
    
    if (mechName) {
	CFRELEASE_NULL(mechName);
    }
	
}

void
do_Move(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFErrorRef error = NULL;

    CFUUIDRef from = HeimCredMessageCopyAttributes(request, "from", CFUUIDGetTypeID());
    CFUUIDRef to = HeimCredMessageCopyAttributes(request, "to", CFUUIDGetTypeID());
    
    if (from == NULL || to == NULL || CFEqual(from, to)) {
	CFRELEASE_NULL(from);
	CFRELEASE_NULL(to);

	os_log_error(GSSOSLog(), "move missing required values");
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(&error, kHeimCredErrorMissingRequiredValue, keys, values, 1);
	addErrorToReply(reply, error);
	return;
    }

    if (!checkACLInCredentialChain(peer, from, NULL) || !checkACLInCredentialChain(peer, to, NULL)) {
	os_log_error(GSSOSLog(), "no access to caches");
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(&error, kHeimCredErrorUnknownKey, keys, values, 1);
	addErrorToReply(reply, error);

	CFRELEASE_NULL(from);
	CFRELEASE_NULL(to);
	return;
    }
    
    HeimCredRef credfrom = (HeimCredRef)CFDictionaryGetValue(peer->session->items, from);
    HeimCredRef credto = (HeimCredRef)CFDictionaryGetValue(peer->session->items, to);
    struct HeimMech *credToMech = NULL;
    
    if (credfrom == NULL) {
	os_log_error(GSSOSLog(), "from is empty");
	// this is not an error because the intent is to move the contents of "from" to "to".  while this cant really happen, there is nothing to move which means that "from" can safely be deleted.
	CFRELEASE_NULL(from);
	CFRELEASE_NULL(to);
	return;
    }
    heim_assert(credfrom != credto, "must not be same");

    if (credto && !CFEqual(credfrom->mech->name, credto->mech->name)) {
	os_log_error(GSSOSLog(), "moving between mechs is not supported");
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(&error, kHeimCredErrorUpdateNotAllowed, keys, values, 1);
	addErrorToReply(reply, error);

	CFRELEASE_NULL(from);
	CFRELEASE_NULL(to);
	return;
    }

    CFMutableDictionaryRef newattrs = CFDictionaryCreateMutableCopy(NULL, 0, credfrom->attributes);
    CFDictionaryRemoveValue(peer->session->items, from);
    credfrom = NULL;

    if (isTemporaryCache(newattrs) && (credto == NULL || (credto && !isTemporaryCache(credto->attributes)))) {
	CFDictionaryRemoveValue(newattrs, kHEIMAttrTemporaryCache);
    }

    CFDictionarySetValue(newattrs, kHEIMAttrUUID, to);

    if (credto == NULL) {
	credto = HeimCredCreateItem(to);
	heim_assert(credto != NULL, "out of memory");

	HeimCredAssignMech(credto, newattrs);

	credto->attributes = newattrs;
	CFDictionarySetValue(peer->session->items, credto->uuid, credto);
	credToMech = credto->mech;
	CFRELEASE_NULL(credto);

    } else {
	CFUUIDRef parentUUID = CFDictionaryGetValue(credto->attributes, kHEIMAttrParentCredential);
	if (parentUUID) {
	    CFDictionarySetValue(newattrs, kHEIMAttrParentCredential, parentUUID);
	}

	// if the destination was the default credential, then keep it as the default
	CFBooleanRef defaultCredential = CFDictionaryGetValue(credto->attributes, kHEIMAttrDefaultCredential);
	if (defaultCredential != NULL && CFBooleanGetValue(defaultCredential)) {
	    CFDictionarySetValue(newattrs, kHEIMAttrDefaultCredential, kCFBooleanTrue);
	}

	//if the "from" was the default, then keep it as the default
	CFBooleanRef wasDefaultCredential = CFDictionaryGetValue(newattrs, kHEIMAttrDefaultCredential);
	if (wasDefaultCredential != NULL && CFBooleanGetValue(wasDefaultCredential)) {
	    CFDictionarySetValue(peer->session->defaultCredentials, credto->mech->name, credto->uuid);
	}

	CFRELEASE_NULL(credto->attributes);
	credto->attributes = newattrs;
	credToMech = credto->mech;
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

    if (credToMech!=NULL && credToMech->notifyCaches!=NULL) {
	credToMech->notifyCaches();
	credToMech = NULL;
    }
    HeimCredCTX.needFlush = 1;
}

static void
StatusCredential(const void *key, const void *value, void *context)
{
    HeimCredRef cred = (HeimCredRef)value;
    xpc_object_t sc = (__bridge xpc_object_t)context;
    
    NSString *us = CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid));
    if (cred->mech->statusCallback!=NULL) {
	CFDictionaryRef status = cred->mech->statusCallback(cred);
	if (status!=nil && us!=nil) {
	    xpc_object_t xc = _CFXPCCreateXPCObjectFromCFObject(status);
	    CFRELEASE_NULL(status);
	    if (xc!=nil) {
		xpc_dictionary_set_value(sc, ([us UTF8String] ?: "none"), xc);
	    } else {
		os_log_error(GSSOSLog(), "status callback failed to convert to xpc object");
	    }
	}
    }
    
}

static void
StatusSession(const void *key, const void *value, void *context)
{
    struct HeimSession *session = (struct HeimSession *)value;
    xpc_object_t ss = (__bridge xpc_object_t)context;
    xpc_object_t sc = xpc_dictionary_create(NULL, NULL, 0);
    CFDictionaryApplyFunction(session->items, StatusCredential, (__bridge void *)(sc));
    CFStringRef sessionID = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@"), key);
    char *s = rk_cfstring2cstring(sessionID);
    xpc_dictionary_set_value(ss, s, sc);
    free(s);
    CFRELEASE_NULL(sessionID);
}

void
do_Status(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    uid_t uid = HeimCredGlobalCTX.getUid(peer->peer);
    struct stat sb;


    if (uid == 0 || HeimCredGlobalCTX.hasEntitlement(peer, "com.apple.private.gssapi.credential-introspection")) {
	xpc_object_t ss = xpc_dictionary_create(NULL, NULL, 0);

	CFDictionaryApplyFunction(HeimCredCTX.sessions, StatusSession, (__bridge void *)(ss));
	xpc_dictionary_set_value(reply, "items", ss);
    }

    if (stat([archivePath UTF8String], &sb) == 0) {
	xpc_dictionary_set_int64(reply, "cache-size", (int64_t)sb.st_size);
    } else {
	xpc_dictionary_set_int64(reply, "cache-size", 0);
    }
}

void
do_DeleteAll(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFErrorRef error = NULL;
    CFArrayRef items = NULL;
    
#if TARGET_OS_OSX
    const void *const keys[] = { CFSTR("CommonErrorCode") };
    const void *const values[] = { kCFBooleanTrue };
    HCMakeError(&error, kHeimCredErrorCommandUnavailable, keys, values, 1);
    goto out;
#else
    uid_t uid = HeimCredGlobalCTX.getUid(peer->peer);
    if (uid == 0 && HeimCredGlobalCTX.hasEntitlement(peer, "com.apple.private.gssapi.deleteall")) {
	os_log_info(GSSOSLog(), "Start of delete all");
	items = QueryCopy(peer, request, "query");
	if (items == NULL) {
	    const void *const keys[] = { CFSTR("CommonErrorCode") };
	    const void *const values[] = { kCFBooleanTrue };
	    HCMakeError(&error, kHeimCredErrorNoItemsMatchesQuery, keys, values, 1);
	    goto out;
	}
	
	CFIndex n, count = CFArrayGetCount(items);
	for (n = 0; n < count; n++) {
	    HeimCredRef cred = (HeimCredRef)CFArrayGetValueAtIndex(items, n);
	    deleteCredInternal(cred, peer);
	}
	
	HeimCredCTX.needFlush = 1;
    } else {
	const void *const keys[] = { CFSTR("CommonErrorCode") };
	const void *const values[] = { kCFBooleanTrue };
	HCMakeError(&error, kHeimCredErrorMissingEntitlement, keys, values, 1);
	os_log_error(GSSOSLog(), "Error in DeleteAll: %@", error);
    }
#endif
    out:
    if (error) {
	addErrorToReply(reply, error);
	CFRELEASE_NULL(error);
    }
    CFRELEASE_NULL(items);

}

void
do_RetainCache(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef uuid = HeimCredMessageCopyAttributes(request, "uuid", CFUUIDGetTypeID());
    CFMutableDictionaryRef attrs;
    CFErrorRef error = NULL;

    if (uuid == NULL)
	return;

    os_log_debug(GSSOSLog(), "do_RetainCache: %@", uuid);

    if (!checkACLInCredentialChain(peer, uuid, NULL)) {
	CFRELEASE_NULL(uuid);
	return;
    }

    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
    CFRELEASE_NULL(uuid);
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

    NSNumber *count = nil;
    int newCount = -1;
    count = CFDictionaryGetValue(attrs, kHEIMAttrRetainStatus);
    if (count != nil) {
	newCount = [count intValue];
	newCount++;
	os_log_debug(GSSOSLog(), "the new count is %d for %@", newCount, cred->uuid);
	CFDictionarySetValue(attrs, kHEIMAttrRetainStatus, (__bridge const void *)@(newCount));
    } else {
	os_log_error(GSSOSLog(), "the retain count is missing: %@", cred->uuid);
	CFRELEASE_NULL(attrs);
	goto out;
    }

    CFRELEASE_NULL(cred->attributes);
    cred->attributes = attrs;

    HeimCredCTX.needFlush = 1;

    out:
    CFRELEASE_NULL(error);
}

void
do_ReleaseCache(struct peer *peer, xpc_object_t request, xpc_object_t reply)
{
    CFUUIDRef uuid = HeimCredMessageCopyAttributes(request, "uuid", CFUUIDGetTypeID());
    CFMutableDictionaryRef attrs;
    CFErrorRef error = NULL;

    if (uuid == NULL)
	return;

    os_log_debug(GSSOSLog(), "do_ReleaseCache: %@", uuid);

    if (!checkACLInCredentialChain(peer, uuid, NULL)) {
	CFRELEASE_NULL(uuid);
	return;
    }

    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(peer->session->items, uuid);
    CFRELEASE_NULL(uuid);
    if (cred == NULL)
	return;

    CFRetain(cred);  //cred is retained in case we delete it

    heim_assert(CFGetTypeID(cred) == HeimCredGetTypeID(), "cred wrong type");

    if (cred->attributes) {
	attrs = CFDictionaryCreateMutableCopy(NULL, 0, cred->attributes);
	if (attrs == NULL) {
	    CFRelease(cred);
	    return;
	}
    } else {
	attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    NSNumber *count = nil;
    int newCount = -1;
    count = CFDictionaryGetValue(attrs, kHEIMAttrRetainStatus);
    if (count != nil) {
	newCount = [count intValue];
	newCount--;
	os_log_debug(GSSOSLog(), "the new count is %d for %@", newCount, cred->uuid);
	CFDictionarySetValue(attrs, kHEIMAttrRetainStatus, (__bridge const void *)@(newCount));
    } else {
	os_log_error(GSSOSLog(), "the retain count is missing: %@", cred->uuid);
	CFRELEASE_NULL(attrs);
	goto out;
    }

    CFRELEASE_NULL(cred->attributes);
    cred->attributes = attrs;

    HeimCredCTX.needFlush = 1;

    if (newCount < 1) {
	os_log_debug(GSSOSLog(), "the new count is deleting cache for %@", cred->uuid);
	//if the last reference to the cred is released, then delete the cache
	deleteCredInternal(cred, peer);
    }

    out:
    CFRELEASE_NULL(error);
    CFRELEASE_NULL(cred);
}

/*
 *
 */

struct HeimSession *
HeimCredCopySession(int sessionID)
{
    struct HeimSession *session;
    CFNumberRef sid;

    sid = CFNumberCreate(NULL, kCFNumberIntType, &sessionID);
    heim_assert(sid != NULL, "out of memory");


    session = (struct HeimSession *)CFDictionaryGetValue(HeimCredCTX.sessions, sid);
    if (session) {
	CFRELEASE_NULL(sid);
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
    CFRELEASE_NULL(sid);

    return session;
}


static void
addErrorToReply(xpc_object_t reply, CFErrorRef error)
{
    if (error == NULL)
	return;

    xpc_object_t xe = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_int64(xe, "error-code", CFErrorGetCode(error));
    xpc_dictionary_set_value(reply, "error", xe);
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
	os_log_error(GSSOSLog(), "unknown key");
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
	    os_log_error(GSSOSLog(), "key missing key");
	}
    } else {
	CFTypeID expectedType = GetTypeFromSchemaRule(key, rule, true);

	if (expectedType != CFGetTypeID(ov)) {
	    const void *const keys[] = { CFSTR("Key"), CFSTR("Rule"), CFSTR("CommonErrorCode") };
	    const void *const values[] = { key, rule, kCFBooleanTrue };
	    HCMakeError(ctx->error, kHeimCredErrorMissingSchemaKey, keys, values, 3);
	    ctx->valid = false;
	    os_log_error(GSSOSLog(), "key have wrong type key");
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
		      HeimCredAuthCallback authCallback,
		      HeimCredNotifyCaches notifyCaches,
		      bool readRestricted,
		      CFArrayRef readOnlyCommands)
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
    mech->notifyCaches = notifyCaches;
    mech->readRestricted = readRestricted;
    mech->readOnlyCommands = readOnlyCommands;

    CFDictionarySetValue(HeimCredCTX.mechanisms, name, mech);
    CFRELEASE_NULL(mech);
    
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
    CFDictionarySetValue(schema, kHEIMAttrAltDSID, CFSTR("s"));
    CFDictionarySetValue(schema, kHEIMAttrUserID, CFSTR("n"));
    CFDictionarySetValue(schema, kHEIMAttrASID, CFSTR("n"));
    CFDictionarySetValue(schema, kHEIMAttrTemporaryCache, CFSTR("b"));

#if 0
    CFDictionarySetValue(schema, kHEIMAttrTransient, CFSTR("b"));
    CFDictionarySetValue(schema, kHEIMAttrAllowedDomain, CFSTR("as"));
    CFDictionarySetValue(schema, kHEIMAttrStatus, CFSTR(""));
    CFDictionarySetValue(schema, kHEIMAttrExpire, CFSTR("t"));
    CFDictionarySetValue(schema, kHEIMAttrRenewTill, CFSTR("t"));
#endif

    return schema;
}

void
_HeimCredRegisterGeneric(void)
{
    CFMutableSetRef set;
    CFMutableDictionaryRef schema;

    set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    schema = _HeimCredCreateBaseSchema(kHEIMObjectGeneric);

    CFSetAddValue(set, schema);
    CFRELEASE_NULL(schema);
    _HeimCredRegisterMech(kHEIMTypeGeneric, set, KerberosStatusCallback, NULL, NULL, false, NULL);
    CFRELEASE_NULL(set);
}

void
_HeimCredRegisterConfiguration(void)
{
    CFMutableSetRef set;
    CFMutableDictionaryRef schema;

    set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    schema = _HeimCredCreateBaseSchema(kHEIMObjectConfiguration);

    CFSetAddValue(set, schema);
    CFRELEASE_NULL(schema);
    _HeimCredRegisterMech(kHEIMTypeConfiguration, set, ConfigurationStatusCallback, NULL, NULL, false, NULL);
    CFRELEASE_NULL(set);
}


void
peer_final(void *ptr)
{
    struct peer *peer = ptr;
    CFRELEASE_NULL(peer->bundleID);
    CFRELEASE_NULL(peer->callingAppBundleID);
    CFRELEASE_NULL(peer->currentDSID);
    if (peer->session) {
	CFRelease(peer->session); //session is retained after this is called
	peer->session = NULL;
    }
    free(peer);
}

static void
HeimCredRemoveItemsForASID(struct HeimSession *session, au_asid_t asid)
{
    os_log_debug(GSSOSLog(), "HeimCredRemoveItemsForASID: %d", asid);
    NSMutableArray *itemsToDelete = [@[] mutableCopy];
    NSNumber *asidToDelete = [NSNumber numberWithInt:asid];
    
    NSDictionary *items = (__bridge NSDictionary *)session->items;
    [items enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
	HeimCredRef cred = (__bridge HeimCredRef)obj;
	if (cred && cred->attributes) {
	    NSNumber *credASID = (__bridge NSNumber *)CFDictionaryGetValue(cred->attributes, kHEIMAttrASID);
	    if ([asidToDelete isEqualToNumber:credASID]) {
		[itemsToDelete addObject:key];
		os_log_debug(GSSOSLog(), "item to be deleted: %@", key);
	    }
	}
    }];
    
    [itemsToDelete enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
	CFUUIDRef uuid = (__bridge CFUUIDRef)obj;
	HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(session->items, uuid);
	if (cred) {
	    CFDictionaryRemoveValue(session->items, uuid);
	    os_log_debug(GSSOSLog(), "item deleted: %@", uuid);
	}
    }];
    
}

void
RemoveSession(au_asid_t asid)
{

    if (HeimCredGlobalCTX.useUidMatching) {
	@autoreleasepool {
	    CFNumberRef asidNumber = CFNumberCreate(NULL, kCFNumberIntType, &asid);
	    struct HeimSession *session = (struct HeimSession *)CFDictionaryGetValue(HeimCredCTX.sessions, (__bridge const void *)@(-1));
	    if (session) {
		HeimCredRemoveItemsForASID(session, asid);
	    }
	    os_log_info(GSSOSLog(), "Session ended: %@", asidNumber);
	    CFRELEASE_NULL(asidNumber);
	}
    } else {
	CFNumberRef sid = CFNumberCreate(NULL, kCFNumberIntType, &asid);
	heim_assert(sid != NULL, "out of memory");
	CFDictionaryRemoveValue(HeimCredCTX.sessions, sid);
	os_log_info(GSSOSLog(), "Session ended: %@", sid);
	CFRELEASE_NULL(sid);
    }

}

static void
addUUIDAttributeStringToDictionary(NSDictionary *sourceDictionary, NSString *sourceAttribute, NSMutableDictionary *destinationDictionary, NSString *destinationAttribute)
{
    CFUUIDRef uuid = (__bridge CFUUIDRef)sourceDictionary[sourceAttribute];
    if (uuid!=NULL) {
	CFStringRef uuidstr = CFUUIDCreateString(NULL, uuid);
	if (uuidstr!=NULL) {
	    destinationDictionary[destinationAttribute] = (__bridge id)(uuidstr);
	    CFRELEASE_NULL(uuidstr);
	}
    }
}

CFTypeRef
KerberosStatusCallback(HeimCredRef cred) CF_RETURNS_RETAINED
{
    NSDictionary *attributes = (__bridge NSDictionary *)cred->attributes;
    NSMutableDictionary *result = [@{} mutableCopy];
    if (attributes[(id)kHEIMAttrParentCredential] != nil) {
	result[@"name"] = attributes[(id)kHEIMAttrClientName];
	result[@"serverName"] = attributes[(id)kHEIMAttrServerName];
	result[@"credExpire"] = attributes[(id)kHEIMAttrExpire];
	result[@"authTime"] = attributes[(id)kHEIMAttrAuthTime];
	result[@"renewTill"] = attributes[(id)kHEIMAttrRenewTill];
	HEIMDAL_MUTEX_lock(&cred->event_mutex);
	result[@"renewTime"] = [NSDate dateWithTimeIntervalSince1970:cred->renew_time];
	result[@"acquireStatus"] = [NSNumber numberWithInt:cred->acquire_status];
	result[@"expire"] = [NSDate dateWithTimeIntervalSince1970:cred->expire];
	HEIMDAL_MUTEX_unlock(&cred->event_mutex);
	result[@"mech"] = (__bridge id _Nullable)cred->mech->name;
	addUUIDAttributeStringToDictionary(attributes, (id)kHEIMAttrUUID, result, @"uuid");
	addUUIDAttributeStringToDictionary(attributes, (id)kHEIMAttrParentCredential, result, @"parent");
    } else {
	result[@"name"] = attributes[(id)kHEIMAttrClientName];
	result[@"acl"] = attributes[(id)kHEIMAttrBundleIdentifierACL];
	result[@"altdsid"] = attributes[(id)kHEIMAttrAltDSID];
	result[@"userId"] = attributes[(id)kHEIMAttrUserID];
	result[@"retainStatus"] = attributes[(id)kHEIMAttrRetainStatus];
	addUUIDAttributeStringToDictionary(attributes, (id)kHEIMAttrUUID, result, @"uuid");
    }
    return CFBridgingRetain(result);
}


CFTypeRef
KerberosAcquireCredStatusCallback(HeimCredRef cred) CF_RETURNS_RETAINED
{
    NSDictionary *attributes = (__bridge NSDictionary *)cred->attributes;
    
    NSMutableDictionary *result = [@{} mutableCopy];
    result[@"name"] = attributes[(id)kHEIMAttrClientName];
    result[@"credExpire"] = attributes[(id)kHEIMAttrExpire];
    result[@"status"] = attributes[(id)kHEIMAttrStatus];
    result[@"authTime"] = attributes[(id)kHEIMAttrAuthTime];
    HEIMDAL_MUTEX_lock(&cred->event_mutex);
    result[@"renewTime"] = [NSDate dateWithTimeIntervalSince1970:cred->renew_time];
    result[@"nextAcquireTime"] = [NSDate dateWithTimeIntervalSince1970:cred->next_acquire_time];
    result[@"expire"] = [NSDate dateWithTimeIntervalSince1970:cred->expire];
    result[@"acquireStatus"] = [NSNumber numberWithInt:cred->acquire_status];
    HEIMDAL_MUTEX_unlock(&cred->event_mutex);
    result[@"mech"] = (__bridge id _Nullable)cred->mech->name;
    addUUIDAttributeStringToDictionary(attributes, (id)kHEIMAttrUUID, result, @"uuid");
    addUUIDAttributeStringToDictionary(attributes, (id)kHEIMAttrParentCredential, result, @"parent");
    return CFBridgingRetain(result);
}

CFTypeRef
ConfigurationStatusCallback(HeimCredRef cred) CF_RETURNS_RETAINED
{
    NSDictionary *attributes = (__bridge NSDictionary *)cred->attributes;
    
    NSMutableDictionary *result = [@{} mutableCopy];
    result[@"name"] = attributes[(id)kHEIMAttrClientName];
    result[@"servername"] = (attributes[(id)kHEIMAttrServerName] ?: @"none");
    result[@"authTime"] = attributes[(id)kHEIMAttrAuthTime];
    result[@"userId"] = attributes[(id)kHEIMAttrUserID];
    result[@"mech"] = (__bridge id _Nullable)cred->mech->name;
    addUUIDAttributeStringToDictionary(attributes, (id)kHEIMAttrUUID, result, @"uuid");
    addUUIDAttributeStringToDictionary(attributes, (id)kHEIMAttrParentCredential, result, @"parent");
    return CFBridgingRetain(result);
}


