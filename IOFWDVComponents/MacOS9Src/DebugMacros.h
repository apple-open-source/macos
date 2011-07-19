/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
/*
	File:		DebugMacros.h

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	й 1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(RS)	Richard Sepulveda

	Change History (most recent first):

		 <2>	 6/28/99	RS		Added new macro FailMessageVal().
		 <1>	  6/7/99	RS		first checked in
		 <2>	  6/7/99	RS		Added additional debug macros that will allow the programmer to
									include an additional string and value in the DebugStr message.
*/

#ifndef __DEBUGMACROS__
#define __DEBUGMACROS__

#include <DriverServices.h>
#include <NumberFormatting.h>

#include "OxcartDebug.h"

#undef AssertionFailed

// if/branch macro
#define BranchIf(condition, label)	if((condition)) goto label;

#define	AssertionMessage(cond, file, line, handler)	\
			"\pAssertion \"" #cond "\" failed in " #file " at line " #line " goto " #handler

#define	AssertionFailed(cond, file, line, handler)	\
			OXCART_DEBUGSTR( (StringPtr) AssertionMessage (cond, file, line, handler) );

#define	AssertionMessageString(cond, file, line, handler)	\
			"\pAssertion \"" #cond "\" failed in " #file " at line " #line " goto " #handler " value -> "

#define	AssertionFailedString(cond, file, line, handler, str)						\
			{																		\
			Str255 mystr21;															\
			PStrCopy( mystr21, AssertionMessageString (cond, file, line, handler));	\
			PStrCat( mystr21, str);													\
			DebugStr( mystr21);														\
			}

#ifdef DEBUG
#define FailWithVal(cond, handler, num)									\
	if (cond) {															\
		Str255 mystr;													\
		NumToString( num, mystr);										\
		AssertionFailedString(cond, __FILE__, __LINE__, handler, mystr)	\
		goto handler;													\
	}
#else
#define FailWithVal(cond, handler,num)					\
	if (cond) {											\
		goto handler;									\
	}
#endif DEBUG

#ifdef DEBUG
#define FailWithStringVal(cond, handler, str, num)								\
	if (cond) {																	\
		Str255 mystr, mystr2;													\
		NumToString( num, mystr);												\
		PStrCopy( mystr2, str);													\
		PStrCat( mystr2, mystr);												\
		AssertionFailedString(cond, __FILE__, __LINE__, handler, mystr2)		\
		goto handler;															\
	}
#else
#define FailWithStringVal(cond, handler, str, num)		\
	if (cond) {											\
		goto handler;									\
	}
#endif DEBUG

#ifdef DEBUG
#define FailWithString(cond, handler, str)								\
	if (cond) {															\
		AssertionFailedString(cond, __FILE__, __LINE__, handler, str)	\
		goto handler;													\
	}
#else
#define FailWithString(cond, handler, str, num)			\
	if (cond) {											\
		goto handler;									\
	}
#endif DEBUG

// This checks for the exception, and if true then goto handler
#ifdef DEBUG
#define FailIf(cond, handler)								\
	if (cond) {												\
		AssertionFailed(cond, __FILE__, __LINE__, handler)	\
		goto handler;										\
	}
#else
#define FailIf(cond, handler)								\
	if (cond) {												\
		goto handler;										\
	}
#endif

#ifdef DEBUG
#define FailWithActionVal(cond, action, handler, num)					\
	if (cond) {															\
		Str255 mystr;													\
		NumToString( num, mystr);										\
		AssertionFailedString(cond, __FILE__, __LINE__, handler, mystr)	\
		{ action; }														\
		goto handler;													\
	}
#else
#define FailWithActionVal(cond, action, handler, num)	\
	if (cond) {											\
		{ action; }										\
		goto handler;									\
	}
#endif DEBUG

// This checks for the exception, and if true do the action and goto handler
#ifdef DEBUG
#define FailWithAction(cond, action, handler)				\
	if (cond) {												\
		AssertionFailed(cond, __FILE__, __LINE__, handler)	\
		{ action; }											\
		goto handler;										\
	}
#else
#define FailWithAction(cond, action, handler)				\
	if (cond) {												\
		{ action; }											\
		goto handler;										\
	}
#endif

// This will insert debugging code in the application to check conditions
// and displays the condition in the debugger if true.  This code is
// completely removed in non-debug builds.

#ifdef DEBUG
#define FailMessage(cond)		if (cond) AssertionFailed(cond, __FILE__, __LINE__, handler)
#else
#define FailMessage(cond)		{}
#endif

#ifdef DEBUG
#define FailMessageVal(cond, num)											\
	if (cond) {																\
		Str255 __mystr;														\
		NumToString( num, __mystr);											\
		AssertionFailedString(cond, __FILE__, __LINE__, handler, __mystr)	\
	}
#else
#define FailMessageVal(cond, num)	{}
#endif	

// This allows you to test for the result of a condition (i.e. CloseComponent)
// and break if it returns a non zero result, otherwise it ignores the result.
// When a non-debug build is done, the result is ignored.

#ifdef DEBUG
#define ErrorMessage(cond)		if (cond) AssertionFailed(cond, __FILE__, __LINE__, handler)
#else
#define ErrorMessage(cond)		(cond)
#endif

// This will display a given message in the debugger, this code is completely
// removed in non-debug builds.

#ifdef DEBUG
#define DebugMessage(s)			DebugString((ConstStr255Param)s)
#else
#define DebugMessage(s)			{}
#endif

// еее THESE MACROS ARE ONLY ACTIVE IF DEBUGVERBOSE IS DEFINED еее
// This checks for the exception, and if true then goto handler

#if defined(DEBUG) && defined(DEBUGVERBOSE)
#define FailIfVerbose(cond, handler)						\
	if (cond) {												\
		AssertionFailed(cond, __FILE__, __LINE__, handler)	\
		goto handler;										\
	}
#else
#define FailIfVerbose(cond, handler)						\
	if (cond) {												\
		goto handler;										\
	}
#endif

// This checks for the exception, and if true do the action and goto handler

#if defined(DEBUG) && defined(DEBUGVERBOSE)
#define FailWithActionVerbose(cond, action, handler)		\
	if (cond) {												\
		AssertionFailed(cond, __FILE__, __LINE__, handler)	\
		{ action; }											\
		goto handler;										\
	}
#else
#define FailWithActionVerbose(cond, action, handler)		\
	if (cond) {												\
		{ action; }											\
		goto handler;										\
	}
#endif

// This will insert debugging code in the application to check conditions
// and displays the condition in the debugger if true.  This code is
// completely removed in non-debug builds.

#if defined(DEBUG) && defined(DEBUGVERBOSE)
#define FailMessageVerbose(cond)	if (cond) AssertionFailed(cond, __FILE__, __LINE__, handler)
#else
#define FailMessageVerbose(cond)	{}
#endif

// This allows you to test for the result of a condition (i.e. CloseComponent)
// and break if it returns a non zero result, otherwise it ignores the result.
// When a non-debug build is done, the result is ignored.

#if defined(DEBUG) && defined(DEBUGVERBOSE)
#define ErrorMessageVerbose(cond)	if (cond) AssertionFailed(cond, __FILE__, __LINE__, handler)
#else
#define ErrorMessageVerbose(cond)	(cond)
#endif

// This will insert debugging code in the application to check conditions
// and displays the condition in the debugger if true.  This code is
// completely removed in non-debug builds.

#ifdef DEBUG
#define VDQFailMessage(cond,s)								\
	if (cond) {											\
		DebugStr ((ConstStr255Param)"\p"#s);			\
	}
#else
#define VDQFailMessage(cond, s)							\
	((void)	0)
#endif DEBUG

// This allows you to test for the result of a condition (i.e. CloseComponent)
// and break if it returns a non zero result, otherwise it ignores the result.
// When a non-debug build is done, the result is ignored.

#ifdef DEBUG
#define VDQErrorMessage(cond)		if (cond) DebugStr((ConstStr255Param)"\p"#cond)
#else
#define VDQErrorMessage(cond)		(cond)
#endif DEBUG

// This will display a given message in the debugger, this code is completely
// removed in non-debug builds.

#ifdef DEBUG
#define VDQDebugMessage(s)			DebugStr ((ConstStr255Param)"\p"#s)
#else
#define VDQDebugMessage(s)			((void)	0)
#endif DEBUG


#if 0
//#ifdef DEBUG

#undef DisposHandle
#define DisposeHandle(thandle)		{ DisposeHandle(thandle); if (MemError() != noErr) DebugMessage(wackyHandle); }
#define DisposHandle DisposeHandle

#undef DisposPtr
#define DisposePtr(tptr)			{ DisposePtr(tptr); if (MemError() != noErr) DebugMessage(wackyPointer); }
#define DisposPtr DisposePtr

#endif

#endif
