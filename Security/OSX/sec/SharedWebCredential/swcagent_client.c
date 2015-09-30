/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecBasePriv.h>
#include <Security/SecInternal.h>
#include <Security/SecuritydXPC.h>

#include <utilities/debugging.h>
#include <utilities/SecCFError.h>
#include <utilities/SecXPCError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecDb.h> // TODO Fixme this gets us SecError().
#include "swcagent_client.h"

#include <xpc/private.h>


CFStringRef sSWCAXPCErrorDomain = CFSTR("com.apple.security.swcagent");
CFStringRef sSWCASecAttrServer = CFSTR("srvr");

//
// MARK: XPC IPC.
//

static xpc_connection_t swca_create_connection(const char *name) {
    if (!name)
        name = kSWCAXPCServiceName;
    xpc_connection_t connection;
    connection = xpc_connection_create_mach_service(name, NULL, 0);
    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
        const char *description = xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION);
        secnotice("xpc", "got event: %s", description);
    });
    xpc_connection_resume(connection);
    return connection;
}

static xpc_connection_t sSWCAConnection;

static xpc_connection_t swca_connection(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        sSWCAConnection = swca_create_connection(NULL);
    });
    return sSWCAConnection;
}

xpc_object_t
swca_message_with_reply_sync(xpc_object_t message, CFErrorRef *error)
{
    xpc_object_t reply = NULL;
    xpc_connection_t connection = swca_connection();

    const int max_tries = 4; // Per <rdar://problem/17829836> N61/12A342: Audio Playback... for why this needs to be at least 3, so we made it 4.

    unsigned int tries_left = max_tries;
    do {
        if (reply) xpc_release(reply);
        reply = xpc_connection_send_message_with_reply_sync(connection, message);
    } while (reply == XPC_ERROR_CONNECTION_INTERRUPTED && --tries_left > 0);

    if (xpc_get_type(reply) == XPC_TYPE_ERROR) {
        CFIndex code =  0;
        if (reply == XPC_ERROR_CONNECTION_INTERRUPTED || reply == XPC_ERROR_CONNECTION_INVALID)
            code = kSecXPCErrorConnectionFailed;
        else if (reply == XPC_ERROR_TERMINATION_IMMINENT)
            code = kSecXPCErrorUnknown;
        else
            code = kSecXPCErrorUnknown;

        char *conn_desc = xpc_copy_description(connection);
        const char *description = xpc_dictionary_get_string(reply, XPC_ERROR_KEY_DESCRIPTION);
        SecCFCreateErrorWithFormat(code, sSWCAXPCErrorDomain, NULL, error, NULL, CFSTR("%s: %s"), conn_desc, description);
        free(conn_desc);
        xpc_release(reply);
        reply = NULL;
    }

    return reply;
}

xpc_object_t swca_create_message(enum SWCAXPCOperation op, CFErrorRef* error)
{
    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    if (message) {
        xpc_dictionary_set_uint64(message, kSecXPCKeyOperation, op);
    } else {
        SecCFCreateError(kSecXPCErrorConnectionFailed, sSWCAXPCErrorDomain,
                         CFSTR("xpc_dictionary_create returned NULL"), NULL, error);
    }
    return message;
}

// Return true if there is no error in message, return false and set *error if there is.
bool swca_message_no_error(xpc_object_t message, CFErrorRef *error) {
    xpc_object_t xpc_error = xpc_dictionary_get_value(message, kSecXPCKeyError);
    if (xpc_error == NULL)
        return true;

    if (error) {
        *error = SecCreateCFErrorWithXPCObject(xpc_error);
    }
    return false;
}

// Return int value of message reply (or -1 if error)
long swca_message_response(xpc_object_t replyMessage, CFErrorRef *error) {
    int32_t value = -1;
    CFTypeRef result = NULL;
    if (!swca_message_no_error(replyMessage, error) ||
        !SecXPCDictionaryCopyPListOptional(replyMessage, kSecXPCKeyResult, &result, error) ||
        !result) {
        return value;
    }
    CFTypeID typeID = CFGetTypeID(result);
    if (typeID == CFBooleanGetTypeID()) {
        value = (CFEqual((CFBooleanRef)result, kCFBooleanTrue)) ? 1 : 0;
    }
    else if (typeID == CFNumberGetTypeID()) {
        if (!CFNumberGetValue((CFNumberRef)result, kCFNumberSInt32Type, &value)) {
            value = -1;
        }
    }
    CFReleaseSafe(result);
    return value;
}

bool swca_autofill_enabled(const audit_token_t *auditToken)
{
    bool result = false;
    CFErrorRef error = NULL;
    xpc_object_t message = swca_create_message(swca_enabled_request_id, &error);
    if (message) {
        xpc_dictionary_set_data(message, kSecXPCKeyClientToken, auditToken, sizeof(audit_token_t));
        xpc_object_t reply = swca_message_with_reply_sync(message, &error);
        if (reply) {
            long value = swca_message_response(reply, &error);
            result = (value > 0);
            xpc_release(reply);
        }
        xpc_release(message);
    }
    CFReleaseSafe(error);
    return result;
}

bool swca_confirm_operation(enum SWCAXPCOperation op,
                            const audit_token_t *auditToken,
                            CFTypeRef query,
                            CFErrorRef *error,
                            void (^add_negative_entry)(CFStringRef fqdn))
{
    bool result = false;
    xpc_object_t message = swca_create_message(op, error);
    if (message) {
        xpc_dictionary_set_data(message, kSecXPCKeyClientToken, auditToken, sizeof(audit_token_t));
        if (SecXPCDictionarySetPList(message, kSecXPCKeyQuery, query, error)) {
            xpc_object_t reply = swca_message_with_reply_sync(message, error);
            if (reply) {
                long value = swca_message_response(reply, error);
                //
                // possible values (see CFUserNotification.h):
                // unable to get message response = -1
                // kCFUserNotificationDefaultResponse   = 0  "Don't Allow" (i.e. permanently)
                // kCFUserNotificationAlternateResponse = 1  "OK"
                // kCFUserNotificationOtherResponse     = 2  (no longer used)
                //
                result = (value == 1);
                if (value == 0 && add_negative_entry) {
                    CFStringRef fqdn = CFDictionaryGetValue(query, sSWCASecAttrServer);
                    add_negative_entry(fqdn);
                }
                xpc_release(reply);
            }
        }
        xpc_release(message);
    }
    return result;
}

// Return retained dictionary value of message reply (or NULL if error)
CFTypeRef swca_message_copy_response(xpc_object_t replyMessage, CFErrorRef *error) {
    CFTypeRef result = NULL;
    if (!swca_message_no_error(replyMessage, error) ||
        !SecXPCDictionaryCopyPListOptional(replyMessage, kSecXPCKeyResult, &result, error)) {
        CFReleaseNull(result);
    }
    return result;
}

CFDictionaryRef swca_copy_selected_dictionary(enum SWCAXPCOperation op,
                                              const audit_token_t *auditToken,
                                              CFTypeRef items, // array of dictionaries
                                              CFErrorRef *error)
{
    CFDictionaryRef result = NULL;
    xpc_object_t message = swca_create_message(op, error);
    if (message) {
        xpc_dictionary_set_data(message, kSecXPCKeyClientToken, auditToken, sizeof(audit_token_t));
        if (SecXPCDictionarySetPList(message, kSecXPCKeyQuery, items, error)) {
            xpc_object_t reply = swca_message_with_reply_sync(message, error);
            if (reply) {
                result = (CFDictionaryRef) swca_message_copy_response(reply, error);
                if (!(result && CFGetTypeID(result) == CFDictionaryGetTypeID())) {
                    CFReleaseNull(result);
                }
                xpc_release(reply);
            }
        }
        xpc_release(message);
    }
    return result;
}

// Return retained array value of message reply (or NULL if error)
CFArrayRef swca_copy_pairs(enum SWCAXPCOperation op,
                           const audit_token_t *auditToken,
                           CFErrorRef *error)
{
    CFArrayRef result = NULL;
    xpc_object_t message = swca_create_message(op, error);
    if (message) {
        xpc_dictionary_set_data(message, kSecXPCKeyClientToken, auditToken, sizeof(audit_token_t));
        xpc_object_t reply = swca_message_with_reply_sync(message, error);
        if (reply) {
            result = (CFArrayRef) swca_message_copy_response(reply, error);
            if (!(result && CFGetTypeID(result) == CFArrayGetTypeID())) {
                CFReleaseNull(result);
            }
            xpc_release(reply);
        }
        xpc_release(message);
    }
    return result;
}

bool swca_set_selection(enum SWCAXPCOperation op,
                        const audit_token_t *auditToken,
                        CFTypeRef dictionary,
                        CFErrorRef *error)
{
    bool result = false;
    xpc_object_t message = swca_create_message(op, error);
    if (message) {
        xpc_dictionary_set_data(message, kSecXPCKeyClientToken, auditToken, sizeof(audit_token_t));
        if (SecXPCDictionarySetPList(message, kSecXPCKeyQuery, dictionary, error)) {
            xpc_object_t reply = swca_message_with_reply_sync(message, error);
            if (reply) {
                long value = swca_message_response(reply, error);
                if (value != 0) {
                    result = true;
                };
                xpc_release(reply);
            }
        }
        xpc_release(message);
    }
    return result;
}

bool swca_send_sync_and_do(enum SWCAXPCOperation op, CFErrorRef *error,
                                bool (^add_to_message)(xpc_object_t message, CFErrorRef* error),
                                bool (^handle_response)(xpc_object_t response, CFErrorRef* error)) {
    xpc_object_t message = swca_create_message(op, error);
    bool ok = false;
    if (message) {
        if (!add_to_message || add_to_message(message, error)) {
            xpc_object_t response = swca_message_with_reply_sync(message, error);
            if (response) {
                if (swca_message_no_error(response, error)) {
                    ok = (!handle_response || handle_response(response, error));
                }
                xpc_release(response);
            }
        }
        xpc_release(message);
    }

    return ok;
}


/* vi:set ts=4 sw=4 et: */
