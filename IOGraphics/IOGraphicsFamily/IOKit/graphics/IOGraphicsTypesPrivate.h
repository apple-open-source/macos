/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOGRAPHICSTYPESPRIVATE_H
#define _IOKIT_IOGRAPHICSTYPESPRIVATE_H

#include <IOKit/graphics/IOGraphicsTypes.h>

enum {
    // This is the ID given to a programmable timing used at boot time
    kIODisplayModeIDInvalid   = (IODisplayModeID) 0xFFFFFFFF,
    kIODisplayModeIDCurrent   = (IODisplayModeID) 0x00000000,
    kIODisplayModeIDAliasBase = (IODisplayModeID) 0x40000000
};


enum {
    // options for IOServiceRequestProbe()
    kIOFBForceReadEDID                  = 0x00000100,
    kIOFBAVProbe                        = 0x00000200,
    kIOFBSetTransform                   = 0x00000400,
    kIOFBTransformShift                 = 16,
    kIOFBScalerUnderscan                = 0x01000000,
};

enum {
    // transforms
    kIOFBRotateFlags                    = 0x0000000f,

    kIOFBSwapAxes                       = 0x00000001,
    kIOFBInvertX                        = 0x00000002,
    kIOFBInvertY                        = 0x00000004,

    kIOFBRotate0                        = 0x00000000,
    kIOFBRotate90                       = kIOFBSwapAxes | kIOFBInvertX,
    kIOFBRotate180                      = kIOFBInvertX  | kIOFBInvertY,
    kIOFBRotate270                      = kIOFBSwapAxes | kIOFBInvertY
};

// private IOPixelInformation.flags
enum {
    kFramebufferAGPFastWriteAccess      = 0x00100000,
    kFramebufferDeepMode                = 0x00200000
};

enum {
    kIOFBHWCursorSupported              = 0x00000001,
    kIOFBCursorPans                     = 0x00010000
};

enum {
    // Controller attributes
    kIOFBSpeedAttribute                 = ' dgs',
    kIOFBWSStartAttribute               = 'wsup',
    kIOFBProcessConnectChangeAttribute  = 'wsch',
    kIOFBEndConnectChangeAttribute      = 'wsed',

    kIOFBMatchedConnectChangeAttribute  = 'wsmc',

    // Connection attributes
    kConnectionInTVMode                 = 'tvmd',
    kConnectionWSSB                     = 'wssb',

    kConnectionRawBacklight             = 'bklt',
    kConnectionBacklightSave            = 'bksv',

    kConnectionVendorTag                = 'vtag'
};

enum {
    // kConnectionInTVMode values
    kConnectionNonTVMode                = 0,
    kConnectionNTSCMode                 = 1,
    kConnectionPALMode                  = 2
};

// values for kIOCapturedAttribute
enum {
    kIOCaptureDisableDisplayChange      = 0x00000001,
    kIOCaptureDisableDisplayDimming     = 0x00000002
};

/*! @enum FramebufferConstants
    @constant kIOFBVRAMMemory The memory type for IOConnectMapMemory() to get the VRAM memory. Use a memory type equal to the IOPixelAperture index to get a particular pixel aperture.
*/
enum {
    kIOFBVRAMMemory             = 110
};

#define kIOFBGammaHeaderSizeKey         "IOFBGammaHeaderSize"

#define kIONDRVFramebufferGenerationKey "IONDRVFramebufferGeneration"

#define kIOFramebufferOpenGLIndexKey    "IOFramebufferOpenGLIndex"

#define kIOFBCurrentPixelClockKey       "IOFBCurrentPixelClock"
#define kIOFBCurrentPixelCountKey       "IOFBCurrentPixelCount"
#define kIOFBCurrentPixelCountRealKey   "IOFBCurrentPixelCountReal"

#define kIOFBTransformKey               "IOFBTransform"
#define kIOFBRotatePrefsKey             "framebuffer-rotation"
#define kIOFBStartupTimingPrefsKey      "startup-timing"

#define kIOFBCapturedKey                "IOFBCaptured"

#define kIOFBMirrorDisplayModeSafeKey   "IOFBMirrorDisplayModeSafe"

#define kIOFBConnectInterruptDelayKey   "connect-interrupt-delay"

#define kIOFBUIScaleKey					"IOFBUIScale"

#define kIOGraphicsPrefsKey             "IOGraphicsPrefs"
#define kIODisplayPrefKeyKey            "IODisplayPrefsKey"
#define kIOGraphicsPrefsParametersKey   "IOGraphicsPrefsParameters"
#define kIOGraphicsIgnoreParametersKey  "IOGraphicsIgnoreParameters"

#define kIOGraphicsPrefsVersionKey      "version"
enum 
{
    kIOGraphicsPrefsCurrentVersion 	= 2
};

#define kIODisplayFastBootEDIDKey       "nv-edid"

#define kIOFBBuiltInKey                 "built-in"
#define kIOFBIntegratedKey              "IOFBIntegrated"

#define kIOMultimediaConnectionIDKey            "IOMultimediaConnectionID"
#define kIOMultimediaConnectionIDDefault        "hdmi-1"
#define kIOMultimediaConnectionPropertiesKey    "IOMultimediaConnectionProperties"
#define kIOCEAEDIDVersionKey                    "IOCEAEDIDVersion"
#define kIOCEADataBlocksKey                     "IOCEADataBlocks"

#define detailedTimingModeID            __reservedA[0]

#ifndef kIORequestIdleKey
#define kIORequestIdleKey               "IORequestIdle"
#endif

enum {
    kIOAccelSpecificID          = 0x00000002
};

#ifndef kIOFBLowPowerAggressiveness
#define kIOFBLowPowerAggressiveness     ((uint32_t) iokit_family_err(sub_iokit_graphics, 1))
#endif

#ifndef kIOFBCaptureAggressiveness
#define kIOFBCaptureAggressiveness      ((uint32_t) iokit_family_err(sub_iokit_graphics, 2))
#endif

#ifndef kIODisplayDimAggressiveness
#define kIODisplayDimAggressiveness     ((uint32_t) iokit_family_err(sub_iokit_graphics, 3))
#endif

#define kIOFBMessageConnectChange       iokit_family_err(sub_iokit_graphics, 100)
#define kIOFBMessageEndConnectChange    iokit_family_err(sub_iokit_graphics, 105)

#if 1
enum
{
    // kConnectionColorMode attribute
    kIODisplayColorModeReserved   = 0x00000000,
    kIODisplayColorModeRGB        = 0x00000001,
    kIODisplayColorModeYCbCr422   = 0x00000010,
    kIODisplayColorModeYCbCr444   = 0x00000100,
    kIODisplayColorModeRGBLimited = 0x00001000,
    kIODisplayColorModeAuto       = 0x10000000,
};
#endif

enum
{
    kUpstreamProtocolMsgWrite   = '\0auw',
    kUpstreamProtocolMsgRead    = '\0aur',
    kUpstreamProtocolConfig     = 'aupc',
    kUpstreamProtocolHDCPStatus = 'auph',
    kUpstreamProtocolHDCPConfigStatus = 'aupp',
    kUpstreamProtocolMsgStatus  = 'aums',
    kColorSpaceSelection        = 'cyuv'
};

enum
{
    // AppleUpstream status change (HDCP downstream status has changed)
    kIOFBAUSInterruptType          = 'aus ',
    // AppleUpstream Data ready  (AppleUpstreamUserClient message is ready for read)
    kIOFBAUDInterruptType          = 'aud '
};

#define kIOFBDPDeviceIDKey          "dp-device-id"
#define kIOFBDPDeviceTypeKey        "device-type"
#define kIOFBDPDeviceTypeDongleKey  "branch-device"

enum
{
    kDPRegisterLinkStatus      = 0x200,
    kDPRegisterLinkStatusCount = 6,
    kDPRegisterServiceIRQ      = 0x201,
};

enum
{
    kDPLinkStatusSinkCountMask = 0x3f,
};

enum
{
    kDPIRQRemoteControlCommandPending = 0x01,
    kDPIRQAutomatedTestRequest        = 0x02,
    kDPIRQContentProtection           = 0x04,
    kDPIRQMCCS                        = 0x08,
    kDPIRQSinkSpecific                = 0x40,
};

enum
{
    // values for graphic-options & kIOMirrorDefaultAttribute
//  kIOMirrorDefault       = 0x00000001,
//  kIOMirrorForced        = 0x00000002,
    kIOGPlatformYCbCr      = 0x00000004,
//  kIOMirrorHint          = 0x00010000,
    kIOMirrorNoAutoHDMI    = 0x00000010,
};

// values for displayOnline options

enum
{
	kIODisplayOptionBacklight  = 0x00000001,
	kIODisplayOptionDimDisable = 0x00000002
};

// boot compress gamma types

struct IOFBGammaPoint
{
	uint16_t in;
	uint16_t out;
};
typedef struct IOFBGammaPoint IOFBGammaPoint;

struct IOFBGamma
{
	uint16_t       pointCount;
	IOFBGammaPoint points[0];
};
typedef struct IOFBGamma IOFBGamma;

struct IOFBCompressedGamma
{
	IOFBGamma red;
	IOFBGamma green;
	IOFBGamma blue;
};
typedef struct IOFBCompressedGamma IOFBCompressedGamma;

struct IOFBBootGamma
{
	uint32_t            vendor;
	uint32_t            product;
	uint32_t            serial;
	uint16_t            length;
	uint16_t            resvA;
	uint32_t            resvB;
	IOFBCompressedGamma gamma;
};
typedef struct IOFBBootGamma IOFBBootGamma;

#define kIOFBBootGammaKey			"boot-gamma"
#define kIOFBBootGammaRestoredKey	"boot-gamma-restored"

// uint32_t OSData
#define kIOScreenRestoreStateKey "IOScreenRestoreState"

// values for kIOScreenRestoreStateKey
enum
{
	kIOScreenRestoreStateNone   = 0x00000000,
	kIOScreenRestoreStateNormal = 0x00000001,
	kIOScreenRestoreStateDark   = 0x00000002,
};

// Single source
#define DBG_IOG_NOTIFY_SERVER               10  // 0x0A 0x5320028: arg1 regID, arg2 serverNotified
#define DBG_IOG_SERVER_ACK                  11  // 0x0B 0x532002C: arg1 regID, arg2 serverState
#define DBG_IOG_VRAM_RESTORE                12	// 0x0C 0x5320030: arg1 regID
#define DBG_IOG_VRAM_BLACK                  13	// 0x0D 0x5320034: arg1 regID
#define DBG_IOG_WSAA_DEFER_ENTER            14	// 0x0E 0x5320038: arg1 regID, arg2 raw wsaaValue
#define DBG_IOG_WSAA_DEFER_EXIT             15	// 0x0F 0x532003C: arg1 regID, arg2 raw wsaaValue
#define DBG_IOG_SET_POWER_STATE             16	// 0x10 0x5320040: arg1 power ordinal, arg2 DBG_IOG_SOURCE_xxx (below)
#define DBG_IOG_SYSTEM_POWER_CHANGE         17	// 0x11 0x5320044: arg1 messageType, arg2 entry-0/exit-fromCapabilities or error, arg3 entry-0/exit-toCapabilities or 0
#define DBG_IOG_ACK_POWER_STATE             18	// 0x12 0x5320048: arg1 DBG_IOG_SOURCE_xxx (below), arg2 regID, arg3 state
#define DBG_IOG_SET_POWER_ATTRIBUTE         19	// 0x13 0x532004C: arg1 regID, arg2 power ordinal
#define DBG_IOG_ALLOW_POWER_CHANGE          20  // 0x14 0x5320050:
#define DBG_IOG_MUX_ALLOW_POWER_CHANGE      21  // 0x15 0x5320054:
#define DBG_IOG_SERVER_TIMEOUT              22  // 0x16 0x5320058: arg1 regID
#define DBG_IOG_NOTIFY_CALLOUT              23  // 0x17 0x532005C: arg1 regID, arg2/3/4 - Hex-i-fied OSMetaClass::name
#define DBG_IOG_MUX_POWER_MESSAGE           24  // 0x18 0x5320060: arg1 messageType/error, arg2 0/-1 (success early exit 1),-2 (success early exit 2), 0 (final exit)
#define DBG_IOG_FB_POWER_CHANGE             25  // 0x19 0x5320064: arg1 regID, arg2 powerOrdinal
#define DBG_IOG_WAKE_FROM_DOZE              26  // 0x1A 0x5320068: arg1 x, arg2 y
#define DBG_IOG_RECEIVE_POWER_NOTIFICATION  27  // 0x1B 0x532006C: arg1 DBG_IOG_PWR_EVENT_xxx enum (below), arg2 pmValue
#define DBG_IOG_CHANGE_POWER_STATE_PRIV     28  // 0x1C 0x5320070: arg1 DBG_IOG_SOURCE_xxx (below), arg2 state
#define DBG_IOG_CLAMP_POWER_ON              29  // 0x1D 0x5320074: arg1 DBG_IOG_SOURCE_xxx (below)
#define DBG_IOG_SET_TIMER_PERIOD            30  // 0x1E 0x5320078: arg1 DBG_IOG_SOURCE_xxx (below), arg2 idle time
#define DBG_IOG_HANDLE_EVENT                31  // 0x1F 0x532007C: arg1 regID, arg2 event
#define DBG_IOG_SET_ATTRIBUTE_EXT           32  // 0x20 0x5320080: arg1 regID, arg2 entry-attribute/exit-error, arg3 entry-value
#define DBG_IOG_CLAMSHELL                   33  // 0x21 0x5320084: arg2 DBG_IOG_SOURCE_xxx, arg2-4 varies on SOURCE
#define DBG_IOG_HANDLE_VBL_INTERRUPT        35  // 0x23 0x532008C: arg1 VBL delta real, arg2 VBL delta calculated, arg 3 VBL time, arg 4 VBL count
#define DBG_IOG_WAIT_QUIET                  36  // 0x24 0x5320090: arg1 GPU regID, arg2 error, arg3 regID, arg4 timeout in secs 
#define DBG_IOG_PLATFORM_CONSOLE            37  // 0x25 0x5320094: arg1 regID, arg2 hasVInfo, arg3 op, arg4:string where
#define DBG_IOG_CONSOLE_CONFIG              38  // 0x26 0x5320098: arg1 regID, arg2 width << 32 | height, arg3 rowBytes, arg4 depth << 32 | scale
#define DBG_IOG_VRAM_CONFIG                 39  // 0x27 0x532009c: arg1 regID, arg2 height, arg3 rowBytes, arg4 len
#define DBG_IOG_SET_GAMMA_TABLE             40  // 0x28 0x53200A0: arg1 regID, arg2 DBG_IOG_SOURCE_xxx (below), arg3 exit-error
#define DBG_IOG_NEW_USER_CLIENT             41  // 0x29 0x53200a4: arg1 regID, arg2 type, arg3  exit-error, arg4 0->normal, 1->diagnostic, 2->waitQuiet
#define DBG_IOG_FB_CLOSE                    42  // 0x2A 0x53200a8: arg1 regID, arg2 sys

// Multiple sources
#define DBG_IOG_SET_DISPLAY_MODE            100 // 0x64 0x5320190: arg1 DBG_IOG_SOURCE_xxx (below), arg2 regID, arg3 entry-modeID/exit-error, arg4 entry-depth/exit-0.
#define DBG_IOG_SET_DETAILED_TIMING         101 // 0x65 0x5320194: arg1 DBG_IOG_SOURCE_xxx (below), arg2 regID, arg3 entry-0/exit-error

// Source of event found in arg1
#define DBG_IOG_SOURCE_MATCH_FRAMEBUFFER             1
#define DBG_IOG_SOURCE_PROCESS_CONNECTION_CHANGE     2
#define DBG_IOG_SOURCE_EXT_SET_DISPLAY_MODE          3
#define DBG_IOG_SOURCE_EXT_SET_PROPERTIES            4
#define DBG_IOG_SOURCE_IODISPLAYWRANGLER             5
#define DBG_IOG_SOURCE_IODISPLAY                     6
#define DBG_IOG_SOURCE_STOP                          7
#define DBG_IOG_SOURCE_CHECK_POWER_WORK              8
#define DBG_IOG_SOURCE_SET_AGGRESSIVENESS            9
#define DBG_IOG_SOURCE_APPLEBACKLIGHTDISPLAY        10
#define DBG_IOG_SOURCE_NEWUSERCLIENT                11
#define DBG_IOG_SOURCE_OPEN                         12
#define DBG_IOG_SOURCE_CLOSE                        13
#define DBG_IOG_SOURCE_FINDCONSOLE                  14
#define DBG_IOG_SOURCE_SUSPEND                      15
#define DBG_IOG_SOURCE_UPDATE_GAMMA_TABLE           16
#define DBG_IOG_SOURCE_DEFERRED_CLUT                17
#define DBG_IOG_SOURCE_RESTORE_FRAMEBUFFER          18
#define DBG_IOG_SOURCE_EXT_GET_ATTRIBUTE            19
#define DBG_IOG_SOURCE_GET_ATTRIBUTE                20
#define DBG_IOG_SOURCE_POSTWAKE                     21
#define DBG_IOG_SOURCE_END_CONNECTION_CHANGE        22
#define DBG_IOG_SOURCE_SYSWILLPOWERON               23
#define DBG_IOG_SOURCE_IOGETHWCLAMSHELL             24
#define DBG_IOG_SOURCE_CLAMSHELL_HANDLER            25
#define DBG_IOG_SOURCE_READ_CLAMSHELL               26
#define DBG_IOG_SOURCE_RESET_CLAMSHELL              27
#define DBG_IOG_SOURCE_SYSWORK_READCLAMSHELL        28
#define DBG_IOG_SOURCE_SYSWORK_RESETCLAMSHELL       29
#define DBG_IOG_SOURCE_SYSWORK_ENABLECLAMSHELL      30
#define DBG_IOG_SOURCE_SYSWORK_PROBECLAMSHELL       31
#define DBG_IOG_SOURCE_CLAMSHELL_OFFLINE_CHANGE     32

// IOGraphics receive power notification event types
#define DBG_IOG_PWR_EVENT_DESKTOPMODE               1
#define DBG_IOG_PWR_EVENT_DISPLAYONLINE             2
#define DBG_IOG_PWR_EVENT_SYSTEMPWRCHANGE           3
#define DBG_IOG_PWR_EVENT_PROCCONNECTCHANGE         4
// Clamshell States
#define DBG_IOG_CLAMSHELL_STATE_NOT_PRESENT         0
#define DBG_IOG_CLAMSHELL_STATE_CLOSED              1
#define DBG_IOG_CLAMSHELL_STATE_OPEN                2


/* Values for IOFramebuffer::message().
   Follows mach/error.h layout.
     31:26 system (6 bits)
     25:14 subsystem (12 bits)
     13:0  code(14) */
#define iog_msg(msg) iokit_family_msg(sub_iokit_graphics, (msg))

/* Temporary backchannel for AGC(AppleGPUWrangler)->IOG.
 * Not on IOG workloops.
 * Provider and arg are ignored.
 * Since IOGRAPHICSTYPES_REV 49. */
#define kIOMessageGraphicsNotifyTerminated                      iog_msg(0x2001)

/* Request IOFramebuffer::probeAccelerator().
 * Not on IOG workloops.
 * Provider and arg are ignored.
 * Since IOGRAPHICSTYPES_REV 52. */
#define kIOMessageGraphicsProbeAccelerator                      iog_msg(0x2002)

/* Eject messages to IOAccelerator.
 * Not on IOG workloops.
 * Provider and arg are ignored.
 * Since IOGRAPHICSTYPES_REV 62. */
#define kIOMessageGraphicsDeviceEject                           iog_msg(0x2003)
#define kIOMessageGraphicsDeviceEjectFinalize                   iog_msg(0x2004)
#define kIOMessageGraphicsDeviceEjectCancel                     iog_msg(0x2005)

enum {
    kVendorDeviceNVidia,
    kVendorDeviceAMD,
    kVendorDeviceIntel,
};

#define kPCI_VID_APPLE                  0x106B  // IG Framebuffer driver reports this value, instead of 0x8086
#define kPCI_VID_AMD                    0x1B62
#define kPCI_VID_AMD_ATI                0x1002
#define kPCI_VID_INTEL                  0x8086
#define kPCI_VID_NVIDIA                 0x10DE
#define kPCI_VID_NVIDIA_AGEIA           0x1971

#endif /* ! _IOKIT_IOGRAPHICSTYPESPRIVATE_H */

