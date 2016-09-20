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
#include <exception>
#include <dispatch/dispatch.h>

#include "SecTranslocateShared.hpp"
#include "SecTranslocateServer.hpp"
#include "SecTranslocateUtilities.hpp"
#include "SecTranslocateDANotification.hpp"
#include "SecTranslocateXPCServer.hpp"
#include "SecTranslocateLSNotification.hpp"
#undef check //The CoreServices code pulls in a check macro that we don't want

#include <security_utilities/unix++.h>
#include <security_utilities/logging.h>

namespace Security {
    
using namespace Security::UnixPlusPlus;

namespace SecTranslocate {

using namespace std;

/* Try to cleanup every 12 hrs */
#define TRANSLOCATION_CLEANUP_INTERVAL 12ULL * 60ULL * 60ULL * NSEC_PER_SEC
#define TRANSLOCATION_CLEANUP_LEEWAY TRANSLOCATION_CLEANUP_INTERVAL/2ULL

/* Initialize a dispatch queue to serialize operations */
TranslocatorServer::TranslocatorServer(dispatch_queue_t q):syncQ(q), da(q), ls(q),xpc(q)
{
    if (!q)
    {
        Syslog::critical("SecTranslocate: TranslocatorServer failed to create the dispatch queue");
        UnixError::throwMe(ENOMEM);
    }
    dispatch_retain(syncQ);

    setupPeriodicCleanup();

    Syslog::warning("SecTranslocate: Server started");
}

/* Destroy the dispatch queue and listeners when they are no longer needed */
TranslocatorServer::~TranslocatorServer()
{
    if( syncQ )
    {
        dispatch_release(syncQ);
    }

    if(cleanupTimer)
    {
        dispatch_source_cancel(cleanupTimer);
        cleanupTimer = NULL;
    }
}

// This is intended for use by the host process of the server if necessary
// Create a translocation for original path if appropriate
string TranslocatorServer::translocatePathForUser(const TranslocationPath &originalPath, const string &destPath)
{
    __block string newPath;
    __block exception_ptr exception(0);
    
    dispatch_sync(syncQ, ^{
        try
        {
            newPath = Security::SecTranslocate::translocatePathForUser(originalPath,destPath);
        }
        catch (...)
        {
            exception = current_exception();
        }
    });
    if (exception)
    {
        rethrow_exception(exception);
    }
    return newPath;
}

// This is intended for use by the host process of the server if necessary
// Destroy the translocation mount at translocatedPath if allowed
bool TranslocatorServer::destroyTranslocatedPathForUser(const string &translocatedPath)
{
    __block bool result = false;
    __block exception_ptr exception(0);
    dispatch_sync(syncQ, ^{
        try
        {
            result = Security::SecTranslocate::destroyTranslocatedPathForUser(translocatedPath);
        }
        catch (...)
        {
            exception = current_exception();
        }
    });
    if (exception)
    {
        rethrow_exception(exception);
    }
    return result;
}

void TranslocatorServer::appLaunchCheckin(pid_t pid)
{
    //This is thrown on the queue as an async task in the call so don't need to do anything extra.
    ls.checkIn(pid);
}

void TranslocatorServer::setupPeriodicCleanup()
{
    cleanupTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, syncQ);

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, TRANSLOCATION_CLEANUP_INTERVAL);
    dispatch_source_set_timer(cleanupTimer, when, TRANSLOCATION_CLEANUP_INTERVAL, TRANSLOCATION_CLEANUP_LEEWAY);

    dispatch_source_set_cancel_handler(cleanupTimer, ^{
        dispatch_release(cleanupTimer);
    });

    dispatch_source_set_event_handler(cleanupTimer, ^{
        try
        {
            Syslog::notice("SecTranslocate: attempting to cleanup unused translocation points");
            tryToDestroyUnusedTranslocationMounts();
        }
        catch (Security::UnixError err)
        {
            int error = err.unixError();
            Syslog::error("SecTranslocate: got unix error[ %d : %s ] while trying to cleanup translocation points.",error, strerror(error));
        }
        catch (...)
        {
            Syslog::error("SecTranslocate: unknown error while trying to cleanup translocation points.");
        }
    });

    dispatch_resume(cleanupTimer);
}

} //namespace SecTranslocate
} //namespace SecTranslocate
