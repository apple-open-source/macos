/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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


#ifndef __ATA_SMART_LIB_PRIV_H__
#define __ATA_SMART_LIB_PRIV_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Structures
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

typedef struct ATASMARTReadLogStruct
{
	UInt8			numSectors;
	UInt8			logAddress;
	void *			buffer;
	UInt32			bufferSize;
} ATASMARTReadLogStruct;

typedef struct ATASMARTWriteLogStruct
{
	UInt8			numSectors;
	UInt8			logAddress;
	const void *	buffer;
	UInt32			bufferSize;
} ATASMARTWriteLogStruct;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

enum
{
	kIOATASMARTUserClientAccessBit		= 16,
	kIOATASMARTUserClientAccessMask		= (1 << kIOATASMARTUserClientAccessBit)
};

enum
{
	kIOATASMARTLibConnection = 12
};

enum
{
	kIOATASMARTEnableDisableOperations				= 0,	// kIOUCScalarIScalarO, 1, 0
	kIOATASMARTEnableDisableAutoSave				= 1,	// kIOUCScalarIScalarO, 1, 0
	kIOATASMARTReturnStatus							= 2,	// kIOUCScalarIScalarO, 0, 1
	kIOATASMARTExecuteOffLineImmediate				= 3,	// kIOUCScalarIScalarO, 1, 0
	kIOATASMARTReadData								= 4,	// kIOUCScalarIStructO, 1, 0
	kIOATASMARTReadDataThresholds					= 5,	// kIOUCScalarIStructO, 1, 0
	kIOATASMARTReadLogAtAddress						= 6,	// kIOUCScalarIStructI, 0, sizeof (ATASMARTReadLogStruct)
	kIOATASMARTWriteLogAtAddress					= 7,	// kIOUCScalarIStructI, 0, sizeof (ATASMARTWriteLogStruct)
	
	kIOATASMARTMethodCount
};


#define kATASMARTUserClientClassKey			"ATASMARTUserClient"
#define kATASMARTUserClientTypeIDKey		"24514B7A-2804-11D6-8A02-003065704866"
#define kATASMARTUserClientLibLocationKey	"IOATABlockStorage.kext/Contents/PlugIns/ATASMARTLib.plugin"


#endif	/* __ATA_SMART_LIB_PRIV_H__ */