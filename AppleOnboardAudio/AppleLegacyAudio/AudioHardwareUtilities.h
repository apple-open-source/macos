/*
 *  AudioHardwareUtilities.h
 *  Project : Apple02Audio
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 * 
 * A series of macros used in the Apple02Audio code in order to simplify
 * reading.
 *
 */


#ifndef __AUDIOHARDWAREUTILITIES__
#define __AUDIOHARDWAREUTILITIES__

//#define	LOG_FAIL
#ifdef DEBUG
#ifndef LOG_FAIL
#define	LOG_FAIL
#endif
#endif

#define	kDebugIOSleepDelay		20
// Debugging help
#ifdef DEBUGLOG
// #define IOSleep(x) ;
#define debugIOLog( message ) \
	{IOLog( message ); IOSleep(kDebugIOSleepDelay);}
#define debug2IOLog( message, arg2 ) \
	{IOLog( message, arg2 ); IOSleep(kDebugIOSleepDelay);}
#define debug3IOLog( message, arg2, arg3 ) \
	{IOLog( message, arg2, arg3 ); IOSleep(kDebugIOSleepDelay);}
#define debug4IOLog( message, arg2, arg3, arg4 ) \
	{IOLog( message, arg2, arg3, arg4 ); IOSleep(kDebugIOSleepDelay);}
#define debug5IOLog( message, arg2, arg3, arg4, arg5 ) \
	{IOLog( message, arg2, arg3, arg4, arg5 ); IOSleep(kDebugIOSleepDelay);}
#define debug6IOLog( message, arg2, arg3, arg4, arg5, arg6 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6 ); IOSleep(kDebugIOSleepDelay);}
#define debug7IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 ); IOSleep(kDebugIOSleepDelay);}
#define debug8IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ); IOSleep(kDebugIOSleepDelay);}
#define debug9IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) \
    {IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ); IOSleep(kDebugIOSleepDelay);}
#else
#define debugIOLog( message ) ;
#define debug2IOLog( message, arg2 ) ;
#define debug3IOLog( message, arg2, arg3 ) ;
#define debug4IOLog( message, arg2, arg3, arg4 ) ;
#define debug5IOLog( message, arg2, arg3, arg4, arg5 ) ;
#define debug6IOLog( message, arg2, arg3, arg4, arg5, arg6 ) ;
#define debug7IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 ) ;
#define debug8IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) ;
#define debug9IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) ;
#endif


#ifdef DEBUGLOG
#define debugIrqIOLog( message ) \
	{IOLog( message );}
#define debug2IrqIOLog( message, arg2 ) \
	{IOLog( message, arg2 );}
#define debug3IrqIOLog( message, arg2, arg3 ) \
	{IOLog( message, arg2, arg3 );}
#define debug4IrqIOLog( message, arg2, arg3, arg4 ) \
	{IOLog( message, arg2, arg3, arg4 );}
#define debug5IrqIOLog( message, arg2, arg3, arg4, arg5 ) \
	{IOLog( message, arg2, arg3, arg4, arg5 );}
#define debug6IrqIOLog( message, arg2, arg3, arg4, arg5, arg6 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6 );}
#define debug7IrqIOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 );}
#define debug8IrqIOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 );}
#define debug9IrqIOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) \
    {IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 );}
#else
#define debugIrqIOLog( message ) ;
#define debug2IrqIOLog( message, arg2 ) ;
#define debug3IrqIOLog( message, arg2, arg3 ) ;
#define debug4IrqIOLog( message, arg2, arg3, arg4 ) ;
#define debug5IrqIOLog( message, arg2, arg3, arg4, arg5 ) ;
#define debug6IrqIOLog( message, arg2, arg3, arg4, arg5, arg6 ) ;
#define debug7IrqIOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 ) ;
#define debug8IrqIOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) ;
#define debug9IrqIOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) ;
#endif

#define DEBUG_IOLOG debugIOLog
#define DEBUG2_IOLOG debug2IOLog
#define DEBUG3_IOLOG debug3IOLog


#ifdef DEBUGLOG 
    #define CLOG( stuff )  IOLog( stuff )
#else 
    #define CLOG( stuff )  ; 
#endif

// Bytes parsing
#define NEXTENDOFSTRING(bytes, idx)  while('\0' != bytes[idx]) idx++;
#define ASSIGNSTARTSTRING(startidx, parser)  startidx = ++parser;
#define ASSIGNSTOPSTRING(stopidx, parser)  stopidx = parser+1;
#define NEXTENDOFWORD(bytes, idx, size)  while(' ' != bytes[idx] && idx <= size) idx++;
#define ASSIGNNEXTWORD(bytes, startidx, stopidx)  do {stopidx++; startidx = stopidx;}\
                            while((bytes[stopidx -1] == ' ') && (bytes[stopidx] == ' '))
#define READWORDASNUMBER(result, bytesPtr, startidx, stopidx) \
    {char temp[stopidx-startidx+1];memcpy(temp, bytesPtr+startidx, stopidx-startidx);temp[stopidx-startidx] = 0; result = getStringAsNumber(temp);};

// Macros definitions
#define CLEAN_RELEASE(thingPtr) if(thingPtr) {thingPtr->release(); thingPtr=NULL;}

//	-----------------------------------------------------------------
#define SoundAssertionMessage( cond, file, line, handler ) \
    "Sound assertion \"" #cond "\" failed in " #file " at line " #line " goto " #handler "\n"

#define SoundAssertionFailed( cond, file, line, handler ) \
    IOLog( SoundAssertionMessage( cond, file, line, handler ));

//	-----------------------------------------------------------------
#ifdef LOG_FAIL
#define	FailIf( cond, handler )										\
    if( cond ){														\
        SoundAssertionFailed( cond, __FILE__, __LINE__, handler )	\
        goto handler; 												\
    }
#else
#define	FailIf( cond, handler )										\
    if( cond ){														\
        goto handler; 												\
    }
#endif

#ifdef LOG_FAIL
#define	FAIL_IF( cond, handler )										\
    if( cond ){														\
        SoundAssertionFailed( cond, __FILE__, __LINE__, handler )	\
        goto handler; 												\
    }
#else
#define	FAIL_IF( cond, handler )										\
    if( cond ){														\
        goto handler; 												\
    }
#endif


//	-----------------------------------------------------------------
#ifdef LOG_FAIL
#define	FailWithAction( cond, action, handler )						\
    if( cond ){														\
        SoundAssertionFailed( cond, __FILE__, __LINE__, handler )	\
            { action; }												\
        goto handler; 												\
    }
#else
#define	FailWithAction( cond, action, handler )						\
    if( cond ){														\
            { action; }												\
        goto handler; 												\
    }
#endif

//	-----------------------------------------------------------------
#ifdef LOG_FAIL
#define FailMessage(cond)		if (cond) SoundAssertionFailed(cond, __FILE__, __LINE__, handler)
#else
#define FailMessage(cond)		{}
#endif

#endif //__AUDIOHARDWAREUTILITIES__
