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
// threading - multi-threading support
//
// Once upon a time, this file provided a system-independent abstraction layer
// for various thread models. These times are long gone, and we might as well
// admit that we're sitting on top of pthreads (plus certain other system facilities).
//
#ifndef _H_THREADING
#define _H_THREADING

#include <security_utilities/utilities.h>
#include <security_utilities/errors.h>
#include <security_utilities/debugging.h>
# include <pthread.h>

#include <security_utilities/threading_internal.h>


namespace Security {


//
// Potentially, debug-logging all Mutex activity can really ruin your
// performance day. We take some measures to reduce the impact, but if
// you really can't stomach any overhead, define THREAD_NDEBUG to turn
// (only) thread debug-logging off. NDEBUG will turn this on automatically.
// On the other hand, throwing out all debug code will change the ABI of
// Mutexi in incompatible ways. Thus, we still generate the debug-style out-of-line
// code even with THREAD_NDEBUG, so that debug-style code will work with us.
// If you want to ditch it completely, #define THREAD_CLEAN_NDEBUG.
//
#if defined(NDEBUG) || defined(THREAD_CLEAN_NDEBUG)
# if !defined(THREAD_NDEBUG)
#  define THREAD_NDEBUG
# endif
#endif


//
// An abstraction of a per-thread untyped storage slot of pointer size.
// Do not use this in ordinary code; this is for implementing other primitives only.
// Use a PerThreadPointer or ThreadNexus.
//
class ThreadStoreSlot {
public:
	typedef void Destructor(void *);
    ThreadStoreSlot(Destructor *destructor = NULL);
    ~ThreadStoreSlot();

    void *get() const			{ return pthread_getspecific(mKey); }
    operator void * () const	{ return get(); }
    void operator = (void *value) const
    {
        if (int err = pthread_setspecific(mKey, value))
            UnixError::throwMe(err);
    }

private:
    pthread_key_t mKey;
};


//
// Per-thread pointers are implemented using the pthread TLS (thread local storage)
// facility.
// Let's be clear on what gets destroyed when, here. Following the pthread lead,
// when a thread dies its PerThreadPointer object(s) are properly destroyed.
// However, if a PerThreadPointer itself is destroyed, NOTHING HAPPENS. Yes, there are
// reasons for this. This is not (on its face) a bug, so don't yell. But be aware...
//
template <class T>
class PerThreadPointer : public ThreadStoreSlot {
public:
	PerThreadPointer(bool cleanup = true) : ThreadStoreSlot(cleanup ? destructor : NULL) { }
	operator bool() const		{ return get() != NULL; }
	operator T * () const		{ return reinterpret_cast<T *>(get()); }
    T *operator -> () const		{ return static_cast<T *>(*this); }
    T &operator * () const		{ return *static_cast<T *>(get()); }
    void operator = (T *t)		{ ThreadStoreSlot::operator = (t); }
	
private:
	static void destructor(void *element)
	{ delete reinterpret_cast<T *>(element); }
};


//
// Pthread Synchronization primitives.
// These have a common header, strictly for our convenience.
//
class LockingPrimitive {
protected:
	LockingPrimitive() { }
	
    void check(int err)	{ if (err) UnixError::throwMe(err); }
};


//
// Mutexi
//
class Mutex : public LockingPrimitive {
    NOCOPY(Mutex)
    friend class Condition;

public:
	enum Type {
		normal,
		recursive
	};
	
    Mutex();							// normal
	Mutex(Type type);					// recursive
	~Mutex();							// destroy (must be unlocked)
    void lock();						// lock and wait
	bool tryLock();						// instantaneous lock (return false if busy)
    void unlock();						// unlock (must be locked)

private:
    pthread_mutex_t me;
};


class RecursiveMutex : public Mutex
{
public:
	RecursiveMutex() : Mutex(recursive) {}
	~RecursiveMutex() {}
};

//
// Condition variables
//
class Condition : public LockingPrimitive {
    NOCOPY(Condition)

public:	
    Condition(Mutex &mutex);			// create with specific Mutex
	~Condition();
    void wait();						// wait for signal
	void signal();						// signal one
    void broadcast();					// signal all

    Mutex &mutex;						// associated Mutex
	
private:
    pthread_cond_t me;
};


//
// A CountingMutex adds a counter to a Mutex.
// NOTE: This is not officially a semaphore - it's an automatically managed
// counter married to a Mutex.
//
class CountingMutex : public Mutex {
public:
    CountingMutex() : mCount(0) { }
    ~CountingMutex() { assert(mCount == 0); }

    void enter();						// lock, add one, unlock
    bool tryEnter();					// enter or return false
    void exit();						// lock, subtract one, unlock

    // these methods do not lock - use only while you hold the lock
    unsigned int count() const { return mCount; }
    bool isIdle() const { return mCount == 0; }

    // convert Mutex lock to CountingMutex enter/exit. Expert use only
    void finishEnter();					// all but the initial lock
	void finishExit();					// all but the initial lock
   
private:
    unsigned int mCount;				// counter level
};
 

//
// A guaranteed-unlocker stack-based class.
// By default, this will use lock/unlock methods, but you can provide your own
// alternates (to, e.g., use enter/exit, or some more specialized pair of operations).
//
// NOTE: StLock itself is not thread-safe. It is intended for use (usually on the stack)
// by a single thread.
//
template <class Lock,
	void (Lock::*_lock)() = &Lock::lock,
	void (Lock::*_unlock)() = &Lock::unlock>
class StLock {
public:
	StLock(Lock &lck) : me(lck)			{ (me.*_lock)(); mActive = true; }
	StLock(Lock &lck, bool option) : me(lck), mActive(option) { }
	~StLock()							{ if (mActive) (me.*_unlock)(); }

	bool isActive() const				{ return mActive; }
	void lock()							{ if(!mActive) { (me.*_lock)(); mActive = true; }}
	void unlock()						{ if(mActive) { (me.*_unlock)(); mActive = false; }}
	void release()						{ assert(mActive); mActive = false; }

	operator const Lock &() const		{ return me; }
	
protected:
	Lock &me;
	bool mActive;
};

template <class TakeLock, class ReleaseLock,
	void (TakeLock::*_lock)() = &TakeLock::lock,
	void (TakeLock::*_unlock)() = &TakeLock::unlock,
	void (ReleaseLock::*_rlock)() = &ReleaseLock::lock,
	void (ReleaseLock::*_runlock)() = &ReleaseLock::unlock>
class StSyncLock {
public:
    StSyncLock(TakeLock &tlck, ReleaseLock &rlck) : taken(tlck), released(rlck) { 
		(released.*_unlock)(); 
		(taken.*_lock)(); 
		mActive = true; 
	}
    StSyncLock(TakeLock &tlck, ReleaseLock &rlck, bool option) : taken(tlck), released(rlck), mActive(option) { }
    ~StSyncLock()						{ if (mActive) { (taken.*_unlock)(); (released.*_rlock)(); }}
	
	bool isActive() const				{ return mActive; }
	void lock()							{ if(!mActive) { (released.*_runlock)(); (taken.*_lock)(); mActive = true; }}
	void unlock()						{ if(mActive) { (taken.*_unlock)(); (released.*_rlock)(); mActive = false; }}
	void release()						{ assert(mActive); mActive = false; }
	
protected:
    TakeLock &taken;
    ReleaseLock &released;
    bool mActive;
};


//
// Atomic increment/decrement operations.
// The default implementation uses a Mutex. However, many architectures can do
// much better than that.
// Be very clear on the nature of AtomicCounter. It implies no memory barriers of
// any kind. This means that (1) you cannot protect any other memory region with it
// (use a Mutex for that), and (2) it may not enforce cross-processor ordering, which
// means that you have no guarantee that you'll see modifications by other processors
// made earlier (unless another mechanism provides the memory barrier).
// On the other hand, if your compiler has brains, this is blindingly fast...
//
template <class Integer = uint32_t>
class StaticAtomicCounter {
protected:
	Integer mValue;
	
public:
    operator Integer() const	{ return mValue; }

    // infix versions (primary)
    Integer operator ++ ()		{ return Atomic<Integer>::increment(mValue); }
    Integer operator -- ()		{ return Atomic<Integer>::decrement(mValue); }
    
    // postfix versions
    Integer operator ++ (int)	{ return Atomic<Integer>::increment(mValue) - 1; }
    Integer operator -- (int)	{ return Atomic<Integer>::decrement(mValue) + 1; }

    // generic offset
    Integer operator += (int delta) { return Atomic<Integer>::add(delta, mValue); }
};


template <class Integer = int>
class AtomicCounter : public StaticAtomicCounter<Integer> {
public:
    AtomicCounter(Integer init = 0)	{ StaticAtomicCounter<Integer>::mValue = init; }
};


//
// A class implementing a separate thread of execution.
// Do not expect many high-level semantics to be portable. If you can,
// restrict yourself to expect parallel execution and little else.
//
class Thread {
    NOCOPY(Thread)
public:
    class Identity {
        friend class Thread;
        
        Identity(pthread_t id) : mIdent(id) { }
    public:
        Identity() { }
        
        static Identity current()	{ return pthread_self(); }

        bool operator == (const Identity &other) const
        { return pthread_equal(mIdent, other.mIdent); }
        
        bool operator != (const Identity &other) const
        { return !(*this == other); }
    
    private:
        pthread_t mIdent;
    };

public:
    Thread() { }				// constructor
    virtual ~Thread();	 		// virtual destructor
    void run();					// begin running the thread
    
public:
	static void yield();		// unstructured short-term processor yield
    
protected:
    virtual void action() = 0; 	// the action to be performed

private:
    Identity self;				// my own identity (instance constant)

    static void *runner(void *); // argument to pthread_create
};


//
// A "just run this function in a thread" variant of Thread
//
class ThreadRunner : public Thread {
    typedef void Action();
public:
    ThreadRunner(Action *todo);

private:
    void action();
    Action *mAction;
};


} // end namespace Security

#endif //_H_THREADING
