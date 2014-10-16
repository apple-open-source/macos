/*
 * Copyright (c) 2009 Apple, Inc.  All Rights Reserved.
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

/*
 *  IOMIGMachPort.c
 *
 *  Created by Rob Yepez on 1/29/09.
 *  Copyright 2008 Apple Inc.. All rights reserved.
 *
 */

#include <AssertMacros.h>
#include <CoreFoundation/CFRuntime.h>

#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <servers/bootstrap.h>
#include "IOMIGMachPort.h"

typedef struct __IOMIGMachPort {
    CFRuntimeBase                       cfBase;   // base CFType information
  
    CFRunLoopRef                        runLoop;
    CFStringRef                         runLoopMode;
    
    dispatch_queue_t                    dispatchQueue;
    dispatch_source_t                   dispatchSource;
    
    CFMachPortRef                       port;
    CFRunLoopSourceRef                  source;
    CFIndex                             maxMessageSize;
    
    IOMIGMachPortDemuxCallback          demuxCallback;
    void *                              demuxRefcon;
    
    IOMIGMachPortTerminationCallback    terminationCallback;
    void *                              terminationRefcon;
    

} __IOMIGMachPort, *__IOMIGMachPortRef;


static void             __IOMIGMachPortRelease(CFTypeRef object);
static void             __IOMIGMachPortRegister(void);
static void             __IOMIGMachPortSourceCallback(void * info);
static void             __IOMIGMachPortPortCallback(CFMachPortRef port, void *msg, CFIndex size __unused, void *info);
static Boolean          __NoMoreSenders(mach_msg_header_t *request, mach_msg_header_t *reply);


static const CFRuntimeClass __IOMIGMachPortClass = {
    0,                          // version
    "IOMIGMachPort",          // className
    NULL,                       // init
    NULL,                       // copy
    __IOMIGMachPortRelease,   // finalize
    NULL,                       // equal
    NULL,                       // hash
    NULL,                       // copyFormattingDesc
    NULL,
    NULL,
    NULL
};

static pthread_once_t           __IOMIGMachPortTypeInit   = PTHREAD_ONCE_INIT;
static CFTypeID                 __IOMIGMachPortTypeID     = _kCFRuntimeNotATypeID;
static CFMutableDictionaryRef   __ioPortCache             = NULL;
static pthread_mutex_t          __ioPortCacheLock         = PTHREAD_MUTEX_INITIALIZER;

//------------------------------------------------------------------------------
// __IOMIGMachPortRegister
//------------------------------------------------------------------------------
void __IOMIGMachPortRegister(void)
{
    __ioPortCache         = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
    __IOMIGMachPortTypeID = _CFRuntimeRegisterClass(&__IOMIGMachPortClass);
}

//------------------------------------------------------------------------------
// __IOMIGMachPortRelease
//------------------------------------------------------------------------------
void __IOMIGMachPortRelease(CFTypeRef object)
{
    IOMIGMachPortRef migPort = (IOMIGMachPortRef)object;
    
    if ( migPort->port ) {
        CFMachPortInvalidate(migPort->port);
        CFRelease(migPort->port);
    }
    
    if ( migPort->source ) {
        CFRelease(migPort->source);
    }
    
    if ( migPort->dispatchSource ) {
        dispatch_release(migPort->dispatchSource);
    }
}

//------------------------------------------------------------------------------
// IOMIGMachPortGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOMIGMachPortGetTypeID(void)
{
    if ( _kCFRuntimeNotATypeID == __IOMIGMachPortTypeID )
        pthread_once(&__IOMIGMachPortTypeInit, __IOMIGMachPortRegister);
    
    return __IOMIGMachPortTypeID;
}

//------------------------------------------------------------------------------
// IOMIGMachPortCreate
//------------------------------------------------------------------------------
IOMIGMachPortRef IOMIGMachPortCreate(CFAllocatorRef allocator, CFIndex maxMessageSize, mach_port_t port)
{
    IOMIGMachPortRef  migPort  = NULL;
    void *              offset  = NULL;
    uint32_t            size;
    
    require(maxMessageSize > 0, exit);
    
    /* allocate service */
    size    = sizeof(__IOMIGMachPort) - sizeof(CFRuntimeBase);
    migPort  = ( IOMIGMachPortRef)_CFRuntimeCreateInstance(allocator, IOMIGMachPortGetTypeID(), size, NULL);
    
    require(migPort, exit);

    offset = migPort;
    bzero(offset + sizeof(CFRuntimeBase), size);
        
    CFMachPortContext context = {0, migPort, NULL, NULL, NULL};
    migPort->port = (port != MACH_PORT_NULL) ? 
            CFMachPortCreateWithPort(allocator, port, __IOMIGMachPortPortCallback, &context, NULL) :
            CFMachPortCreate(allocator, __IOMIGMachPortPortCallback, &context, NULL);
    
    require(migPort->port, exit);
    
    migPort->maxMessageSize = maxMessageSize;
    
    return migPort;

exit:
    if ( migPort )
        CFRelease(migPort);
        
    return NULL;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOMIGMachPortSourceCallback
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOMIGMachPortSourceCallback(void * info)
{
    IOMIGMachPortRef migPort = (IOMIGMachPortRef)info;
    
    CFRetain(migPort);
    
    mach_port_t         port    = CFMachPortGetPort(migPort->port);
    mach_msg_size_t     size    = migPort->maxMessageSize + MAX_TRAILER_SIZE;
    mach_msg_header_t * msg     = (mach_msg_header_t *)CFAllocatorAllocate(CFGetAllocator(migPort), size, 0);
    
    msg->msgh_size = size;
    for (;;) {
        msg->msgh_bits = 0;
        msg->msgh_local_port = port;
        msg->msgh_remote_port = MACH_PORT_NULL;
        msg->msgh_id = 0;
        kern_return_t ret = mach_msg(msg, MACH_RCV_MSG|MACH_RCV_LARGE|MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)|MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AV), 0, msg->msgh_size, port, 0, MACH_PORT_NULL);
        if (MACH_MSG_SUCCESS == ret) break;
        if (MACH_RCV_TOO_LARGE != ret) goto inner_exit;
        uint32_t newSize = round_msg(msg->msgh_size + MAX_TRAILER_SIZE);
        msg = CFAllocatorReallocate(CFGetAllocator(migPort), msg, newSize, 0);
        msg->msgh_size = newSize;
    }

    __IOMIGMachPortPortCallback(migPort->port, msg, msg->msgh_size, migPort);

inner_exit:
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, msg);
    CFRelease(migPort);
}


//------------------------------------------------------------------------------
// IOMIGMachPortScheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOMIGMachPortScheduleWithDispatchQueue(IOMIGMachPortRef migPort, dispatch_queue_t queue)
{
    
    mach_port_t port = CFMachPortGetPort(migPort->port);
    
    migPort->dispatchQueue = queue;
    
    require(migPort->dispatchQueue, exit);
    
    // init the sources
    if ( !migPort->dispatchSource ) {
        migPort->dispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, port, 0, migPort->dispatchQueue);
        require(migPort->dispatchSource, exit);

        dispatch_set_context(migPort->dispatchSource, migPort);
        dispatch_source_set_event_handler_f(migPort->dispatchSource, __IOMIGMachPortSourceCallback);
    }
    
    dispatch_resume(migPort->dispatchSource);
    
exit:
    return;
}

//------------------------------------------------------------------------------
// IOMIGMachPortUnscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOMIGMachPortUnscheduleFromDispatchQueue(IOMIGMachPortRef migPort, dispatch_queue_t queue)
{
    if ( !queue || !migPort->dispatchQueue)
        return;
    
    if ( queue != migPort->dispatchQueue )
        return;
    
    migPort->dispatchQueue = NULL;

    if ( migPort->dispatchSource ) {
        dispatch_release(migPort->dispatchSource);
        migPort->dispatchSource = NULL;
    }
}

//------------------------------------------------------------------------------
// IOMIGMachPortScheduleWithRunLoop
//------------------------------------------------------------------------------
void IOMIGMachPortScheduleWithRunLoop(IOMIGMachPortRef migPort, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{    
    migPort->runLoop        = runLoop;
    migPort->runLoopMode    = runLoopMode;
    
    require(migPort->runLoop, exit);
    require(migPort->runLoopMode, exit);

    // init the sources
    if ( !migPort->source ) {
        migPort->source = CFMachPortCreateRunLoopSource(CFGetAllocator(migPort), migPort->port, 1);
        require(migPort->source, exit);
    }

    CFRunLoopAddSource(runLoop, migPort->source, runLoopMode);
        
exit:
    return;
}

//------------------------------------------------------------------------------
// IOMIGMachPortUnscheduleFromRunLoop
//------------------------------------------------------------------------------
void IOMIGMachPortUnscheduleFromRunLoop(IOMIGMachPortRef migPort, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    if ( !runLoop || !runLoopMode || !migPort->runLoop || !migPort->runLoopMode)
        return;
        
    if ( !CFEqual(runLoop, migPort->runLoop) || !CFEqual(runLoopMode, migPort->runLoopMode) )
        return;
        
    migPort->runLoop     = NULL;
    migPort->runLoopMode = NULL;
    
    if ( migPort->source )
        CFRunLoopRemoveSource(runLoop, migPort->source, runLoopMode);
}

//------------------------------------------------------------------------------
// IOMIGMachPortGetPort
//------------------------------------------------------------------------------
mach_port_t IOMIGMachPortGetPort(IOMIGMachPortRef migPort)
{
    return CFMachPortGetPort(migPort->port);
}

//------------------------------------------------------------------------------
// IOMIGMachPortRegisterDemuxCallback
//------------------------------------------------------------------------------
void IOMIGMachPortRegisterDemuxCallback(IOMIGMachPortRef migPort, IOMIGMachPortDemuxCallback callback, void *refcon)
{
    migPort->demuxCallback = callback;
    migPort->demuxRefcon   = refcon;
}

//------------------------------------------------------------------------------
// IOMIGMachPortRegisterTerminationCallback
//------------------------------------------------------------------------------
void IOMIGMachPortRegisterTerminationCallback(IOMIGMachPortRef migPort, IOMIGMachPortTerminationCallback callback, void *refcon)
{
    migPort->terminationCallback = callback;
    migPort->terminationRefcon   = refcon;
}

//------------------------------------------------------------------------------
// __IOMIGMachPortPortCallback
//------------------------------------------------------------------------------
void __IOMIGMachPortPortCallback(CFMachPortRef port __unused, void *msg, CFIndex size __unused, void *info)
{
    IOMIGMachPortRef  migPort      = (IOMIGMachPortRef)info;
    mig_reply_error_t * bufRequest  = msg;
    mig_reply_error_t * bufReply    = NULL;
    mach_msg_return_t   mr;
    int                 options;

    require(migPort, exit);
    CFRetain(migPort);
    
    bufReply = CFAllocatorAllocate(NULL, migPort->maxMessageSize, 0);
    require(bufReply, exit);
    
    // let's see if we have no more senders
    if ( __NoMoreSenders(&bufRequest->Head, &bufReply->Head) ) {
        if ( migPort->terminationCallback )
            (*migPort->terminationCallback)(migPort, migPort->terminationRefcon);
        else {
            goto exit;
        }
    } else {
        if ( migPort->demuxCallback )
            (*migPort->demuxCallback)(migPort, &bufRequest->Head, &bufReply->Head, migPort->demuxRefcon);
        else {
            mach_msg_destroy(&bufRequest->Head);
            goto exit;
        }
    }

    if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
        (bufReply->RetCode != KERN_SUCCESS)) {        
        
        //This return code is a little tricky -- it appears that the
        //demux routine found an error of some sort, but since that
        //error would not normally get returned either to the local
        //user or the remote one, we pretend it's ok.
        require(bufReply->RetCode != MIG_NO_REPLY, exit);

        // destroy any out-of-line data in the request buffer but don't destroy
        // the reply port right (since we need that to send an error message).

        bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
        mach_msg_destroy(&bufRequest->Head);
    }

    if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
        //no reply port, so destroy the reply
        if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
            mach_msg_destroy(&bufReply->Head);
        }
        
        goto exit;
    }

    // send reply.
    // We don't want to block indefinitely because the migPort
    // isn't receiving messages from the reply port.
    // If we have a send-once right for the reply port, then
    // this isn't a concern because the send won't block.
    // If we have a send right, we need to use MACH_SEND_TIMEOUT.
    // To avoid falling off the kernel's fast RPC path unnecessarily,
    // we only supply MACH_SEND_TIMEOUT when absolutely necessary.

    options = MACH_SEND_MSG;
    if (MACH_MSGH_BITS_REMOTE(bufReply->Head.msgh_bits) != MACH_MSG_TYPE_MOVE_SEND_ONCE) {
       options |= MACH_SEND_TIMEOUT;
    }

    mr = mach_msg(&bufReply->Head,
              options,
              bufReply->Head.msgh_size,
              0,
              MACH_PORT_NULL,
              MACH_MSG_TIMEOUT_NONE,
              MACH_PORT_NULL);


    // Has a message error occurred?
    switch (mr) {
        case MACH_SEND_INVALID_DEST:
        case MACH_SEND_TIMED_OUT:
            // the reply can't be delivered, so destroy it
            mach_msg_destroy(&bufReply->Head);
            break;

        default :
            // Includes success case.
            break;
    }
    
exit:
    if ( bufReply )
        CFAllocatorDeallocate(NULL, bufReply);
        
    if ( migPort )
        CFRelease(migPort);
}

//------------------------------------------------------------------------------
// __NoMoreSenders
//------------------------------------------------------------------------------
Boolean __NoMoreSenders(mach_msg_header_t *request, mach_msg_header_t *reply)
{
	mach_no_senders_notification_t	*Request = (mach_no_senders_notification_t *)request;
	mig_reply_error_t               *Reply   = (mig_reply_error_t *)reply;

	reply->msgh_bits        = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request->msgh_bits), 0);
	reply->msgh_remote_port = request->msgh_remote_port;
	reply->msgh_size        = sizeof(mig_reply_error_t);	// Minimal size: update as needed
	reply->msgh_local_port  = MACH_PORT_NULL;
	reply->msgh_id          = request->msgh_id + 100;

	if ((Request->not_header.msgh_id > MACH_NOTIFY_LAST) ||
	    (Request->not_header.msgh_id < MACH_NOTIFY_FIRST)) {
		Reply->NDR     = NDR_record;
		Reply->RetCode = MIG_BAD_ID;
		return FALSE;	// if this is not a notification message 
	}

	switch (Request->not_header.msgh_id) {
		case MACH_NOTIFY_NO_SENDERS :
			Reply->Head.msgh_bits		= 0;
			Reply->Head.msgh_remote_port	= MACH_PORT_NULL;
			Reply->RetCode			= KERN_SUCCESS;
			return TRUE;
		default :
			break;
	}

	Reply->NDR     = NDR_record;
	Reply->RetCode = MIG_BAD_ID;
	return FALSE;	// if this is not a notification we are handling
}

//------------------------------------------------------------------------------
//IOMIGMachPortCacheAdd
//------------------------------------------------------------------------------
void IOMIGMachPortCacheAdd(mach_port_t port, CFTypeRef server)
{
    pthread_mutex_lock(&__ioPortCacheLock);

    CFDictionarySetValue(__ioPortCache, (void *)port, server);
    
    pthread_mutex_unlock(&__ioPortCacheLock);
}

//------------------------------------------------------------------------------
// IOMIGMachPortCacheRemove
//------------------------------------------------------------------------------
void IOMIGMachPortCacheRemove(mach_port_t port)
{
    pthread_mutex_lock(&__ioPortCacheLock);
    
    CFDictionaryRemoveValue(__ioPortCache, (void *)port);
    
    pthread_mutex_unlock(&__ioPortCacheLock);
}

//------------------------------------------------------------------------------
// IOMIGMachPortCacheCopy
//------------------------------------------------------------------------------
CFTypeRef IOMIGMachPortCacheCopy(mach_port_t port)
{
    CFTypeRef server;
    
    pthread_mutex_lock(&__ioPortCacheLock);
    
    server = (CFTypeRef)CFDictionaryGetValue(__ioPortCache, (void *)port);
    if ( server ) CFRetain(server);
    
    pthread_mutex_unlock(&__ioPortCacheLock);
    
    return server;
}

