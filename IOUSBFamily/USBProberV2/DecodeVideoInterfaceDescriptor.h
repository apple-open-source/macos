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
#import "DescriptorDecoder.h"
#import "BusProberSharedFunctions.h"
#import "BusProbeDevice.h"


enum  VideoClassSpecific
{
    // Video Interface Class Code
    CC_VIDEO                    = 0x0E,

    // Video Interface Subclass Codes
    //
    SC_UNDEFINED		= 0x00,
    SC_VIDEOCONTROL		= 0x01,
    SC_VIDEOSTREAMING		= 0x02,
    SC_VIDEO_INTERFACE_COLLECTION               = 0x03,

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
    // CS_ENDPOINT              = 0x25,

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
    VS_FORMAT_MPEG4SL		= 0xFF, // TBD -- will not be 0x0b
    VS_FORMAT_DV		= 0x0c,
    VS_COLORFORMAT              = 0x0d,
    VS_FORMAT_VENDOR		= 0x0e,
    VS_FRAME_VENDOR		= 0x0f,

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
    VC_CONTROL_UNDEFINED                        = 0x00,
    VC_VIDEO_POWER_MODE_CONTROL                 = 0x01,
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
    CT_PRIVACY_CONTROL                  = 0x11,

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
    
    // Media Transport Terminal Control Selectors
    //
    MTT_CONTROL_UNDEFINED               = 0x00,
    TRANSPORT_CONTROL                   = 0X01,
    ATN_INFORMATION_CONTROL             = 0X02,
    MEDIA_INFORMATION_CONTROL           = 0X03,
    TIME_CODE_INFORMATION_CONTROL       = 0X04,
    

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
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint16_t	bcdVDC;
    uint16_t	wTotalLength;
    uint32_t	dwClockFrequency;
    uint8_t		bInCollection;		// Number of Video Streaming Interfaces in the collection
    uint8_t		baInterfaceNr[1];
};
typedef struct IOUSBVCInterfaceDescriptor IOUSBVCInterfaceDescriptor;
#pragma options align=reset

// Video Control Standard Input Terminal Descriptor
//
#pragma pack(1)
struct IOUSBVCInputTerminalDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bTerminalID;
    uint16_t	wTerminalType;
    uint8_t		bAssocTerminal;
    uint8_t		iTerminal;
};
typedef struct IOUSBVCInputTerminalDescriptor IOUSBVCInputTerminalDescriptor;
#pragma options align=reset

// Video Class Standard Output Terminal Descriptor
//
#pragma pack(1)
struct IOUSBVCOutputTerminalDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bTerminalID;
    uint16_t	wTerminalType;
    uint8_t		bAssocTerminal;
    uint8_t		bSourceID;
    uint8_t		iTerminal;
};
typedef struct IOUSBVCOutputTerminalDescriptor IOUSBVCOutputTerminalDescriptor;
#pragma options align=reset

// Video Class Camera Terminal Descriptor
//
#pragma pack(1)
struct IOUSBVCCameraTerminalDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bTerminalID;
    uint16_t	wTerminalType;
    uint8_t		bAssocTerminal;
    uint8_t		iTerminal;
    uint16_t	wObjectiveFocalLengthMin;
    uint16_t	wObjectiveFocalLengthMax;
    uint16_t	wOcularFocalLength;
    uint8_t		bControlSize;			// Size of the bmControls field
    uint8_t		bmControls[1];
};
typedef struct IOUSBVCCameraTerminalDescriptor IOUSBVCCameraTerminalDescriptor;
#pragma options align=reset

// Video Class Selector Unit Descriptor
//
#pragma pack(1)
struct IOUSBVCSelectorUnitDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bUnitID;
    uint8_t		bNrInPins;
    uint8_t		baSourceID[1];
};
typedef struct IOUSBVCSelectorUnitDescriptor IOUSBVCSelectorUnitDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCSelectorUnit2Descriptor
{
    uint8_t	iSelector;
};
typedef struct IOUSBVCSelectorUnit2Descriptor IOUSBVCSelectorUnit2Descriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCProcessingUnitDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bUnitID;
    uint8_t		bSourceID;
    uint16_t	wMaxMultiplier;
    uint8_t		bControlSize;
    uint8_t		bmControls[1];
};
typedef struct IOUSBVCProcessingUnitDescriptor IOUSBVCProcessingUnitDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCProcessingUnit2Descriptor
{
    uint8_t		iProcessing;
};
typedef struct IOUSBVCProcessingUnit2Descriptor IOUSBVCProcessingUnit2Descriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCExtensionUnitDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bUnitID;
	uint8_t		guidFormat[16];
    uint8_t		bNumControls;
    uint8_t		bNrInPins;
    uint8_t		baSourceID[1];
};
typedef struct IOUSBVCExtensionUnitDescriptor IOUSBVCExtensionUnitDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCExtensionUnit2Descriptor
{
    uint8_t		bControlSize;
    uint8_t		bmControls[1];
};
typedef struct IOUSBVCExtensionUnit2Descriptor IOUSBVCExtensionUnit2Descriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCExtensionUnit3Descriptor
{
    uint8_t		iExtension;
};
typedef struct IOUSBVCExtensionUnit3Descriptor IOUSBVCExtensionUnit3Descriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVCInterruptEndpointDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint16_t	wMaxTransferSize;
};
typedef struct IOUSBVCInterruptEndpointDescriptor IOUSBVCInterruptEndpointDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVSInputHeaderDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bNumFormats;
    uint16_t	wTotalLength;
    uint8_t		bEndpointAddress;
    uint8_t		bmInfo;
    uint8_t		bTerminalLink;
    uint8_t		bStillCaptureMethod;
    uint8_t		bTriggerSupport;
    uint8_t		bTriggerUsage;
    uint8_t		bControlSize;
    uint8_t		bmControls[1];
};
typedef struct IOUSBVSInputHeaderDescriptor IOUSBVSInputHeaderDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVSOutputHeaderDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bNumFormats;
    uint16_t	wTotalLength;
    uint8_t		bEndpointAddress;
    uint8_t		bTerminalLink;
};
typedef struct IOUSBVSOutputHeaderDescriptor IOUSBVSOutputHeaderDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MJPEGFormatDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFormatIndex;
    uint8_t		bNumFrameDescriptors;
    uint8_t		bmFlags;
    uint8_t		bDefaultFrameIndex;
    uint8_t		bAspectRatioX;
    uint8_t		bAspectRatioY;
    uint8_t		bmInterlaceFlags;
    uint8_t		bCopyProtect;
};
typedef struct IOUSBVDC_MJPEGFormatDescriptor IOUSBVDC_MJPEGFormatDescriptor;
#pragma options align=reset

struct IOSUBVDC_StillImageSize
{
	uint16_t	wWidth;
	uint16_t	wHeight;
};
typedef struct IOSUBVDC_StillImageSize IOSUBVDC_StillImageSize;

struct IOSUBVDC_StillImageCompressionPattern
{
	uint8_t		bNumCompressionPattern;
	uint8_t		bCompression[1];
};
typedef struct IOSUBVDC_StillImageCompressionPattern IOSUBVDC_StillImageCompressionPattern;

#pragma pack(1)
struct IOUSBVDC_StillImageFrameDescriptor
{
    uint8_t					bLength;
    uint8_t					bDescriptorType;
    uint8_t					bDescriptorSubType;
    uint8_t					bEndpointAddress;
    uint8_t					bNumImageSizePatterns;
	IOSUBVDC_StillImageSize	dwSize[1];
};
typedef struct IOUSBVDC_StillImageFrameDescriptor IOUSBVDC_StillImageFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MJPEGFrameDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFrameIndex;
    uint8_t		bmCapabilities;
    uint16_t	wWidth;
    uint16_t	wHeight;
    uint32_t	dwMinBitRate;
    uint32_t	dwMaxBitRate;
    uint32_t	dwMaxVideoFrameBufferSize;
    uint32_t	dwDefaultFrameInterval;
    uint8_t		bFrameIntervalType;
    uint32_t	dwMinFrameInterval;
    uint32_t	dwMaxFrameInterval;
    uint32_t	dwFrameIntervalStep;
};
typedef struct IOUSBVDC_MJPEGFrameDescriptor IOUSBVDC_MJPEGFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MJPEGDiscreteFrameDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFrameIndex;
    uint8_t		bmCapabilities;
    uint16_t	wWidth;
    uint16_t	wHeight;
    uint32_t	dwMinBitRate;
    uint32_t	dwMaxBitRate;
    uint32_t	dwMaxVideoFrameBufferSize;
    uint32_t	dwDefaultFrameInterval;
    uint8_t		bFrameIntervalType;
    uint32_t	dwFrameInterval[1];
};
typedef struct IOUSBVDC_MJPEGDiscreteFrameDescriptor IOUSBVDC_MJPEGDiscreteFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_UncompressedFormatDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFormatIndex;
    uint8_t		bNumFrameDescriptors;
    uint8_t		guidFormat[16];
    uint8_t		bBitsPerPixel;
    uint8_t		bDefaultFrameIndex;
    uint8_t		bAspectRatioX;
    uint8_t		bAspectRatioY;
    uint8_t		bmInterlaceFlags;
    uint8_t		bCopyProtect;
};
typedef struct IOUSBVDC_UncompressedFormatDescriptor IOUSBVDC_UncompressedFormatDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_UncompressedFrameDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFrameIndex;
    uint8_t		bmCapabilities;
    uint16_t	wWidth;
    uint16_t	wHeight;
    uint32_t	dwMinBitRate;
    uint32_t	dwMaxBitRate;
    uint32_t	dwMaxVideoFrameBufferSize;
    uint32_t	dwDefaultFrameInterval;
    uint8_t		bFrameIntervalType;
    uint32_t	dwMinFrameInterval;
    uint32_t	dwMaxFrameInterval;
    uint32_t	dwFrameIntervalStep;
};
typedef struct IOUSBVDC_UncompressedFrameDescriptor IOUSBVDC_UncompressedFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_UncompressedDiscreteFrameDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFrameIndex;
    uint8_t		bmCapabilities;
    uint16_t	wWidth;
    uint16_t	wHeight;
    uint32_t	dwMinBitRate;
    uint32_t	dwMaxBitRate;
    uint32_t	dwMaxVideoFrameBufferSize;
    uint32_t	dwDefaultFrameInterval;
    uint8_t		bFrameIntervalType;
    uint32_t	dwFrameInterval[1];
};
typedef struct IOUSBVDC_UncompressedDiscreteFrameDescriptor IOUSBVDC_UncompressedDiscreteFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_VendorFormatDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFormatIndex;
    uint8_t		bNumFrameDescriptors;
    uint8_t		guidMajorFormat[16];
    uint8_t		guidSubFormat[16];
    uint8_t		guidSpecifierFormat[16];
    uint8_t		bPayloadClass;
    uint8_t		bDefaultFrameIndex;
    uint8_t		bCopyProtect;
};
typedef struct IOUSBVDC_VendorFormatDescriptor IOUSBVDC_VendorFormatDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_VendorFrameDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFrameIndex;
    uint8_t		bmCapabilities;
    uint16_t	wWidth;
    uint16_t	wHeight;
    uint32_t	dwMinBitRate;
    uint32_t	dwMaxBitRate;
    uint32_t	dwMaxVideoFrameBufferSize;
    uint32_t	dwDefaultFrameInterval;
    uint8_t		bFrameIntervalType;
    uint32_t	dwMinFrameInterval;
    uint32_t	dwMaxFrameInterval;
    uint32_t	dwFrameIntervalStep;
};
typedef struct IOUSBVDC_VendorFrameDescriptor IOUSBVDC_VendorFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_VendorDiscreteFrameDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFrameIndex;
    uint8_t		bmCapabilities;
    uint16_t	wWidth;
    uint16_t	wHeight;
    uint32_t	dwMinBitRate;
    uint32_t	dwMaxBitRate;
    uint32_t	dwMaxVideoFrameBufferSize;
    uint32_t	dwDefaultFrameInterval;
    uint8_t		bFrameIntervalType;
    uint32_t	dwFrameInterval[1];
};
typedef struct IOUSBVDC_VendorDiscreteFrameDescriptor IOUSBVDC_VendorDiscreteFrameDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_DVFormatDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFormatIndex;
    uint32_t	dwMaxVideoFrameBufferSize;
    uint8_t		bFormatType;
};
typedef struct IOUSBVDC_DVFormatDescriptor IOUSBVDC_DVFormatDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MPEG1SSFormatDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFormatIndex;
    uint16_t	wPacketLength;
    uint16_t	wPackLength;
    uint8_t		bPackDataType;
};
typedef struct IOUSBVDC_MPEG1SSFormatDescriptor IOUSBVDC_MPEG1SSFormatDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MPEG2PSFormatDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFormatIndex;
    uint16_t	wPacketLength;
    uint16_t	wPackLength;
    uint8_t		bPackDataType;
};
typedef struct IOUSBVDC_MPEG2PSFormatDescriptor IOUSBVDC_MPEG2PSFormatDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_MPEG2PTSFormatDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bFormatIndex;
    uint8_t		bDataOffset;
    uint8_t		bPacketLength;
    uint8_t		bStrideLength;
};
typedef struct IOUSBVDC_MPEG2PTSFormatDescriptor IOUSBVDC_MPEG2PTSFormatDescriptor;
#pragma options align=reset

#pragma pack(1)
struct IOUSBVDC_ColorFormatDescriptor
{
    uint8_t		bLength;
    uint8_t		bDescriptorType;
    uint8_t		bDescriptorSubType;
    uint8_t		bColorPrimaries;
    uint8_t       bTransferCharacteristics;
    uint8_t		bMatrixCoefficients;
};
typedef struct IOUSBVDC_ColorFormatDescriptor IOUSBVDC_ColorFormatDescriptor;
#pragma options align=reset


@interface DecodeVideoInterfaceDescriptor : NSObject {

}

+(void)decodeBytes:(uint8_t *)descriptor forDevice:(BusProbeDevice *)thisDevice  withDeviceInterface:(IOUSBDeviceRef)deviceIntf;
    char MapNumberToVersion( int i );

@end
