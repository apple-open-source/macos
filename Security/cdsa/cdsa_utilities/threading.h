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
#ifndef _H_THREADING
#define _H_THREADING

#include <Security/utilities.h>
#include <Security/debugging.h>

#if _USE_THREADS == _USE_PTHREADS
# include <pthread.h>
#endif

#include <Security/threading_internal.h>


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
#if _USE_THREADS == _USE_PTHREADS

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

#endif //_USE_PTHREADS


//
// Per-thread pointers are patterned after the pthread TLS (thread local storage)
// facility.
// Let's be clear on what gets destroyed when, here. Following the pthread lead,
// when a thread dies its PerThreadPointer object(s) are properly destroyed.
// However, if a PerThreadPointer itself is destroyed, NOTHING HAPPENS. Yes, there are
// reasons for this. This is not (on its face) a bug, so don't yell. But be aware...
//
#if _USE_THREADS == _USE_PTHREADS

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

#elif _USE_THREADS == _USE_NO_THREADS

template <class T>
class PerThreadPointer {
public:
	PerThreadPointer(bool cleanup = true) : mCleanup(cleanup) { }
    ~PerThreadPointer()			{ /* no cleanup - see comment above */ }
	operator bool() const		{ return mValue != NULL; }
	operator T * () const		{ return mValue; }
    T *operator -> () const		{ return mValue; }
    T &operator * () const		{ assert(mValue); return *mValue; }
    void operator = (T *t)		{ mValue = t; }

private:
    T *mValue;
    bool mCleanup;
};

#else
# error Unsupported threading model
#endif //_USE_THREADS


//
// Basic Mutex operations.
// This will be some as-cheap-as-feasible locking primitive that only
// controls one bit (locked/unlocked), plus whatever you contractually
// put under its control.
//
#if _USE_THREADS == _USE_PTHREADS

class Mutex {
    NOCOPY(Mutex)
    
    void check(int err)	{ if (err) UnixError::throwMe(err); }

public:
#if defined(THREAD_NDEBUG) && !defined(THREAD_MAKE_STUBS)
	Mutex(bool = true)	{ check(pthread_mutex_init(&me, NULL)); }
    void lock()			{ check(pthread_mutex_lock(&me)); }
	bool tryLock() {
		if (int err = pthread_mutex_trylock(&me))
			if (err == EBUSY) return false; else UnixError::throwMe(err);
		else return true;
	}
	void unlock()		{ check(pthread_mutex_unlock(&me)); }
    ~Mutex()			{ check(pthread_mutex_destroy(&me)); }
#else //THREAD_NDEBUG
    Mutex(bool log = true);
	~Mutex();
    void lock();
	bool tryLock();
    void unlock();
#endif //THREAD_NDEBUG

private:
    pthread_mutex_t me;
	
#if !defined(THREAD_CLEAN_NDEBUG)
	bool debugLog;						// log *this* mutex
	unsigned long useCount;				// number of locks succeeded
	unsigned long contentionCount;		// number of contentions (valid only if debugLog)
	static bool debugHasInitialized;	// global: debug state set up
	static bool loggingMutexi;			// global: we are debug-logging mutexi
#endif //THREAD_CLEAN_NDEBUG
};

#elif _USE_THREADS == _USE_NO_THREADS

class Mutex {
public:
    void lock(bool = true) { }
    void unlock() { }
    bool tryLock() { return true; }
};

#else
# error Unsupported threading model
#endif //_USE_THREADS


//
// A CountingMutex adds a counter to a Mutex.
// NOTE: This is not officially a semaphore, even if it happens to be implemented with
//  one on some platforms.
//
class CountingMutex : public Mutex {
    // note that this implementation works for any system implementing Mutex *somehow*
public:
    CountingMutex() : mCount(0) { }
    ~CountingMutex() { assert(mCount == 0); }

    void enter();
    bool tryEnter();
    void exit();

    // these methods do not lock - use only while you hold the lock
    unsigned int count() const { return mCount; }
    bool isIdle() const { return mCount == 0; }

    // convert Mutex lock to CountingMutex enter/exit. Expert use only
    void finishEnter();
	void finishExit();
   
private:
    unsigned int mCount;
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
template <class Integer = int>
class StaticAtomicCounter {
protected:

#if defined(_HAVE_ATOMIC_OPERATIONS)
    AtomicWord mValue;
public:
    operator Integer() const	{ return mValue; }

    // infix versions (primary)
    Integer operator ++ ()		{ return atomicIncrement(mValue); }
    Integer operator -- ()		{ return atomicDecrement(mValue); }
    
    // postfix versions
    Integer operator ++ (int)	{ return atomicIncrement(mValue) - 1; }
    Integer operator -- (int)	{ return atomicDecrement(mValue) + 1; }

    // generic offset
    Integer operator += (int delta) { return atomicOffset(mValue, delta); }

#else // no atomic integers, use locks

    Integer mValue;
    mutable Mutex mLock;
public:
    StaticAtomicCounter(Integer init = 0) : mValue(init), mLock(false) { }
    operator Integer() const	{ StLock<Mutex> _(mLock); return mValue; }
    Integer operator ++ ()		{ StLock<Mutex> _(mLock); return ++mValue; }
    Integer operator -- ()		{ StLock<Mutex> _(mLock); return --mValue; }
    Integer operator ++ (int)	{ StLock<Mutex> _(mLock); return mValue++; }
    Integer operator -- (int)	{ StLock<Mutex> _(mLock); return mValue--; }
    Integer operator += (int delta) { StLock<Mutex> _(mLock); return mValue += delta; }
#endif
};


template <class Integer = int>
class AtomicCounter : public StaticAtomicCounter<Integer> {
public:
    AtomicCounter(Integer init = 0)	{ mValue = 0; }
};


//
// A class implementing a separate thread of execution.
// Do not expect many high-level semantics to be portable. If you can,
// restrict yourself to expect parallel execution and little else.
//
#if _USE_THREADS == _USE_PTHREADS

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
        
#if !defined(NDEBUG)
        static const int idLength = 10;
        static void getIdString(char id[idLength]);
#endif //NDEBUG
    
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

#elif _USE_THREADS == _USE_NO_THREADS

class Thread {
    NOCOPY(Thread)
public:
	Thread() { }				// constructor
    virtual ~Thread() { }		// virtual destructor
    void run() { action(); }	// just synchronously run the action
    
public:
    class Identity {
    public:
        static Identity current() { return Identity(); }
	
        bool operator == (const Identity &) const		{ return true; }	// all the same
        bool operator != (const Identity &) const		{ return false; }

#if !defined(NDEBUG)
        static const idLength = 9;
        static void getIdString(char id[idLength]) { memcpy(id, "nothread", idLength); }
#endif
        
    private:
        Identity() { }
    };
	
public:
	void yield() { assert(false); }

protected:
    virtual void action() = 0;	// implement action of thread
};

#else
# error Unsupported threading model
#endif


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


//
// A NestingMutex allows recursive re-entry by the same thread.
// Some pthread implementations support this through a mutex attribute.
// OSX's doesn't, naturally. This implementation works on all pthread platforms.
//
class NestingMutex {
public:
    NestingMutex();
    
    void lock();
    bool tryLock();
    void unlock();

private:
    Mutex mLock;
    Mutex mWait;
    Thread::Identity mIdent;
    uint32 mCount;
};

} // end namespace Security

#endif //_H_THREADING
