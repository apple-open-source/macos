/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : thread_macosx.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 7/6/00
	    License: Copyright (C) 2000 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This handles thread function abstraction.
	            
********************************************************************/

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "thread_generic.h"

#define PCSC_MUTEX_LOCKED    1
#define PCSC_MUTEX_UNLOCKED  0

int SYS_MutexInit(PCSCLITE_MUTEX_T mMutex)
{
	int retval;
	retval = pthread_mutex_init(mMutex, NULL);
	return retval;
}

int SYS_MutexDestroy(PCSCLITE_MUTEX_T mMutex)
{
	int retval;
	retval = pthread_mutex_destroy(mMutex);
	return retval;
}

int SYS_MutexLock(PCSCLITE_MUTEX_T mMutex)
{
	int retval;
	retval = pthread_mutex_lock(mMutex);
	return retval;
}

int SYS_MutexUnLock(PCSCLITE_MUTEX_T mMutex)
{
	int retval;
	retval = pthread_mutex_unlock(mMutex);
	return retval;
}

int SYS_ThreadCreate(PCSCLITE_THREAD_T * pthThread, int attributes,
	PCSCLITE_THREAD_FUNCTION(pvFunction), LPVOID pvArg)
{
	pthread_attr_t attr;
	int rx;

	if (0 != pthread_attr_init(&attr))
		return 0;

	if (attributes & THREAD_ATTR_DETACHED)
		if (0 != pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
		{
			pthread_attr_destroy(&attr);
			return 0;
		}
	
	rx = pthread_create(pthThread, &attr, pvFunction, pvArg);
	
	pthread_attr_destroy(&attr);
	
	return (0 == rx);	// return 1 if success, 0 otherwise
}

int SYS_ThreadCancel(PCSCLITE_THREAD_T * pthThread)
{

	int retval;
	retval = pthread_cancel(*pthThread);

	if (retval == 0)
	{
		return 1;
	} else
	{
		return 0;
	}
}

int SYS_ThreadDetach(PCSCLITE_THREAD_T pthThread)
{
	// Returns 1 (true) if thread detached OK, 0 (false) otherwise
	if (pthThread)
		return (pthread_detach(pthThread) == 0);	// 0 result is success

	return 0;
}

int SYS_ThreadJoin(PCSCLITE_THREAD_T *pthThread, LPVOID* pvRetVal)
{

	int retval;
	retval = pthread_join(*pthThread, pvRetVal);

	if (retval == 0)
	{
		return 1;
	} else
	{
		return 0;
	}
}

int SYS_ThreadExit(LPVOID pvRetVal)
{

	pthread_exit(pvRetVal);
	return 1;
}
