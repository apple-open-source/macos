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
	File:		HIDNextItem.c
*/

#include "HIDLib.h"

/*
 *-----------------------------------------------------------------------------
 *
 * HIDNextItem - Get the Next Item
 *
 *	 Input:
 *			  ptDescriptor			- Descriptor Structure
 *	 Output:
 *			  ptItem				- Caller-provided Item Structure
 *	 Returns:
 *			  kHIDSuccess		  - Success
 *			  kHIDEndOfDescriptorErr - End of the HID Report Descriptor
 *
 *-----------------------------------------------------------------------------
*/
OSStatus HIDNextItem(HIDReportDescriptor *ptDescriptor)
{
	HIDItem *ptItem;
	unsigned char iHeader;
	unsigned char *psD;
	int i;
	int iLength;
	UInt32 *piX;
	int iSize;
	int iByte = 0;
/*
 *	Disallow Null Pointers
*/
	if (ptDescriptor==NULL)
		return kHIDNullPointerErr;
/*
 *	Use local pointers
*/
	ptItem = &ptDescriptor->item;
	psD = ptDescriptor->descriptor;
	piX = &ptDescriptor->index;
	iLength = ptDescriptor->descriptorLength;
/*
 *	Don't go past the end of the buffer
*/
	if (*piX >= iLength)
		return kHIDEndOfDescriptorErr;
/*
 *	Get the header byte
*/
	iHeader = psD[(*piX)++];
/*
 *	Don't go past the end of the buffer
*/
	if (*piX > iLength)
		return kHIDEndOfDescriptorErr;
	ptItem->itemType = iHeader;
	ptItem->itemType &= kHIDItemTypeMask;
	ptItem->itemType >>= kHIDItemTypeShift;
/*
 *	Long Item Header
 *	Skip Long Items!
*/
	if (iHeader==kHIDLongItemHeader)
	{
		iSize = psD[(*piX)++];
		ptItem->tag = *piX++;
	}
/*
 *	Short Item Header
*/
	else
	{
		iSize = iHeader;
		iSize &= kHIDItemSizeMask;
		if (iSize==3)
			iSize = 4;
		ptItem->byteCount = iSize;
		ptItem->tag = iHeader;
		ptItem->tag &= kHIDItemTagMask;
		ptItem->tag >>= kHIDItemTagShift;
	}
/*
 *	Don't go past the end of the buffer
*/
	if ((*piX + iSize) > iLength)
		return kHIDEndOfDescriptorErr;
/*
 *	Pick up the data
*/
	ptItem->unsignedValue = 0;
	if (iSize==0)
	{
		ptItem->signedValue = 0;
		return kHIDSuccess;
	}
/*
 *	Get the data bytes
*/
	for (i = 0; i < iSize; i++)
	{
		iByte = psD[(*piX)++];
		ptItem->unsignedValue |= (iByte << (i*8));
	}
/*
 *	Keep one value unsigned
*/
	ptItem->signedValue = ptItem->unsignedValue;
/*
 *	Sign extend one value
*/
	if ((iByte & 0x80) != 0)
	{
		while (i < sizeof(int))
			ptItem->signedValue |= (0xFF << ((i++)*8));
	}
	return kHIDSuccess;
}
