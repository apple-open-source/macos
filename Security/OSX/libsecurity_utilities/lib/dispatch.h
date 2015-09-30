/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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

//
// dispatch - libdispatch wrapper
//
#ifndef _H_DISPATCH
#define _H_DISPATCH

#include <dispatch/dispatch.h>
#include <security_utilities/utilities.h>
#include <security_utilities/threading.h>

#include <exception>

namespace Security {
namespace Dispatch {


// Wraps dispatch objects which can be used to queue blocks, i.e. dispatch groups and queues.
// If a block throws an exception, no further blocks are enqueued and the exception is rethrown
// after waiting for completion of all blocks.
class ExceptionAwareEnqueuing {
	NOCOPY(ExceptionAwareEnqueuing)
public:
	ExceptionAwareEnqueuing();

	void enqueueWithDispatcher(void (^dispatcher)(dispatch_block_t), dispatch_block_t block);
	void throwPendingException();
private:
	Mutex mLock;
	bool mExceptionPending;
	std::exception_ptr mException;
};


class Queue {
	NOCOPY(Queue)
public:
	Queue(const char *label, bool concurrent, dispatch_qos_class_t qos_class = QOS_CLASS_DEFAULT);
	virtual ~Queue();

	operator dispatch_queue_t () const { return mQueue; }

	void enqueue(dispatch_block_t block);
	void wait();

private:
	ExceptionAwareEnqueuing enqueuing;
	dispatch_queue_t mQueue;
};


class Group {
	NOCOPY(Group)
public:
	Group();
	virtual ~Group();

	operator dispatch_group_t () const { return mGroup; }

	void enqueue(dispatch_queue_t queue, dispatch_block_t block);
	void wait();

private:
	ExceptionAwareEnqueuing enqueuing;
	dispatch_group_t mGroup;
};


class Semaphore {
	NOCOPY(Semaphore)
public:
	Semaphore(long count);
	Semaphore(Semaphore& semaphore);
	virtual ~Semaphore();

	operator dispatch_semaphore_t () const { return mSemaphore; };

	bool signal();
	bool wait(dispatch_time_t timeout = DISPATCH_TIME_FOREVER);

private:
	dispatch_semaphore_t mSemaphore;
};


class SemaphoreWait {
	NOCOPY(SemaphoreWait)
public:
	SemaphoreWait(SemaphoreWait& originalWait);
	SemaphoreWait(Semaphore& semaphore, dispatch_time_t timeout = DISPATCH_TIME_FOREVER);
	virtual ~SemaphoreWait();

	bool acquired() const { return mAcquired; };

private:
	Semaphore &mSemaphore;
	bool mAcquired;
};


} // end namespace Dispatch
} // end namespace Security

#endif // !_H_DISPATCH
