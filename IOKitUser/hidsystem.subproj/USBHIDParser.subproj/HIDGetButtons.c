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
	File:		HIDGetButtons.c
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDGetButtons - Get the state of the buttons for a Page
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  usagePage			   - Page Criteria or zero
 *			  iCollection			- Collection Criteria or zero
 *			  piUsageList			- Usages for pressed buttons
 *			  piUsageListLength		- Max entries in UsageList
 *			  ptPreparsedData		- Pre-Parsed Data
 *			  psReport				- An HID Report
 *			  iReportLength			- The length of the Report
 *	 Output:
 *			  piValue				- Pointer to usage Value
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus 
HIDGetButtons  (HIDReportType			reportType,
				UInt32					iCollection,
				HIDUsageAndPagePtr		ptUsageList,
				UInt32 *				piUsageListLength,
				HIDPreparsedDataRef 	preparsedDataRef,
				void *					psReport,
				UInt32					iReportLength)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	HIDCollection *ptCollection;
	HIDReportItem *ptReportItem;
	int iR, iE;
	long iValue;
	int iStart;
	int iReportItem;
	int iMaxUsages;
	HIDUsageAndPage tUsageAndPage;
	
/*
 *	Disallow Null Pointers
*/
	if ((ptPreparsedData == NULL)
	 || (ptUsageList == NULL)
	 || (piUsageListLength == NULL)
	 || (psReport == NULL))
		return kHIDNullPointerErr;
	if (ptPreparsedData->hidTypeIfValid != kHIDOSType)
		return kHIDInvalidPreparsedDataErr;
/*
 *	Save the UsageList size
*/
	iMaxUsages = *piUsageListLength;
	*piUsageListLength = 0;
/*
 *	Search only the scope of the Collection specified
 *	Go through the ReportItems
 *	Filter on ReportType
*/
	ptCollection = &ptPreparsedData->collections[iCollection];
	for (iR=0; iR<ptCollection->reportItemCount; iR++)
	{
		iReportItem = ptCollection->firstReportItem + iR;
		ptReportItem = &ptPreparsedData->reportItems[iReportItem];
		if ((ptReportItem->reportType == reportType)
		 && HIDIsButton(ptReportItem, preparsedDataRef))
		{
/*
 *			Save Arrays and Bitmaps
*/
			iStart = ptReportItem->startBit;
			for (iE=0; iE<ptReportItem->globals.reportCount; iE++)
			{
				OSStatus status = noErr;
				iValue = 0;
				
				if ((ptReportItem->dataModes & kHIDDataArrayBit) == kHIDDataArray)
				{
					status = HIDGetData(psReport, iReportLength, iStart, ptReportItem->globals.reportSize, &iValue, false);
					if (!status)
						status = HIDPostProcessRIValue (ptReportItem, &iValue);
					if (status) return status;
					
					iStart += ptReportItem->globals.reportSize;
					HIDUsageAndPageFromIndex(preparsedDataRef,ptReportItem,ptReportItem->globals.logicalMinimum+iE,&tUsageAndPage);
					if (*piUsageListLength >= iMaxUsages)
						return kHIDBufferTooSmallErr;
					ptUsageList[(*piUsageListLength)++] = tUsageAndPage;
				}
				else
				{
					status = HIDGetData(psReport, iReportLength, iStart, 1, &iValue, false);
					if (!status)
						status = HIDPostProcessRIValue (ptReportItem, &iValue);
					if (status) return status;

					iStart++;
					if (iValue != 0)
					{
						HIDUsageAndPageFromIndex(preparsedDataRef,ptReportItem,ptReportItem->globals.logicalMinimum+iE,&tUsageAndPage);
						if (*piUsageListLength >= iMaxUsages)
							return kHIDBufferTooSmallErr;
						ptUsageList[(*piUsageListLength)++] = tUsageAndPage;
					}
				}
			}
		}
	}
	return kHIDSuccess;
}
