//
//  CMLog.m
//  DisplayServices
//
//  Created by jfu on Wed Dec 06 2000.
//  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "stdarg.h"
#import "CMDebug.h"
#import "CMLog.h"

#if DEBUG

// Private Prototypes
static void _CMLog( const char *fmt, va_list listp );


//***************************************************************************
// CMLogDebug
//
//---------------------------------------------------------------------------
void CMLogDebug( const char *fmt, ... )
{
	va_list listp;

	va_start( listp, fmt );
	_CMLog( fmt, listp );
	va_end( listp );
}

//***************************************************************************
// _CMLog
//
//---------------------------------------------------------------------------
static void _CMLog( const char *fmt, va_list listp )
{
	static FILE *logFile = NULL;
	
	if ( logFile == NULL )
		logFile = fopen( "/tmp/cm.log", "a" );

	if ( logFile )
	{
		//--- Log the reference time before doing the rest ---
		fprintf( logFile, "[%1.5f] ", [NSDate timeIntervalSinceReferenceDate] );
		
		vfprintf( logFile, fmt, listp );
		fflush( logFile );
	}
}

#endif
