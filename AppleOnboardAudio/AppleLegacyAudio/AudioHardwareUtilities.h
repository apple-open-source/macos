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

#ifdef DEBUGLOG
	#define DEBUG_LEVEL 1
#endif

#include <IOKit/usb/IOUSBLog.h>

//	NOTE:  Use of 'LOG_FAIL' allows the fail messages to appear in the log without
//	all of the standard logging associated with a DEBUG build (i.e. hopefully failures only).
#define	LOG_FAIL

#ifdef DEBUG
	#ifndef LOG_FAIL
		#define	LOG_FAIL
	#endif
#endif

#ifdef DEBUGLOG
	#define debugIOLog( level, message... ) \
		do {USBLog( level, message );} while (0)
#else
	#define debugIOLog( message... ) ;
#endif

#define	IOLog1Float( preString, inFloat, postString )				\
{																	\
	char fstr[32];													\
	float2string(inFloat, fstr);									\
	IOLog ("%s%s%s", preString, fstr, postString);					\
}																

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
#ifdef LOG_FAIL
	#ifndef DEBUGLOG
		#define SoundAssertionMessage( cond, file, line, handler ) \
			"Sound assertion \"" #cond "\" failed in " #file " at line " #line " goto " #handler "\n"
	#else
		#define SoundAssertionMessage( cond, file, line, handler ) \
			"Sound assertion \"" #cond "\" failed in " #file " at line " #line " goto " #handler
	#endif
#else
	#define SoundAssertionMessage( cond, file, line, handler ) \
		"Sound assertion \"" #cond "\" failed in " #file " at line " #line " goto " #handler
#endif
	

#ifdef LOG_FAIL
	#ifndef DEBUGLOG
		#define SoundAssertionFailed( cond, file, line, handler ) \
			IOLog(SoundAssertionMessage( cond, file, line, handler ));
	#else
		#define SoundAssertionFailed( cond, file, line, handler ) \
			debugIOLog( 1, SoundAssertionMessage( cond, file, line, handler ));
	#endif
#else
	#define SoundAssertionFailed( cond, file, line, handler ) \
		debugIOLog( 1, SoundAssertionMessage( cond, file, line, handler ));
#endif

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
