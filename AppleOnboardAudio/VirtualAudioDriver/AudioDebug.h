//--------------------------------------------------------------------------------
//  AudioDebug.h
//  VirtualAudioDriver
//
//  Created by Matt Mora on Thu Apr 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//
//--------------------------------------------------------------------------------


#ifndef _AUDIODEBUG_H_
#define _AUDIODEBUG_H_

#include <libkern/OSTypes.h>

//	-----------------------------------------------------------------
#define SoundAssertionMessage( cond, file, line, handler ) \
    "Sound assertion \"" #cond "\" failed in " #file " at line " #line " goto " #handler "\n"

#define SoundAssertionFailed( cond, file, line, handler ) \
    IOLog( SoundAssertionMessage( cond, file, line, handler ));

//	-----------------------------------------------------------------
#ifdef DEBUG
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
#ifdef DEBUG
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
#ifdef DEBUG
#define FailMessage(cond, handler)									\
	if (cond) {														\
		SoundAssertionFailed(cond, __FILE__, __LINE__, handler)		\
		goto handler;												\
	}
#else
#define FailMessage(cond, handler)									\
	if (cond) {														\
		goto handler;												\
	}
#endif

//	-----------------------------------------------------------------
#ifdef DEBUGLOG
#define debugIOLog( message ) \
	{IOLog( message ); IOSleep(20);}
#define debug2IOLog( message, arg2 ) \
	{IOLog( message, arg2 ); IOSleep(20);}
#define debug3IOLog( message, arg2, arg3 ) \
	{IOLog( message, arg2, arg3 ); IOSleep(20);}
#define debug4IOLog( message, arg2, arg3, arg4 ) \
	{IOLog( message, arg2, arg3, arg4 ); IOSleep(20);}
#define debug5IOLog( message, arg2, arg3, arg4, arg5 ) \
	{IOLog( message, arg2, arg3, arg4, arg5 ); IOSleep(20);}
#define debug6IOLog( message, arg2, arg3, arg4, arg5, arg6 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6 ); IOSleep(20);}
#define debug7IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 ); IOSleep(20);}
#define debug8IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) \
	{IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ); IOSleep(20);}
#else
#define debugIOLog( message ) ;
#define debug2IOLog( message, arg2 ) ;
#define debug3IOLog( message, arg2, arg3 ) ;
#define debug4IOLog( message, arg2, arg3, arg4 ) ;
#define debug5IOLog( message, arg2, arg3, arg4, arg5 ) ;
#define debug6IOLog( message, arg2, arg3, arg4, arg5, arg6 ) ;
#define debug7IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7 ) ;
#define debug8IOLog( message, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) ;
#endif

#endif /* _AUDIODEBUG_H_ */
