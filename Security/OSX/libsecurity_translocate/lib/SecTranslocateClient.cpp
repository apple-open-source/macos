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

#include <dispatch/dispatch.h>
#include <xpc/xpc.h>
#include <unistd.h>

#include <security_utilities/unix++.h>
#include <security_utilities/logging.h>

#include "SecTranslocateClient.hpp"
#include "SecTranslocateShared.hpp"
#include "SecTranslocateInterface.hpp"

namespace Security {

namespace SecTranslocate {

using namespace std;

TranslocatorClient::TranslocatorClient(dispatch_queue_t q):syncQ(q)
{
    if(syncQ == NULL)
    {
        Syslog::critical("SecTranslocate::TranslocatorClient initialized without a queue.");
        UnixError::throwMe(EINVAL);
    }

    uint64_t flags = 0;
    uid_t euid = geteuid();

    /* 0 - is root so it gets the root lsd
       1-300 = are treated by launch services as "role users" They share a copy of the LS Database with root
               and thus must be sent to the root lsd. */
    if (euid <= 300)
    {
        flags |= XPC_CONNECTION_MACH_SERVICE_PRIVILEGED; //forces call to the root lsd
    }

    service = xpc_connection_create_mach_service(SECTRANSLOCATE_XPC_SERVICE_NAME,
                                                 syncQ,
                                                 flags);
    if (service == NULL)
    {
        Syslog::critical("SecTranslocate: TranslocatorClient, failed to create xpc mach service");
        UnixError::throwMe(ENOMEM);
    }
    xpc_connection_set_event_handler(service, ^(xpc_object_t event) {
        xpc_type_t type = xpc_get_type(event);
        if (type == XPC_TYPE_ERROR)
        {
            Syslog::error("SecTranslocate, client, xpc error: %s", xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
        }
        else
        {
            char* description = xpc_copy_description(event);
            Syslog::error("SecTranslocate, client, xpc unexpected type: %s", description);
            free(description);
        }
    });

    dispatch_retain(syncQ);
    xpc_connection_resume(service);
}

TranslocatorClient::~TranslocatorClient()
{
    xpc_connection_cancel(service);
    dispatch_release(syncQ);
}

string TranslocatorClient::translocatePathForUser(const TranslocationPath &originalPath, const string &destPath)
{
    string outPath;

    if (!originalPath.shouldTranslocate())
    {
        return originalPath.getOriginalRealPath();  //return original path if we shouldn't translocate
    }

    //We should run translocated, so get a translocation point
    xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);

    if( msg == NULL)
    {
        Syslog::error("SecTranslocate: TranslocatorClient, failed to allocate message to send");
        UnixError::throwMe(ENOMEM);
    }

    xpc_dictionary_set_string(msg, kSecTranslocateXPCMessageFunction, kSecTranslocateXPCFuncCreate);
    /* send the original real path rather than the calculated path to let the server do all the work */
    xpc_dictionary_set_string(msg, kSecTranslocateXPCMessageOriginalPath, originalPath.getOriginalRealPath().c_str());
    if(!destPath.empty())
    {
        xpc_dictionary_set_string(msg, kSecTranslocateXPCMessageDestinationPath, destPath.c_str());
    }

    xpc_object_t reply = xpc_connection_send_message_with_reply_sync(service, msg);
    xpc_release(msg);

    if(reply == NULL)
    {
        Syslog::error("SecTranslocate, TranslocatorClient, create, no reply returned");
        UnixError::throwMe(ENOMEM);
    }

    xpc_type_t type = xpc_get_type(reply);
    if (type == XPC_TYPE_DICTIONARY)
    {
        if(int64_t error = xpc_dictionary_get_int64(reply, kSecTranslocateXPCReplyError))
        {
            Syslog::error("SecTranslocate, TranslocatorClient, create, error received %lld", error);
            xpc_release(reply);
            UnixError::throwMe((int)error);
        }
        const char * result = xpc_dictionary_get_string(reply, kSecTranslocateXPCReplySecurePath);
        if (result == NULL)
        {
            Syslog::error("SecTranslocate, TranslocatorClient, create, no result path received");
            xpc_release(reply);
            UnixError::throwMe(EINVAL);
        }
        outPath=result;
        xpc_release(reply);
    }
    else
    {
        const char* errorMsg = NULL;
        if (type == XPC_TYPE_ERROR)
        {
            errorMsg = "SecTranslocate, TranslocatorClient, create, xpc error returned: %s";
        }
        else
        {
            errorMsg = "SecTranslocate, TranslocatorClient, create, unexpected type of return object: %s";
        }
        const char *s = xpc_copy_description(reply);
        Syslog::error(errorMsg, s);
        free((char*)s);
        xpc_release(reply);
        UnixError::throwMe(EINVAL);
    }

    return outPath;
}

void TranslocatorClient::appLaunchCheckin(pid_t pid)
{
    xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);

    xpc_dictionary_set_string(msg, kSecTranslocateXPCMessageFunction, kSecTranslocateXPCFuncCheckIn);
    xpc_dictionary_set_int64(msg, kSecTranslocateXPCMessagePid, pid);

    /* no reply expected so just send the message and move along */
    xpc_connection_send_message(service, msg);

    xpc_release(msg);
}

bool TranslocatorClient::destroyTranslocatedPathForUser(const string &translocatedPath)
{
    Syslog::error("SecTranslocate, TranslocatorClient, delete operation not allowed");
    UnixError::throwMe(EPERM);
}

} //namespace SecTranslocate
} //namespace Security
