/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CLog
 * Interface for a circular, permanent log file.
 */

#ifndef __CLog_h__
#define __CLog_h__	1

#include <stdarg.h>		// for inline functions

#include "PrivateTypes.h"
#include "CString.h"
#include "CFile.h"

class DSMutexSemaphore;

//-----------------------------------------------------------------------------
//	* Logging Flags
//
//		The user may turn logging on or off, and select from the type of logging
//		desired. Certain types of logging information are NOOPs if the
//		compile-time symbol DEBUG is not defined.
//
//-----------------------------------------------------------------------------

enum eLogFlags {
	kLogNone		= 0x00000000,			// Log no information
	kLogMeta		= 0x00000001,			// Info about the log itself
	kLogApplication	= 0x00000002,			// Application info

	// Listener
	kLogListener	= 0x00000010,

	// Handler
	kLogHandler		= 0x00000100,
	kLogMsgQueue	= 0x00000200,

	// Threads
	kLogThreads		= 0x00001000,			// Information about Threads

	// Endpoint
	kLogEndpoint	= 0x00002000,

	// Plugin
	kLogPlugin		= 0x00004000,

	// Proxy
	kLogConnection	= 0x00008000,

	// Transactions
	kLogMsgTrans	= 0x00010000,

	// TCP Endpoint
	kLogTCPEndpoint	= 0x00020000,

	// Assert
	kLogAssert		= 0xF0000000,

	// Everything
	kLogEverything	= 0x7FFFFFFF
};

// log types

typedef enum {
	keServerLog		= 1,
	keErrorLog,
	keDebugLog,
	keInfoLog
} eLogType;


//-----------------------------------------------------------------------------
//	* CLog: a little more than your basic log class.
//
//	This class is responsible for writing CStrings to permanent storage.
//	Limits can be placed on the size of the log file, in which case the class
//	will "wrap", writing over the oldest data. When the object is destroyed,
//	it will properly resequence the file.
//-----------------------------------------------------------------------------

class CLog
{
public:
	/**** Typedefs, enums, and constants. ****/
	// Constructor constants.
	enum {
		kFileWrap		= 0x1,
		kComments		= 0x2,
		kRollLog		= 0x4,
		kTimeDateStamp	= 0x20,
		kThreadInfo		= 0x80
	};

	enum {
		kLengthUnlimited	= -1UL,
		kLengthReasonable	= 0x8000,	// 32K
		kTypeDefault		= 'TEXT',
		kCreatorSimpleText	= 'ttxt',
		kCreatorCodeWarrior	= 'CWIE',
		kCreatorBBEdit		= 'R*ch'
	};

	// Function prototype for append hook.
	typedef void	(*AppendHook) ( const CString &line );

public:
	// Static methods
	static sInt32	Initialize			(	OptionBits	srvrFlags		= kLogEverything,
											OptionBits	errFlags		= kLogEverything,
											OptionBits	debugFlags		= kLogMeta,
											OptionBits	infoFlags		= kLogMeta,
											bool		inOpenDbgLog	= false,
											bool		inOpenInfoLog	= false );
	static void		Deinitialize		( void );
	static void		StartLogging		( eLogType inWhichLog, uInt32 inFlag );
	static void		StopLogging			( eLogType inWhichLog, uInt32 inFlag );
	static void		ToggleLogging		( eLogType inWhichLog, uInt32 inFlag );
	static bool		IsLogging			( eLogType inWhichLog, uInt32 inFlag );
	static void		StartDebugLog		( void );
	static void		StopDebugLog		( void );
	static void		StartErrorLog		( void );
	static void		StopErrorLog		( void );
	static void		StartInfoLog		( void );
	static void		StopInfoLog			( void );
	static CLog*	GetServerLog		( void );
	static CLog*	GetErrorLog			( void );
	static CLog*	GetDebugLog			( void );
	static CLog*	GetInfoLog			( void );

public:
				CLog (	const char		*file,
						uInt32			maxLength	= kLengthUnlimited,
						OptionBits		flags		= kThreadInfo,
						OSType			type		= kTypeDefault,
						OSType			creator		= kCreatorSimpleText );
	virtual		~CLog ( void );

	// New methods.
	virtual void	GetInfo		(	CFileSpec	&fileSpec,
									uInt32		&startOffset,
									uInt32		&dataLength,
									bool		&hasWrapped );

	virtual void	SetMaxLength	( uInt32	maxLength );
	virtual OSErr	Append			( const CString &line );
	virtual OSErr	ClearLog		( void );
	virtual void	AddHook			( AppendHook newHook );

	virtual long			Lock	( void );
	virtual void			UnLock	( void );


protected:
	// Class globals
	static OptionBits		fSrvrLogFlags;
	static OptionBits		fErrLogFlags;
	static OptionBits		fDbgLogFlags;
	static OptionBits		fInfoLogFlags;
	static CLog			   *fServerLog;
	static CLog			   *fErrorLog;
	static CLog			   *fDebugLog;
	static CLog			   *fInfoLog;
	static CString		   *fServerLogName;
	static CString		   *fErrorLogName;
	static CString		   *fDebugLogName;
	static CString		   *fInfoLogName;

	// Instance data
	CFileSpec			fFileSpec;		// Necessary for file moves after resequencing
	CFile			   *fFile;
	uInt32				fFlags;
	uInt32				fMaxLength;
	uInt32				fOffset;
	uInt32				fLength;
	AppendHook			fHooks[ 8 ];

	DSMutexSemaphore	   *fLock;

};


//-----------------------------------------------------------------------------
//	* Preprocessor Macros
//-----------------------------------------------------------------------------
#pragma mark **** CLog Convenience Macros ****

// assertion macros.
// Moved MailAssert from CThread.h

#if (defined(APP_DEBUG) || defined(DEBUG))

#define MyAssert(condition) \
			if (!(condition)) { ASSERTLOG(kLogEverything,"***** ASSERTION FAILED! *****"); }

#define SignalIf_(test) \
			if (test) { ASSERTLOG(kLogEverything, "***** Signal a failed test! *****"); }

#else		 // (defined(APP_DEBUG) || defined(DEBUG))

#define MyAssert(condtion)
#define SignalIf_(test)

#endif	// (defined(APP_DEBUG) || defined(DEBUG))


inline void SrvrLog ( long lType, const char *szpPattern, ... )
{
	if ( CLog::GetServerLog() != nil )
	{
		va_list	args;
		va_start( args, szpPattern );
		if ( CLog::IsLogging( keServerLog, lType ) )
		{
			CLog::GetServerLog()->Append( CString( szpPattern, args ) );
		}
	}
} // SrvrLog


inline void ErrLog ( long lType, const char *szpPattern, ... )
{
	if ( CLog::GetErrorLog() == nil )
	{
		CLog::StartErrorLog(); //create error log on demand
	}
	if ( CLog::GetErrorLog() != nil )
	{
		va_list	args;
		va_start( args, szpPattern );
		if ( CLog::IsLogging( keErrorLog, lType ) )
		{
			CLog::GetErrorLog()->Append( CString( szpPattern, args ) );
		}
	}
} // ErrLog


inline void DbgLog ( long lType, const char *szpPattern, ... )
{
	if ( CLog::GetDebugLog() != nil )
	{
		// Just in case it was deleted while I was waiting for it
		if ( CLog::GetDebugLog() != nil )
		{
			va_list	args;
			va_start( args, szpPattern );
			if ( CLog::IsLogging( keDebugLog, lType ) )
			{
				CLog::GetDebugLog()->Append( CString( szpPattern, args ) );
			}
		}
	}
} // DbgLog


inline void InfoLog ( long lType, const char *szpPattern, ... )
{
	if ( CLog::GetInfoLog() != nil )
	{
		// Just in case it was deleted while I was waiting for it
		if ( CLog::GetInfoLog() != nil )
		{
			va_list	args;
			va_start( args, szpPattern );
			if ( CLog::IsLogging( keInfoLog, lType ) )
			{
				CLog::GetInfoLog()->Append( CString( szpPattern, args ) );
			}
		}
	}
} // GetInfoLog


// Server log
#define SRVRLOG( flg, p0 )	::SrvrLog( flg, p0 );
#define SRVRLOG1( flg, p0, p1 )	::SrvrLog( flg, p0, p1 );
#define SRVRLOG2( flg, p0, p1, p2 )	::SrvrLog( flg, p0, p1, p2 );
#define SRVRLOG3( flg, p0, p1, p2, p3 )	::SrvrLog( flg, p0, p1, p2, p3 );
#define SRVRLOG4( flg, p0, p1, p2, p3, p4 )	::SrvrLog( flg, p0, p1, p2, p3, p4 );
#define SRVRLOG5( flg, p0, p1, p2, p3, p4, p5 )	::SrvrLog( flg, p0, p1, p2, p3, p4, p5 );
#define SRVRLOG6( flg, p0, p1, p2, p3, p4, p5, p6 )	::SrvrLog( flg, p0, p1, p2, p3, p4, p5, p6 );
#define SRVRLOG7( flg, p0, p1, p2, p3, p4, p5, p6, p7 )	::SrvrLog( flg, p0, p1, p2, p3, p4, p5, p6, p7 );
#define SRVRLOG8( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 )	::SrvrLog( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 );

// Error log
#define ERRORLOG( flg, p0 )	::ErrLog( flg, p0 );
#define ERRORLOG1( flg, p0, p1 )	::ErrLog( flg, p0, p1 );
#define ERRORLOG2( flg, p0, p1, p2 )	::ErrLog( flg, p0, p1, p2 );
#define ERRORLOG3( flg, p0, p1, p2, p3 )	::ErrLog( flg, p0, p1, p2, p3 );
#define ERRORLOG4( flg, p0, p1, p2, p3, p4 )	::ErrLog( flg, p0, p1, p2, p3, p4 );
#define ERRORLOG5( flg, p0, p1, p2, p3, p4, p5 )	::ErrLog( flg, p0, p1, p2, p3, p4, p5 );
#define ERRORLOG6( flg, p0, p1, p2, p3, p4, p5, p6 )	::ErrLog( flg, p0, p1, p2, p3, p4, p5, p6 );
#define ERRORLOG7( flg, p0, p1, p2, p3, p4, p5, p6, p7 )	::ErrLog( flg, p0, p1, p2, p3, p4, p5, p6, p7 );
#define ERRORLOG8( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 )	::ErrLog( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 );

// Debug log
#define DBGLOG( flg, p0 )	::DbgLog( flg, p0 );
#define DBGLOG1( flg, p0, p1 )	::DbgLog( flg, p0, p1 );
#define DBGLOG2( flg, p0, p1, p2 )	::DbgLog( flg, p0, p1, p2 );
#define DBGLOG3( flg, p0, p1, p2, p3 )	::DbgLog( flg, p0, p1, p2, p3 );
#define DBGLOG4( flg, p0, p1, p2, p3, p4 )	::DbgLog( flg, p0, p1, p2, p3, p4 );
#define DBGLOG5( flg, p0, p1, p2, p3, p4, p5 )	::DbgLog( flg, p0, p1, p2, p3, p4, p5 );
#define DBGLOG6( flg, p0, p1, p2, p3, p4, p5, p6 )	::DbgLog( flg, p0, p1, p2, p3, p4, p5, p6 );
#define DBGLOG7( flg, p0, p1, p2, p3, p4, p5, p6, p7 )	::DbgLog( flg, p0, p1, p2, p3, p4, p5, p6, p7 );
#define DBGLOG8( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 )	::DbgLog( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 );

#define INFOLOG( flg, p0 )	::InfoLog( flg, p0 );
#define INFOLOG1( flg, p0, p1 )	::InfoLog( flg, p0, p1 );
#define INFOLOG2( flg, p0, p1, p2 )	::InfoLog( flg, p0, p1, p2 );
#define INFOLOG3( flg, p0, p1, p2, p3 )	::InfoLog( flg, p0, p1, p2, p3 );
#define INFOLOG4( flg, p0, p1, p2, p3, p4 )	::InfoLog( flg, p0, p1, p2, p3, p4 );
#define INFOLOG5( flg, p0, p1, p2, p3, p4, p5 )	::InfoLog( flg, p0, p1, p2, p3, p4, p5 );
#define INFOLOG6( flg, p0, p1, p2, p3, p4, p5, p6 )	::InfoLog( flg, p0, p1, p2, p3, p4, p5, p6 );
#define INFOLOG7( flg, p0, p1, p2, p3, p4, p5, p6, p7 )	::InfoLog( flg, p0, p1, p2, p3, p4, p5, p6, p7 );
#define INFOLOG8( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 )	::InfoLog( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 );

#ifdef XXXX

#define INFOLOG( flg, p0 )
#define INFOLOG1( flg, p0, p1 )
#define INFOLOG2( flg, p0, p1, p2 )
#define INFOLOG3( flg, p0, p1, p2, p3 )
#define INFOLOG4( flg, p0, p1, p2, p3, p4 )
#define INFOLOG5( flg, p0, p1, p2, p3, p4, p5 )
#define INFOLOG6( flg, p0, p1, p2, p3, p4, p5, p6 )
#define INFOLOG7( flg, p0, p1, p2, p3, p4, p5, p6, p7 )
#define INFOLOG8( flg, p0, p1, p2, p3, p4, p5, p6, p7, p8 )

#endif

#define LogAssert_(test)													\
	do {																	\
		if ( !(test) )														\
		{																	\
			DBGLOG2( kLogAssert, "Assert in %s at %d", __FILE__, __LINE__ );	\
		}																	\
	} while (false)

#define LogErrAssert_(test,err)																	\
	do {																						\
		if ( !(test) )																			\
		{																						\
			DBGLOG3( kLogAssert, "Assert in %s at %d with error = %d.", __FILE__, __LINE__, err );	\
		}																						\
	} while (false)

#endif	// __CLog_h__
