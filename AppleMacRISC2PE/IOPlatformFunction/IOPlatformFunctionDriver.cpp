/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include "IOPlatformFunctionDriver.h"

#define super IOService
OSDefineMetaClassAndStructors(IOPlatformFunctionDriver, IOService)

bool IOPlatformFunctionDriver::start(IOService *nub) {
	instantiateFunctionSymbol = OSSymbol::withCString(kInstantiatePlatformFunctions);
	
	// Make ourselves known to the world
	publishResource(instantiateFunctionSymbol, this);
	
	kprintf ("IOPlatformFunctionDriver::start\n");
	return super::start (nub);
}

//*********************************************************************************
// instantiatePlatformFunctions
//*********************************************************************************
IOReturn IOPlatformFunctionDriver::callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
					void *param1, void *param2,
					void *param3, void *param4) {
	
    if (functionName == instantiateFunctionSymbol) 
		return instantiatePlatformFunctions ((IOService *) param1, (OSArray **)param2);
	
	// We should only be called as a published resource so anything else is unsupported
	return kIOReturnUnsupported;
}

//*********************************************************************************
// instantiatePlatformFunctions
//
// A call platform function call invoked by other drivers to tap into 
// IOPlatformFunction services.  They provide us their nub, which we search for
// available platform-do-xxx functions.  Any we find we parse and return to the
// caller in the pfArray parameter.
//*********************************************************************************
IOReturn IOPlatformFunctionDriver::instantiatePlatformFunctions (IOService *nub, OSArray **pfArray)
{
	OSDictionary			*propTable;
	OSCollectionIterator	*propIter;
	OSSymbol				*propKey;
	OSData					*propData, *moreData;
	OSArray					*tempArray;
	IOPlatformFunction		*platformFunction;
	IOReturn				result;
	
	result = kIOReturnSuccess;

	if ( ((propTable = nub->dictionaryWithProperties()) == 0) ||
	     ((propIter = OSCollectionIterator::withCollection(propTable)) == 0) ) {
		if (propTable) propTable->release();
		// Nothing to do, go home
		return result;
	}

	*pfArray = tempArray = NULL;
	
	/*
	 * Iterate through all the properties looking for any "platform-do-xxx" property names.  When we find
	 * one, we create one (or more) IOPlatformFunction objects based on the data and add the objects
	 * to an OSArray of IOPlatformFunction objects.  We do this for every "platform-do-xxx" property
	 * we find.
	 */
	while ((propKey = OSDynamicCast(OSSymbol, propIter->getNextObject())) != 0) {
		if (strncmp(kFunctionProvidedPrefix,
		            propKey->getCStringNoCopy(),
		            strlen(kFunctionProvidedPrefix)) == 0) {
			propData = OSDynamicCast(OSData, propTable->getObject(propKey));

			/*
			 * OK, we have a platform-do-xxx function property and create an IOPlatformFunction object based
			 * on the data.  Note that the data may indicate more than on action assigned to the function.
			 * For example, you might have a function named "platform-do-set-power" that requires
			 * different actions going down to sleep than waking from sleep.  In such a case, the additional
			 * data is returned as a new OSData object in moreData and we create separate objects based on 
			 * this data.
			 */
			do {
				moreData = NULL;
				if (platformFunction = IOPlatformFunction::withPlatformDoFunction(propKey, propData, &moreData)) {
					if (!tempArray) {
						tempArray = OSArray::withCapacity (1);
						if (!tempArray) {
							result =  kIOReturnNoMemory;
							break;
						}
					}
					// Add object to array, which will retain object
					tempArray->setObject(platformFunction);
					platformFunction->release();
					
					// If there's more data, loop and create additional platform function object(s) with remaining data
					propData = moreData;
				}
			} while (moreData);
		}
    }
	
	if (propTable) propTable->release();
	if (propIter) propIter->release();
	
	*pfArray = tempArray;
	return result;
}



