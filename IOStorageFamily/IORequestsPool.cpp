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

#include "IORequestsPool.h"
#include "IORequest.h"
#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>

IOReturn IORequestsPool::init(uint32_t numOfRequests, uint32_t maxIOSize, uint8_t numOfAddressBits, uint32_t allignment, IOMapper *mapper)
{
	IOReturn retVal;
	IORequest *newRequest;

	/* Init requests list */
	fLock = IOLockAlloc();
	if (fLock == NULL)
		return kIOReturnNoSpace;

	queue_init(&fFreeRequests);

	for (uint32_t i = 0; i < numOfRequests; i++) {

		newRequest = IONewZero(class IORequest, 1);
		if (newRequest == NULL) {
			retVal = kIOReturnNoSpace;
			goto FailedToAlloc;
		}

		retVal = newRequest->init(i, maxIOSize, numOfAddressBits, allignment, mapper);
		if ( retVal != kIOReturnSuccess)
			goto FailedToInit;

		queue_enter_first(&fFreeRequests, newRequest, class IORequest *, fRequests);
	}
	
    fNumOfWaiters = 0;

	return kIOReturnSuccess;

FailedToInit:
	/* The request isn't in the list and not initalized */
	IODelete(newRequest, class IORequest *, 1);
FailedToAlloc:
	/* Remove all requests from the list and deinit them */
	deinit();
	return retVal;
}

void IORequestsPool::deinit()
{
	IORequest *request;

	while(!queue_empty(&fFreeRequests)) {
		queue_remove_first(&fFreeRequests, request, class IORequest *, fRequests);
		request->deinit();
		IODelete(request, class IORequest *, 1);
	}
	
	IOLockFree(fLock);

}

IORequest *IORequestsPool::getReguest()
{
	IORequest *request = NULL;
	
	IOLockLock(fLock);

	/* If the pool is empty - wait for some request will be returned */
    while (queue_empty(&fFreeRequests)) {
        fNumOfWaiters++;
		IOLockSleep(fLock, &fFreeRequests, THREAD_UNINT);
        fNumOfWaiters--;
    }

	queue_remove_first(&fFreeRequests, request, class IORequest *, fRequests);

	IOLockUnlock(fLock);
	
	return request;
}

void IORequestsPool::putRequest(IORequest *request)
{

	IOLockLock(fLock);

	/* If some thread is waiting - wake it up */
	if (fNumOfWaiters > 0)
		IOLockWakeup(fLock, &fFreeRequests, true);

	queue_enter_first(&fFreeRequests, request, class IORequest *, fRequests);

	IOLockUnlock(fLock);
}
