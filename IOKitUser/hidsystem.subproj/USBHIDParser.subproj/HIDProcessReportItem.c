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
	File:		HIDProcessReportItem.c
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDProcessReportItem - Process a Report Item MainItem
 *
 *	 Input:
 *			  ptDescriptor			- The Descriptor Structure
 *			  ptPreparsedData		- The PreParsedData Structure
 *	 Output:
 *			  ptDescriptor			- The Descriptor Structure
 *			  ptPreparsedData		- The PreParsedData Structure
 *	 Returns:
 *			  kHIDSuccess		   - Success
 *			  kHIDNullPointerErr	  - Argument, Pointer was Null
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDProcessReportItem(HIDReportDescriptor *ptDescriptor, HIDPreparsedDataPtr ptPreparsedData)
{
	OSStatus error = noErr;
	HIDReportItem *ptReportItem;
	HIDReportSizes *ptReport;
	int iBits;
/*
 *	Disallow NULL Pointers
*/

	if ((ptDescriptor == NULL) || (ptPreparsedData == NULL))
		return kHIDNullPointerErr;
/*
 *	Begin to initialize the new Report Item structure
*/

	ptReportItem = &ptPreparsedData->reportItems[ptPreparsedData->reportItemCount++];
	ptReportItem->dataModes = ptDescriptor->item.unsignedValue;
	ptReportItem->globals = ptDescriptor->globals;
	ptReportItem->flags = 0;
	
/*
 *	Reality Check on the Report Main Item
*/
	// Don't check ranges for constants (MS Sidewinder, for one, does not reset)
	//if (!(ptReportItem->dataModes & kHIDDataConstantBit)) // don't think we need this anymore
	{
		// Determine the maximum signed value for a given report size.
		// (Don't allow shifting into sign bit.)
		SInt32 posSize = (ptReportItem->globals.reportSize >= 32) ? 
						31 : ptReportItem->globals.reportSize;
		SInt32 realMax = (1<<posSize) - 1;
		
		if (ptReportItem->globals.logicalMinimum > realMax)
		{
			error = kHIDBadLogicalMinimumErr;
			ptReportItem->globals.logicalMinimum = 0;
		}
		if (ptReportItem->globals.logicalMaximum > realMax)
		{
			if (error == noErr)
				error = kHIDBadLogicalMaximumErr;
			ptReportItem->globals.logicalMaximum = realMax;
		}
		if (ptReportItem->globals.logicalMinimum > ptReportItem->globals.logicalMaximum)
		{
			SInt32	temp;
			if (error == noErr)
				error = kHIDInvertedLogicalRangeErr;
			
			// mark as a 'reversed' item
			ptReportItem->flags |= kHIDReportItemFlag_Reversed;
			
			temp = ptReportItem->globals.logicalMaximum;
			ptReportItem->globals.logicalMaximum = ptReportItem->globals.logicalMinimum;
			ptReportItem->globals.logicalMinimum = temp;
		}
	}
	
	// check to see if we got half a range (we don't need to fix this, since 'isRange' will be false
	if ((error == noErr) && (ptDescriptor->haveUsageMin || ptDescriptor->haveUsageMax))
		error = kHIDUnmatchedUsageRangeErr;
	if ((error == noErr) && (ptDescriptor->haveStringMin || ptDescriptor->haveStringMax))
		error = kHIDUnmatchedStringRangeErr;
	if ((error == noErr) && (ptDescriptor->haveDesigMin || ptDescriptor->haveDesigMax))
		error = kHIDUnmatchedDesignatorRangeErr;
	
	// if the physical min/max are out of wack, use the logical values
	if (ptReportItem->globals.physicalMinimum >= ptReportItem->globals.physicalMaximum)
	{
		// equal to each other is not an error, just means to use the logical values
		if ((error == noErr) && 
			(ptReportItem->globals.physicalMinimum > ptReportItem->globals.physicalMaximum))
			error = kHIDInvertedPhysicalRangeErr;

		ptReportItem->globals.physicalMinimum = ptReportItem->globals.logicalMinimum;
		ptReportItem->globals.physicalMaximum = ptReportItem->globals.logicalMaximum;
	}
	
	// if strict error checking is true, return any errors
	if (error != noErr && ptPreparsedData->flags & kHIDFlag_StrictErrorChecking)
		return error;
	
/*
 *	Continue to initialize the new Report Item structure
*/

	ptReportItem->parent = ptDescriptor->parent;
	ptReportItem->firstUsageItem = ptDescriptor->firstUsageItem;
	ptDescriptor->firstUsageItem = ptPreparsedData->usageItemCount;
	ptReportItem->usageItemCount = ptPreparsedData->usageItemCount - ptReportItem->firstUsageItem;
	ptReportItem->firstStringItem = ptDescriptor->firstStringItem;
	ptDescriptor->firstStringItem = ptPreparsedData->stringItemCount;
	ptReportItem->stringItemCount = ptPreparsedData->stringItemCount - ptReportItem->firstStringItem;
	ptReportItem->firstDesigItem = ptDescriptor->firstDesigItem;
	ptDescriptor->firstDesigItem = ptPreparsedData->desigItemCount;
	ptReportItem->desigItemCount = ptPreparsedData->desigItemCount - ptReportItem->firstDesigItem;
/*
 *	Update the Report by the size of this item
*/

	ptReport = &ptPreparsedData->reports[ptReportItem->globals.reportIndex];
	iBits = ptReportItem->globals.reportSize * ptReportItem->globals.reportCount;
	switch (ptDescriptor->item.tag)
	{
		case kHIDTagFeature:
			ptReportItem->reportType = kHIDFeatureReport;
            ptReportItem->startBit = ptReport->featureBitCount;
			ptReport->featureBitCount += iBits;
			break;
		case kHIDTagOutput:
			ptReportItem->reportType = kHIDOutputReport;
            ptReportItem->startBit = ptReport->outputBitCount;
			ptReport->outputBitCount += iBits;
			break;
		case kHIDTagInput:
			ptReportItem->reportType = kHIDInputReport;
            ptReportItem->startBit = ptReport->inputBitCount;
			ptReport->inputBitCount += iBits;
			break;
		default:
			ptReportItem->reportType = kHIDUnknownReport;
            ptReportItem->startBit = 0;
			break;
	}
	return kHIDSuccess;
}
