/*
 *
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


#import <Foundation/Foundation.h>
#import "DescriptorDecoder.h"
#import "BusProberSharedFunctions.h"
#import "BusProbeDevice.h"


enum  VideoClassSpecific
{
    // Video Interface Subclass Codes
    //
    SC_UNDEFINED		= 0x00,
    SC_VIDEOCONTROL		= 0x01,
    SC_VIDEOSTREAMING		= 0x02,

    // Video Interface Protocol Codes
    //
    PC_PROTOCOL_UNDEFINED	= 0x00,

    // Video Class Specific Descriptor Types
    //
    CS_UNDEFINED		= 0x20,
    CS_DEVICE			= 0x21,
    CS_CONFIGURATION		= 0x22,
    CS_STRING			= 0x23,
    // CS_INTERFACE		= 0x24,
    // CS_ENDPOINT			= 0x25,

    // Video Class Specific Control Interface Descriptor Types
    //
    VC_DESCRIPTOR_UNDEFINED	= 0x00,
    VC_HEADER			= 0x01,
    VC_INPUT_TERMINAL		= 0x02,
    VC_OUTPUT_TERMINAL		= 0x03,
    VC_SELECTOR_UNIT		= 0x04,
    VC_PROCESSING_UNIT		= 0x05,
    VC_EXTENSION_UNIT		= 0x06,

    // Video Class Specific Streaming Interface Descriptor Types
    //
    VS_UNDEFINED		= 0x00,
    VS_INPUT_HEADER		= 0x01,
    VS_OUTPUT_HEADER		= 0x02,
    VS_STILL_IMAGE_FRAME	= 0x03,
    VS_FORMAT_UNCOMPRESSED	= 0x04,
    VS_FRAME_UNCOMPRESSED	= 0x05,
    VS_FORMAT_MJPEG		= 0x06,
    VS_FRAME_MJPEG		= 0x07,
    VS_FORMAT_MPEG1		= 0x08,
    VS_FORMAT_MPEG2PS		= 0x09,
    VS_FORMAT_MPEG2TS		= 0x0a,
    VS_FORMAT_MPEG4SL		= 0x0b,
    VS_FORMAT_DV		= 0x0c,
    VS_FORMAT_VENDOR		= 0x0d,
    VS_FRAME_VENDOR		= 0x0e,

    // Video Class Specific Endpoint Descriptor Subtypes
    //
    EP_UNDEFINED		= 0x00,
    EP_GENERAL			= 0x01,
    EP_ENDPOINT			= 0x02,
    EP_INTERRUPT		= 0x03,

    // Video Class Specific Request Codes
    //
    RC_UNDEFINED		= 0x00,
    SET_CUR			= 0x01,
    GET_CUR			= 0x81,
    GET_MIN			= 0x82,
    GET_MAX			= 0x83,
    GET_RES			= 0x84,
    GET_LEN			= 0x85,
    GET_INFO			= 0x86,
    GET_DEF			= 0x87,

    // Video Control Interface Control Selectors
    //
    VC_UNDEFINED_CONTROL	= 0x00,
    VC_VIDEO_POWER_MODE_CONTROL	= 0x01,
    VC_REQUEST_ERROR_CODE_CONTROL		= 0x02,
    VC_REQUEST_INDICATE_HOST_CLOCK_CONTROL	= 0x03,

    // Terminal Control Selectors
    //
    TE_CONTROL_UNDEFINED	= 0x00,

    // Selector Unit Control Selectors
    //
    SU_CONTROL_UNDEFINED	= 0x00,
    SU_INPUT_SELECT_CONTROL	= 0x01,

    // Camera Terminal Control Selectors
    //
    CT_CONTROL_UNDEFINED		= 0x00,
    CT_SCANNING_MODE_CONTROL		= 0x01,
    CT_AE_MODE_CONTROL			= 0x02,
    CT_AE_PRIORITY_CONTROL		= 0x03,
    CT_EXPOSURE_TIME_ABSOLUTE_CONTROL	= 0x04,
    CT_EXPOSURE_TIME_RELATIVE_CONTROL	= 0x05,
    CT_FOCUS_ABSOLUTE_CONTROL		= 0x06,
    CT_FOCUS_RELATIVE_CONTROL		= 0x07,
    CT_FOCUS_AUTO_CONTROL		= 0x08,
    CT_IRIS_ABSOLUTE_CONTROL		= 0x09,
    CT_IRIS_RELATIVE_CONTROL		= 0x0A,
    CT_ZOOM_ABSOLUTE_CONTROL 		= 0x0B,
    CT_ZOOM_RELATIVE_CONTROL		= 0x0C,
    CT_PANTILT_ABSOLUTE_CONTROL		= 0x0D,
    CT_PANTILT_RELATIVE_CONTROL		= 0x0E,
    CT_ROLL_ABSOLUTE_CONTROL		= 0x0F,
    CT_ROLL_RELATIVE_CONTROL		= 0x10,

    // Processing Unit Control Selectors
    //
    PU_CONTROL_UNDEFINED		= 0x00,
    PU_BACKLIGHT_COMPENSATION_CONTROL	= 0x01,
    PU_BRIGHTNESS_CONTROL		= 0x02,
    PU_CONTRAST_CONTROL			= 0x03,
    PU_GAIN_CONTROL			= 0x04,
    PU_POWER_LINE_FREQUENCY_CONTROL	= 0x05,
    PU_HUE_CONTROL			= 0x06,
    PU_SATURATION_CONTROL		= 0x07,
    PU_SHARPNESS_CONTROL		= 0x08,
    PU_GAMMA_CONTROL			= 0x09,
    PU_WHITE_BALANCE_TEMPERATURE_CONTROL	= 0x0A,
    PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL	= 0x0B,
    PU_WHITE_BALANCE_COMPONENT_CONTROL		= 0x0C,
    PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL	= 0x0D,
    PU_DIGITAL_MULTIPLIER_CONTROL		= 0x0E,
    PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL		= 0x0F,
    PU_HUE_AUTO_CONTROL				= 0x10,

    // Extension Unit Control Selectors
    //
    XU_CONTROL_UNDEFINED		= 0x00,
    XU_ENABLE_CONTROL			= 0x01,

    // Video Streaming Interface Control Selectors
    //
    VS_CONTROL_UNDEFINED		= 0x00,
    VS_PROBE_CONTROL			= 0x01,
    VS_COMMIT_CONTROL			= 0x02,
    VS_STILL_PROBE_CONTROL		= 0x03,
    VS_STILL_COMMIT_CONTROL		= 0x04,
    VS_STILL_IMAGE_TRIGGER_CONTROL	= 0x05,
    VS_STREAM_ERROR_CODE_CONTROL	= 0x06,
    VS_GENERATE_KEY_FRAME_CONTROL	= 0x07,
    VS_UPDATE_FRAME_SEGMENT_CONTROL	= 0x08,
    VS_SYNCH_DELAY_CONTROL		= 0x09,

    // USB Terminal Types
    //
    TT_VENDOR_SPECIFIC			= 0x0100,
    TT_STREAMING			= 0x0101,

    // Input Terminal Types
    //
    ITT_VENDOR_SPECIFIC			= 0x0200,
    ITT_CAMERA				= 0x0201,
    ITT_MEDIA_TRANSPORT_UNIT		= 0x0202,

    // Output Terminal Types
    //
    OTT_VENDOR_SPECIFIC			= 0x0300,
    OTT_DISPLAY				= 0x0301,
    OTT_MEDIA_TRANSPORT_OUTPUT		= 0x0302,

    // External Terminal Types
    //
    EXTERNAL_VENDOR_SPECIFIC		= 0x0400,
    COMPOSITE_CONNECTOR			= 0x0401,
    SVIDEO_CONNECTOR			= 0x0402,
    COMPONENT_CONNECTOR			= 0x0403,

};

enum UncompressedFormatGUID
{
    UNCOMPRESSED_YUV2_HI		= 0x3259555900000010ULL,
    UNCOMPRESSED_YUV2_LO		= 0x800000aa00389b71ULL,
    UNCOMPRESSED_NV12_HI		= 0x3231564E00000010ULL,
    UNCOMPRESSED_NV12_LO		= 0x800000aa00389b71ULL,
};

// Standard Video Class Control Interface Descriptor
//
#pragma pack(1)
struct IOUSBVCInterfaceDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt16	bcdVDC;
    UInt16	wTotalLength;
    UInt32	dwClockFrequency;
    UInt8	bInCollection;		// Number of Video Streaming Interfaces in the collection
    UInt8	baInterfaceNr[1];
};
typedef struct IOUSBVCInterfaceDescriptor IOUSBVCInterfaceDescriptor;
#pragma options align=reset

// Video Control Standard Input Terminal Descriptor
//
#pragma pack(1)
struct IOUSBVCInputTerminalDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bTerminalID;
    UInt16	wTerminalType;
    UInt8	bAssocTerminal;
    UInt8	iTerminal;
};
typedef struct IOUSBVCInputTerminalDescriptor IOUSBVCInputTerminalDescriptor;
#pragma options align=reset

// Video Class Standard Output Terminal Descriptor
//
#pragma pack(1)
struct IOUSBVCOutputTerminalDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bTerminalID;
    UInt16	wTerminalType;
    UInt8	bAssocTerminal;
    UInt8	bSourceID;
    UInt8	iTerminal;
};
typedef struct IOUSBVCOutputTerminalDescriptor IOUSBVCOutputTerminalDescriptor;
#pragma options align=reset

// Video Class Camera Terminal Descriptor
//
#pragma pack(1)
struct IOUSBVCCameraTerminalDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bTerminalID;
    UInt16	wTerminalType;
    UInt8	bAssocTerminal;
    UInt8	iTerminal;
    UInt16	wObjectiveFocalLengthMin;
    UInt16	wObjectiveFocalLengthMax;
    UInt16	wOcularFocalLength;
    UInt8	bControlSize;			// Size of the bmControls field
    UInt8	bmControls[1];
};
typedef struct IOUSBVCCameraTerminalDescriptor IOUSBVCCameraTerminalDescriptor;
#pragma options align=reset

// Video Class Selector Unit Descriptor
//
#pragma pack(1)
struct IOUSBVCSelectorUnitDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bUnitID;
    UInt8	bNrInPins;
    UInt8	baSourceID[1];
};
typedef struct IOUSBVCSelectorUnitDescriptor IOUSBVCSelectorUnitDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCSelectorUnit2Descriptor
{
    UInt8	iSelector;
};
typedef struct IOUSBVCSelectorUnit2Descriptor IOUSBVCSelectorUnit2Descriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCProcessingUnitDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bUnitID;
    UInt8	bSourceID;
    UInt16	wMaxMultiplier;
    UInt8	bControlSize;
    UInt8	bmControls[1];
};
typedef struct IOUSBVCProcessingUnitDescriptor IOUSBVCProcessingUnitDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCProcessingUnit2Descriptor
{
    UInt8	iProcessing;
};
typedef struct IOUSBVCProcessingUnit2Descriptor IOUSBVCProcessingUnit2Descriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCExtensionUnitDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bUnitID;
    UInt64	guidExtensionCodeHi;
    UInt64	guidExtensionCodeLo;
    UInt8	bNumControls;
    UInt8	bNrInPins;
    UInt8	baSourceID[1];
};
typedef struct IOUSBVCExtensionUnitDescriptor IOUSBVCExtensionUnitDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCExtensionUnit2Descriptor
{
    UInt8	bControlSize;
    UInt8	bmControls[1];
};
typedef struct IOUSBVCExtensionUnit2Descriptor IOUSBVCExtensionUnit2Descriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCExtensionUnit3Descriptor
{
    UInt8	iExtension;
};
typedef struct IOUSBVCExtensionUnit3Descriptor IOUSBVCExtensionUnit3Descriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCInterruptEndpointDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt16	wMaxTransferSize;
};
typedef struct IOUSBVCInterruptEndpointDescriptor IOUSBVCInterruptEndpointDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVSInputHeaderDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bNumFormats;
    UInt16	wTotalLength;
    UInt8	bEndpointAddress;
    UInt8	bmInfo;
    UInt8	bTerminalLink;
    UInt8	bStillCaptureMethod;
    UInt8	bTriggerSupport;
    UInt8	bTriggerUsage;
    UInt8	bControlSize;
    UInt8	bmControls[1];
};
typedef struct IOUSBVSInputHeaderDescriptor IOUSBVSInputHeaderDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVSOutputHeaderDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bNumFormats;
    UInt16	wTotalLength;
    UInt8	bEndpointAddress;
    UInt8	bTerminalLink;
};
typedef struct IOUSBVSOutputHeaderDescriptor IOUSBVSOutputHeaderDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MJPEGFormatDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bFormatIndex;
    UInt8	bNumFrameDescriptors;
    UInt8	bmFlags;
    UInt8	bDefaultFrameIndex;
    UInt8	bAspectRatioX;
    UInt8	bAspectRatioY;
    UInt8	bmInterlaceFlags;
    UInt8	bCopyProtect;
};
typedef struct IOUSBVDC_MJPEGFormatDescriptor IOUSBVDC_MJPEGFormatDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MJPEGFrameDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bFrameIndex;
    UInt8	bmCapabilities;
    UInt16	wWidth;
    UInt16	wHeight;
    UInt32	dwMinBitRate;
    UInt32	dwMaxBitRate;
    UInt32	dwMaxVideoFrameBufferSize;
    UInt32	dwDefaultFrameInterval;
    UInt8	bFrameIntervalType;
    UInt32	dwMinFrameInterval;
    UInt32	dwMaxFrameInterval;
    UInt32	dwFrameIntervalStep;
};
typedef struct IOUSBVDC_MJPEGFrameDescriptor IOUSBVDC_MJPEGFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MJPEGDiscreteFrameDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bFrameIndex;
    UInt8	bmCapabilities;
    UInt16	wWidth;
    UInt16	wHeight;
    UInt32	dwMaxBitRate;
    UInt32	dwMaxVideoFrameBufferSize;
    UInt32	dwDefaultFrameInterval;
    UInt8	bFrameIntervalType;
    UInt32	dwFrameInterval[1];
};
typedef struct IOUSBVDC_MJPEGDiscreteFrameDescriptor IOUSBVDC_MJPEGDiscreteFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_UncompressedFormatDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bFormatIndex;
    UInt8	bNumFrameDescriptors;
    UInt64	guidFormatHi;
    UInt64	guidFormatLo;
    UInt8	bBitsPerPixel;
    UInt8	bDefaultFrameIndex;
    UInt8	bAspectRatioX;
    UInt8	bAspectRatioY;
    UInt8	bmInterlaceFlags;
    UInt8	bCopyProtect;
};
typedef struct IOUSBVDC_UncompressedFormatDescriptor IOUSBVDC_UncompressedFormatDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_UncompressedFrameDescriptor
{
    UInt8	bLength;
    UInt8	bDescriptorType;
    UInt8	bDescriptorSubType;
    UInt8	bFrameIndex;
    UInt8	bmCapabilities;
    UInt16	wWidth;
    UInt16	wHeight;
    UInt32	dwMinBitRate;
    UInt32	dwMaxBitRate;
    UInt32	dwMaxVideoFrameBufferSize;
    UInt32	dwDefaultFrameInterval;
    UInt8	bFrameIntervalType;
    UInt32	dwMinFrameInterval;
    UInt32	dwMaxFrameInterval;
    UInt32	dwFrameIntervalStep;
};
typedef struct IOUSBVDC_UncompressedFrameDescriptor IOUSBVDC_UncompressedFrameDescriptor;
#pragma options align=reset


@interface DecodeVideoInterfaceDescriptor : NSObject {

}

+(void)decodeBytes:(UInt8 *)descriptor forDevice:(BusProbeDevice *)thisDevice  withDeviceInterface:(IOUSBDeviceInterface **)deviceIntf;
    char MapNumberToVersion( int i );

@end
