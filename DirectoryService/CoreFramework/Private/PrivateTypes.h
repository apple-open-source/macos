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
 * @header PrivateTypes
 */

#ifndef __PrivateTypes_h__
#define	__PrivateTypes_h__	1

#include "DirServicesTypes.h"

typedef		char					sInt8;
typedef		unsigned char			uInt8;

typedef		short					sInt16;
typedef		unsigned short			uInt16;

typedef		long					sInt32;
typedef		unsigned long			uInt32;

typedef		long long				sInt64;
typedef		unsigned long long		uInt64;

typedef		unsigned char			Byte;
typedef		signed char				SignedByte;

typedef unsigned char *				StringPtr;

typedef unsigned long 				FourCharCode;

typedef FourCharCode 				OSType;
typedef FourCharCode 				ResType;
typedef OSType *					OSTypePtr;
typedef ResType *					ResTypePtr;

typedef sInt16 						OSErr;
typedef sInt32 						OSStatus;

typedef uInt32 						OptionBits;

typedef unsigned char				Boolean;

#ifdef DSDEBUGFW
	#include <syslog.h>

	#define kStdErr LOG_INFO

	#define LOG syslog
	#define LOG1 syslog
	#define LOG2 syslog
	#define LOG3 syslog
	#define LOG4 syslog
#else
#ifdef DEBUG
	#include <stdio.h>

	#define kStdErr stderr

	#define LOG fprintf
	#define LOG1 fprintf
	#define LOG2 fprintf
	#define LOG3 fprintf
	#define LOG4 fprintf
#else
	#define LOG( flg, msg )
	#define LOG1( flg, msg, p1 )
	#define LOG2( flg, msg, p1, p2)
	#define LOG3( flg, msg, p1, p2, p3)
	#define LOG4( flg, msg, p1, p2, p3, p4)
#endif
#endif

/* errors used originally from MacErrors.h but prefixed with ds_ */
/* CoreServices.h has been fully removed from DirectoryService */
enum {
    ds_readErr                     = -19,		/*I/O System Errors*/
    ds_writErr                     = -20,		/*I/O System Errors*/
    ds_fnOpnErr                    = -38,		/*File not open*/
    ds_fnfErr                      = -43,		/*File not found*/
    ds_gfpErr                      = -52,		/*get file position error*/
    ds_permErr                     = -54		/*permissions error (on file open)*/
};

#ifndef nil
	#define nil NULL
#endif

typedef enum
{
	kNoScriptCode		= 0,
	kUniCodeScript		= 1,
	kASCIICodeScript	= 2,	// means fBufferData is a valid CString
	kUnKnownScript		= 3

} eScriptCode;

typedef struct
{
	unsigned long		fBufferSize;
	unsigned long		fBufferLength;

	tDataNodePtr		fPrevPtr;
	tDataNodePtr		fNextPtr;
	uInt32				fType;
	eScriptCode			fScriptCode;

	char				fBufferData[ 1 ];
} tDataBufferPriv;

typedef enum {
	kUnknownNodeType		= 0x00000000,
	kDirNodeType			= 0x00000001,
	kLocalNodeType			= 0x00000002,
	kSearchNodeType			= 0x00000004,
	kConfigNodeType			= 0x00000008,
	kLocalHostedType		= 0x00000010,
	kDefaultNetworkNodeType		= 0x00000020,
	kContactsSearchNodeType	= 0x00000040,
	kNetworkSearchNodeType	= 0x00000080
} eDirNodeType;

typedef enum {
	kDSRefStateUnknown		= 0,
	kDSRefStateValid		= 1,
	kDSRefStateInvalid		= 2,
	kDSRefStateSuspended	= 3
} eDSRefState;

typedef enum {
	kDSEvalutateState = 1
} eDSTransitionType;

#endif
