/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// threading - generic thread support
//


//
// Since we are planning to generate "stub" out of line code for threading methods,
// we must force THREAD_NDEBUG to off while compiling our header. Trust me.
//
#if !defined(THREAD_CLEAN_NDEBUG)
# define THREAD_MAKE_STUBS
#endif
#include <Security/threading.h>


//
// Thread-local storage primitive
//
#if _USE_THREADS == _USE_PTHREADS

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

#endif


//
// Mutex implementation
//
#if _USE_THREADS == _USE_PTHREADS

#if !defined(THREAD_CLEAN_NDEBUG)

bool Mutex::debugHasInitialized;
bool Mutex::loggingMutexi;

Mutex::Mutex(bool log)
{
#if !defined(THREAD_NDEBUG)
	// this debug-setup code isn't interlocked, but it's idempotent
	// (don't worry, be happy)
	if (!debugHasInitialized) {
		loggingMutexi = Debug::debugging("mutex");
		debugHasInitialized = true;
	}
	debugLog = log && loggingMutexi;
    useCount = contentionCount = 0;
#else
    debugLog = false;
#endif //THREAD_NDEBUG    
	check(pthread_mutex_init(&me, NULL));
}

Mutex::~Mutex()
{
#if !defined(THREAD_NDEBUG)
	if (debugLog && (useCount > 100 || contentionCount > 0))
		debug("mutex", "%p destroyed after %ld/%ld locks/contentions", this, useCount, contentionCount);
#endif //THREAD_NDEBUG
	check(pthread_mutex_destroy(&me));
}

void Mutex::lock()
{
#if !defined(THREAD_NDEBUG)
	useCount++;
	if (debugLog) {
		switch (int err = pthread_mutex_trylock(&me)) {
		case 0:
			break;
		case EBUSY:
			if (debugLog)
				debug("mutex", "%p contended (%ld of %ld)", this, ++contentionCount, useCount);
			check(pthread_mutex_lock(&me));
			break;
		default:
			UnixError::throwMe(err);
		}
		if (useCount % 100 == 0)
			debug("mutex", "%p locked %ld", this, useCount);
		else
			debug("mutex", "%p locked", this);
        return;
    }
#endif //THREAD_NDEBUG
	check(pthread_mutex_lock(&me));
}

bool Mutex::tryLock()
{
	useCount++;
	if (int err = pthread_mutex_trylock(&me)) {
		if (err != EBUSY)
			UnixError::throwMe(err);
#if !defined(THREAD_NDEBUG)
		if (debugLog)
			debug("mutex", "%p trylock contended (%ld of %ld)",
				this, ++contentionCount, useCount);
#endif //THREAD_NDEBUG
		return false;
	}
#if !defined(THREAD_NDEBUG)
	if (debugLog)
		if (useCount % 100 == 0)
			debug("mutex", "%p locked %ld", this, useCount);
		else
			debug("mutex", "%p locked", this);
#endif //THREAD_NDEBUG
	return true;
}

void Mutex::unlock()
{
#if !defined(MUTEX_NDEBUG)
	if (debugLog)
		debug("mutex", "%p unlocked", this);
#endif //MUTEX_NDEBUG
	check(pthread_mutex_unlock(&me));
}

#endif //!THREAD_CLEAN_NDEBUG
#endif //PTHREADS


//
// CountingMutex implementation.
// Note that this is a generic implementation based on a specific Mutex type.
// In other words, it should work no matter how Mutex is implemented.
// Also note that CountingMutex is expected to interlock properly with Mutex,
// so you canNOT just use an AtomicCounter here.
//
void CountingMutex::enter()
{
    lock();
    mCount++;
    debug("mutex", "%p up to %d", this, mCount);
    unlock();
}

bool CountingMutex::tryEnter()		
{
    if (!tryLock())
        return false;
    mCount++;
    debug("mutex", "%p up to %d (was try)", this, mCount);
    unlock();
    return true;
}

void CountingMutex::exit()
{
    lock();
    assert(mCount > 0);
    mCount--;
    debug("mutex", "%p down to %d", this, mCount);
    unlock();
}

void CountingMutex::finishEnter()
{
    mCount++;
    debug("mutex", "%p finish up to %d", this, mCount);
    unlock();
}

void CountingMutex::finishExit()
{
    assert(mCount > 0);
    mCount--; 
    debug("mutex", "%p finish down to %d", this, mCount);
    unlock();
}



//
// Threads implementation
//
#if _USE_THREADS == _USE_PTHREADS

Thread::~Thread()
{
}

void Thread::run()
{
    if (int err = pthread_create(&self.mIdent, NULL, runner, this))
        UnixError::throwMe(err);
	debug("thread", "%p created", self.mIdent);
}

void *Thread::runner(void *arg)
{
    Thread *me = static_cast<Thread *>(arg);
    if (int err = pthread_detach(me->self.mIdent))
        UnixError::throwMe(err);
	debug("thread", "%p starting", me->self.mIdent);
    me->action();
	debug("thread", "%p terminating", me->self.mIdent);
    delete me;
    return NULL;
}

void Thread::yield()
{
	sched_yield();
}

#if !defined(NDEBUG)

#include <Security/memutils.h>

void Thread::Identity::getIdString(char id[idLength])
{
	pthread_t current = pthread_self();
	// We're not supposed to know what a pthread_t is. Just print the first few bytes...
	// (On MacOS X, it's a pointer to a pthread_t internal structure, so this works fine.)
	void *p;
	memcpy(&p, &current, sizeof(p));
	snprintf(id, idLength, "%lx", long(p));
}

#endif // NDEBUG

#endif // PTHREADS


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
// This implementation uses mWait as a "sloppy" wait blocker (only).
// It should be a semaphore of course, but we don't have a semaphore
// abstraction right now. The authoritative locking protocol is based on mLock.
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
