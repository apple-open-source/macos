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

#ifndef _IOUSERBLOCKSTORAGEDEVICE_KEXT_H
#define _IOUSERBLOCKSTORAGEDEVICE_KEXT_H

#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/IOLocks.h>
#include <BlockStorageDeviceDriverKit/IOUserBlockStorageDevice.h>
#include "IORequestsPool.h"
#include <stdatomic.h>

class IOPerfControlClient;
class IORequest;
class IOMapper;
class IOUserBlockStorageDevice : public IOBlockStorageDevice
{
	OSDeclareDefaultStructorsWithDispatch ( IOUserBlockStorageDevice );
public:
	virtual bool init ( OSDictionary * dictionary = NULL ) APPLE_KEXT_OVERRIDE;
	virtual void free( void ) APPLE_KEXT_OVERRIDE;
	virtual bool start (IOService* provider) APPLE_KEXT_OVERRIDE;
	virtual void stop (IOService* provider) APPLE_KEXT_OVERRIDE;
	virtual bool willTerminate( IOService * provider, IOOptionBits options ) APPLE_KEXT_OVERRIDE;

	IOReturn doAsyncReadWrite (IOMemoryDescriptor *inBuffer,
				   uint64_t inLBA,
				   uint64_t inBlockCount,
				   IOStorageAttributes *inAttributes,
				   IOStorageCompletion *inCompletion ) APPLE_KEXT_OVERRIDE;

	IOReturn doSynchronize(UInt64 block, UInt64 nblks, IOStorageSynchronizeOptions options) APPLE_KEXT_OVERRIDE;
	IOReturn doUnmap(IOBlockStorageDeviceExtent *extents,
			 UInt32 extentsCount,
			 IOStorageUnmapOptions options = 0) APPLE_KEXT_OVERRIDE;
	IOReturn doEjectMedia ( void ) APPLE_KEXT_OVERRIDE;

	virtual IOReturn doFormatMedia ( uint64_t inByteCapacity ) APPLE_KEXT_OVERRIDE;
	virtual UInt32 doGetFormatCapacities ( UInt64 *ioCapacities,
					 UInt32  inCapacitiesMaxCount ) const
	    APPLE_KEXT_OVERRIDE;

	virtual char *getVendorString ( void ) APPLE_KEXT_OVERRIDE;
	virtual char *getProductString ( void ) APPLE_KEXT_OVERRIDE;
	virtual char *getRevisionString ( void ) APPLE_KEXT_OVERRIDE;
	virtual char *getAdditionalDeviceInfoString ( void ) APPLE_KEXT_OVERRIDE;

	virtual IOReturn reportMaxValidBlock ( uint64_t *ioMaxBlock ) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportRemovability ( bool *ioIsRemovable ) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportWriteProtection ( bool *ioIsWriteProtected ) APPLE_KEXT_OVERRIDE;

	virtual IOReturn reportBlockSize ( uint64_t *ioBlockSize ) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportEjectability ( bool *ioIsEjectable ) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportMediaState ( bool *ioMediaPresent,
				    bool *ioChanged ) APPLE_KEXT_OVERRIDE;
    IOPerfControlClient *getPerfControlClient();
private:
	IORequest *removeOutstandingRequestAndMarkSlotFree(uint32_t index);
	IORequest *removeOutstandingRequestAndMarkSlotAborted(uint32_t index);
	bool addOutstandingRequestAndMarkSlotOccupied(IORequest * request);
	bool markSlotFree(IORequest *request);

private:
	IOMapper *fMapper;
	IORequestsPool fRequestsPool;

	/* Array of outstanding requests */
	IORequest * _Atomic *fOutstandingRequests;

	struct DeviceParams fDeviceParams;

	OSString *fVendorName;
	OSString *fProductName;
	OSString *fRevision;
	OSString *fAdditionDeviceInfo;

	IOPerfControlClient * fPerfControlClient;
	bool fPoolInitialized;

};

#endif /* _IOUSERBLOCKSTORAGEDEVICE_KEXT_H */
