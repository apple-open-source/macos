/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include <os/log.h>

#include <DriverKit/IOLib.h>
#include <DriverKit/IOMemoryMap.h>
#include <BlockStorageDeviceDriverKit/IOUserBlockStorageDevice.h>
#include <DriverKit/IODispatchQueue.h>

#undef super
#define super IOService

struct IOUserBlockStorageDevice_IVars
{
	IODispatchQueue *fCompletionQueue;
};

bool IOUserBlockStorageDevice::init()
{
	if (!super::init())
		return false;

	ivars = IONewZero(IOUserBlockStorageDevice_IVars, 1);
	if (!ivars)
		return false;

	return true;

}

void IOUserBlockStorageDevice::free()
{
	IODelete ( ivars, IOUserBlockStorageDevice_IVars, 1 );
	super::free ( );
}

kern_return_t
IMPL(IOUserBlockStorageDevice, DoAsyncUnmap)
{
	IOMemoryMap *map;
	kern_return_t retVal;

	retVal = buffer->CreateMapping(0,0,0,0,0,&map);
	if (retVal != kIOReturnSuccess) {
		os_log(OS_LOG_DEFAULT, "IOUserBlockStorageDevice::DoAsyncUnmap() - Failed to map the buffer");
		goto out;
	}

	retVal = DoAsyncUnmapPriv(requestID, (struct BlockRange *)map->GetAddress(), numOfRanges);

	OSSafeReleaseNULL(map)
out:
	return retVal;
}

kern_return_t IMPL(IOUserBlockStorageDevice, Stop)
{
	os_log(OS_LOG_DEFAULT, "IOUserBlockStorageDevice::Stop()");
    
    void  ( ^finalize )( void ) = ^{
 
        ivars->fCompletionQueue->release();
        
        Stop ( provider, SUPERDISPATCH );
        
    };

    /* Stop dispatch queue */
    ivars->fCompletionQueue->Cancel ( finalize );
 
    return kIOReturnSuccess;
}

kern_return_t IMPL(IOUserBlockStorageDevice, Start)
{
	kern_return_t retVal;

	os_log(OS_LOG_DEFAULT, "IOUserBlockStorageDevice::Start()");

	retVal = IODispatchQueue::Create ( "CompletionQueue", 0, 0, &ivars->fCompletionQueue );
	if (retVal != kIOReturnSuccess) {
		os_log(OS_LOG_DEFAULT, "IOUserBlockStorageDevice::Start() - failed to create dispatch queue");
		return retVal;
	}

	retVal = SetDispatchQueue ( "Completion", ivars->fCompletionQueue );
	if (retVal != kIOReturnSuccess) {
		os_log(OS_LOG_DEFAULT, "IOUserBlockStorageDevice::Start() - failed to set dispatch queue");
		return retVal;
	}

	retVal = RegisterDext();
	if (retVal != kIOReturnSuccess) {
		os_log(OS_LOG_DEFAULT, "IOUserBlockStorageDevice::Start() - failed to set register dext");
		return retVal;
	}

	return Start(provider, SUPERDISPATCH);
}
