/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <mach/mach_port.h>
 
#include <IOKit/kext/OSKext.h>
#include "kext_tools_util.h"
#include "kextd_globals.h"

CFMachPortRef _gKextutilLock = NULL;
static CFRunLoopSourceRef _sKextloadLockRunLoopSource = NULL;

static void _gKextutilLock_died(CFMachPortRef port, void * info);
void kextd_process_kernel_requests(void);

/******************************************************************************
 * _kextmanager_lock_volume tries to lock volumes for clients (kextload)
 *****************************************************************************/
static void removeKextloadLock(void)
{
    if (_gKextutilLock) {
        mach_port_t machPort;

        CFMachPortSetInvalidationCallBack(_gKextutilLock, NULL);
        machPort = CFMachPortGetPort(_gKextutilLock);
        SAFE_RELEASE_NULL(_gKextutilLock);
        mach_port_deallocate(mach_task_self(), machPort);
    }

    if (_sKextloadLockRunLoopSource) {
       CFRunLoopSourceInvalidate(_sKextloadLockRunLoopSource);
       SAFE_RELEASE_NULL(_sKextloadLockRunLoopSource);
    }
    
    if (gKernelRequestsPending) {
        kextd_process_kernel_requests();
    }

    CFRunLoopWakeUp(CFRunLoopGetCurrent());
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
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "kextmanager_lock_kextload requires non-NULL lockstatus.");
        mig_result = KERN_SUCCESS;
        result = EINVAL;
        goto finish;
    }

    if (gClientUID != 0) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel,
            "Non-root process doesn't need to lock as it will fail to load.");
        mig_result = KERN_SUCCESS;
        result = EPERM;
        goto finish;
    }

    if (_gKextutilLock) {
        mig_result = KERN_SUCCESS;
        result = EBUSY;
        goto finish;
    }

    result = ENOMEM;
    _gKextutilLock = CFMachPortCreateWithPort(kCFAllocatorDefault,
        client, /* callback */ NULL,
        /* context */ NULL, /* shouldFree */ false);
    if (!_gKextutilLock) {
        goto finish;
    }
    CFMachPortSetInvalidationCallBack(_gKextutilLock, _gKextutilLock_died);
    _sKextloadLockRunLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, _gKextutilLock, /* order */ 0);
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
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogIPCFlag,
                "Trouble while locking for kextload - %s.",
                safe_mach_error_string(mig_result));
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
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Non-root kextload doesn't need to lock/unlock.");
        mig_result = KERN_SUCCESS;
        goto finish;
    }
    
    if (client != CFMachPortGetPort(_gKextutilLock)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "%d not used to lock for kextload.", client);
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
 * _gKextutilLock_died tells us when the receive right went away
 * - this is okay if we're currently unlocked; bad otherwise
 *****************************************************************************/
static void _gKextutilLock_died(CFMachPortRef port, void * info)
{
    if (port == _gKextutilLock) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Client exited without releasing kextload lock.");
        removeKextloadLock();
    }
    return;
}
