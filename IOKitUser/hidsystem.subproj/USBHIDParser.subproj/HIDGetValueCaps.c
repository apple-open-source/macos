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
	File:		HIDGetValueCaps.c
*/

#include "HIDLib.h"

#if 0	// Moving to Capabilities instead of Caps.
/*
 *------------------------------------------------------------------------------
 *
 * HIDGetSpecificValueCaps - Get the binary values for a report type
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  usagePage			   - Page Criteria or zero
 *			  iCollection			- Collection Criteria or zero
 *			  usage				   - usage Criteria or zero
 *			  valueCaps			 - ValueCaps Array
 *			  piValueCapsLength	   - Maximum Entries
 *			  ptPreparsedData		- Pre-Parsed Data
 *	 Output:
 *			  piValueCapsLength	   - Entries Populated
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDGetSpecificValueCaps(HIDReportType reportType,
									  HIDUsage usagePage,
									  UInt32 iCollection,
									  HIDUsage usage,
									  HIDValueCapsPtr valueCaps,
									  UInt32 *piValueCapsLength,
									  HIDPreparsedDataRef preparsedDataRef)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	HIDCollection *ptCollection;
	HIDCollection *ptParent;
	HIDReportItem *ptReportItem;
	HIDP_UsageItem *ptUsageItem;
	HIDStringItem *ptStringItem;
	HIDDesignatorItem *ptDesignatorItem;
	HIDP_UsageItem *ptFirstCollectionUsageItem;
	HIDValueCaps *ptCapability;
	int iR, iU;
	int parent;
	int iReportItem, iUsageItem;
	int iMaxCaps;
	UInt32 iCount;
/*
 *	Disallow Null Pointers
*/
	if ((valueCaps == NULL)
	 || (piValueCapsLength == NULL)
	 || (ptPreparsedData == NULL))
		return kHIDNullPointerErr;
	if (ptPreparsedData->hidTypeIfValid != kHIDOSType)
		return kHIDInvalidPreparsedDataErr;
/*
 *	Save the buffer size
*/
	iMaxCaps = *piValueCapsLength;
	*piValueCapsLength = 0;
/*
 *	The Collection must be in range
*/
	if ((iCollection < 0) || (iCollection >= ptPreparsedData->collectionCount))
		return kHIDBadParameterErr;
/*
 *	Search only the scope of the Collection specified
*/
	ptCollection = &ptPreparsedData->collections[iCollection];
	for (iR=0; iR<ptCollection->reportItemCount; iR++)
	{
		iReportItem = ptCollection->firstReportItem + iR;
		ptReportItem = &ptPreparsedData->reportItems[iReportItem];
/*
 *		Search only reports of the proper type
*/
		if ((ptReportItem->reportType == reportType)
		 && ((ptReportItem->globals.usagePage == usagePage)
		  || (usagePage == 0))
		 && HIDIsVariable(ptReportItem, preparsedDataRef))
		{
/*
 *			Search the usages
*/
			  for (iU=0; iU<ptReportItem->usageItemCount; iU++)
			  {
/*
 *				  Copy all usages if the usage above is zero
 *					or copy all that "match"
*/
				  iUsageItem = ptReportItem->firstUsageItem + iU;
				  ptUsageItem = &ptPreparsedData->usageItems[iUsageItem];
				  
				  // ¥¥ we assume there is a 1-1 corresponence between usage items, string items, and designator items
				  // ¥¥Êthis is not necessarily the case, but its better than nothing
				  ptStringItem = &ptPreparsedData->stringItems[ptReportItem->firstStringItem + iU];
				  ptDesignatorItem = &ptPreparsedData->desigItems[ptReportItem->firstDesigItem + iU];

				  if (HIDUsageInRange(ptUsageItem,usagePage,usage))
				  {
/*
 *					  Only copy if there's room
*/
					  if (*piValueCapsLength >= iMaxCaps)
						  return kHIDBufferTooSmallErr;
					  ptCapability = &valueCaps[(*piValueCapsLength)++];



/*
 *					  Populate the Capability Structure
*/
					  parent = ptReportItem->parent;
					  ptParent = &ptPreparsedData->collections[parent];
					  ptFirstCollectionUsageItem = &ptPreparsedData->usageItems[ptParent->firstUsageItem];
					  ptCapability->collection = parent;
					  ptCapability->collectionUsagePage = ptParent->usagePage;
					  ptCapability->collectionUsage = ptFirstCollectionUsageItem->usage;
					  ptCapability->bitField =	ptReportItem->dataModes;
					  ptCapability->reportID = ptReportItem->globals.reportID;
					  ptCapability->usagePage = ptUsageItem->usagePage;
					
					  ptCapability->isAbsolute = !(ptReportItem->dataModes & kHIDDataRelative);

					  ptCapability->isRange = ptUsageItem->isRange;
					  if (ptUsageItem->isRange)
					  {
						ptCapability->u.range.usageMin = ptUsageItem->usageMinimum;
						ptCapability->u.range.usageMax = ptUsageItem->usageMaximum;
					  }
					  else
						ptCapability->u.notRange.usage = ptUsageItem->usage;

					  // if there really are that many items
					  if (iU < ptReportItem->stringItemCount)
					  {
						  ptCapability->isStringRange = ptStringItem->isRange;
						  
						  if (ptStringItem->isRange)
						  {
							ptCapability->u.range.stringMin = ptStringItem->minimum;
							ptCapability->u.range.stringMax = ptStringItem->maximum;
						  }
						  else
							ptCapability->u.notRange.stringIndex = ptStringItem->index;
					  }
					  // default, clear it
					  else
					  {
					  	ptCapability->isStringRange = false;
						ptCapability->u.notRange.stringIndex = 0;
					  }

					  // if there really are that many items
					  if (iU < ptReportItem->desigItemCount)
					  {
						  ptCapability->isDesignatorRange = ptDesignatorItem->isRange;
						  
						  if (ptDesignatorItem->isRange)
						  {
							ptCapability->u.range.designatorMin = ptDesignatorItem->minimum;
							ptCapability->u.range.designatorMax = ptDesignatorItem->maximum;
						  }
						  else
							ptCapability->u.notRange.designatorIndex = ptDesignatorItem->index;
					  }
					  // default, clear it
					  else
					  {
					  	ptCapability->isDesignatorRange = false;
						ptCapability->u.notRange.designatorIndex = 0;
					  }

					  ptCapability->bitSize = ptReportItem->globals.reportSize;

					  ptCapability->logicalMin = ptReportItem->globals.logicalMinimum;
					  ptCapability->logicalMax = ptReportItem->globals.logicalMaximum;
					  ptCapability->physicalMin = ptReportItem->globals.physicalMinimum;
					  ptCapability->physicalMax = ptReportItem->globals.physicalMaximum;

					  if (ptUsageItem->isRange)
					  {
						  iCount = ptUsageItem->usageMaximum - ptUsageItem->usageMinimum;
						  if (iCount < 0)
							  iCount = -iCount;
					  }
					  else
						  HIDHasUsage(preparsedDataRef,ptReportItem,
										ptUsageItem->usagePage,ptUsageItem->usage,
										NULL,&iCount);
					  ptCapability->reportCount = iCount;
				  }
			  }
		}
	}
	return kHIDSuccess;
}

/*
 *------------------------------------------------------------------------------
 *
 * HIDGetValueCaps - Get the binary values for a report type
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  valueCaps			 - ValueCaps Array
 *			  piValueCapsLength	   - Maximum Entries
 *			  ptPreparsedData		- Pre-Parsed Data
 *	 Output:
 *			  piValueCapsLength	   - Entries Populated
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDGetValueCaps(HIDReportType reportType,
							   HIDValueCapsPtr valueCaps,
							   UInt32 *piValueCapsLength,
							   HIDPreparsedDataRef preparsedDataRef)
{
	return HIDGetSpecificValueCaps(reportType,0,0,0,valueCaps,
									 piValueCapsLength,preparsedDataRef);
}

#endif

/*
 *------------------------------------------------------------------------------
 *
 * HIDGetSpecificValueCapabilities - Get the binary values for a report type
 *									This is the same as HIDGetSpecificValueCaps,
 *									except that it takes a HIDValueCapabilitiesPtr
 *									so it can return units and unitExponents.
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  usagePage			   - Page Criteria or zero
 *			  iCollection			- Collection Criteria or zero
 *			  usage				   - usage Criteria or zero
 *			  valueCaps			 - ValueCaps Array
 *			  piValueCapsLength	   - Maximum Entries
 *			  ptPreparsedData		- Pre-Parsed Data
 *	 Output:
 *			  piValueCapsLength	   - Entries Populated
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDGetSpecificValueCapabilities(HIDReportType reportType,
									  HIDUsage usagePage,
									  UInt32 iCollection,
									  HIDUsage usage,
									  HIDValueCapabilitiesPtr valueCaps,
									  UInt32 *piValueCapsLength,
									  HIDPreparsedDataRef preparsedDataRef)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	HIDCollection *ptCollection;
	HIDCollection *ptParent;
	HIDReportItem *ptReportItem;
	HIDP_UsageItem *ptUsageItem;
	HIDStringItem *ptStringItem;
	HIDDesignatorItem *ptDesignatorItem;
	HIDP_UsageItem *ptFirstCollectionUsageItem;
	HIDValueCapabilities *ptCapability;
	int iR, iU;
	int parent;
	int iReportItem, iUsageItem;
	int iMaxCaps;
	UInt32 iCount;
/*
 *	Disallow Null Pointers
*/
	if ((valueCaps == NULL)
	 || (piValueCapsLength == NULL)
	 || (ptPreparsedData == NULL))
		return kHIDNullPointerErr;
	if (ptPreparsedData->hidTypeIfValid != kHIDOSType)
		return kHIDInvalidPreparsedDataErr;
/*
 *	Save the buffer size
*/
	iMaxCaps = *piValueCapsLength;
	*piValueCapsLength = 0;
/*
 *	The Collection must be in range
*/
	if ((iCollection < 0) || (iCollection >= ptPreparsedData->collectionCount))
		return kHIDBadParameterErr;
/*
 *	Search only the scope of the Collection specified
*/
	ptCollection = &ptPreparsedData->collections[iCollection];
	for (iR=0; iR<ptCollection->reportItemCount; iR++)
	{
		iReportItem = ptCollection->firstReportItem + iR;
		ptReportItem = &ptPreparsedData->reportItems[iReportItem];
/*
 *		Search only reports of the proper type
*/
		if ((ptReportItem->reportType == reportType)
		 && ((ptReportItem->globals.usagePage == usagePage)
		  || (usagePage == 0))
		 && HIDIsVariable(ptReportItem, preparsedDataRef))
		{
/*
 *			Search the usages
*/
			  for (iU=0; iU<ptReportItem->usageItemCount; iU++)
			  {
/*
 *				  Copy all usages if the usage above is zero
 *					or copy all that "match"
*/
				  iUsageItem = ptReportItem->firstUsageItem + iU;
				  ptUsageItem = &ptPreparsedData->usageItems[iUsageItem];
				  
				  // ¥¥ we assume there is a 1-1 corresponence between usage items, string items, and designator items
				  // ¥¥Êthis is not necessarily the case, but its better than nothing
				  ptStringItem = &ptPreparsedData->stringItems[ptReportItem->firstStringItem + iU];
				  ptDesignatorItem = &ptPreparsedData->desigItems[ptReportItem->firstDesigItem + iU];

				  if (HIDUsageInRange(ptUsageItem,usagePage,usage))
				  {
/*
 *					  Only copy if there's room
*/
					  if (*piValueCapsLength >= iMaxCaps)
						  return kHIDBufferTooSmallErr;
					  ptCapability = &valueCaps[(*piValueCapsLength)++];

/*
 *					  Populate the Capability Structure
*/
					  parent = ptReportItem->parent;
					  ptParent = &ptPreparsedData->collections[parent];
					  ptFirstCollectionUsageItem = &ptPreparsedData->usageItems[ptParent->firstUsageItem];
					  ptCapability->collection = parent;
					  ptCapability->collectionUsagePage = ptParent->usagePage;
					  ptCapability->collectionUsage = ptFirstCollectionUsageItem->usage;
					  ptCapability->bitField =	ptReportItem->dataModes;
					  ptCapability->reportID = ptReportItem->globals.reportID;
					  ptCapability->usagePage = ptUsageItem->usagePage;
					  ptCapability->unitExponent = ptReportItem->globals.unitExponent;
					  ptCapability->units = ptReportItem->globals.units;
					  ptCapability->reserved = 0;							// for future expansion
					  ptCapability->pbVersion = kHIDCurrentCapabilitiesPBVersion;
					
					  ptCapability->isAbsolute = !(ptReportItem->dataModes & kHIDDataRelative);

					  ptCapability->isRange = ptUsageItem->isRange;
					  if (ptUsageItem->isRange)
					  {
						ptCapability->u.range.usageMin = ptUsageItem->usageMinimum;
						ptCapability->u.range.usageMax = ptUsageItem->usageMaximum;
					  }
					  else
						ptCapability->u.notRange.usage = ptUsageItem->usage;

					  // if there really are that many items
					  if (iU < ptReportItem->stringItemCount)
					  {
						  ptCapability->isStringRange = ptStringItem->isRange;
						  
						  if (ptStringItem->isRange)
						  {
							ptCapability->u.range.stringMin = ptStringItem->minimum;
							ptCapability->u.range.stringMax = ptStringItem->maximum;
						  }
						  else
							ptCapability->u.notRange.stringIndex = ptStringItem->index;
					  }
					  // default, clear it
					  else
					  {
					  	ptCapability->isStringRange = false;
						ptCapability->u.notRange.stringIndex = 0;
					  }

					  // if there really are that many items
					  if (iU < ptReportItem->desigItemCount)
					  {
						  ptCapability->isDesignatorRange = ptDesignatorItem->isRange;
						  
						  if (ptDesignatorItem->isRange)
						  {
							ptCapability->u.range.designatorMin = ptDesignatorItem->minimum;
							ptCapability->u.range.designatorMax = ptDesignatorItem->maximum;
						  }
						  else
							ptCapability->u.notRange.designatorIndex = ptDesignatorItem->index;
					  }
					  // default, clear it
					  else
					  {
					  	ptCapability->isDesignatorRange = false;
						ptCapability->u.notRange.designatorIndex = 0;
					  }

					  ptCapability->bitSize = ptReportItem->globals.reportSize;

					  ptCapability->logicalMin = ptReportItem->globals.logicalMinimum;
					  ptCapability->logicalMax = ptReportItem->globals.logicalMaximum;
					  ptCapability->physicalMin = ptReportItem->globals.physicalMinimum;
					  ptCapability->physicalMax = ptReportItem->globals.physicalMaximum;

					  if (ptUsageItem->isRange)
					  {
						  iCount = ptUsageItem->usageMaximum - ptUsageItem->usageMinimum;
						  if (iCount < 0)
							  iCount = -iCount;
					  }
					  else
						  HIDHasUsage(preparsedDataRef,ptReportItem,
										ptUsageItem->usagePage,ptUsageItem->usage,
										NULL,&iCount);
					  ptCapability->reportCount = iCount;
				  }
			  }
		}
	}
	return kHIDSuccess;
}

/*
 *------------------------------------------------------------------------------
 *
 * HIDGetValueCapabilities - Get the binary values for a report type
 *									This is the same as HIDGetValueCaps,
 *									except that it takes a HIDValueCapabilitiesPtr
 *									so it can return units and unitExponents.
 *
 *	 Input:
 *			  reportType		   - HIDP_Input, HIDP_Output, HIDP_Feature
 *			  valueCaps			 - ValueCaps Array
 *			  piValueCapsLength	   - Maximum Entries
 *			  ptPreparsedData		- Pre-Parsed Data
 *	 Output:
 *			  piValueCapsLength	   - Entries Populated
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDGetValueCapabilities(HIDReportType reportType,
							   HIDValueCapabilitiesPtr valueCaps,
							   UInt32 *piValueCapsLength,
							   HIDPreparsedDataRef preparsedDataRef)
{
	return HIDGetSpecificValueCapabilities(reportType,0,0,0,valueCaps,
									 piValueCapsLength,preparsedDataRef);
}
