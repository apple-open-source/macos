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

#ifndef _IOREQUEST_H
#define _IOREQUEST_H

#include <IOKit/storage/IOStorage.h>
#include <kern/queue.h>
#include <IOKit/IOLocks.h>

class IOMemoryDescriptor;
class IODMACommand;
class IOMapper;

class IORequest {
public:
	IOReturn init(uint32_t index, uint32_t maxIOSize, uint8_t numOfAddressBits, uint32_t allignment, IOMapper *mapper);
	void deinit();

	IOReturn prepare(IOStorageCompletion *completion, IOMemoryDescriptor *ioBuffer, uint64_t *dmaAddr, uint64_t *dmaSize);
	void complete(IOReturn status, uint64_t bytesTransfered);
	void reset();

	void waitForCompletion();
	void signalCompleted(IOReturn status);
	IOReturn prepareToWait();

	IODirection getDirection();
	IOReturn getStatus() { return fSyncStatus; }

	uint32_t getIndex() { return fIndex; }

	void setIORequest(bool isIORequest) { fIsIORequest = isIORequest; }
	bool getIORequest() { return fIsIORequest; }
	
	void setBytesToTransfer(uint64_t bytestoTransfer) {fBytestoTransfer = bytestoTransfer; }
	uint64_t getBytesToTransfer() { return fBytestoTransfer; };
	queue_chain_t fRequests;

private:

	IODMACommand *fDMACommand;

	uint32_t fIndex;
	bool fIsIORequest;
	uint64_t fBytestoTransfer;
	IOStorageCompletion fCompletion;
	IOLock *fSyncLock;
	bool fSyncCompleted;
	IOReturn fSyncStatus;
};

#endif /* _IOREQUEST_H */
