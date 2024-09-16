/*
 * Copyright (c) 2014-2016 Apple Inc.  All Rights Reserved.
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

#include <mach/mach.h>
#include <mach/message.h>

#include <stdlib.h>
#include <sys/queue.h>
#include <libproc.h>

#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItemPriv.h> /* for SecErrorGetOSStatus */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPriv.h>

#include <Foundation/Foundation.h>

#if TARGET_OS_IPHONE
#include <CoreFoundation/CFUserNotification.h>
#else
#include <CoreFoundation/CFUserNotificationPriv.h>
#endif

#if TARGET_OS_IPHONE
#include <SpringBoardServices/SpringBoardServices.h>
#include <MobileCoreServices/LSApplicationProxy.h>
#endif

#if TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV
#include <dlfcn.h>
#include <WebUI/WBUAutoFillData.h>

typedef WBSAutoFillDataClasses (*WBUAutoFillGetEnabledDataClasses_f)(void);
#endif

#include <syslog.h>
#include <bsm/libbsm.h>
#include <utilities/SecIOFormat.h>
#include <utilities/debugging.h>

#include <ipc/securityd_client.h>
#include "swcagent_client.h"

#include "keychain/securityd/SecItemServer.h"
#include "trust/trustd/SecTrustServer.h"
#include "trust/trustd/SecTrustStoreServer.h"
#include "keychain/securityd/spi.h"
#include <Security/SecTask.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecXPCError.h>

#include <Security/SecEntitlements.h>

#include <Security/SecuritydXPC.h>

#include <xpc/xpc.h>
#include <xpc/private.h>
#include <xpc/connection_private.h>
#include <AssertMacros.h>

#if TARGET_OS_IOS
#import <LocalAuthentication/LocalAuthentication.h>
#import <LocalAuthentication/LAContext+Private.h>
#import <MobileGestalt.h>
#import <ManagedConfiguration/ManagedConfiguration.h>
#endif

static NSString *swca_string_table = @"SharedWebCredentials";

static bool SecTaskGetBooleanValueForEntitlement(SecTaskRef task, CFStringRef entitlement)
{
    CFStringRef canModify = (CFStringRef)SecTaskCopyValueForEntitlement(task, entitlement, NULL);
    if (!canModify)
        return false;
    CFTypeID canModifyType = CFGetTypeID(canModify);
    bool ok = (CFBooleanGetTypeID() == canModifyType) && CFBooleanGetValue((CFBooleanRef)canModify);
    CFRelease(canModify);
    return ok;
}

static CFArrayRef SecTaskCopyArrayOfStringsForEntitlement(SecTaskRef task, CFStringRef entitlement)
{
    CFArrayRef value = (CFArrayRef)SecTaskCopyValueForEntitlement(task,
        entitlement, NULL);
    if (value) {
        if (CFGetTypeID(value) == CFArrayGetTypeID()) {
            CFIndex ix, count = CFArrayGetCount(value);
            for (ix = 0; ix < count; ++ix) {
                CFStringRef string = (CFStringRef)CFArrayGetValueAtIndex(value, ix);
                if (CFGetTypeID(string) != CFStringGetTypeID()) {
                    CFRelease(value);
                    value = NULL;
                    break;
                }
            }
        } else {
            CFRelease(value);
            value = NULL;
        }
    }

    return value;
}

/* Identify a client */
enum {
	CLIENT_TYPE_BUNDLE_IDENTIFIER,
	CLIENT_TYPE_EXECUTABLE_PATH,
};

@interface Client : NSObject
@property int client_type;
@property(retain) NSString *client;
@property(retain) NSString *client_name;
@property(retain) NSString *path;
@property(retain) NSBundle *bundle;
@end

@implementation Client
@end

static Client *identify_client(pid_t pid)
{
	Client *client = [[Client alloc] init];
	if (!client)
		return nil;

	char path_buf[PROC_PIDPATHINFO_SIZE] = "";
	NSURL *path_url;

#if TARGET_OS_IPHONE
	if (proc_pidpath(pid, path_buf, sizeof(path_buf)) <= 0) {
		secnotice("swcagent", "Refusing client without path (pid %d)", pid);
		return nil;
	}
#else
	size_t path_len = sizeof(path_buf);
	if (responsibility_get_responsible_for_pid(pid, NULL, NULL, &path_len, path_buf) != 0) {
		secnotice("swcagent", "Refusing client without path (pid %d)", pid);
		[client release];
		return nil;
	}
#endif
	path_buf[sizeof(path_buf) - 1] = '\0';

	if (!(client.path = [NSString stringWithUTF8String:path_buf]) ||
	    !(path_url = [NSURL fileURLWithPath:client.path])) {
		secnotice("swcagent", "Refusing client without path (pid %d)", pid);
		return nil;
	}

	NSURL *bundle_url;
	if ((bundle_url = CFBridgingRelease(_CFBundleCopyBundleURLForExecutableURL((__bridge CFURLRef)path_url))) &&
	    (client.bundle = [NSBundle bundleWithURL:bundle_url]) &&
	    (client.client = [client.bundle bundleIdentifier])) {
		client.client_type = CLIENT_TYPE_BUNDLE_IDENTIFIER;
		CFStringRef client_name_cf = NULL;
#if TARGET_OS_IPHONE
		client.client_name = [[LSApplicationProxy applicationProxyForIdentifier:client.client] localizedNameForContext:nil];
#else
		if (!LSCopyDisplayNameForURL((__bridge CFURLRef)bundle_url, &client_name_cf))
			client.client_name = (__bridge_transfer NSString *)client_name_cf;
#endif
		if (client_name_cf)
			CFRelease(client_name_cf);
	} else {
#if TARGET_OS_IPHONE
		secnotice("swcagent", "Refusing client without bundle identifier (%s)", path_buf);
		return nil;
#else
		client.client_type = CLIENT_TYPE_EXECUTABLE_PATH;
		CFBooleanRef is_app = NULL;
		CFStringRef client_name_cf;
		if (bundle_url &&
		    CFURLCopyResourcePropertyForKey((__bridge CFURLRef)bundle_url, kCFURLIsApplicationKey, &is_app, NULL) &&
		    is_app == kCFBooleanTrue) {
			if ((client.client = [bundle_url path]) &&
			    !LSCopyDisplayNameForURL((__bridge CFURLRef)bundle_url, &client_name_cf))
				client.client_name = (__bridge_transfer NSString *)client_name_cf;
		} else {
			client.client = client.path;
			if (!LSCopyDisplayNameForURL((__bridge CFURLRef)path_url, &client_name_cf))
				client.client_name = (__bridge_transfer NSString *)client_name_cf;
		}
		if (is_app)
			CFRelease(is_app);
#endif
	}

    return client;
}

struct __SecTask {
    CFRuntimeBase base;

    audit_token_t token;

    /* Track whether we've loaded entitlements independently since after the
     * load, entitlements may legitimately be NULL */
    Boolean entitlementsLoaded;
    CFDictionaryRef entitlements;
    
    /* for debugging only, shown by debugDescription */
    int lastFailure;
};

static Client* SecTaskCopyClient(SecTaskRef task)
{
    // SecTaskCopyDebugDescription is not sufficient to get the localized client name
    pid_t pid;
    audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
    Client *client = identify_client(pid);
    return client;
}

static CFArrayRef SecTaskCopyAccessGroups(SecTaskRef task) {
    return SecTaskCopyArrayOfStringsForEntitlement(task, kSecEntitlementAssociatedDomains);
}

// Local function declarations

static CFArrayRef gActiveArray = NULL;
static CFDictionaryRef gActiveItem = NULL;


static CFStringRef SWCAGetOperationDescription(enum SWCAXPCOperation op)
{
    switch (op) {
        case swca_add_request_id:
            return CFSTR("swc add");
        case swca_update_request_id:
            return CFSTR("swc update");
        case swca_delete_request_id:
            return CFSTR("swc delete");
        case swca_copy_request_id:
            return CFSTR("swc copy");
        case swca_select_request_id:
            return CFSTR("swc select");
        case swca_copy_pairs_request_id:
            return CFSTR("swc copy pairs");
        case swca_set_selection_request_id:
            return CFSTR("swc set selection");
        case swca_enabled_request_id:
            return CFSTR("swc enabled");
        default:
            return CFSTR("Unknown xpc operation");
    }
}

#if !TARGET_OS_SIMULATOR && TARGET_OS_IPHONE && !TARGET_OS_WATCH
static dispatch_once_t                      sWBUInitializeOnce	= 0;
static void *                               sWBULibrary			= NULL;
static WBUAutoFillGetEnabledDataClasses_f	sWBUAutoFillGetEnabledDataClasses_f	= NULL;

static OSStatus _SecWBUEnsuredInitialized(void);

static OSStatus _SecWBUEnsuredInitialized(void)
{
    __block OSStatus status = errSecNotAvailable;

    dispatch_once(&sWBUInitializeOnce, ^{
        sWBULibrary = dlopen("/System/Library/PrivateFrameworks/WebUI.framework/WebUI", RTLD_LAZY | RTLD_LOCAL);
        assert(sWBULibrary);
        if (sWBULibrary) {
            sWBUAutoFillGetEnabledDataClasses_f = (WBUAutoFillGetEnabledDataClasses_f)(uintptr_t) dlsym(sWBULibrary, "WBUAutoFillGetEnabledDataClasses");
        }
    });

    if (sWBUAutoFillGetEnabledDataClasses_f) {
        status = noErr;
    }
    return status;
}
#endif

static bool SWCAIsAutofillEnabled(void)
{
#if TARGET_OS_SIMULATOR
    // Assume the setting's on in the simulator: <rdar://problem/17057358> WBUAutoFillGetEnabledDataClasses call failing in the Simulator
    return true;
#elif TARGET_OS_IPHONE && !TARGET_OS_WATCH
    OSStatus status = _SecWBUEnsuredInitialized();
    if (status) { return false; }
    WBSAutoFillDataClasses autofill = sWBUAutoFillGetEnabledDataClasses_f();
    return ((autofill & WBSAutoFillDataClassUsernamesAndPasswords) != 0);
#else
    //%%% unsupported platform
    return false;
#endif
}

static NSBundle* swca_get_security_bundle(void)
{
    static NSBundle *security_bundle;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString *security_path = @"/System/Library/Frameworks/Security.framework";
#if TARGET_OS_SIMULATOR
        security_path = [NSString stringWithFormat:@"%s%@", getenv("IPHONE_SIMULATOR_ROOT"), security_path];
#endif
        security_bundle = [NSBundle bundleWithPath:security_path];
    });
    return security_bundle;
}

static CFOptionFlags swca_handle_request(enum SWCAXPCOperation operation, Client* client, CFArrayRef domains)
{
    CFUserNotificationRef notification = NULL;
    NSMutableDictionary *notification_dictionary = NULL;
    NSString *request_key;
    NSString *request_format;
    NSString *default_button_key;
    NSString *alternate_button_key;
    NSString *other_button_key;
    NSString *info_message_key;
    NSString *domain;
    char *op = NULL;
    CFOptionFlags response = 0 | kCFUserNotificationCancelResponse;
    BOOL alert_sem_held = NO;

check_database:
    /* If we have previously allowed this operation/domain for this client,
     * check and don't prompt again.
     */
    ; /* %%% TBD */

    /* Only display one alert at a time. */
	static dispatch_semaphore_t alert_sem;
	static dispatch_once_t alert_once;
	dispatch_once(&alert_once, ^{
		if (!(alert_sem = dispatch_semaphore_create(1)))
			abort();
	});
	if (!alert_sem_held) {
		alert_sem_held = YES;
		if (dispatch_semaphore_wait(alert_sem, DISPATCH_TIME_NOW)) {
			/* Wait for the active alert, then recheck the database in case both alerts are for the same client. */
			//secnotice("swcagent", "Delaying prompt for pid %d", pid);
			dispatch_semaphore_wait(alert_sem, DISPATCH_TIME_FOREVER);
			goto check_database;
		}
	}

    notification_dictionary = [NSMutableDictionary dictionary];
    domain = nil;
    if (1 == [(__bridge NSArray *)domains count]) {
        domain = (NSString *)[(__bridge NSArray *)domains objectAtIndex:0];
    }
    switch (operation) {
        case swca_add_request_id:
            op = "ADD";
            break;
        case swca_update_request_id:
            op = "UPDATE";
            break;
        case swca_delete_request_id:
            op = "DELETE";
            break;
        case swca_copy_request_id:
            op = (domain) ? "COPY" : "COPYALL";
            break;
        default:
            op = "USE";
            break;
    }
    if (!op) {
        goto out;
    }
    request_key = [NSString stringWithFormat:@"SWC_REQUEST_%s", op];
    request_format = NSLocalizedStringFromTableInBundle(request_key, swca_string_table, swca_get_security_bundle(), nil);
    alternate_button_key = (op) ? [NSString stringWithFormat:@"SWC_ALLOW_%s", op] : nil;
    default_button_key = @"SWC_NEVER";
    other_button_key = @"SWC_DENY";
    info_message_key = @"SWC_INFO_MESSAGE";

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    notification_dictionary[(__bridge NSString *)kCFUserNotificationAlertHeaderKey] = [NSString stringWithFormat:request_format, client.client_name, domain];
#pragma clang diagnostic pop
    notification_dictionary[(__bridge NSString *)kCFUserNotificationAlertMessageKey] = NSLocalizedStringFromTableInBundle(info_message_key, swca_string_table, swca_get_security_bundle(), nil);
    notification_dictionary[(__bridge NSString *)kCFUserNotificationDefaultButtonTitleKey] = NSLocalizedStringFromTableInBundle(default_button_key, swca_string_table, swca_get_security_bundle(), nil);
    notification_dictionary[(__bridge NSString *)kCFUserNotificationAlternateButtonTitleKey] = NSLocalizedStringFromTableInBundle(alternate_button_key, swca_string_table, swca_get_security_bundle(), nil);

    if (other_button_key) {
    // notification_dictionary[(__bridge NSString *)kCFUserNotificationOtherButtonTitleKey] = NSLocalizedStringFromTableInBundle(other_button_key, swc_table, security_bundle, nil);
    }
    notification_dictionary[(__bridge NSString *)kCFUserNotificationLocalizationURLKey] = [swca_get_security_bundle() bundleURL];
    notification_dictionary[(__bridge NSString *)SBUserNotificationAllowedApplicationsKey] = client.client;

	SInt32 error;
	if (!(notification = CFUserNotificationCreate(NULL, 0, kCFUserNotificationStopAlertLevel | kCFUserNotificationNoDefaultButtonFlag, &error, (__bridge CFDictionaryRef)notification_dictionary)) ||
	    error)
		goto out;
	if (CFUserNotificationReceiveResponse(notification, 0, &response))
		goto out;

out:
	if (alert_sem_held)
		dispatch_semaphore_signal(alert_sem);
    if (notification)
        CFRelease(notification);
    return response;
}

static bool swca_process_response(CFOptionFlags response, CFTypeRef *result)
{
    int32_t value = (int32_t)(response & 0x3);
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    *result = number;
    return (NULL != number);
}

static bool swca_confirm_add(CFDictionaryRef attributes, Client* client, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error)
{
    CFStringRef domain = (CFStringRef) CFDictionaryGetValue(attributes, kSecAttrServer);
    CFArrayRef domains = CFArrayCreate(kCFAllocatorDefault, (const void **)&domain, 1, &kCFTypeArrayCallBacks);
    CFOptionFlags response = swca_handle_request(swca_add_request_id, client, domains);
    return swca_process_response(response, result);
}

static bool swca_confirm_copy(CFDictionaryRef attributes, Client* client, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error)
{
    CFStringRef domain = (CFStringRef) CFDictionaryGetValue(attributes, kSecAttrServer);
    CFArrayRef domains = CFArrayCreate(kCFAllocatorDefault, (const void **)&domain, 1, &kCFTypeArrayCallBacks);
    CFOptionFlags response = swca_handle_request(swca_copy_request_id, client, domains);
    return swca_process_response(response, result);
}

static bool swca_confirm_update(CFDictionaryRef attributes, Client* client, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error)
{
    CFStringRef domain = (CFStringRef) CFDictionaryGetValue(attributes, kSecAttrServer);
    CFArrayRef domains = CFArrayCreate(kCFAllocatorDefault, (const void **)&domain, 1, &kCFTypeArrayCallBacks);
    CFOptionFlags response = swca_handle_request(swca_update_request_id, client, domains);
    return swca_process_response(response, result);
}

static bool swca_confirm_delete(CFDictionaryRef attributes, Client* client, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error)
{
    CFStringRef domain = (CFStringRef) CFDictionaryGetValue(attributes, kSecAttrServer);
    CFArrayRef domains = CFArrayCreate(kCFAllocatorDefault, (const void **)&domain, 1, &kCFTypeArrayCallBacks);
    CFOptionFlags response = swca_handle_request(swca_delete_request_id, client, domains);
    return swca_process_response(response, result);
}

static bool swca_select_item(CFArrayRef items, Client* client, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error)
{
    CFUserNotificationRef notification = NULL;
    NSMutableDictionary *notification_dictionary = NULL;
    NSString *request_title_format;
    NSString *info_message_key;
    NSString *default_button_key;
    NSString *alternate_button_key;
    CFOptionFlags response = 0 | kCFUserNotificationCancelResponse;
    CFIndex item_count = (items) ? CFArrayGetCount(items) : (CFIndex) 0;
    BOOL alert_sem_held = NO;

    if (item_count < 1) {
        return false;
    }

entry:
    ;
    /* Only display one alert at a time. */
    static dispatch_semaphore_t select_alert_sem;
    static dispatch_once_t select_alert_once;
    dispatch_once(&select_alert_once, ^{
        if (!(select_alert_sem = dispatch_semaphore_create(1)))
            abort();
    });
    if (!alert_sem_held) {
        alert_sem_held = YES;
        if (dispatch_semaphore_wait(select_alert_sem, DISPATCH_TIME_NOW)) {
            /* Wait for the active alert */
            dispatch_semaphore_wait(select_alert_sem, DISPATCH_TIME_FOREVER);
            goto entry;
        }
    }

    CFRetainSafe(items);
    CFReleaseSafe(gActiveArray);
    gActiveArray = items;
    CFReleaseSafe(gActiveItem);
    gActiveItem = NULL;  // selection will be set by remote view controller
    //gActiveItem = CFArrayGetValueAtIndex(items, 0);
    //CFRetainSafe(gActiveItem);

    notification_dictionary = [NSMutableDictionary dictionary];
    request_title_format = NSLocalizedStringFromTableInBundle(@"SWC_ALERT_TITLE", swca_string_table, swca_get_security_bundle(), nil);
    default_button_key = @"SWC_ALLOW_USE";
    alternate_button_key = @"SWC_CANCEL";
    info_message_key = @"SWC_INFO_MESSAGE";

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    notification_dictionary[(__bridge NSString *)kCFUserNotificationAlertHeaderKey] = [NSString stringWithFormat: request_title_format, client.client_name];
#pragma clang diagnostic pop
    notification_dictionary[(__bridge NSString *)kCFUserNotificationAlertMessageKey] = NSLocalizedStringFromTableInBundle(info_message_key, swca_string_table, swca_get_security_bundle(), nil);
    notification_dictionary[(__bridge NSString *)kCFUserNotificationDefaultButtonTitleKey] = NSLocalizedStringFromTableInBundle(default_button_key, swca_string_table, swca_get_security_bundle(), nil);
    notification_dictionary[(__bridge NSString *)kCFUserNotificationAlternateButtonTitleKey] = NSLocalizedStringFromTableInBundle(alternate_button_key, swca_string_table, swca_get_security_bundle(), nil);

    notification_dictionary[(__bridge NSString *)kCFUserNotificationLocalizationURLKey] = [swca_get_security_bundle() bundleURL];
    notification_dictionary[(__bridge NSString *)kCFUserNotificationAlertTopMostKey] = [NSNumber numberWithBool:YES];

    // additional keys for remote view controller
    notification_dictionary[(__bridge NSString *)SBUserNotificationDismissOnLock] = [NSNumber numberWithBool:YES];
    notification_dictionary[(__bridge NSString *)SBUserNotificationDontDismissOnUnlock] = [NSNumber numberWithBool:YES];
    notification_dictionary[(__bridge NSString *)SBUserNotificationRemoteServiceBundleIdentifierKey] = @"com.apple.SharedWebCredentialViewService";
    notification_dictionary[(__bridge NSString *)SBUserNotificationRemoteViewControllerClassNameKey] = @"SWCViewController";
    notification_dictionary[(__bridge NSString *)SBUserNotificationAllowedApplicationsKey] = client.client;

    SInt32 err;
    if (!(notification = CFUserNotificationCreate(NULL, 0, 0, &err, (__bridge CFDictionaryRef)notification_dictionary)) ||
        err)
        goto out;
    if (CFUserNotificationReceiveResponse(notification, 0, &response))
        goto out;

    //NSLog(@"Selection: %@, Response: %lu", gActiveItem, (unsigned long)response);
    if (result && response == kCFUserNotificationDefaultResponse) {
        CFRetainSafe(gActiveItem);
        *result = gActiveItem;
    }

out:
    if (alert_sem_held) {
        dispatch_semaphore_signal(select_alert_sem);
    }
    CFReleaseSafe(notification);
    CFReleaseNull(gActiveArray);
    CFReleaseNull(gActiveItem);

    return (result && *result);
}

/*
 * Return a SecTaskRef iff orignal client have the entitlement kSecEntitlementAssociatedDomains
 */

static SecTaskRef
swcaCopyClientFromMessage(xpc_object_t event, CFErrorRef *error)
{
    SecTaskRef clientTask = NULL;
    audit_token_t auditToken = {};

    // fetch audit token for process which called securityd
    size_t length = 0;
    const uint8_t *bytes = xpc_dictionary_get_data(event, kSecXPCKeyClientToken, &length);
    if (length != sizeof(audit_token_t)) {
        SecError(errSecMissingEntitlement, error, CFSTR("swcagent_xpc - wrong length for client id"));
        return NULL;
    }

    memcpy(&auditToken, bytes, sizeof(audit_token_t));

    // identify original client
    clientTask = SecTaskCreateWithAuditToken(kCFAllocatorDefault, auditToken);
    if (clientTask == NULL) {
        pid_t pid = 0;
        audit_token_to_au32(auditToken, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
        SecError(errSecMissingEntitlement, error, CFSTR("can't get entitlement of original client pid %d"), (int)pid);
        return NULL;
    }


    // check for presence of original client's shared credential entitlement
    CFArrayRef domains = SecTaskCopyArrayOfStringsForEntitlement(clientTask, kSecEntitlementAssociatedDomains);
    if (domains == NULL) {
        SecError(errSecMissingEntitlement, error, CFSTR("%@ lacks entitlement %@"),
                 clientTask, kSecEntitlementAssociatedDomains);
        CFReleaseNull(clientTask);
        return NULL;
    } else {
        CFReleaseNull(domains);
    }

    return clientTask;
}


static void swca_xpc_dictionary_handler(const xpc_connection_t connection, xpc_object_t event) {
	xpc_type_t type = xpc_get_type(event);
    __block CFErrorRef error = NULL;
    xpc_object_t xpcError = NULL;
    xpc_object_t replyMessage = NULL;
    SecTaskRef clientTask = NULL;
    Client* client = NULL;
    CFArrayRef accessGroups = NULL;

    secdebug("swcagent_xpc", "entering");
    if (type == XPC_TYPE_DICTIONARY) {
        bool entitlementOK = false;
        replyMessage = xpc_dictionary_create_reply(event);

        uint64_t operation = xpc_dictionary_get_uint64(event, kSecXPCKeyOperation);
        secinfo("swcagent_xpc", "operation: %@ (%" PRIu64 ")", SWCAGetOperationDescription((enum SWCAXPCOperation)operation), operation);


        if (operation == swca_copy_pairs_request_id || operation == swca_set_selection_request_id) {
            /* check entitlement */
            xpc_object_t ent = xpc_connection_copy_entitlement_value(connection, "com.apple.private.associated-domains");
            if (ent)
                entitlementOK = true;

        } else {
            clientTask = swcaCopyClientFromMessage(event, &error);
            if (clientTask) {
                accessGroups = SecTaskCopyAccessGroups(clientTask);
                client = SecTaskCopyClient(clientTask);
                CFReleaseNull(clientTask);
            }

            if (accessGroups && client)
                entitlementOK = true;

        }

        if (entitlementOK) {
            switch (operation)
            {
            case swca_add_request_id:
            {
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                //secdebug("ipc", "swcagent: got swca_add_request_id, query: %@", query);
                if (query) {
                    CFTypeRef result = NULL;
                    // confirm that we can add this item
                    if (swca_confirm_add(query, client, accessGroups, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFRelease(result);
                    }
                    CFRelease(query);
                }
                break;
            }
            case swca_copy_request_id:
            {
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                //secdebug("ipc", "swcagent: got swca_copy_request_id, query: %@", query);
                if (query) {
                    CFTypeRef result = NULL;
                    // confirm that we can copy this item
                    if (swca_confirm_copy(query, client, accessGroups, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFRelease(result);
                    }
                    CFRelease(query);
                }
                break;
            }
            case swca_update_request_id:
            {
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                //secdebug("ipc", "swcagent: got swca_update_request_id, query: %@", query);
                if (query) {
                    CFTypeRef result = NULL;
                    // confirm that we can copy this item
                    if (swca_confirm_update(query, client, accessGroups, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFRelease(result);
                    }
                    CFRelease(query);
                }
                break;
            }
            case swca_delete_request_id:
            {
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                //secdebug("ipc", "swcagent: got swca_delete_request_id, query: %@", query);
                if (query) {
                    CFTypeRef result = NULL;
                    // confirm that we can copy this item
                    if (swca_confirm_delete(query, client, accessGroups, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFRelease(result);
                    }
                    CFRelease(query);
                }
                break;
            }
            case swca_select_request_id:
            {
                CFArrayRef items = SecXPCDictionaryCopyArray(event, kSecXPCKeyQuery, &error);
                secdebug("ipc", "swcagent: got swca_select_request_id, items: %@", items);
                if (items) {
                    CFTypeRef result = NULL;
                    // select a dictionary from an input array of dictionaries
                    if (swca_select_item(items, client, accessGroups, &result, &error) && result) {
#if TARGET_OS_IOS
                        LAContext *ctx = [LAContext new];
#if TARGET_OS_VISION
                        if ([ctx canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:nil] &&
#else
                        if ([ctx canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometricsOrCompanion error:nil] &&
#endif
                            [[MCProfileConnection sharedConnection] isAuthenticationBeforeAutoFillRequired]) {
                            NSString *subTitle = NSLocalizedStringFromTableInBundle(@"SWC_FILLPWD", swca_string_table, swca_get_security_bundle(), nil);
                            dispatch_semaphore_t sema = dispatch_semaphore_create(0);
                            [ctx evaluatePolicy:LAPolicyDeviceOwnerAuthentication localizedReason:subTitle reply:^(BOOL success, NSError * _Nullable laError) {
                                if (success || ([laError.domain isEqual:LAErrorDomain] && laError.code == LAErrorPasscodeNotSet)) {
                                    SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                                }
                                dispatch_semaphore_signal(sema);
                            }];
                            dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
                        } else {
#endif
                            SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
#if TARGET_OS_IOS
                        }
#endif
                        CFRelease(result);
                    }
                    CFRelease(items);
                }
                break;
            }
            case swca_copy_pairs_request_id:
            {
                secdebug("ipc", "swcagent: got swca_copy_pairs_request_id");
                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, gActiveArray, &error);
                break;
            }
            case swca_set_selection_request_id:
            {
                CFDictionaryRef dict = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                secdebug("ipc", "swcagent: got swca_set_selection_request_id, dict: %@", dict);
                if (dict) {
                    int32_t value = (int32_t) 1;
                    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
                    SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, number, &error);
                    CFReleaseSafe(number);
                }
                CFReleaseSafe(gActiveItem);
                gActiveItem = dict;
                break;
            }
            case swca_enabled_request_id:
            {
                // return Safari's password autofill enabled status
                CFTypeRef result = (SWCAIsAutofillEnabled()) ? kCFBooleanTrue : kCFBooleanFalse;
                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                break;
            }
            default:
                secdebug("ipc", "swcagent: got unsupported request id (%ld)", (long)operation);
                break;
            }
        }
        if (error)
        {
            if(SecErrorGetOSStatus(error) == errSecItemNotFound)
                secdebug("ipc", "%@ %@ %@", clientTask, SWCAGetOperationDescription((enum SWCAXPCOperation)operation), error);
            else
                secerror("%@ %@ %@", clientTask, SWCAGetOperationDescription((enum SWCAXPCOperation)operation), error);

            xpcError = SecCreateXPCObjectWithCFError(error);
            xpc_dictionary_set_value(replyMessage, kSecXPCKeyError, xpcError);
        } else if (replyMessage) {
            secdebug("ipc", "%@ %@ responding %@", clientTask, SWCAGetOperationDescription((enum SWCAXPCOperation)operation), replyMessage);
        }
    } else {
        SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, &error, 0, CFSTR("Messages expect to be xpc dictionary, got: %@"), event);
        secerror("%@: returning error: %@", clientTask, error);
        xpcError = SecCreateXPCObjectWithCFError(error);
        replyMessage = xpc_create_reply_with_format(event, "{%string: %value}", kSecXPCKeyError, xpcError);
    }

    if (replyMessage) {
        xpc_connection_send_message(connection, replyMessage);
    }
    CFReleaseSafe(error);
    CFReleaseSafe(accessGroups);
}

static xpc_connection_t swclistener = NULL;

static void swca_xpc_init(void)
{
    secdebug("swcagent_xpc", "start");

    swclistener = xpc_connection_create_mach_service(kSWCAXPCServiceName, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!swclistener) {
        seccritical("swcagent failed to register xpc listener, exiting");
        abort();
    }

    xpc_connection_set_event_handler(swclistener, ^(xpc_object_t connection) {
        if (xpc_get_type(connection) == XPC_TYPE_CONNECTION) {
            audit_token_t auditToken = {};
            SecTaskRef clientTask = NULL;

            /*
             * check that our caller have the private entitlement to invoke swcagent
             */

            xpc_connection_get_audit_token(connection, &auditToken);
            clientTask = SecTaskCreateWithAuditToken(kCFAllocatorDefault, auditToken);
            if (clientTask == NULL) {
                secerror("can't get sectask of connection %@", connection);
                xpc_connection_cancel(connection);
                return;
            }

            if (!SecTaskGetBooleanValueForEntitlement(clientTask, kSecEntitlementPrivateAssociatedDomains)) {
                secerror("client %@ lacks entitlement %@", clientTask, kSecEntitlementPrivateAssociatedDomains);
                CFReleaseNull(clientTask);
                xpc_connection_cancel(connection);
                return;
            }
            CFReleaseNull(clientTask);

            xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
                if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
                    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                        swca_xpc_dictionary_handler(connection, event);
                    });
                }
            });
            xpc_connection_resume(connection);
        }
    });
    xpc_connection_resume(swclistener);
}

int main(int argc, char *argv[])
{
    @autoreleasepool {
        swca_xpc_init();
        dispatch_main();
    }
}

/* vi:set ts=4 sw=4 et: */
