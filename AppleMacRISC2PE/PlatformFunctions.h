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
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _PLATFORMFUNCTIONS_H
#define _PLATFORMFUNCTIONS_H

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
//#define DEBUG 1

#ifdef DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Macro to add an object to an OSSet, even if the OSSet is not yet allocated
// obj is an OSObject* (or pointer to instance of OSObject-derived class)
// set is an OSSet*

#define ADD_OBJ_TO_SET(obj, set)										\
{	if (set)															\
	{																	\
		if (!(set)->setObject(obj))										\
		{																\
			DLOG("Error adding to set\n");					\
			return(-1);													\
		}																\
	}																	\
	else																\
	{																	\
		(set) = OSSet::withObjects(&(const OSObject *)(obj), 1, 1);		\
		if (!(set))														\
		{																\
			DLOG("Error creating set\n");						\
			return(-1);													\
		}																\
	}																	\
}

// Device tree property keys, platform function prefixes
#define kFunctionProvidedPrefix		"platform-do-"
#define kFunctionRequiredPrefix		"platform-"
#define kFunctionGetTargetPrefix	"gettarget-"
#define kFunctionRegisterPrefix		"register-"
#define kFunctionUnregisterPrefix	"unregister-"
#define kFunctionEvtEnablePrefix	"enable-"
#define kFunctionEvtDisablePrefix	"disable-"

// Platform function flags - UInt32
enum {
	kFlagOnInit		= 0x80000000,
	kFlagOnTerm		= 0x40000000,
	kFlagOnSleep	= 0x20000000,
	kFlagOnWake		= 0x10000000,
	kFlagOnDemand	= 0x08000000,
	kFlagIntGen		= 0x04000000
};

// Platform function opcodes - UInt32
enum {
	kCommandCommandList				= 0,	// Command List
	kCommandCommandListLength		= 1,	// 1 longword of data in command
	kCommandWriteGPIO				= 1,	// Write to a GPIO
	kCommandWriteGPIOLength			= 2,	// 2 longwords of data in command
	kCommandReadGPIO				= 2,	// Read from a GPIO
	kCommandReadGPIOLength			= 3,	// 3 longwords of data in command
	kCommandWriteReg32				= 3,	// Write to a 32-bit register
	kCommandWriteReg32Length		= 3,	// 3 longwords of data in command
	kCommandReadReg32				= 4,	// Read from a 32-bit register
	kCommandReadReg32Length			= 1,	// 1 longword of data in command
	kCommandWriteReg16				= 5,	// Write to a 16-bit register
	kCommandWriteReg16Length		= 3,	// 3 longwords of data in command
	kCommandReadReg16				= 6,	// Read from a 16-bit register
	kCommandReadReg16Length			= 1,	// 1 longword of data in command
	kCommandWriteReg8				= 7,	// Write to an 8-bit register
	kCommandWriteReg8Length			= 3,	// 3 longwords of data in command
	kCommandReadReg8				= 8,	// Read from an 8-bit register
	kCommandReadReg8Length			= 1,	// 1 longword of data in command
	kCommandDelay					= 9,	// Delay between commands
	kCommandDelayLength				= 1,	// 1 longword of data in command
	kCommandWaitReg32				= 10,	// Wait for hw state change in 32-bit register
	kCommandWaitReg32Length			= 3,	// 3 longwords of data in command
	kCommandWaitReg16				= 11,	// Wait for hw state change in 16-bit register
	kCommandWaitReg16Length			= 3,	// 3 longwords of data in command
	kCommandWaitReg8				= 12,	// Wait for hw state change in 8-bit register
	kCommandWaitReg8Length			= 3,	// 3 longwords of data in command
	kCommandReadI2C					= 13,	// Read from I2C bus
	kCommandReadI2CLength			= 1,	// 1 longword of data in command
	kCommandWriteI2C				= 14,	// Write to I2C bus
	kCommandWriteI2CLength			= 1,	// Variable - 1 longword of data + length of array in command
	kCommandRMWI2C					= 15,	// Read-Modify-Write to I2C bus
	kCommandRMWI2CLength			= 3,	// Variable - 3 longwords of data + length of mask array + length of value array
	kCommandGeneralI2C				= 16,	// General I2C
	kCommandGeneralI2CLength		= 0,	// Unspecified
	kCommandShiftBytesRight			= 17,	// Shift byte stream right
	kCommandShiftBytesRightLength	= 2,	// 2 longwords of data in command
	kCommandShiftBytesLeft			= 18,	// Shift byte stream left
	kCommandShiftBytesLeftLength	= 2,	// 2 longwords of data in command
	kCommandConfigRead				= 19,	// Config cycle read
	kCommandConfigReadLength		= 2,	// 2 longwords of data in command
	kCommandConfigWrite				= 20,	// Config cycle write
	kCommandConfigWriteLength		= 2,	// 2 longwords of data in command
	kCommandConfigRMW				= 21,	// Config read-modify-write cycle
	kCommandConfigRMWLength			= 4		// Variable - 4 longwords of data + length of mask array + length of value array
};

#endif
