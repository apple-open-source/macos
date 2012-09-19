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

#define kUSBAudioInterfaceDesc (0x24)
#define kUSBAudioEndPointDesc  (0x25)

// <rdar://10009579>
#define kUSBAudioDescriptorBytesPerLine     16
// String buffer size to log desciptor:
//     Each byte in descriptor requires 3 chars: 2 for nibbles + a space char
//     One byte for null terminator
#define kUSBAudioMaxDescriptorStringSize    ( ( kUSBAudioDescriptorBytesPerLine * 3 ) + 1 ) * sizeof ( char )  


#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descContents[32];
} GenericAudioDescriptor, *GenericAudioDescriptorPtr;
#pragma options align=reset

enum  AudioClassSpecific {
    ACS_HEADER 			= 0x01,
    ACS_INPUT_TERMINAL 		= 0x02,
    ACS_OUTPUT_TERMINAL	 	= 0x03,
    ACS_MIXER_UNIT 		= 0x04,
    ACS_SELECTOR_UNIT 		= 0x05,
    ACS_FEATURE_UNIT 		= 0x06,
    ACS_PROCESSING_UNIT		= 0x07,
    ACS_EXTENSION_UNIT 		= 0x08,
    ACS_UNDEFINED		= 0x20,
    ACS_DEVICE			= 0x21,
    ACS_CONFIGURATION		= 0x22,
    ACS_STRING			= 0x23,
    ACS_INTERFACE		= 0x24,
    ACS_ENDPOINT		= 0x25,
    ACS_FORMAT_TYPE		= 0x02,
    ACS_FORMAT_SPECIFIC		= 0x03,
    ACS_FORMAT_TYPE_UNDEF	= 0x00,
    ACS_FORMAT_TYPE_I		= 0x01,
    ACS_FORMAT_TYPE_II		= 0x02,
    ACS_FORMAT_TYPE_III		= 0x03,
    ACS_ASTREAM_UNDEF		= 0x00,
    ACS_ASTREAM_GENERAL		= 0x01,
    ACS_ASTREAM_TYPE		= 0x02,
    ACS_ASTREAM_SPECIFIC	= 0x03,
    AC_CONTROL_SUBCLASS		= 0x01,
    AC_STREAM_SUBCLASS		= 0x02,
    AC_MIDI_SUBCLASS		= 0x03
};

// Standard Audio Stream Isoc Audio Data Endpoint Descriptor
//    Refer to USB Audio Class Devices pp. 61-62.
#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	asAddress;
    UInt8	asAttributes;
    UInt16	asMaxPacketSize;
    UInt8	asInterval;
    UInt8	asRefresh;
    UInt8	asSynchAddress;
} AS_IsocEndPtDesc, *AS_IsocEndPtDescPtr;
#pragma options align=reset

// Class Specific Audio Stream Isoc Audio Data Endpoint Descriptor
//    Refer to USB Audio Class Devices pp. 62-63.
#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubtype;
    UInt8	asAttributes;
    UInt8	bLockDelayUnits;
    UInt16	wLockDelay;
} CSAS_IsocEndPtDesc, *CSAS_IsocEndPtDescPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt16	descVersNum;
    UInt16	descTotalLength;
    UInt8	descAICNum; /* Number of elements in the Audio Interface Collection. */
    UInt8	descInterfaceNum[1];
} AudioCtrlHdrDescriptor, *AudioCtrlHdrDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descTermID;
    UInt16	descTermType;
    UInt8	descOutTermID;
    UInt8	descNumChannels;
    UInt16	descChannelConfig;
    UInt8	descChannelNames;
    UInt8	descTermName;
} AudioCtrlInTermDescriptor, *AudioCtrlInTermDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descTermID;
    UInt16	descTermType;
    UInt8	descInTermID;
    UInt8	descSourceID;
    UInt8	descTermName;
} AudioCtrlOutTermDescriptor, *AudioCtrlOutTermDescriptorPtr;

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descUnitID;
    UInt8	descNumPins;
    UInt8	descSourcePID[1];
} AudioCtrlMixerDescriptor, *AudioCtrlMixerDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descUnitID;
    UInt8	descNumPins;
    UInt8	descSourcePID[1];
} AudioCtrlSelectorDescriptor, *AudioCtrlSelectorDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descUnitID;
    UInt8	descSourceID;
    UInt8	descCtrlSize;
    UInt8	descControls[1];
} AudioCtrlFeatureDescriptor, *AudioCtrlFeatureDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descUnitID;
    UInt16	descExtensionCode;
    UInt8	descNumPins;
    UInt8	descSourcePID[1];
} AudioCtrlExtDescriptor, *AudioCtrlExtDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	terminalID;
    UInt8	delay;
    UInt16	formatTag;
} CSAS_InterfaceDescriptor, *CSAS_InterfaceDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct acProcessingDescriptor{						//	ееее WARNING еее ADDING ELEMENTS WILL KILL CODE!!!
    UInt8			descLen;				//	size of this descriptor in bytes
    UInt8			bDescriptorType;		//	const CS_INTERFACE
    UInt8			bDescriptorSubtype;		//	const FEATURE_UNIT
    UInt8			bUnitID;
    UInt16			wProcessType;
    UInt8			bNrPins;
    UInt8			bSourceID;
}acProcessingDescriptor;
#pragma options align=reset
typedef acProcessingDescriptor *acProcessingDescriptorPtr;

#pragma pack(1)
typedef struct acProcessingDescriptorCont{
				UInt8			bNrChannels;
				UInt16			wChannelConfig;
                                UInt8			iChannelNames;
                                UInt8			bControlSize;
                                UInt16			bmControls;
                                UInt8			iProcessing;
}acProcessingDescriptorCont;
#pragma options align=reset
typedef acProcessingDescriptorCont *acProcessingDescriptorContPtr;

/* Refer to USB PDF files for Frmts10.pdf pp. 10 for Type I Format Descriptor. */
#pragma pack(1)
typedef struct {
    UInt8	byte1;
    UInt8	byte2;
    UInt8	byte3;
} CSAS_Freq3, *CSAS_Freq3Ptr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    CSAS_Freq3	lowerSamFreq;
    CSAS_Freq3	upperSamFreq;
} CSAS_ContTbl, *CSAS_ContTblPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    CSAS_Freq3	samFreq[1];
} CSAS_DiscreteTbl, *CSAS_DiscreteTblPtr;
#pragma options align=reset
#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	formatType;
    UInt8	numberOfChannels;
    UInt8	subFrameSize;
    UInt8	bitResolution;
    UInt8	sampleFreqType;
    union {
        CSAS_ContTbl		cont;
        CSAS_DiscreteTbl	discrete;
    } sampleFreqTables;
} CSAS_FormatTypeIDesc, *CSAS_FormatTypeIDescPtr;
#pragma options align=reset

enum  Audio20ClassSpecific {
    AC20S_HEADER 			= 0x01,
    AC20S_INPUT_TERMINAL	= 0x02,
    AC20S_OUTPUT_TERMINAL	= 0x03,
    AC20S_MIXER_UNIT 		= 0x04,
    AC20S_SELECTOR_UNIT		= 0x05,
    AC20S_FEATURE_UNIT 		= 0x06,
	AC20S_EFFECT_UNIT		= 0x07,
    AC20S_PROCESSING_UNIT	= 0x08,
    AC20S_EXTENSION_UNIT	= 0x09,
	AC20S_CLOCK_SOURCE		= 0x0A,
	AC20S_CLOCK_SELECTOR	= 0x0B,
	AC20S_CLOCK_MULTIPLIER	= 0x0C,
	AC20S_SAMPLE_RATE_CONVERTER = 0x0D,

    AC20S_FORMAT_TYPE_IV	= 0x04,
    AC20S_EXTENDED_FORMAT_TYPE_I	= 0x81,
    AC20S_EXTENDED_FORMAT_TYPE_II	= 0x82,
    AC20S_EXTENDED_FORMAT_TYPE_III	= 0x83
	
};

// Standard Audio Stream Isoc Audio Data Endpoint Descriptor
//    Refer to USB Audio Class Devices pp. 61-62.
#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	asAddress;
    UInt8	asAttributes;
    UInt16	asMaxPacketSize;
    UInt8	asInterval;
} AS20_IsocEndPtDesc, *AS20_IsocEndPtDescPtr;
#pragma options align=reset

// Class Specific Audio Stream Isoc Audio Data Endpoint Descriptor
//    Refer to USB Audio Class Devices pp. 62-63.
#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubtype;
    UInt8	asAttributes;
	UInt8	bmControls;
    UInt8	bLockDelayUnits;
    UInt16	wLockDelay;
} CSA20S_IsocEndPtDesc, *CSAS20_IsocEndPtDescPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt16	descVersNum;
	UInt8	descCategory;
    UInt16	descTotalLength;
    UInt8	descbmControls;
} Audio20CtrlHdrDescriptor, *Audio20CtrlHdrDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descTermID;
    UInt16	descTermType;
    UInt8	descOutTermID;
	UInt8	descClockSourceID;
    UInt8	descNumChannels;
    UInt32	descChannelConfig;
    UInt8	descChannelNames;
	UInt16	descbmControls;
    UInt8	descTermName;
} Audio20CtrlInTermDescriptor, *Audio20CtrlInTermDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descClockID;
    UInt8	descAttributes;
    UInt8	descbmControls;
	UInt8	descAssocTermID;
    UInt8	desciClockSourceName;
} Audio20ClockSourceDescriptor, *Audio20ClockSourceDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descClockID;
    UInt8	descNumPins;
    UInt8	descClockPID[1];
} Audio20ClockSelectorDescriptor, *Audio20ClockSelectorDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descClockID;
    UInt8	descClockSourceID;
    UInt8	descbmControls;
    UInt8	desciClockMultiplierName;
} Audio20ClockMultiplierDescriptor, *Audio20ClockMultiplierDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descTermID;
    UInt16	descTermType;
    UInt8	descInTermID;
    UInt8	descSourceID;
    UInt8	descClockSourceID;
	UInt16	bmControls;
    UInt8	descTermName;
} Audio20CtrlOutTermDescriptor, *Audio20CtrlOutTermDescriptorPtr;

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descUnitID;
    UInt8	descSourceID;
    UInt8	descControls[1];
} Audio20CtrlFeatureDescriptor, *Audio20CtrlFeatureDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	descUnitID;
    UInt16	descExtensionCode;
    UInt8	descNumPins;
    UInt8	descSourcePID[1];
} Audio20CtrlExtDescriptor, *Audio20CtrlExtDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	terminalID;
	UInt8	bmControls;
	UInt8	formatType;
	UInt32	bmFormats;
	UInt8	numChannels;
	UInt32	channelConfig;
    UInt8	channelNames;
} CS20AS_InterfaceDescriptor, *CS20AS_InterfaceDescriptorPtr;
#pragma options align=reset

#pragma pack(1)
typedef struct ac20ProcessingDescriptor{						//	ееее WARNING еее ADDING ELEMENTS WILL KILL CODE!!!
    UInt8			descLen;				//	size of this descriptor in bytes
    UInt8			bDescriptorType;		//	const CS_INTERFACE
    UInt8			bDescriptorSubtype;		//	const FEATURE_UNIT
    UInt8			bUnitID;
    UInt16			wProcessType;
    UInt8			bNrPins;
    UInt8			bSourceID[1];
}ac20ProcessingDescriptor, *ac20ProcessingDescriptorPtr;
#pragma options align=reset
typedef ac20ProcessingDescriptor *a20cProcessingDescriptorPtr;

#pragma pack(1)
typedef struct ac20ProcessingDescriptorCont{
				UInt8			bNrChannels;
				UInt32			wChannelConfig;
                                UInt8			iChannelNames;
                                UInt16			bmControls;
                                UInt8			iProcessing;
}ac20ProcessingDescriptorCont;
#pragma options align=reset
typedef ac20ProcessingDescriptorCont *ac20ProcessingDescriptorContPtr;

/* Refer to USB PDF files for Frmts10.pdf pp. 10 for Type I Format Descriptor. */
#pragma pack(1)
typedef struct {
    UInt8	descLen;
    UInt8	descType;
    UInt8	descSubType;
    UInt8	formatType;
	UInt8	slotSize;
    UInt8	bitResolution;
} CS20AS_FormatTypeIDesc, *CS20AS_FormatTypeIDescPtr;
#pragma options align=reset

@interface DecodeAudioInterfaceDescriptor : NSObject {

}

+(void)decodeBytes:(UInt8 *)descriptor forDevice:(BusProbeDevice *)thisDevice;
void decodeBytes10( UInt8 *descriptor, BusProbeDevice * thisDevice );
void decodeBytes20( UInt8 *descriptor, BusProbeDevice * thisDevice );

@end
