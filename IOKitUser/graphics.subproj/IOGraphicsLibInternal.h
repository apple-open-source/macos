/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

enum {
    kDisplayAppleVendorID	= 0x610
};

enum {
    // disable any use of scaled modes,
    kOvrFlagDisableScaling	= 0x00000001,
    // remove driver modes,
    kOvrFlagDisableNonScaled	= 0x00000002,
    // disable scaled modes made up by the system (just use the override list)
    kOvrFlagDisableGenerated	= 0x00000004
};

enum {
    // skips all the various checks and always installs
    kScaleInstallAlways		= 0x00000001,
    // disables the install of a stretched version if the aspect is different
    kScaleInstallNoStretch	= 0x00000002
};

enum {
    kIOMirrorNoTrim		= 0x00000010
};

enum {
    kAppleNTSCManufacturerFlag	= 0x40,
    kApplePALManufacturerFlag	= 0x20
};

enum {
    kIOMirrorHint = 0x10000
};

enum {
    kIODisplayOnlyPreferredName	= 0x00000200
};

enum {
    kIOFBEDIDStdEstMode		= 0x00000001,
    kIOFBEDIDDetailedMode	= 0x00000002,
    kIOFBStdMode		= 0x00000004,
    kIOFBDriverMode		= 0x00000008,
    kIOFBScaledMode		= 0x00000010,
    kIOFBGTFMode		= 0x00000020
};

struct IOFBResolutionSpec {
    UInt32		timingID;
    UInt32		width;
    UInt32		height;
    float		refreshRate;
    Boolean		needInterlace;
};
typedef struct IOFBResolutionSpec IOFBResolutionSpec;

struct IOFBOvrDimensions {
    UInt32		width;
    UInt32		height;
    IOOptionBits	setFlags;
    IOOptionBits	clearFlags;
};
typedef struct IOFBOvrDimensions IOFBOvrDimensions;

struct EDIDDetailedTimingDesc {
    UInt16	clock;
    UInt8	horizActive;
    UInt8	horizBlanking;
    UInt8	horizHigh;
    UInt8	verticalActive;
    UInt8	verticalBlanking;
    UInt8	verticalHigh;
    UInt8	horizSyncOffset;
    UInt8	horizSyncWidth;
    UInt8	verticalSyncOffsetWidth;
    UInt8	syncHigh;
    UInt8	horizImageSize;
    UInt8	verticalImageSize;
    UInt8	imageSizeHigh;
    UInt8	horizBorder;
    UInt8	verticalBorder;
    UInt8	flags;
};
typedef struct EDIDDetailedTimingDesc EDIDDetailedTimingDesc;

struct EDIDGeneralDesc {
    UInt16	flag1;
    UInt8	flag2;
    UInt8	type;
    UInt8	flag3;
    UInt8	data[13];
};
typedef struct EDIDGeneralDesc EDIDGeneralDesc;

union EDIDDesc {
    EDIDDetailedTimingDesc	timing;
    EDIDGeneralDesc		general;
};
typedef union EDIDDesc EDIDDesc;

struct EDID {
    UInt8	header[8];
    UInt8	vendorProduct[4];
    UInt8	serialNumber[4];
    UInt8	weekOfManufacture;
    UInt8	yearOfManufacture;
    UInt8	version;
    UInt8	revision;
    UInt8	displayParams[5];
    UInt8	colorCharacteristics[10];
    UInt8	establishedTimings[3];
    UInt16	standardTimings[8];
    EDIDDesc	descriptors[4];
    UInt8	extension;
    UInt8	checksum;
};
typedef struct EDID EDID;

struct GTFTimingCurve
{
    UInt32 startHFrequency;
    UInt32 c;
    UInt32 m;
    UInt32 k;
    UInt32 j;
};
typedef struct GTFTimingCurve GTFTimingCurve;

struct IOFBConnect
{
    io_service_t		framebuffer;
    io_connect_t		connect;
    struct IOFBConnect *	next;
    struct IOFBConnect *	nextDependent;
    SInt64			dependentID;
    SInt32			dependentIndex;
    CFMutableDictionaryRef	iographicsProperties;
    CFMutableDictionaryRef	kernelInfo;
    CFMutableDictionaryRef	modes;
    CFMutableArrayRef		modesArray;
    CFMutableDictionaryRef	overrides;
    UInt32			 driverModeCount;
    IOFBDisplayModeDescription * driverModeInfo;
    IONotificationPortRef	notifyPort;
    io_iterator_t		interestNotifier;
    IOOptionBits		state;
    IOOptionBits		previousState;
    IODisplayModeID		defaultMode;
    IOIndex			defaultDepth;
    IODisplayModeID		default4By3Mode;
    UInt32			ovrFlags;
    UInt32			mirrorDefaultFlags;
    IODisplayVendorID		displayVendor;
    IODisplayProductID		displayProduct;
    IOFBOvrDimensions		dimensions;
    UInt32			defaultWidth;
    UInt32			defaultHeight;
    UInt32			defaultImageWidth;
    UInt32			defaultImageHeight;
    IODisplayTimingRange *	fbRange;
    GTFTimingCurve		gtfCurves[2];
    UInt32			numGTFCurves;
    Boolean			suppressRefresh;
    Boolean			detailedRefresh;
    Boolean			trimToDependent;
    Boolean			defaultToDependent;
    Boolean			make4By3;
    Boolean			defaultNot4By3;
    Boolean			relaunch;

    const IOFBMessageCallbacks * clientCallbacks;
    void *			 clientCallbackRef;
};
typedef struct IOFBConnect * IOFBConnectRef;


__private_extern__ IOFBConnectRef
IOFBConnectToRef( io_connect_t connect );

__private_extern__ void
IODisplayInstallTimings( IOFBConnectRef connectRef );

__private_extern__ kern_return_t
IOFBInstallMode( IOFBConnectRef connectRef, IODisplayModeID mode,
                 IODisplayModeInformation * info, IOTimingInformation * timingInfo,
                 UInt32 driverFlags, IOOptionBits modeGenFlags );

io_service_t
IODisplayForFramebuffer(
	io_service_t		framebuffer,
	IOOptionBits		options );

__private_extern__ CFDictionaryRef
_IODisplayCreateInfoDictionary(
    IOFBConnectRef		connectRef,
    io_service_t		framebuffer,
    IOOptionBits		options );

__private_extern__ IOReturn
IOCheckTimingWithDisplay( IOFBConnectRef connectRef,
			  const IOTimingInformation * timing,
			  IOOptionBits modeGenFlags );

__private_extern__ IOReturn
readFile(const char *path, vm_offset_t * objAddr, vm_size_t * objSize);

__private_extern__ float
ratioOver( float a, float b );

__private_extern__ CFMutableDictionaryRef
readPlist( const char * path, UInt32 key );
