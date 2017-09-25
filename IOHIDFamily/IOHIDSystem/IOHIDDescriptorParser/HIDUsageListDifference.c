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
	File:		HIDUsageListDifference.c

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
 * In - Is a usage in a UsageList?
 *
 *	 Input:
 *			  piUsageList			- usage List
 *			  iUsageListLength		- Max entries in usage Lists
 *			  usage				   - The usage
 *	 Output:
 *	 Returns: true or false
 *
 *------------------------------------------------------------------------------
*/
static Boolean IsUsageInUsageList(HIDUsage *piUsageList, UInt32 iUsageListLength, HIDUsage usage)
{
	unsigned int i;
	for (i = 0; i < iUsageListLength; i++)
		if (piUsageList[i] == usage)
			return true;
	return false;
}

/*
 *------------------------------------------------------------------------------
 *
 * HIDUsageListDifference - Return adds and drops given present and past
 *
 *	 Input:
 *			  piPreviouUL			- Previous usage List
 *			  piCurrentUL			- Current usage List
 *			  piBreakUL				- Break usage List
 *			  piMakeUL				- Make usage List
 *			  iUsageListLength		- Max entries in usage Lists
 *	 Output:
 *			  piBreakUL				- Break usage List
 *			  piMakeUL				- Make usage List
 *	 Returns:
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDUsageListDifference(HIDUsage *piPreviousUL, HIDUsage *piCurrentUL, HIDUsage *piBreakUL, HIDUsage *piMakeUL, UInt32 iUsageListLength)
{
	int i;
	HIDUsage usage;
	int iBreakLength=0;
	int iMakeLength=0;
	for (i = 0; i < (int)iUsageListLength; i++)
	{
/*
 *		If in Current but not Previous then it's a Make
*/
		usage = piCurrentUL[i];
		if ((usage != 0) && (!IsUsageInUsageList(piPreviousUL,iUsageListLength,usage))
						  && (!IsUsageInUsageList(piMakeUL,iMakeLength,usage)))
			piMakeUL[iMakeLength++] = usage;
/*
 *		If in Previous but not Current then it's a Break
*/
		usage = piPreviousUL[i];
		if ((usage != 0) && (!IsUsageInUsageList(piCurrentUL,iUsageListLength,usage))
						  && (!IsUsageInUsageList(piBreakUL,iBreakLength,usage)))
			piBreakUL[iBreakLength++] = usage;
	}
/*
 *	Clear the rest of the usage Lists
*/
	while (iMakeLength < (int)iUsageListLength)
		piMakeUL[iMakeLength++] = 0;
	while (iBreakLength < (int)iUsageListLength)
		piBreakUL[iBreakLength++] = 0;
	return kHIDSuccess;
}
