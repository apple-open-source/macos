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

#include "IOUserBlockStorageDevice_kext.h"
#include "IORequest.h"

#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOMapper.h>
#include <IOKit/perfcontrol/IOPerfControl.h>

#define ABORTED_REQUEST ((IORequest *)(uintptr_t)(-1LL))
#define kSyncCallTOSec 5

#define kIOBlockStorageDeviceDextEntitlement "com.apple.developer.driverkit.family.block-storage-device"

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(IOUserBlockStorageDevice, IOBlockStorageDevice)

#define LOG_PRINT(level, fmt, args...) IOLog("%s %s: " fmt, level, __PRETTY_FUNCTION__,  ## args)
#define LOG_ERR(fmt, args...) LOG_PRINT("ERR", fmt, ## args)
#define LOG_INFO(fmt, args...) LOG_PRINT("INFO", fmt, ## args)

#ifdef ENABLE_LOG
	#define LOG_DBG(fmt, args...) LOG_PRINT("DBG", fmt, ## args)
#else
	#define LOG_DBG(fmt, args...)
#endif

bool IOUserBlockStorageDevice::init ( OSDictionary * dict )
{
	LOG_INFO("Allocate resources\n");

	bool retVal;
	retVal = super::init ( dict );
	if (!retVal) {
		LOG_ERR("Failed to init");
		goto out;
	}

	fMapper = NULL;
	fVendorName = NULL;
	fProductName = NULL;
	fRevision = NULL;
	fAdditionDeviceInfo = NULL;
	fPoolInitialized = false;

	retVal = setProperty ( kIOServiceDEXTEntitlementsKey, kIOBlockStorageDeviceDextEntitlement );
out:
	return retVal;
}

void IOUserBlockStorageDevice::free( void )
{

	/* At this point, there is no client using  IOUserBlockStorageDevice and the dext is stopped */
	IODelete(fOutstandingRequests, IORequest *, fDeviceParams.numOfOutstandingIOs);

	if (fPoolInitialized)
		fRequestsPool.deinit();

	OSSafeReleaseNULL(fMapper);
	OSSafeReleaseNULL(fVendorName);
	OSSafeReleaseNULL(fProductName);
	OSSafeReleaseNULL(fRevision);
	OSSafeReleaseNULL(fAdditionDeviceInfo);

	super::free();

}
bool IOUserBlockStorageDevice::willTerminate( IOService * provider, IOOptionBits options )
{
	bool retVal;
	uint32_t i;
	IORequest *request;

	LOG_INFO("Terminating");

	retVal = super::willTerminate ( provider, options );

	/* TODO: Add the counter of oustanding requests to skip the loop if there are no such ones */
	/* Complete all outstanding IO requests */
	for (i = 0; i < fDeviceParams.numOfOutstandingIOs; i++) {

		request = removeOutstandingRequestAndMarkSlotAborted(i);
		if (request == NULL)
			continue;

		if (request->getIORequest()) {
			LOG_INFO("Completing outstanding async request %u", request->getIndex());
			request->complete(kIOReturnAborted, 0);
			fRequestsPool.putRequest(request);
		}
		else {
			LOG_INFO("Completing outstanding sync request %u", request->getIndex());
			request->signalCompleted(kIOReturnAborted);
		}
	}

	return retVal;
}

void IOUserBlockStorageDevice::stop(IOService *provider)
{
	LOG_INFO("Stopped");

	if (fPerfControlClient) {
		fPerfControlClient->unregisterDevice(getProvider(), this);
		fPerfControlClient->release();
		fPerfControlClient = NULL;
	}

	super::stop ( provider );
}

bool IOUserBlockStorageDevice::start(IOService *provider)
{

	OSDictionary *	dict = NULL;

	LOG_INFO("Started");

	if (super::start(provider) == false) {
		LOG_ERR("Failed to start base");
		return false;
	}

	if (kIOReturnSuccess != Start(provider)) {
		LOG_ERR("Start() execution failed");
		goto FailedToStart;
	}

	/* Get device specific parameters */
	GetDeviceParams(&fDeviceParams);

	if (fDeviceParams.blockSize == 0) {
		LOG_ERR("Bad block size %u", fDeviceParams.blockSize);
		goto FailedToStart;
	}

	if (fDeviceParams.numOfOutstandingIOs == 0) {
		LOG_ERR("Bad num of outstanding IOs %u", fDeviceParams.numOfOutstandingIOs);
		goto FailedToStart;
	}

	dict = OSDictionary::withCapacity ( 2 );
	if (dict == NULL) {
		LOG_ERR("Failed to allocate new dictionary");
		goto FailedToAllocDictionary;
	}

	/* Set properties */
	setProperty ( kIOMinimumSegmentAlignmentByteCountKey, fDeviceParams.minSegmentAlignment, 32);
	setProperty ( kIOMaximumByteCountReadKey, fDeviceParams.maxIOSize, sizeof ( fDeviceParams.maxIOSize ) * NBBY );
	setProperty ( kIOMaximumByteCountWriteKey, fDeviceParams.maxIOSize, sizeof ( fDeviceParams.maxIOSize ) * NBBY );
	setProperty ( kIOMaximumBlockCountReadKey, fDeviceParams.maxIOSize / fDeviceParams.blockSize, sizeof ( fDeviceParams.maxIOSize ) * NBBY );
	setProperty ( kIOMaximumBlockCountWriteKey, fDeviceParams.maxIOSize / fDeviceParams.blockSize, sizeof ( fDeviceParams.maxIOSize ) * NBBY );
	dict->setObject ( kIOStorageFeatureUnmap, fDeviceParams.isUnmapSupported ? kOSBooleanTrue : kOSBooleanFalse);
	dict->setObject ( kIOStorageFeatureForceUnitAccess, fDeviceParams.isFUASupported ? kOSBooleanTrue : kOSBooleanFalse);
	setProperty ( kIOStorageFeaturesKey, dict );
	OSSafeReleaseNULL(dict);

	/* Allocate array of outstanding requests */
	fOutstandingRequests = IONewZero(IORequest * _Atomic, fDeviceParams.numOfOutstandingIOs);
	if (fOutstandingRequests == NULL) {
		LOG_ERR("failed to allocate outstanding requests table\n");
		goto FailedToAlloc;
	}

	fMapper = IOMapper::copyMapperForDevice(provider);

	/* Initialize requests pool */
	if (fRequestsPool.init(fDeviceParams.numOfOutstandingIOs,
			       fDeviceParams.maxIOSize,
			       fDeviceParams.numOfAddressBits,
			       fDeviceParams.minSegmentAlignment,
			       fMapper) != kIOReturnSuccess) {
		LOG_ERR("failed to init requests pool\n");
		goto FailedToInitPool;
	}

	fPoolInitialized = true;

	registerService();

	return true;

FailedToInitPool:
	IODelete(fOutstandingRequests, IORequest *, fDeviceParams.numOfOutstandingIOs);
FailedToAlloc:
FailedToAllocDictionary:
	/* TODO: Stop() should be called explicitly ? */
FailedToStart:
	super::stop(provider);
	
	return false;
}

IOReturn IOUserBlockStorageDevice::doAsyncReadWrite ( IOMemoryDescriptor *inBuffer,
						     uint64_t inLBA,
						     uint64_t inBlockCount,
						     IOStorageAttributes *inAttributes,
						     IOStorageCompletion *inCompletion )
{
	IODirection direction;
	IOReturn retVal = kIOReturnSuccess;
	IORequest *request = NULL;
	IOUserStorageOptions options;
	bool isReadRequest;

	uint64_t dmaAddr;
	uint64_t dmaSize;

	if (isInactive()) {
		LOG_ERR("Service is terminating");
		retVal = kIOReturnNoDevice;
		goto ServiceIsInactive;
	}

	direction = inBuffer->getDirection();

	/* Get the request from the pool and fill it with the relevant data */
	request = fRequestsPool.getReguest();

	/* Prepare request */
	retVal = request->prepare(inCompletion, inBuffer, &dmaAddr, &dmaSize);
	if (retVal != kIOReturnSuccess) {
		LOG_ERR("Failed to prepare request");
		retVal = kIOReturnError;
		goto FailedToPrepare;
	}
	
	/* Submit the IO */
	request->setIORequest(true);
	request->setBytesToTransfer(inBlockCount * fDeviceParams.blockSize);

	if (!addOutstandingRequestAndMarkSlotOccupied(request)) {
		LOG_INFO("Failed to put request to the outstanding array");
		retVal = kIOReturnError;
		goto FailedToMakeOutstanding;
	}
	
	options = kIOUserStorageOptionNone;
	if (inAttributes != NULL) {
		if (inAttributes->options & kIOStorageOptionForceUnitAccess)
			options |= kIOUserStorageOptionForceUnitAccess;
	}

	isReadRequest = (direction == kIODirectionIn) ? true : false;
	LOG_DBG("%s: request ID %u, dma addr 0x%llX, start LBA 0x%llX, num of blocks 0x%llX\n",
		isReadRequest ? "READ" : "WRITE",
		request->getIndex(),
		dmaAddr,
		inLBA,
		inBlockCount);

	retVal = DoAsyncReadWrite(isReadRequest, request->getIndex(), dmaAddr, dmaSize, inLBA, inBlockCount, options);
	if (retVal != kIOReturnSuccess) {
		LOG_ERR("Failed to submit request");
		request = removeOutstandingRequestAndMarkSlotFree(request->getIndex());
		/* If request is NULL, it means that termination is in process and the request will be completed
		 during termination */
		if (request)
			goto FailedToSubmit;
	}

	return kIOReturnSuccess;

FailedToSubmit:
FailedToMakeOutstanding:
	request->reset();
FailedToPrepare:
	fRequestsPool.putRequest(request);
ServiceIsInactive:
	return retVal;
}

void IMPL(IOUserBlockStorageDevice, CompleteIO)
{
	LOG_DBG("request ID %u\n", requestID);

	IORequest *request;

	if (requestID >= fDeviceParams.numOfOutstandingIOs) {
		LOG_ERR("Bad request ID");
		return;
	}

	if (isInactive()) {
		LOG_ERR("Service is terminating");
		return;
	}

	/* Retrieve the outstanding request */
	request = removeOutstandingRequestAndMarkSlotFree(requestID);
	if (request) {

		/* Sanity check of bytesTransfered value */
		if (bytesTransferred > request->getBytesToTransfer()) {
			LOG_ERR("The number of transferred bytes is wrong");
			IOStatus = kIOReturnIOError;
			bytesTransferred = 0;
		}

		/* Complete request */
		request->complete(IOStatus, bytesTransferred);

		/* Return requst to the pool */
		fRequestsPool.putRequest(request);
	}

}

IOReturn IOUserBlockStorageDevice::doSynchronize(UInt64 block, UInt64 nblks, IOStorageSynchronizeOptions options)
{
	IORequest *request = NULL;
	IOReturn retVal = kIOReturnSuccess;

	if (isInactive()) {
		LOG_ERR("Service is terminating");
		retVal = kIOReturnNoDevice;
		goto ServiceIsInactive;
	}

	/* Get request from the pool */
	request = fRequestsPool.getReguest();
	
	/* Prepare to wait */
	request->prepareToWait();
	
	LOG_DBG("request ID %u\n", request->getIndex());

	request->setIORequest(false);
	request->setBytesToTransfer(0);
	if (!addOutstandingRequestAndMarkSlotOccupied(request)) {
		LOG_INFO("Failed to put request to the outstanding array");
		retVal = kIOReturnNoDevice;
		goto FailedToMakeOutstanding;
	}

	/* Send request */
	retVal = DoAsyncSynchronize(request->getIndex(), block, nblks);
	if (retVal != kIOReturnSuccess) {
		markSlotFree(request);
		LOG_ERR("Failed to submit Synchronize request");
		goto FailedToSubmit;
	}

	/* Wait for the request to be completed */
	request->waitForCompletion();

	retVal = request->getStatus();

FailedToSubmit:
FailedToMakeOutstanding:
	/* return request to the pool */
	fRequestsPool.putRequest(request);
ServiceIsInactive:
	return retVal;
}

void IMPL(IOUserBlockStorageDevice, Complete)
{
	LOG_DBG("request ID %u\n", requestID);

	IORequest *request;

	if (requestID >= fDeviceParams.numOfOutstandingIOs) {
		LOG_ERR("Bad request ID");
		return;
	}

	if (isInactive()) {
		LOG_ERR("Service is terminating");
		return;
	}

	/* Retrieve the outstanding request */
	request = removeOutstandingRequestAndMarkSlotFree(requestID);
	if (request)
		/* Wake up sleeping thread */
		request->signalCompleted(status);
}

IOReturn IOUserBlockStorageDevice::doUnmap(IOBlockStorageDeviceExtent *extents,
					   UInt32 extentsCount,
					   IOStorageUnmapOptions options)
{

	uint32_t i;
	IORequest *request = NULL;
	IOReturn retVal = kIOReturnSuccess;
	struct BlockRange *blockRanges;
	uint32_t numOfRanges;
	uint32_t rangesSent;
	IOBufferMemoryDescriptor *dataBuffer;
	uint32_t numOfRangesInTheBuffer;

	if (isInactive()) {
		LOG_ERR("Service is terminating");
		retVal = kIOReturnNoDevice;
		goto ServiceIsInactive;
	}

	LOG_DBG("Number of extents %u\n", extentsCount);

	numOfRangesInTheBuffer = (fDeviceParams.maxNumOfUnmapRegions != 0) ? fDeviceParams.maxNumOfUnmapRegions : extentsCount;

	dataBuffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
								 kIOMemoryKernelUserShared | kIODirectionInOut,
								 numOfRangesInTheBuffer * sizeof(struct BlockRange));
	if (dataBuffer == NULL) {
		LOG_ERR("Failed to allocate data buffer\n");
		retVal = kIOReturnNoMemory;
		goto FailedToAllocate;
	};

	blockRanges = (struct BlockRange *)dataBuffer->getBytesNoCopy();

	rangesSent = 0;

	/* Get request from the pool */
	request = fRequestsPool.getReguest();
	LOG_DBG("request ID %u\n", request->getIndex());

	/* TODO: Map the the buffer once in the user space */

	/* Iterate over all extents, unampping at most numOfRangesInTheBuffer extents in every iteration */
	do {

		/* Calculate number of ranges to be sent in this iteration */
		numOfRanges = ((extentsCount - rangesSent ) >= numOfRangesInTheBuffer ) ? numOfRangesInTheBuffer : (extentsCount - rangesSent );
	
		request->prepareToWait();
		request->setIORequest(false);
		request->setBytesToTransfer(0);

		if (!addOutstandingRequestAndMarkSlotOccupied(request)) {
			LOG_INFO("Failed to put request to the outstanding array");
			retVal = kIOReturnNoDevice;
			break;
		}

		for (i = 0; i < numOfRanges; i++) {
			blockRanges[i].startBlock = extents[i + rangesSent].blockStart;
			blockRanges[i].numOfBlocks = extents[i + rangesSent].blockCount;
			LOG_DBG("Range %u - [%llu %llu]\n", i, blockRanges[i].startBlock, blockRanges[i].startBlock + blockRanges[i].numOfBlocks - 1);
		}

		retVal = DoAsyncUnmap(request->getIndex(), dataBuffer, numOfRanges);
		if (retVal != kIOReturnSuccess) {
			markSlotFree(request);
			LOG_ERR("Failed to submit Unmap request");
			break;
		}

		/* Wait for the request to be completed */
		request->waitForCompletion();
		retVal = request->getStatus();

		rangesSent += numOfRanges;
	}
	while( (rangesSent != extentsCount) && (retVal == kIOReturnSuccess));

	/* Release the buffer */
	OSSafeReleaseNULL(dataBuffer);

	/* return request to the pool */
	fRequestsPool.putRequest(request);

FailedToAllocate:
ServiceIsInactive:
	return retVal;

}

IOReturn IOUserBlockStorageDevice::doEjectMedia ( void )
{
	IORequest *request = NULL;
	IOReturn retVal = kIOReturnSuccess;

	if (isInactive()) {
		LOG_ERR("Service is terminating");
		retVal = kIOReturnNoDevice;
		goto ServiceIsInactive;
	}

	/* Get request from the pool */
	request = fRequestsPool.getReguest();
	
	/* Prepare to wait */
	request->prepareToWait();
	
	LOG_DBG("request ID %u\n", request->getIndex());

	request->setIORequest(false);
	request->setBytesToTransfer(0);

	if (!addOutstandingRequestAndMarkSlotOccupied(request)) {
		LOG_INFO("Failed to put request to the outstanding array");
		retVal = kIOReturnNoDevice;
		goto FailedToMakeOutstanding;
	}

	/* Send request */
	retVal = DoAsyncEjectMedia(request->getIndex());
	if (retVal != kIOReturnSuccess) {
		markSlotFree(request);
		LOG_ERR("Failed to submit Eject request");
		goto FailedToSubmit;
	}

	/* Wait for the request to be completed */
	request->waitForCompletion();

	retVal = request->getStatus();

FailedToSubmit:
FailedToMakeOutstanding:
	/* return request to the pool */
	fRequestsPool.putRequest(request);
ServiceIsInactive:
	return retVal;

}

IOReturn IOUserBlockStorageDevice::doFormatMedia ( uint64_t inByteCapacity )
{

	return kIOReturnUnsupported;
}


UInt32 IOUserBlockStorageDevice::doGetFormatCapacities ( UInt64 *ioCapacities,
							UInt32  inCapacitiesMaxCount ) const
{

	if (ioCapacities != NULL && inCapacitiesMaxCount != 0) {
		*ioCapacities = fDeviceParams.numOfBlocks * fDeviceParams.blockSize;
	}
	
	return 1;

}

char *
IOUserBlockStorageDevice::getVendorString ( void )
{
	struct DeviceString deviceString = {.data = {0}};

	if (fVendorName == NULL) {
		
		GetVendorString(&deviceString);
		deviceString.data[sizeof(deviceString.data) - 1] = '\0';
		fVendorName = OSString::withCString(deviceString.data);
	}

	return (char *)fVendorName->getCStringNoCopy();
}

char *
IOUserBlockStorageDevice::getProductString ( void )
{
	struct DeviceString deviceString = {.data = {0}};

	if (fProductName == NULL) {
		
		GetProductString(&deviceString);
		deviceString.data[sizeof(deviceString.data) - 1] = '\0';
		fProductName = OSString::withCString(deviceString.data);
	}

	return (char *)fProductName->getCStringNoCopy();
}

char *
IOUserBlockStorageDevice::getRevisionString ( void )
{
	struct DeviceString deviceString = {.data = {0}};

	if (fRevision == NULL) {
		
		GetVendorString(&deviceString);
		deviceString.data[sizeof(deviceString.data) - 1] = '\0';
		fRevision = OSString::withCString(deviceString.data);
	}

	return (char *)fRevision->getCStringNoCopy();
}

char *
IOUserBlockStorageDevice::getAdditionalDeviceInfoString ( void )
{
	struct DeviceString deviceString = {.data = {0}};

	if (fAdditionDeviceInfo == NULL) {
		
		GetVendorString(&deviceString);
		deviceString.data[sizeof(deviceString.data) - 1] = '\0';
		fAdditionDeviceInfo = OSString::withCString(deviceString.data);
	}

	return (char *)fAdditionDeviceInfo->getCStringNoCopy();
}

IOReturn
IOUserBlockStorageDevice::reportBlockSize ( uint64_t *ioBlockSize )
{

	*ioBlockSize = fDeviceParams.blockSize;
	return kIOReturnSuccess;
}

IOReturn
IOUserBlockStorageDevice::reportEjectability ( bool *ioIsEjectable )
{
	return ReportEjectability(ioIsEjectable);
}

IOReturn
IOUserBlockStorageDevice::reportMediaState ( bool *ioMediaPresent, bool *ioChanged )
{

	*ioMediaPresent = true;
	*ioChanged      = true;

	return kIOReturnSuccess;
}

IOReturn
IOUserBlockStorageDevice::reportMaxValidBlock ( uint64_t *ioMaxBlock )
{

	*ioMaxBlock = fDeviceParams.numOfBlocks - 1;
	return kIOReturnSuccess;
}

IOReturn
IOUserBlockStorageDevice::reportRemovability ( bool *ioIsRemovable )
{
	return ReportRemovability(ioIsRemovable);
}

IOReturn
IOUserBlockStorageDevice::reportWriteProtection ( bool *ioIsWriteProtected )
{
	return ReportWriteProtection(ioIsWriteProtected);
}

IOReturn IMPL(IOUserBlockStorageDevice, DoAsyncUnmap)
{
	return kIOReturnSuccess;
}

kern_return_t IMPL(IOUserBlockStorageDevice, Stop)
{
	return kIOReturnSuccess;
}

kern_return_t IMPL(IOUserBlockStorageDevice, Start)
{
	return kIOReturnSuccess;
}

IORequest *IOUserBlockStorageDevice::removeOutstandingRequestAndMarkSlotFree(uint32_t index)
{
	IORequest *request = NULL;

	/* If the slot doesn't contain the pointer to the real object */
	request = atomic_load(&fOutstandingRequests[ index ]);
	if (request == ABORTED_REQUEST)
		return NULL;

	/* If the the value was changed by contetded thread - NULL is returned */
	if (atomic_compare_exchange_strong(&fOutstandingRequests[ index ], &request, NULL))
		return request;
	else
		return NULL;

}

bool IOUserBlockStorageDevice::markSlotFree(IORequest *request)
{
	/* If the the value was changed by contetded thread - NULL is returned */
	return atomic_compare_exchange_strong(&fOutstandingRequests[ request->getIndex() ], &request, NULL);
}

IORequest *IOUserBlockStorageDevice::removeOutstandingRequestAndMarkSlotAborted(uint32_t index)
{
	return atomic_exchange ( &fOutstandingRequests[index], ABORTED_REQUEST );
}

bool IOUserBlockStorageDevice::addOutstandingRequestAndMarkSlotOccupied(IORequest * request)
{
	IORequest *tmp = NULL;

	return atomic_compare_exchange_strong(&fOutstandingRequests[ request->getIndex() ], &tmp, request);
}

kern_return_t IMPL(IOUserBlockStorageDevice, RegisterDext)
{
	LOG_INFO("Registering dext");

	fPerfControlClient = IOPerfControlClient::copyClient(this, 0);
	if (fPerfControlClient) {
		IOReturn ret = fPerfControlClient->registerDevice(getProvider(), this);
		if ( ret != kIOReturnSuccess ) {
			fPerfControlClient->release();
			fPerfControlClient = NULL;
		}
	}

	return kIOReturnSuccess;
}

IOPerfControlClient *IOUserBlockStorageDevice::getPerfControlClient()
{
	return fPerfControlClient;
}
