/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * @header CInternalDispatchThread
 */

#include "CInternalDispatchThread.h"
#include "CLog.h"

//--------------------------------------------------------------------------------------------------
//	* CInternalDispatchThread()
//
//--------------------------------------------------------------------------------------------------

CInternalDispatchThread::CInternalDispatchThread ( const OSType inThreadSig ) : DSCThread(inThreadSig)
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
} // ~CInternalDispatchThread

//--------------------------------------------------------------------------------------------------
//	* UpdateHandlerInternalMsgData(void)
//
//--------------------------------------------------------------------------------------------------

void CInternalDispatchThread::UpdateHandlerInternalMsgData( sComData* inOldMsgData, sComData* inNewMsgData )
{
	//DBGLOG1( kLogHandler, "CPH::UpdateHandlerInternalMsgData:: called with fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
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
				//DBGLOG1( kLogHandler, "CPH::UpdateHandlerInternalMsgData:: switched fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
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
	//DBGLOG1( kLogHandler, "CPH::GetHandlerInternalMsgData:: asked for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
	if ( (fInternalDispatchStackHeight >= 0) && (fInternalDispatchStackHeight < kMaxInternalDispatchRecursion) )
	{
		if ( fInternalMsgDataList[fInternalDispatchStackHeight] != nil )
		{
			//DBGLOG1( kLogHandler, "CPH::GetHandlerInternalMsgData:: returned for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
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
	//DBGLOG1( kLogHandler, "CPH::ResetHandlerInternalMsgData:: request for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
	if ( (fInternalDispatchStackHeight > 0) && (fInternalDispatchStackHeight < kMaxInternalDispatchRecursion) )
	{
		if ( fInternalMsgDataList[fInternalDispatchStackHeight] != nil )
		{
			free(fInternalMsgDataList[fInternalDispatchStackHeight]);
			fInternalMsgDataList[fInternalDispatchStackHeight] = nil;
			//DBGLOG1( kLogHandler, "CPH::ResetHandlerInternalMsgData:: done for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
		}
	}
	fInternalDispatchStackHeight--;
	if (fInternalDispatchStackHeight == -1)
	{
		bInternalDispatchActive = false;
	}
} // ResetHandlerInternalMsgData

//--------------------------------------------------------------------------------------------------
//	* SetHandlerInternalMsgData(void)
//
//--------------------------------------------------------------------------------------------------

void CInternalDispatchThread::SetHandlerInternalMsgData( void )
{
	fInternalDispatchStackHeight++;
	//DBGLOG1( kLogHandler, "CPH::SetHandlerInternalMsgData:: request for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
	if ( (fInternalDispatchStackHeight > 0) && (fInternalDispatchStackHeight < kMaxInternalDispatchRecursion) )
	{
		fInternalMsgDataList[fInternalDispatchStackHeight] = (sComData *)::calloc( 1, sizeof( sComData ) + kMsgBlockSize );
		if ( fInternalMsgDataList[fInternalDispatchStackHeight] != nil )
		{
			fInternalMsgDataList[fInternalDispatchStackHeight]->fDataSize		= kMsgBlockSize;
			fInternalMsgDataList[fInternalDispatchStackHeight]->fDataLength    = 0;
			//DBGLOG1( kLogHandler, "CPH::SetHandlerInternalMsgData:: done for fInternalDispatchStackHeight = %d", fInternalDispatchStackHeight );
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
