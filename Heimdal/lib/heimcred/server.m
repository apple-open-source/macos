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

#import <TargetConditionals.h>

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <System/sys/codesign.h>

#import <xpc/xpc.h>
#import <xpc/private.h>
#import <libproc.h>
#import <launch.h>
#import <launch_priv.h>
#import <bsm/libbsm.h>
#import <bsm/audit.h>
#import <bsm/audit_session.h>
#include <sandbox.h>

#include <dirhelper_priv.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pwd.h>

#import <os/log.h>
#import <os/log_private.h>
#import <os/transaction_private.h>

#import "common.h"
#import "aks.h"
#import "heimbase.h"
#import "gsscred.h"
#import "ManagedAppManager.h"
#import "krb5.h"
#import "config.h"
#import "acquirecred.h"
#import "GSSCredHelper.h"
#import "GSSCredHelperClient.h"
#import "GSSCredXPCHelperClient.h"

#define _PATH_KCM_CONF	    SYSCONFDIR "/kcm.conf"

#if __has_include(<MobileKeyBag/MobileKeyBag.h>)
#include <MobileKeyBag/MobileKeyBag.h>
#define HAVE_MOBILE_KEYBAG_SUPPORT 1
#endif

#if __has_include(<UserManagement/UserManagement.h>)
#include <UserManagement/UserManagement.h>
#endif

static dispatch_queue_t runQueue;

static CFStringRef
CopySigningIdentitier(xpc_connection_t conn)
{
#if TARGET_OS_SIMULATOR
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
    } else {
	free(buffer);
	return NULL;
    }

    
    if (p[len - 1] != '\0') {
	free(buffer);
	return NULL;
    }
    
    res = CFStringCreateWithBytes(NULL, (UInt8 *)p, len - 1, kCFStringEncodingUTF8, false);
    free(buffer);
    return res;
#endif
}

// this method will verify that the audit token is for an apple signed caller with the supplied identifier
static bool
verifyAppleSigned(struct peer *peer, NSString *identifierToVerify)
{
    bool applesigned = false;
#if TARGET_OS_OSX
    OSStatus result = noErr;
    SecCodeRef codeRef = NULL;
    SecRequirementRef secRequirementRef = NULL;
    NSString *requirement;
    NSDictionary *attributes;
    
    audit_token_t auditToken;
    xpc_connection_get_audit_token(peer->peer, &auditToken);
    
    requirement = [NSString stringWithFormat:@"identifier \"%@\" and anchor apple", identifierToVerify];
    os_log_debug(GSSOSLog(), "requirement: %@", requirement);
    result = SecRequirementCreateWithString((__bridge CFStringRef _Nonnull)requirement, kSecCSDefaultFlags, &secRequirementRef);
    if (result || !secRequirementRef) {
	os_log_error(GSSOSLog(), "Error creating requirement %d ", result);
	applesigned = false;
	goto cleanup;
    }
    
    attributes = @{(id)kSecGuestAttributeAudit:[NSData dataWithBytes:auditToken.val length:sizeof(auditToken)]};
    result = SecCodeCopyGuestWithAttributes(NULL, (__bridge CFDictionaryRef _Nullable)attributes, kSecCSDefaultFlags, &codeRef);
    if (result || !codeRef) {
	os_log_error(GSSOSLog(), "Error creating code ref: %d ", result);
	applesigned = false;
	goto cleanup;
    }
    
    CFErrorRef error;
    result = SecCodeCheckValidityWithErrors(codeRef, kSecCSDefaultFlags, secRequirementRef, &error);
    if (result) {
	os_log_error(GSSOSLog(), "Error checking requirement: %d, %@", result, (__bridge NSError *)error);
	applesigned = false;
	goto cleanup;
    }
    
    applesigned = true;
    
cleanup:
    
    CFRELEASE_NULL(secRequirementRef);
    CFRELEASE_NULL(codeRef);
#endif
    return applesigned;
}
    
static void saveToDiskIfNeeded(void)
{
    dispatch_assert_queue(runQueue);
    dispatch_async(runQueue, ^{  //this dispatch is not needed rdar://66399773 (tsan incorrectly identifying threading issues for work on a serial dispatch queue)
    if (HeimCredCTX.needFlush) {
	HeimCredCTX.needFlush = false;

	if (!HeimCredCTX.flushPending) {
	    HeimCredCTX.flushPending = true;

	    os_transaction_t transaction = os_transaction_create("com.apple.Heimdal.GSSCred.StoreCredCache");

	    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC), runQueue, ^{
		//run the save on the runQueue to prevent threading issues with commands and creds
		HeimCredCTX.flushPending = false;
		storeCredCache();
		(void)transaction;
	    });
	}
    }
    });
}

static void executeOnRunQueue(dispatch_block_t block)
{
    dispatch_async(runQueue, block);
}

static void GSSCred_peer_event_handler(struct peer *peer, xpc_object_t event)
{
    dispatch_assert_queue(runQueue);
    @autoreleasepool {
	
	xpc_object_t reply  = NULL;
	xpc_type_t type = xpc_get_type(event);
	if (type == XPC_TYPE_ERROR) {
	    peer->peer = nil;
	    return;
	}
	
	assert(type == XPC_TYPE_DICTIONARY);
	
	/*
	 * Check if we are impersonating a different bundle
	 */
	const char *bundleString = xpc_dictionary_get_string(event, "impersonate");
	if (bundleString) {
	    CFStringRef bundle = CFStringCreateWithBytes(NULL, (UInt8 *)bundleString, strlen(bundleString), kCFStringEncodingUTF8, false);
	    if (bundle && !CFEqual(peer->bundleID, bundle)) {
		if (HeimCredGlobalCTX.hasEntitlement(peer, "com.apple.private.accounts.bundleidspoofing")) {
		    CFRELEASE_NULL(peer->bundleID);
		    peer->bundleID = CFRetain(bundle);
		    peer->needsManagedAppCheck = true;
		    peer->isManagedApp = false;
		    os_log_debug(GSSOSLog(), "impersonating app: %s", CFStringGetCStringPtr(peer->bundleID, kCFStringEncodingUTF8));
		} else {
		    xpc_connection_cancel(peer->peer);
		    CFRELEASE_NULL(bundle);
		    return;
		}
	    }
	    if (bundle) {
		CFRELEASE_NULL(bundle);
	    }
	}
	//the bundle id above should be used along with audit token for impersonation.  The code below is to prevent a client from assigning only the audittoken and have it be accepted.
#if TARGET_OS_OSX
	size_t tokenLength;
	const void *data = xpc_dictionary_get_data(event, "impersonate_token", &tokenLength);
	if (tokenLength >= sizeof(audit_token_t)) {
	    audit_token_t impersonate_token = { 0 };
	    memcpy(&impersonate_token, data, sizeof(audit_token_t));
	    if (!HeimCredGlobalCTX.hasEntitlement(peer, "com.apple.private.accounts.bundleidspoofing")) {
		xpc_connection_cancel(peer->peer);
		return;
	    }
	    
	}
#endif
	const char *cmd = xpc_dictionary_get_string(event, "command");
	if (cmd == NULL) {
	    os_log_error(GSSOSLog(), "peer sent invalid no command");
	    xpc_connection_cancel(peer->peer);
	} else if (strcmp(cmd, "wakeup") == 0) {
	
	} else if (strcmp(cmd, "create") == 0) {
	    reply = xpc_dictionary_create_reply(event);
	    do_CreateCred(peer, event, reply);
	} else if (strcmp(cmd, "delete") == 0) {
	    reply = xpc_dictionary_create_reply(event);
	    do_Delete(peer, event, reply);
	} else if (strcmp(cmd, "delete-all") == 0) {
	    reply = xpc_dictionary_create_reply(event);
	    do_DeleteAll(peer, event, reply);
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
	    do_RetainCache(peer, event, reply);
	} else if (strcmp(cmd, "release-transient") == 0) {
	    reply = xpc_dictionary_create_reply(event);
	    do_ReleaseCache(peer, event, reply);
	} else if (strcmp(cmd, "status") == 0) {
	    reply = xpc_dictionary_create_reply(event);
	    do_Status(peer, event, reply);
	} else if (strcmp(cmd, "doauth") == 0) {
	    reply = xpc_dictionary_create_reply(event);
	    do_Auth(peer, event, reply);
	} else {
	    os_log_error(GSSOSLog(), "peer sent invalid command %s", cmd);
	    xpc_connection_cancel(peer->peer);
	}
	
	if (reply) {
	    xpc_connection_send_message(peer->peer, reply);
	}
	
	HeimCredGlobalCTX.saveToDiskIfNeeded();
    }
}

static void
call_peer_final(void *ptr)
{
    dispatch_assert_queue(runQueue);
    peer_final(ptr);
}

#if !TARGET_OS_IPHONE
/*
 *
 */

static void GSSCredHelper_peer_event_handler(GSSHelperPeer *peer, xpc_object_t event)
{
    @autoreleasepool {
	xpc_object_t reply  = NULL;
	xpc_type_t type = xpc_get_type(event);
	if (type == XPC_TYPE_ERROR) {
	    return;
	}
	
	assert(type == XPC_TYPE_DICTIONARY);
	
	const char *cmd = xpc_dictionary_get_string(event, "command");
	if (cmd == NULL) {
	    os_log_error(GSSHelperOSLog(), "peer sent invalid no command");
	    xpc_connection_cancel(peer.conn);
	} else if (strcmp(cmd, "wakeup") == 0) {

	} else if (strcmp(cmd, "acquire") == 0) {
	    reply = xpc_dictionary_create_reply(event);
	    [GSSCredHelper do_Acquire:peer request:event reply:reply];
	} else if (strcmp(cmd, "refresh") == 0) {
	    reply = xpc_dictionary_create_reply(event);
	    [GSSCredHelper do_Refresh:peer request:event reply:reply];
	}

	if (reply) {
	    xpc_connection_send_message(peer.conn, reply);
	}
    }
}

static void GSSCred_session_handler(xpc_connection_t peerconn)
{
    audit_token_t token;
    uid_t uid;

    xpc_connection_get_audit_token(peerconn, &token);
    uid = HeimCredGlobalCTX.getUid(peerconn);

    if (HeimCredCTX.session != HeimCredGlobalCTX.getAsid(peerconn) && uid != 0) {
	os_log_error(GSSHelperOSLog(), "client[pid-%d] is not in same session or root", (int)xpc_connection_get_pid(peerconn));
	xpc_connection_cancel(peerconn);
	return;
    }
   
    @autoreleasepool {
	xpc_type_t type = xpc_get_type(peerconn);
	if (type == XPC_TYPE_ERROR) {
	    //when the connection closes, exit the process.  A new one will be spawned later.
	    os_log_debug(GSSHelperOSLog(), "conection closed, exiting.");
	    exit(0);
	}

	GSSHelperPeer *peer = [[GSSHelperPeer alloc] init];
	
	peer.conn = peerconn;
	peer.bundleIdentifier = CFBridgingRelease(CopySigningIdentitier(peerconn));
	peer.session = HeimCredCTX.session;

	os_log_debug(GSSHelperOSLog(), "new connection from %@", peer.bundleIdentifier);
	
	xpc_connection_set_context(peerconn, (__bridge void * _Nullable)peer);
	
	xpc_connection_set_event_handler(peerconn, ^(xpc_object_t event) {
		GSSCredHelper_peer_event_handler(peer, event);
	});
	xpc_connection_set_target_queue(peerconn, runQueue);
	xpc_connection_resume(peerconn);
    }
}
#endif

/*
 *
 */

static void GSSCred_event_handler(xpc_connection_t peerconn)
{
    @autoreleasepool {
	
	struct peer *peer;
	
	xpc_type_t type = xpc_get_type(peerconn);
	if (type == XPC_TYPE_ERROR)
	    return;
	
	peer = calloc(1, sizeof(*peer));
	heim_assert(peer != NULL, "out of memory");
	
	peer->peer = peerconn;
	peer->bundleID = CopySigningIdentitier(peerconn);
	if (peer->bundleID == NULL) {
	    char path[MAXPATHLEN];
	    
	    if (proc_pidpath(getpid(), path, sizeof(path)) <= 0)
		path[0] = '\0';
	    
	    os_log_error(GSSOSLog(), "client[pid-%d] \"%s\" is not signed", (int)xpc_connection_get_pid(peerconn), path);
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
	    free(peer);
	    xpc_connection_cancel(peerconn);
	    return;
#else
	    peer->bundleID = CFStringCreateWithFormat(NULL, NULL, CFSTR("unsigned-binary-path:%s-end"), path);
	    heim_assert(peer->bundleID != NULL, "out of memory");
#endif
	}
	peer->callingAppBundleID = CFRetain(peer->bundleID);
	os_log_debug(GSSOSLog(), "new connection from %@",  peer->callingAppBundleID);
	if (HeimCredGlobalCTX.useUidMatching) {
	    peer->session = HeimCredCopySession(-1);
	} else {
	    peer->session = HeimCredCopySession(HeimCredGlobalCTX.getAsid(peerconn));
	}
	heim_assert(peer->session != NULL, "out of memory");
	
	peer->needsManagedAppCheck = true;
	peer->isManagedApp = false;
	peer->access_status = IAKERB_NOT_CHECKED;
	
	xpc_connection_set_context(peerconn, peer);
	xpc_connection_set_finalizer_f(peerconn, call_peer_final);
	
	xpc_connection_set_event_handler(peerconn, ^(xpc_object_t event) {
	    GSSCred_peer_event_handler(peer, event);
	});
	xpc_connection_set_target_queue(peerconn, runQueue);
	    
	xpc_connection_resume(peerconn);
    }
}

static bool haveBooleanEntitlement(struct peer *peer, const char *entitlement)
{
    bool res = false;

    xpc_object_t ent = xpc_connection_copy_entitlement_value(peer->peer, entitlement);
    if (ent) {
	if (xpc_get_type(ent) == XPC_TYPE_BOOL && xpc_bool_get_value(ent))
	    res = true;
    }

    return res;
}

#if TARGET_OS_IOS && HAVE_MOBILE_KEYBAG_SUPPORT
static bool
deviceIsMultiuser(void)
{
    static dispatch_once_t once;
    static bool result;
    
    dispatch_once(&once, ^{
	CFDictionaryRef deviceMode = MKBUserTypeDeviceMode(NULL, NULL);
	CFTypeRef value = NULL;
	
	if (deviceMode && CFDictionaryGetValueIfPresent(deviceMode, kMKBDeviceModeKey, &value) && CFEqual(value, kMKBDeviceModeMultiUser)) {
	    result = true;
	}
        CFRELEASE_NULL(deviceMode);
    });
    
    return result;
}

static NSString * currentAltDSID(void)
{
    UMUser *currentUser = [[UMUserManager sharedManager] currentUser];
    os_log_debug(GSSOSLog(), "Current altDSID: %{private}@", currentUser.alternateDSID);
    return currentUser.alternateDSID;
}
#else
static bool
deviceIsMultiuser(void)
{
    return false;
}

static NSString * currentAltDSID(void)
{
    return nil;
}
#endif /* HAVE_MOBILE_KEYBAG_SUPPORT && TARGET_OS_IOS */

static CFPropertyListRef
getValueFromPreferences(CFStringRef key)
{
    CFStringRef domain = CFSTR("com.apple.Heimdal.GSSCred");
    CFPreferencesSynchronize(domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    CFPropertyListRef value = (__bridge CFTypeRef)(CFBridgingRelease(CFPreferencesCopyValue(key, domain, kCFPreferencesAnyUser, kCFPreferencesAnyHost)));
    CFRELEASE_NULL(domain);
    return value;
}

static bool
getLegacyKCMUidMatching(void)
{
    bool useUidMatching = false;
    krb5_context kcm_context = NULL;
    krb5_error_code ret;
    const char *config_file = _PATH_KCM_CONF;
    
    ret = krb5_init_context(&kcm_context);
    if (ret) {
	os_log_error(GSSOSLog(), "krb5_init_context failed: %d", ret);
	return false;
    }

    char **files;
    
    ret = krb5_prepend_config_files_default(config_file, &files);
    if (ret) {
	os_log_error(GSSOSLog(), "error getting kcm configuration files");
	krb5_free_context(kcm_context);
	return false;
    }
    
    ret = krb5_set_config_files(kcm_context, files);
    krb5_free_config_files(files);
    if (ret) {
	os_log_error(GSSOSLog(), "error reading kcm configuration files");
	krb5_free_context(kcm_context);
	return false;
    }
    
    useUidMatching = krb5_config_get_bool_default(kcm_context, NULL,
						    0,
						    "kcm",
						    "use-uid-matching", NULL);
    if (useUidMatching) {
	os_log_error(GSSOSLog(), "*** Using 'use-uid-matching' in the kcm section of a krb5.conf file is deprecated.  It will be removed in a future macOS version.  Use the GSSCred default instead. ");
    }
    
    krb5_free_context(kcm_context);
    return useUidMatching;
}

static bool
useUidForMatching(void)
{
    static bool useUidMatching = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
	NSNumber *value = (__bridge NSNumber *)HeimCredGlobalCTX.getValueFromPreferences(CFSTR("use-uid-matching"));
	useUidMatching = ((value != nil) ? value.boolValue : getLegacyKCMUidMatching());
    });
    return useUidMatching;
    
}

static time_t getCredRenewInterval(void)
{
    time_t renew_interval = 3600 * 2;  //two hour default
    NSNumber *value = (__bridge NSNumber *)HeimCredGlobalCTX.getValueFromPreferences(CFSTR("renew-interval"));
    if (value) {
	renew_interval = ([value longValue] < 60) ? 60 : [value longValue];  //min interval is 60 seconds
    }
    
    return renew_interval;
}

static bool
sessionExists(pid_t asid)
{
#if TARGET_OS_IPHONE
    return true;
#else
    auditinfo_addr_t aia;
    aia.ai_asid = asid;
    
    if (audit_get_sinfo_addr(&aia, sizeof(aia)) == 0)
	return true;
    return false;
#endif
}

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
	    
	    dispatch_async(runQueue, ^{  /// <---- this is dispatched to the same queue as auth work to avoid concurrency issues
		int sessionID = aio.ai_asid;
		RemoveSession(sessionID);
	    });
	}
    });
}


/*
 *
 */
#if TARGET_OS_OSX
static void
enterSandbox(void)
{
	char buf[PATH_MAX] = "";

	char *home_env = getenv("HOME");
	if (home_env == NULL) {
		os_log_debug(OS_LOG_DEFAULT, "$HOME not set, falling back to using getpwuid");
		struct passwd *pwd = getpwuid(getuid());
		if (pwd == NULL) {
			os_log_error(OS_LOG_DEFAULT, "failed to get passwd entry for uid %u", getuid());
			exit(EXIT_FAILURE);
		}
		home_env = pwd->pw_dir;
	}

	char *home = realpath(home_env, NULL);
	if (home == NULL) {
		os_log_error(OS_LOG_DEFAULT, "failed to resolve user's home directory: %{darwin.errno}d", errno);
		exit(EXIT_FAILURE);
	}

	if (!_set_user_dir_suffix("com.apple.GSSCred") ||
	    confstr(_CS_DARWIN_USER_TEMP_DIR, buf, sizeof(buf)) == 0) {
		os_log_error(OS_LOG_DEFAULT, "failed to initialize temporary directory: %{darwin.errno}d", errno);
		exit(EXIT_FAILURE);
	}

	char *tempdir = realpath(buf, NULL);
	if (tempdir == NULL) {
		os_log_error(OS_LOG_DEFAULT, "failed to resolve temporary directory: %{darwin.errno}d", errno);
		exit(EXIT_FAILURE);
	}

	if (confstr(_CS_DARWIN_USER_CACHE_DIR, buf, sizeof(buf)) == 0) {
		os_log_error(OS_LOG_DEFAULT, "failed to initialize cache directory: %{darwin.errno}d", errno);
		exit(EXIT_FAILURE);
	}

	char *cachedir = realpath(buf, NULL);
	if (cachedir == NULL) {
		os_log_error(OS_LOG_DEFAULT, "failed to resolve cache directory: %{darwin.errno}d", errno);
		exit(EXIT_FAILURE);
	}

	const char *parameters[] = {
		"HOME", home,
		"TMPDIR", tempdir,
		"DARWIN_CACHE_DIR", cachedir,
		NULL
	};

	char *sberror = NULL;
	// Note: The name of the sandbox profile here does not include the '.sb' extension.
	if (sandbox_init_with_parameters("com.apple.GSSCred", SANDBOX_NAMED, parameters, &sberror) != 0) {
		os_log_error(OS_LOG_DEFAULT, "Failed to enter sandbox: %{public}s", sberror);
		exit(EXIT_FAILURE);
	}

	free(home);
	free(tempdir);
	free(cachedir);
}
#endif

int main(int argc, const char *argv[])
{
    xpc_connection_t conn;
    bool startedWithInstanceID = false;
    
    os_log_set_client_type(OS_LOG_CLIENT_TYPE_LOGD_DEPENDENCY, 0);

    umask(S_IRWXG | S_IRWXO);

#if TARGET_OS_OSX
    enterSandbox();
#endif
    
    @autoreleasepool {
	/* Tell logd we're special */
	
	os_log_info(GSSOSLog(), "Starting GSSCred");
	HeimCredGlobalCTX.getValueFromPreferences = getValueFromPreferences;
	HeimCredGlobalCTX.currentAltDSID = currentAltDSID;
	HeimCredGlobalCTX.managedAppManager = [[ManagedAppManager alloc] init];
	HeimCredGlobalCTX.hasEntitlement = haveBooleanEntitlement;
	HeimCredGlobalCTX.getUid = xpc_connection_get_euid;
	HeimCredGlobalCTX.getAsid = xpc_connection_get_asid;
	HeimCredGlobalCTX.encryptData = ksEncryptData;
	HeimCredGlobalCTX.decryptData = ksDecryptData;
	HeimCredGlobalCTX.verifyAppleSigned = verifyAppleSigned;
	HeimCredGlobalCTX.sessionExists = sessionExists;
	HeimCredGlobalCTX.saveToDiskIfNeeded = saveToDiskIfNeeded;
	HeimCredGlobalCTX.isMultiUser = deviceIsMultiuser();
	HeimCredGlobalCTX.useUidMatching = useUidForMatching();
	HeimCredGlobalCTX.expireFunction = expire_func;
	HeimCredGlobalCTX.renewFunction = renew_func;
	HeimCredGlobalCTX.finalFunction = final_func;
	HeimCredGlobalCTX.notifyCaches = notifyChangedCaches;
	HeimCredGlobalCTX.renewInterval = getCredRenewInterval();
	HeimCredGlobalCTX.gssCredHelperClientClass = GSSCredXPCHelperClient.class;
	HeimCredGlobalCTX.executeOnRunQueue = executeOnRunQueue;
	
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
	_HeimCredRegisterKerberosAcquireCred();
	
	CFRELEASE_NULL(HeimCredCTX.globalSchema);
	
#if TARGET_OS_SIMULATOR
	archivePath = [[NSString alloc] initWithFormat:@"%@/Library/Caches/com.apple.GSSCred.simulator-archive", NSHomeDirectory()];
#else
	archivePath = @"/var/db/heim-credential-store.archive";
#endif
	
	runQueue = dispatch_queue_create("com.apple.GSSCred", DISPATCH_QUEUE_SERIAL);
	heim_assert(runQueue != NULL, "dispatch_queue_create failed");
	
#if !TARGET_OS_IPHONE
	char *instanceId = getenv(LAUNCH_ENV_INSTANCEID);
	if (instanceId) {
	    startedWithInstanceID = true;
	    /*
	     * Pull out uuid and session stored in the sessionUUID
	     */
	    os_log_debug(GSSHelperOSLog(), "Starting with instance id: %s", instanceId);
	    uuid_t sessionUUID;
	    if (uuid_parse(instanceId, (void *)&sessionUUID) != 0) {
		os_log_error(GSSOSLog(), "can't parse LAUNCH_ENV_INSTANCEID as a uuid");
		return EPERM;
	    }
	    
	    uid_t auid;
	    memcpy(&auid, &sessionUUID, sizeof(auid));
	    
	    HeimCredCTX.session = auid;
	    os_log_debug(GSSHelperOSLog(), "Session id: %u", auid);
	    if (HeimCredCTX.session == 0) {
		os_log_error(GSSHelperOSLog(), "0 is not a valid session");
		return EPERM;
	    }
	    
	    /*
	     * Join that session
	     */
	    mach_port_name_t session_port;
	    int ret = audit_session_port(HeimCredCTX.session, &session_port);
	    if (ret) {
		os_log_error(GSSHelperOSLog(), "could not get audit session port for %d: %s", HeimCredCTX.session, strerror(errno));
		return EPERM;
	    }
	    audit_session_join(session_port);
	    mach_port_deallocate(current_task(), session_port);
	    
	    
	    conn = xpc_connection_create_mach_service("com.apple.GSSCred",
						      runQueue,
						      XPC_CONNECTION_MACH_SERVICE_LISTENER);
	    
	    xpc_connection_set_event_handler(conn, ^(xpc_object_t object){
		
		GSSCred_session_handler(object);
	    });
	    
	} else
#endif
	{

	    heim_ipc_init_globals();
	    _HeimCredInitCommon();
	    readCredCache();
	    SessionMonitor();  //start the session monitor after loading the data to prevent conflicts.
	    
	    
	    conn = xpc_connection_create_mach_service("com.apple.GSSCred",
						      runQueue,
						      XPC_CONNECTION_MACH_SERVICE_LISTENER);
	    
	    xpc_connection_set_event_handler(conn, ^(xpc_object_t object){
		GSSCred_event_handler(object);
	    });
	    heim_ipc_resume_events();
	}
	
	xpc_connection_resume(conn);
    }
    
    if (startedWithInstanceID) {
	os_log_info(GSSHelperOSLog(), "Starting run loop");
    } else {
	os_log_info(GSSOSLog(), "Starting run loop");
    }

    [[NSRunLoop currentRunLoop] run];
    
    return EXIT_SUCCESS;
}
