/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import "DescriptorDecoder.h"
#import "BusProberSharedFunctions.h"
#import "BusProbeDevice.h"

#define	UnpackReportSize(packedByte)	((packedByte) & 0x03)
#define	UnpackReportType(packedByte)	(((packedByte) & 0x0C) >> 2)
#define	UnpackReportTag(packedByte)	(((packedByte) & 0xF0) >> 4)

enum
{
    kReport_TypeMain			= 0,
    kReport_TypeGlobal			= 1,
    kReport_TypeLocal			= 2,
    kReport_TypeReserved		= 3,
    
    kReport_TagLongItem			= 0x0F,
    
    // main items
    kReport_TagInput			= 0x08,
    kReport_TagOutput			= 0x09,
    kReport_TagFeature			= 0x0B,
    kReport_TagCollection		= 0x0A,
    kReport_TagEndCollection	= 0x0C,
    
    // global items
    kReport_TagUsagePage		= 0x00,
    kReport_TagLogicalMin		= 0x01,
    kReport_TagLogicalMax		= 0x02,
    kReport_TagPhysicalMin		= 0x03,
    kReport_TagPhysicalMax		= 0x04,
    kReport_TagUnitExponent		= 0x05,
    kReport_TagUnit				= 0x06,
    kReport_TagReportSize		= 0x07,
    kReport_TagReportID			= 0x08,
    kReport_TagReportCount		= 0x09,
    kReport_TagPush				= 0x0A,
    kReport_TagPop				= 0x0B,
    
    // local items
    kReport_TagUsage			= 0x00,
    kReport_TagUsageMin			= 0x01,
    kReport_TagUsageMax			= 0x02,
    kReport_TagDesignatorIndex	= 0x03,
    kReport_TagDesignatorMin	= 0x04,
    kReport_TagDesignatorMax	= 0x05,
    kReport_TagStringIndex		= 0x07,
    kReport_TagStringMin		= 0x08,
    kReport_TagStringMax		= 0x09,
    kReport_TagSetDelimiter		= 0x0A
};

// Collection constants
enum
{
    kCollection_Physical		= 0x00,
    kCollection_Application		= 0x01,
    kCollection_Logical			= 0x02
};

// I/O constants (used for Input/Output/Feature tags)
enum
{
    kIO_Data_or_Constant				= 0x0001,
    kIO_Array_or_Variable				= 0x0002,
    kIO_Absolute_or_Relative			= 0x0004,
    kIO_NoWrap_or_Wrap					= 0x0008,
    kIO_Linear_or_NonLinear				= 0x0010,
    kIO_PreferredState_or_NoPreferred	= 0x0020,
    kIO_NoNullPosition_or_NullState		= 0x0040,
    kIO_NonVolatile_or_Volatile			= 0x0080,		// reserved for Input
    kIO_BitField_or_BufferedBytes		= 0x0100
};

// Usage pages from HID Usage Tables spec 1.0
enum
{
    kUsage_PageGenericDesktop			= 0x01,
    kUsage_PageSimulationControls		= 0x02,
    kUsage_PageVRControls				= 0x03,
    kUsage_PageSportControls			= 0x04,
    kUsage_PageGameControls				= 0x05,
    kUsage_PageKeyboard					= 0x07,
    kUsage_PageLED						= 0x08,
    kUsage_PageButton					= 0x09,
    kUsage_PageOrdinal					= 0x0A,
    kUsage_PageTelephonyDevice			= 0x0B,
    kUsage_PageConsumer					= 0x0C,
    kUsage_PageDigitizers				= 0x0D,
    kUsage_PagePID				= 0x0F,
    kUsage_PageUnicode					= 0x10,
    kUsage_PageAlphanumericDisplay		= 0x14,
    kUsage_PageMonitor					= 0x80,
    kUsage_PageMonitorEnumeratedValues	= 0x81,
    kUsage_PageMonitorVirtualControl 	= 0x82,
    kUsage_PageMonitorReserved			= 0x83,
    kUsage_PagePowerDevice				= 0x84,
    kUsage_PageBatterySystem			= 0x85,
    kUsage_PowerClassReserved			= 0x86,
    kUsage_PowerClassReserved2			= 0x87,
	kUsage_VendorDefinedStart			= 0xff00
};

// Usage constants for Generic Desktop page (01) from HID Usage Tables spec 1.0
enum
{
    kUsage_01_Pointer		= 0x01,
    kUsage_01_Mouse			= 0x02,
    kUsage_01_Joystick		= 0x04,
    kUsage_01_GamePad		= 0x05,
    kUsage_01_Keyboard		= 0x06,
    kUsage_01_Keypad		= 0x07,
    
    kUsage_01_X				= 0x30,
    kUsage_01_Y				= 0x31,
    kUsage_01_Z				= 0x32,
    kUsage_01_Rx			= 0x33,
    kUsage_01_Ry			= 0x34,
    kUsage_01_Rz			= 0x35,
    kUsage_01_Slider		= 0x36,
    kUsage_01_Dial			= 0x37,
    kUsage_01_Wheel			= 0x38,
    kUsage_01_HatSwitch		= 0x39,
    kUsage_01_CountedBuffer	= 0x3A,
    kUsage_01_ByteCount		= 0x3B,
    kUsage_01_MotionWakeup	= 0x3C,
    
    kUsage_01_Vx			= 0x40,
    kUsage_01_Vy			= 0x41,
    kUsage_01_Vz			= 0x42,
    kUsage_01_Vbrx			= 0x43,
    kUsage_01_Vbry			= 0x44,
    kUsage_01_Vbrz			= 0x45,
    kUsage_01_Vno			= 0x46,
    
    kUsage_01_SystemControl		= 0x80,
    kUsage_01_SystemPowerDown 	= 0x81,
    kUsage_01_SystemSleep 		= 0x82,
    kUsage_01_SystemWakeup		= 0x83,
    kUsage_01_SystemContextMenu = 0x84,
    kUsage_01_SystemMainMenu	= 0x85,
    kUsage_01_SystemAppMenu		= 0x86,
    kUsage_01_SystemMenuHelp	= 0x87,
    kUsage_01_SystemMenuExit	= 0x88,
    kUsage_01_SystemMenuSelect	= 0x89,
    kUsage_01_SystemMenuRight	= 0x8A,
    kUsage_01_SystemMenuLeft	= 0x8B,
    kUsage_01_SystemMenuUp		= 0x8C,
    kUsage_01_SystemMenuDown	= 0x8D
};

/*!
 @typedef IOUSBCCIDDescriptor
 @discussion USB Device CHIP CARD ID Descriptor.  See the USB CCID Specification at <a href="http://www.usb.org"TARGET="_blank">http://www.usb.org</a>.
 */

#pragma pack(1)
struct IOUSBCCIDDescriptor 
{
	UInt8 			bLength;
	UInt8 			bDescriptorType;
	UInt16			bcdCCID;
	UInt8			bMaxSlotIndex;
	UInt8			bVoltageSupport;
	UInt32			dwProtocols;
	UInt32			dwDefaultClock;
	UInt32			dwMaximumClock;
	UInt8			bNumClockSupported;
	UInt32			dwDataRate;
	UInt32			dwMaxDataRate;
	UInt8			bNumDataRatesSupported;
	UInt32			dwMaxIFSD;
	UInt32			dwSyncProtocols;
	UInt32			dwMechanical;
	UInt32			dwFeatures;
	UInt32			dwMaxCCIDMessageLength;
	UInt8			bClassGetResponse;
	UInt8			bClassEnvelope;
	UInt16			wLcdLayout;
	UInt8			bPINSupport;
	UInt8			bMaxCCIDBusySlots;
};
typedef struct 	IOUSBCCIDDescriptor 		IOUSBCCIDDescriptor;
typedef 		IOUSBCCIDDescriptor *	IOUSBCCIDDescriptorPtr;

#pragma options align=reset




/*	end HID Constants Spec 1.0 	*/

@interface DecodeHIDDescriptor : NSObject {

}

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice withDeviceInterface:(IOUSBDeviceRef)deviceIntf  isinCurrentConfig:(Boolean)inCurrentConfig;

+(void)decodeHIDReport:(UInt8 *)reportDesc forDevice:(BusProbeDevice *)thisDevice atDepth:(UInt16)depth reportLen:(UInt16)length;

@end
