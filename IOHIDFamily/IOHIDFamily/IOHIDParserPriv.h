/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2012 Apple Computer, Inc.  All Rights Reserved.
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
#ifndef _IOKIT_HID_IOHIDPARSERPRIV_H
#define _IOKIT_HID_IOHIDPARSERPRIV_H

/* the following constants are from the USB HID Specification (www.usb.org)*/

/*------------------------------------------------------------------------------*/
/*																				*/
/* HID Main Item Header Bit Definitions											*/
/*																				*/
/*------------------------------------------------------------------------------*/
enum {
	kHIDDataBufferedBytes		= 0x0100,
	kHIDDataVolatileBit			= 0x80,
	kHIDDataVolatile			= 0x80,
	kHIDDataNullStateBit		= 0x40,
	kHIDDataNullState			= 0x40,
	kHIDDataNoPreferredBit		= 0x20,
	kHIDDataNoPreferred			= 0x20,
	kHIDDataNonlinearBit		= 0x10,
	kHIDDataNonlinear			= 0x10,
	kHIDDataWrapBit				= 0x08,
	kHIDDataWrap				= 0x08,
	kHIDDataRelativeBit			= 0x04,
	kHIDDataRelative			= 0x04,
	kHIDDataAbsolute			= 0x00,
	kHIDDataVariableBit			= 0x02,
	kHIDDataVariable			= 0x02,
	kHIDDataArrayBit			= 0x02,
	kHIDDataArray				= 0x00,
	kHIDDataConstantBit			= 0x01,
	kHIDDataConstant			= 0x01
};

#if KERNEL
/*------------------------------------------------------------------------------*/
/*																				*/
/* HID Header																	*/
/*																				*/
/* ---------------------------------------------------------					*/
/* |  7	  |	 6	 |	5	|  4   |  3	  |	 2	 |	1	|  0   |					*/
/* |			Tag			   |	Type	 |	  Size	   |					*/
/* ---------------------------------------------------------					*/
/*------------------------------------------------------------------------------*/
enum {
	kHIDItemSizeMask			= 0x03,
	kHIDItemTagMask				= 0xF0,
	kHIDItemTagShift			= 4,
	kHIDItemTypeMask			= 0x0C,
	kHIDItemTypeShift			= 2,
	kHIDLongItemHeader			= 0xFE
};

/*------------------------------------------------------------------------------*/
/*																				*/
/* HID Item Type Definitions													*/
/*																				*/
/*------------------------------------------------------------------------------*/
enum {
	kHIDTypeMain				= 0,
	kHIDTypeGlobal				= 1,
	kHIDTypeLocal				= 2,
	kHIDTypeLong				= 3
};

/*------------------------------------------------------------------------------*/
/*																				*/
/* HID Item Tag Definitions - Main Items										*/
/*																				*/
/*------------------------------------------------------------------------------*/
enum {
	kHIDTagInput				= 8,
	kHIDTagOutput				= 9,
	kHIDTagCollection			= 0x0A,
	kHIDTagFeature				= 0x0B,
	kHIDTagEndCollection		= 0x0C
};

/*------------------------------------------------------------------------------*/
/*																				*/
/* HID Item Tag Definitions - Globals											*/
/*																				*/
/*------------------------------------------------------------------------------*/
enum {
	kHIDTagUsagePage			= 0,
	kHIDTagLogicalMinimum		= 1,
	kHIDTagLogicalMaximum		= 2,
	kHIDTagPhysicalMinimum		= 3,
	kHIDTagPhysicalMaximum		= 4,
	kHIDTagUnitExponent			= 5,
	kHIDTagUnit					= 6,
	kHIDTagReportSize			= 7,
	kHIDTagReportID				= 8,
	kHIDTagReportCount			= 9,
	kHIDTagPush					= 0x0A,
	kHIDTagPop					= 0x0B
};

/*------------------------------------------------------------------------------*/
/*																				*/
/* HID Item Tag Definitions - Locals											*/
/*																				*/
/*------------------------------------------------------------------------------*/
enum {
	kHIDTagUsage				= 0,
	kHIDTagUsageMinimum			= 1,
	kHIDTagUsageMaximum			= 2,
	kHIDTagDesignatorIndex		= 3,
	kHIDTagDesignatorMinimum	= 4,
	kHIDTagDesignatorMaximum	= 5,
	kHIDTagStringIndex			= 7,
	kHIDTagStringMinimum		= 8,
	kHIDTagStringMaximum		= 9,
	kHIDTagSetDelimiter			= 0x0A
};

/*------------------------------------------------------------------------------*/
/*																				*/
/* HID Collection Data Definitions												*/
/*																				*/
/*------------------------------------------------------------------------------*/
enum {
	kHIDPhysicalCollection		= 0x00,
	kHIDApplicationCollection	= 0x01
};

/*------------------------------------------------------------------------------*/
/*																				*/
/* HIDLibrary private defs														*/
/*																				*/
/*------------------------------------------------------------------------------*/

enum
{	
	kHIDOSType			=	'hid '
};

struct HIDItem
{
	IOByteCount		byteCount;
	SInt32			itemType;
	SInt32			tag;
	SInt32			signedValue;
	UInt32			unsignedValue;
};
typedef struct HIDItem HIDItem;

struct HIDGlobalItems
{
	HIDUsage		usagePage;
	SInt32			logicalMinimum;
	SInt32			logicalMaximum;
	SInt32			physicalMinimum;
	SInt32			physicalMaximum;
	SInt32			unitExponent;
	SInt32			units;
	IOByteCount		reportSize;
	SInt32			reportID;
	SInt32			reportCount;
	SInt32			reportIndex;
};
typedef struct HIDGlobalItems HIDGlobalItems;

struct HIDReportSizes
{
	SInt32		reportID;
	SInt32		inputBitCount;
	SInt32		outputBitCount;
	SInt32		featureBitCount;
};
typedef struct HIDReportSizes HIDReportSizes;

struct HIDCollection
{
	SInt32		data;
	SInt32		usagePage;
	SInt32		firstUsageItem;
	SInt32		usageItemCount;
	SInt32		firstReportItem;
	SInt32		reportItemCount;
	SInt32		parent;
	SInt32		children;
	SInt32		firstChild;
	SInt32		nextSibling;
};
typedef struct HIDCollection HIDCollection;

struct HIDCollectionExtendedNode
{
	HIDUsage	collectionUsage;
	HIDUsage	collectionUsagePage;
	UInt32		parent;
	UInt32		numberOfChildren;
	UInt32		nextSibling;
	UInt32		firstChild;
        UInt32		data;
};
typedef struct HIDCollectionExtendedNode HIDCollectionExtendedNode, * HIDCollectionExtendedNodePtr;

enum
{	
	kHIDReportItemFlag_Reversed			=	0x00000001
};

struct HIDReportItem
{
	UInt32				reportType;
	HIDGlobalItems		globals;
	SInt32				startBit;
	SInt32				parent;
	SInt32				dataModes;
	SInt32				firstUsageItem;
	SInt32				usageItemCount;
	SInt32				firstStringItem;
	SInt32				stringItemCount;
	SInt32				firstDesigItem;
	SInt32				desigItemCount;
	UInt32				flags;
};
typedef struct HIDReportItem HIDReportItem;

struct HIDP_UsageItem
{
	Boolean		isRange;
	Boolean		reserved;
	HIDUsage	usagePage;
	HIDUsage	usage;
	SInt32		usageMinimum;
	SInt32		usageMaximum;
};
typedef struct HIDP_UsageItem HIDP_UsageItem;

struct HIDStringItem
{
	Boolean		isRange;
	Boolean		reserved;
	SInt32		index;
	SInt32		minimum;
	SInt32		maximum;
};
typedef struct HIDStringItem HIDStringItem;
typedef HIDStringItem HIDDesignatorItem;

struct HIDPreparsedData
{
	UInt32				hidTypeIfValid;
	HIDCollection 		*collections;
	UInt32				collectionCount;
	HIDReportItem 		*reportItems;
	UInt32				reportItemCount;
	HIDReportSizes 		*reports;
	UInt32				reportCount;
	HIDP_UsageItem 		*usageItems;
	UInt32				usageItemCount;
	HIDStringItem 		*stringItems;
	UInt32				stringItemCount;
	HIDDesignatorItem 	*desigItems;
	UInt32				desigItemCount;
	UInt8 *				rawMemPtr;
	UInt32				flags;
	IOByteCount			numBytesAllocated;
};
typedef struct HIDPreparsedData HIDPreparsedData;
typedef HIDPreparsedData * HIDPreparsedDataPtr;

#ifdef __cplusplus
extern "C" {
#endif

// Private methods
extern 
OSStatus
HIDGetCollectionExtendedNodes ( HIDCollectionExtendedNodePtr	collectionNodes,
                                UInt32 *			collectionNodesSize,
                                HIDPreparsedDataRef		preparsedDataRef );
#ifdef __cplusplus
}
#endif


#endif

#endif /* !_IOKIT_HID_IOHIDPARSERPRIV_H */
