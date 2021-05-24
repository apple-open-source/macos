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

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFXPCBridge.h>
#import "gsscred.h"
#import "heimcred.h"
#import "mock_aks.h"
#import "GSSCredTestUtil.h"

@interface GSSCredTestUtil ()


@end

@implementation GSSCredTestUtil {
    
}

#pragma mark -
#pragma mark peer

+ (struct peer *)createPeer:(NSString *)bundle identifier:(int)sid {
    return [self createPeer:bundle callingBundleId:nil identifier:sid];
}

+ (struct peer *)createPeer:(NSString *)bundle callingBundleId:(NSString * _Nullable)callingApp identifier:(int)sid
{
    struct peer * p = calloc(1, sizeof(*p));
    p->bundleID = (__bridge CFStringRef)bundle;
    p->callingAppBundleID = (__bridge CFStringRef)(callingApp ? : bundle);
    p->session = HeimCredCopySession(sid);
    p->needsManagedAppCheck = true;
    p->isManagedApp = false;
    return p;
}

+ (void)freePeer:(struct peer *)ptr {
    if (ptr != NULL) {
	peer_final(ptr);
	ptr = NULL;
    }
}

#pragma mark -
#pragma mark create

//create a standard kerberos id returning only the uuid.
+ (BOOL)createCredentialAndCache:(struct peer *)peer name:(NSString*)clientName returningCacheUuid:(CFUUIDRef *)uuid
{
    return [self createCredentialAndCache:peer name:clientName returningCacheUuid:uuid credentialUUID:NULL];
}

//create a standard kerberos id returning only the cache uuid and cred.
+ (BOOL)createCredentialAndCache:(struct peer *)peer name:(NSString*)clientName returningCacheUuid:(CFUUIDRef *)cacheUUID credentialUUID:(CFUUIDRef *)credUUID
{
    CFDictionaryRef replyAttributes;
    bool worked = [GSSCredTestUtil createCredentialAndCache:peer name:clientName returningCredentialDictionary:&replyAttributes];
    if (worked && replyAttributes) {

	if (credUUID) {
	    *credUUID = CFDictionaryGetValue(replyAttributes, kHEIMAttrUUID);
	    if (*credUUID) CFRetain(*credUUID);
	}

	if (cacheUUID) {
	    if (CFDictionaryContainsKey(replyAttributes, kHEIMAttrParentCredential)) {
		*cacheUUID = CFDictionaryGetValue(replyAttributes, kHEIMAttrParentCredential);
		if (*cacheUUID) CFRetain(*cacheUUID);
	    }
	}
	CFRelease(replyAttributes);
	[self flushCache]; //we have to manually save, because GSSCred does this as part of the XPC handler
	return YES;

    }

    return NO;

}

//create a standard kerberos id returning the dictonary of attributes
+ (BOOL)createCredentialAndCache:(struct peer *)peer name:(NSString*)clientName returningCredentialDictionary:(CFDictionaryRef *)dict
{
    
    CFUUIDRef parentUUID = NULL;
    NSDictionary *parentAttributes = @{ };
    
    [self createCredential:peer name:clientName attributes:(__bridge CFDictionaryRef)(parentAttributes) returningUuid:&parentUUID];
    
    if (parentUUID == NULL)
    {
	return false;
    }
    
    NSDictionary *attributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)parentUUID,
				  (id)kHEIMAttrLeadCredential:@YES,
				  (id)kHEIMAttrAuthTime:[NSDate date],
				  (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				  (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
				  (id)kHEIMAttrExpire:[NSDate dateWithTimeIntervalSinceNow:300]
    };
    CFRELEASE_NULL(parentUUID);
    return [GSSCredTestUtil createCredential:peer name:clientName attributes:(__bridge CFDictionaryRef)attributes returningDictionary:dict];
    
}

//create a special kerberos identity with the supplied attributes
+ (BOOL)createCredential:(struct peer *)peer name:(NSString*)clientName attributes:(CFDictionaryRef)attributes returningUuid:(CFUUIDRef *)uuid
{
    CFDictionaryRef replyAttributes;
    bool worked = [GSSCredTestUtil createCredential:peer name:clientName attributes:attributes returningDictionary:&replyAttributes];
    if (worked && replyAttributes) {
	*uuid = CFDictionaryGetValue(replyAttributes, kHEIMAttrUUID);
	if (*uuid) CFRetain(*uuid);
	CFRelease(replyAttributes);
	[self flushCache]; //we have to manually save, because GSSCred does this as part of the XPC handler
	return YES;
	
    }
    
    return NO;
    
}

+ (BOOL)createCredential:(struct peer *)peer name:(NSString*)clientName attributes:(CFDictionaryRef)attributes returningDictionary:(CFDictionaryRef *)dict
{
    
    CFMutableDictionaryRef allAttrs;
    if (attributes) {
	allAttrs = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
    } else {
	allAttrs = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    }
    CFDictionarySetValue(allAttrs, kHEIMObjectType, kHEIMObjectKerberos);
    CFDictionarySetValue(allAttrs, kHEIMAttrType, kHEIMTypeKerberos);
    CFDictionarySetValue(allAttrs, kHEIMAttrClientName, (__bridge const void *)(clientName));
    
    bool result = [self executeCreateCred:peer forAttributes:allAttrs returningDictionary:dict];
    CFRELEASE_NULL(allAttrs);
    return result;
}


//runs the raw create cred command
+ (BOOL)executeCreateCred:(struct peer * _Nullable)peer forAttributes:(CFDictionaryRef)allAttrs returningDictionary:(CFDictionaryRef * _Nonnull)dict
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "create");
    xpc_dictionary_set_int64(request, "version", 0);
    
    xpc_object_t requestAttributes = _CFXPCCreateXPCObjectFromCFObject(allAttrs);
    xpc_dictionary_set_value(request, "attributes", requestAttributes);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_CreateCred(peer, request, reply);
    
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NO;
    }
    
    *dict = HeimCredMessageCopyAttributes(reply, "attributes", CFDictionaryGetTypeID());
    if (!*dict) {
	return NO;
    }
    
    return YES;
}

+ (BOOL)createNTLMCredential:(struct peer *)peer returningUuid:(CFUUIDRef *)uuid
{
    CFDictionaryRef replyAttributes;
    bool worked = [GSSCredTestUtil createNTLMCredential:peer returningDictionary:&replyAttributes];
    if (worked && replyAttributes) {
	*uuid = CFDictionaryGetValue(replyAttributes, kHEIMAttrUUID);
	if (*uuid) CFRetain(*uuid);
	CFRelease(replyAttributes);
	[self flushCache]; //we have to manually save, because GSSCred does this as part of the XPC handler
	return YES;
    }
    
    return NO;
    
}

+ (BOOL)createNTLMCredential:(struct peer *)peer returningDictionary:(CFDictionaryRef *)dict
{
    
    return [GSSCredTestUtil createNTLMCredential:peer attributes:NULL returningDictionary:dict];
}

+ (BOOL)createNTLMCredential:(struct peer *)peer attributes:(CFDictionaryRef)attributes returningDictionary:(CFDictionaryRef *)dict
{
    
    CFMutableDictionaryRef allAttrs;
    if (attributes) {
	allAttrs = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
    } else {
	allAttrs = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    }
    
    CFDictionarySetValue(allAttrs, kHEIMObjectType, kHEIMObjectNTLM);
    CFDictionarySetValue(allAttrs, kHEIMAttrType, kHEIMTypeNTLM);
    CFDictionarySetValue(allAttrs, kHEIMAttrNTLMUsername, CFSTR("foo"));
    CFDictionarySetValue(allAttrs, kHEIMAttrNTLMDomain, CFSTR("bar"));
    
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "auth");
    xpc_dictionary_set_int64(request, "version", 0);
    
    xpc_object_t requestAttributes = _CFXPCCreateXPCObjectFromCFObject(allAttrs);
    CFRelease(allAttrs);
    xpc_dictionary_set_value(request, "attributes", requestAttributes);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_CreateCred(peer, request, reply);
    
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NO;
    }
    
    *dict = HeimCredMessageCopyAttributes(reply, "attributes", CFDictionaryGetTypeID());
    if (!*dict) {
	return NO;
    }
    
    return YES;
}

#pragma mark -
#pragma mark move

+ (BOOL)move:(struct peer * _Nullable)peer from:(CFUUIDRef)from to:(CFUUIDRef)to
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "move");
    xpc_dictionary_set_int64(request, "version", 0);

    HeimCredSetUUID(request, "from", from);
    HeimCredSetUUID(request, "to", to);

    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_Move(peer, request, reply);

    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NO;
    }

   return YES;
}

#pragma mark -
#pragma mark fetch

+ (BOOL)fetchCredential:(struct peer *)peer uuid:(CFUUIDRef)uuid
{
    CFDictionaryRef replyAttributes = NULL;
    bool worked = [GSSCredTestUtil fetchCredential:peer uuid:uuid returningDictionary:&replyAttributes];
    if (replyAttributes) { CFRelease(replyAttributes); }
    if (!worked) {
	return NO;
    }
    
    return YES;
}

+ (BOOL)fetchCredential:(struct peer *)peer uuid:(CFUUIDRef) uuid returningDictionary:(CFDictionaryRef *)dict
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "fetch");
    xpc_dictionary_set_int64(request, "version", 0);
    
    if (uuid) {
	HeimCredSetUUID(request, "uuid", uuid);
    }
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_Fetch(peer, request, reply);
    
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NO;
    }
    
    *dict = HeimCredMessageCopyAttributes(reply, "attributes", CFDictionaryGetTypeID());
    if (!*dict) {
	return NO;
    }
    
    return YES;
}

+ (CFUUIDRef)getDefaultCredential:(struct peer *)peer CF_RETURNS_RETAINED
{
    const char *mech = [(NSString*)kHEIMTypeKerberos UTF8String];
    
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "default");
    xpc_dictionary_set_string(request, "mech", mech);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_GetDefault(peer, request, reply);
    
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NULL;
    }
    
    CFUUIDRef defaultCred = HeimCredMessageCopyAttributes(reply, "default", CFUUIDGetTypeID());
    
    [self flushCache];  //we must flush the cache here or the newly elected cred will not be saved to disk.  This is handled for us in server.m
    
    return defaultCred;
}

+ (BOOL)fetchDefaultCredential:(struct peer *)peer returningName:(CFStringRef *)name
{
    
    CFUUIDRef defCred = NULL;
    defCred = [GSSCredTestUtil getDefaultCredential:peer];
    
    CFDictionaryRef attrs = NULL;
    bool result = [GSSCredTestUtil fetchCredential:peer uuid:defCred returningDictionary:&attrs];
    CFRELEASE_NULL(defCred);
    if (result) {
	CFStringRef def = CFDictionaryGetValue(attrs, kHEIMAttrClientName);
	*name = CFRetain(def);
	CFRELEASE_NULL(attrs);
	return true;
    }
    return false;
}

#pragma mark -
#pragma mark query

+ (NSUInteger)itemCount:(struct peer *)peer
{
    NSDictionary *items = (__bridge NSDictionary *)(peer->session->items);
    return [items count];
}

+ (BOOL)queryAllKerberos:(struct peer *)peer returningArray:(NSArray * _Nonnull __autoreleasing *_Nonnull)items
{
    return [self queryAll:peer type:kHEIMTypeKerberos returningArray:items];
}

+ (BOOL)queryAll:(struct peer *)peer type:(CFStringRef)type returningArray:(NSArray * _Nonnull __autoreleasing *_Nonnull)items;
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "query");
    xpc_dictionary_set_int64(request, "version", 0);
    
    NSDictionary *query = @{(id)kHEIMAttrType:(__bridge id)type};
    
    xpc_object_t xpcquery = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)(query));
    xpc_dictionary_set_value(request, "query", xpcquery);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_Query(peer, request, reply);
    
    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NO;
    }
    
    NSArray *results = CFBridgingRelease(HeimCredMessageCopyAttributes(reply, "items", CFArrayGetTypeID()));
    *items = results;
    if (!*items) {
	return NO;
    }
    return YES;
}

+ (BOOL)queryAll:(struct peer *)peer parentUUID:(CFUUIDRef)parentUUID returningArray:(NSArray * _Nonnull __autoreleasing *_Nonnull)items
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "query");
    xpc_dictionary_set_int64(request, "version", 0);
    
    NSDictionary *query = @{(id)kHEIMAttrType:(id)kHEIMTypeKerberos, (id)kHEIMAttrParentCredential:(__bridge id)parentUUID};
    
    xpc_object_t xpcquery = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)(query));
    xpc_dictionary_set_value(request, "query", xpcquery);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_Query(peer, request, reply);
    
    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NO;
    }
    
    NSArray *all = CFBridgingRelease(HeimCredMessageCopyAttributes(reply, "items", CFArrayGetTypeID()));
    
    NSMutableArray *results = [@[] mutableCopy];
    
    [all enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
	CFDictionaryRef replyAttributes = NULL;
	CFUUIDRef uuid = (__bridge CFUUIDRef)obj;
	
	bool worked = [GSSCredTestUtil fetchCredential:peer uuid:uuid returningDictionary:&replyAttributes];
	if (worked && replyAttributes) {
	    NSString *client = CFDictionaryGetValue(replyAttributes, kHEIMAttrServerName);
	    [results addObject:client];
	}
	if (replyAttributes) CFRelease(replyAttributes);
    }];
    
    *items = results;
    if (!*items) {
	return NO;
    }
    return YES;
}

+ (BOOL)queryAllCredentials:(struct peer *)peer returningArray:(NSArray * _Nonnull __autoreleasing *_Nonnull)items
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "query");
    xpc_dictionary_set_int64(request, "version", 0);
    
    NSDictionary *query = @{(id)kHEIMAttrType:(id)kHEIMTypeKerberos,
			    (id)kHEIMAttrServerName:(id)kCFNull
    };
    
    xpc_object_t xpcquery = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)(query));
    xpc_dictionary_set_value(request, "query", xpcquery);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_Query(peer, request, reply);
    
    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NO;
    }
    
    NSArray *all = CFBridgingRelease(HeimCredMessageCopyAttributes(reply, "items", CFArrayGetTypeID()));
    
    NSMutableArray *results = [@[] mutableCopy];
    
    [all enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
	CFDictionaryRef replyAttributes = NULL;
	CFUUIDRef uuid = (__bridge CFUUIDRef)obj;
	
	bool worked = [GSSCredTestUtil fetchCredential:peer uuid:uuid returningDictionary:&replyAttributes];
	if (worked && replyAttributes) {
	    NSString *client = CFDictionaryGetValue(replyAttributes, kHEIMAttrClientName);
	    [results addObject:client];
	}
	if (replyAttributes)
	    CFRelease(replyAttributes);
    }];
    
    *items = results;
    if (!*items) {
	return NO;
    }
    return YES;
}

+ (void)showStatus:(struct peer *)peer
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_Status(peer, request, reply);
    NSDictionary *response = CFBridgingRelease(_CFXPCCreateCFObjectFromXPCObject(reply));
    NSLog(@"%@", response);
    
}

#pragma mark -
#pragma mark update

+ (int64_t)setAttributes:(struct peer * _Nullable)peer uuid:(CFUUIDRef) uuid attributes:(CFDictionaryRef _Nonnull)attributes returningDictionary:(CFDictionaryRef _Nonnull * _Nullable)dict {
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "fetch");
    xpc_dictionary_set_int64(request, "version", 0);
    
    if (uuid) {
	HeimCredSetUUID(request, "uuid", uuid);
    }
    
    xpc_object_t requestAttributes = _CFXPCCreateXPCObjectFromCFObject(attributes);
    xpc_dictionary_set_value(request, "attributes", requestAttributes);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_SetAttrs(peer, request, reply);
    
    [self flushCache]; //we have to manually save, because GSSCred does this as part of the XPC handler
    
    if (dict) {
	*dict = HeimCredMessageCopyAttributes(reply, "attributes", CFDictionaryGetTypeID());
    }
    
    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	int64_t errorCode = xpc_dictionary_get_int64(error, "error-code");
	return errorCode;
    }
    return 0;
}

#pragma mark -
#pragma mark delete

+ (int64_t)delete:(struct peer *)peer uuid:(CFUUIDRef)uuid
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "delete");
    xpc_dictionary_set_int64(request, "version", 0);
    
    NSDictionary *query = @{(id)kHEIMAttrUUID:(__bridge id)uuid};  //this is the same as delete by uuid run for destroy on a cache
    
    xpc_object_t xpcquery = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)(query));
    xpc_dictionary_set_value(request, "query", xpcquery);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_Delete(peer, request, reply);
    
    [self flushCache]; //we have to manually save, because GSSCred does this as part of the XPC handler
    
    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	int64_t errorCode = xpc_dictionary_get_int64(error, "error-code");
	return errorCode;
    }
    return 0;
}

+ (int64_t)deleteAll:(struct peer *)peer dsid:(NSString *)dsid
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "delete-all");
    xpc_dictionary_set_int64(request, "version", 0);
    
    NSDictionary *query = @{(id)kHEIMAttrAltDSID:dsid, (id)kHEIMObjectType:(id)kHEIMObjectAny};
    
    xpc_object_t xpcquery = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)(query));
    xpc_dictionary_set_value(request, "query", xpcquery);
    
    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_DeleteAll(peer, request, reply);
    
    [self flushCache]; //we have to manually save, because GSSCred does this as part of the XPC handler
    
    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	int64_t errorCode = xpc_dictionary_get_int64(error, "error-code");
	return errorCode;
    }
    return 0;
}

+ (BOOL)deleteCacheContents:(struct peer *)peer parentUUID:(CFUUIDRef)parentUUID
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "query");
    xpc_dictionary_set_int64(request, "version", 0);

    NSDictionary *query = @{(id)kHEIMAttrType:(id)kHEIMTypeKerberos, (id)kHEIMAttrParentCredential:(__bridge id)parentUUID};

    xpc_object_t xpcquery = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)(query));
    xpc_dictionary_set_value(request, "query", xpcquery);

    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_Query(peer, request, reply);

    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	return NO;
    }

    NSArray *all = (__bridge NSArray *)(HeimCredMessageCopyAttributes(reply, "items", CFArrayGetTypeID()));


    [all enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
	CFUUIDRef uuid = (__bridge CFUUIDRef)obj;
	[GSSCredTestUtil delete:peer uuid:uuid];
    }];

    return YES;
}
#pragma mark -
#pragma mark hold

+ (int64_t)hold:(struct peer *)peer uuid:(CFUUIDRef)uuid
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "retain-transient");
    xpc_dictionary_set_int64(request, "version", 0);

    HeimCredSetUUID(request, "uuid", uuid);

    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_RetainCache(peer, request, reply);

    [self flushCache]; //we have to manually save, because GSSCred does this as part of the XPC handler

    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	int64_t errorCode = xpc_dictionary_get_int64(error, "error-code");
	return errorCode;
    }
    return 0;
}

+ (int64_t)unhold:(struct peer *)peer uuid:(CFUUIDRef)uuid
{
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(request, "command", "release-transient");
    xpc_dictionary_set_int64(request, "version", 0);

    HeimCredSetUUID(request, "uuid", uuid);

    xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
    do_ReleaseCache(peer, request, reply);

    [self flushCache]; //we have to manually save, because GSSCred does this as part of the XPC handler

    //check for error
    xpc_object_t error = xpc_dictionary_get_dictionary(reply, "error");
    if (error) {
	int64_t errorCode = xpc_dictionary_get_int64(error, "error-code");
	return errorCode;
    }
    return 0;
}

#pragma mark -
#pragma mark utility

+ (void)flushCache
{
    static NSObject *_lock;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
	_lock = [NSObject new];
    });

    //dispatch to main to prevent concurrency issues with the event threads.
    dispatch_sync(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
	@synchronized (_lock) {
	    if (HeimCredCTX.needFlush) {
		HeimCredCTX.needFlush = false;
		storeCredCache();
	    }
	}
    });
}


@end





