/*
 *  CMLog.h
 *  DisplayServices
 *
 *  Created by jfu on Wed Dec 06 2000.
 *  Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
 *
 */

// еее This file must be ANSI C compliant, not Objective-C еее //

#import "CMDebug.h"

#if DEBUG
	
	#define CMLogArg0( message )						CMLogDebug( message )
	#define CMLogArg1( message, a1 )					CMLogDebug( message, a1 )
	#define CMLogArg2( message, a1, a2 )				CMLogDebug( message, a1, a2 )
	#define CMLogArg3( message, a1, a2, a3 )			CMLogDebug( message, a1, a2, a3 )
	#define CMLogArg4( message, a1, a2, a3, a4 )		CMLogDebug( message, a1, a2, a3, a4 )
	#define CMLogArg5( message, a1, a2, a3, a4, a5 )	CMLogDebug( message, a1, a2, a3, a4, a5 )

	//
	// writes a printf-style line to /tmp/cm.log
	//
	void CMLogDebug( const char *fmt, ... );
	
	//
	// writes the given printf-style line to /tmp/CM.log, followed
	// by the contents of nsRect, and ending with a newline (independently
	// of any newline characters that may have been specified in fmt).
	// 
	//void CMLogDumpNSRectDebug( void *nsRect, const char *fmt, ... );
	
	//
	// writes the given printf-style line to /tmp/cm.log, followed
	// by the contents of nsPoint, and ending with a newline (independently
	// of any newline characters that may have been specified in fmt).
	// 
	//void CMLogDumpNSPointDebug( void *nsPoint , const char *fmt, ... );

#else

	#define CMLogArg0( message )
	#define CMLogArg1( message, a1 )
	#define CMLogArg2( message, a1, a2 )
	#define CMLogArg3( message, a1, a2, a3 )
	#define CMLogArg4( message, a1, a2, a3, a4 )
	#define CMLogArg5( message, a1, a2, a3, a4, a5 )

#endif
