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
 /*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#include "IOPlatformFunction.h"

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
	
	if (result != kIOPFNoError) {
		iterator->release();
		return false;
	}
	
	/*
	 * If more data was found, create a new OSData object with the remaining data.
	 * This new object is returned to the caller, who is responsible for using it to
	 * create a new IOPlatformFunction object to handle it
	 */
	if (moreData) {
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
		DLOG ("IOPF::initWithPlatformDoFunction(%lx) - creating platformFunctionSymbol '%s'\n", this, tmpFunctionName);
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

/********************************************************************************************************/
/* IOPlatformFunctionIterator::getNextCommand															*/
/********************************************************************************************************/
bool IOPlatformFunctionIterator::getNextCommand(UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result)
{
	if (commandDone) {
		*result = kIOPFNoError;
		return false;
	}
	
	// If this is our first real iteration (i.e., we've been reset), then skip pHandle and flags
	if (commandPtr == platformFunction->platformFunctionPtr) {
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
	commandDone = (commandPtr == NULL);
	DLOG ("IOPFI::getNextCommand(%lx) - cmd %ld, dataLenRemaining %ld, cmdLen %ld, commandPtr 0x%lx\n", 
		this, *cmd, dataLengthRemaining, *cmdLen, commandPtr);
	
	return true;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::scanSubCommand															*/
/********************************************************************************************************/
UInt32 *IOPlatformFunctionIterator::scanSubCommand (UInt32 *cmdPtr, UInt32 lenRemaining,
					bool quickScan, UInt32 *cmd, UInt32 *cmdLen,
					UInt32 *param1, UInt32 *param2, UInt32 *param3, UInt32 *param4, 
					UInt32 *param5, UInt32 *param6, UInt32 *param7, UInt32 *param8, 
					UInt32 *param9, UInt32 *param10, UInt32 *result)
{
	UInt32 cmdLongLen, tmpLen, maskLen, arrayLen, offset, paramsUsed;
	
	DLOG ("IOPFI::scanSubCommand(%lx) - entered\n", this);
	*result = kIOPFNoError;
	*cmd = *(cmdPtr++);			// command is first word
	cmdLongLen = 1;
	if (*cmd == kCommandCommandList) {
		/*
		 * A commandList is treated specially as it is never really returned to the client.
		 * All we do is note that we're dealing with a command list and keep track of how
		 * many subCommands are in the list
		 */
		isCommandList = true;
		totalCommandCount = *(cmdPtr++);
		DLOG ("IOPFI::scanSubCommand(%lx) - got commandList, totalCmdCount %ld\n", this, totalCommandCount);
		cmdLongLen += kCommandCommandListLength;
		*cmdLen = cmdLongLen * sizeof(UInt32);
		if (*cmdLen > lenRemaining)
			*result = kIOPFBadCmdLength;		// Too many parameters to return or not enough data
		else
			paramsUsed = 0;
	} else {
		DLOG ("IOPFI::scanSubCommand(%lx) - parsing command %ld\n", this, *cmd);
		/*
		 * Most commands are fixed length - a command plus some number of words of data
		 * These are easy to deal with - we just note the length and return one of the words
		 * in each parameter until there's no data left.
		 */
		switch (*cmd) {
			case kCommandWriteGPIO:
				cmdLongLen += kCommandWriteGPIOLength;
				break;
			case kCommandReadGPIO:
				cmdLongLen += kCommandReadGPIOLength;
				break;
			case kCommandWriteReg32:
				cmdLongLen += kCommandWriteReg32Length;
				break;
			case kCommandReadReg32:
				cmdLongLen += kCommandReadReg32Length;
				break;
			case kCommandWriteReg16:
				cmdLongLen += kCommandWriteReg16Length;
				break;
			case kCommandReadReg16:
				cmdLongLen += kCommandReadReg16Length;
				break;
			case kCommandWriteReg8:
				cmdLongLen += kCommandWriteReg8Length;
				break;
			case kCommandReadReg8:
				cmdLongLen += kCommandReadReg8Length;
				break;
			case kCommandDelay:
				cmdLongLen += kCommandDelayLength;
				break;
			case kCommandWaitReg32:
				cmdLongLen += kCommandWaitReg32Length;
				break;
			case kCommandWaitReg16:
				cmdLongLen += kCommandWaitReg16Length;
				break;
			case kCommandWaitReg8:
				cmdLongLen += kCommandWaitReg8Length;
				break;
			case kCommandReadI2C:
				cmdLongLen += kCommandReadI2CLength;
				break;
			case kCommandGeneralI2C:
				cmdLongLen += kCommandGeneralI2CLength;
				break;
			case kCommandShiftBytesRight:
				cmdLongLen += kCommandShiftBytesRightLength;
				break;
			case kCommandShiftBytesLeft:
				cmdLongLen += kCommandShiftBytesLeftLength;
				break;
			case kCommandReadConfig:
				cmdLongLen += kCommandReadConfigLength;
				break;
			case kCommandWriteConfig:
				cmdLongLen += kCommandWriteConfigLength;
				break;
			default:
				// May not really be unknown (yet), but other commands are variable length
				// so we deal with them later
				*result = kIOPFUnknownCmd;
				break;
		}
		
		if (*result != kIOPFUnknownCmd) {
			/*
			 * If we're here, then we're dealing with a known, fixed length command
			 * so we return the appropriate amount of data
			 */
			tmpLen = cmdLongLen;
			if (((tmpLen - 1) > kIOPFMaxParams) || ((tmpLen * sizeof(UInt32)) > lenRemaining))
				*result = kIOPFBadCmdLength;		// Too many parameters to return or not enough data
			else {
				*cmdLen = cmdLongLen * sizeof(UInt32);		// return byte length
				tmpLen--;
				if (quickScan) {
					// Just need to calculate the length, not return any data
					DLOG ("IOPFI::scanSubCommand(%lx) -  quickScan skip %ld parameters\n", this, tmpLen);
					cmdPtr += tmpLen;
				} else {
					DLOG ("IOPFI::scanSubCommand(%lx) - copying %ld parameters, 1st param 0x%lx\n", this, tmpLen, *cmdPtr);
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
		} else {
			/*
			 * If we're here, then likely we are dealing with a variable length command.  These
			 * are typically a fixed portion followed by one or more variable length arrays of data.
			 * The information in the fixed portion tells how long the variable length portion is.
			 * Again we return the right number of parameters based on the command.  Note that for
			 * variable length data, it is not the data that is returned but the address of the array
			 * that comprises the data.  Clients must know to treat the data accordingly.
			 */
			*result = kIOPFNoError;
			DLOG ("IOPFI::scanSubCommand(%lx) - processing variable command %ld!\n", this, *cmd);
			switch (*cmd) {
				case kCommandWriteI2C:
					/* 
					 * 1) number of bytes
					 * 2) array of bytes
					 */
					*cmdLen = (cmdLongLen + kCommandWriteI2CLength) * sizeof (UInt32);	// byte length, so far
					arrayLen = *(cmdPtr++);
					DLOG ("IOPFI::scanSubCommand(%lx) - arrayLen %ld\n", this, arrayLen);
					*cmdLen += arrayLen;
					if (*cmdLen > lenRemaining)
						*result = kIOPFBadCmdLength;		// Too many parameters to return or not enough data
					else {
						if (!quickScan) {
							*param1 = arrayLen;				// Array len
							*param2 = (UInt32)cmdPtr;		// Address of array
							paramsUsed = 2;
						}
						cmdPtr = (UInt32 *)((UInt8 *)cmdPtr + arrayLen);
					}
					break;
				case kCommandRMWI2C:
					/* 
					 * 1) number of bytes to mask
					 * 2) number of bytes in value
					 * 3) number of bytes to transfer
					 * 4) array of bytes containing mask
					 * 5) array of bytes containing value
					 */
					*cmdLen = (cmdLongLen + kCommandRMWI2CLength) * sizeof (UInt32);	// byte length, so far
					maskLen = *(cmdPtr++);
					arrayLen = *(cmdPtr++);
					*cmdLen += (arrayLen + maskLen);
					if (*cmdLen > lenRemaining)
						*result = kIOPFBadCmdLength;		// Too many parameters to return or not enough data
					else {
						if (!quickScan) {
							*param1 = maskLen;			// Mask len
							*param2 = arrayLen;			// Array len
							*param3 = *(cmdPtr++);		// Transfer len
							*param4 = (UInt32)cmdPtr;	// Address of mask array
							// Adjust for length of mask array
							cmdPtr = (UInt32 *)((UInt8 *)cmdPtr + maskLen);
							*param5 = (UInt32)cmdPtr;	// Address of value array
							// Adjust for length of value array
							cmdPtr = (UInt32 *)((UInt8 *)cmdPtr + arrayLen);
							paramsUsed = 5;
						} else
							cmdPtr = (UInt32 *)((UInt8 *)cmdPtr + *cmdLen);
					}
					break;
				case kCommandRMWConfig:
					/* 
					 * 1) offset
					 * 2) number of bytes to mask
					 * 3) number of bytes in value
					 * 4) number of bytes to transfer
					 * 5) array of bytes containing mask
					 * 6) array of bytes containing value
					 */
					*cmdLen = (cmdLongLen + kCommandRMWConfigLength) * sizeof (UInt32);	// byte length, so far
					offset = *(cmdPtr++);
					maskLen = *(cmdPtr++);
					arrayLen = *(cmdPtr++);
					*cmdLen += (arrayLen + maskLen);
					if (*cmdLen > lenRemaining)
						*result = kIOPFBadCmdLength;		// Too many parameters to return or not enough data
					else {
						if (!quickScan) {
							*param1 = offset;			// Offset
							*param2 = maskLen;			// Mask len
							*param3 = arrayLen;			// Array len
							*param4 = *(cmdPtr++);		// Transfer len
							*param5 = (UInt32)cmdPtr;	// Address of mask array
							 // Adjust for length of mask array
							cmdPtr = (UInt32 *)((UInt8 *)cmdPtr + maskLen);
							*param6 = (UInt32)cmdPtr;	// Address of value array
							// Adjust for length of value array
							cmdPtr = (UInt32 *)((UInt8 *)cmdPtr + arrayLen);
							paramsUsed = 6;
						} else
							cmdPtr = (UInt32 *)((UInt8 *)cmdPtr + *cmdLen);
					}
					break;
				default:
					// OK, now it really is unknown
					IOLog ("IOPFI::scanSubCommand - unknown command %ld\n", *cmd);
					*result = kIOPFUnknownCmd;
					break;
			}

		}
		if (isCommandList)
			currentCommandCount++;
		
		if (!quickScan) {
			// Clean up unused parameters
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
	
	DLOG ("IOPFI::scanSubCommand(%lx) - done, *result %ld, *cmdLen %ld, lenRemaining %ld, cmdPtr 0x%lx\n",
		this, *result, *cmdLen, lenRemaining, cmdPtr);
	return cmdPtr;
}

/********************************************************************************************************/
/* IOPlatformFunctionIterator::scanCommand																*/
/********************************************************************************************************/
UInt32 *IOPlatformFunctionIterator::scanCommand (UInt32 *cmdPtr, UInt32 dataLen, UInt32 *cmdTotalLen,
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
		
		// If this is a command list and we've seen the required number of subcommands, we're done
		if (isCommandList && (currentCommandCount == totalCommandCount))
			break;
		
	} while (dataLen);

	// return updated cmdPtr (meaning there are more commands), or NULL if we're completely done
	return cmdPtr;
}

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

