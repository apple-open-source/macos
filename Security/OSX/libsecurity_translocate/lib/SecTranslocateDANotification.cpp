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

#include <exception>
#include <dlfcn.h>
#include <dispatch/dispatch.h>
#include <DiskArbitration/DiskArbitration.h>

#include <security_utilities/logging.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/unix++.h>

#include "SecTranslocateDANotification.hpp"
#include "SecTranslocateShared.hpp"
#include "SecTranslocateUtilities.hpp"

#define DA_FRAMEWORK_PATH "/System/Library/Frameworks/DiskArbitration.framework/Versions/A/DiskArbitration"

namespace Security {
namespace SecTranslocate {

typedef enum {
	SCT_DA_DISK_DISAPPEARED,
	SCT_DA_DISK_UNMOUNT_APPROVAL,
	SCT_DA_DISK_DESCRIPTION_CHANGE
} SecTranslocateDACallbackType_t;

typedef CFDictionaryRef (*DiskCopyDescription_t)(DADiskRef);
typedef DASessionRef (*SessionCreate_t) (CFAllocatorRef);
typedef void (*SessionSetDispatchQueue_t)(DASessionRef,dispatch_queue_t);
typedef void (*RegisterDiskDisappearedCallback_t) (DASessionRef, CFDictionaryRef, DADiskDisappearedCallback, void*);
typedef void (*RegisterDiskUnmountApprovalCallback_t) (DASessionRef, CFDictionaryRef, DADiskUnmountApprovalCallback, void*);
typedef void (*RegisterDiskDescriptionChangedCallback_t) (DASessionRef, CFDictionaryRef, CFArrayRef,
	DADiskDescriptionChangedCallback, void*);
typedef void (*UnregisterCallback_t)(DASessionRef, void*, void*);

class DiskArbitrationProxy
{
public:
    static DiskArbitrationProxy* get();

    inline CFDictionaryRef diskCopyDescription(DADiskRef disk) const
        { return pDiskCopyDescription ? pDiskCopyDescription(disk) : NULL; };
    inline DASessionRef sessionCreate (CFAllocatorRef allocator) const
        { return pSessionCreate ? pSessionCreate(allocator) : NULL; };
    inline void sessionSetDispatchQueue (DASessionRef s, dispatch_queue_t q) const
        { if(pSessionSetDispatchQueue) pSessionSetDispatchQueue(s,q); };
    inline void registerDiskDisappearedCallback (DASessionRef s, CFDictionaryRef d, DADiskDisappearedCallback c, void* x) const
        { if(pRegisterDiskDisappearedCallback) pRegisterDiskDisappearedCallback(s,d,c,x); };
    inline void registerDiskUnmountApprovalCallback (DASessionRef s, CFDictionaryRef d, DADiskUnmountApprovalCallback c, void* x) const
        { if(pRegisterDiskUnmountApprovalCallback) pRegisterDiskUnmountApprovalCallback(s,d,c,x); };
	inline void registerDiskDescriptionChangedCallback (DASessionRef s, CFDictionaryRef d, CFArrayRef a, DADiskDescriptionChangedCallback c, void* x) const
		{ if(pRegisterDiskDescriptionChangedCallback) pRegisterDiskDescriptionChangedCallback(s,d,a,c,x); };
    inline void unregisterCallback (DASessionRef s, void* c, void* x) const
        { if(pUnregisterCallback) pUnregisterCallback(s,c,x); };
    inline CFDictionaryRef diskDescriptionMatchVolumeMountable() const
        { return pDiskDescriptionMatchVolumeMountable ? *pDiskDescriptionMatchVolumeMountable : NULL; };
    inline CFStringRef diskDescriptionVolumePathKey() const
        { return pDiskDescriptionVolumePathKey ? *pDiskDescriptionVolumePathKey : NULL; };

private:
    DiskArbitrationProxy();

    void* handle;
    DiskCopyDescription_t pDiskCopyDescription;
    SessionCreate_t pSessionCreate;
    SessionSetDispatchQueue_t pSessionSetDispatchQueue;
    RegisterDiskDisappearedCallback_t pRegisterDiskDisappearedCallback;
    RegisterDiskUnmountApprovalCallback_t pRegisterDiskUnmountApprovalCallback;
	RegisterDiskDescriptionChangedCallback_t pRegisterDiskDescriptionChangedCallback;
    UnregisterCallback_t pUnregisterCallback;
    CFDictionaryRef* pDiskDescriptionMatchVolumeMountable;
    CFStringRef* pDiskDescriptionVolumePathKey;
};

DiskArbitrationProxy::DiskArbitrationProxy()
{
    handle = checkedDlopen(DA_FRAMEWORK_PATH, RTLD_LAZY | RTLD_NOLOAD);

    pDiskCopyDescription = (DiskCopyDescription_t) checkedDlsym(handle, "DADiskCopyDescription");
    pSessionCreate = (SessionCreate_t) checkedDlsym(handle, "DASessionCreate");

    pSessionSetDispatchQueue = (SessionSetDispatchQueue_t) checkedDlsym(handle, "DASessionSetDispatchQueue");
    pRegisterDiskDisappearedCallback = (RegisterDiskDisappearedCallback_t) checkedDlsym(handle, "DARegisterDiskDisappearedCallback");
    pRegisterDiskUnmountApprovalCallback = (RegisterDiskUnmountApprovalCallback_t) checkedDlsym(handle, "DARegisterDiskUnmountApprovalCallback");
	pRegisterDiskDescriptionChangedCallback = (RegisterDiskDescriptionChangedCallback_t) checkedDlsym(handle,"DARegisterDiskDescriptionChangedCallback");
    pUnregisterCallback = (UnregisterCallback_t) checkedDlsym(handle, "DAUnregisterCallback");
    pDiskDescriptionMatchVolumeMountable = (CFDictionaryRef*) checkedDlsym(handle, "kDADiskDescriptionMatchVolumeMountable");
    pDiskDescriptionVolumePathKey = (CFStringRef*) checkedDlsym(handle, "kDADiskDescriptionVolumePathKey");
}

DiskArbitrationProxy* DiskArbitrationProxy::get()
{
    static dispatch_once_t initialized;
    static DiskArbitrationProxy* me = NULL;
    __block exception_ptr exception(0);

    dispatch_once(&initialized, ^{
        try
        {
            me = new DiskArbitrationProxy();
        }
        catch (...)
        {
            Syslog::critical("SecTranslocate: error while creating DiskArbitrationProxy");
            exception = current_exception();
        }
    });

    if (me == NULL)
    {
        if(exception)
        {
            rethrow_exception(exception); //already logged in this case
        }
        else
        {
            Syslog::critical("SecTranslocate: DiskArbitrationProxy initialization has failed");
            UnixError::throwMe(EINVAL);
        }
    }

    return me;
}
/*
 For Disk Arbitration need to
 1. create a session and hold on to it.
 2. associate it with a queue
 3. register for call backs  (DADiskDisappearedCallback, DADiskDescriptionChangedCallback and DADiskUnmountApprovalCallback)
 4. provide a function to get the mounton path from DADiskref
 5. Return a dissenter if unmount is gonna fail because something is in use (i.e. if my unmount fails)
 */

/* Returns false if we failed an unmount call. anything else returns true */
static bool cleanupDisksOnVolume(DADiskRef disk, SecTranslocateDACallbackType_t type)
{
    bool result = true;
    string fspathString;
    try
    {
        DiskArbitrationProxy *dap = DiskArbitrationProxy::get();
        CFRef<CFDictionaryRef> dict = dap->diskCopyDescription(disk);

        if(!dict)
        {
            Syslog::error("SecTranslocate:disk cleanup, failed to get disk description");
            UnixError::throwMe(EINVAL);
        }

        CFURLRef fspath = (CFURLRef)CFDictionaryGetValue(dict, dap->diskDescriptionVolumePathKey());

        if(fspath)
        {
			if (type != SCT_DA_DISK_DESCRIPTION_CHANGE) {
				//For the disk disappeared call back, it looks like we won't get a volume path so we'll keep the empty string
				fspathString = cfString(fspath);
			} else {
				// For disk description changed, volume path key will be populated on mount and null on unmount
				// we only care about unmount, so bail if there is a volume path key
				return result;
			}
        }

        result = destroyTranslocatedPathsForUserOnVolume(fspathString);
    }
    catch (...)
    {
        // This function is called from inside a DiskArbitration callback so we need to consume the exception
        // more specific errors are assumed to be logged by the thrower
        Syslog::error("SecTranslocate: DiskArbitration callback: failed to clean up mountpoint(s) related to volume: %s",
                      fspathString.empty() ? "unknown" : fspathString.c_str());
    }
    
    return result;
}

static void diskDisappearedCallback(DADiskRef disk, void* __unused context)
{
    (void)cleanupDisksOnVolume(disk, SCT_DA_DISK_DISAPPEARED);
}

static DADissenterRef unmountApprovalCallback(DADiskRef disk, void* __unused context)
{
    (void)cleanupDisksOnVolume(disk, SCT_DA_DISK_UNMOUNT_APPROVAL);
    return NULL; //For now, we won't raise a dissent, just let the unmount fail. The dissent text would get used by UI.
}

static void diskDescriptionChangedCallback(DADiskRef disk, CFArrayRef __unused daKeys, void* __unused context)
{
	(void)cleanupDisksOnVolume(disk, SCT_DA_DISK_DESCRIPTION_CHANGE);
}

DANotificationMonitor::DANotificationMonitor(dispatch_queue_t q)
{
    DiskArbitrationProxy *dap = DiskArbitrationProxy::get();
    if (q == NULL)
    {
        Syslog::critical("SecTranslocate::DANotificationMonitor initialized without a queue.");
        UnixError::throwMe(EINVAL);
    }

    diskArbitrationSession = dap->sessionCreate(kCFAllocatorDefault);
    if(!diskArbitrationSession)
    {
        Syslog::critical("SecTranslocate: Failed to create the disk arbitration session");
        UnixError::throwMe(ENOMEM);
    }

    dap->sessionSetDispatchQueue(diskArbitrationSession, q);
    /* register so we can cleanup from force unmounts */
    dap->registerDiskDisappearedCallback( diskArbitrationSession, dap->diskDescriptionMatchVolumeMountable(), diskDisappearedCallback, NULL );
	dap->registerDiskDescriptionChangedCallback(diskArbitrationSession, dap->diskDescriptionMatchVolumeMountable(), NULL, diskDescriptionChangedCallback, NULL);
    /* register so we can clean up pre-unmount */
    dap->registerDiskUnmountApprovalCallback( diskArbitrationSession, dap->diskDescriptionMatchVolumeMountable(), unmountApprovalCallback, NULL );
}

DANotificationMonitor::~DANotificationMonitor()
{
    DiskArbitrationProxy::get()->unregisterCallback(diskArbitrationSession,(void*)diskDisappearedCallback, NULL);
    DiskArbitrationProxy::get()->unregisterCallback(diskArbitrationSession,(void*)unmountApprovalCallback, NULL);
    CFRelease(diskArbitrationSession);
}

} //namespace SecTranslocate
} //namespace Security
