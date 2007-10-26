/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CInternalDispatchThread
 */

#include "CInternalDispatchThread.h"
#include "CLog.h"

//--------------------------------------------------------------------------------------------------
//	* CInternalDispatchThread()
//
//--------------------------------------------------------------------------------------------------

CInternalDispatchThread::CInternalDispatchThread ( const UInt32 inThreadSig ) : DSCThread(inThreadSig)
{
	fInternalDispatchStackHeight = -1;
	//normally internal dispatch is only one deep
	{
		fHandlerInternalMsgData = (sComData *)::calloc( 1, sizeof( sComData ) + kMsgBlockSize );
		if ( fHandlerInternalMsgData != nil )
		{
			fHandlerInternalMsgData->fDataSize		= kMsgBlockSize;
			fHandlerInternalMsgData->fDataLength    = 0;
		}
	}
	fInternalMsgDataList[0] = fHandlerInternalMsgData;
	for (int idx = 1; idx < kMaxInternalDispatchRecursion; idx++)
	{
		fInternalMsgDataList[idx] = nil;
		bInternalLockCalledList[idx] = false;
	}
} // CInternalDispatchThread

//--------------------------------------------------------------------------------------------------
//	* ~CInternalDispatchThread()
//
//--------------------------------------------------------------------------------------------------

CInternalDispatchThread::~CInternalDispatchThread()
{
	if (fHandlerInternalMsgData != nil)
	{
		free(fHandlerInternalMsgData);
		fHandlerInternalMsgData = nil;
	}
	
	for (int idx = 1; idx < kMaxInternalDispatchRecursion; idx++)
	{
		if (bInternalLockCalledList[idx])
		{
			DbgLog( kLogHandler, "CPH::Destructor: final Unlock was not called for level <%d>", idx );
			bInternalLockCalledList[idx] = false;
		}
	}
} // ~CInternalDispatchThread

//--------------------------------------------------------------------------------------------------
//	* UpdateHandlerInternalMsgData(void)
//
//--------------------------------------------------------------------------------------------------

void CInternalDispatchThread::UpdateHandlerInternalMsgData( sComData* inOldMsgData, sComData* inNewMsgData )
{
	//DbgLog( kLogHandler, "CPH::UpdateHandlerInternalMsgData:: called with fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
	if ( (fInternalDispatchStackHeight >= 0) && (fInternalDispatchStackHeight < kMaxInternalDispatchRecursion) )
	{
		if ( (inOldMsgData != nil) && (inNewMsgData != nil) )
		{
			if (fInternalMsgDataList[fInternalDispatchStackHeight] == inOldMsgData)
			{
				fInternalMsgDataList[fInternalDispatchStackHeight] = inNewMsgData;
				if (fInternalDispatchStackHeight == 0)
				{
					fHandlerInternalMsgData = inNewMsgData;
				}
				//DbgLog( kLogHandler, "CPH::UpdateHandlerInternalMsgData:: switched fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
			}
		}
	}
} // UpdateHandlerInternalMsgData

//--------------------------------------------------------------------------------------------------
//	* GetHandlerInternalMsgData(void)
//
//--------------------------------------------------------------------------------------------------

sComData* CInternalDispatchThread::GetHandlerInternalMsgData( void )
{
	//DbgLog( kLogHandler, "CPH::GetHandlerInternalMsgData:: asked for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
	if ( (fInternalDispatchStackHeight >= 0) && (fInternalDispatchStackHeight < kMaxInternalDispatchRecursion) )
	{
		if ( fInternalMsgDataList[fInternalDispatchStackHeight] != nil )
		{
			//DbgLog( kLogHandler, "CPH::GetHandlerInternalMsgData:: returned for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
			return(fInternalMsgDataList[fInternalDispatchStackHeight]);
		}
	}
	return(nil);
} // ResetHandlerInternalMsgData

//--------------------------------------------------------------------------------------------------
//	* ResetHandlerInternalMsgData(void)
//
//--------------------------------------------------------------------------------------------------

void CInternalDispatchThread::ResetHandlerInternalMsgData( void )
{
	//DbgLog( kLogHandler, "CPH::ResetHandlerInternalMsgData:: request for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
	//DbgLog( kLogHandler, "CPH::ResetHandlerInternalMsgData:: called with bInternalLockCalled = %s", bInternalLockCalledList[fInternalDispatchStackHeight] ? "true" : "false" );
	if ( (fInternalDispatchStackHeight > 0) && (fInternalDispatchStackHeight < kMaxInternalDispatchRecursion) )
	{
		if ( bInternalLockCalledList[fInternalDispatchStackHeight] && ( fInternalMsgDataList[fInternalDispatchStackHeight] != nil ) )
		{
			bInternalLockCalledList[fInternalDispatchStackHeight] = false;
			free(fInternalMsgDataList[fInternalDispatchStackHeight]);
			fInternalMsgDataList[fInternalDispatchStackHeight] = nil;
			//DbgLog( kLogHandler, "CPH::ResetHandlerInternalMsgData:: done for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
		}
	}
	
	if ( fInternalDispatchStackHeight > -1 )
		fInternalDispatchStackHeight--;
	
	if ( fInternalDispatchStackHeight == -1 )
	{
		bInternalDispatchActive = false;
		
		// if we have no active requests, let's clean up any internal blocks and reset to the default
		if ( fHandlerInternalMsgData != NULL && fHandlerInternalMsgData->fDataSize > kMsgBlockSize )
		{
			// log excessive size buffers
			if ( fHandlerInternalMsgData->fDataSize > 128 * 1024 )
			{
				DbgLog( kLogHandler, "CInternalDispatchThread::ResetHandlerInternalMsgData buffer was excessive size %d",
					    fHandlerInternalMsgData->fDataSize );
			}

			free( fHandlerInternalMsgData );
			
			fHandlerInternalMsgData = (sComData *)::calloc( 1, sizeof( sComData ) + kMsgBlockSize );
			if ( fHandlerInternalMsgData != nil )
			{
				fHandlerInternalMsgData->fDataSize      = kMsgBlockSize;
				fHandlerInternalMsgData->fDataLength    = 0;
			}
		}
		
		fInternalMsgDataList[0] = fHandlerInternalMsgData;
	}
} // ResetHandlerInternalMsgData

//--------------------------------------------------------------------------------------------------
//	* SetHandlerInternalMsgData(void)
//
//--------------------------------------------------------------------------------------------------

void CInternalDispatchThread::SetHandlerInternalMsgData( void )
{
	//DBGLOG1( kLogHandler, "CPH::SetHandlerInternalMsgData:: called with bInternalLockCalled = %s", bInternalLockCalledList[fInternalDispatchStackHeight] ? "true" : "false" );
	fInternalDispatchStackHeight++;
	//DBGLOG1( kLogHandler, "CPH::SetHandlerInternalMsgData:: request for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
	bInternalLockCalledList[fInternalDispatchStackHeight] = true;
	if ( (fInternalDispatchStackHeight > 0) && (fInternalDispatchStackHeight < kMaxInternalDispatchRecursion) )
	{
		fInternalMsgDataList[fInternalDispatchStackHeight] = (sComData *)::calloc( 1, sizeof( sComData ) + kMsgBlockSize );
		if ( fInternalMsgDataList[fInternalDispatchStackHeight] != nil )
		{
			fInternalMsgDataList[fInternalDispatchStackHeight]->fDataSize		= kMsgBlockSize;
			fInternalMsgDataList[fInternalDispatchStackHeight]->fDataLength    = 0;
			//DbgLog( kLogHandler, "CPH::SetHandlerInternalMsgData:: done for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
		}
	}
	if ( (fInternalDispatchStackHeight >= 0) && (fInternalDispatchStackHeight < kMaxInternalDispatchRecursion) )
	{
		bInternalDispatchActive = true;
	}
} // SetHandlerInternalMsgData

//--------------------------------------------------------------------------------------------------
//	* SetInternalDispatchActive(void)
//
//--------------------------------------------------------------------------------------------------

void CInternalDispatchThread::SetInternalDispatchActive( bool inInternalDispatchActive )
{
	bInternalDispatchActive = inInternalDispatchActive;
}

//--------------------------------------------------------------------------------------------------
//	* GetInternalDispatchActive(void)
//
//--------------------------------------------------------------------------------------------------

bool CInternalDispatchThread::GetInternalDispatchActive( void )
{
	return(bInternalDispatchActive);
}
