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
	File:		HIDHasUsage.c
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HidP_UsageFromIndex
 *
 *	 Input:
 *			  ptPreparsedData		- The Preparsed Data
 *			  ptReportItem			- The Report Item
 *			  usagePage			   - The usage Page to find
 *			  usage				   - The usage to find
 *			  piIndex(optional)		- The usage Index pointer
 *			  piCount(optional)		- The usage Count pointer
 *	 Output:
 *			  piIndex				- The usage Index
 *	 Returns:
 *			  The usage
 *
 *------------------------------------------------------------------------------
*/
Boolean HIDHasUsage (HIDPreparsedDataRef preparsedDataRef,
					   HIDReportItem *ptReportItem,
					   HIDUsage usagePage, HIDUsage usage,
					   UInt32 *piIndex, UInt32 *piCount)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	int iUsageItem;
	UInt32 iUsageIndex;
	int iUsages;
	int i;
	UInt32 iCountsLeft;
	HIDP_UsageItem *ptUsageItem;
	Boolean bOnPage;
/*
 *	Disallow Null Pointers
*/
	if ((ptPreparsedData == NULL)
	 || (ptReportItem == NULL))
		return 0;
	if (ptPreparsedData->hidTypeIfValid != kHIDOSType)
		return 0;
/*
 *	Look through the usage Items for this usage
*/
	iUsageItem = ptReportItem->firstUsageItem;
	iUsageIndex = 0;
	for (i=0; i<ptReportItem->usageItemCount; i++)
	{
/*
 *	   Each usage Item is either a usage or a usage range
*/
		ptUsageItem = &ptPreparsedData->usageItems[iUsageItem++];
		bOnPage = ((usagePage == 0) || (usagePage == ptUsageItem->usagePage));
		if (ptUsageItem->isRange)
		{
/*
 *			For usage Ranges
 *			  If the index is in the range
 *				then return the usage
 *			  Otherwise adjust the index by the size of the range
*/
			if ((usage >= ptUsageItem->usageMinimum)
			 && (usage <= ptUsageItem->usageMaximum))
			{
				if (piIndex != NULL)
					*piIndex = iUsageIndex + (ptUsageItem->usageMinimum - usage);
/*
 *				If this usage is the last one for this ReportItem
 *				  then it gets all of the remaining reportCount
*/
				if (piCount != NULL)
				{
					if (((i+1) == ptReportItem->usageItemCount)
					 && (usage == ptUsageItem->usageMaximum))
					{
						iCountsLeft = ptReportItem->globals.reportCount - iUsageIndex - 1;
						if (iCountsLeft > 1)
							*piCount = iCountsLeft;
						else
							*piCount = 1;
					}
					else
						*piCount = 1;
				}
				if (bOnPage)
					return true;
			}
			iUsages = ptUsageItem->usageMaximum - ptUsageItem->usageMinimum + 1;
			if (iUsages < 0)
				iUsages = -iUsages;
			iUsageIndex += iUsages;
		}
		else
		{
/*
 *			For Usages
 *			If the index is zero
 *			  then return this usage
 *			Otherwise one less to index through
*/
			if (usage == ptUsageItem->usage)
			{
				if (piIndex != NULL)
					*piIndex = iUsageIndex;
				if (piCount != NULL)
				{
					if ((i+1) == ptReportItem->usageItemCount)
					{
						iCountsLeft = ptReportItem->globals.reportCount - iUsageIndex - 1;
						if (iCountsLeft > 1)
							*piCount = iCountsLeft;
						else
						   *piCount = 1;
					}
					else
						*piCount = 1;
				}
				if (bOnPage)
					return true;
			}
			iUsageIndex++;
		}
	}
	return false;
}
