/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _PLATFORMFUNCTIONS_H
#define _PLATFORMFUNCTIONS_H

#ifndef PFPARSE
#	include <IOKit/IOLib.h>
#	include <IOKit/IOService.h>
#	include <libkern/c++/OSObject.h>
#	include <libkern/c++/OSIterator.h>
#else
#	include <stdio.h>
#	include <libkern/OSTypes.h>
#endif

#ifdef DLOG
#	undef DLOG
#endif

#ifdef PFPARSE
// Always defined for command line tool
#	define IOPFDEBUG 1
#else
// For IOPlatformFunction kext debugging, uncomment to enable debug output
//#define IOPFDEBUG 1
#endif

#ifdef IOPFDEBUG
#	ifdef PFPARSE
__BEGIN_DECLS
		void dprintf(const char *fmt, ...);
__END_DECLS
#		define DLOG  dprintf
#	else
#		define DLOG(fmt, args...)  kprintf(fmt, ## args)
#	endif
#else
#	define DLOG(fmt, args...)
#endif

// To enumerate a nubs platform functions do:
// 	result = provider->getPlatform()->callPlatformFunction (kInstantiatePlatformFunctions, true, (void *)provider, 
//		(void *)&platformFuncArray, (void *)0, (void *)0);
//
// NOTE - true as the second parameter means the callPlatformFunction can block
#define kInstantiatePlatformFunctions "InstantiatePlatformFunctions"

// Device tree property keys, platform function prefixes
#define kFunctionProvidedPrefix		"platform-do-"
#define kFunctionRequiredPrefix		"platform-"

#define kIOPFMaxParams 10

// Platform function flags - UInt32
enum {
	kIOPFFlagOnInit		= 0x80000000,
	kIOPFFlagOnTerm		= 0x40000000,
	kIOPFFlagOnSleep	= 0x20000000,
	kIOPFFlagOnWake		= 0x10000000,
	kIOPFFlagOnDemand	= 0x08000000,
	kIOPFFlagIntGen		= 0x04000000,
	kIOPFFlagHighSpeed	= 0x02000000,
	kIOPFFlagLowSpeed	= 0x01000000
};

// Platform function selectors - use as OSSymbols
#define	kIOPFInterruptRegister		"IOPFInterruptRegister"
#define	kIOPFInterruptUnRegister	"IOPFInterruptUnRegister"
#define	kIOPFInterruptEnable		"IOPFInterruptEnable"
#define	kIOPFInterruptDisable		"IOPFInterruptDisable"

// Platform function opcodes - UInt32
enum {
	kCommandCommandList				= 0,	// [0x0] Command List
	kCommandCommandListLength		= 1,	// 1 longword of data in command
	kCommandWriteGPIO				= 1,	// [0x1] Write to a GPIO
	kCommandWriteGPIOLength			= 2,	// 2 longwords of data in command
	kCommandReadGPIO				= 2,	// [0x2] Read from a GPIO
	kCommandReadGPIOLength			= 3,	// 3 longwords of data in command
	kCommandWriteReg32				= 3,	// [0x3] Write to a 32-bit register
	kCommandWriteReg32Length		= 3,	// 3 longwords of data in command
	kCommandReadReg32				= 4,	// [0x4] Read from a 32-bit register
	kCommandReadReg32Length			= 1,	// 1 longword of data in command
	kCommandWriteReg16				= 5,	// [0x5] Write to a 16-bit register
	kCommandWriteReg16Length		= 3,	// 3 longwords of data in command
	kCommandReadReg16				= 6,	// [0x6] Read from a 16-bit register
	kCommandReadReg16Length			= 1,	// 1 longword of data in command
	kCommandWriteReg8				= 7,	// [0x7] Write to an 8-bit register
	kCommandWriteReg8Length			= 3,	// 3 longwords of data in command
	kCommandReadReg8				= 8,	// [0x8] Read from an 8-bit register
	kCommandReadReg8Length			= 1,	// 1 longword of data in command
	kCommandDelay					= 9,	// [0x9] Delay between commands
	kCommandDelayLength				= 1,	// 1 longword of data in command
	kCommandWaitReg32				= 10,	// [0xA] Wait for hw state change in 32-bit register
	kCommandWaitReg32Length			= 3,	// 3 longwords of data in command
	kCommandWaitReg16				= 11,	// [0xB] Wait for hw state change in 16-bit register
	kCommandWaitReg16Length			= 3,	// 3 longwords of data in command
	kCommandWaitReg8				= 12,	// [0xC] Wait for hw state change in 8-bit register
	kCommandWaitReg8Length			= 3,	// 3 longwords of data in command
	kCommandReadI2C					= 13,	// [0xD] Read from I2C bus
	kCommandReadI2CLength			= 1,	// 1 longword of data in command
	kCommandWriteI2C				= 14,	// [0xE] Write to I2C bus
	kCommandWriteI2CLength			= 1,	// Variable - 1 longword of data + length of array in command
	kCommandRMWI2C					= 15,	// [0xF] Read-Modify-Write to I2C bus
	kCommandRMWI2CLength			= 3,	// Variable - 3 longwords of data + length of mask array + length of value array
	kCommandGeneralI2C				= 16,	// [0x10] General I2C
	kCommandGeneralI2CLength		= 0,	// Unspecified
	kCommandShiftBytesRight			= 17,	// [0x11] Shift byte stream right
	kCommandShiftBytesRightLength	= 2,	// 2 longwords of data in command
	kCommandShiftBytesLeft			= 18,	// [0x12] Shift byte stream left
	kCommandShiftBytesLeftLength	= 2,	// 2 longwords of data in command
	kCommandReadConfig				= 19,	// [0x13] Config cycle read
	kCommandReadConfigLength		= 2,	// 2 longwords of data in command
	kCommandWriteConfig				= 20,	// [0x14] Config cycle write
	kCommandWriteConfigLength		= 2,	// 2 longwords of data in command
	kCommandRMWConfig				= 21,	// [0x15] Config read-modify-write cycle
	kCommandRMWConfigLength			= 4,	// Variable - 4 longwords of data + length of mask array + length of value array
	kCommandReadI2CSubAddr			= 22,	// [0x16] Read from SubAddress on I2C bus
	kCommandReadI2CSubAddrLength	= 2,	// 2 longwords of data in command
	kCommandWriteI2CSubAddr			= 23,	// [0x17] Write to SubAddress on I2C bus
	kCommandWriteI2CSubAddrLength	= 2,	// Variable - 2 longwords of data + length of array in command
	kCommandI2CMode					= 24,	// [0x18] Set I2C bus transfer mode
	kCommandI2CModeLength			= 1,	// 1 longword of data in command
	kCommandRMWI2CSubAddr			= 25,	// [0x19] Modify-Write to SubAddress
	kCommandRMWI2CSubAddrLength		= 4,	// 4 longwords of data + length of mask array + length of value array
	kCommandReadReg32MaskShRtXOR	= 26,	// [0x1A] Read 32 bit register, them mask, shift right and XOR
	kCommandReadReg32MaskShRtXORLength	= 4,	// 4 longwords of data
	kCommandReadReg16MaskShRtXOR	= 27,	// [0x1B] Read 16 bit register, them mask, shift right and XOR
	kCommandReadReg16MaskShRtXORLength	= 4,	// 4 longwords of data
	kCommandReadReg8MaskShRtXOR		= 28,	// [0x1C] Read 8 bit register, them mask, shift right and XOR
	kCommandReadReg8MaskShRtXORLength	= 4,	// 4 longwords of data
	kCommandWriteReg32ShLtMask		= 29,	// [0x1D] Write 32 bit register, with shift left and mask
	kCommandWriteReg32ShLtMaskLength	= 3,	// 3 longwords of data
	kCommandWriteReg16ShLtMask		= 30,	// [0x1E] Write 16 bit register, with shift left and mask
	kCommandWriteReg16ShLtMaskLength	= 3,	// 3 longwords of data
	kCommandWriteReg8ShLtMask		= 31,	// [0x1F] Write 8 bit register, with shift left and mask
	kCommandWriteReg8ShLtMaskLength		= 3,	// 4 longwords of data
	kCommandMaxCommand				= kCommandWriteReg8ShLtMask
};

enum {
	kIOPFNoError					= 0,
	kIOPFUnknownCmd					= 1,
	kIOPFBadCmdLength				= 2
};

#ifndef PFPARSE
#define mypfobject this
/*!
    @class IOPlatformFunction
    @abstract A class abstracting platform-do-function properties.  Note that this differs somewhat from a platform-do-function, which can contain multiple commands.  An IOPlatformFunction object deals with a single command, defined as (pHandle, flags, command[List])
*/
class IOPlatformFunction : public OSObject
{
	friend class IOPlatformFunctionIterator;

    OSDeclareDefaultStructors(IOPlatformFunction)	

protected:
	OSData							*platformFunctionData;
	UInt32							*platformFunctionPtr;
	UInt32							platformFunctionDataLen;			// byte length
	IOPlatformFunctionIterator		*iterator;
	UInt32 							flags;
	UInt32							pHandle;
	const OSSymbol					*platformFunctionSymbol;			// Valid only for on-demand & int functions

public:
    /*!
        @function withPlatformDoFunction
        @abstract A static constructor function to create and initialize an instance of IOPlatformFunction using data from a platform-do-function property
        @param functionName The name of the function - same as the property name
        @param functionData The data from the property
		@param moreFunctionData If data contains more than one complete command, then moreFunctionData returns a new object with the additional data
        @result Returns an instance of IOPlatformFunction or 0 if an error occurred.
    */
    static IOPlatformFunction *withPlatformDoFunction(OSSymbol *functionName, OSData *functionData,
                            OSData **moreFunctionData);

    /*!
        @function initWithPlatformDoFunction
        @abstract Member function to initialize an instance of IOPlatformFunction using data from a platform-do-function property
        @param functionName The name of the function - same as the property name
        @param functionData The data from the property
		@param moreFunctionData If data contains more than one complete command, then moreFunctionData returns a new object with the additional data
        @result Returns true on success, false otherwise.
    */
    bool initWithPlatformDoFunction(OSSymbol *functionName, OSData *functionData,
                            OSData **moreFunctionData);
    /*!
        @function free
        @abstract Releases and deallocates resources created by the OSNumber instances.
        @discussion This function should not be called directly, use release() instead.
    */
    virtual void free();

    /*!
        @function validatePlatformFunction
        @abstract Validates that the function handles a particular situation (flags and pHandle)
		@param flagsMask Function flags are validated against flagsMask
		@param pHandleValue If not NULL, function pHandle is validated against pHandleValue
        @result Returns true if command is valid for particular flagsMask and pHandle, false otherwise.
    */
    virtual bool validatePlatformFunction(UInt32 flagsMask, UInt32 pHandleValue);

    /*!
        @function platformFunctionMatch
        @abstract Called by the driver to determine if this object handles a particular callPlatformFunction call
		@param funcName Function name as invoked in the callPlatformFunction call
		@param flagsMask Function flags are validated against flagsMask
		@param pHandleValue If not NULL, function pHandle is validated against pHandleValue
        @result Returns true if command matches funcName, flagsMask and pHandle, false otherwise.
    */
    virtual bool platformFunctionMatch(const OSSymbol *funcSym, UInt32 flagsMask, UInt32 pHandleValue);

    /*!
        @function getPlatformFunctionName
        @abstract A member function which returns the function name associated with the command
        @result Returns an OSString representing the name (valid only for on-demand and interrupt functions)
    */
    virtual const OSSymbol *getPlatformFunctionName() const;
    /*!
        @function getCommandFlags
        @abstract A member function which returns the flags associated with the command.
        @result Returns flags value.
    */
    virtual UInt32 getCommandFlags() const;

    /*!
        @function getCommandPHandle
        @abstract A member function which returns the pHandle associated with the command.
        @result Returns the pHandle value
    */
    virtual UInt32 getCommandPHandle() const;
    /*!
        @function getCommandIterator
        @abstract A member function which returns a IOPlatformFunctionIterator object associated with the command data.  The iterator object is retained on behalf of the caller, who should release it when done.
        @result Returns the internal value as an 16-bit value.
    */
    virtual IOPlatformFunctionIterator *getCommandIterator();
    /*!
        @function publishPlatformFunction
        @abstract A member function which publishes the platform function in the IORegistry
		@param handler The IOService object that will handle the related callPlatformFunction call (typically the driver that created this object)
        @result Returns true on success, false otherwise
    */
    virtual void publishPlatformFunction(IOService *handler);

    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 0);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 1);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 2);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 3);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 4);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 5);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 6);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 7);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 8);
    OSMetaClassDeclareReservedUnused(IOPlatformFunction, 9);
};

/*!
    @class IOPlatformFunctionIterator
    @discussion
    IOPlatformFunctionIterator objects provide a consistent mechanism to iterate through IOPlatformFunction objects.
*/
class IOPlatformFunctionIterator : public OSIterator
{
    OSDeclareDefaultStructors(IOPlatformFunctionIterator)

private:
    const IOPlatformFunction	*platformFunction;
	UInt32						*commandPtr;
	UInt32						dataLengthRemaining;
	bool						isCommandList;
	bool						commandDone;
	UInt32						totalCommandCount;
	UInt32						currentCommandCount;
    bool						valid;

public:
    /*!
        @function withPlatformFunction
        @abstract A static constructor function which creates and initializes an instance of IOPlatformFunctionIterator for the provided IOPlatformFunction object.
        @param inFunc The IOPlatformFunction derived collection object to be iteratated.
        @result Returns a new instance of OSCollection or 0 on failure.
    */
    static IOPlatformFunctionIterator *withIOPlatformFunction(const IOPlatformFunction *inFunc);

    /*!
        @function initWithIOPlatformFunction
        @abstract A member function to initialize the intance of IOPlatformFunctionIterator with the provided IOPlatformFunction object.
        @param inFunc The IOPlatformFunction derived collection object to be iteratated.
        @result Returns true if the initialization was successful or false on failure.
    */
    virtual bool initWithIOPlatformFunction(const IOPlatformFunction *inFunc);

    /*!
        @function free
        @abstract A member function to release and deallocate all resources created or used by the IOPlatformFunctionIterator object.
        @discussion This function should not be called directly, use release() instead.
    */
    virtual void free();

    /*!
        @function reset
        @abstract A member function which resets the iterator to begin the next iteration from the beginning of the collection.
    */
    virtual void reset();

    /*!
        @function isValid
        @abstract A member function for determining if the collection was modified during iteration.
    */
    virtual bool isValid();

    /*!
        @function getNextObject
        @abstract A member function to get the next object in the collection being iterated (does nothing in this context).
        @result Returns the next object in the collection or 0 when the end has been reached (always returns NULL).
    */
   virtual OSObject *getNextObject();

    /*!
        @function getNextCommand
        @abstract A member function to get the next command in the IOPlatformFunction being iterated.
        @result Returns true and fills in its parameters the next command in the object or false when the end has been reached.
		@discussion This is the primary iterate function and the only one which should be called by other objects
    */
    virtual bool getNextCommand(UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result);

    /*!
        @function scanSubCommand
        @abstract A member function to scan a single command beginning at cmdPtr
        @param cmdPtr A longword pointer to the start of the command
        @param lenRemaining A count of the longwords remaining in the entire command set
		@param quickScan If true, only cmd, cmdLen and an updated cmdPtr are returned.  ParamN are ignored
		@param cmd The returned value of the command
		@param cmdLen The returned length of the command, in longwords
		@param paramN Returned values associated with the various parts of the command.  Not filled in unless quickScan is false.
		@param result kIOPFNoError on success, otherwise an appropriate error code
        @result Returns updated pointer to next command or NULL if there are no more commands or an error occurred.
		@discussion This function should not be called directly.  Use getNextCommand to perform the iteration.
    */
	virtual UInt32 *scanSubCommand (UInt32 *cmdPtr,  UInt32 lenRemaining,
					bool quickScan, UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result);
    
    /*!
        @function scanCommand
        @abstract A member function to scan an entire command set beginning at cmdPtr
        @param cmdPtr A longword pointer to the start of the command
		@param dataLen Amount of data (in longwords) in the command
        @param cmdTotalLen Returned count of the longwords making up the entire command set
		@param flags Returned flags associated with the commmand
		@param pHandle Returned pHandle associated with the commmand
		@param result kIOPFNoError on success, otherwise an appropriate error code
        @result Returns updated pointer to next command set or NULL if there are no more command sets or an error occurred.
		@discussion This function should not be called directly.  Use getNextCommand to perform the iteration.
    */
	virtual UInt32 *scanCommand (UInt32 *cmdPtr, UInt32 dataLen, UInt32 *cmdTotalLen,
					UInt32 *flags, UInt32 *pHandle, UInt32 *result);
    
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 0);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 1);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 2);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 3);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 4);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 5);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 6);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 7);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 8);
    OSMetaClassDeclareReservedUnused(IOPlatformFunctionIterator, 9);
};
#else
#	ifndef NULL
#		define NULL 0L
#	endif
#define mypfobject NULL
	extern UInt32				*commandPtr, *platformFunctionPtr;
	extern UInt32				dataLengthRemaining;
	extern bool					isCommandList;
	extern bool					commandDone;
	extern UInt32				totalCommandCount;
	extern UInt32				currentCommandCount;

__BEGIN_DECLS
bool getNextCommand(UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result);
					
UInt32 *scanSubCommand (UInt32 *cmdPtr, UInt32 lenRemaining,
					bool quickScan, UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result);
					
UInt32 *scanCommand (UInt32 *cmdPtr, UInt32 dataLen, UInt32 *cmdTotalLen,
					UInt32 *flags, UInt32 *pHandle, UInt32 *result);
__END_DECLS
#endif // PFPARSE

#endif
