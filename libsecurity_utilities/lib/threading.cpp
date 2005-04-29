/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// threading - generic thread support
//
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/memutils.h>


//
// Thread-local storage primitive
//
ThreadStoreSlot::ThreadStoreSlot(Destructor *destructor)
{
    if (int err = pthread_key_create(&mKey, destructor))
        UnixError::throwMe(err);
}

ThreadStoreSlot::~ThreadStoreSlot()
{
    //@@@ if we wanted to dispose of pending task objects, we'd have
    //@@@ to keep a set of them and delete them explicitly here
    pthread_key_delete(mKey);
}


//
// Common locking primitive handling
//
bool LockingPrimitive::debugHasInitialized;
bool LockingPrimitive::loggingMutexi;

inline void LockingPrimitive::init(bool log)
{
#if !defined(THREAD_NDEBUG)
	// this debug-setup code isn't interlocked, but it's idempotent
	// (don't worry, be happy)
	if (!debugHasInitialized) {
		loggingMutexi = Debug::debugging("mutex") || Debug::debugging("mutex-c");
		debugHasInitialized = true;
	}
	debugLog = log && loggingMutexi;
#else
    debugLog = false;
#endif //THREAD_NDEBUG
}


//
// Mutex implementation
//
struct MutexAttributes {
	pthread_mutexattr_t recursive;
	pthread_mutexattr_t checking;
	
	MutexAttributes()
	{
		pthread_mutexattr_init(&recursive);
		pthread_mutexattr_settype(&recursive, PTHREAD_MUTEX_RECURSIVE);
#if !defined(NDEBUG)
		pthread_mutexattr_init(&checking);
		pthread_mutexattr_settype(&checking, PTHREAD_MUTEX_ERRORCHECK);
#endif //NDEBUG
	}
};

static ModuleNexus<MutexAttributes> mutexAttrs;


Mutex::Mutex(bool log)
{
	init(log);
	mUseCount = mContentionCount = 0;
	check(pthread_mutex_init(&me, NULL));
}

Mutex::Mutex(Type type, bool log)
{
	init(log);
	mUseCount = mContentionCount = 0;
	switch (type) {
	case normal:
#if defined(NDEBUG)		// deployment version - normal mutex
		check(pthread_mutex_init(&me, NULL));
#else					// debug version - checking mutex
		check(pthread_mutex_init(&me, &mutexAttrs().checking));
#endif //NDEBUG
		break;
	case recursive:		// requested recursive (is also checking, always)
		check(pthread_mutex_init(&me, &mutexAttrs().recursive));
		break;
	};
}


Mutex::~Mutex()
{
#if !defined(THREAD_NDEBUG)
	if (debugLog) {
		if (mContentionCount > 0)
			secdebug("mutex-c", "%p destroyed after %ld/%ld locks/contentions",
					 this, mUseCount, mContentionCount);
		else if (mUseCount > 100)
			secdebug("mutex", "%p destroyed after %ld locks", this, mUseCount);
	}
#endif //THREAD_NDEBUG
	check(pthread_mutex_destroy(&me));
}


void Mutex::lock()
{
#if !defined(THREAD_NDEBUG)
	mUseCount++;
	if (debugLog) {
		switch (int err = pthread_mutex_trylock(&me)) {
		case 0:
			break;
		case EBUSY:
			if (debugLog)
				secdebug("mutex-c", "%p contended (%ld of %ld)", this, ++mContentionCount, mUseCount);
			check(pthread_mutex_lock(&me));
			break;
		default:
			UnixError::throwMe(err);
		}
		if (mUseCount % 100 == 0)
			secdebug("mutex", "%p locked %ld", this, mUseCount);
		else
			secdebug("mutex", "%p locked", this);
        return;
    }
#endif //THREAD_NDEBUG
	check(pthread_mutex_lock(&me));
}


bool Mutex::tryLock()
{
	mUseCount++;
	if (int err = pthread_mutex_trylock(&me)) {
		if (err != EBUSY)
			UnixError::throwMe(err);
#if !defined(THREAD_NDEBUG)
		if (debugLog)
			secdebug("mutex-c", "%p trylock contended (%ld of %ld)",
				this, ++mContentionCount, mUseCount);
#endif //THREAD_NDEBUG
		return false;
	}
#if !defined(THREAD_NDEBUG)
	if (debugLog)
		if (mUseCount % 100 == 0)
			secdebug("mutex", "%p locked %ld", this, mUseCount);
		else
			secdebug("mutex", "%p locked", this);
#endif //THREAD_NDEBUG
	return true;
}


void Mutex::unlock()
{
#if !defined(MUTEX_NDEBUG)
	if (debugLog)
		secdebug("mutex", "%p unlocked", this);
#endif //MUTEX_NDEBUG
	check(pthread_mutex_unlock(&me));
}


//
// Condition variables
//
Condition::Condition(Mutex &lock) : mutex(lock)
{
	init(true);
    check(pthread_cond_init(&me, NULL));
}

Condition::~Condition()
{
	check(pthread_cond_destroy(&me));
}

void Condition::wait()
{
    check(pthread_cond_wait(&me, &mutex.me));
}

void Condition::signal()
{
    check(pthread_cond_signal(&me));
}

void Condition::broadcast()
{
    check(pthread_cond_broadcast(&me));
}


//
// CountingMutex implementation.
//
void CountingMutex::enter()
{
    lock();
    mCount++;
    secdebug("mutex", "%p up to %d", this, mCount);
    unlock();
}

bool CountingMutex::tryEnter()		
{
    if (!tryLock())
        return false;
    mCount++;
    secdebug("mutex", "%p up to %d (was try)", this, mCount);
    unlock();
    return true;
}

void CountingMutex::exit()
{
    lock();
    assert(mCount > 0);
    mCount--;
    secdebug("mutex", "%p down to %d", this, mCount);
    unlock();
}

void CountingMutex::finishEnter()
{
    mCount++;
    secdebug("mutex", "%p finish up to %d", this, mCount);
    unlock();
}

void CountingMutex::finishExit()
{
    assert(mCount > 0);
    mCount--; 
    secdebug("mutex", "%p finish down to %d", this, mCount);
    unlock();
}



//
// Threads implementation
//
Thread::~Thread()
{
}

void Thread::run()
{
    if (int err = pthread_create(&self.mIdent, NULL, runner, this))
        UnixError::throwMe(err);
	secdebug("thread", "%p created", self.mIdent);
}

void *Thread::runner(void *arg)
{
    Thread *me = static_cast<Thread *>(arg);
    if (int err = pthread_detach(me->self.mIdent))
        UnixError::throwMe(err);
	secdebug("thread", "%p starting", me->self.mIdent);
    me->action();
	secdebug("thread", "%p terminating", me->self.mIdent);
    delete me;
    return NULL;
}

void Thread::yield()
{
	::sched_yield();
}


//
// ThreadRunner implementation
//
ThreadRunner::ThreadRunner(Action *todo)
{
    mAction = todo;
    run();
}

void ThreadRunner::action()
{
    mAction();
}


//
// Nesting Mutexi.
// This is obsolete; use Mutex(Mutex::recursive).
//
NestingMutex::NestingMutex() : mCount(0)
{ }

void NestingMutex::lock()
{
    while (!tryLock()) {
        mWait.lock();
        mWait.unlock();
    }
}

bool NestingMutex::tryLock()
{
    StLock<Mutex> _(mLock);
    if (mCount == 0) {	// initial lock
        mCount = 1;
        mIdent = Thread::Identity::current();
        mWait.lock();
        return true;
    } else if (mIdent == Thread::Identity::current()) {	// recursive lock
        mCount++;
        return true;
    } else {	// locked by another thread
        return false;
    }
}

void NestingMutex::unlock()
{
    StLock<Mutex> _(mLock);
    assert(mCount > 0 && mIdent == Thread::Identity::current());
    if (--mCount == 0)	// last recursive unlock
        mWait.unlock();
}
