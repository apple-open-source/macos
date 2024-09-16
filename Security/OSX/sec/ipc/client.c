/*
 * Copyright (c) 2007-2009,2012-2023 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <TargetConditionals.h>

// client.c is from the iOS world and must compile with an iOS view of the headers
#ifndef SEC_IOS_ON_OSX
#define SEC_IOS_ON_OSX 1
#endif // SEC_IOS_ON_OSX

#if TARGET_OS_OSX
#ifndef SECITEM_SHIM_OSX
#define SECITEM_SHIM_OSX 1
#endif // SECITEM_SHIM_OSX
#endif // TARGET_OS_OSX

#include <stdbool.h>
#include <sys/queue.h>
#include <syslog.h>
#include <vproc_priv.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <os/feature_private.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecBasePriv.h>
#include <Security/SecInternal.h>
#include <Security/SecuritydXPC.h>
#include <Security/SecTask.h>
#include <Security/SecItemPriv.h>

#include <utilities/debugging.h>
#include <utilities/SecCFError.h>
#include <utilities/SecXPCError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecDb.h> // TODO Fixme this gets us SecError().
#include <utilities/SecFileLocations.h>
#include <ipc/securityd_client.h>
#include "featureflags/featureflags.h"

#include "server_security_helpers.h"

struct securityd *gSecurityd;
struct trustd *gTrustd;

//
// MARK: XPC IPC.
//

/* Hardcoded Access Groups for the server itself */
static CFArrayRef SecServerCopyAccessGroups(void) {
    CFArrayRef accessGroups = CFArrayCreateForCFTypes(kCFAllocatorDefault,
#if NO_SERVER
                                                      CFSTR("test"),
                                                      CFSTR("apple"),
                                                      CFSTR("lockdown-identities"),
                                                      CFSTR("123456.test.group"),
                                                      CFSTR("123456.test.group2"),
                                                      CFSTR("com.apple.cfnetwork"),
                                                      CFSTR("com.apple.bluetooth"),
#endif
                                                      CFSTR("sync"),
                                                      CFSTR("com.apple.security.sos"),
                                                      CFSTR("com.apple.security.ckks"),
                                                      CFSTR("com.apple.security.octagon"),
                                                      CFSTR("com.apple.security.egoIdentities"),
                                                      CFSTR("com.apple.security.sos-usercredential"),
                                                      CFSTR("com.apple.sbd"),
                                                      CFSTR("com.apple.lakitu"),
                                                      CFSTR("com.apple.security.securityd"),
                                                      CFSTR("com.apple.ProtectedCloudStorage"),
                                                      NULL);
    if (os_feature_enabled(CryptoTokenKit, UseTokens)) {
        CFMutableArrayRef mutableGroups = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, accessGroups);
        CFArrayAppendValue(mutableGroups, kSecAttrAccessGroupToken);
        CFAssignRetained(accessGroups, mutableGroups);
    }

    return accessGroups;
}

static __thread SecurityClient threadLocalClient;
static SecurityClient gClient;

void
SecSecurityFixUpClientWithPersona(SecurityClient* src, SecurityClient* dest)
{
    memcpy(dest, src, sizeof(struct SecurityClient));
    
    dest->musr = NULL;
    
    if (gSecurityd && gSecurityd->sec_fill_security_client_muser) {
        gSecurityd->sec_fill_security_client_muser(dest);
    }
}

#if KEYCHAIN_SUPPORTS_SINGLE_DATABASE_MULTIUSER
// Only for testing.
void
SecSecuritySetMusrMode(bool inEduMode, uid_t uid, int activeUser)
{
    (void)SecSecurityClientGet(); // Initializes `gClient` and `threadLocalClient` as a side effect.
    gClient.inEduMode = inEduMode;
    gClient.uid = uid;
    threadLocalClient.inEduMode = inEduMode;
    threadLocalClient.uid = uid;
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
    gClient.activeUser = activeUser;
    threadLocalClient.activeUser = activeUser;
#endif
}

void
SecSecuritySetPersonaMusr(CFStringRef uuid)
{
    if (gClient.inEduMode) {
        abort();
    }
    SecurityClient* client = SecSecurityClientGet();
    CFReleaseNull(client->musr);
    
    client->isMusrOverridden = uuid ? true : false;
    
    if (uuid) {
        CFUUIDRef u = CFUUIDCreateFromString(NULL, uuid);
        if (u == NULL) {
            abort();
        }
        CFUUIDBytes ubytes = CFUUIDGetUUIDBytes(u);
        CFReleaseNull(u);
        client->musr = CFDataCreate(NULL, (const void *)&ubytes, sizeof(ubytes));
    }
}
#endif // KEYCHAIN_SUPPORTS_SINGLE_DATABASE_MULTIUSER

SecurityClient *
SecSecurityClientGet(void)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        gClient.task = NULL;
        gClient.accessGroups = SecServerCopyAccessGroups();
#if KEYCHAIN_SUPPORTS_SYSTEM_KEYCHAIN
        gClient.allowSystemKeychain = true;
#endif
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER
        gClient.allowSyncBubbleKeychain = true;
        gClient.activeUser = 501;
#endif
        gClient.isNetworkExtension = false;
#if KEYCHAIN_SUPPORTS_SINGLE_DATABASE_MULTIUSER
        gClient.inEduMode = false;
        gClient.musr = NULL;
        gClient.isMusrOverridden = false;
#endif
        gClient.applicationIdentifier = NULL;
        gClient.isAppClip = false;
        gClient.allowKeychainSharing = false;
    });

    static __thread dispatch_once_t onceTokenThreadLocalClient;
    dispatch_once(&onceTokenThreadLocalClient, ^{
        secnotice("thread-local-client-debug", "SecSecurityClientGet new thread!");
        memcpy(&threadLocalClient, &gClient, sizeof(struct SecurityClient));
        // `memcpy`-ing CF objects doesn't adjust their reference counts, so do
        // that manually to avoid overreleases in the testing functions.
        CFRetainSafe(threadLocalClient.task);
        CFRetainSafe(threadLocalClient.accessGroups);
#if KEYCHAIN_SUPPORTS_SINGLE_DATABASE_MULTIUSER
        CFRetainSafe(threadLocalClient.musr);
#endif
        CFRetainSafe(threadLocalClient.applicationIdentifier);
    });
    
#if KEYCHAIN_SUPPORTS_SINGLE_DATABASE_MULTIUSER
    /* muser needs to be filled out in the context of the thread instead of a global value */
    if (gSecurityd && gSecurityd->sec_fill_security_client_muser) {
        gSecurityd->sec_fill_security_client_muser(&threadLocalClient);
    }

#endif
    return &threadLocalClient;

}

CFArrayRef SecAccessGroupsGetCurrent(void) {
    SecurityClient *client = SecSecurityClientGet();
    assert(client && client->accessGroups);
    return client->accessGroups;
}

// Only for testing.
void SecAccessGroupsSetCurrent(CFArrayRef accessGroups) {
    secnotice("thread-local-client-debug", "SecAccessGroupsSetCurrent begin! Setting access groups: %@", accessGroups);
    // Not thread safe at all, but OK because it is meant to be used only by tests.
    (void)SecSecurityClientGet(); // Initializes `gClient` and `threadLocalClient` as a side effect.
    secnotice("thread-local-client-debug", "SecAccessGroupsSetCurrent releasing gClient access groups");
    CFReleaseNull(gClient.accessGroups);
    secnotice("thread-local-client-debug", "SecAccessGroupsSetCurrent releasing threadLocalClient access groups");
    CFReleaseNull(threadLocalClient.accessGroups);
    gClient.accessGroups = CFRetainSafe(accessGroups);
    threadLocalClient.accessGroups = CFRetainSafe(accessGroups);
    secnotice("thread-local-client-debug", "SecAccessGroupsSetCurrent end!");
}

// Testing
void SecSecurityClientRegularToAppClip(void) {
    (void)SecSecurityClientGet(); // Initializes `gClient` and `threadLocalClient` as a side effect.
    gClient.isAppClip = true;
    threadLocalClient.isAppClip = true;
}

// Testing
void SecSecurityClientAppClipToRegular(void) {
    (void)SecSecurityClientGet(); // Initializes `gClient` and `threadLocalClient` as a side effect.
    gClient.isAppClip = false;
    threadLocalClient.isAppClip = false;
}

// Testing
void SecSecurityClientSetApplicationIdentifier(CFStringRef identifier) {
    (void)SecSecurityClientGet(); // Initializes `gClient` and `threadLocalClient` as a side effect.
    CFReleaseNull(gClient.applicationIdentifier);
    CFReleaseNull(threadLocalClient.applicationIdentifier);
    gClient.applicationIdentifier = CFRetainSafe(identifier);
    threadLocalClient.applicationIdentifier = CFRetainSafe(identifier);
}

// Testing
void SecSecurityClientSetKeychainSharingState(SecSecurityClientKeychainSharingState state) {
    (void)SecSecurityClientGet(); // Initializes `gClient` and `threadLocalClient` as a side effect.
    switch (state) {
        case SecSecurityClientKeychainSharingStateDisabled:
            gClient.allowKeychainSharing = false;
            threadLocalClient.allowKeychainSharing = false;
            break;
        case SecSecurityClientKeychainSharingStateEnabled:
            gClient.allowKeychainSharing = true;
            threadLocalClient.allowKeychainSharing = true;
            break;
    }
}

#if !TARGET_OS_IPHONE
static bool securityd_in_system_context(void) {
    static bool runningInSystemContext;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        runningInSystemContext = (getuid() == 0);
        if (!runningInSystemContext) {
            char *manager;
            if (vproc_swap_string(NULL, VPROC_GSK_MGR_NAME, NULL, &manager) == NULL) {
                runningInSystemContext = (!strcmp(manager, VPROCMGR_SESSION_SYSTEM) ||
                                          !strcmp(manager, VPROCMGR_SESSION_LOGINWINDOW));
                free(manager);
            }
        }
    });
    return runningInSystemContext;
}
#endif

static const char *securityd_service_name(void) {
	return kSecuritydXPCServiceName;
}

#define SECURITY_TARGET_UID_UNSET   ((uid_t)-1)
static uid_t securityd_target_uid = SECURITY_TARGET_UID_UNSET;

void
_SecSetSecuritydTargetUID(uid_t uid)
{
    securityd_target_uid = uid;
}


static xpc_connection_t securityd_create_connection(const char *name, uid_t target_uid, uint64_t flags) {
    const char *serviceName = name;
    if (!serviceName) {
        serviceName = securityd_service_name();
    }
    xpc_connection_t connection;
    connection = xpc_connection_create_mach_service(serviceName, NULL, flags);
    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
        const char *description = xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION);
        secinfo("xpc", "got event: %s", description);
    });
    if (target_uid != SECURITY_TARGET_UID_UNSET) {
#if TARGET_OS_OSX
        xpc_connection_set_target_uid(connection, target_uid);
#else
        xpc_connection_set_target_user_session_uid(connection, target_uid);
#endif
    }
    xpc_connection_resume(connection);
    return connection;
}

static bool is_trust_operation(enum SecXPCOperation op) {
    switch (op) {
        case sec_trust_store_contains_id:
        case sec_trust_store_set_trust_settings_id:
        case sec_trust_store_remove_certificate_id:
        case sec_trust_evaluate_id:
        case sec_trust_store_copy_all_id:
        case sec_trust_store_copy_usage_constraints_id:
        case sec_ocsp_cache_flush_id:
        case sec_ota_pki_trust_store_asset_version_id:
        case sec_ota_pki_trust_store_content_digest_id:
        case sec_ota_pki_trust_store_version_id:
        case sec_ota_pki_asset_version_id:
        case kSecXPCOpOTAPKIGetNewAsset:
        case kSecXPCOpOTASecExperimentGetNewAsset:
        case kSecXPCOpOTASecExperimentGetAsset:
        case kSecXPCOpOTAPKICopyTrustedCTLogs:
        case kSecXPCOpOTAPKICopyCTLogForKeyID:
        case kSecXPCOpNetworkingAnalyticsReport:
        case kSecXPCOpSetCTExceptions:
        case kSecXPCOpCopyCTExceptions:
        case sec_trust_get_exception_reset_count_id:
        case sec_trust_increment_exception_reset_count_id:
        case kSecXPCOpSetCARevocationAdditions:
        case kSecXPCOpCopyCARevocationAdditions:
        case kSecXPCOpValidUpdate:
        case kSecXPCOpSetTransparentConnectionPins:
        case kSecXPCOpCopyTransparentConnectionPins:
        case sec_trust_settings_set_data_id:
        case sec_trust_settings_copy_data_id:
        case sec_truststore_remove_all_id:
        case sec_trust_reset_settings_id:
        case sec_trust_store_migrate_plist_id:
            return true;
        default:
            break;
    }
    return false;
}

// trust operations which need to be managed by the system trustd instance
static bool is_system_trust_operation(enum SecXPCOperation op) {
    switch (op) {
        case sec_trust_store_contains_id:
        case sec_trust_store_set_trust_settings_id:
        case sec_trust_store_remove_certificate_id:
        case sec_trust_store_copy_all_id:
        case sec_trust_store_copy_usage_constraints_id:
        case sec_truststore_remove_all_id:
        case sec_trust_store_migrate_plist_id:
            return true;
        default:
            break;
    }
    return false;
}

// sSC* manage a pool of xpc_connection_t objects to securityd
static dispatch_queue_t sSecuritydConnectionsQueue;
static CFMutableArrayRef sSecuritydConnectionsPool;
static unsigned sSecuritydConnectionsCount;     // connections in circulation
#define MAX_SECURITYD_CONNECTIONS 5

static xpc_connection_t _securityd_connection(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sSecuritydConnectionsCount = 0;
        sSecuritydConnectionsQueue = dispatch_queue_create("com.apple.security.securityd_connections", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        sSecuritydConnectionsPool = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
    });

    __block xpc_connection_t ret = NULL;
    dispatch_sync(sSecuritydConnectionsQueue, ^{
        if (CFArrayGetCount(sSecuritydConnectionsPool) > 0) {
            ret = (xpc_connection_t)CFArrayGetValueAtIndex(sSecuritydConnectionsPool, 0);
            CFArrayRemoveValueAtIndex(sSecuritydConnectionsPool, 0);
        } else if (sSecuritydConnectionsCount < MAX_SECURITYD_CONNECTIONS) {
            ret = securityd_create_connection(kSecuritydXPCServiceName, securityd_target_uid, 0);
            ++sSecuritydConnectionsCount;
            secinfo("xpc", "Adding securityd connection to pool, total now %d", sSecuritydConnectionsCount);
        }   // No connection available and no room in the pool for a new one, touch luck!
    });
    return ret;
}

// whether we need to target the foreground user session, see rdar://83785274
static bool is_foreground_user_operation(enum SecXPCOperation op) {
    switch (op) {
        case kSecXPCOpTransmogrifyToSyncBubble:
        case kSecXPCOpTransmogrifyToSystemKeychain:
        case kSecXPCOpDeleteUserView:
#if TARGET_OS_IPHONE
        case sec_trust_evaluate_id:
#endif
            return true;
        default:
            return false;
    }
}

// Don't use the connection pool, because the foreground uid may change, and we want the current one each time.
// This only affects edu mode SPIs.
static xpc_connection_t _securityd_connection_to_foreground_user_session(void) {
#if TARGET_OS_OSX
    // This shoulnd't be called on macos, but in case it is, fall back to default behavior,
    // but w/o using the pool.
    return securityd_create_connection(kSecuritydXPCServiceName, securityd_target_uid, 0);
#else
    if (!xpc_user_sessions_enabled()) {
        return securityd_create_connection(kSecuritydXPCServiceName, securityd_target_uid, 0);
    }
    errno_t error = 0;
    uid_t foreground_uid = xpc_user_sessions_get_foreground_uid(&error);
    if (error != 0) {
        secerror("xpc: could not get foreground uid %d", error);
        return securityd_create_connection(kSecuritydXPCServiceName, securityd_target_uid, 0);
    }

    if (securityd_target_uid != SECURITY_TARGET_UID_UNSET && foreground_uid != securityd_target_uid) {
        secerror("xpc: uid targets not equal. using foreground: %d not global: %d", foreground_uid, securityd_target_uid);
    } else {
        if(foreground_uid != 501) {
            secnotice("xpc", "user sessions enabled, targeting %d", foreground_uid);
        } else {
            secinfo("xpc", "user sessions enabled, targeting %d", foreground_uid);
        }
    }
    return securityd_create_connection(kSecuritydXPCServiceName, foreground_uid, 0);
#endif
}

// Don't use the connection pool for now.
// This only affects system keychain calls.
static xpc_connection_t _securityd_connection_to_system_keychain(void) {
#if KEYCHAIN_SUPPORTS_SPLIT_SYSTEM_KEYCHAIN
    secnotice("xpc", "using system keychain XPC");
    return securityd_create_connection(kSecuritydSystemXPCServiceName, securityd_target_uid, 0);
#else
    // Shouldn't be called when system keychain is not available, but just in case it is,
    // fallback to default behavior, but w/o using the pool.
    return securityd_create_connection(kSecuritydXPCServiceName, securityd_target_uid, 0);
#endif // KEYCHAIN_SUPPORTS_SPLIT_SYSTEM_KEYCHAIN
}

typedef enum _security_fw_backend {
    security_fw_backend_SECD,
    security_fw_backend_SECD_FOREGROUND,
    security_fw_backend_SECD_SYSTEM,
    security_fw_backend_TRUSTD,
    security_fw_backend_TRUSTD_FOREGROUND,
    security_fw_backend_TRUSTD_SYSTEM,
    security_fw_backend_UNKNOWN,
} security_fw_backend;

static xpc_connection_t securityd_connection(bool isForegroundUserOp, bool useSystemKeychain, security_fw_backend* backend) {
    unsigned tries = 0;
    xpc_connection_t ret = NULL;
    do {
        if (isForegroundUserOp) {
            *backend = security_fw_backend_SECD_FOREGROUND;
            ret = _securityd_connection_to_foreground_user_session();
        } else if (useSystemKeychain) {
            *backend = security_fw_backend_SECD_SYSTEM;
            ret = _securityd_connection_to_system_keychain();
        } else {
            *backend = security_fw_backend_SECD;
            ret = _securityd_connection();
        }
        if (!ret) {
            usleep(2500);
        }
        ++tries;
        if (tries % 100 == 0) {  // 1/4 second is a long time to wait, but maybe you're overdoing it also?
            secwarning("xpc: have been trying %d times to get a securityd connection", tries);
        }
    } while (!ret);
    return ret;
}

static void return_securityd_connection_to_pool(enum SecXPCOperation op, bool useSystemKeychain, xpc_connection_t conn) {
    if (is_trust_operation(op)) {
        if (is_foreground_user_operation(op)) {
            // clean up foreground connections, preserve the global one
            secinfo("xpc", "cleaning up unpooled xpc conn to trustd %p", conn);
            xpc_connection_cancel(conn);
            xpc_release(conn);
        }
        return;
    }
    if (!is_foreground_user_operation(op) && !useSystemKeychain) {
        dispatch_sync(sSecuritydConnectionsQueue, ^{
            if (CFArrayGetCount(sSecuritydConnectionsPool) >= MAX_SECURITYD_CONNECTIONS) {
                xpc_connection_cancel(conn);
                secerror("xpc: Unable to re-enqueue securityd connection because already at limit");
                if (sSecuritydConnectionsCount < MAX_SECURITYD_CONNECTIONS) {
                    secerror("xpc: connection pool full but tracker does not agree (%d vs %ld)", sSecuritydConnectionsCount, CFArrayGetCount(sSecuritydConnectionsPool));
                }
                abort();    // We've miscalculated?
            }
            CFArrayAppendValue(sSecuritydConnectionsPool, conn);
        });
    } else {
        // clean up foreground/system-keychain connection
        secinfo("xpc", "cleaning up unpooled xpc conn %p", conn);
        xpc_connection_cancel(conn);
        xpc_release(conn);
    }
}

static xpc_connection_t sTrustdConnection;
static xpc_connection_t trustd_connection(void) {
#if TARGET_OS_OSX
	static dispatch_once_t once;
	dispatch_once(&once, ^{
        bool sysCtx = securityd_in_system_context();
        uint64_t flags = (sysCtx) ? XPC_CONNECTION_MACH_SERVICE_PRIVILEGED : 0;
        const char *serviceName = (sysCtx) ? kTrustdXPCServiceName : kTrustdAgentXPCServiceName;
		sTrustdConnection = securityd_create_connection(serviceName, SECURITY_TARGET_UID_UNSET, flags);
	});
	return sTrustdConnection;
#else
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        sTrustdConnection = securityd_create_connection(kTrustdXPCServiceName, SECURITY_TARGET_UID_UNSET, 0);
    });
    return sTrustdConnection;
#endif
}

static xpc_connection_t sTrustdSystemInstanceConnection;
static xpc_connection_t trustd_system_connection(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        uint64_t flags = 0;
#if TARGET_OS_OSX
        flags = XPC_CONNECTION_MACH_SERVICE_PRIVILEGED;
#endif
        sTrustdSystemInstanceConnection = securityd_create_connection(kTrustdXPCServiceName, SECURITY_TARGET_UID_UNSET, flags);
    });
    return sTrustdSystemInstanceConnection;
}

static xpc_connection_t trustd_connection_to_foreground_user_session(void) {
	// This is normally kTrustdXPCServiceName but can be changed with SecServerSetTrustdMachServiceName()
	const char *serviceName = xpc_connection_get_name(trustd_connection());
#if TARGET_OS_IPHONE
    if (!xpc_user_sessions_enabled()) {
        return securityd_create_connection(serviceName, SECURITY_TARGET_UID_UNSET, 0);
    }
    errno_t error = 0;
    uid_t foreground_uid = xpc_user_sessions_get_foreground_uid(&error);
    if (error != 0) {
        secerror("xpc: could not get foreground uid %d", error);
        return securityd_create_connection(serviceName, SECURITY_TARGET_UID_UNSET, 0);
    }

    if(foreground_uid != 501) {
        secnotice("xpc", "user sessions enabled, targeting %d", foreground_uid);
    } else {
        secinfo("xpc", "user sessions enabled, targeting %d", foreground_uid);
    }
    return securityd_create_connection(serviceName, foreground_uid, 0);
#else
    secerror("xpc: unexpected foreground user operation on macOS, using default behavior");
    return securityd_create_connection(serviceName, SECURITY_TARGET_UID_UNSET, 0);
#endif
}

static xpc_connection_t securityd_connection_for_operation(enum SecXPCOperation op, bool useSystemKeychain, security_fw_backend* backend) {
    bool isTrustOp = is_trust_operation(op);
    bool isSysTrustOp = is_system_trust_operation(op);
    #if SECTRUST_VERBOSE_DEBUG
    {
        bool sysCtx = securityd_in_system_context();
        const char *serviceName = (sysCtx) ? kTrustdXPCServiceName : kTrustdAgentXPCServiceName;
        syslog(LOG_ERR, "Will connect to: %s (op=%d)",
               (isTrustOp && !isSysTrustOp) ? serviceName : kSecuritydXPCServiceName, (int)op);
    }
    #endif
    if (isTrustOp) {
        if (isSysTrustOp) {
            *backend = security_fw_backend_TRUSTD_SYSTEM;
            return trustd_system_connection();
        } else if (is_foreground_user_operation(op)) {
            *backend = security_fw_backend_TRUSTD_FOREGROUND;
            return trustd_connection_to_foreground_user_session();
        } else {
            *backend = security_fw_backend_TRUSTD;
            return trustd_connection();
        }
    }

    bool isForegroundUserOp = is_foreground_user_operation(op);
    return securityd_connection(isForegroundUserOp, useSystemKeychain, backend);
}

// NOTE: This is not thread safe, but this SPI is for testing only.
void SecServerSetTrustdMachServiceName(const char *name) {
    // Make sure sSecXPCServer.queue exists.
    trustd_connection();

    xpc_connection_t oldConnection = sTrustdConnection;
    sTrustdConnection = securityd_create_connection(name, SECURITY_TARGET_UID_UNSET, 0);
    if (oldConnection)
        xpc_release(oldConnection);
}


#define SECURITYD_MAX_XPC_TRIES 4
// Per <rdar://problem/17829836> N61/12A342: Audio Playback... for why this needs to be at least 3, so we made it 4.

static bool
_securityd_process_message_reply(xpc_object_t *reply,
								 CFErrorRef *error,
								 xpc_connection_t connection,
								 uint64_t operation)
{
	if (xpc_get_type(*reply) != XPC_TYPE_ERROR) {
		return true;
	}
	CFIndex code =  0;
	if (*reply == XPC_ERROR_CONNECTION_INTERRUPTED || *reply == XPC_ERROR_CONNECTION_INVALID) {
		code = kSecXPCErrorConnectionFailed;
		seccritical("Failed to talk to %s after %d attempts.",
					(is_trust_operation((enum SecXPCOperation)operation)) ? "trustd" :
#if TARGET_OS_IPHONE
					"securityd",
#else
					"secd",
#endif
					SECURITYD_MAX_XPC_TRIES);
	} else if (*reply == XPC_ERROR_TERMINATION_IMMINENT) {
		code = kSecXPCErrorUnknown;
	} else {
		code = kSecXPCErrorUnknown;
	}

	char *conn_desc = xpc_copy_description(connection);
	const char *description = xpc_dictionary_get_string(*reply, XPC_ERROR_KEY_DESCRIPTION);
	char *invalidation_reason = xpc_connection_copy_invalidation_reason(connection);
	if (invalidation_reason != NULL) {
		SecCFCreateErrorWithFormat(code, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("%s: %s - %s"), conn_desc, description, invalidation_reason);
	} else {
		SecCFCreateErrorWithFormat(code, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("%s: %s"), conn_desc, description);
	}
	free(conn_desc);
	free(invalidation_reason);
	xpc_release(*reply);
	*reply = NULL;
	return false;
}

static bool getBoolValue(CFDictionaryRef dict, CFStringRef key)
{
    CFTypeRef val = CFDictionaryGetValue(dict, key);
    if (val == NULL) {
        return false;
    }
    if(CFGetTypeID(val) == CFBooleanGetTypeID()) {
        return CFBooleanGetValue((CFBooleanRef)val) ? true : false;
    }
    if(CFGetTypeID(val) == CFNumberGetTypeID()) {
        char cval = 0;
        CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, &cval);
        return (cval == 0) ? false : true;
    }
    secnotice("xpc", "unexpected CF type (%lu) for key %s", CFGetTypeID(val), CFStringGetCStringPtr(key, kCFStringEncodingUTF8) ?: "<null>");
    return false;
}

static bool securityd_message_is_for_system_keychain(xpc_object_t message) {
#if !KEYCHAIN_SUPPORTS_SPLIT_SYSTEM_KEYCHAIN
    return false;
#else
    // Even in edumode, if the FF is not set, don't target the new XPC service.
    // That is, to target the new XPC service in edumode, you must also set the FF,
    // because otherwise, the new service will not be running.
    if (!_SecSystemKeychainAlwaysIsEnabled()) {
        secdebug("xpc", "feature flag not set for system keychain");
        return false;
    }

#if DEBUG
    char* envForce = getenv("FORCE_SYSTEM_KEYCHAIN_XPC");
    if (envForce != NULL && strcmp(envForce, "1") == 0) {
        secnotice("xpc", "forcibly using system keychain");
        return true;
    }
#endif // DEBUG

    CFErrorRef error = NULL;
    CFDictionaryRef query = SecXPCDictionaryCopyDictionaryWithoutZeroingDataInMessage(message, kSecXPCKeyQuery, &error);
    if (!query) {
        secinfo("xpc", "no query dict to determine whether for system keychain: %@", error);
        CFReleaseNull(error);
        return false;
    }

    CFReleaseNull(error);

    bool result = false;
    bool flagAlways = getBoolValue(query, kSecUseSystemKeychainAlways);
    bool inEduMode = SecIsEduMode();
    bool flagOld = getBoolValue(query, kSecUseSystemKeychain);

    secinfo("xpc", "flagAlways:%{bool}d inEduMode:%{bool}d flagOld:%{bool}d", flagAlways, inEduMode, flagOld);

    if (flagAlways) {
        secnotice("xpc", "kSecUseSystemKeychainAlways… present, using system keychain");
        result = true;
    } else if (inEduMode && flagOld) {
        secnotice("xpc", "kSecUseSystemKeychain present, using system keychain");
        result = true;
    } else {
        secinfo("xpc", "not using system keychain");
    }

    CFReleaseNull(query);

    return result;
#endif // KEYCHAIN_SUPPORTS_SPLIT_SYSTEM_KEYCHAIN
}

XPC_RETURNS_RETAINED static xpc_object_t security_fw_send_message_with_reply_sync_inner(xpc_object_t message, xpc_connection_t connection, uint64_t operation, bool useSystemKeychain, CFErrorRef* error)
{
    xpc_object_t reply = NULL;

    const int max_tries = SECURITYD_MAX_XPC_TRIES;

    unsigned int tries_left = max_tries;
    do {
        if (reply) xpc_release(reply);
        reply = xpc_connection_send_message_with_reply_sync(connection, message);
    } while (reply == XPC_ERROR_CONNECTION_INTERRUPTED && --tries_left > 0);

	_securityd_process_message_reply(&reply, error, connection, operation);

    return_securityd_connection_to_pool((enum SecXPCOperation)operation, useSystemKeychain, connection);

    return reply;
}

__attribute__((noinline)) XPC_RETURNS_RETAINED static xpc_object_t security_fw_CALLING_SECD_OR_SECURITYD(xpc_object_t message, xpc_connection_t connection, uint64_t operation, bool useSystemKeychain, CFErrorRef* error)
{
    return security_fw_send_message_with_reply_sync_inner(message, connection, operation, useSystemKeychain, error);
}

__attribute__((noinline)) XPC_RETURNS_RETAINED static xpc_object_t security_fw_CALLING_SECURITYD_FOREGROUND(xpc_object_t message, xpc_connection_t connection, uint64_t operation, bool useSystemKeychain, CFErrorRef* error)
{
    return security_fw_send_message_with_reply_sync_inner(message, connection, operation, useSystemKeychain, error);
}

__attribute__((noinline)) XPC_RETURNS_RETAINED static xpc_object_t security_fw_CALLING_SECURITYD_SYSTEM(xpc_object_t message, xpc_connection_t connection, uint64_t operation, bool useSystemKeychain, CFErrorRef* error)
{
    return security_fw_send_message_with_reply_sync_inner(message, connection, operation, useSystemKeychain, error);
}

__attribute__((noinline)) static xpc_object_t security_fw_CALLING_TRUSTD(xpc_object_t message, xpc_connection_t connection, uint64_t operation, bool useSystemKeychain, CFErrorRef* error)
{
    return security_fw_send_message_with_reply_sync_inner(message, connection, operation, useSystemKeychain, error);
}

__attribute__((noinline)) static xpc_object_t security_fw_CALLING_TRUSTD_FOREGROUND(xpc_object_t message, xpc_connection_t connection, uint64_t operation, bool useSystemKeychain, CFErrorRef* error)
{
    return security_fw_send_message_with_reply_sync_inner(message, connection, operation, useSystemKeychain, error);
}

__attribute__((noinline)) static xpc_object_t security_fw_CALLING_TRUSTD_SYSTEM(xpc_object_t message, xpc_connection_t connection, uint64_t operation, bool useSystemKeychain, CFErrorRef* error)
{
    return security_fw_send_message_with_reply_sync_inner(message, connection, operation, useSystemKeychain, error);
}

__attribute__((noinline)) static xpc_object_t security_fw_CALLING_UNKNOWN_BACKEND_THIS_IS_A_BUG(xpc_object_t message, xpc_connection_t connection, uint64_t operation, bool useSystemKeychain, CFErrorRef* error)
{
    return security_fw_send_message_with_reply_sync_inner(message, connection, operation, useSystemKeychain, error);
}

XPC_RETURNS_RETAINED xpc_object_t securityd_message_with_reply_sync(xpc_object_t message, CFErrorRef *error)
{
    uint64_t operation = xpc_dictionary_get_uint64(message, kSecXPCKeyOperation);
    bool useSystemKeychain = securityd_message_is_for_system_keychain(message);
    security_fw_backend backend = security_fw_backend_UNKNOWN;
    xpc_connection_t connection = securityd_connection_for_operation((enum SecXPCOperation)operation, useSystemKeychain, &backend);

    switch (backend) {
        case security_fw_backend_SECD:
            return security_fw_CALLING_SECD_OR_SECURITYD(message, connection, operation, useSystemKeychain, error);
        case security_fw_backend_SECD_FOREGROUND:
            return security_fw_CALLING_SECURITYD_FOREGROUND(message, connection, operation, useSystemKeychain, error);
        case security_fw_backend_SECD_SYSTEM:
            return security_fw_CALLING_SECURITYD_SYSTEM(message, connection, operation, useSystemKeychain, error);
        case security_fw_backend_TRUSTD:
            return security_fw_CALLING_TRUSTD(message, connection, operation, useSystemKeychain, error);
        case security_fw_backend_TRUSTD_FOREGROUND:
            return security_fw_CALLING_TRUSTD_FOREGROUND(message, connection, operation, useSystemKeychain, error);
        case security_fw_backend_TRUSTD_SYSTEM:
            return security_fw_CALLING_TRUSTD_SYSTEM(message, connection, operation, useSystemKeychain, error);
        case security_fw_backend_UNKNOWN:
            [[fallthrough]];
        default:
            return security_fw_CALLING_UNKNOWN_BACKEND_THIS_IS_A_BUG(message, connection, operation, useSystemKeychain, error);
    }
}

static void
_securityd_message_with_reply_async_inner(xpc_object_t message,
										  dispatch_queue_t replyq,
										  securityd_handler_t handler,
										  uint32_t tries_left)
{
	uint64_t operation = xpc_dictionary_get_uint64(message, kSecXPCKeyOperation);
    bool useSystemKeychain = securityd_message_is_for_system_keychain(message);
    security_fw_backend backend = security_fw_backend_UNKNOWN;// unused in async case
    xpc_connection_t connection = securityd_connection_for_operation((enum SecXPCOperation)operation, useSystemKeychain, &backend);

	xpc_retain(message);
	dispatch_retain(replyq);
	securityd_handler_t handlerCopy = Block_copy(handler);
	xpc_connection_send_message_with_reply(connection, message, replyq, ^(xpc_object_t _Nonnull reply) {
		if (reply == XPC_ERROR_CONNECTION_INTERRUPTED && tries_left > 0) {
			_securityd_message_with_reply_async_inner(message, replyq, handlerCopy, tries_left - 1);
		} else {
			CFErrorRef error = NULL;
			_securityd_process_message_reply(&reply, &error, connection, operation);
			handlerCopy(reply, error);
			CFReleaseNull(error);
		}
		xpc_release(message);
		dispatch_release(replyq);
		Block_release(handlerCopy);

		return_securityd_connection_to_pool((enum SecXPCOperation)operation, useSystemKeychain, connection);
	});
}

void
securityd_message_with_reply_async(xpc_object_t message,
								   dispatch_queue_t replyq,
								   securityd_handler_t handler)
{
	_securityd_message_with_reply_async_inner(message, replyq, handler, SECURITYD_MAX_XPC_TRIES);
}

XPC_RETURNS_RETAINED
xpc_object_t
securityd_create_message(enum SecXPCOperation op, CFErrorRef* error)
{
    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    if (message) {
        xpc_dictionary_set_uint64(message, kSecXPCKeyOperation, op);
    } else {
        SecCFCreateError(kSecXPCErrorConnectionFailed, sSecXPCErrorDomain,
                         CFSTR("xpc_dictionary_create returned NULL"), NULL, error);
    }
    return message;
}

// Return true if there is no error in message, return false and set *error if there is.
bool securityd_message_no_error(xpc_object_t message, CFErrorRef *error) {
    xpc_object_t xpc_error = NULL;
    if (message == NULL)
        return false;

    xpc_error = xpc_dictionary_get_value(message, kSecXPCKeyError);
    if (xpc_error == NULL)
        return true;

    CFErrorRef localError = SecCreateCFErrorWithXPCObject(xpc_error);

#if TARGET_OS_IPHONE
    secdebug("xpc", "Talking to securityd failed with error: %@", localError);
#else
#if !defined(NDEBUG)
    uint64_t operation = xpc_dictionary_get_uint64(message, kSecXPCKeyOperation);
#endif
    secdebug("xpc", "Talking to %s failed with error: %@",
             (is_trust_operation((enum SecXPCOperation)operation)) ? "trustd" : "secd", localError);
#endif

    if (error) {
        *error = localError;
    } else {
        CFReleaseSafe(localError);
    }
    return false;
}

bool securityd_send_sync_and_do(enum SecXPCOperation op, CFErrorRef *error,
                                bool (^add_to_message)(xpc_object_t message, CFErrorRef* error),
                                bool (^handle_response)(xpc_object_t _Nonnull response, CFErrorRef* error)) {
    xpc_object_t message = securityd_create_message(op, error);
    bool ok = false;
    if (message) {
        if (!add_to_message || add_to_message(message, error)) {
            xpc_object_t response = securityd_message_with_reply_sync(message, error);
            if (response) {
                if (securityd_message_no_error(response, error)) {
                    ok = (!handle_response || handle_response(response, error));
                }
                xpc_release(response);
            }
        }
        xpc_release(message);
    }

    return ok;
}

void securityd_send_async_and_do(enum SecXPCOperation op, dispatch_queue_t replyq,
								 bool (^add_to_message)(xpc_object_t message, CFErrorRef* error),
								 securityd_handler_t handler) {
	CFErrorRef error = NULL;
	xpc_object_t message = securityd_create_message(op, &error);
	if (message == NULL) {
		handler(NULL, error);
		CFReleaseNull(error);
		return;
	}

	if (add_to_message != NULL) {
		if (!add_to_message(message, &error)) {
			handler(NULL, error);
			xpc_release(message);
			CFReleaseNull(error);
			return;
		}
	}

	securityd_message_with_reply_async(message, replyq, ^(xpc_object_t reply, CFErrorRef error2) {
		if (error2 != NULL) {
			handler(NULL, error2);
			return;
		}
		CFErrorRef error3 = NULL;
		if (!securityd_message_no_error(reply, &error3)) {
			handler(NULL, error3);
			CFReleaseNull(error3);
			return;
		}
		handler(reply, NULL);
	});
	xpc_release(message);
}


CFDictionaryRef
_SecSecuritydCopyWhoAmI(CFErrorRef *error)
{
    CFDictionaryRef reply = NULL;
    xpc_object_t message = securityd_create_message(kSecXPCOpWhoAmI, error);
    if (message) {
        xpc_object_t response = securityd_message_with_reply_sync(message, error);
        if (response) {
            reply = _CFXPCCreateCFObjectFromXPCObject(response);
            xpc_release(response);
        } else {
            secerror("Securityd failed getting whoamid with error: %@",
                     error ? *error : NULL);
        }
        xpc_release(message);
    }
    return reply;
}

// This function is only effective in edu mode (Shared iPad).
// This function will call out to the foreground user session.
bool
_SecSyncBubbleTransfer(CFArrayRef services, uid_t uid, CFErrorRef *error)
{
    xpc_object_t message;
    bool reply = false;

    message = securityd_create_message(kSecXPCOpTransmogrifyToSyncBubble, error);
    if (message) {
        xpc_dictionary_set_int64(message, "uid", uid);
        if (SecXPCDictionarySetPList(message, "services", services, error)) {
            xpc_object_t response = securityd_message_with_reply_sync(message, error);
            if (response) {
                reply = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
                if (!reply)
                    securityd_message_no_error(response, error);
                xpc_release(response);
            }
            xpc_release(message);
        }
    }
    return reply;
}

// This function is only effective in edu mode (Shared iPad).
// This function will call out to the foreground user session.
bool
_SecSystemKeychainTransfer(CFErrorRef *error)
{
    xpc_object_t message;
    bool reply = false;

    message = securityd_create_message(kSecXPCOpTransmogrifyToSystemKeychain, error);
    if (message) {
        xpc_object_t response = securityd_message_with_reply_sync(message, error);
        if (response) {
            reply = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
            if (!reply)
                securityd_message_no_error(response, error);
            xpc_release(response);
        }
        xpc_release(message);
    }
    return reply;
}

// This function is to be used when:
// - provisioning edu mode (Shared iPad) from a clean install, after transfering (transmogrifying) items to the system keychain
// - upgrade installing a device in edu mode (Shared iPad), because the system keychain items were previously protected by a different keybag
bool
_SecSystemKeychainTranscrypt(CFErrorRef *error)
{
    xpc_object_t message;
    bool reply = false;

    message = securityd_create_message(kSecXPCOpTranscryptToSystemKeychainKeybag, error);
    if (message) {
        xpc_object_t response = securityd_message_with_reply_sync(message, error);
        if (response) {
            reply = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
            if (!reply)
                securityd_message_no_error(response, error);
            xpc_release(response);
        }
        xpc_release(message);
    }
    return reply;
}

// This function is only effective in edu mode (Shared iPad).
// This function will call out to the foreground user session.
bool
_SecSyncDeleteUserViews(uid_t uid, CFErrorRef *error)
{
    xpc_object_t message;
    bool reply = false;

    message = securityd_create_message(kSecXPCOpDeleteUserView, error);
    if (message) {
        xpc_dictionary_set_int64(message, "uid", uid);

        xpc_object_t response = securityd_message_with_reply_sync(message, error);
        if (response) {
            reply = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
            if (!reply)
                securityd_message_no_error(response, error);
            xpc_release(response);
        }
        xpc_release(message);
    }
    return reply;
}

XPC_RETURNS_RETAINED xpc_endpoint_t
_SecSecuritydCopyEndpoint(enum SecXPCOperation op, CFErrorRef *error)
{
    xpc_endpoint_t endpoint = NULL;
    xpc_object_t message = securityd_create_message(op, error);
    if (message) {
        xpc_object_t response = securityd_message_with_reply_sync(message, error);
        if (response) {
            endpoint = xpc_dictionary_get_value(response, kSecXPCKeyEndpoint);
            if (endpoint) {
                if(xpc_get_type(endpoint) != XPC_TYPE_ENDPOINT) {
                    secerror("endpoint was not an endpoint");
                    endpoint = NULL;
                } else {
                    xpc_retain(endpoint);
                }
            } else {
                secerror("endpoint was null");
            }
            xpc_release(response);
        } else {
            secerror("Securityd failed getting endpoint with error: %@", error ? *error : NULL);
        }
        xpc_release(message);
    }
    return endpoint;
}


XPC_RETURNS_RETAINED xpc_endpoint_t
_SecSecuritydCopyCKKSEndpoint(CFErrorRef *error)
{
    return NULL;
}

XPC_RETURNS_RETAINED xpc_endpoint_t
_SecSecuritydCopyKeychainControlEndpoint(CFErrorRef* error)
{
    return _SecSecuritydCopyEndpoint(kSecXPCOpKeychainControlEndpoint, error);
}

XPC_RETURNS_RETAINED xpc_endpoint_t
_SecSecuritydCopySFKeychainEndpoint(CFErrorRef* error)
{
    return _SecSecuritydCopyEndpoint(kSecXPCOpSFKeychainEndpoint, error);
}
