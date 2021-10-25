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

#ifndef _IOREQUESTSPOOL_H
#define _IOREQUESTSPOOL_H

#include <kern/queue.h>
#include <IOKit/IOLocks.h>

class IORequest;
class IOMapper;
class IORequestsPool {
public:
	IOReturn init(uint32_t numOfRequests, uint32_t maxIOSize, uint8_t numOfAddressBits, uint32_t allignment, IOMapper *mapper);
	void deinit();

	IORequest *getReguest();
	void putRequest(IORequest *request);
private:
	/* Protects the list */
	IOLock *fLock;

	/* List of free requests */
	queue_head_t fFreeRequests;
    
    /* waiters */
    uint32_t fNumOfWaiters;
};
#endif /* _IOREQUESTSPOOL_H */
