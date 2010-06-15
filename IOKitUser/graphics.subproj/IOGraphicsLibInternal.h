/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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


#if 0

#include <mach/mach_time.h>
#warning **LOGS**
#define RLOG 1
#define DEBG(cref, fmt, args...)                        \
if (cref->logfile) {                                    \
    uint64_t time = mach_absolute_time();       \
    fprintf(cref->logfile, "%10lld %s: ", ((time - cref->time0) / 1000), __FUNCTION__); \
    fprintf(cref->logfile, fmt, ## args);               \
    fflush(cref->logfile);                              \
}

#define TIMESTART()									\
{													\
    uint64_t time, start;				            \
	start = mach_absolute_time();

#define TIMEEND(name)					                                \
    time = mach_absolute_time();                                        \
    syslog(LOG_ERR, "%10lld : %s\n", ((time - start) / 1000), name);    \
}

#else  /* !RLOG */

#define DEBG(cref, fmt, args...)  {}
#define TIMESTART()
#define TIMEEND(name)

#endif


#if IOGRAPHICSTYPES_REV < 12

enum { kDisplayModeValidateAgainstDisplay = 0x00002000 };

#endif

enum {
    kDisplayAppleVendorID       = 0x610
};

enum {
    // disable any use of scaled modes,
    kOvrFlagDisableScaling      = 0x00000001,
    // remove driver modes,
    kOvrFlagDisableNonScaled    = 0x00000002,
    // disable scaled modes made up by the system (just use the override list)
    kOvrFlagDisableGenerated    = 0x00000004
};

enum {
    // skips all the various checks and always installs
    kScaleInstallAlways         = 0x00000001,
    // disables the install of a stretched version if the aspect is different
    kScaleInstallNoStretch      = 0x00000002,
    // install resolution untransformed
    kScaleInstallNoResTransform = 0x00000004
};

enum {
    kIOMirrorNoTrim             = 0x00000010
};

enum {
    kAppleNTSCManufacturerFlag           = 0x40,
    kApplePALManufacturerFlag            = 0x20,
    kAppleNTSCDefaultPALManufacturerFlag = 0x04
};

enum {
    kIOMirrorHint = 0x10000
};

enum {
    kIOFBEDIDStdEstMode         = 0x00000001,
    kIOFBEDIDDetailedMode       = 0x00000002,
    kIOFBStdMode                = 0x00000004,
    kIOFBDriverMode             = 0x00000008,
    kIOFBScaledMode             = 0x00000010,
    kIOFBGTFMode                = 0x00000020,
    kIOFBCVTEstMode             = 0x00000040,

    kIOFBTimingMatch            = 0x80000000
};

enum {
    kResSpecNeedInterlace         = 0x00000001,
    kResSpecReducedBlank          = 0x00000002,
    kResSpecInternalReducedBlank  = 0x00000004
};

struct IOFBResolutionSpec {
    UInt32              timingID;
    UInt32              width;
    UInt32              height;
    float               refreshRate;
    UInt32              flags;
};
typedef struct IOFBResolutionSpec IOFBResolutionSpec;

struct IOFBOvrDimensions {
    UInt32              width;
    UInt32              height;
    IOOptionBits        setFlags;
    IOOptionBits        clearFlags;
};
typedef struct IOFBOvrDimensions IOFBOvrDimensions;

struct EDIDDetailedTimingDesc {
    UInt16      clock;
    UInt8       horizActive;
    UInt8       horizBlanking;
    UInt8       horizHigh;
    UInt8       verticalActive;
    UInt8       verticalBlanking;
    UInt8       verticalHigh;
    UInt8       horizSyncOffset;
    UInt8       horizSyncWidth;
    UInt8       verticalSyncOffsetWidth;
    UInt8       syncHigh;
    UInt8       horizImageSize;
    UInt8       verticalImageSize;
    UInt8       imageSizeHigh;
    UInt8       horizBorder;
    UInt8       verticalBorder;
    UInt8       flags;
};
typedef struct EDIDDetailedTimingDesc EDIDDetailedTimingDesc;

struct EDIDGeneralDesc {
    UInt16      flag1;
    UInt8       flag2;
    UInt8       type;
    UInt8       flag3;
    UInt8       data[13];
};
typedef struct EDIDGeneralDesc EDIDGeneralDesc;

union EDIDDesc {
    EDIDDetailedTimingDesc      timing;
    EDIDGeneralDesc             general;
};
typedef union EDIDDesc EDIDDesc;

struct EDID {
    UInt8       header[8];
    UInt8       vendorProduct[4];
    UInt8       serialNumber[4];
    UInt8       weekOfManufacture;
    UInt8       yearOfManufacture;
    UInt8       version;
    UInt8       revision;
    UInt8       displayParams[5];
    UInt8       colorCharacteristics[10];
    UInt8       establishedTimings[3];
    UInt16      standardTimings[8];
    EDIDDesc    descriptors[4];
    UInt8       extension;
    UInt8       checksum;
};
typedef struct EDID EDID;

#define featureSupport  displayParams[4]

struct GTFTimingCurve
{
    UInt32 startHFrequency;
    UInt32 c;
    UInt32 m;
    UInt32 k;
    UInt32 j;
};
typedef struct GTFTimingCurve GTFTimingCurve;

enum {
    kExtTagCEA  = 0x02,
    kExtTagVTB  = 0x10,
    kExtTagDI   = 0x40
};


struct DIEXT {
    UInt8       header;
    UInt8       version;

    // digital only
    UInt8       standardSupported;
    UInt8       standardVersion[4];
    UInt8       dataFormatDesc;
    UInt8       dataFormats;
    UInt8       minPixelClockPerLink;
    UInt8       maxPixelClockPerLink[2];
    UInt8       crossoverFreq[2];

    // analogue/digital
    UInt8       subPixelLayout;
    UInt8       subPixelConfig;
    UInt8       subPixelShape;
    UInt8       horizontalPitch;
    UInt8       verticalPitch;
    UInt8       majorCapabilities;
    UInt8       miscCapabilities;
    UInt8       frameRateConversion;
    UInt8       convertedVerticalFreq[2];
    UInt8       convertedHorizontalFreq[2];
    UInt8       displayScanOrientation;

    UInt8       colorLuminanceDefault;
    UInt8       colorLuminancePreferred;
    UInt8       colorLuminanceCapabilities[2];

    UInt8       colorDepthFlags;
    UInt8       rgbBitsPerColor[3];
    UInt8       yuvBitsPerColor[3];

    UInt8       aspectRatioConversionModes;

    UInt8       packetizedDigitalVideo[16];
    UInt8       reserved[17];
    UInt8       audio[9];
    UInt8       gamma[46];

    UInt8       checksum;
};
typedef struct DIEXT DIEXT;

struct VTBEXT {
    UInt8       header;
    UInt8       version;
    UInt8       numDetailedTimings;
    UInt8       numCVTTimings;
    UInt8       numStandardTimings;
    UInt8       data[122];
    UInt8       checksum;
};
typedef struct VTBEXT VTBEXT;

struct VTBCVTTimingDesc {
    UInt8       verticalSize;
    UInt8       verticalSizeHigh;
    UInt8       refreshRates;
};
typedef struct VTBCVTTimingDesc VTBCVTTimingDesc;

// VTBCVTTimingDesc.verticalSizeHigh
enum {
    kVTBCVTAspectRatioMask      = 0x0c,
    kVTBCVTAspectRatio4By3      = 0x00,
    kVTBCVTAspectRatio16By9     = 0x04,
    kVTBCVTAspectRatio16By10    = 0x08,
};

// VTBCVTTimingDesc.refreshRates
enum {
    kVTBCVTPreferredRefreshMask = 0x60,
    kVTBCVTPreferredRefresh50   = 0x00,
    kVTBCVTPreferredRefresh60   = 0x20,
    kVTBCVTPreferredRefresh75   = 0x40,
    kVTBCVTPreferredRefresh85   = 0x60,

    kVTBCVTRefresh50            = 0x10,
    kVTBCVTRefresh60            = 0x08,
    kVTBCVTRefresh75            = 0x04,
    kVTBCVTRefresh85            = 0x02,
    kVTBCVTRefresh60RBlank      = 0x01
};

struct CEA861EXT {
    UInt8       header;
    UInt8       version;
    UInt8       detailedTimingsOffset;
    UInt8       flags;
    UInt8       data[123];
    UInt8       checksum;
};
typedef struct CEA861EXT CEA861EXT;

// CEA861EXT.flags (v2)
enum {
    kDTVSupportsUnderscan       = 0x80,
    kDTVSupportsBasicAudio      = 0x40,
    kDTVSupportsYUV444          = 0x20,
    kDTVSupportsYUV422          = 0x10,
    kDTVNumNativeTimings        = 0x0f
};

enum { kNumVendors = 4, kAllVendors = 0 };
enum {
    kIODisplayDitherControlDefault   =  kIODisplayDitherDefault << kIODisplayDitherRGBShift
                                     | (kIODisplayDitherDefault << kIODisplayDitherYCbCr444Shift)
                                     | (kIODisplayDitherDefault << kIODisplayDitherYCbCr422Shift),
};

struct IOFBConnect
{
    io_service_t                framebuffer;
    io_connect_t                connect;
    struct IOFBConnect *        next;
    struct IOFBConnect *        nextDependent;
    SInt64                      dependentID;
    SInt32                      dependentIndex;
    CFMutableDictionaryRef      iographicsProperties;
#if RLOG
    FILE *                      logfile;
    uint64_t                    time0;
#else
    void *                      __pad;
    uint64_t                    __pad2;
#endif
    CFMutableDictionaryRef      kernelInfo;
    CFMutableDictionaryRef      modes;
    CFMutableArrayRef           modesArray;
    CFMutableDictionaryRef      overrides;
    UInt32                       driverModeCount;
    IOFBDisplayModeDescription * driverModeInfo;
    IONotificationPortRef       notifyPort;
    io_iterator_t               interestNotifier;
    IOOptionBits                state;
    IOOptionBits                previousState;
    IOItemCount                 arbModeBase;
    IODisplayModeID             arbModeIDSeed;
    IODisplayModeID             startMode;
    IODisplayModeID             matchMode;
    IOIndex                     matchDepth;
    IODisplayModeID             defaultMode;
    IOIndex                     defaultDepth;
    IODisplayModeID             default4By3Mode;
    UInt32                      defaultMinWidth;
    UInt32                      defaultMinHeight;
    UInt32                      ovrFlags;
    UInt32                      mirrorDefaultFlags;
    IODisplayVendorID           displayVendor;
    IODisplayProductID          displayProduct;
    IOFBOvrDimensions           dimensions;
    int32_t                     defaultIndex;
    UInt32                      defaultWidth;
    UInt32                      defaultHeight;
    UInt32                      defaultImageWidth;
    UInt32                      defaultImageHeight;
    UInt64                      dualLinkCrossover;
    UInt32                      maxDisplayLinks;
    float                       nativeAspect;
    IODisplayTimingRange *       fbRange;       // only during IODisplayInstallTimings()
    IODisplayScalerInformation * scalerInfo;    // only during IOFBBuildModeList()
    GTFTimingCurve              gtfCurves[2];
    UInt32                      numGTFCurves;
    UInt64                      transform;

    uint32_t                    vendorsFound;
    uint32_t                    supportedColorModes[kNumVendors];
    uint32_t                    supportedComponentDepths[kNumVendors];
    uint32_t                    ditherControl[kNumVendors];

    Boolean                     defaultOnly;
    Boolean                     gtfDisplay;
    Boolean                     cvtDisplay;
    Boolean                     supportsReducedBlank;
    Boolean                     hasCEAExt;
    Boolean                     hasDIEXT;
    Boolean                     hasInterlaced;
    Boolean                     hasHDMI;
    Boolean                     hasShortVideoDescriptors;
    Boolean                     suppressRefresh;
    Boolean                     detailedRefresh;
    Boolean                     useScalerUnderscan;
    Boolean                     addTVFlag;
    Boolean                     trimToDependent;
    Boolean                     defaultToDependent;
    Boolean                     make4By3;
    Boolean                     defaultNot4By3;
    Boolean                     relaunch;
    Boolean                     firstBoot;
    Boolean                     displayMirror;

    struct IOAccelConnectStruct * transformSurface;

    const IOFBMessageCallbacks * clientCallbacks;
    void *                       clientCallbackRef;
};
typedef struct IOFBConnect * IOFBConnectRef;


__private_extern__ IOFBConnectRef
IOFBConnectToRef( io_connect_t connect );

__private_extern__ void
IODisplayInstallTimings( IOFBConnectRef connectRef );

__private_extern__ kern_return_t
IOFBSetKernelDisplayConfig( IOFBConnectRef connectRef );

__private_extern__ kern_return_t
IOFBInstallMode( IOFBConnectRef connectRef, IODisplayModeID mode,
                 IOFBDisplayModeDescription * desc,
                 UInt32 driverFlags, IOOptionBits modeGenFlags );

__private_extern__ CFDictionaryRef
_IODisplayCreateInfoDictionary(
    IOFBConnectRef              connectRef,
    io_service_t                framebuffer,
    IOOptionBits                options );

__private_extern__ IOReturn
IOCheckTimingWithDisplay( IOFBConnectRef connectRef,
                          IOFBDisplayModeDescription * desc,
                          IOOptionBits modeGenFlags );

__private_extern__ kern_return_t
IOFBDriverPreflight(IOFBConnectRef connectRef, IOFBDisplayModeDescription * desc);
__private_extern__ Boolean
ValidateTimingInformation( IOFBConnectRef connectRef, const IOTimingInformation * timingInfo );
__private_extern__ Boolean
IOFBTimingSanity(IOTimingInformation * timingInfo);
__private_extern__ Boolean
InvalidTiming( IOFBConnectRef connectRef, const IOTimingInformation * timingInfo );
__private_extern__ void
UpdateTimingInfoForTransform(IOFBConnectRef connectRef, 
                                IOFBDisplayModeDescription * desc,
                                IOOptionBits flags );

__private_extern__ IOReturn
readFile(const char *path, vm_address_t * objAddr, vm_size_t * objSize);

__private_extern__ float
ratioOver( float a, float b );

__private_extern__ CFMutableDictionaryRef
readPlist( const char * path, UInt32 key );

__private_extern__ Boolean
writePlist( const char * path, CFMutableDictionaryRef dict, UInt32 key );

__private_extern__ void
IOFBLogTiming(IOFBConnectRef connectRef, const IOTimingInformation * timing);
__private_extern__ void
IOFBLogRange(IOFBConnectRef connectRef, const IODisplayTimingRange * range);

__private_extern__ float
RefreshRateFromDetailedTiming( IODetailedTimingInformationV2 * detailed );
