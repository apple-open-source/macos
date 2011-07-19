/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
 *  FakeUPSPlugin.cpp
 *  BatteryFaker
 *
 *  Created by Ethan on 4/23/06.
 *
 */

#include <syslog.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOReturn.h>  

#include    "FakeUPSPlugin.h"

extern "C" 
{
    #include <IOKit/IOCFUnserialize.h> 
    #include "fakeupsServer.h" // mig generated
}

#define     kUPSBootstrapServerName     "com.apple.FakeUPS.control"
#define     kSysLogLevel                LOG_ERR

/*******************************************************************************
 *
 * Forward Declarations
 *
 ******************************************************************************/

void initMigServer(void);

void mig_server_callback(
    CFMachPortRef port, 
    void *msg, 
    CFIndex size, 
    void *info);

static boolean_t pm_mig_demux(
    mach_msg_header_t * request,
    mach_msg_header_t * reply);
    

// defined by MiG
extern "C" {
    boolean_t fakeups_server(
        mach_msg_header_t *, 
        mach_msg_header_t *);
}

/*******************************************************************************
 * _fakeups_set_properties
 *
 * mig handling call
 ******************************************************************************/
kern_return_t _fakeups_set_properties
(
	mach_port_t server,
	vm_offset_t properties,
	mach_msg_type_number_t propertiesCnt,
	int *return_val
)
{
    CFTypeRef           unwrapped_data = NULL;
    CFDictionaryRef     *transmitted_properties = NULL;
    CFStringRef         *errorResultString = NULL;
       
    unwrapped_data = IOCFUnserialize(
                        (const char *)properties, kCFAllocatorDefault,
                        0, errorResultString);

    if( unwrapped_data )
    {
        transmitted_properties = (CFDictionaryRef *)unwrapped_data;
    } else {
        *return_val = kIOReturnError;
        goto exit;
    }
    
    CFShow(transmitted_properties);

exit:
    return KERN_SUCCESS;
}

/*******************************************************************************
 * initMigServer
 *
 * Registers bootstrap port for kUPSBootstrapServerName
 * Initializes mig receiving code, so we can receive mig
 * CFDictionaries of properties from BatteryFaker.app
 ******************************************************************************/
void initMigServer(void)
{
#if 0
    kern_return_t           kern_result = 0;
    CFMachPortRef           cf_mach_port = 0;
    CFRunLoopSourceRef      cfmp_rls = 0;
    mach_port_t             our_port;

    cf_mach_port = CFMachPortCreate(0, mig_server_callback, 0, 0);
    if(!cf_mach_port) {
        syslog(kSysLogLevel, "error error creating CFMachPort\n");
        goto bail;
    }
    our_port = CFMachPortGetPort(cf_mach_port);
    cfmp_rls = CFMachPortCreateRunLoopSource(0, cf_mach_port, 0);
    if(!cfmp_rls) {
        syslog(kSysLogLevel, "error error no CFMachPort RunLoopSource!\n");
        goto bail;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), cfmp_rls, kCFRunLoopDefaultMode);

    kern_result = bootstrap_create_service(
                        bootstrap_port, 
                        kUPSBootstrapServerName, 
                        &our_port);
    if (BOOTSTRAP_SUCCESS != kern_result) {
        goto bail;
    }

    kern_result = bootstrap_check_in(
                        bootstrap_port,
                        kUPSBootstrapServerName, 
                        &our_port);
    if (BOOTSTRAP_SUCCESS != kern_result) {
        goto bail;
    }

    switch (kern_result) {
      case BOOTSTRAP_NOT_PRIVILEGED:
        syslog(kSysLogLevel, "UPSFaker exit: BOOTSTRAP_NOT_PRIVILIGED");
        break;
      case BOOTSTRAP_SERVICE_ACTIVE:
        syslog(kSysLogLevel, "UPSFaker exit: BOOTSTRAP_SERVICE_ACTIVE");
        break;
      default:
        syslog(kSysLogLevel, "UPSFaker exit: undefined mig error 0x%08x", kern_result);
        break;
    }

bail:
    if(cfmp_rls) CFRelease(cfmp_rls);
#endif
    return;
}


/*******************************************************************************
 * pm_mig_demux
 *
 ******************************************************************************/
static boolean_t pm_mig_demux(
    mach_msg_header_t * request,
    mach_msg_header_t * reply)
{
    boolean_t processed = FALSE;

    processed = fakeups_server(request, reply);
    if(processed) return true;
        
    // mig request is not in our subsystem range!
    // generate error reply packet
    reply->msgh_bits        = MACH_MSGH_BITS( 
                                MACH_MSGH_BITS_REMOTE(request->msgh_bits), 0);
    reply->msgh_remote_port = request->msgh_remote_port;
    reply->msgh_size        = sizeof(mig_reply_error_t);    /* Minimal size */
    reply->msgh_local_port  = MACH_PORT_NULL;
    reply->msgh_id          = request->msgh_id + 100;
    ((mig_reply_error_t *)reply)->NDR = NDR_record;
    ((mig_reply_error_t *)reply)->RetCode = MIG_BAD_ID;
    
    return processed;
}



/*******************************************************************************
 * mig_server_callback
 *
 ******************************************************************************/
__private_extern__ void mig_server_callback(
    CFMachPortRef port, 
    void *msg, 
    CFIndex size, 
    void *info)
{
    mig_reply_error_t * bufRequest = (mig_reply_error_t *)msg;
    mig_reply_error_t * bufReply = (mig_reply_error_t *) CFAllocatorAllocate(
        NULL, _fakeups_subsystem.maxsize, 0);
    mach_msg_return_t   mr;
    int                 options;

    /*
     * we have a request message 
     * pm_mig_demux actually maps this message to, 
     * and executes, our local mig calls.
     */
    (void) pm_mig_demux(&bufRequest->Head, &bufReply->Head);

    if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
        (bufReply->RetCode != KERN_SUCCESS)) {

        if (bufReply->RetCode == MIG_NO_REPLY) {
            /*
             * This return code is a little tricky -- it appears that the
             * demux routine found an error of some sort, but since that
             * error would not normally get returned either to the local
             * user or the remote one, we pretend it's ok.
             */
            CFAllocatorDeallocate(NULL, bufReply);
            return;
        }

        /*
         * destroy any out-of-line data in the request buffer but don't destroy
         * the reply port right (since we need that to send an error message).
         */
        bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
        mach_msg_destroy(&bufRequest->Head);
    }

    if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
        /* no reply port, so destroy the reply */
        if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
            mach_msg_destroy(&bufReply->Head);
        }
        CFAllocatorDeallocate(NULL, bufReply);
        return;
    }

    /*
     * send reply.
     *
     * We don't want to block indefinitely because the client
     * isn't receiving messages from the reply port.
     * If we have a send-once right for the reply port, then
     * this isn't a concern because the send won't block.
     * If we have a send right, we need to use MACH_SEND_TIMEOUT.
     * To avoid falling off the kernel's fast RPC path unnecessarily,
     * we only supply MACH_SEND_TIMEOUT when absolutely necessary.
     */

    options = MACH_SEND_MSG;
    if ( MACH_MSGH_BITS_REMOTE(bufReply->Head.msgh_bits) 
       == MACH_MSG_TYPE_MOVE_SEND_ONCE) 
    {
        options |= MACH_SEND_TIMEOUT;
    }
    mr = mach_msg(&bufReply->Head,          /* msg */
              options,                      /* option */
              bufReply->Head.msgh_size,     /* send_size */
              0,                            /* rcv_size */
              MACH_PORT_NULL,               /* rcv_name */
              MACH_MSG_TIMEOUT_NONE,        /* timeout */
              MACH_PORT_NULL);              /* notify */


    /* Has a message error occurred? */
    switch (mr) {
        case MACH_SEND_INVALID_DEST:
        case MACH_SEND_TIMED_OUT:
            /* the reply can't be delivered, so destroy it */
            mach_msg_destroy(&bufReply->Head);
            break;

        default :
            /* Includes success case.  */
            break;
    }

    CFAllocatorDeallocate(NULL, bufReply);
}

