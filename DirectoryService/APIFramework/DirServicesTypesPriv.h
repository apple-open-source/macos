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
 * @header DirServicesTypesPriv
 */

#ifndef __DirServicesTypesPriv_h__
#define	__DirServicesTypesPriv_h__	1

#include <stdio.h>

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

#ifndef nil
	#define nil NULL
#endif

#endif  // DirServicesTypesPriv
