/*
 * Copyright (c) 2007-2009,2012-2015 Apple Inc. All Rights Reserved.
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

#include <stdbool.h>
#include <sys/queue.h>
#include <syslog.h>
#include <vproc_priv.h>

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
#include <ipc/securityd_client.h>

struct securityd *gSecurityd;

//
// MARK: XPC IPC.
//

/* Hardcoded Access Groups for the server itself */
static CFArrayRef SecServerCopyAccessGroups(void) {
    return CFArrayCreateForCFTypes(kCFAllocatorDefault,
#if NO_SERVER
                                   CFSTR("test"),
                                   CFSTR("apple"),
                                   CFSTR("lockdown-identities"),
                                   CFSTR("123456.test.group"),
                                   CFSTR("123456.test.group2"),
#else
                                   CFSTR("sync"),
#endif
                                   CFSTR("com.apple.security.sos"),
                                   CFSTR("com.apple.sbd"),
                                   CFSTR("com.apple.lakitu"),
                                   kSecAttrAccessGroupToken,
                                   NULL);
}

static SecurityClient gClient;

#if TARGET_OS_IOS
void
SecSecuritySetMusrMode(bool mode, uid_t uid, int activeUser)
{
    gClient.inMultiUser = mode;
    gClient.uid = uid;
    gClient.activeUser = activeUser;
}
#endif

SecurityClient *
SecSecurityClientGet(void)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        gClient.task = NULL,
        gClient.accessGroups = SecServerCopyAccessGroups();
        gClient.allowSystemKeychain = true;
        gClient.allowSyncBubbleKeychain = true;
        gClient.isNetworkExtension = false;
#if TARGET_OS_IPHONE
        gClient.inMultiUser = false;
        gClient.activeUser = 501;
#endif
    });
    return &gClient;
}

CFArrayRef SecAccessGroupsGetCurrent(void) {
    SecurityClient *client = SecSecurityClientGet();
    assert(client && client->accessGroups);
    return client->accessGroups;
}

// Only for testing.
void SecAccessGroupsSetCurrent(CFArrayRef accessGroups);
void SecAccessGroupsSetCurrent(CFArrayRef accessGroups) {
    // Not thread safe at all, but OK because it is meant to be used only by tests.
    gClient.accessGroups = accessGroups;
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

static xpc_connection_t securityd_create_connection(const char *name, uint64_t flags) {
    const char *serviceName = name;
    if (!serviceName) {
        serviceName = securityd_service_name();
    }
    xpc_connection_t connection;
    connection = xpc_connection_create_mach_service(serviceName, NULL, flags);
    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
        const char *description = xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION);
        secnotice("xpc", "got event: %s", description);
    });
    xpc_connection_resume(connection);
    return connection;
}

static xpc_connection_t sSecuritydConnection;
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
static xpc_connection_t sTrustdConnection;
#endif

static xpc_connection_t securityd_connection(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        sSecuritydConnection = securityd_create_connection(kSecuritydXPCServiceName, 0);
    });
    return sSecuritydConnection;
}

static xpc_connection_t trustd_connection(void) {
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
	static dispatch_once_t once;
	dispatch_once(&once, ^{
        bool sysCtx = securityd_in_system_context();
        uint64_t flags = (sysCtx) ? XPC_CONNECTION_MACH_SERVICE_PRIVILEGED : 0;
        const char *serviceName = (sysCtx) ? kTrustdXPCServiceName : kTrustdAgentXPCServiceName;
		sTrustdConnection = securityd_create_connection(serviceName, flags);
	});
	return sTrustdConnection;
#else
	// on iOS all operations are still handled by securityd
	return securityd_connection();
#endif
}

static bool is_trust_operation(enum SecXPCOperation op) {
	switch (op) {
		case sec_trust_store_contains_id:
		case sec_trust_store_set_trust_settings_id:
		case sec_trust_store_remove_certificate_id:
		case sec_trust_evaluate_id:
		case sec_trust_store_copy_all_id:
		case sec_trust_store_copy_usage_constraints_id:
		case sec_device_is_internal_id:
			return true;
		default:
			break;
	}
	return false;
}

static xpc_connection_t securityd_connection_for_operation(enum SecXPCOperation op) {
	bool isTrustOp = is_trust_operation(op);
	#if SECTRUST_VERBOSE_DEBUG
    {
        bool sysCtx = securityd_in_system_context();
        const char *serviceName = (sysCtx) ? kTrustdXPCServiceName : kTrustdAgentXPCServiceName;
        syslog(LOG_ERR, "Will connect to: %s (op=%d)",
               (isTrustOp) ? serviceName : kSecuritydXPCServiceName, (int)op);
    }
	#endif
	return (isTrustOp) ? trustd_connection() : securityd_connection();
}

// NOTE: This is not thread safe, but this SPI is for testing only.
void SecServerSetMachServiceName(const char *name) {
    // Make sure sSecXPCServer.queue exists.
    securityd_connection();

    xpc_connection_t oldConection = sSecuritydConnection;
    sSecuritydConnection = securityd_create_connection(name, 0);
    if (oldConection)
        xpc_release(oldConection);
}

xpc_object_t
securityd_message_with_reply_sync(xpc_object_t message, CFErrorRef *error)
{
    xpc_object_t reply = NULL;
    uint64_t operation = xpc_dictionary_get_uint64(message, kSecXPCKeyOperation);
    xpc_connection_t connection = securityd_connection_for_operation((enum SecXPCOperation)operation);

    const int max_tries = 4; // Per <rdar://problem/17829836> N61/12A342: Audio Playback... for why this needs to be at least 3, so we made it 4.

    unsigned int tries_left = max_tries;
    do {
        if (reply) xpc_release(reply);
        reply = xpc_connection_send_message_with_reply_sync(connection, message);
    } while (reply == XPC_ERROR_CONNECTION_INTERRUPTED && --tries_left > 0);

    if (xpc_get_type(reply) == XPC_TYPE_ERROR) {
        CFIndex code =  0;
        if (reply == XPC_ERROR_CONNECTION_INTERRUPTED || reply == XPC_ERROR_CONNECTION_INVALID) {
            code = kSecXPCErrorConnectionFailed;
#if TARGET_OS_IPHONE
            seccritical("Failed to talk to %s after %d attempts.", "securityd",
                        max_tries);
#else
            seccritical("Failed to talk to %s after %d attempts.",
                (is_trust_operation((enum SecXPCOperation)operation)) ? "trustd" : "secd",
                max_tries);
#endif
        } else if (reply == XPC_ERROR_TERMINATION_IMMINENT)
            code = kSecXPCErrorUnknown;
        else
            code = kSecXPCErrorUnknown;

        char *conn_desc = xpc_copy_description(connection);
        const char *description = xpc_dictionary_get_string(reply, XPC_ERROR_KEY_DESCRIPTION);
        SecCFCreateErrorWithFormat(code, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("%s: %s"), conn_desc, description);
        free(conn_desc);
        xpc_release(reply);
        reply = NULL;
    }

    return reply;
}

xpc_object_t securityd_create_message(enum SecXPCOperation op, CFErrorRef* error)
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
    xpc_object_t xpc_error = xpc_dictionary_get_value(message, kSecXPCKeyError);
    if (xpc_error == NULL)
        return true;

    if (error) {
        *error = SecCreateCFErrorWithXPCObject(xpc_error);
    }
    return false;
}

bool securityd_send_sync_and_do(enum SecXPCOperation op, CFErrorRef *error,
                                bool (^add_to_message)(xpc_object_t message, CFErrorRef* error),
                                bool (^handle_response)(xpc_object_t response, CFErrorRef* error)) {
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
            securityd_message_no_error(response, error);
        }
        xpc_release(message);
    }
    return reply;
}

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



/* vi:set ts=4 sw=4 et: */
