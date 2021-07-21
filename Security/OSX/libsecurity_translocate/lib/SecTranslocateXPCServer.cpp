/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#include <string>
#include <vector>

#include <dispatch/dispatch.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <sandbox.h>

#include <security_utilities/unix++.h>
#include <security_utilities/debugging.h>

#include "SecTranslocateInterface.hpp"
#include "SecTranslocateXPCServer.hpp"
#include "SecTranslocateUtilities.hpp"
#include "SecTranslocateShared.hpp"
#include "SecTranslocateEnumUtils.hpp"

namespace Security {
namespace SecTranslocate {

static void doCreate(xpc_object_t msg, xpc_object_t reply, audit_token_t audit_token)
{
    const int original = xpc_dictionary_dup_fd(msg, kSecTranslocateXPCMessageOriginalPath);
    const int dest = xpc_dictionary_dup_fd(msg, kSecTranslocateXPCMessageDestinationPath);
    const int64_t opts = xpc_dictionary_get_int64(msg, kSecTranslocateXPCMessageOptions);

    TranslocationOptions options = static_cast<TranslocationOptions>(opts);

    if (original == -1) {
        secerror("SecTranslocate: XPCServer, doCreate no path to translocate");
        UnixError::throwMe(EINVAL);
    }
    ExtendedAutoFileDesc destFd(dest);
    int rc = sandbox_check_by_audit_token(audit_token, "file-read*", SANDBOX_FILTER_DESCRIPTOR, original);
    if (rc == 1) {
        secerror("SecTranslocate: XPCServer, doCreate path to translocate disallowed by sandbox");
        UnixError::throwMe(EPERM);
    } else if (rc == -1) {
        int error = errno;
        secerror("SecTranslocate: XPCServer, doCreate error checking path to translocate against sandbox");
        UnixError::throwMe(error);
    }
    if (destFd.isOpen()) {
        rc = sandbox_check_by_audit_token(audit_token, "file-mount", SANDBOX_FILTER_DESCRIPTOR, destFd.fd());
        if (rc == 1) {
            secerror("SecTranslocate: XPCServer, doCreate destination path disallowed by sandbox");
            UnixError::throwMe(EPERM);
        } else if (rc == -1) {
            int error = errno;
            secerror("SecTranslocate: XPCServer, doCreate error checking destination path against sandbox");
            UnixError::throwMe(error);
        }
    }

    string result;
    
    if ((options & TranslocationOptions::Generic) == TranslocationOptions::Generic) {
        GenericTranslocationPath tPath(original, TranslocationOptions::Unveil);
        result = tPath.getOriginalRealPath();

        if (tPath.shouldTranslocate()) {
            result = Security::SecTranslocate::translocatePathForUser(tPath, destFd);
        }
    } else {
        TranslocationPath tPath(original, TranslocationOptions::Default);
        result = tPath.getOriginalRealPath();

        if (tPath.shouldTranslocate()) {
            result = Security::SecTranslocate::translocatePathForUser(tPath, destFd);
        }
    }
    
    xpc_dictionary_set_string(reply, kSecTranslocateXPCReplySecurePath, result.c_str());
}

static void doCheckIn(xpc_object_t msg)
{
    if (xpc_dictionary_get_value(msg, kSecTranslocateXPCMessagePid) == NULL) {
        secerror("SecTranslocate, XpcServer, doCheckin, no pid provided");
        UnixError::throwMe(EINVAL);
    }
    int64_t pid = xpc_dictionary_get_int64(msg, kSecTranslocateXPCMessagePid);
    Translocator * t = getTranslocator();
    if (t) {
        t->appLaunchCheckin((pid_t)pid);
    } else {
        seccritical("SecTranslocate, XpcServer, doCheckin, No top level translocator");
        UnixError::throwMe(EINVAL);
    }
}

XPCServer::XPCServer(dispatch_queue_t q):notificationQ(q)
{
    if (q == NULL) {
        seccritical("SecTranslocate: XPCServer, no dispatch queue provided");
        UnixError::throwMe(EINVAL);
    }
    //notificationQ is assumed to be serial
    service = xpc_connection_create_mach_service(SECTRANSLOCATE_XPC_SERVICE_NAME,
                                                 notificationQ,
                                                 XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (service == NULL) {
        seccritical("SecTranslocate: XPCServer, failed to create xpc mach service");
        UnixError::throwMe(ENOMEM);
    }

    dispatch_retain(notificationQ);
    xpc_connection_set_event_handler(service, ^(xpc_object_t cmsg) {
        if (xpc_get_type(cmsg) == XPC_TYPE_CONNECTION) {
            xpc_connection_t connection = xpc_connection_t(cmsg);
            secdebug("sectranslocate","SecTranslocate: XPCServer, Connection from pid %d", xpc_connection_get_pid(connection));
            xpc_connection_set_event_handler(connection, ^(xpc_object_t msg) {
                if (xpc_get_type(msg) == XPC_TYPE_DICTIONARY) {
                    xpc_retain(msg);
                    dispatch_async(notificationQ, ^{	// async from here
                        const char *function = xpc_dictionary_get_string(msg, kSecTranslocateXPCMessageFunction);
                        audit_token_t audit_token;
                        xpc_connection_get_audit_token(connection, &audit_token);
                        secdebug("sectranslocate","SecTranslocate: XPCServer, pid %d requested %s", xpc_connection_get_pid(connection), function);
                        xpc_object_t reply = xpc_dictionary_create_reply(msg);
                        try {
                            if (function == NULL) {
                                xpc_dictionary_set_int64(reply, kSecTranslocateXPCReplyError, EINVAL);
                            } else if (!strcmp(function, kSecTranslocateXPCFuncCreate)) {
                                doCreate(msg, reply, audit_token);
                            } else if (!strcmp(function, kSecTranslocateXPCFuncCheckIn)) {
                                doCheckIn(msg);
                            } else {
                                xpc_dictionary_set_int64(reply, kSecTranslocateXPCReplyError, EINVAL);
                            }
                        } catch (Security::UnixError err) {
                            xpc_dictionary_set_int64(reply, kSecTranslocateXPCReplyError, err.unixError());
                        } catch (...) {
                            xpc_dictionary_set_int64(reply, kSecTranslocateXPCReplyError, EINVAL);
                        }
                        xpc_release(msg);
                        if (reply) {
                            xpc_connection_send_message(connection, reply);
                            xpc_release(reply);
                        }
                    });
                }
            });
            xpc_connection_resume(connection);
        } else {
            const char *s = xpc_copy_description(cmsg);
            secerror("SecTranslocate: XPCServer, unexpected incoming message - %s", s);
            free((char*)s);
        }
    });
    xpc_connection_resume(service);
}

XPCServer::~XPCServer()
{
    xpc_connection_cancel(service);
    dispatch_release(notificationQ);
}

} //namespace Security
} //namespace SecTranslocate
