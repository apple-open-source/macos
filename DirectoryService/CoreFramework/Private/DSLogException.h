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
 * @header DSLogException
 */

#ifndef __DSLogException_h__
#define __DSLogException_h__	1

#include "PrivateTypes.h"

// Log macros

#define LogThenThrowIfDSErrorMacro( inDSError )																\
{																											\
	sInt32 aDSError = inDSError;																			\
	if ( aDSError != eDSNoErr )																				\
	{																										\
		LOG3( kStdErr, "*** DS Error: File: %s. Line: %d. Error = %d\n", __FILE__, __LINE__, aDSError );	\
		throw( aDSError );																					\
	}																										\
} if (true)

#define LogThenThrowIfTrueMacro( inCheck, inDSError )																\
{																													\
	sInt32	aDSError	= inDSError;																				\
	if ( inCheck )																									\
	{																												\
		LOG3( kStdErr, "*** DS If True Error: File: %s. Line: %d. Error = %d\n", __FILE__, __LINE__, aDSError );	\
		throw( aDSError );																							\
	}																												\
} if (true)

#define LogThenThrowIfNilMacro( inPtr, inDSError )																\
{																												\
	sInt32	aDSError	= inDSError;																			\
	if ( inPtr == nil )																							\
	{																											\
		LOG3( kStdErr, "*** DS If nil Error: File: %s. Line: %d. Error = %d\n", __FILE__, __LINE__, aDSError );	\
		throw( aDSError );																						\
	}																											\
} if (true)

#define LogThenThrowThisIfDSErrorMacro( inDSError, inThrowThisError )																			\
{																																				\
	sInt32 aDSError = inDSError;																												\
	if ( aDSError != eDSNoErr )																													\
	{																																			\
		LOG4( kStdErr, "*** DS Error: File: %s. Line: %d. Error = %d. Thrown Error = %d\n", __FILE__, __LINE__, aDSError, inThrowThisError );	\
		throw( inThrowThisError );																												\
	}																																			\
} if (true)

#endif	// __DSLogException_h__
