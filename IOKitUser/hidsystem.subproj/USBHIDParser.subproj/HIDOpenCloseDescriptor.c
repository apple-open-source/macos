/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
	File:		HIDOpenCloseDescriptor.c
*/

#include "HIDLib.h"

////#include <DriverServices.h>

//#include <stdlib.h>

/*
 *------------------------------------------------------------------------------
 *
 * HIDCloseReportDescriptor - Close the Descriptor
 *
 *	 Input:
 *			  ptPreparsedData		- The PreParsedData Structure
 *	 Output:
 *			  ptPreparsedData		- The PreParsedData Structure
 *	 Returns:
 *			  kHIDSuccess		   - Success
 *			  kHIDNullPointerErr	  - Argument, Pointer was Null
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDCloseReportDescriptor(HIDPreparsedDataRef preparsedDataRef)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
//	OSStatus iStatus;
/*
 *	Disallow NULL Pointers
*/
	if (ptPreparsedData == NULL)
		return kHIDNullPointerErr;
/*
 *	If it's marked closed then don't do anything
*/
	if (ptPreparsedData->hidTypeIfValid != kHIDOSType)
		return kHIDInvalidPreparsedDataErr;
/*
 *	Free any memory that was allocated
*/
	if (ptPreparsedData->rawMemPtr != NULL)
	{
//		PoolDeallocate (ptPreparsedData->rawMemPtr);
		free (ptPreparsedData->rawMemPtr);
		ptPreparsedData->rawMemPtr = NULL;
	}
/*
 *	Mark closed
*/
	ptPreparsedData->hidTypeIfValid = 0;
/*
 *	Deallocate the preparsed data
*/
//	iStatus = PoolDeallocate (ptPreparsedData);
//
//	return iStatus;
	free (ptPreparsedData);
    ptPreparsedData = NULL;

	return noErr;
}

/*
 *------------------------------------------------------------------------------
 *
 * HIDOpenReportDescriptor - Initialize the HID Parser
 *
 *	 Input:
 *			  psHidReportDescriptor - The HID Report Descriptor (String)
 *			  descriptorLength	   - Length of the Descriptor in bytes
 *			  ptPreparsedData		- The PreParsedData Structure
 *	 Output:
 *			  ptPreparsedData		- The PreParsedData Structure
 *	 Returns:
 *			  kHIDSuccess		   - Success
 *			  kHIDNullPointerErr	  - Argument, Pointer was Null
 *
 *------------------------------------------------------------------------------
*/
OSStatus
HIDOpenReportDescriptor	   (void *					hidReportDescriptor,
							UInt32					descriptorLength,
							HIDPreparsedDataRef *	preparsedDataRef,
							UInt32					flags)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	OSStatus iStatus;
	HIDReportDescriptor tDescriptor;

/*
 *	Disallow NULL Pointers
*/
	if ((hidReportDescriptor == NULL) || (preparsedDataRef == NULL))
		return kHIDNullPointerErr;
	
/*
 *	Initialize the return result, and allocate space for preparsed data
*/
	*preparsedDataRef = NULL;
	
//	ptPreparsedData = PoolAllocateResident (sizeof (HIDPreparsedData), kShouldClearMem);
	ptPreparsedData = malloc (sizeof (HIDPreparsedData));
	
/*
 *	Make sure we got the memory
*/
	if (ptPreparsedData == NULL)
		return kHIDNotEnoughMemoryErr;
        
    memset(ptPreparsedData, 0, sizeof(HIDPreparsedData));	// kShouldClearMem

/*
 *	Copy the flags field
*/
	ptPreparsedData->flags = flags;
/*
 *	Initialize the memory allocation pointer
*/
	ptPreparsedData->rawMemPtr = NULL;
/*
 *	Set up the descriptor structure
*/
	tDescriptor.descriptor = hidReportDescriptor;
	tDescriptor.descriptorLength = descriptorLength;
/*
 *	Count various items in the descriptor
 *	  allocate space within the PreparsedData structure
 *	  and initialize the counters there
*/
	iStatus = HIDCountDescriptorItems(&tDescriptor,ptPreparsedData);
	if (iStatus != kHIDSuccess)
		return iStatus;
/*
 *	Parse the Descriptor
 *	  filling in the structures in the PreparsedData structure
*/
	iStatus = HIDParseDescriptor(&tDescriptor, ptPreparsedData);
/*
 *	Mark the PreparsedData initialized, maybe
*/
	if (iStatus == kHIDSuccess && ptPreparsedData->rawMemPtr != NULL)
	{
		ptPreparsedData->hidTypeIfValid = kHIDOSType;
		*preparsedDataRef = (HIDPreparsedDataRef) ptPreparsedData;
	}
	else	// something failed, deallocate everything, and make sure we return an error
	{
//		if (ptPreparsedData->rawMemPtr != NULL)
//			PoolDeallocate (ptPreparsedData->rawMemPtr);
		if (ptPreparsedData->rawMemPtr != NULL)
        {
			free (ptPreparsedData->rawMemPtr);
            ptPreparsedData->rawMemPtr = NULL;
        }
			
//		PoolDeallocate (ptPreparsedData);
		free (ptPreparsedData);
        ptPreparsedData - NULL;
		
		if (iStatus == kHIDSuccess)
			iStatus = kHIDNotEnoughMemoryErr;
	}
	
	return iStatus;
}
