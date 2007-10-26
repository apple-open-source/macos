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
    // options for IOServiceRequestProbe()
    kIOFBForceReadEDID			= 0x00000100,
    kIOFBAVProbe			= 0x00000200,
    kIOFBSetTransform			= 0x00000400,
    kIOFBTransformShift			= 16,
    kIOFBScalerUnderscan		= 0x01000000,
};

enum {
    // transforms
    kIOFBRotateFlags		        = 0x0000000f,

    kIOFBSwapAxes			= 0x00000001,
    kIOFBInvertX			= 0x00000002,
    kIOFBInvertY			= 0x00000004,

    kIOFBRotate0			= 0x00000000,
    kIOFBRotate90			= kIOFBSwapAxes | kIOFBInvertX,
    kIOFBRotate180			= kIOFBInvertX  | kIOFBInvertY,
    kIOFBRotate270			= kIOFBSwapAxes | kIOFBInvertY
};

// private IOPixelInformation.flags
enum {
    kFramebufferAGPFastWriteAccess	= 0x00100000,
    kFramebufferDeepMode		= 0x00200000
};

enum {
    kIOFBHWCursorSupported		= 0x00000001,
    kIOFBCursorPans			= 0x00010000
};

enum {
    // Controller attributes
    kIOFBSpeedAttribute			= ' dgs',

    // Connection attributes
    kConnectionInTVMode			= 'tvmd',
    kConnectionWSSB			= 'wssb',

    kConnectionRawBacklight		= 'bklt',
    kConnectionBacklightSave		= 'bksv'
};

enum {
    // kConnectionInTVMode values
    kConnectionNonTVMode		= 0,
    kConnectionNTSCMode			= 1,
    kConnectionPALMode			= 2
};

// values for kIOCapturedAttribute
enum {
    kIOCaptureDisableDisplayChange	= 0x00000001,
    kIOCaptureDisableDisplayDimming	= 0x00000002
};

#define kIOFBGammaHeaderSizeKey		"IOFBGammaHeaderSize"

#define kIONDRVFramebufferGenerationKey "IONDRVFramebufferGeneration"

#define kIOFramebufferOpenGLIndexKey 	"IOFramebufferOpenGLIndex"

#define kIOFBCurrentPixelClockKey 	"IOFBCurrentPixelClock"
#define kIOFBCurrentPixelCountKey 	"IOFBCurrentPixelCount"

#define kIOFBTransformKey 		"IOFBTransform"
#define kIOFBRotatePrefsKey 		"framebuffer-rotation"
#define kIOFBCapturedKey 		"IOFBCaptured"

#define kIOFBMirrorDisplayModeSafeKey	"IOFBMirrorDisplayModeSafe"

#define kIOFBConnectInterruptDelayKey 	"connect-interrupt-delay"

#define kIOGraphicsPrefsKey 		"IOGraphicsPrefs"
#define kIODisplayPrefKeyKey 		"IODisplayPrefsKey"
#define kIOGraphicsPrefsParametersKey  "IOGraphicsPrefsParameters"

#define kIODisplayFastBootEDIDKey 	"nv-edid"

#define kIOFBBuiltInKey			"built-in"

#define kIOMultimediaConnectionIDKey		"IOMultimediaConnectionID"
#define kIOMultimediaConnectionIDDefault	"hdmi-1"
#define kIOMultimediaConnectionPropertiesKey	"IOMultimediaConnectionProperties"
#define kIOCEAEDIDVersionKey			"IOCEAEDIDVersion"
#define kIOCEADataBlocksKey			"IOCEADataBlocks"

#define detailedTimingModeID		__reservedA[0]

#ifndef kIORequestIdleKey
#define kIORequestIdleKey		"IORequestIdle"
#endif

enum {
    kIOAccelSpecificID		= 0x00000002
};

#ifndef kIOFBLowPowerAggressiveness
#define kIOFBLowPowerAggressiveness	iokit_family_err(sub_iokit_graphics, 1)
#endif

#ifndef kIOFBCaptureAggressiveness
#define kIOFBCaptureAggressiveness	iokit_family_err(sub_iokit_graphics, 2)
#endif

#ifndef kIODisplayDimAggressiveness
#define kIODisplayDimAggressiveness	iokit_family_err(sub_iokit_graphics, 3)
#endif

#endif /* ! _IOKIT_IOGRAPHICSTYPESPRIVATE_H */

