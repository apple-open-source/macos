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
	File:		HIDCheckReport.c
*/

#include "HIDLib.h"

/*
 *------------------------------------------------------------------------------
 *
 * HIDCheckReport - Check the Report ID, Type, and Length
 *
 *	 Input:
 *			  reportType		   - The Specified Report Type
 *			  ptPreparsedData		- The Preparsed Data
 *			  ptReportItem			- The Report Item
 *			  psReport				- The Report
 *			  iReportLength			- The Report Length
 *	 Output:
 *	 Returns:
 *			  kHIDSuccess, HidP_IncompatibleReportID,
 *			  kHIDInvalidReportLengthErr, kHIDInvalidReportTypeErr
 *
 *------------------------------------------------------------------------------
*/
OSStatus HIDCheckReport(HIDReportType reportType, HIDPreparsedDataRef preparsedDataRef,
							 HIDReportItem *ptReportItem, void *report, UInt32 iReportLength)
{
	HIDPreparsedDataPtr ptPreparsedData = (HIDPreparsedDataPtr) preparsedDataRef;
	int reportID, reportIndex;
	int iExpectedLength;
	Byte * psReport = report;
/*
 *	See if this is the correct Report ID
*/
	reportID = psReport[0]&0xFF;
	if ((ptPreparsedData->reportCount > 1)
	 && (reportID != ptReportItem->globals.reportID))
		return kHIDIncompatibleReportErr;
/*
 *	Check for the correct Length for the Type
*/
	reportIndex = ptReportItem->globals.reportIndex;
	switch(reportType)
	{
		case kHIDInputReport:
			iExpectedLength = (ptPreparsedData->reports[reportIndex].inputBitCount + 7)/8;
			break;
		case kHIDOutputReport:
			iExpectedLength = (ptPreparsedData->reports[reportIndex].outputBitCount + 7)/8;
			break;
		case kHIDFeatureReport:
			iExpectedLength = (ptPreparsedData->reports[reportIndex].featureBitCount + 7)/8;
			break;
		default:
			return kHIDInvalidReportTypeErr;
	}
	if (iExpectedLength != iReportLength)
		return kHIDInvalidReportLengthErr;
	if (reportType != ptReportItem->reportType)
		return kHIDIncompatibleReportErr;
	return kHIDSuccess;
}
