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
	File:		HIDSetUsageValue.c
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDSetUsageValue - Set the value for a usage
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  usagePage			   - Page Criteria or zero
 *			  iCollection			- Collection Criteria or zero
 *			  usage				   - usage Criteria or zero
 *			  iValue				- The usage Value
 *			  ptPreparsedData		- Pre-Parsed Data
 *			  psReport				- An HID Report
 *			  iReportLength			- The length of the Report
 *	 Output:
 *			  piValue				- Pointer to usage Value
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDSetUsageValue(HIDReportType reportType,
							   HIDUsage usagePage,
							   UInt32 iCollection,
							   HIDUsage usage,
							   SInt32 iUsageValue,
							   HIDPreparsedDataRef preparsedDataRef,
							   void *psReport,
							   ByteCount iReportLength)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	HIDCollection *ptCollection;
	HIDReportItem *ptReportItem;
	OSStatus iStatus;
	int iR;
	int iStart;
	int iReportItem;
	UInt32 iUsageIndex;
	Boolean bIncompatibleReport = false;
	Boolean butNotReally = false;
/*
 *	Disallow Null Pointers
*/
	if ((ptPreparsedData == NULL)
	 || (psReport == NULL))
		return kHIDNullPointerErr;
	if (ptPreparsedData->hidTypeIfValid != kHIDOSType)
		return kHIDInvalidPreparsedDataErr;
/*
 *	The Collection must be in range
*/
	if ((iCollection < 0) || (iCollection >= ptPreparsedData->collectionCount))
		return kHIDBadParameterErr;
/*
 *	Search only the scope of the Collection specified
 *	Go through the ReportItems
 *	Filter on ReportType and usagePage
*/
	ptCollection = &ptPreparsedData->collections[iCollection];
	for (iR=0; iR<ptCollection->reportItemCount; iR++)
	{
		iReportItem = ptCollection->firstReportItem + iR;
		ptReportItem = &ptPreparsedData->reportItems[iReportItem];
		if (HIDIsVariable(ptReportItem, preparsedDataRef)
		 && HIDHasUsage(preparsedDataRef,ptReportItem,usagePage,usage,&iUsageIndex,NULL))
		{
/*
 *			This may be the proper place
 *			Let's check for the proper Report ID, Type, and Length
*/
			iStatus = HIDCheckReport(reportType,preparsedDataRef,ptReportItem,
									   psReport,iReportLength);
/*
 *			The Report ID or Type may not match.
 *			This may not be an error (yet)
*/
			if (iStatus == kHIDIncompatibleReportErr)
				bIncompatibleReport = true;
			else if (iStatus != kHIDSuccess)
				return iStatus;
			else
			{
				butNotReally = true;
/*
 *				Write out the data
*/
				iStart = ptReportItem->startBit
					  + (ptReportItem->globals.reportSize * iUsageIndex);
				iStatus = HIDPreProcessRIValue (ptReportItem, &iUsageValue);
				iStatus = HIDPutData(psReport, iReportLength, iStart,
									   ptReportItem->globals.reportSize, iUsageValue);
				if (iStatus != kHIDSuccess)
					return iStatus;
				return kHIDSuccess;
			}
		}
	}
	// If any of the report items were not the right type, we have set the bIncompatibleReport flag.
	// However, if any of the report items really were the correct type, we have done our job of checking
	// and really didn't find a usage. Don't let the bIncompatibleReport flag wipe out our valid test.
	if (bIncompatibleReport && !butNotReally)
		return kHIDIncompatibleReportErr;
	return kHIDUsageNotFoundErr;
}
