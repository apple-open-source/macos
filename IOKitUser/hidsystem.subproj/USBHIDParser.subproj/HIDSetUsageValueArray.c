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
	File:		HIDSetUsageValueArray.c
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDSetUsageValueArray - Set the values for a usage
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  usagePage			   - Page Criteria
 *			  iCollection			- Collection Criteria or zero
 *			  usage				   - usage Criteria
 *			  psBuffer				- Pointer to usage Buffer
 *			  iByteLength			- Length of usage Buffer
 *			  ptPreparsedData		- Pre-Parsed Data
 *			  psReport				- An HID Report
 *			  iReportLength			- The length of the Report
 *	 Output:
 *			  piValue				- Pointer to usage Value
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDSetUsageValueArray(HIDReportType reportType,
									HIDUsage usagePage,
									UInt32 iCollection,
									HIDUsage usage,
									UInt8 *psUsageBuffer,
									UInt32 iByteLength,
									HIDPreparsedDataRef preparsedDataRef,
									void *psReport,
									UInt32 iReportLength)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	HIDCollection *ptCollection;
	HIDReportItem *ptReportItem;
	OSStatus iStatus;
	int i;
	int iR;
	long iValue;
	int iStart;
	int iReportItem;
	UInt32 iUsageIndex;
	UInt32 iCount;
	int byteCount;
	Boolean bIncompatibleReport = false;
	Boolean butNotReally = false;
/*
 *	Disallow Null Pointers
*/
	if ((ptPreparsedData == NULL)
	 || (psUsageBuffer == NULL)
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
		 && HIDHasUsage(preparsedDataRef,ptReportItem,usagePage,usage,&iUsageIndex,&iCount))
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
 *				Disallow single count variables
 *				Count is set by HasUsage
*/
				if (iCount <= 1)
					return kHIDNotValueArrayErr;
/*
 *				Write out the data
*/
				iStart = ptReportItem->startBit + (ptReportItem->globals.reportSize * iUsageIndex);
				byteCount = (ptReportItem->globals.reportSize * iCount + 7)/8;
				if (byteCount > iByteLength)
					byteCount = iByteLength;
				for (i=0; i<byteCount; i++)
				{
					iValue = *psUsageBuffer++;
					iStatus = HIDPreProcessRIValue (ptReportItem, &iValue);
					iStatus = HIDPutData(psReport, iReportLength, iStart, 8, iValue);
					if (iStatus != kHIDSuccess)
						return iStatus;
					iStart += 8;
				}
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
