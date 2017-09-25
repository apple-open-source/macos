/*
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
	File:		HIDProcessGlobalItem.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	� 1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(DF)	David Ferguson
		(BWS)	Brent Schorsch

	Change History (most recent first):

          <HID1>	  3/4/03	RY		Removed returning of error for zero values of:
                                                        reportSize, reportID and usagePage 
	  <USB2>	10/18/99	DF		Lets try not reporting an error with zero report count
	  <USB1>	  3/5/99	BWS		first checked in
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDProcessGlobalItem - Process a GlobalItem
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
OSStatus HIDProcessGlobalItem(HIDReportDescriptor *ptDescriptor, HIDPreparsedDataPtr ptPreparsedData)
{
	HIDReportSizes *ptReport;
	HIDGlobalItems *ptGlobals;
	HIDItem *ptItem;
	int reportIndex;
/*
 *	Disallow NULL Pointers
*/
	if ((ptDescriptor == NULL) || (ptPreparsedData == NULL))
		return kHIDNullPointerErr;
/*
 *	Process by tag
*/
	ptItem = &ptDescriptor->item;
	ptGlobals = &ptDescriptor->globals;
	switch (ptItem->tag)
	{
/*
 *		usage Page
*/
		case kHIDTagUsagePage:
#if 0
			// some device actually have a usage page of zero specified.  we must allow it!
			if (ptItem->unsignedValue == 0)
				return kHIDUsagePageZeroErr;
#endif
			ptGlobals->usagePage = ptItem->unsignedValue;
			break;
/*
 *		Logical Minimum
*/
		case kHIDTagLogicalMinimum:
			ptGlobals->logicalMinimum = ptItem->signedValue;
			break;
/*
 *		Logical Maximum
*/
		case kHIDTagLogicalMaximum:
			ptGlobals->logicalMaximum = ptItem->signedValue;
			break;
/*
 *		Physical Minimum
*/
		case kHIDTagPhysicalMinimum:
			ptGlobals->physicalMinimum = ptItem->signedValue;
			break;
/*
 *		Physical Maximum
*/
		case kHIDTagPhysicalMaximum:
			ptGlobals->physicalMaximum = ptItem->signedValue;
			break;
/*
 *		Unit Exponent
*/
		case kHIDTagUnitExponent:
			ptGlobals->unitExponent = ptItem->signedValue;
			break;
/*
 *		Unit
*/
		case kHIDTagUnit:
			ptGlobals->units = ptItem->unsignedValue;
			break;
/*
 *		Report Size in Bits
*/
		case kHIDTagReportSize:
			ptGlobals->reportSize = ptItem->unsignedValue;
#if 0
			// some device actually have a report size of zero specified.  we must allow it!
			if (ptGlobals->reportSize == 0)
				return kHIDReportSizeZeroErr;
#endif
			break;
/*
 *		Report ID
*/
		case kHIDTagReportID:
#if 0
			// some device actually have a report id of zero specified.  we must allow it!
			if (ptItem->unsignedValue == 0)
				return kHIDReportIDZeroErr;
#endif
/*
 *			Look for the Report ID in the table
*/
			reportIndex = 0;
			while ((reportIndex < (int)ptPreparsedData->reportCount)
				&& (ptPreparsedData->reports[reportIndex].reportID != (SInt32)ptItem->unsignedValue))
				reportIndex++;
/*
 *			Initialize the entry if it's new and there's room for it
 *			  Start with 8 bits for the Report ID
*/
			if (reportIndex == (int)ptPreparsedData->reportCount)
			{
				ptReport = &ptPreparsedData->reports[ptPreparsedData->reportCount++];
				ptReport->reportID = ptItem->unsignedValue;
				ptReport->inputBitCount = 8;
				ptReport->outputBitCount = 8;
				ptReport->featureBitCount = 8;
			}
/*
 *			Remember which report is being processed
*/
			ptGlobals->reportID = ptItem->unsignedValue;
			ptGlobals->reportIndex = reportIndex;
			break;
/*
 *		Report Count
*/
		case kHIDTagReportCount:
#if 0
			// some device actually have a report count of zero specified.  we must allow it!
			if (ptItem->unsignedValue == 0)
				return kHIDReportCountZeroErr;
#endif				
			ptGlobals->reportCount = ptItem->unsignedValue;
			break;
/*
 *		Push Globals
*/
		case kHIDTagPush:
			ptDescriptor->globalsStack[ptDescriptor->globalsNesting++] = ptDescriptor->globals;
			break;
/*
 *		Pop Globals
*/
		case kHIDTagPop:
			ptDescriptor->globals = ptDescriptor->globalsStack[--ptDescriptor->globalsNesting];
			break;
	}
	return kHIDSuccess;
}
