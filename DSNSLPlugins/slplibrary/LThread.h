/*
	File:		LThread.h

	Contains:	Interface for the LThread abstract base class and UMainThread
				concrete derived class.
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


#ifndef _LThread_H_
#define _LThread_H_


/**** Required system headers. ****/
// ANSI / POSIX headers
#include <stdio.h>		// for fputs()
#include <unistd.h>		// for _POSIX_THREADS

// Universal Headers
#include <Carbon/Carbon.h>
#include <Carbon/Carbon.h>		// for threadProtocolErr


/**** Forward declarations. ****/


/**** Typedefs and constants. ****/
#ifdef _POSIX_THREADS
 #include <pthread.h>
 typedef pthread_t	ThreadIDT;
#else	/* _POSIX_THREADS */
 #include <mach/cthreads.h>
 typedef cthread_t	ThreadIDT;
#endif	/* _POSIX_THREADS */

// error codes
enum {
		errKilledThread = 28000, 
		errBadThreadState
};


/******************************************************************************
	==>  LThread (abstract) class definition  <==
******************************************************************************/
class LThread
{
public:
	/**** Typedefs, enums and constants. ****/
	// thread creation flags (ignored)
	enum EThreadOption	{ threadOption_UsePool = 0x0002,  
						  threadOption_Alloc   = 0x0004, 
						  threadOption_NoFPU   = 0x0008,  
						  threadOption_Exact   = 0x0010, 
						  threadOption_Main    = 0x1000, 
						  threadOption_Default = threadOption_Alloc  };

	/**** Class method protoypes. ****/
	static LThread	*GetCurrentThread	( void );
	static LThread	*GetMainThread		( void )
						{ return sMainThread; }
	static Boolean	InMainThread		( void );
	static void		Yield				( const LThread *inYieldTo = NULL );

	/**** Constructor. ****/
					LThread	( EThreadOption inFlags = threadOption_Default );

	/**** Instance method protoypes. ****/
			Boolean	IsCurrent		( void ) const;
			void	SetNextOfKin	( LThread *inThread )
						{ mNextOfKin = inThread; }
	virtual void	DeleteThread	( void *inResult = NULL );
	virtual void	Resume			( void );

protected:
	/**** Instance method protoypes. ****/
	virtual			~LThread		( void );

	// thread execution
	virtual void	*Run			( void ) = 0;	// pure virtual
	virtual void	ThreadDied		( const LThread &inThread );

	/**** Instance variables. ****/
	ThreadIDT	mThread;
	pthread_t	mPThreadID;
	LThread		*mNextOfKin;
	void		*mResult;

private:
	/**** Class globals. ****/
	static LThread	*sMainThread;
	static Boolean	sInited;

	/**** Private class method protoypes. ****/
	// thread execution wrapper
	static void		*_RunWrapper	( void *arg );

	/**** Invalid methods and undefined operations. ****/
	// Copy constructor
					LThread			( const LThread & );
	// Assignment
			LThread	&operator=		( const LThread & );
};


/******************************************************************************
	==>  UMainThread (mostly) concrete class definition  <==
******************************************************************************/
class UMainThread : public LThread
{
public:
//	typedef LThread inherited;
public:
			UMainThread		( void )
			: LThread ( LThread::threadOption_Main )
//			: inherited ( LThread::threadOption_Main )
				{ }	// there's nothing to do
	virtual	~UMainThread	( void )
				{ }	// there's nothing to do
protected:
	virtual void	*Run	( void );
};


/**** Inline class method definitions. ****/
// Since the application's main thread starts implicitly at main(),
// throw an exception.
inline void *
UMainThread::Run ()
{
	::fputs ("Entered UMainThread::Run() -- this should never happen!", stderr);
//	throw (threadProtocolErr);
	return NULL;
}


#endif	/* _LThread_H_ */
