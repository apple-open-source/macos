/*
	File:		NSLSemaphore.h

	Contains:	Interface for the NSLSemaphore (lock) base class.
				IMPORTANT:
				* This is an independently derived implementation,
				* OPTIMIZED FOR MAC OS X'S POSIX THREADS,
				* of Metrowerks' PowerPlant Thread classes, which likely
				* makes the class and method names in this header file also
				* copyright Metrowerks.

	Version:	AppleShare X $Revision: 1.1 $

	Copyright:	© 1998-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:
		DRI:				Chris Jalbert
		Other Contact:		Michael Dasenbrock
		Technology:			RAdmin, AppleShare X; Directory Services, Mac OS X

	Writers:
		(cpj)	Chris Jalbert

	Change History (most recent first):

		 <7>	 10/18/99	cpj		Included pthread headers instead of
									LThread.h to remove dependencies.
		 <6>	 09/16/99	cpj		Stripped out cthread (Hera) version.
									Standardized error constants.
		 <5>	 09/04/99	cpj		Added namespace qualifiers.
		 <4>	 06/24/99	cpj		Ported to Beaker. Code compiles.
		 <1>	 06/30/98	cpj		Initial checkin.
		 <0>	 1/26/98	cpj		Initial creation.
*/


#ifndef _NSLSemaphore_H_
#define _NSLSemaphore_H_


/**** Required system headers. ****/
// ANSI / POSIX headers
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

// Universal / CoreFoundation Headers
#include <Carbon/Carbon.h>


//namespace PowerPlant {

/**** Typedefs, enums and constants. ****/
/*
const SInt32	semaphore_WaitForever = -1 ;
const SInt32	semaphore_NoWait	  =  0 ;
// error codes
enum {
	errSemaphoreDestroyed = 28020,
	errSemaphoreTimedOut,
	errSemaphoreNotOwner,
	errSemaphoreAlreadyReset,
	errSemaphoreOther
} ;
*/


/******************************************************************************
	==>  NSLSemaphore class definition  <==
******************************************************************************/
class NSLSemaphore
{
public:
	/**** Typedefs, enums and constants. ****/
	enum eWaitTime {
		kForever = -1,
		kNever = 0
	} ;
	enum eErr {
		semDestroyedErr = 28020,
		semTimedOutErr,
		semNotOwnerErr,
		semAlreadyResetErr,
		semOtherErr
	} ;

	/**** Instance method protoypes. ****/
	// ctor and dtor.
			NSLSemaphore			( SInt32 initialCount = 0 ) ;
	virtual	~NSLSemaphore			( void ) ;

	// New methods.
	virtual void		Signal	( void ) ;
	virtual OSStatus	Wait	( SInt32 milliSecs = kForever ) ;

protected:
	/**** Instance variables. ****/
	pthread_mutex_t		mConditionLock ;
	pthread_cond_t		mSemaphore ;
	SInt32				mExcessSignals ;
	bool				mDestroying ;

private:
	/**** Invalid methods and undefined operations. ****/
	// Copy constructor
							NSLSemaphore	( const NSLSemaphore & ) ;
	// Assignment
			NSLSemaphore &	operator=	( const NSLSemaphore & ) ;
} ;

//}	// namespace PowerPlant

#endif	/* _NSLSemaphore_H_ */
