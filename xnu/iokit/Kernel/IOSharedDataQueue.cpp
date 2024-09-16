/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#define IOKIT_ENABLE_SHARED_PTR

#include <IOKit/IOSharedDataQueue.h>
#include <IOKit/IODataQueueShared.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/c++/OSSharedPtr.h>

#include <vm/vm_kern_xnu.h>

#ifdef enqueue
#undef enqueue
#endif

#ifdef dequeue
#undef dequeue
#endif

#define super IODataQueue

OSDefineMetaClassAndStructors(IOSharedDataQueue, IODataQueue)

OSSharedPtr<IOSharedDataQueue>
IOSharedDataQueue::withCapacity(UInt32 size)
{
	OSSharedPtr<IOSharedDataQueue> dataQueue = OSMakeShared<IOSharedDataQueue>();

	if (dataQueue) {
		if (!dataQueue->initWithCapacity(size)) {
			return nullptr;
		}
	}

	return dataQueue;
}

OSSharedPtr<IOSharedDataQueue>
IOSharedDataQueue::withEntries(UInt32 numEntries, UInt32 entrySize)
{
	OSSharedPtr<IOSharedDataQueue> dataQueue = OSMakeShared<IOSharedDataQueue>();

	if (dataQueue) {
		if (!dataQueue->initWithEntries(numEntries, entrySize)) {
			return nullptr;
		}
	}

	return dataQueue;
}

Boolean
IOSharedDataQueue::initWithCapacity(UInt32 size)
{
	IODataQueueAppendix *   appendix;
	vm_size_t               allocSize;
	kern_return_t           kr;

	if (!super::init()) {
		return false;
	}

	_reserved = IOMallocType(ExpansionData);
	if (!_reserved) {
		return false;
	}

	if (size > UINT32_MAX - DATA_QUEUE_MEMORY_HEADER_SIZE - DATA_QUEUE_MEMORY_APPENDIX_SIZE) {
		return false;
	}

	allocSize = round_page(size + DATA_QUEUE_MEMORY_HEADER_SIZE + DATA_QUEUE_MEMORY_APPENDIX_SIZE);

	if (allocSize < size) {
		return false;
	}

	kr = kmem_alloc(kernel_map, (vm_offset_t *)&dataQueue, allocSize,
	    (kma_flags_t)(KMA_DATA | KMA_ZERO), IOMemoryTag(kernel_map));
	if (kr != KERN_SUCCESS) {
		return false;
	}

	dataQueue->queueSize    = size;
//  dataQueue->head         = 0;
//  dataQueue->tail         = 0;

	if (!setQueueSize(size)) {
		return false;
	}

	appendix            = (IODataQueueAppendix *)((UInt8 *)dataQueue + size + DATA_QUEUE_MEMORY_HEADER_SIZE);
	appendix->version   = 0;

	if (!notifyMsg) {
		notifyMsg = IOMallocType(mach_msg_header_t);
		if (!notifyMsg) {
			return false;
		}
	}
	bzero(notifyMsg, sizeof(mach_msg_header_t));

	setNotificationPort(MACH_PORT_NULL);

	return true;
}

void
IOSharedDataQueue::free()
{
	if (dataQueue) {
		kmem_free(kernel_map, (vm_offset_t)dataQueue, round_page(getQueueSize() +
		    DATA_QUEUE_MEMORY_HEADER_SIZE + DATA_QUEUE_MEMORY_APPENDIX_SIZE));
		dataQueue = NULL;
		if (notifyMsg) {
			IOFreeType(notifyMsg, mach_msg_header_t);
			notifyMsg = NULL;
		}
	}

	if (_reserved) {
		IOFreeType(_reserved, ExpansionData);
		_reserved = NULL;
	}

	super::free();
}

OSSharedPtr<IOMemoryDescriptor>
IOSharedDataQueue::getMemoryDescriptor()
{
	OSSharedPtr<IOMemoryDescriptor> descriptor;

	if (dataQueue != NULL) {
		descriptor = IOMemoryDescriptor::withAddress(dataQueue, getQueueSize() + DATA_QUEUE_MEMORY_HEADER_SIZE + DATA_QUEUE_MEMORY_APPENDIX_SIZE, kIODirectionOutIn);
	}

	return descriptor;
}


IODataQueueEntry *
IOSharedDataQueue::peek()
{
	IODataQueueEntry *entry      = NULL;
	UInt32            headOffset;
	UInt32            tailOffset;

	if (!dataQueue) {
		return NULL;
	}

	// Read head and tail with acquire barrier
	// See rdar://problem/40780584 for an explanation of relaxed/acquire barriers
	headOffset = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_RELAXED);
	tailOffset = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->tail, __ATOMIC_ACQUIRE);

	if (headOffset != tailOffset) {
		volatile IODataQueueEntry * head = NULL;
		UInt32              headSize     = 0;
		UInt32              headOffset   = dataQueue->head;
		UInt32              queueSize    = getQueueSize();

		if (headOffset > queueSize) {
			return NULL;
		}

		head         = (IODataQueueEntry *)((char *)dataQueue->queue + headOffset);
		headSize     = head->size;

		// Check if there's enough room before the end of the queue for a header.
		// If there is room, check if there's enough room to hold the header and
		// the data.

		if ((headOffset > UINT32_MAX - DATA_QUEUE_ENTRY_HEADER_SIZE) ||
		    (headOffset + DATA_QUEUE_ENTRY_HEADER_SIZE > queueSize) ||
		    (headOffset + DATA_QUEUE_ENTRY_HEADER_SIZE > UINT32_MAX - headSize) ||
		    (headOffset + headSize + DATA_QUEUE_ENTRY_HEADER_SIZE > queueSize)) {
			// No room for the header or the data, wrap to the beginning of the queue.
			// Note: wrapping even with the UINT32_MAX checks, as we have to support
			// queueSize of UINT32_MAX
			entry = dataQueue->queue;
		} else {
			entry = (IODataQueueEntry *)head;
		}
	}

	return entry;
}

Boolean
IOSharedDataQueue::enqueue(void * data, UInt32 dataSize)
{
	UInt32             head;
	UInt32             tail;
	UInt32             newTail;
	const UInt32       entrySize = dataSize + DATA_QUEUE_ENTRY_HEADER_SIZE;
	IODataQueueEntry * entry;

	// Force a single read of head and tail
	// See rdar://problem/40780584 for an explanation of relaxed/acquire barriers
	tail = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->tail, __ATOMIC_RELAXED);
	head = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_ACQUIRE);

	// Check for overflow of entrySize
	if (dataSize > UINT32_MAX - DATA_QUEUE_ENTRY_HEADER_SIZE) {
		return false;
	}
	// Check for underflow of (getQueueSize() - tail)
	if (getQueueSize() < tail || getQueueSize() < head) {
		return false;
	}

	if (tail >= head) {
		// Is there enough room at the end for the entry?
		if ((entrySize <= UINT32_MAX - tail) &&
		    ((tail + entrySize) <= getQueueSize())) {
			entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

			entry->size = dataSize;
			__nochk_memcpy(&entry->data, data, dataSize);

			// The tail can be out of bound when the size of the new entry
			// exactly matches the available space at the end of the queue.
			// The tail can range from 0 to dataQueue->queueSize inclusive.

			newTail = tail + entrySize;
		} else if (head > entrySize) { // Is there enough room at the beginning?
			// Wrap around to the beginning, but do not allow the tail to catch
			// up to the head.

			dataQueue->queue->size = dataSize;

			// We need to make sure that there is enough room to set the size before
			// doing this. The user client checks for this and will look for the size
			// at the beginning if there isn't room for it at the end.

			if ((getQueueSize() - tail) >= DATA_QUEUE_ENTRY_HEADER_SIZE) {
				((IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail))->size = dataSize;
			}

			__nochk_memcpy(&dataQueue->queue->data, data, dataSize);
			newTail = entrySize;
		} else {
			return false; // queue is full
		}
	} else {
		// Do not allow the tail to catch up to the head when the queue is full.
		// That's why the comparison uses a '>' rather than '>='.

		if ((head - tail) > entrySize) {
			entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

			entry->size = dataSize;
			__nochk_memcpy(&entry->data, data, dataSize);
			newTail = tail + entrySize;
		} else {
			return false; // queue is full
		}
	}

	// Publish the data we just enqueued
	__c11_atomic_store((_Atomic UInt32 *)&dataQueue->tail, newTail, __ATOMIC_RELEASE);

	if (tail != head) {
		//
		// The memory barrier below paris with the one in ::dequeue
		// so that either our store to the tail cannot be missed by
		// the next dequeue attempt, or we will observe the dequeuer
		// making the queue empty.
		//
		// Of course, if we already think the queue is empty,
		// there's no point paying this extra cost.
		//
		__c11_atomic_thread_fence(__ATOMIC_SEQ_CST);
		head = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_RELAXED);
	}

	if (tail == head) {
		// Send notification (via mach message) that data is now available.
		sendDataAvailableNotification();
	}
	return true;
}

Boolean
IOSharedDataQueue::dequeue(void *data, UInt32 *dataSize)
{
	Boolean             retVal          = TRUE;
	volatile IODataQueueEntry *  entry  = NULL;
	UInt32              entrySize       = 0;
	UInt32              headOffset      = 0;
	UInt32              tailOffset      = 0;
	UInt32              newHeadOffset   = 0;

	if (!dataQueue || (data && !dataSize)) {
		return false;
	}

	// Read head and tail with acquire barrier
	// See rdar://problem/40780584 for an explanation of relaxed/acquire barriers
	headOffset = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_RELAXED);
	tailOffset = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->tail, __ATOMIC_ACQUIRE);

	if (headOffset != tailOffset) {
		volatile IODataQueueEntry * head = NULL;
		UInt32              headSize     = 0;
		UInt32              queueSize    = getQueueSize();

		if (headOffset > queueSize) {
			return false;
		}

		head         = (IODataQueueEntry *)((char *)dataQueue->queue + headOffset);
		headSize     = head->size;

		// we wrapped around to beginning, so read from there
		// either there was not even room for the header
		if ((headOffset > UINT32_MAX - DATA_QUEUE_ENTRY_HEADER_SIZE) ||
		    (headOffset + DATA_QUEUE_ENTRY_HEADER_SIZE > queueSize) ||
		    // or there was room for the header, but not for the data
		    (headOffset + DATA_QUEUE_ENTRY_HEADER_SIZE > UINT32_MAX - headSize) ||
		    (headOffset + headSize + DATA_QUEUE_ENTRY_HEADER_SIZE > queueSize)) {
			// Note: we have to wrap to the beginning even with the UINT32_MAX checks
			// because we have to support a queueSize of UINT32_MAX.
			entry           = dataQueue->queue;
			entrySize       = entry->size;
			if ((entrySize > UINT32_MAX - DATA_QUEUE_ENTRY_HEADER_SIZE) ||
			    (entrySize + DATA_QUEUE_ENTRY_HEADER_SIZE > queueSize)) {
				return false;
			}
			newHeadOffset   = entrySize + DATA_QUEUE_ENTRY_HEADER_SIZE;
			// else it is at the end
		} else {
			entry           = head;
			entrySize       = entry->size;
			if ((entrySize > UINT32_MAX - DATA_QUEUE_ENTRY_HEADER_SIZE) ||
			    (entrySize + DATA_QUEUE_ENTRY_HEADER_SIZE > UINT32_MAX - headOffset) ||
			    (entrySize + DATA_QUEUE_ENTRY_HEADER_SIZE + headOffset > queueSize)) {
				return false;
			}
			newHeadOffset   = headOffset + entrySize + DATA_QUEUE_ENTRY_HEADER_SIZE;
		}
	} else {
		// empty queue
		return false;
	}

	if (data) {
		if (entrySize > *dataSize) {
			// not enough space
			return false;
		}
		__nochk_memcpy(data, (void *)entry->data, entrySize);
		*dataSize = entrySize;
	}

	__c11_atomic_store((_Atomic UInt32 *)&dataQueue->head, newHeadOffset, __ATOMIC_RELEASE);

	if (newHeadOffset == tailOffset) {
		//
		// If we are making the queue empty, then we need to make sure
		// that either the enqueuer notices, or we notice the enqueue
		// that raced with our making of the queue empty.
		//
		__c11_atomic_thread_fence(__ATOMIC_SEQ_CST);
	}

	return retVal;
}

UInt32
IOSharedDataQueue::getQueueSize()
{
	if (!_reserved) {
		return 0;
	}
	return _reserved->queueSize;
}

Boolean
IOSharedDataQueue::setQueueSize(UInt32 size)
{
	if (!_reserved) {
		return false;
	}
	_reserved->queueSize = size;
	return true;
}

OSMetaClassDefineReservedUnused(IOSharedDataQueue, 0);
OSMetaClassDefineReservedUnused(IOSharedDataQueue, 1);
OSMetaClassDefineReservedUnused(IOSharedDataQueue, 2);
OSMetaClassDefineReservedUnused(IOSharedDataQueue, 3);
OSMetaClassDefineReservedUnused(IOSharedDataQueue, 4);
OSMetaClassDefineReservedUnused(IOSharedDataQueue, 5);
OSMetaClassDefineReservedUnused(IOSharedDataQueue, 6);
OSMetaClassDefineReservedUnused(IOSharedDataQueue, 7);
