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
	File:		HIDProcessLocalItem.c

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	� 1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(BWS)	Brent Schorsch

	Change History (most recent first):

	  <USB1>	  3/5/99	BWS		first checked in
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDProcessLocalItem - Process a LocalItem
 *
 *   Input:
 *            ptDescriptor          - The Descriptor Structure
 *            ptPreparsedData       - The PreParsedData Structure
 *   Output:
 *            ptPreparsedData       - The PreParsedData Structure
 *   Returns:
 *            kHIDSuccess          - Success
 *            kHIDNullPointerErr      - Argument, Pointer was Null
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDProcessLocalItem(HIDReportDescriptor *ptDescriptor,
                                  HIDPreparsedDataPtr ptPreparsedData)
{
    HIDDesignatorItem *ptDesignatorItem;
    HIDStringItem *ptStringItem;
    HIDP_UsageItem *ptUsageItem;
    HIDItem *ptItem;
/*
 *  Disallow NULL Pointers
*/
    if ((ptDescriptor == NULL) || (ptPreparsedData == NULL))
        return kHIDNullPointerErr;
/*
 *  Process the LocalItem by tag
*/
    ptItem = &ptDescriptor->item;
    switch (ptItem->tag)
    {
/*
 *      Note that Tag = usage Item may represent either
 *          a UsagePair with the usagePage implied, or
 *          a UsagePair defined by an extended usage
 *      If a Tag = usage Item has 1 or 2 bytes of data
 *          then the current usagePage is used
 *      If a Tag = usage Item has 4 bytes of data
 *          then the high order bytes are the usagePage
 *
 *      Note that the Microsoft HID Parser uses the last
 *          usagePage defined before the MainItem with which
 *          the usage is associated rather than the current
 *          usagePage.  The method used here is more generic
 *          although multiple UsagePages for a MainItem are
 *          unlikely due to the MS limitation.
*/
        case kHIDTagUsage:
            ptUsageItem = &ptPreparsedData->usageItems[ptPreparsedData->usageItemCount++];
            ptUsageItem->isRange = false;
            if (ptItem->byteCount == 4)
            {
                ptUsageItem->usagePage = ptItem->unsignedValue>>16;
                ptUsageItem->usage = ptItem->unsignedValue&0xFFFFL;
            }
            else
            {
                ptUsageItem->usagePage = ptDescriptor->globals.usagePage;
                ptUsageItem->usage = ptItem->unsignedValue;
            }
            break;
/*
 *      Note that Tag = usage Minimum Item may represent either
 *          a UsagePair with the usagePage implied, or
 *          a UsagePair defined by an extended usage
 *      If a Tag = usage Item has 1 or 2 bytes of data
 *          then the current usagePage is used
 *      If a Tag = usage Item has 4 bytes of data
 *          then the high order bytes are the usagePage
*/
        case kHIDTagUsageMinimum:
            if (ptDescriptor->haveUsageMax)
            {
                ptUsageItem = &ptPreparsedData->usageItems[ptPreparsedData->usageItemCount++];
                ptUsageItem->isRange = true;
                if (ptItem->byteCount == 4)
                {
                    ptUsageItem->usagePage = ptItem->unsignedValue>>16;
                    ptUsageItem->usageMinimum = ptItem->unsignedValue&0xFFFFL;
                }
                else
                {
                    ptUsageItem->usagePage = ptDescriptor->globals.usagePage;
                    ptUsageItem->usageMinimum = ptItem->unsignedValue;
                }
                if (ptUsageItem->usagePage != (HIDUsage)ptDescriptor->rangeUsagePage)
                    return kHIDInvalidRangePageErr;
                ptUsageItem->usageMaximum = ptDescriptor->usageMaximum;
                if (ptUsageItem->usageMaximum < ptUsageItem->usageMinimum)
                    return kHIDInvertedUsageRangeErr;
                ptDescriptor->haveUsageMax = false;
                ptDescriptor->haveUsageMin = false;
            }
            else
            {
                if (ptItem->byteCount == 4)
                {
                    ptDescriptor->rangeUsagePage = ptItem->unsignedValue>>16;
                    ptDescriptor->usageMinimum = ptItem->unsignedValue&0xFFFFL;
                }
                else
                {
                    ptDescriptor->rangeUsagePage = ptDescriptor->globals.usagePage;
                    ptDescriptor->usageMinimum = ptItem->unsignedValue;
                }
                ptDescriptor->haveUsageMin = true;
            }
            break;
/*
 *      Note that Tag = usage Maximum Item may represent either
 *          a UsagePair with the usagePage implied, or
 *          a UsagePair defined by an extended usage
 *      If a Tag = usage Item has 1 or 2 bytes of data
 *          then the current usagePage is used
 *      If a Tag = usage Item has 4 bytes of data
 *          then the high order bytes are the usagePage
*/
        case kHIDTagUsageMaximum:
            if (ptDescriptor->haveUsageMin)
            {
                ptUsageItem = &ptPreparsedData->usageItems[ptPreparsedData->usageItemCount++];
                ptUsageItem->isRange = true;
                if (ptItem->byteCount == 4)
                {
                    ptUsageItem->usagePage = ptItem->unsignedValue>>16;
                    ptUsageItem->usageMaximum = ptItem->unsignedValue&0xFFFFL;
                }
                else
                {
                    ptUsageItem->usagePage = ptDescriptor->globals.usagePage;
                    ptUsageItem->usageMaximum = ptItem->unsignedValue;
                }
                if (ptUsageItem->usagePage != (HIDUsage)ptDescriptor->rangeUsagePage)
                    return kHIDInvalidRangePageErr;
                ptUsageItem->usageMinimum = ptDescriptor->usageMinimum;
                if (ptUsageItem->usageMaximum < ptUsageItem->usageMinimum)
                    return kHIDInvertedUsageRangeErr;
                ptDescriptor->haveUsageMax = false;
                ptDescriptor->haveUsageMin = false;
            }
            else
            {
                if (ptItem->byteCount == 4)
                {
                    ptDescriptor->rangeUsagePage = ptItem->unsignedValue>>16;
                    ptDescriptor->usageMaximum = ptItem->unsignedValue&0xFFFFL;
                }
                else
                {
                    ptDescriptor->rangeUsagePage = ptDescriptor->globals.usagePage;
                    ptDescriptor->usageMaximum = ptItem->unsignedValue;
                }
                ptDescriptor->haveUsageMax = true;
            }
            break;
/*
 *      Designators
*/
        case kHIDTagDesignatorIndex:
            ptDesignatorItem = &ptPreparsedData->desigItems[ptPreparsedData->desigItemCount++];
            ptDesignatorItem->isRange = false;
            ptDesignatorItem->index = ptItem->unsignedValue;
            break;
        case kHIDTagDesignatorMinimum:
            if (ptDescriptor->haveDesigMax)
            {
                ptDesignatorItem = &ptPreparsedData->desigItems[ptPreparsedData->desigItemCount++];
                ptDesignatorItem->isRange = true;
                ptDesignatorItem->minimum = ptItem->unsignedValue;
                ptDesignatorItem->maximum = ptDescriptor->desigMaximum;
                ptDescriptor->haveDesigMin = false;
                ptDescriptor->haveDesigMax = false;
            }
            else
            {
                ptDescriptor->desigMinimum = ptItem->unsignedValue;
                ptDescriptor->haveDesigMin = true;
            }
            break;
        case kHIDTagDesignatorMaximum:
            if (ptDescriptor->haveDesigMin)
            {
                ptDesignatorItem = &ptPreparsedData->desigItems[ptPreparsedData->desigItemCount++];
                ptDesignatorItem->isRange = true;
                ptDesignatorItem->maximum = ptItem->unsignedValue;
                ptDesignatorItem->minimum = ptDescriptor->desigMinimum;
                ptDescriptor->haveDesigMin = false;
                ptDescriptor->haveDesigMax = false;
            }
            else
            {
                ptDescriptor->desigMaximum = ptItem->unsignedValue;
                ptDescriptor->haveDesigMax = true;
            }
            break;
/*
 *      Strings
*/
        case kHIDTagStringIndex:
            ptStringItem = &ptPreparsedData->stringItems[ptPreparsedData->stringItemCount++];
            ptStringItem->isRange = false;
            ptStringItem->index = ptItem->unsignedValue;
            break;
        case kHIDTagStringMinimum:
            if (ptDescriptor->haveStringMax)
            {
                ptStringItem = &ptPreparsedData->stringItems[ptPreparsedData->stringItemCount++];
                ptStringItem->isRange = true;
                ptStringItem->minimum = ptItem->unsignedValue;
                ptStringItem->maximum = ptDescriptor->stringMaximum;
                ptDescriptor->haveStringMin = false;
                ptDescriptor->haveStringMax = false;
            }
            else
            {
                ptDescriptor->stringMinimum = ptItem->unsignedValue;
                ptDescriptor->haveStringMin = true;
            }
            break;
        case kHIDTagStringMaximum:
            if (ptDescriptor->haveStringMin)
            {
                ptStringItem = &ptPreparsedData->stringItems[ptPreparsedData->stringItemCount++];
                ptStringItem->isRange = true;
                ptStringItem->maximum = ptItem->unsignedValue;
                ptStringItem->minimum = ptDescriptor->stringMinimum;
                ptDescriptor->haveStringMin = false;
                ptDescriptor->haveStringMax = false;
            }
            else
            {
                ptDescriptor->stringMaximum = ptItem->unsignedValue;
                ptDescriptor->haveStringMax = true;
            }
            break;
/*
 *      Delimiters (are not processed)
*/
        case kHIDTagSetDelimiter:
            break;
    }
    return kHIDSuccess;
}
