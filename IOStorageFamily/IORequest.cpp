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

#include "IORequest.h"

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>

IOReturn IORequest::init(uint32_t Index, uint32_t maxIOSize, uint8_t numOfAddressBits, uint32_t allignment, IOMapper *mapper)
{
	fIndex = Index;

	fSyncLock = IOLockAlloc();
	if (fSyncLock == NULL)
		goto FailedtoAllocLock;

	fDMACommand = IODMACommand::withSpecification(kIODMACommandOutputHost64,
						      numOfAddressBits,
						      maxIOSize,
						      IODMACommand::kMapped,
						      maxIOSize,
						      allignment,
						      mapper);
	if (fDMACommand == NULL)
		goto FailedToAllocDMACommand;

	return kIOReturnSuccess;

FailedToAllocDMACommand:
	IOLockFree(fSyncLock);
FailedtoAllocLock:
	return kIOReturnNoSpace;
}

void IORequest::deinit()
{
	OSSafeReleaseNULL(fDMACommand);
	IOLockFree(fSyncLock);
}

void IORequest::waitForCompletion()
{
	IOReturn retVal = kIOReturnSuccess;

	/* Wait for the request to be completed */
	IOLockLock(fSyncLock);

	if (!fSyncCompleted) {
		IOLockSleep(fSyncLock, this, THREAD_UNINT);
	}

	IOLockUnlock(fSyncLock);

}

void IORequest::signalCompleted(IOReturn status)
{
	/* Wake up sleeping thread */
	IOLockLock(fSyncLock);

	fSyncCompleted = true;
	fSyncStatus = status;
	IOLockWakeup(fSyncLock, this, true);

	IOLockUnlock(fSyncLock);
}

IOReturn IORequest::prepare(IOStorageCompletion *completion, IOMemoryDescriptor *ioBuffer, uint64_t *dmaAddr, uint64_t *dmaSize)
{
	UInt64 offset = 0;
	IODMACommand::Segment64	segment[32];
	UInt32 numOfSegments = 32;
	IOReturn retVal;

	fCompletion = *completion;

	/* The defualt is auto prepared */
	retVal = fDMACommand->setMemoryDescriptor(ioBuffer);
	if (retVal != kIOReturnSuccess)
		goto FailedToSet;

	retVal = fDMACommand->genIOVMSegments(&offset, &segment, &numOfSegments);
	if ((retVal != kIOReturnSuccess))
		goto FailedToGen;

	/* Memory must be mapped contiguosly to the IOVM */
	if (numOfSegments != 1) {
		retVal = kIOReturnInvalid;
		goto IncorrectMapping;
	}

	*dmaAddr = segment[0].fIOVMAddr;
	*dmaSize = segment[0].fLength;

	return kIOReturnSuccess;

IncorrectMapping:
FailedToGen:
	fDMACommand->clearMemoryDescriptor();
FailedToSet:
	return retVal;
}
void IORequest::reset()
{
	fDMACommand->clearMemoryDescriptor();
}

void IORequest::complete(IOReturn status, uint64_t bytesTransfered)
{
	/* The default is autocompleted */
	fDMACommand->clearMemoryDescriptor();

	/* Complete the IO */
	IOStorage::complete(&fCompletion, status, bytesTransfered);

}

IOReturn IORequest::prepareToWait()
{
	fSyncCompleted = false;
	
	return kIOReturnSuccess;
}

IODirection IORequest::getDirection()
{
	return fDMACommand->getMemoryDescriptor()->getDirection();
}
