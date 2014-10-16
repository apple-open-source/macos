/*
 * Copyright (c) 2007-2009,2012-2014 Apple Inc. All Rights Reserved.
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
#else
                                   CFSTR("sync"),
#endif
                                   CFSTR("com.apple.security.sos"),
                                   NULL);
}

CFArrayRef SecAccessGroupsGetCurrent(void) {
    static CFArrayRef gSecServerAccessGroups;
    static dispatch_once_t only_do_this_once;
    dispatch_once(&only_do_this_once, ^{
        gSecServerAccessGroups = SecServerCopyAccessGroups();
        assert(gSecServerAccessGroups);
    });
    return gSecServerAccessGroups;
}

static xpc_connection_t securityd_create_connection(const char *name) {
    if (!name)
        name = kSecuritydXPCServiceName;
    xpc_connection_t connection;
    connection = xpc_connection_create_mach_service(name, NULL, 0);
    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
        const char *description = xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION);
        secnotice("xpc", "got event: %s", description);
    });
    xpc_connection_resume(connection);
    return connection;
}

static xpc_connection_t sSecuritydConnection;

static xpc_connection_t securityd_connection(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        sSecuritydConnection = securityd_create_connection(NULL);
    });
    return sSecuritydConnection;
}

// NOTE: This is not thread safe, but this SPI is for testing only.
void SecServerSetMachServiceName(const char *name) {
    // Make sure sSecXPCServer.queue exists.
    securityd_connection();

    xpc_connection_t oldConection = sSecuritydConnection;
    sSecuritydConnection = securityd_create_connection(name);
    if (oldConection)
        xpc_release(oldConection);
}

xpc_object_t
securityd_message_with_reply_sync(xpc_object_t message, CFErrorRef *error)
{
    xpc_object_t reply = NULL;
    xpc_connection_t connection = securityd_connection();

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
            seccritical("Failed to talk to secd after %d attempts.", max_tries);
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


/* vi:set ts=4 sw=4 et: */
