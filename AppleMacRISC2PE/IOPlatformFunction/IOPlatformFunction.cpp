/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#include "IOPlatformFunction.h"

/*
 * Table descriptions:  scanSubCommand is driven by two tables.  The first, the commandArray is indexed
 * by command number and each entry is just the address of the associated commandDescriptorArray. The
 * command descriptor array is a variable length array laid out as follows (each pN is a SInt32):
 *
 * p0, p1, p2, p3..., 0
 *
 * p0 is a summary descriptor - for a fixed length command it is a positive number representing the
 * 		length of the command, in longwords.  This need be the only entry in the descriptor.  For a variable 
 * 		length command it is the negative of the fixed length portion of the variable command and will be
 *		followed by descriptions of the individual parameters.
 * p1 to pN - for a variable length command, p1 up to p10 describe each parameter as follows:
 *		a number in the fixed portion of the command is just treated as side of the corresponding parameter.
 *		A negative number in the variable portion of the command is treated as the negative index of
 *		the length parameter associated with the variable data
 *
 *	For example, the command kCommandRMWConfig is laid out as follows:
 * 		1) offset
 * 		2) number of bytes to mask
 * 		3) number of bytes in value
 * 		4) number of bytes to transfer
 * 		5) array of bytes containing mask
 * 		6) array of bytes containing value
 *
 * The corresponding command descriptor array would be:
 *
 *	p0		-kCommandRMWConfigLength			// Negative of length of fixed portion (longwords)
 *  p1		4									// offset (4 bytes)
 *  p2		4									// number of bytes of mask (4 bytes)
 *  p3		4									// number of bytes of value (4 bytes)
 *  p4		4									// number of bytes to transfer (4 bytes)
 *  p5		-2									// index of number of bytes of mask data (i.e., p2)
 *  p6		-3									// index of number of bytes of value data (i.e., p3)
 *  		0									// Zero array terminator
 */
#define kCommandDescTerminator 0

static SInt32 gCommandCommandListDesc[] 		= { kCommandCommandListLength, kCommandDescTerminator };
static SInt32 gCommandWriteGPIODesc[] 			= { kCommandWriteGPIOLength, kCommandDescTerminator };
static SInt32 gCommandReadGPIODesc[] 			= { kCommandReadGPIOLength, kCommandDescTerminator };
static SInt32 gCommandWriteReg32Desc[] 			= { kCommandWriteReg32Length, kCommandDescTerminator };
static SInt32 gCommandReadReg32Desc[] 			= { kCommandReadReg32Length, kCommandDescTerminator };
static SInt32 gCommandWriteReg16Desc[] 			= { kCommandWriteReg16Length, kCommandDescTerminator };
static SInt32 gCommandReadReg16Desc[] 			= { kCommandReadReg16Length, kCommandDescTerminator };
static SInt32 gCommandWriteReg8Desc[] 			= { kCommandWriteReg8Length, kCommandDescTerminator };
static SInt32 gCommandReadReg8Desc[] 			= { kCommandReadReg8Length, kCommandDescTerminator };
static SInt32 gCommandDelayDesc[] 				= { kCommandDelayLength, kCommandDescTerminator };
static SInt32 gCommandWaitReg32Desc[] 			= { kCommandWaitReg32Length, kCommandDescTerminator };
static SInt32 gCommandWaitReg16Desc[] 			= { kCommandWaitReg16Length, kCommandDescTerminator };
static SInt32 gCommandWaitReg8Desc[] 			= { kCommandWaitReg8Length, kCommandDescTerminator };
static SInt32 gCommandReadI2CDesc[] 			= { kCommandReadI2CLength, kCommandDescTerminator };
static SInt32 gCommandWriteI2CDesc[] 			= { -kCommandWriteI2CLength, 4, -1, kCommandDescTerminator };
static SInt32 gCommandRMWI2CDesc[] 				= { -kCommandRMWI2CLength, 4, 4, 4, -1, -2, kCommandDescTerminator };
static SInt32 gCommandGeneralI2CDesc[] 			= { kCommandGeneralI2CLength, kCommandDescTerminator };
static SInt32 gCommandShiftBytesRightDesc[] 	= { kCommandShiftBytesRightLength, kCommandDescTerminator };
static SInt32 gCommandShiftBytesLeftDesc[] 		= { kCommandShiftBytesLeftLength, kCommandDescTerminator };
static SInt32 gCommandReadConfigDesc[] 			= { kCommandReadConfigLength, kCommandDescTerminator };
static SInt32 gCommandWriteConfigDesc[] 		= { -kCommandWriteConfigLength, 4, 4, -2, kCommandDescTerminator };
static SInt32 gCommandRMWConfigDesc[] 			= { -kCommandRMWConfigLength, 4, 4, 4, 4, -2, -3, kCommandDescTerminator };
static SInt32 gCommandReadI2CSubAddrDesc[] 		= { kCommandReadI2CSubAddrLength, kCommandDescTerminator };
static SInt32 gCommandWriteI2CSubAddrDesc[] 	= { -kCommandWriteI2CSubAddrLength, 4, 4, -2, kCommandDescTerminator };
static SInt32 gCommandI2CModeDesc[] 			= { kCommandI2CModeLength, kCommandDescTerminator }; 
static SInt32 gCommandRMWI2CSubAddrDesc[] 		= { -kCommandRMWI2CSubAddrLength, 4, 4, 4, 4, -2, -3, kCommandDescTerminator }; 
static SInt32 gCommandReadReg32MaskShRtXORDesc[] = { kCommandReadReg32MaskShRtXORLength, kCommandDescTerminator };
static SInt32 gCommandReadReg16MaskShRtXORDesc[] = { kCommandReadReg16MaskShRtXORLength, kCommandDescTerminator };
static SInt32 gCommandReadReg8MaskShRtXORDesc[] = { kCommandReadReg8MaskShRtXORLength, kCommandDescTerminator };
static SInt32 gCommandWriteReg32ShLtMaskDesc[] 	= { kCommandWriteReg32ShLtMaskLength, kCommandDescTerminator };
static SInt32 gCommandWriteReg16ShLtMaskDesc[] 	= { kCommandWriteReg16ShLtMaskLength, kCommandDescTerminator };
static SInt32 gCommandWriteReg8ShLtMaskDesc[] 	= { kCommandWriteReg8ShLtMaskLength, kCommandDescTerminator };
static SInt32 gCommandMaskandCompareDesc[] 		= { -kCommandMaskandCompareLength, 4, -1, -1,  kCommandDescTerminator };

// Command array - one descriptor entry for each command, indexed by command
static SInt32 *gCommandArray[] = {
	gCommandCommandListDesc,			// kCommandCommandList [ 0 ]
	gCommandWriteGPIODesc,				// kCommandWriteGPIO [ 1 ]
	gCommandReadGPIODesc,				// kCommandReadGPIO [ 2 ]
	gCommandWriteReg32Desc,				// kCommandWriteReg32 [ 3 ]
	gCommandReadReg32Desc,				// kCommandReadReg32 [ 4 ]
	gCommandWriteReg16Desc,				// kCommandWriteReg16 [ 5 ]
	gCommandReadReg16Desc,				// kCommandReadReg16 [ 6 ]
	gCommandWriteReg8Desc,				// kCommandWriteReg8 [ 7 ]
	gCommandReadReg8Desc,				// kCommandReadReg8 [ 8 ]
	gCommandDelayDesc,					// kCommandDelay [ 9 ]
	gCommandWaitReg32Desc,				// kCommandWaitReg32 [ 10 ]
	gCommandWaitReg16Desc,				// kCommandWaitReg16 [ 11 ]
	gCommandWaitReg8Desc,				// kCommandWaitReg8	[ 12 ]
	gCommandReadI2CDesc,				// kCommandReadI2C [ 13 ]
	gCommandWriteI2CDesc,				// kCommandWriteI2C [ 14 ]
	gCommandRMWI2CDesc,					// kCommandRMWI2C [ 15 ]
	gCommandGeneralI2CDesc,				// kCommandGeneralI2C [ 16 ]
	gCommandShiftBytesRightDesc,		// kCommandShiftBytesRight [ 17 ]
	gCommandShiftBytesLeftDesc,			// kCommandShiftBytesLeft [ 18 ]
	gCommandReadConfigDesc,				// kCommandReadConfig [ 19 ]
	gCommandWriteConfigDesc,			// kCommandWriteConfig [ 20 ]
	gCommandRMWConfigDesc,				// kCommandRMWConfig [ 21 ]
	gCommandReadI2CSubAddrDesc,			// kCommandReadI2CSubAddr [ 22 ]
	gCommandWriteI2CSubAddrDesc,		// kCommandWriteI2CSubAddr [ 23 ]
	gCommandI2CModeDesc,				// kCommandI2CMode [ 24] 
	gCommandRMWI2CSubAddrDesc,			// kCommandRMWI2CSubAddr [ 25] 
	gCommandReadReg32MaskShRtXORDesc,	// kCommandReadReg32MaskShRtXOR	[ 26 ]
	gCommandReadReg16MaskShRtXORDesc,	// kCommandReadReg16MaskShRtXOR	[ 27 ]
	gCommandReadReg8MaskShRtXORDesc,	// kCommandReadReg8MaskShRtXOR [ 28 ]
	gCommandWriteReg32ShLtMaskDesc,		// kCommandWriteReg32ShLtMask [ 29 ]
	gCommandWriteReg16ShLtMaskDesc,		// kCommandWriteReg16ShLtMask [ 30 ]
	gCommandWriteReg8ShLtMaskDesc,		// kCommandWriteReg8ShLtMask [ 31 ]
	gCommandMaskandCompareDesc			// kCommandMaskandCompare [ 32 ]
};

#ifndef PFPARSE
/********************************************************************************************************/
/* class IOPlatformFunction																				*/
/********************************************************************************************************/
#define super OSObject

OSDefineMetaClassAndStructors(IOPlatformFunction, OSObject);

/********************************************************************************************************/
/* IOPlatformFunction::initWithPlatformDoFunction														*/
/********************************************************************************************************/
bool IOPlatformFunction::initWithPlatformDoFunction(OSSymbol *functionName, OSData *functionData,
                            OSData **moreFunctionData)
{
	char		tmpFunctionName[128], *tmpNamePtr, *funcNamePtr;
	UInt32		*moreData;
	UInt32		totalLen, result;

    if (!super::init() || !functionName || !functionData)
        return false;
	
	platformFunctionData = functionData;
	platformFunctionPtr = (UInt32 *)functionData->getBytesNoCopy();
	platformFunctionDataLen = functionData->getLength();
	
	// Create an associated iterator - we'll keep this around as long as we exist
	iterator = IOPlatformFunctionIterator::withIOPlatformFunction (this);
	if (!iterator)
		return false;

	/* 
	 * Make a preliminary scan of the command data.  If scanCommand returns "moreData" then we pass
	 * that back to the caller so the caller can make a new IOPlatformFunction object based on that data
	 */
	moreData = iterator->scanCommand (platformFunctionPtr, platformFunctionDataLen, &totalLen,
		&flags, &pHandle, &result);
	
	if (result != kIOPFNoError)
		return false;
	
	/*
	 * If more data was found, create a new OSData object with the remaining data.
	 * This new object is returned to the caller, who is responsible for using it to
	 * create a new IOPlatformFunction object to handle it
	 */
	if (moreData) {
		totalLen += (sizeof(UInt32) * 2);	// Account for pHandle & flags
		*moreFunctionData = OSData::withBytes (moreData, (platformFunctionDataLen - totalLen));
		
		platformFunctionDataLen = totalLen;				// Save adjusted length
	} else {
		// Nothing else to see here, move along...
		*moreFunctionData = NULL;
	}
		
	platformFunctionData->retain();
	
	/*
	 * If this is an on-demand or interrupt function, create the necessary function name
	 */
	if (flags & (kIOPFFlagOnDemand | kIOPFFlagIntGen)) {
		*tmpFunctionName = '\0';
		tmpNamePtr = tmpFunctionName;
		funcNamePtr = (char *)functionName->getCStringNoCopy();
		
		// Generate new function name from "platform-do-functionName", but without the "-do"
		while (*funcNamePtr != '-')		// Copy start of string
			*(tmpNamePtr++) = *(funcNamePtr++);
		funcNamePtr += 3;				// Skip -do
		while (*funcNamePtr)			// Copy rest of string
			*(tmpNamePtr++) = *(funcNamePtr++);
		
		sprintf (tmpNamePtr, "-%08lx", pHandle);		// Append pHandle
        platformFunctionSymbol = OSSymbol::withCString(tmpFunctionName);
		DLOG ("IOPF::initWithPlatformDoFunction(%lx) - creating platformFunctionSymbol '%s'\n", mypfobject, tmpFunctionName);
	} else
		// Don't bother for other cases as we don't need to publish a name
		platformFunctionSymbol = NULL;
	
    return true;
}

/********************************************************************************************************/
/* IOPlatformFunction::withPlatformDoFunction															*/
/********************************************************************************************************/
IOPlatformFunction *IOPlatformFunction::withPlatformDoFunction(OSSymbol *functionName, OSData *functionData,
                            OSData **moreFunctionData)
{
    IOPlatformFunction *me = new IOPlatformFunction;

    if (me && !me->initWithPlatformDoFunction(functionName, functionData, moreFunctionData)) {
        me->free();
        return NULL;
    }

    return me;
}

/********************************************************************************************************/
/* IOPlatformFunction::free																				*/
/********************************************************************************************************/
void IOPlatformFunction::free()
{
    if (iterator) {
        iterator->release();
        iterator = NULL;
    }
	
    if (platformFunctionSymbol) {
        platformFunctionSymbol->release();
        platformFunctionSymbol = NULL;
    }
	
    if (platformFunctionData) {
        platformFunctionData->release();
        platformFunctionData = NULL;
    }

    super::free();
}

/********************************************************************************************************/
/* IOPlatformFunction::validatePlatformFunction															*/
/********************************************************************************************************/
bool IOPlatformFunction::validatePlatformFunction(UInt32 flagsMask, UInt32 pHandleValue)
{
	// Only validate against pHandle if caller cares about pHandle
	if (pHandleValue && (pHandle != pHandleValue))
		return false;
	
	// Validate against flags
	return ((flags & flagsMask) != 0);
}

/********************************************************************************************************/
/* IOPlatformFunction::platformFunctionMatch															*/
/********************************************************************************************************/
bool IOPlatformFunction::platformFunctionMatch(const OSSymbol *funcSym, UInt32 flagsMask, UInt32 pHandleValue)
{
	/*
	 * platformFunctionMatch only makes sense for on-demand and interrupt generating functions
	 * Those are the only functions for which we generate a platformFunctionSymbol
	 * So if we don't have a symbol, just return false
	 */
	if (!(platformFunctionSymbol && funcSym))
		return false;
		
	return (platformFunctionSymbol->isEqualTo(funcSym) && validatePlatformFunction (flagsMask, pHandleValue));
}

/********************************************************************************************************/
/* IOPlatformFunction::getPlatformFunctionName															*/
/********************************************************************************************************/
const OSSymbol *IOPlatformFunction::getPlatformFunctionName() const
{
	return platformFunctionSymbol;
}

/********************************************************************************************************/
/* IOPlatformFunction::getCommandFlags																	*/
/********************************************************************************************************/
UInt32 IOPlatformFunction::getCommandFlags() const
{
	return flags;
}

/********************************************************************************************************/
/* IOPlatformFunction::getCommandPHandle																*/
/********************************************************************************************************/
UInt32 IOPlatformFunction::getCommandPHandle() const
{
	return pHandle;
}

/********************************************************************************************************/
/* IOPlatformFunction::getCommandIterator																*/
/********************************************************************************************************/
IOPlatformFunctionIterator *IOPlatformFunction::getCommandIterator()
{
	/*
	 * The iterator is created once in our start routine, what we do here is
	 * reset it to the beginning and retain it on behalf of the caller (who
	 * should release it when done)
	 */
	if (iterator) {
		iterator->retain();
		iterator->reset();
	}
	return iterator;
}

/********************************************************************************************************/
/* IOPlatformFunction::publishPlatformFunction															*/
/********************************************************************************************************/
void IOPlatformFunction::publishPlatformFunction(IOService *handler)
{
	if (platformFunctionSymbol)
		handler->publishResource(platformFunctionSymbol, handler);

	return;
}

    OSMetaClassDefineReservedUnused(IOPlatformFunction, 0);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 1);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 2);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 3);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 4);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 5);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 6);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 7);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 8);
    OSMetaClassDefineReservedUnused(IOPlatformFunction, 9);

/********************************************************************************************************/
/* class IOPlatformFunctionIterator																		*/
/********************************************************************************************************/
#undef super
#define super OSIterator

OSDefineMetaClassAndStructors(IOPlatformFunctionIterator, OSIterator);

/********************************************************************************************************/
/* IOPlatformFunctionIterator::withIOPlatformFunction													*/
/********************************************************************************************************/
IOPlatformFunctionIterator *IOPlatformFunctionIterator::withIOPlatformFunction(const IOPlatformFunction *inFunc)
{
    IOPlatformFunctionIterator *me = new IOPlatformFunctionIterator;

    if (me && !me->initWithIOPlatformFunction(inFunc)) {
        me->free();
        return 0;
    }

    return me;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::initWithIOPlatformFunction												*/
/********************************************************************************************************/
bool IOPlatformFunctionIterator::initWithIOPlatformFunction(const IOPlatformFunction *inFunc)
{
    if (!inFunc)
        return false;

    platformFunction = inFunc;
    commandPtr = NULL;
    totalCommandCount = 0;
    currentCommandCount = 0;
	isCommandList = false;
    valid = true;

    return true;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::free																		*/
/********************************************************************************************************/
void IOPlatformFunctionIterator::free()
{
	valid = false;
	
    super::free();
	return;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::reset																	*/
/********************************************************************************************************/
void IOPlatformFunctionIterator::reset()
{
	commandPtr = platformFunction->platformFunctionPtr;
	dataLengthRemaining = platformFunction->platformFunctionDataLen;
	commandDone = false;
	totalCommandCount = currentCommandCount = 0;
	return;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::isValid																	*/
/********************************************************************************************************/
bool IOPlatformFunctionIterator::isValid()
{
	return valid;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::getNextObject															*/
/********************************************************************************************************/
OSObject *IOPlatformFunctionIterator::getNextObject()
{
	// N/A for this class
	return NULL;
}
#endif // !PFPARSE

/********************************************************************************************************/
/* IOPlatformFunctionIterator::getNextCommand															*/
/********************************************************************************************************/
bool 
#ifndef PFPARSE
IOPlatformFunctionIterator::
#endif
	getNextCommand(UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result)
{
	if (commandDone) {
		*result = kIOPFNoError;
		return false;
	}
	
	// If this is our first real iteration (i.e., we've been reset), then skip pHandle and flags
#ifdef PFPARSE
	if (commandPtr == platformFunctionPtr) {
#else
	if (commandPtr == platformFunction->platformFunctionPtr) {
#endif
		commandPtr += 2;		// Skip pHandle and flags and adjust length remaining
		dataLengthRemaining -= 2 * sizeof(UInt32);

		// If this is also a command list then skip over the commandList command and return the first real command
		if (isCommandList) {
			// Skip past command list command
			commandPtr = scanSubCommand (commandPtr, dataLengthRemaining, true, cmd, cmdLen, NULL, NULL, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL, NULL, result); 
			dataLengthRemaining -= *cmdLen;
		}
	}
	
	/*
	 * Return the next command in the command string as determined by the current commandPtr.  Also
	 * return the associated data and update commandPtr accordingly.
	 */
	commandPtr = scanSubCommand (commandPtr, dataLengthRemaining, false, cmd, cmdLen, param1, param2, param3, param4, param5,
		param6, param7, param8, param9, param10, result);
	
	if (*result != kIOPFNoError)
		return false;
	
	dataLengthRemaining -= *cmdLen;
	if (commandPtr == NULL) {
		commandDone = true;
		DLOG ("IOPFI::getNextCommand - cmd %ld, dataLenRemaining %ld, cmdLen %ld, commandPtr NULL\n", 
			*cmd, dataLengthRemaining, *cmdLen);
	} else {
		commandDone = false;
		DLOG ("IOPFI::getNextCommand - cmd %ld, dataLenRemaining %ld, cmdLen %ld, *commandPtr 0x%x\n", 
			*cmd, dataLengthRemaining, *cmdLen, *commandPtr);
	}
	
	return true;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::scanSubCommand															*/
/********************************************************************************************************/
UInt32 *
#ifndef PFPARSE
IOPlatformFunctionIterator::
#endif
	scanSubCommand (UInt32 *cmdPtr, UInt32 lenRemaining,
					bool quickScan, UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result)
{
	SInt32		fixedLen, *cmdDescPtr, paramDesc, descIndex;
	UInt32		*cmdBase, cmdLongLen, paramsUsed, tmpLen, *paramPtr;
	
	*result = kIOPFNoError;
	cmdBase = cmdPtr;
	*cmd = *(cmdPtr++);			// command is first word
	if (*cmd > kCommandMaxCommand) {
		*result = kIOPFUnknownCmd;
		return cmdPtr;
	}

	cmdLongLen = 1;				// Include command itself in total length
	if (*cmd == kCommandCommandList) {
		/*
		 * A commandList is treated specially as it is never really returned to the client.
		 * All we do is note that we're dealing with a command list and keep track of how
		 * many subCommands are in the list
		 */
		isCommandList = true;
		totalCommandCount = *(cmdPtr++);
		DLOG ("IOPFI::scanSubCommand(0x%lx) - got commandList, totalCmdCount %ld\n", mypfobject, totalCommandCount);
		cmdLongLen += kCommandCommandListLength;
		*cmdLen = cmdLongLen * sizeof(UInt32);
		if (*cmdLen > lenRemaining)
			*result = kIOPFBadCmdLength;		// Too many parameters to return or not enough data
		else
			paramsUsed = 0;
	} else {
		cmdDescPtr = gCommandArray[*cmd];			// Look up command descriptor array
		fixedLen = cmdDescPtr[0];
		if (fixedLen >= 0)						// Fixed length command
			cmdLongLen += fixedLen;
		else
			cmdLongLen += (-fixedLen);			// Fixed portion of variable length command
		/*
			* If we're here, then we're dealing with a known, fixed length command
			* so we return the appropriate amount of data
			*/
		tmpLen = cmdLongLen;
		//DLOG ("fixed command: tmpLen %ld, cmdLongLen %ld, lenRemaining %ld\n", tmpLen, cmdLongLen, lenRemaining);
		if (((tmpLen - 1) > kIOPFMaxParams) || ((tmpLen * sizeof(UInt32)) > lenRemaining))
			*result = kIOPFBadCmdLength;		// Too many parameters to return or not enough data
		else {
			*cmdLen = cmdLongLen * sizeof(UInt32);		// return byte length
			tmpLen--;
			if (quickScan) {
				// Just need to calculate the length, not return any data
				cmdPtr += tmpLen;
			} else {
				DLOG ("IOPFI::scanSubCommand(%lx) - copying %ld parameters, 1st param 0x%lx\n", mypfobject, tmpLen, *cmdPtr);
				paramsUsed = tmpLen;
				while (1) {
					// Copy the right number of parameters into place
					*param1 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param2 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param3 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param4 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param5 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param6 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param7 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param8 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param9 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					*param10 = *(cmdPtr++);
					if (!(--tmpLen)) break;
					
					// Should not get here
					DLOG ("IOPFI::scanSubCommand - passed param10, OOPS!\n");
					break;
				}
			}
		}
		if (fixedLen < 0) {				// Deal with variable portion of command - the current assumption is all other data is variable
			*cmdLen = cmdLongLen * sizeof (UInt32);	// byte length, so far
			descIndex = cmdLongLen;		// Index of start of variable data in command descriptor array
			while ((paramDesc = cmdDescPtr[descIndex]) != kCommandDescTerminator) {
				// paramDesc is the index into the fixed portion of the command sequence for the word
				// that contains the length of the current parameter.  It is negative to signify a
				// variable length parameter.  However, we do not currently accommodate a fixed length
				// parameter within the variable length portion.
				if (paramDesc < 0)
					paramDesc = -paramDesc;
					
				*cmdLen += cmdBase[paramDesc];		// Add length for this parameter by looking up previous data
				if (!quickScan) {
					switch (descIndex) {
						case 1:
							paramPtr = param1;
							break;
						case 2:
							paramPtr = param2;
							break;
						case 3:
							paramPtr = param3;
							break;
						case 4:
							paramPtr = param4;
							break;
						case 5:
							paramPtr = param5;
							break;
						case 6:
							paramPtr = param6;
							break;
						case 7:
							paramPtr = param7;
							break;
						case 8:
							paramPtr = param8;
							break;
						case 9:
							paramPtr = param9;
							break;
						case 10:
							paramPtr = param10;
							break;
						default:
							break;
					}
					*paramPtr = (UInt32)cmdPtr;
					paramsUsed++;
				}
				cmdPtr = (UInt32 *)((UInt8 *)cmdPtr + cmdBase[paramDesc]);
				
				descIndex++;
			}
		}
		if (isCommandList)
			currentCommandCount++;
		
		if (!quickScan) {
			/*
			 * Clean up unused parameters - this is primarily a diagnostic aid
			 * Leaving garbage in parameters can be suggestive that they represent
			 * real information.  Making them zero eliminates that.
			 */
			switch (paramsUsed) {
				case 0:
					if (param1) *param1 = 0;
				case 1:
					if (param2) *param2 = 0;
				case 2:
					if (param3) *param3 = 0;
				case 3:
					if (param4) *param4 = 0;
				case 4:
					if (param5) *param5 = 0;
				case 5:
					if (param6) *param6 = 0;
				case 6:
					if (param7) *param7 = 0;
				case 7:
					if (param8) *param8 = 0;
				case 8:
					if (param9) *param9 = 0;
				case 9:
					if (param10) *param10 = 0;
				default:
					break;
			}
		}
	}
	
	// If there was an error or there is no data remaining, return NULL, otherwise return remaining data
	if ((*result != kIOPFNoError) || (*cmdLen == lenRemaining))
		cmdPtr = NULL;
	
	if (cmdPtr)
		DLOG ("IOPFI::scanSubCommand - done, *result %ld, *cmdLen %ld, lenRemaining %ld, *cmdPtr\n",
			*result, *cmdLen, lenRemaining, cmdPtr);
	else
		DLOG ("IOPFI::scanSubCommand - done, *result %ld, *cmdLen %ld, lenRemaining %ld, cmdPtr NULL\n",
			*result, *cmdLen, lenRemaining);
	return cmdPtr;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::scanCommand																*/
/********************************************************************************************************/
UInt32 *
#ifndef PFPARSE
IOPlatformFunctionIterator::
#endif
	scanCommand (UInt32 *cmdPtr, UInt32 dataLen, UInt32 *cmdTotalLen,
					UInt32 *flags, UInt32 *pHandle, UInt32 *result)
{
	UInt32	cmdLen, cmd;
	
	// Minimum of three longwords required (pHandle, flags, command)
	if (dataLen < 3) {
		*result = kIOPFBadCmdLength;
		return NULL;
	}
	
	// Remember pHandle and flags
	*pHandle = *(cmdPtr++);
	*flags = *(cmdPtr++);
	dataLen -= 2 * sizeof(UInt32);		// Adjust for pHandle and flags
	
	DLOG ("IOPFI::scanCommand: pHandle 0x%lx, flags 0x%lx, len left %ld\n", *pHandle, *flags, dataLen);
	
	*cmdTotalLen = 0;
	
	// Sanity check first command
	if ((*cmdPtr < 0) || (*cmdPtr > kCommandMaxCommand)) {
		*result = kIOPFBadCmdLength;
		return NULL;
	}

	/*
	 * Make a pass through the entire command, ignoring data in the command for now.
	 * The purpose here is to validate the command and also to see if there are additional
	 * commands beyond the current set.  This allows us to break a separate command out into
	 * its own IOPlatformFunction object.
	 */
	do {
		cmdPtr = scanSubCommand (cmdPtr, dataLen, true, &cmd, &cmdLen, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL, result);
		
		*cmdTotalLen += cmdLen;

		if ((*result != kIOPFNoError) || !cmdPtr)
			break;
		
		dataLen -= cmdLen;
		if (dataLen < 0) {
			*result = kIOPFBadCmdLength;
			return NULL;
		}
		
		// If this is single command or it's a command list and we've seen the required number of subcommands, we're done
		//if (isCommandList && (currentCommandCount == totalCommandCount))
		if (!isCommandList || (currentCommandCount == totalCommandCount))
			break;
		
	} while (dataLen);

	if (cmdPtr)
		DLOG ("IOPFI::scanCommand: done, *cmdPtr 0x%lx\n", *cmdPtr);
	else
		DLOG ("IOPFI::scanCommand: done, cmdPtr NULL\n");

	// return updated cmdPtr (meaning there are more commands), or NULL if we're completely done
	return cmdPtr;
}

#ifndef PFPARSE
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 0);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 1);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 2);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 3);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 4);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 5);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 6);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 7);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 8);
    OSMetaClassDefineReservedUnused(IOPlatformFunctionIterator, 9);
#endif
