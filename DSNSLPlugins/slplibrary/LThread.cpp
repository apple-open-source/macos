/*
	File:		LThread.cpp

	Contains:	Implementation of the LThread abstract base class.
				IMPORTANT:
				* This is an independently derived implementation,
				* OPTIMIZED FOR MAC OS X'S POSIX THREADS,
				* of Metrowerks' PowerPlant Thread classes, which likely
				* makes the class and method names in this header file also
				* copyright Metrowerks.

	Version:	NetInfo Plus 1.0

	Written by:	Michael Dasenbrock

	Copyright:	© 1999 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

	File Ownership:
		DRI:				David M. O'Rourke
		Other Contact:		Michael Dasenbrock
		Technology:			NetInfo Plus

	Writers:

	Change History (most recent first):


	To Do:
*/


// ANSI / POSIX headers
#include <unistd.h>	// for _POSIX_THREADS

// Project Headers
#include "LThread.h"
//#include "CLog.h"

#if !TARGET_OS_UNIX && !TARGET_API_MAC_OSX
#warning "This is implementation is only for Mac OS X!"
#endif	/* !TARGET_OS_UNIX && !TARGET_API_MAC_OSX */


// ----------------------------------------------------------------------------
//	Ÿ LThread Class Globals
// ----------------------------------------------------------------------------

LThread	*LThread::sMainThread;
Boolean	LThread::sInited;

#ifdef _POSIX_THREADS
# define NO_THREAD NULL

static pthread_attr_t	_DefaultAttrs;
static pthread_key_t	_ObjectKey, _NameKey;

#else	/* _POSIX_THREADS */

# define NO_THREAD NO_CTHREAD
static inline ThreadIDT pthread_self (void)
	{ return cthread_self (); }

#endif	/* _POSIX_THREADS */


// ----------------------------------------------------------------------------
//	Ÿ CThread Class (static) Methods
// ----------------------------------------------------------------------------
#pragma mark **** Class Methods ****

// cthread_fork() / pthread_create() callback function.
// This must be a static method so it can access member variables.

void *LThread::_RunWrapper ( void *arg )
{
	LThread		*oMe = (LThread *) arg;
	void		*theResult;
	ThreadIDT	tMe = ::pthread_self ();
	int error, oldtype;

#ifdef _POSIX_THREADS
	::pthread_setspecific (_ObjectKey, oMe);
	::pthread_setspecific (_NameKey, "LThread");
	error = ::pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
//	fprintf(stderr, "pthread_setcanceltype, err = %d, oldtype = %d\n", error, oldtype);
#else	/* _POSIX_THREADS */
	// Make use of the cthread struct fields.
	::cthread_set_name (tMe, "LThread");
	cthread_set_data (tMe, arg);
#endif	/* _POSIX_THREADS */

	oMe->mThread = tMe;

	// Execute the thread.
//	try {
		oMe->mResult = theResult = oMe->Run ();
//	}

//	catch (...)
	{
//		LOG(kLogThreads, "** LThread::_RunWrapper(): catch block");
        oMe->mResult = theResult = (void *) errKilledThread;
	}

	// Notify next of kin.
	if (oMe->mNextOfKin)
		oMe->mNextOfKin->ThreadDied (*oMe);

	// Mark the thread as completed.
	delete oMe;

    return theResult;
}

// Yield to a specific thread.
void LThread::Yield (const LThread *inYieldTo)
{
	// There is no way to yield to a specific thread, so just yield.
//	::cthread_yield ();
}

// Simple thread information.
Boolean LThread::InMainThread (void)
{
	return (sMainThread->mThread == ::pthread_self ());
}
LThread *LThread::GetCurrentThread (void)
{
	if (!sInited)
		return 0;
#ifdef _POSIX_THREADS
	return (LThread *) ::pthread_getspecific (_ObjectKey);
#else	/* _POSIX_THREADS */
	return (LThread *)(cthread_data (cthread_self ()));
#endif	/* _POSIX_THREADS */
}


// ----------------------------------------------------------------------------
//	Ÿ LThread Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** Instance Methods ****

// ----------------------------------------------------------------------------
//	Ÿ Constructor and Destructor
// ----------------------------------------------------------------------------

LThread::LThread ( LThread::EThreadOption inFlags )
	: mThread( NO_THREAD ), mNextOfKin( 0 ), mResult( NULL )
{
	// All ctor arguments are ignored, except for main thread flag.
	if (inFlags & threadOption_Main)
	{
		// Two main threads are not allowed!
/*		if ((sMainThread != NULL) || sInited)
			throw (threadProtocolErr);
*/		
		// Handle class initialization.
		sInited = true;

		// set up this thread to be the main thread
		sMainThread = this;
		mThread = ::pthread_self ();
		mPThreadID = 0;

		// Initialize the OS thread package.
#ifdef _POSIX_THREADS
		::pthread_attr_init (&_DefaultAttrs);
		::pthread_attr_setdetachstate (&_DefaultAttrs, PTHREAD_CREATE_DETACHED);
		::pthread_key_create (&_ObjectKey, NULL);
		::pthread_key_create (&_NameKey, NULL);
		::pthread_setspecific (_ObjectKey, this);
		::pthread_setspecific (_NameKey, "UMainThread");
#else	/* _POSIX_THREADS */
		::cthread_set_name (mThread, "UMainThread");
		cthread_set_data (mThread, (any_t) this);
#endif	/* _POSIX_THREADS */
		return;
	} else if (!sInited) {
		// The first thread must be the main thread.
		//throw (threadProtocolErr);

		::pthread_attr_init (&_DefaultAttrs);
		::pthread_attr_setdetachstate (&_DefaultAttrs, PTHREAD_CREATE_DETACHED);
	}
}

LThread::~LThread (void)
{
//	if (this == sMainThread)
//		throw threadProtocolErr;
	mThread = NO_THREAD;
}

// Start the thread running.
void LThread::Resume (void)
{
	// Throw an exception if the thread is already running.
	if (mThread != NO_THREAD) {
//		throw (errBadThreadState);
		return;
	}

	// Currently detaching so threads don't stick around.
#ifdef _POSIX_THREADS
	int error;
	
    error  = ::pthread_create(&mPThreadID, &_DefaultAttrs, _RunWrapper, (void *) this);
#else	/* _POSIX_THREADS */
	::cthread_detach (::cthread_fork (_RunWrapper, (void *) this));
#endif	/* _POSIX_THREADS */
}


// Is this the thread that's executing?
Boolean LThread::IsCurrent (void) const
{
	return (mThread == ::pthread_self ());
}

// As a debugging convenience, throw an exception.
void *LThread::Run (void)
{
//	LOG(kLogThreads, "** LThread::Run() - This should be non-reached.!!! **");
//	throw threadProtocolErr;
	return NULL;
}


// As a debugging convenience, throw an exception.
void LThread::ThreadDied ( const LThread &inThread )
{
#pragma unused ( inThread )
}

// don't call on self; call from main thread
void LThread::DeleteThread( void *inResult )
{
	int error;

	error = pthread_cancel( mPThreadID );
	delete this;
}




