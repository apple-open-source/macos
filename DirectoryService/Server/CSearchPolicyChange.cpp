/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CSearchPolicyChange
 */

#include "CSearchPolicyChange.h"
#include "CLog.h"

//#include <stdio.h>			// for popen()
#include <sys/wait.h>		// for waitpid() et al
//#include <sys/stat.h>		// for stat()
#include <syslog.h>


// --------------------------------------------------------------------------------
// * Globals
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// * Externs
// --------------------------------------------------------------------------------
extern DSMutexSemaphore	   *gKerberosToolForkLock;

//--------------------------------------------------------------------------------------------------
// * CSearchPolicyChange()
//
//--------------------------------------------------------------------------------------------------

CSearchPolicyChange::CSearchPolicyChange ( uInt32 inChangeType )
	: DSCThread( kTSSearchPolicyChangeThread )
{
	fThreadSignature = kTSSearchPolicyChangeThread;

	fChangeType = inChangeType; //currently not used
	
} // CSearchPolicyChange



//--------------------------------------------------------------------------------------------------
// * ~CSearchPolicyChange()
//
//--------------------------------------------------------------------------------------------------

CSearchPolicyChange::~CSearchPolicyChange()
{
} // ~CSearchPolicyChange



//--------------------------------------------------------------------------------------------------
// * StartThread()
//
//--------------------------------------------------------------------------------------------------

void CSearchPolicyChange::StartThread ( void )
{
	if ( this == nil )
	{
		ERRORLOG( kLogApplication, "SearchPolicyChange StartThread failed with memory error on itself" );
		throw((sInt32)eMemoryError);
	}

	this->Resume();

	SetThreadRunState( kThreadRun );		// Tell our thread it's running

} // StartThread



//--------------------------------------------------------------------------------------------------
// * StopThread()
//
//--------------------------------------------------------------------------------------------------

void CSearchPolicyChange::StopThread ( void )
{
	//not used since this thread is a one shot activity now
	if ( this == nil )
	{
		ERRORLOG( kLogApplication, "SearchPolicyChange StopThread failed with memory error on itself" );
		throw((sInt32)eMemoryError);
	}

	// Check that the current thread context is not our thread context

	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread


//--------------------------------------------------------------------------------------------------
// * ThreadMain()
//
//--------------------------------------------------------------------------------------------------

long CSearchPolicyChange::ThreadMain ( void )
{

	//run the tool /sbin/kerberosautoconfig each time there is a search policy change
	
//	struct stat			statResult;
//	sInt32				result				= eDSNoErr;
	const char		   *path				= "/sbin/kerberosautoconfig";
	//FILE			   *pipe				= nil;
	pid_t				ourPID				= -1;
	int					status				= 0;

	gKerberosToolForkLock->Wait();
	
	syslog(LOG_INFO,"Authentication Search Policy was updated/changed.");
	
//	result = ::stat( path, &statResult );
//	if (result == eDSNoErr)
//	{
		if ((ourPID = vfork()) < 0)
		{
			ERRORLOG(kLogApplication, "CSearchPolicyChange:: /sbin/kerberosautoconfig tool not able to be run.");
			gKerberosToolForkLock->Signal();
			return( 0 );
		}
		if (ourPID == 0)
		{
			setsid();
			execlp(path, path, (char *) 0);
			_exit(errno);	
		}
		else
		{
			waitpid(ourPID, &status, 0);
			DBGLOG(kLogApplication, "CSearchPolicyChange:: /sbin/kerberosautoconfig tool was run.");
		}
/*
		// execute the command
		pipe = popen(path, "r");
		
		//errno unreliable with popen()
	
		if (pipe != nil)
		{
			pclose(pipe);
			DBGLOG(kLogApplication, "CSearchPolicyChange:: /sbin/kerberosautoconfig tool was run.");
		}
		else
		{
			ERRORLOG(kLogApplication, "CSearchPolicyChange:: /sbin/kerberosautoconfig tool not able to be run.");
		}
*/
//	}
//	else
//	{
//		ERRORLOG(kLogApplication, "CSearchPolicyChange:: /sbin/kerberosautoconfig tool not found.");
//	}
	
	gKerberosToolForkLock->Signal();

	return( 0 );

} // ThreadMain


