/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#include "globals.h"
#include "logging.h"

CFMachPortRef _kextload_lock = NULL;
static CFRunLoopSourceRef _sKextloadLockRunLoopSource = NULL;

static void _kextload_lock_died(CFMachPortRef port, void * info);

/******************************************************************************
 * _kextmanager_lock_volume tries to lock volumes for clients (kextload)
 *****************************************************************************/
static void removeKextloadLock(void)
{
    if (_kextload_lock) {
        mach_port_t machPort;

        CFMachPortSetInvalidationCallBack(_kextload_lock, NULL);
        machPort = CFMachPortGetPort(_kextload_lock);
        CFRelease(_kextload_lock);
        _kextload_lock = NULL;
        mach_port_deallocate(mach_task_self(), machPort);
    }

    if (_sKextloadLockRunLoopSource) {
       CFRunLoopSourceInvalidate(_sKextloadLockRunLoopSource);
       CFRelease(_sKextloadLockRunLoopSource);
       _sKextloadLockRunLoopSource = NULL;
    }
    
    // tell the run loop to check for pending kernel load requests
    CFRunLoopSourceSignal(gKernelRequestRunLoopSource);
    CFRunLoopWakeUp(gMainRunLoop);
    return;
}

/******************************************************************************
 * _kextmanager_lock_volume tries to lock volumes for clients (kextload)
 *****************************************************************************/
kern_return_t _kextmanager_lock_kextload(
    mach_port_t server,
    mach_port_t client,
    int * lockstatus)
{
    kern_return_t mig_result = KERN_FAILURE;
    int result;

    if (!lockstatus) {
        kextd_error_log("kextmanager_lock_kextload requires non-NULL lockstatus");
        mig_result = KERN_SUCCESS;
        result = EINVAL;
        goto finish;
    }

    if (gClientUID != 0) {
        kextd_error_log("non-root kextload doesn't need to lock");
        mig_result = KERN_SUCCESS;
        result = EPERM;
        goto finish;
    }

    if (_kextload_lock) {
        mig_result = KERN_SUCCESS;
        result = EBUSY;
        goto finish;
    }

    result = ENOMEM;
    _kextload_lock = CFMachPortCreateWithPort(kCFAllocatorDefault,
        client, /* callback */ NULL,
        /* context */ NULL, /* should free */ false);
    if (!_kextload_lock) {
        goto finish;
    }
    CFMachPortSetInvalidationCallBack(_kextload_lock, _kextload_lock_died);
    _sKextloadLockRunLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, _kextload_lock, /* order */ 0);
    if (!_sKextloadLockRunLoopSource) {
        goto finish;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), _sKextloadLockRunLoopSource,
        kCFRunLoopDefaultMode);
    // Don't release it, we reference it elsewhere

    mig_result = KERN_SUCCESS;
    result = 0;

finish:
    if (mig_result != KERN_SUCCESS) {
        if (gClientUID == 0) {
            kextd_error_log("trouble while locking for kextload");
        }
        removeKextloadLock();
    } else {
        *lockstatus = result;  // only meaningful if mig_result == KERN_SUCCESS
    }

    return mig_result;
}

/******************************************************************************
 * _kextmanager_unlock_kextload unlocks for clients (kextload)
 *****************************************************************************/
kern_return_t _kextmanager_unlock_kextload(
    mach_port_t server,
    mach_port_t client)
{
    kern_return_t mig_result = KERN_FAILURE;
    
    if (gClientUID != 0) {
        kextd_error_log("non-root kextload doesn't need to lock/unlock");
        mig_result = KERN_SUCCESS;
        goto finish;
    }
    
    if (client != CFMachPortGetPort(_kextload_lock)) {
        kextd_error_log("%d not used to lock for kextload", client);
        goto finish;
    }

    removeKextloadLock();
    
    mig_result = KERN_SUCCESS;
    
finish:    
    // we don't need the extra send right added by MiG
    mach_port_deallocate(mach_task_self(), client);
    
    return mig_result;
}

/******************************************************************************
 * _kextload_lock_died tells us when the receive right went away
 * - this is okay if we're currently unlocked; bad otherwise
 *****************************************************************************/
static void _kextload_lock_died(CFMachPortRef port, void * info)
{
    if (port == _kextload_lock) {
        kextd_error_log("client exited without releasing kextload lock");
        removeKextloadLock();
    }
    return;
}
