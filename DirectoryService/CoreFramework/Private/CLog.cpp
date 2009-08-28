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
 * @header CLog
 * Implementation of the logging mechanism.
 */

#include <string.h>					// for memset() and strcpy()

#include <NSSystemDirectories.h>	// for NSSearchPath*()
#include <sys/types.h>				// for mode_t
#include <sys/stat.h>				// for mkdir() and stat()
#include <libkern/OSAtomic.h>

#include "CLog.h"
#include "COSUtils.h"
#include "DSMutexSemaphore.h"

const char *kgStringMessageEndOfLine = "\r\n";

// ----------------------------------------------------------------------------
//	* CLog Class Globals
// ----------------------------------------------------------------------------

OptionBits	CLog::fSrvrLogFlags		= kLogMeta;
OptionBits	CLog::fErrLogFlags		= kLogMeta | kLogError;
OptionBits	CLog::fDbgLogFlags		= kLogEverything;
OptionBits	CLog::fInfoLogFlags		= kLogMeta;
CLog	   *CLog::fServerLog		= nil;
CLog	   *CLog::fDebugLog			= nil;
CLog	   *CLog::fErrorLog			= nil;
CLog	   *CLog::fInfoLog			= nil;
CString	   *CLog::fServerLogName	= nil;
CString	   *CLog::fErrorLogName		= nil;
CString	   *CLog::fDebugLogName		= nil;
CString	   *CLog::fInfoLogName		= nil;


//--------------------------------------------------------------------------------------------------
//	* Initialize()
//
//--------------------------------------------------------------------------------------------------

SInt32 CLog::Initialize (	OptionBits srvrFlags,	OptionBits errFlags,
							OptionBits debugFlags,	OptionBits infoFlags,
							bool inOpenDbgLog, bool inOpenInfoLog,
							bool inLocalOnlyMode )
{
	SInt32							result;
	NSSearchPathEnumerationState	eState;
	struct stat						ssFolder;
    char							localPath[ PATH_MAX ];
    CString							csBasePath( 128 );

	try
	{
		// Locate or create the product suite's prefs folder.
		eState = NSStartSearchPathEnumeration( NSLibraryDirectory, NSLocalDomainMask );
		eState = NSGetNextSearchPathEnumeration( eState, localPath );

		fSrvrLogFlags	= srvrFlags;
		fErrLogFlags	= errFlags;
		fDbgLogFlags	= debugFlags;
		fInfoLogFlags	= infoFlags;
		
		fServerLogName = new CString( 128 );
		if ( fServerLogName == nil )
		{
			throw( (SInt32)eMemoryAllocError );
		}

		fErrorLogName = new CString( 128 );
		if ( fErrorLogName == nil )
		{
			throw( (SInt32)eMemoryAllocError );
		}

		fDebugLogName = new CString( 128 );
		if ( fDebugLogName == nil )
		{
			throw( (SInt32)eMemoryAllocError );
		}

		fInfoLogName = new CString( 128 );
		if ( fInfoLogName == nil )
		{
			throw( (SInt32)eMemoryAllocError );
		}

		// Set the base path (/Library)
		csBasePath.Set( localPath );

		// Append the log folder name
		csBasePath.Append( "/" );
		csBasePath.Append( COSUtils::GetStringFromList( kAppStringsListID, kStrLogFolder ) );

		// Create it if it doesn't exist
		result = ::stat( csBasePath.GetData(), &ssFolder );
		if ( result != eDSNoErr )
		{
			result = ::mkdir( csBasePath.GetData(), 0775 );
			::chmod( csBasePath.GetData(), 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		}
		
		// Append the product folder name
		csBasePath.Append( "/" );
		if (inLocalOnlyMode)
		{
			csBasePath.Append( COSUtils::GetStringFromList( kAppStringsListID, kStrLocalProductFolder ) );
		}
		else
		{
			csBasePath.Append( COSUtils::GetStringFromList( kAppStringsListID, kStrProductFolder ) );
		}

		// Create it if it doesn't exist
		result = ::stat( csBasePath.GetData(), &ssFolder );
		if ( result != eDSNoErr )
		{
			result = ::mkdir( csBasePath.GetData(), 0775 );
			::chmod( csBasePath.GetData(), 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		}
		
		csBasePath. Append( "/" );

		// Set the data member log file names
		fServerLogName->Set( csBasePath.GetData() );
		fServerLogName->Append( COSUtils::GetStringFromList( kAppStringsListID, kStrProductFolder ) );
		fServerLogName->Append( "." );
		fServerLogName->Append( COSUtils::GetStringFromList( kAppStringsListID, kStrServerLogFileName ) );

		fErrorLogName->Set( csBasePath.GetData() );
		fErrorLogName->Append( COSUtils::GetStringFromList( kAppStringsListID, kStrProductFolder ) );
		fErrorLogName->Append( "." );
		fErrorLogName->Append( COSUtils::GetStringFromList( kAppStringsListID, kStrErrorLogFileName ) );

		fDebugLogName->Set( csBasePath.GetData() );
		fDebugLogName->Append( COSUtils::GetStringFromList( kAppStringsListID, kStrProductFolder ) );
		fDebugLogName->Append( "." );
		fDebugLogName->Append( COSUtils::GetStringFromList( kAppStringsListID, kStrDebugLogFileName ) );

		fInfoLogName->Set( csBasePath.GetData() );
		fInfoLogName->Append( COSUtils::GetStringFromList( kAppStringsListID, kStrProductFolder ) );
		fInfoLogName->Append( "." );
		fInfoLogName->Append( COSUtils::GetStringFromList( kAppStringsListID, kStrInfoLogFileName ) );

		// Create only the required log files
		if ( result == eDSNoErr )
		{
			// Create the server event log file.
			fServerLog = new CLog( fServerLogName->GetData(), kLengthUnlimited, kThreadInfo | kRollLog );

			// Create the Error event log file on demand and not here
			//fErrorLog = new CLog( fErrorLogName->GetData(), kLengthUnlimited, kThreadInfo | kRollLog );

			if ( inOpenDbgLog == true )
			{
				// Create the debug event log file.
				if (kLogDebugHeader & fDbgLogFlags)
				{
					fDebugLog = new CLog( fDebugLogName->GetData(), kLengthUnlimited, kDebugHdr | kThreadInfo | kRollLog );
				}
				else
				{
					fDebugLog = new CLog( fDebugLogName->GetData(), kLengthUnlimited, kThreadInfo | kRollLog );
				}
			}

			if ( inOpenInfoLog == true )
			{
				// Create the performance event log file.
				fInfoLog = new CLog( fInfoLogName->GetData(), kLengthUnlimited, kThreadInfo | kRollLog );
			}
		}
	}

	catch( SInt32 err )
	{
		result = err;
	}


	return( result );

} // Initialize


//--------------------------------------------------------------------------------------------------
//	* Deinitialize()
//
//--------------------------------------------------------------------------------------------------

void CLog::Deinitialize ( void )
{

	if ( fServerLog != nil )
	{
		delete( fServerLog );
		fServerLog = nil;
	}

	if ( fErrorLog != nil )
	{
		delete( fErrorLog );
		fErrorLog = nil;
	}

	if ( fDebugLog != nil )
	{
		delete( fDebugLog );
		fDebugLog = nil;
	}

	if ( fInfoLog != nil )
	{
		delete( fInfoLog );
		fInfoLog = nil;
	}

} // Deinitialize


//--------------------------------------------------------------------------------------------------
//	* StartLogging()
//
//--------------------------------------------------------------------------------------------------

void CLog::StartLogging ( eLogType inWhichLog, UInt32 inFlag )
{
	switch ( inWhichLog )
	{
		case keServerLog:
			fSrvrLogFlags |= inFlag;
			break;

		case keErrorLog:
			fErrLogFlags |= inFlag;
			break;

		case keDebugLog:
			fDbgLogFlags |= inFlag;
			break;

		case keInfoLog:
			fInfoLogFlags |= inFlag;
			break;
	}
} // StartLogging


//--------------------------------------------------------------------------------------------------
//	* StopLogging()
//
//--------------------------------------------------------------------------------------------------

void CLog::StopLogging ( eLogType inWhichLog, UInt32 inFlag )
{
	switch ( inWhichLog )
	{
		case keServerLog:
			fSrvrLogFlags &= ~inFlag;
			break;

		case keErrorLog:
			fErrLogFlags &= ~inFlag;
			break;

		case keDebugLog:
			fDbgLogFlags &= ~inFlag;
			break;

		case keInfoLog:
			fInfoLogFlags &= ~inFlag;
			break;
	}
} // StopLogging


//--------------------------------------------------------------------------------------------------
//	* SetLoggingPriority()
//
//--------------------------------------------------------------------------------------------------

void CLog::SetLoggingPriority ( eLogType inWhichLog, UInt32 inPriority )
{
	switch ( inWhichLog )
	{
		case keServerLog:
			fSrvrLogFlags = kLogEverything;
			break;

		case keErrorLog:
			fErrLogFlags = kLogEverything;
			break;

		case keDebugLog:
			fDbgLogFlags = kLogEmergency;
			switch ( inPriority )
			{
				case 9:
				case 8:
					fDbgLogFlags |= kLogMeta;
				case 7:
					fDbgLogFlags |= kLogDebug;
				case 6:
					fDbgLogFlags |= kLogInfo;
				case 5:
					fDbgLogFlags |= kLogNotice | kLogPlugin | kLogPerformanceStats;
				case 4:
					fDbgLogFlags |= kLogWarning | kLogAPICalls;
				case 3:
					fDbgLogFlags |= kLogError | kLogThreads | kLogEndpoint | kLogConnection | kLogTCPEndpoint;
				case 2:
					fDbgLogFlags |= kLogCritical | kLogHandler | kLogMsgQueue | kLogMsgTrans;
				case 1:
					fDbgLogFlags |= kLogAlert | kLogApplication | kLogAssert | kLogListener; 
					break;
				case 0:	// we by default do emergency
					break;
			}
			
			DbgLog( kLogMeta, "Debug Logging priority set to %d - filter %X", inPriority, fDbgLogFlags );
			break;

		case keInfoLog:
			fInfoLogFlags = kLogEverything;
			break;
	}
} // SetLoggingPriority


//--------------------------------------------------------------------------------------------------
//	* ToggleLogging()
//
//--------------------------------------------------------------------------------------------------

void CLog::ToggleLogging ( eLogType inWhichLog, UInt32 inFlag )
{
	switch ( inWhichLog )
	{
		case keServerLog:
			if ( fSrvrLogFlags & inFlag )
			{
				fSrvrLogFlags &= ~inFlag;
			}
			else
			{
				fSrvrLogFlags |= inFlag;
			}
			break;

		case keErrorLog:
			if ( fErrLogFlags & inFlag )
			{
				fErrLogFlags &= ~inFlag;
			}
			else
			{
				fErrLogFlags |= inFlag;
			}
			break;

		case keDebugLog:
			if ( fDbgLogFlags & inFlag )
			{
				fDbgLogFlags &= ~inFlag;
			}
			else
			{
				fDbgLogFlags |= inFlag;
			}
			break;

		case keInfoLog:
			if ( fInfoLogFlags & inFlag )
			{
				fInfoLogFlags &= ~inFlag;
			}
			else
			{
				fInfoLogFlags |= inFlag;
			}
			break;
	}
} // ToggleLogging


//--------------------------------------------------------------------------------------------------
//	* IsLogging()
//
//--------------------------------------------------------------------------------------------------

bool CLog::IsLogging ( eLogType inWhichLog, UInt32 inFlag )
{
	switch ( inWhichLog )
	{
		case keServerLog:
			return( fSrvrLogFlags & inFlag );

		case keErrorLog:
			return( fErrLogFlags & inFlag );

		case keDebugLog:
			return( fDbgLogFlags & inFlag );

		case keInfoLog:
			return( fInfoLogFlags & inFlag );
	}

	return( false );

} // IsLogging


//--------------------------------------------------------------------------------------------------
//	* StartDebugLog ()
//
//--------------------------------------------------------------------------------------------------

void CLog::StartDebugLog ( void )
{
	try
	{
		if ( fDebugLog == nil )
		{
			if (kLogDebugHeader & fDbgLogFlags)
			{
				fDebugLog = new CLog( fDebugLogName->GetData(), kLengthUnlimited, kDebugHdr | kThreadInfo | kRollLog );
			}
			else
			{
				fDebugLog = new CLog( fDebugLogName->GetData(), kLengthUnlimited, kThreadInfo | kRollLog );
			}
		}
	}

	catch ( ... )
	{
	}
} // StartDebugLog


//--------------------------------------------------------------------------------------------------
//	* StopDebugLog ()
//
//--------------------------------------------------------------------------------------------------

void CLog::StopDebugLog ( void )
{
	if ( fDebugLog != nil )
	{
		fDebugLog->Lock();

		fDbgLogFlags = kLogMeta;

		delete( fDebugLog );
		fDebugLog = nil;
	}
} // StopDebugLog


//--------------------------------------------------------------------------------------------------
//	* StartErrorLog ()
//
//--------------------------------------------------------------------------------------------------

void CLog::StartErrorLog ( void )
{
	try
	{
		if ( fErrorLog == nil )
		{
			CLog *tempLog = new CLog( fErrorLogName->GetData(), kLengthUnlimited, kThreadInfo | kRollLog );
			
			// use atomic calls to set this so we can deal with multiple clients at once
			if ( OSAtomicCompareAndSwapPtrBarrier(NULL, tempLog, (void **) &fErrorLog) == false )
				delete tempLog;
		}
	}

	catch ( ... )
	{
	}
} // StartErrorLog


//--------------------------------------------------------------------------------------------------
//	* StopErrorLog ()
//
//--------------------------------------------------------------------------------------------------

void CLog::StopErrorLog ( void )
{
	if ( fErrorLog != nil )
	{
		fErrorLog->Lock();

		fErrLogFlags = kLogMeta;

		delete( fErrorLog );
		fErrorLog = nil;
	}
} // StopErrorLog


//--------------------------------------------------------------------------------------------------
//	* StartInfoLog ()
//
//--------------------------------------------------------------------------------------------------

void CLog::StartInfoLog ( void )
{
	try
	{
		if ( fInfoLog == nil )
		{
			fInfoLogFlags = kLogEverything;

			fInfoLog = new CLog( fInfoLogName->GetData(), kLengthUnlimited, kThreadInfo | kRollLog );
		}
	}

	catch( ... )
	{
	}
} // StartInfoLog


//--------------------------------------------------------------------------------------------------
//	* StopInfoLog ()
//
//--------------------------------------------------------------------------------------------------

void CLog::StopInfoLog ( void )
{
	if ( fInfoLog != nil )
	{
		fInfoLog->Lock();

		fInfoLogFlags = kLogMeta;

		delete( fInfoLog );
		fInfoLog = nil;
	}
} // StopInfoLog


//--------------------------------------------------------------------------------------------------
//	* GetServerLog ()
//
//--------------------------------------------------------------------------------------------------

CLog* CLog::GetServerLog ( void )
{
	return( fServerLog );
} // GetServerLog


//--------------------------------------------------------------------------------------------------
//	* GetErrorLog ()
//
//--------------------------------------------------------------------------------------------------

CLog* CLog::GetErrorLog ( void )
{
	return( fErrorLog );
} // GetErrorLog


//--------------------------------------------------------------------------------------------------
//	* GetDebugLog ()
//
//--------------------------------------------------------------------------------------------------

CLog* CLog::GetDebugLog ( void )
{
	return( fDebugLog );
} // GetDebugLog


//--------------------------------------------------------------------------------------------------
//	* GetInfoLog ()
//
//--------------------------------------------------------------------------------------------------

CLog* CLog::GetInfoLog ( void )
{
	return( fInfoLog );
} // GetInfoLog


//--------------------------------------------------------------------------------------------------
//	* Lock()
//
//--------------------------------------------------------------------------------------------------

void CLog::Lock ( void )
{
	fLock->WaitLock();
} // Lock


//--------------------------------------------------------------------------------------------------
//	* UnLock()
//
//--------------------------------------------------------------------------------------------------

void CLog::UnLock ( void )
{
	fLock->SignalLock();
} // UnLock


// ----------------------------------------------------------------------------
//	* CLog Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** CLog Public Instance Methods ****

// ctor & dtor
CLog::CLog ( const char *inFile,
			 UInt32 inMaxLen,
			 OptionBits inFlags,
			 UInt32 type,
			 UInt32 creator )
{
	fFlags				= inFlags;
	fMaxLength			= inMaxLen;
	fOffset				= 0;
	fLength				= 0;

	::memset( fHooks, 0, sizeof( fHooks ) );

	// CFile throws.  We need to catch so we don't throw out of our constructor
	try {
		fFile = new CFile( inFile, true, (inFlags & kRollLog) );
		if ( !fFile->is_open() )
			throw(-1);
		
		if (fFile != nil)
		{
			// Get the length of the file and set the write pointer to EOF.
			fFile->seekp( 0, ios::end );
			fLength = fFile->tellp();
		}
	} catch ( ... ) {
		fFile = nil;
		fLength = 0;
	}

	fLock = new DSMutexSemaphore("CLog::fLock");
} // 


//--------------------------------------------------------------------------------------------------
//	* ~CLog()
//
//--------------------------------------------------------------------------------------------------

CLog::~CLog ( void )
{
	if ( fFile != nil )
	{
		delete( fFile );
		fFile = nil;
	}

	if ( fLock != nil )
	{
		delete( fLock );
		fLock = nil;
	}
} // ~CLog


//--------------------------------------------------------------------------------------------------
//	* SetMaxLength()
//
//--------------------------------------------------------------------------------------------------

void CLog::SetMaxLength ( UInt32 inMaxLen )
{
	fMaxLength = inMaxLen;
} // SetMaxLength


//--------------------------------------------------------------------------------------------------
//	* GetInfo()
//
//--------------------------------------------------------------------------------------------------

void CLog::GetInfo (	CFileSpec	&fileSpec,
						UInt32		&startOffset,
						UInt32		&dataLength,
						bool		&hasWrapped )
{
	::strcpy( fileSpec, fFileSpec );
	startOffset = (UInt32) fOffset;
	dataLength = (UInt32) fLength;
	hasWrapped = false;
} // GetInfo


//--------------------------------------------------------------------------------------------------
//	* ClearLog()
//
//--------------------------------------------------------------------------------------------------

SInt16 CLog::ClearLog ( void )
{
	try {
		if( fFile == nil ) throw ((SInt16) ds_fnfErr);
		
		fFile->seteof( 0 );
		fOffset = 0;
		fLength = 0;
	}
	
	catch ( SInt16 nErr )
	{
		return( nErr );
	}

	return( eDSNoErr );

} // ClearLog


//--------------------------------------------------------------------------------------------------
//	* AddHook()
//
//--------------------------------------------------------------------------------------------------

void CLog::AddHook ( AppendHook fpNewHook )
{
	AppendHook	*pHooks = fHooks;
	for ( ; *pHooks; pHooks++ )
	{
	}
	*pHooks = fpNewHook;
} // AddHook


//--------------------------------------------------------------------------------------------------
//	* Append()
//
//--------------------------------------------------------------------------------------------------

SInt16 CLog::Append ( const CString &line )
{
	CString			csTemp( 60 + line.GetLength() );

	fLock->WaitLock();

	if (fFlags & kThreadInfo)
	{
		csTemp.Sprintf( ( (fFlags & kDebugHdr) ? "%G %D %T - T[%X] - %S" : "%D %T - T[%X] - %S" ), pthread_self(), &line );
	}
	else
	{
		csTemp.Sprintf( ( (fFlags & kDebugHdr) ? "%G %D %T\t%S" : "%D %T\t%S" ) , &line);
	}

	// Append newline if necessary.
	if (csTemp[csTemp.GetLength () - 1] != '\n')
	{
		csTemp.Append ('\n');
	}

	try {
		if( fFile == nil ) throw ((SInt16) ds_fnfErr);

		// wrap up later
		fFile->write( csTemp.GetData(), csTemp.GetLength() );

		// Call all the hooks.
        AppendHook	*pHooks = fHooks ;
		for ( ; *pHooks ; pHooks++)
			(*pHooks) (csTemp);
	}

	catch ( SInt16 nErr )
	{
		fLock->SignalLock();
		return( nErr );
	}

	fLock->SignalLock();

	return( eDSNoErr );

} // Append

#pragma mark -
#pragma mark C functions

void SrvrLog ( SInt32 lType, const char *szpPattern, ... )
{
	if ( (szpPattern != nil) && (CLog::GetServerLog() != nil ) )
	{
		if ( CLog::IsLogging( keServerLog, lType ) )
		{
			va_list	args;
			va_start( args, szpPattern );

			CLog::GetServerLog()->Append( CString( szpPattern, args ) );
		}
	}
} // SrvrLog


void ErrLog ( SInt32 lType, const char *szpPattern, ... )
{
	// if log is not open, we start it now
	if ( szpPattern != nil )
	{
		CLog::StartErrorLog(); //create error log on demand
		
		if ( CLog::IsLogging(keErrorLog, lType) && CLog::GetErrorLog() != nil )
		{
			va_list	args;
			va_start( args, szpPattern );

			CLog::GetErrorLog()->Append( CString( szpPattern, args ) );
		}
	}
} // ErrLog


void DbgLog ( SInt32 lType, const char *szpPattern, ... )
{
	// certain types always get logged to error log
	if ( (lType & (kLogEmergency | kLogAlert | kLogCritical | kLogError)) != 0 )
	{
		CLog::StartErrorLog(); //create error log on demand
		
		if ( CLog::IsLogging(keErrorLog, lType) && CLog::GetErrorLog() != nil )
		{
			va_list	args;
			va_start( args, szpPattern );
			
			CLog::GetErrorLog()->Append( CString( szpPattern, args ) );
		}
	}
	else if ( (szpPattern != nil) && (CLog::GetDebugLog() != nil ) )
	{
		// Just in case it was deleted while I was waiting for it
		if ( CLog::GetDebugLog() != nil )
		{
			if ( CLog::IsLogging( keDebugLog, lType ) )
			{
				va_list	args;
				va_start( args, szpPattern );

				CLog::GetDebugLog()->Append( CString( szpPattern, args ) );
			}
		}
	}
} // DbgLog


void InfoLog ( SInt32 lType, const char *szpPattern, ... )
{
	if ( (szpPattern != nil) && (CLog::GetInfoLog() != nil ) )
	{
		// Just in case it was deleted while I was waiting for it
		if ( CLog::GetInfoLog() != nil )
		{
			if ( CLog::IsLogging( keInfoLog, lType ) )
			{
				va_list	args;
				va_start( args, szpPattern );
				
				CLog::GetInfoLog()->Append( CString( szpPattern, args ) );
			}
		}
	}
} // GetInfoLog
