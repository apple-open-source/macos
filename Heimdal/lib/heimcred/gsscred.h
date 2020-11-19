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

#import <xpc/xpc.h>
#import <Foundation/Foundation.h>
#import <CoreFoundation/CFRuntime.h>
#import <os/log.h>
#import <heim-ipc.h>
#import "common.h"
#import "GSSCredHelperClient.h"

#ifndef gsscred_h
#define gsscred_h

typedef enum {
    IAKERB_NOT_CHECKED = 0,
    IAKERB_ACCESS_DENIED = 1,
    IAKERB_ACCESS_GRANTED = 2
} iakerb_access_status;

struct peer {
    xpc_connection_t peer;
    CFStringRef bundleID;
    CFStringRef callingAppBundleID;
    struct HeimSession *session;
    bool needsManagedAppCheck;
    bool isManagedApp;
    CFStringRef currentDSID;
    iakerb_access_status access_status;
};

@protocol ManagedAppProvider <NSObject>

- (BOOL)isManagedApp:(NSString*)bundleId;

@end

typedef NSString * (*HeimCredCurrentAltDSID)(void);
typedef bool (*HeimCredHasEntitlement)(struct peer *, const char *);
typedef uid_t (*HeimCredGetUid)(xpc_connection_t);
typedef NSData * (*HeimCredEncryptData)(NSData *);
typedef NSData * (*HeimCredDecryptData)(NSData *);
typedef au_asid_t (*HeimCredGetAsid)(xpc_connection_t);
typedef bool (*HeimCredVerifyAppleSigned)(struct peer *, NSString *);
typedef bool (*HeimCredSessionExists)(pid_t asid);
typedef void (*HeimCredSaveToDiskIfNeeded)(void);
typedef CFPropertyListRef (*HeimCredGetValueFromPreferences)(CFStringRef);
typedef void (*HeimExecuteOnRunQueue)(dispatch_block_t);

typedef struct {
    bool isMultiUser;
    HeimCredCurrentAltDSID currentAltDSID;
    HeimCredHasEntitlement hasEntitlement;
    HeimCredGetUid getUid;
    HeimCredGetAsid getAsid;
    HeimCredEncryptData encryptData;
    HeimCredDecryptData decryptData;
    HeimCredVerifyAppleSigned verifyAppleSigned;
    HeimCredSessionExists sessionExists;
    id<ManagedAppProvider> managedAppManager;
    bool useUidMatching;
    HeimCredSaveToDiskIfNeeded saveToDiskIfNeeded;
    HeimCredGetValueFromPreferences getValueFromPreferences;
    heim_ipc_event_callback_t expireFunction;
    heim_ipc_event_callback_t renewFunction;
    heim_ipc_event_final_t finalFunction;
    HeimCredNotifyCaches notifyCaches;
    time_t renewInterval;
    Class<GSSCredHelperClient> gssCredHelperClientClass;
    HeimExecuteOnRunQueue executeOnRunQueue;
} HeimCredGlobalContext;

extern HeimCredGlobalContext HeimCredGlobalCTX;

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
    HeimCredNotifyCaches notifyCaches;
    bool readRestricted;
    CFArrayRef readOnlyCommands;
};

os_log_t GSSOSLog(void);

typedef enum {
    READ_SUCCESS = 0,
    READ_EMPTY = 1,
    READ_SIZE_ERROR = 2,
    READ_EXCEPTION = 3
} cache_read_status;

cache_read_status readCredCache(void);
void storeCredCache(void);
void notifyChangedCaches(void);

bool isAcquireCred(HeimCredRef cred);
bool hasRenewTillInAttributes(CFDictionaryRef attributes);

void _HeimCredRegisterGeneric(void);

void _HeimCredRegisterConfiguration(void);

struct HeimSession * HeimCredCopySession(int sessionID);
void RemoveSession(au_asid_t asid);

void peer_final(void *ptr);

extern NSString *archivePath;

void do_Delete(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_SetAttrs(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_Auth(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_Fetch(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_Query(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_GetDefault(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_Move(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_Status(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_DeleteAll(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_CreateCred(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_RetainCache(struct peer *peer, xpc_object_t request, xpc_object_t reply);
void do_ReleaseCache(struct peer *peer, xpc_object_t request, xpc_object_t reply);

CFTypeRef KerberosStatusCallback(HeimCredRef cred) CF_RETURNS_RETAINED;
CFTypeRef KerberosAcquireCredStatusCallback(HeimCredRef cred) CF_RETURNS_RETAINED;
CFTypeRef ConfigurationStatusCallback(HeimCredRef cred) CF_RETURNS_RETAINED;
#endif /* gsscred_h */
