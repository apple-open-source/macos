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

#include <mach/mach.h>
#include <mach/thread_switch.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/ndrvsupport/IOMacOSVideo.h>
#include "IOGraphicsLibPrivate.h"
#include <IOKit/iokitmig.h>

#ifndef kIOFBDependentIDKey
#define kIOFBDependentIDKey	"IOFBDependentID"
#endif
#ifndef kIOFBDependentIndexKey
#define kIOFBDependentIndexKey	"IOFBDependentIndex"
#endif

enum {
    kAquaMinWidth  = 800,
    kAquaMinHeight = 600
};

#define kIOFirstBootFlagPath	"/var/db/.com.apple.iokit.graphics"
//#define LOG 1

static kern_return_t 
IOFramebufferServerOpen( mach_port_t connect );

static kern_return_t
IOFBLookDefaultDisplayMode( IOFBConnectRef connectRef );

static kern_return_t
IOFBCreateDisplayModeInformation(
        IOFBConnectRef			connectRef,
	IODisplayModeID			displayMode,
	IOFBDisplayModeDescription *	allInfo,
        UInt32 *			driverFlags );

static Boolean
IOFBLookScaleBaseMode( IOFBConnectRef connectRef,
                        IOFBDisplayModeDescription * scaleBase,
                        IOFBDisplayModeDescription * scaleDesc );
static kern_return_t
IOFBInstallScaledModes( IOFBConnectRef connectRef,
                        IOFBDisplayModeDescription * scaleBase );

static struct IOFBConnect *	gAllConnects = 0;
static CFMutableDictionaryRef	gConnectRefDict = 0;
static CFDictionaryRef		gIOGraphicsProperties = 0;

static const char *		gIOGraphicsPropertiesData =
#if 0
  "{"
    "appleDDC = "
    "<00000082 00ff2140 0000008c 00043147 "
    "00000096 00053140 00000098 0003314c "
    "0000009a 0002314f 0000009c 00ff3159 "
    "000000aa 000d494f 000000b4 0001fffc "
    "000000b6 00004540 000000b8 000f454c "
    "000000ba 000e454f 000000bc 00ff4559 "
    "000000be 000b6140 000000c8 000a614a "
    "000000cc 0009614f 000000d0 00ff6159 "
    "000000d2 00ff614f 000000dc 0017ffc4 "
    "000000fa 00ff814f 00000104 00ff8180 "
    "00000106 0008818f 0000010c 00ff8199 "
    "00000118 00ffa940 0000011a 00ffa945 "
    "0000011c 00ffa94a 0000011e 00ffa94f "
    "00000120 00ffa954 00000121 00ffa959 "
    "00000128 00ffc140 0000012a 00ffc14f "
    "0000012c 00ffc940 0000012e 00ffc94f "
    "00000130 00ffd140 00000132 00ffd14f "
    "000001fe 00ffd1c0 00000208 00ffd1cc>;"
  "}";
#endif
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<!DOCTYPE plist SYSTEM \"file://localhost/System/Library/DTDs/PropertyList.dtd\">"
"<plist version=\"0.9\">"
"<dict>"
    "<key>appleDDC</key>"
    "<data>"
    "AAAAggD/IUAAAACMAAQxRwAAAJYABTFAAAAAmAADMUwAAACaAAIxTwAAAJwA/zFZAAAA"
    "qgANSU8AAAC0AAH//AAAALYAAEVAAAAAuAAPRUwAAAC6AA5FTwAAALwA/0VZAAAAvgAL"
    "YUAAAADIAAphSgAAAMwACWFPAAAA0AD/YVkAAADSAP9hTwAAANwAF//EAAAA+gD/gU8A"
    "AAEEAP+BgAAAAQYACIGPAAABDAD/gZkAAAEYAP+pQAAAARoA/6lFAAABHAD/qUoAAAEe"
    "AP+pTwAAASAA/6lUAAABIQD/qVkAAAEoAP/BQAAAASoA/8FPAAABLAD/yUAAAAEuAP/J"
    "TwAAATAA/9FAAAABMgD/0U8AAAH+AP/RwAAAAggA/9HM"
    "</data>"
    "<key>scale-resolutions</key>"
    "<array>"
        "<integer>640</integer>"
        "<integer>800</integer>"
        "<integer>1024</integer>"
        "<integer>1280</integer>"
        "<integer>1344</integer>"
        "<integer>1600</integer>"
        "<integer>1792</integer>"
        "<integer>1856</integer>"
        "<integer>1920</integer>"
        "<integer>2048</integer>"
#if 0
        "<integer>0x1e002d0</integer>"
#endif
    "</array>"
"</dict>"
"</plist>"
;

/* --------------------------------------------------------- */

kern_return_t
IOFramebufferOpen(
	io_service_t    service,
	task_port_t	owningTask,
	unsigned int	type,
	io_connect_t  *	connect )
{
    kern_return_t	kr;

    kr = io_service_open( service, owningTask, type, connect );

    if( (KERN_SUCCESS == kr) && (type == kIOFBServerConnectType))
        kr = IOFramebufferServerOpen( *connect );

    return( kr );
}

kern_return_t
IOFBCreateSharedCursor( mach_port_t connect,
	unsigned int version,
	unsigned int maxWidth, unsigned int maxHeight )
{
    IOFramebufferServerOpen( connect );

    return( IOConnectMethodScalarIScalarO( connect, 0, /*index*/
                    3, 0,
                    version, maxWidth, maxHeight ));
}

extern kern_return_t
IOFBGetFramebufferInformationForAperture( mach_port_t connect,
	    IOPixelAperture		  aperture,
	    IOFramebufferInformation	* info )
{
    IOPixelInformation	pixelInfo;
    IODisplayModeID	mode;
    IOIndex		depth;
    kern_return_t	err;

    err = IOFBGetCurrentDisplayModeAndDepth( connect, &mode, &depth );
    if( err)
	return( err);
    err = IOFBGetPixelInformation( connect, mode, depth, aperture, &pixelInfo );
    if( err)
	return( err);

    err = IOFBGetFramebufferOffsetForAperture( connect, aperture,
						&info->baseAddress);
    info->activeWidth	= pixelInfo.activeWidth;
    info->activeHeight	= pixelInfo.activeHeight;
    info->bytesPerRow	= pixelInfo.bytesPerRow;
    info->bytesPerPlane	= pixelInfo.bytesPerPlane;
    info->bitsPerPixel	= pixelInfo.bitsPerPixel;
    info->pixelType	= pixelInfo.pixelType;
    info->flags		= pixelInfo.flags;

    return( err);
}

extern kern_return_t
IOFBGetFramebufferOffsetForAperture( mach_port_t connect,
	    IOPixelAperture		  aperture,
	    IOByteCount			* offset )
{
    return( IOConnectMethodScalarIScalarO( connect, 8, /*index*/
                    1, 1, 
                    aperture,
                    offset ));
}

extern kern_return_t
IOFBSetBounds( mach_port_t connect,
	    IOGBounds	* rect )
{
    IOByteCount	len = 0;

    return( IOConnectMethodStructureIStructureO( connect, 9, /*index*/
                    sizeof( *rect), &len,
                    rect, NULL ));
}

kern_return_t
IOFBGetCurrentDisplayModeAndDepth( mach_port_t connect,
	IODisplayModeID * displayMode,
	IOIndex 	* depth )
{
    kern_return_t err;

    err = IOConnectMethodScalarIScalarO( connect, 2, /*index*/
                    0, 2,
                    displayMode, depth );

    return( err );
}

extern kern_return_t
IOFBGetPixelFormat( mach_port_t connect,
	IODisplayModeID mode,
	IOIndex 	depth,
        IOPixelAperture aperture,
	IOPixelEncoding * pixelFormat )
{
    IOPixelInformation	pixelInfo;
    kern_return_t	err;

    err = IOFBGetPixelInformation( connect, mode, depth, aperture, &pixelInfo );
    if( err)
	return( err);

    strncpy( *pixelFormat, pixelInfo.pixelFormat, kIOMaxPixelBits );

    return( err);
}

extern kern_return_t
IOFBSetCLUT( mach_port_t connect,
	UInt32		startIndex,
	UInt32		numEntries,
	IOOptionBits	options,
	IOColorEntry *	colors )
{
    return( IOConnectMethodScalarIStructureI( connect, 16, /* index */
            2, numEntries * sizeof( IOColorEntry),
            startIndex, options,
            colors ));
}

extern kern_return_t
IOFBSetGamma( mach_port_t connect,
	UInt32		channelCount,
	UInt32		dataCount,
	UInt32		dataWidth,
	void *		data )
{
    return( IOConnectMethodScalarIStructureI( connect, 11, /* index */
            3, ((dataWidth + 7) / 8) * dataCount * channelCount,
            channelCount, dataCount, dataWidth,
            data ));
}

static IOOptionBits
IOFBGetState( IOFBConnectRef connectRef )
{
    IOOptionBits	state = 0;
    io_service_t	display;

    if( (display = IODisplayForFramebuffer( connectRef->framebuffer, kNilOptions))) {
        state |= kIOFBConnectStateOnline;
        IOObjectRelease( display );
    }
    return( state );
}

extern kern_return_t
IOFBSet444To555Table( io_connect_t connect,
	const unsigned char *	table )
{
    return( IOConnectMethodScalarIStructureI( connect, 15, /* index */
            1, 16 * sizeof( UInt8),
            0, table ));
}

extern kern_return_t
IOFBSet555To444Table( io_connect_t connect,
	const unsigned char *	table )
{
    return( IOConnectMethodScalarIStructureI( connect, 15, /* index */
                1, 32 * sizeof( UInt8),
                1, table ));
}

extern kern_return_t
IOFBSet256To888Table( io_connect_t connect,
	const unsigned int *	table )
{
    return( IOConnectMethodScalarIStructureI( connect, 15, /* index */
                1, 256 * sizeof( UInt32),
                2, table ));
}

extern kern_return_t
IOFBSet888To256Table( io_connect_t connect,
	const unsigned char *	table )
{
    return( IOConnectMethodScalarIStructureI( connect, 15, /* index */
                1, 5 * 256 * sizeof( UInt8),
                3, table ));
}

static kern_return_t
_IOFBGetDisplayModeCount( io_connect_t connect,
	UInt32 * count )
{
    return( IOConnectMethodScalarIScalarO( connect, 6, /*index*/
                    0, 1,
                    count ));
}

static kern_return_t
_IOFBGetDisplayModes( io_connect_t connect,
	UInt32			count,
	IODisplayModeID	*	allDisplayModes )
{
    IOByteCount	len;

    len = count * sizeof( IODisplayModeID);

    return( IOConnectMethodStructureIStructureO( connect, 7, /*index*/
                    0, &len, NULL, (void *) allDisplayModes ));
}


kern_return_t
IOFBGetDisplayModeCount( io_connect_t connect,
	UInt32 * count )
{
    IOFBConnectRef connectRef = IOFBConnectToRef( connect );

    *count = CFArrayGetCount( connectRef->modesArray );

    return( kIOReturnSuccess );
}


kern_return_t
IOFBGetDisplayModes( io_connect_t connect,
	UInt32			count,
	IODisplayModeID	*	allDisplayModes )
{
    IOFBConnectRef connectRef = IOFBConnectToRef( connect );
    UInt32	   i, modeCount;

    modeCount = CFArrayGetCount( connectRef->modesArray );
    if( count < modeCount)
        modeCount = count;

    for( i = 0; i < modeCount; i++ ) {
        CFDictionaryRef	dict;
        CFNumberRef	num;

        dict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( connectRef->modesArray, i );
        num = CFDictionaryGetValue( dict, CFSTR(kIOFBModeIDKey) );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, (SInt32 *) &allDisplayModes[i] );
        else
            allDisplayModes[i] = 0;
    }

    return( kIOReturnSuccess );
}


kern_return_t
IOFBGetAttributeForFramebuffer( io_connect_t connect, io_connect_t otherConnect,
                                IOSelect attribute, UInt32 * value )
{
    IOReturn err;

    if( otherConnect) {
        err = IOConnectAddClient( connect, otherConnect );
        if( err)
            return( err );
    }
    err = IOConnectMethodScalarIScalarO( connect, 18, /*index*/
                                         1, 1,
                                         attribute, value );
    return( err );
}

kern_return_t
IOFBSetAttributeForFramebuffer( io_connect_t connect, io_connect_t otherConnect,
                                IOSelect attribute, UInt32 value )
{
    IOReturn err;

    if( otherConnect) {
        err = IOConnectAddClient( connect, otherConnect );
        if( err)
            return( err );
    }

    err = IOConnectMethodScalarIScalarO( connect, 19, /*index*/
                                         2, 0,
                                         attribute, value );
    return( err );
}

static float
ratioOver( float a, float b )
{
    if( a > b)
        return( a / b );
    else
        return( b / a );
}

kern_return_t
IOFBInstallMode( IOFBConnectRef connectRef, IODisplayModeID mode,
                 IODisplayModeInformation * info, IOTimingInformation * timingInfo,
                 UInt32 driverFlags )
{
    IOReturn		       ret = kIOReturnSuccess;
    CFMutableDictionaryRef     dict;
    CFMutableArrayRef	       array = 0;
    CFNumberRef		       num;
    CFDataRef		       data;
    CFDataRef		       timingData = 0;
    IODisplayModeInformation * otherInfo;

    if( timingInfo && (kIODetailedTimingValid & timingInfo->flags))
        timingData = CFDataCreate( kCFAllocatorDefault,
                                   (UInt8 *) &timingInfo->detailedInfo.v2,
                                   sizeof(IODetailedTimingInformationV2) );

    if( connectRef->trimToDependent && info) do {

        IOFBConnectRef	other;
        CFIndex 	modeCount, i;
        CFDataRef	data;
        Boolean		matches;

        if( 0 == (info->flags & kDisplayModeSafetyFlags /*kDisplayModeSafeFlag*/))
            continue;

        other = connectRef->nextDependent;
        if( !other->modesArray)
            continue;
        modeCount = CFArrayGetCount( other->modesArray );
        if( !modeCount)
            continue;
    
        for( i = 0, matches = false;
             !matches && (i < modeCount);
             i++)  {
    
            dict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( other->modesArray, i );
            data = CFDictionaryGetValue( dict, CFSTR(kIOFBModeDMKey) );
            otherInfo = (IODisplayModeInformation *) CFDataGetBytePtr( data );
    
#define kNeedFlags (kDisplayModeValidFlag | kDisplayModeSafeFlag)
            if( kNeedFlags != (otherInfo->flags & kNeedFlags))
                continue;
            matches = (info->nominalWidth == otherInfo->nominalWidth)
                    && (info->nominalHeight == otherInfo->nominalHeight);
        }
        if( !matches) {
            info->flags &= ~(kDisplayModeSafetyFlags /*kDisplayModeSafeFlag*/);
            info->flags |= (kDisplayModeTelevisionFlag << 1);
        }

    } while( false );

    do {

        if( mode == 0xffffffff) {
            // assign a programmable mode ID
            array = (CFMutableArrayRef) CFDictionaryGetValue( connectRef->kernelInfo,
                                                            CFSTR(kIOFBDetailedTimingsKey) );
            if( !array) {
                array = CFArrayCreateMutable( kCFAllocatorDefault, 0,
                                            &kCFTypeArrayCallBacks );
                if( !array) {
                    ret = kIOReturnNoMemory;
                    continue;
                }
                CFDictionarySetValue( connectRef->kernelInfo,
                                      CFSTR(kIOFBDetailedTimingsKey), array );
                CFRelease( array );
            }
            mode = kDisplayModeIDReservedBase + CFArrayGetCount(array);
            if( timingData)
                CFArrayAppendValue( array, timingData );
        }
    
        if( NULL == info)
            continue;
    
        dict = (CFMutableDictionaryRef) CFDictionaryGetValue( connectRef->modes,
                                                            (const void *) mode );
        if( !dict) {
            dict = CFDictionaryCreateMutable( kCFAllocatorDefault, (CFIndex) 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks );
            if( dict) {
                CFArrayAppendValue( connectRef->modesArray, dict );
                CFDictionarySetValue( connectRef->modes, (const void *) mode, dict );
                CFRelease( dict );
            } else {
                ret = kIOReturnNoMemory;
                continue;
            }
        }
    
        num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &mode );
        if( num) {
            CFDictionarySetValue( dict, CFSTR(kIOFBModeIDKey), num );
            CFRelease( num );
        }
    
        if( driverFlags && (0 == (mode & 0x80000000))) {
            num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &driverFlags );
            if( num) {
                CFDictionarySetValue( dict, CFSTR(kIOFBModeDFKey), num );
                CFRelease( num );
            }
        }
    
        if( info) {
            data = CFDataCreate( kCFAllocatorDefault,
                            (UInt8 *) info, sizeof(IODisplayModeInformation));
            if( data) {
                CFDictionarySetValue( dict, CFSTR(kIOFBModeDMKey), data );
                CFRelease(data);
            }
        }
    
        if( timingData)
            CFDictionarySetValue( dict, CFSTR(kIOFBModeTMKey), timingData );

        if( timingInfo && timingInfo->appleTimingID) {
            num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &timingInfo->appleTimingID );
            if( num) {
                CFDictionarySetValue( dict, CFSTR(kIOFBModeAIDKey), num );
                CFRelease( num );
            }
        }

    } while( false );
    
    if( timingData)
        CFRelease(timingData);

    return( ret );
}

static kern_return_t
IOFBSetKernelConfig( IOFBConnectRef connectRef )
{
    kern_return_t err = kIOReturnSuccess;

    if( CFDictionaryGetCount(connectRef->kernelInfo)) {
        err = IOConnectSetCFProperty( connectRef->connect, CFSTR(kIOFBConfigKey), connectRef->kernelInfo );
#if LOG
        printf("IOConnectSetCFProperty(%x)\n", err);
        CFShow(connectRef->kernelInfo);
#endif
    }

    return( err );
}

static kern_return_t
IOFBBuildModeList( IOFBConnectRef connectRef )
{
    kern_return_t	   	err;
    CFMutableDictionaryRef 	dict;
    CFMutableArrayRef	   	array;
    CFTypeRef			obj;
    IODisplayModeID *	   	modes;
    UInt32		   	i, modeCount;
    IOFBDisplayModeDescription	scaleDesc;
    Boolean		   	scaleCandidate;

    if( connectRef->kernelInfo)
        CFRelease( connectRef->kernelInfo );
    if( connectRef->modes)
        CFRelease( connectRef->modes );
    if( connectRef->modesArray)
        CFRelease( connectRef->modesArray );

    connectRef->suppressRefresh = connectRef->overrides
                && (0 != CFDictionaryGetValue( connectRef->overrides, CFSTR(kIODisplayIsDigitalKey)));

    dict = CFDictionaryCreateMutable( kCFAllocatorDefault, (CFIndex) 0,
                                             (CFDictionaryKeyCallBacks *) 0,
                                             &kCFTypeDictionaryValueCallBacks );
    connectRef->modes = dict;

    dict = (CFMutableDictionaryRef) IORegistryEntryCreateCFProperty(
                                        connectRef->framebuffer, 
                                        CFSTR(kIOFBConfigKey),
                                        kCFAllocatorDefault, kNilOptions);

    if( true && dict && (array = (CFMutableArrayRef) CFDictionaryGetValue( dict, CFSTR(kIOFBModesKey)))) {
        // pick up existing config
        connectRef->kernelInfo = dict;
        CFRetain(array);
        connectRef->modesArray = array;

        if( connectRef->suppressRefresh)
            connectRef->suppressRefresh = (0 != CFDictionaryGetValue(dict, CFSTR("IOFB0Hz")));

        modeCount = CFArrayGetCount( connectRef->modesArray );
        for( i = 0; i < modeCount; i++ ) {
            const void * key;
            CFNumberRef	 num;

            dict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( connectRef->modesArray, i );
            num = CFDictionaryGetValue( dict, CFSTR(kIOFBModeIDKey) );
            CFNumberGetValue( num, kCFNumberSInt32Type, (SInt32 *) &key );
            CFDictionarySetValue( connectRef->modes, key, dict );
        }

        connectRef->relaunch = true;

        return( kIOReturnSuccess );
    }

    dict = CFDictionaryCreateMutable( kCFAllocatorDefault, (CFIndex) 0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks );
    connectRef->kernelInfo = dict;

    connectRef->modesArray = CFArrayCreateMutable( kCFAllocatorDefault, 0,
                                                   &kCFTypeArrayCallBacks );
    CFDictionarySetValue( dict, CFSTR(kIOFBModesKey), connectRef->modesArray );

    // -- install some detailed
    if( connectRef->state & kIOFBConnectStateOnline) {

        IODisplayInstallDetailedTimings( connectRef );

        IOFBSetKernelConfig( connectRef );
    }

    // -- mode list
    err = _IOFBGetDisplayModeCount( connectRef->connect, &modeCount );
    if( kIOReturnSuccess == err) {
        modes = (IODisplayModeID *) calloc( modeCount, sizeof( IODisplayModeID));
        err = _IOFBGetDisplayModes( connectRef->connect, modeCount, modes );
    } else {
        modes = 0;
        modeCount = 0;
    }

    bzero( &scaleDesc, sizeof(scaleDesc) );

    for( i = 0, scaleCandidate = false; i < modeCount; i++)  {

        IOFBDisplayModeDescription	allInfo;
        UInt32				driverFlags;
        
        err = IOFBCreateDisplayModeInformation( connectRef, modes[i], &allInfo, &driverFlags );
        if( kIOReturnSuccess != err)
            continue;

        IOFBInstallMode( connectRef, modes[i], &allInfo.info,
                         (modes[i] & 0x80000000) ? 0 : &allInfo.timingInfo,
                         driverFlags );

        scaleCandidate |= IOFBLookScaleBaseMode( connectRef, &allInfo, &scaleDesc );
    }
    if( modes)
        free( modes );

    // -- scaling
    if( scaleCandidate)
        IOFBInstallScaledModes( connectRef, &scaleDesc );

    if( connectRef->suppressRefresh)
        CFDictionarySetValue(connectRef->kernelInfo, CFSTR("IOFB0Hz"), kCFBooleanTrue);

    if( connectRef->overrides && (obj = CFDictionaryGetValue( connectRef->overrides, CFSTR("IOGFlags")) ))
        CFDictionarySetValue( connectRef->kernelInfo, CFSTR(kIODisplayConnectFlagsKey), obj );

    // -- prune
#if 0
    CFNumberRef		   num;
    CFDataRef		   data;
    IODisplayModeID	   mode;

    modeCount = CFArrayGetCount( connectRef->modesArray );

    for( i = 0; i < modeCount; i++)  {

        IODisplayModeInformation * info;

        dict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( connectRef->modesArray, i );
        data = CFDictionaryGetValue( dict, CFSTR(kIOFBModeDMKey) );
        info = (IODisplayModeInformation *) CFDataGetBytePtr( data );

        if( info->flags & kDisplayModeValidFlag)
            continue;
        CFArrayRemoveValueAtIndex( connectRef->modesArray, i );
        i--; modeCount--;
        num = CFDictionaryGetValue( dict, CFSTR(kIOFBModeIDKey) );
        CFNumberGetValue( num, kCFNumberSInt32Type, &mode );

        CFDictionaryRemoveValue( connectRef->modes, (const void *) mode );
    }
#endif

    // -- install
    err = IOFBSetKernelConfig( connectRef );

    return( err );
}

static void
IOFBUpdateConnectState( IOFBConnectRef connectRef )
{
    connectRef->defaultMode    = 0;
    connectRef->defaultDepth   = 1;

    connectRef->displayVendor  = kDisplayVendorIDUnknown;
    connectRef->displayProduct = kDisplayProductIDGeneric;

    connectRef->state = IOFBGetState( connectRef );
}

static kern_return_t
IOFBRebuild( IOFBConnectRef connectRef )
{

    if( kIOReturnSuccess != IOFBGetAttributeForFramebuffer( connectRef->connect, MACH_PORT_NULL,
                                    kIOMirrorDefaultAttribute, &connectRef->mirrorDefaultFlags))
        connectRef->mirrorDefaultFlags = 0;

#if LOG
    printf("%p: ID (%qx,%d) -> %p, %08lx, %08lx, %08lx\n",
            connectRef, connectRef->dependentID, connectRef->dependentIndex, connectRef->nextDependent,
            connectRef->state, connectRef->nextDependent ? connectRef->nextDependent->state : 0,
            connectRef->mirrorDefaultFlags);
#endif

    connectRef->trimToDependent  = (kIOMirrorForced & connectRef->mirrorDefaultFlags)
                                && (0 != connectRef->dependentIndex)
                                && (connectRef->nextDependent)
                                && (0 != (kIOMirrorHint & connectRef->mirrorDefaultFlags));

    connectRef->defaultToDependent = (kIOMirrorDefault & connectRef->mirrorDefaultFlags)
                                && (0 != connectRef->dependentIndex)
                                && (connectRef->nextDependent)
                                && (0 != (kIOMirrorHint & connectRef->mirrorDefaultFlags));

    IOFBCreateOverrides( connectRef );

    IOFBBuildModeList( connectRef );

    IOFBLookDefaultDisplayMode( connectRef );

    connectRef->make4By3	 = (kIOMirrorDefault & connectRef->mirrorDefaultFlags)
                                && (connectRef->defaultNot4By3)
                                && (0 == connectRef->dependentIndex)
                                && (0 != (kIOMirrorHint & connectRef->mirrorDefaultFlags));

    return( kIOReturnSuccess );
}

static void
IOFBInterestCallback( void * refcon, io_service_t service,
                      natural_t messageType, void * messageArgument )
{
    IOFBConnectRef		connectRef = (IOFBConnectRef) refcon;
    IOReturn			err;
    IODisplayModeID		mode = 0;
    IOIndex			depth;
    IODisplayModeInformation	info;
    UInt32			value;
    UInt32			previousState, previous4By3, previousProduct, previousVendor;

    switch( messageType) {

      case kIOMessageServiceIsSuspended:

    
        IOFBGetAttributeForFramebuffer( connectRef->connect, MACH_PORT_NULL,
                                        kConnectionChanged, &value );
    
        previousState   = connectRef->state;
        previous4By3    = connectRef->make4By3;
        previousProduct = connectRef->displayProduct;
        previousVendor  = connectRef->displayVendor;
    
        IOFBUpdateConnectState( connectRef );

        IOFBRebuild( connectRef );

        if( kIOFBConnectStateOnline & previousState & connectRef->state) do {

            if( previousProduct != connectRef->displayProduct)
                break;
            if( previousVendor != connectRef->displayVendor)
                break;
            if( previous4By3 != connectRef->make4By3)
                break;

            err = IOFBGetCurrentDisplayModeAndDepth( connectRef->connect, &mode, &depth );
            if( kIOReturnSuccess == err)
                err = IOFBGetDisplayModeInformation( connectRef->connect, mode, &info );
            if( (kIOReturnSuccess != err) || (0 == (info.flags & kDisplayModeValidFlag))) {
                mode = 0;
                break;
            }
    
        } while( false );

        if( !mode) {
            mode  = connectRef->defaultMode;
            depth = connectRef->defaultDepth;
        }
    
        if( connectRef->make4By3 && connectRef->default4By3Mode) {
            err = IOFBGetDisplayModeInformation( connectRef->connect, mode, &info );
            if( (kIOReturnSuccess == err) 
              && ratioOver(((float)info.nominalWidth) / ((float)info.nominalHeight), 4.0 / 3.0) > 1.03125) {
                mode = connectRef->default4By3Mode;
            }
        }
    
        err = IOFBSetDisplayModeAndDepth( connectRef->connect, mode, depth );

        connectRef->clientCallbacks->ConnectionChange(connectRef->clientCallbackRef, (void *) NULL);

        break;
    }
}

mach_port_t
IOFBGetNotificationMachPort( io_connect_t connect )
{
    IOFBConnectRef connectRef = IOFBConnectToRef( connect );

    if( connectRef)
        return( IONotificationPortGetMachPort( connectRef->notifyPort ));
    else
        return( MACH_PORT_NULL );
}

kern_return_t
IOFBDispatchMessageNotification( io_connect_t connect, mach_msg_header_t * message,
                                 UInt32 version, const IOFBMessageCallbacks * callbacks, void * callbackRef )
{
    IOFBConnectRef connectRef = IOFBConnectToRef( connect );

    switch( message->msgh_id) {

        case 0:
            callbacks->WillPowerOff(callbackRef, (void *) connect);
            break;

        case 1:
            callbacks->DidPowerOn(callbackRef, (void *) connect);
            break;
    }

    connectRef->clientCallbacks   = callbacks;
    connectRef->clientCallbackRef = callbackRef;
    IODispatchCalloutFromMessage( NULL, message, connectRef->notifyPort );

    return( kIOReturnSuccess );
}

kern_return_t
IOFBAcknowledgeNotification( void * notificationID )
{
    io_connect_t connect = (io_connect_t) notificationID;

    if( connect)
        return( IOFBAcknowledgePM( connect ));
    else
        return( kIOReturnSuccess );
}

extern kern_return_t
IOFBAcknowledgePM( io_connect_t connect )
{
    return( IOConnectMethodScalarIScalarO( connect, 14, /*index*/
                    0, 0 ));
}

// Display mode information

static void
IOFBMakeNumKeys( const void * key, const void * value, void * context )
{
    CFStringRef 		str = key;
    CFMutableDictionaryRef	newTOvr = context;
    const char *    		cStr;
    char *	    		buffer = NULL;
    UInt32			timing;

    cStr = CFStringGetCStringPtr( str, kCFStringEncodingMacRoman);
    if( !cStr) {
        CFIndex bufferSize = CFStringGetLength(str) + 1;
        buffer = malloc( bufferSize);
        if( buffer && CFStringGetCString( str, buffer, bufferSize, kCFStringEncodingMacRoman))
            cStr = buffer;
    }
    if( cStr) {
        timing = strtol( cStr, 0, 0 );
        CFDictionarySetValue( newTOvr, (const void *) timing, value );
    }
    if( buffer)
        free( buffer);
}

void
IOFBCreateOverrides( IOFBConnectRef connectRef )
{
    io_service_t		framebuffer = connectRef->framebuffer;
    CFDictionaryRef		modeDict, oldOvr = 0;
    CFMutableDictionaryRef	newModeDict, ovr = 0;
    CFTypeRef			obj;
    CFNumberRef			num;

    if( connectRef->overrides) {
        CFRelease( connectRef->overrides );
        connectRef->overrides = NULL;
    }

    do {

        oldOvr = IODisplayCreateInfoDictionary( framebuffer, kNilOptions );
        if( !oldOvr)
            continue;

        num = CFDictionaryGetValue( oldOvr, CFSTR("IOGFlags") );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, (SInt32 *) &connectRef->ovrFlags );
        else
            connectRef->ovrFlags = 0;

        num = CFDictionaryGetValue( oldOvr, CFSTR(kDisplayVendorID) );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, (SInt32 *) &connectRef->displayVendor );
        num = CFDictionaryGetValue( oldOvr, CFSTR(kDisplayProductID) );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, (SInt32 *) &connectRef->displayProduct );

        ovr = CFDictionaryCreateMutable( kCFAllocatorDefault, (CFIndex) 0,
                                                &kCFTypeDictionaryKeyCallBacks,  
                                                &kCFTypeDictionaryValueCallBacks );
        if( !ovr)
            continue;

        modeDict = CFDictionaryGetValue( oldOvr, CFSTR("tovr") );
        if( modeDict && (newModeDict = CFDictionaryCreateMutable( 
                                    kCFAllocatorDefault, (CFIndex) 0,
                                    (CFDictionaryKeyCallBacks *) 0,
                                    &kCFTypeDictionaryValueCallBacks )))
        {
            CFDictionarySetValue( ovr, CFSTR("tovr"), newModeDict );
            CFRelease( newModeDict );
            CFDictionaryApplyFunction( modeDict, &IOFBMakeNumKeys, newModeDict );
        }

        modeDict = CFDictionaryGetValue( oldOvr, CFSTR("tinf") );
        if( modeDict && (newModeDict = CFDictionaryCreateMutable(
                                    kCFAllocatorDefault, (CFIndex) 0,
                                    (CFDictionaryKeyCallBacks *) 0,
                                    &kCFTypeDictionaryValueCallBacks )))
        {
            CFDictionarySetValue( ovr, CFSTR("tinf"), newModeDict );
            CFRelease( newModeDict );
            CFDictionaryApplyFunction( modeDict, &IOFBMakeNumKeys, newModeDict );
        }

        if( (obj = CFDictionaryGetValue( oldOvr, CFSTR(kIODisplayEDIDKey)) ))
            CFDictionarySetValue( ovr, CFSTR(kIODisplayEDIDKey), obj );

        if( (obj = CFDictionaryGetValue( oldOvr, CFSTR(kDisplayHorizontalImageSize)) ))
            CFDictionarySetValue( ovr, CFSTR(kDisplayHorizontalImageSize), obj );
        if( (obj = CFDictionaryGetValue( oldOvr, CFSTR(kDisplayVerticalImageSize)) ))
            CFDictionarySetValue( ovr, CFSTR(kDisplayVerticalImageSize), obj );
        if( (obj = CFDictionaryGetValue( oldOvr, CFSTR(kIODisplayIsDigitalKey)) ))
            CFDictionarySetValue( ovr, CFSTR(kIODisplayIsDigitalKey), obj );
        if( (obj = CFDictionaryGetValue( oldOvr, CFSTR(kDisplayFixedPixelFormat)) ))
            CFDictionarySetValue( ovr, CFSTR(kDisplayFixedPixelFormat), obj );
        if( (obj = CFDictionaryGetValue( oldOvr, CFSTR("trng")) ))
            CFDictionarySetValue( ovr, CFSTR("trng"), obj );
        if( (obj = CFDictionaryGetValue( oldOvr, CFSTR("sync")) ))
            CFDictionarySetValue( ovr, CFSTR("sync"), obj );
        if( (obj = CFDictionaryGetValue( oldOvr, CFSTR("scale-resolutions")) ))
            CFDictionarySetValue( ovr, CFSTR("scale-resolutions"), obj );
        if( (obj = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &connectRef->ovrFlags ))) {
            CFDictionarySetValue( ovr, CFSTR("IOGFlags"), obj );
            CFRelease(obj);
        }

    } while( false );

    if( oldOvr)
        CFRelease( oldOvr );

    connectRef->overrides = ovr;
}

static IOIndex
IOFBIndexForPixelBits( IOFBConnectRef connectRef, IODisplayModeID mode,
                                      IOIndex maxIndex, UInt32 bpp )
{
    IOPixelInformation	pixelInfo;
    IOIndex		index, depth = -1;
    kern_return_t	err;

    for( index = 0; index <= maxIndex; index++ ) {

        err = IOFBGetPixelInformation( connectRef->connect, mode, index,
                                        kIOFBSystemAperture, &pixelInfo );
        if( (kIOReturnSuccess == err) && (pixelInfo.bitsPerPixel >= bpp)) {
            depth = index;
            break;
        }
    }

    return( depth );
}

static Boolean
IOFBShouldDefaultDeep( IOFBConnectRef connectRef )
{
    CFNumberRef			num;
    SInt32			vramBytes;
#define kIOFBSmallVRAMBytes	(10 * 1024 * 1024)

    num = IORegistryEntryCreateCFProperty( connectRef->framebuffer, CFSTR(kIOFBMemorySizeKey),
                                            kCFAllocatorDefault, kNilOptions );
    if( num) {
        CFNumberGetValue( num, kCFNumberSInt32Type, &vramBytes );
        CFRelease( num );
    } else
        vramBytes = kIOFBSmallVRAMBytes;
    
    return( vramBytes >= kIOFBSmallVRAMBytes );
}

struct DMTimingOverrideRec {
 UInt32 timingOverrideVersion;
 UInt32 timingOverrideAttributes;   // flags
 UInt32 timingOverrideSetFlags;	    // VDTimingInfoRec.csTimingFlags |= timingOverrideSetFlags
 UInt32 timingOverrideClearFlags;   // VDTimingInfoRec.csTimingFlags &= (~timingOverrideClearFlags)
 UInt32 timingOverrideReserved[16]; // reserved
};
typedef struct DMTimingOverrideRec      DMTimingOverrideRec;

struct DMDisplayTimingInfoRec {
 UInt32 timingInfoVersion;
 UInt32 timingInfoAttributes;       // flags
 SInt32 timingInfoRelativeQuality;  // quality of the timing
 SInt32 timingInfoRelativeDefault;  // relative default of the timing
 UInt32 timingInfoReserved[16];     // reserved
};
typedef struct DMDisplayTimingInfoRec   DMDisplayTimingInfoRec;

#define desireDPI	(75.0)
#define mmPerInch	(25.4)


static kern_return_t
IOFBLookDefaultDisplayMode( IOFBConnectRef connectRef )
{
    IOReturn			err;
    CFDataRef			data;
    UInt32			modeCount;
    SInt32			bestDefault, rDefault;
    SInt32			bestQuality, rQuality;
    CFDictionaryRef		dict;
    IODisplayModeID		mode, bestMode = 0;
    IODisplayModeInformation	bestInfo;
    IODisplayModeInformation *	info;
    SInt32			bestDepth, minDepth, otherDepth;
    CFDictionaryRef		ovr, tinf;
    CFDataRef			modetinf;
    CFNumberRef			num;
    DMDisplayTimingInfoRec *	tinfRec;
    SInt32			i, timingID;
    Boolean			better, defaultToDependent;
    UInt32			desireRefresh;
    UInt32			biggest4By3;
    float			desireHPix, desireVPix;

    ovr = connectRef->overrides;
    if( ovr)
        tinf = CFDictionaryGetValue( ovr, CFSTR("tinf") );
    else
        tinf = 0;

    desireHPix = desireVPix = 0;
    if( ovr && (num = CFDictionaryGetValue( ovr, CFSTR(kDisplayHorizontalImageSize) ))) {
        CFNumberGetValue( num, kCFNumberFloatType, &desireHPix );
        if( desireHPix)
            desireHPix = desireHPix / mmPerInch * desireDPI;
    } 
    if( ovr && (num = CFDictionaryGetValue( ovr, CFSTR(kDisplayVerticalImageSize) ))) {
        CFNumberGetValue( num, kCFNumberFloatType, &desireVPix );
        if( desireVPix)
            desireVPix = desireVPix / mmPerInch * desireDPI;
    }

    bestQuality = bestDefault = 0;
    bestDepth = 1;

    defaultToDependent = false;
    if( connectRef->defaultToDependent) do {

        if( kIOReturnSuccess != IOFBGetCurrentDisplayModeAndDepth( connectRef->nextDependent->connect,
                                                                    &mode, &otherDepth ))
            continue;
        dict = CFDictionaryGetValue( connectRef->nextDependent->modes, (const void *) mode );
        if( dict && (data = CFDictionaryGetValue( dict, CFSTR(kIOFBModeDMKey) ))) {
            info = (IODisplayModeInformation *) CFDataGetBytePtr(data);
            desireHPix = info->nominalWidth;
            desireVPix = info->nominalHeight;
            defaultToDependent = true;
        }

    } while( false );
    
    desireRefresh = (86 << 16);
    biggest4By3 = 0;
    connectRef->default4By3Mode = 0;

    modeCount = CFArrayGetCount( connectRef->modesArray );
    for( i = 0; i < modeCount; i++)  {

        dict = CFArrayGetValueAtIndex( connectRef->modesArray, i );
        better = false;
        data = (CFDataRef) CFDictionaryGetValue( dict, CFSTR(kIOFBModeDMKey) );
        if( !data)
            continue;
        info = (IODisplayModeInformation *) CFDataGetBytePtr(data);

        if( 0 == (info->flags & kDisplayModeValidFlag))
            continue;

        num = CFDictionaryGetValue( dict, CFSTR(kIOFBModeIDKey) );
        if( !num)
            continue;
        CFNumberGetValue( num, kCFNumberSInt32Type, &mode );

        num = CFDictionaryGetValue( dict, CFSTR(kIOFBModeAIDKey) );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, &timingID );
        else
            timingID = 0;

        // make sure it does >= 16bpp
        minDepth = IOFBIndexForPixelBits( connectRef, mode, info->maxDepthIndex, 16);
        if( minDepth < 0)
            continue;
        if( defaultToDependent)
            minDepth = otherDepth;

        if( (info->flags & kDisplayModeSafeFlag)
         && (info->nominalWidth > biggest4By3)
         && (ratioOver(((float)info->nominalWidth) / ((float)info->nominalHeight), 4.0 / 3.0) <= 1.03125)) {
            biggest4By3 = info->nominalWidth;
            connectRef->default4By3Mode = mode;
        }

        if( timingID && tinf && !defaultToDependent
        && (modetinf = CFDictionaryGetValue( tinf, (const void *) timingID ))) {
            tinfRec = (DMDisplayTimingInfoRec *) CFDataGetBytePtr(modetinf);
            rQuality = tinfRec->timingInfoRelativeQuality;
            rDefault = tinfRec->timingInfoRelativeDefault;
        } else
            rQuality = rDefault = 0;

        if( (info->nominalWidth < kAquaMinWidth) || (info->nominalHeight < kAquaMinHeight))
            rDefault--;
        else if (!defaultToDependent && (0 != (info->flags & kDisplayModeDefaultFlag)))
            rDefault++;

        if( !bestMode)
            better = true;
        else {
#if 1
            if( (!defaultToDependent)
            && (bestInfo.flags & kDisplayModeSafeFlag)
            && (0 == (info->flags & kDisplayModeSafeFlag)))
                continue;
#else
            if( 0 == (info->flags & kDisplayModeSafeFlag))
                continue;
#endif
            if( rDefault < bestDefault)
                continue;
            better = (rDefault > bestDefault);

            if( !better) {

                if( (info->nominalWidth == bestInfo.nominalWidth)
                        && (info->nominalHeight == bestInfo.nominalHeight)) {

                    if( defaultToDependent && (0 == (info->flags & kDisplayModeSafeFlag)) )
                        better = (info->refreshRate < (61 << 16))
                            && (info->refreshRate > bestInfo.refreshRate);
                    else
                        better = (info->refreshRate < desireRefresh)
                            && (info->refreshRate > bestInfo.refreshRate);

                } else {
                    if( !better && desireHPix && desireVPix) {
                        SInt32 delta1, delta2;

                        delta1 = ((abs(info->nominalWidth - ((SInt32)desireHPix) ))
                                    + abs(info->nominalHeight - ((SInt32)desireVPix) ));
                        delta2 = (abs(bestInfo.nominalWidth - ((SInt32)desireHPix) )
                                    + abs(bestInfo.nominalHeight - ((SInt32)desireVPix) ));
                        better = (delta1 < delta2);
                    }
                }
            }
        }

        if( better) {
            bestMode = mode;
            bestQuality = rQuality;
            bestDefault = rDefault;
            bestInfo = *info;
            bestDepth = minDepth;
        }
    }

    if( bestMode) {

        connectRef->defaultMode = bestMode;
        if( !defaultToDependent
          && IOFBShouldDefaultDeep( connectRef)
          && (bestInfo.maxDepthIndex > bestDepth))
            bestDepth++;
        connectRef->defaultDepth = bestDepth;

        connectRef->defaultNot4By3 = (ratioOver(((float)bestInfo.nominalWidth) / ((float)bestInfo.nominalHeight), 4.0 / 3.0) > 1.03125);

        err = kIOReturnSuccess;
    } else
        err = IOFBGetCurrentDisplayModeAndDepth( connectRef->connect,
                    &connectRef->defaultMode, &connectRef->defaultDepth );

    return( err );
}

kern_return_t
IOFBGetDefaultDisplayMode( io_connect_t connect,
	IODisplayModeID	* displayMode, IOIndex * displayDepth )
{
    IOFBConnectRef connectRef;

    connectRef = IOFBConnectToRef( connect);
    if( !connectRef)
        return( kIOReturnBadArgument );

    *displayMode = connectRef->defaultMode;
    *displayDepth = connectRef->defaultDepth;

    return( kIOReturnSuccess );
}


static kern_return_t
IOFBInstallScaledMode( IOFBConnectRef connectRef,
                       IOFBDisplayModeDescription * desc,
                       UInt32 width, UInt32 height, IOOptionBits flags )
{
    desc->timingInfo.detailedInfo.v2.scalerFlags      = flags;
    desc->timingInfo.detailedInfo.v2.horizontalScaled = width;
    desc->timingInfo.detailedInfo.v2.verticalScaled   = height;

    return( IOFBInstallMode( connectRef, 0xffffffff, &desc->info, &desc->timingInfo, 0 ));
}

static Boolean
IOFBCheckScaleDupMode( IOFBConnectRef connectRef, IOFBDisplayModeDescription * desc )
{
    CFDictionaryRef	       dict;
    CFDataRef		       data;
    CFIndex		       i, modeCount;
    IODisplayModeInformation * info;
    Boolean		       dup = false;

    modeCount = CFArrayGetCount( connectRef->modesArray );

    for( i = 0; (i < modeCount) && !dup; i++ ) {

        dict = CFArrayGetValueAtIndex( connectRef->modesArray, i );
        if( !dict)
            continue;
        data = (CFDataRef) CFDictionaryGetValue( dict, CFSTR(kIOFBModeDMKey) );
        if( !data)
            continue;
        info = (IODisplayModeInformation *) CFDataGetBytePtr(data);
    
        do {
    
            if( 0 == (kDisplayModeValidFlag & info->flags))
                continue;
            if( kDisplayModeBuiltInFlag & info->flags)
                continue;
            if( info->nominalWidth < (desc->info.nominalWidth - 20))
                continue;
            if( info->nominalWidth > (desc->info.nominalWidth + 20))
                continue;
            if( info->nominalHeight < (desc->info.nominalHeight - 20))
                continue;
            if( info->nominalHeight > (desc->info.nominalHeight + 20))
                continue;
    
            dup = true;
    
        } while( false );
    }

    return( dup );
}

static kern_return_t
IOFBInstallScaledResolution( IOFBConnectRef connectRef,
                       VDScalerInfoRec * scalerInfo,
                       IOFBDisplayModeDescription * desc,
                       float nativeWidth, float nativeHeight,
                       float width, float height,
                       IOOptionBits flags )
{
    UInt32		  need = 0;
    float	          aspectDiff;
    float		  ratio;

    if( width < 640.0)
        return( __LINE__ );
    if( height < 480.0)
        return( __LINE__ );

    if( width > scalerInfo->csMaxHorizontalPixels)
        return( __LINE__ );
    if( height > scalerInfo->csMaxVerticalPixels)
        return( __LINE__ );

    if( width < nativeWidth)
        need |= kScaleCanUpSamplePixelsMask;
    else if( width != nativeWidth)
        need |= kScaleCanDownSamplePixelsMask;
    if( height < nativeHeight)
        need |= kScaleCanUpSamplePixelsMask;
    else if( height != nativeHeight)
        need |= kScaleCanDownSamplePixelsMask;

    if( need != (need & scalerInfo->csScalerFeatures))
        return( __LINE__ );

    aspectDiff = ratioOver( nativeWidth / nativeHeight, width / height );

    if( 0 == (kScaleInstallAlways & flags)) {

        ratio = (width / nativeWidth);
        if( (ratio < 1.18) && (ratio > 0.82))
            return( __LINE__ );
        ratio = (height / nativeHeight);
        if( (ratio < 1.18) && (ratio > 0.82))
            return( __LINE__ );
        if( aspectDiff > 2.0)
            return( __LINE__ );
    }

    desc->info.nominalWidth = 0xfffffffe & ((UInt32) ceil(width));
    desc->info.nominalHeight = 0xfffffffe & ((UInt32) ceil(height));
//    desc->info.refreshRate = (0 << 16);
    desc->info.maxDepthIndex = desc->info.maxDepthIndex;

    desc->info.flags = kDisplayModeValidFlag | kDisplayModeSafeFlag;

    if( (0 == (kScaleInstallAlways & flags)) && IOFBCheckScaleDupMode( connectRef, desc))
        return( __LINE__ );

    if( aspectDiff > 1.03125)
        desc->info.flags |= kDisplayModeNotPresetFlag;

    if( 0 == (kScaleStretchOnlyMask & scalerInfo->csScalerFeatures))
        IOFBInstallScaledMode( connectRef, desc, width, height, 0 );

    if( (0 == (kScaleInstallNoStretch & flags)) && (aspectDiff > 1.03125) && (aspectDiff < 1.5)) {
        desc->info.flags |= kDisplayModeStretchedFlag;
        IOFBInstallScaledMode( connectRef, desc, width, height, kScaleStretchToFitMask );
    }

    return( 0 );
}

static Boolean
IOFBLookScaleBaseMode( IOFBConnectRef connectRef, IOFBDisplayModeDescription * scaleBase,
                        IOFBDisplayModeDescription * scaleDesc )
{
    Boolean found = false;

#if LOG
    printf("%dx%d == %dx%d %08lx %08lx\n",
	   scaleBase->timingInfo.detailedInfo.v2.horizontalActive,
	   scaleBase->timingInfo.detailedInfo.v2.verticalActive,
	   scaleBase->timingInfo.detailedInfo.v2.horizontalScaled,
	   scaleBase->timingInfo.detailedInfo.v2.verticalScaled,
	   scaleBase->info.flags, scaleBase->timingInfo.flags);
#endif

    do {
        if( 0 == (kIODetailedTimingValid & scaleBase->timingInfo.flags))
            continue;

        if( (kDisplayModeValidFlag | kDisplayModeSafeFlag) !=
            ((kDisplayModeValidFlag | kDisplayModeSafeFlag) & scaleBase->info.flags))
            continue;

        if( (kDisplayModeBuiltInFlag
            | kDisplayModeNeverShowFlag
            | kDisplayModeStretchedFlag
            | kDisplayModeNotGraphicsQualityFlag
            | kDisplayModeNotPresetFlag) & scaleBase->info.flags)
            continue;

        if( timingApple_FixedRateLCD != scaleBase->timingInfo.appleTimingID)
            continue;

        if( scaleBase->timingInfo.detailedInfo.v2.horizontalScaled
            != scaleBase->timingInfo.detailedInfo.v2.horizontalActive)
            continue;
        if( scaleBase->timingInfo.detailedInfo.v2.verticalScaled
            != scaleBase->timingInfo.detailedInfo.v2.verticalActive)
            continue;

        if( scaleBase->timingInfo.detailedInfo.v2.horizontalActive
            < scaleDesc->timingInfo.detailedInfo.v2.horizontalActive)
            continue;
        if( scaleBase->timingInfo.detailedInfo.v2.verticalActive
            < scaleDesc->timingInfo.detailedInfo.v2.verticalActive)
            continue;
#if LOG
        printf("choosing\n");
#endif
        found = true;
        *scaleDesc = *scaleBase;
        scaleDesc->timingInfo.appleTimingID = 0;
        scaleDesc->timingInfo.flags = kIODetailedTimingValid;

    } while( false );

    return( found );
}

static kern_return_t
IOFBInstallScaledModes( IOFBConnectRef connectRef, IOFBDisplayModeDescription * scaleBase )
{
    IOReturn			err = kIOReturnSuccess;
    CFDataRef 			data;
    CFDictionaryRef		ovr;
    CFArrayRef			array, array1, array2 = 0;
    CFMutableArrayRef		copyArray = 0;
    CFIndex			count, ovrCount = 0;
    VDScalerInfoRec *		scalerInfo;
    SInt32			i;
    float			h, v, nh, nv;
    Boolean			displayNot4By3;

    if( kOvrFlagDisableScaling & connectRef->ovrFlags)
        return( kIOReturnSuccess );

    data = IORegistryEntryCreateCFProperty( connectRef->framebuffer, CFSTR(kIOFBScalerInfoKey),
						kCFAllocatorDefault, kNilOptions );
    if( !data)
        return( kIOReturnSuccess );

    scalerInfo = (VDScalerInfoRec *) CFDataGetBytePtr(data);

    if( true ) {
        IODetailedTimingInformationV2 preflightTiming;
        unsigned int		      len;
    
        len = sizeof( IODetailedTimingInformationV2);
        err = io_connect_method_structureI_structureO( connectRef->connect, 17, /*index*/
                        (void *) &scaleBase->timingInfo.detailedInfo.v2, len, (void *) &preflightTiming, &len);
        if( kIOReturnSuccess != err) {
#if LOG
            printf("Scale base mode preflight failed\n");
#endif
            return( err );
        }
    }

    array1 = CFDictionaryGetValue( gIOGraphicsProperties, CFSTR("scale-resolutions") );
    if( !array1)
        return( kIOReturnSuccess );

    ovr = connectRef->overrides;
    if( ovr)
        array2 = CFDictionaryGetValue( ovr, CFSTR("scale-resolutions") );
    if( array2)
        copyArray = CFArrayCreateMutableCopy( kCFAllocatorDefault, 0, array2 );
    if( copyArray) {
        ovrCount = CFArrayGetCount(copyArray);
        if( 0 == (kOvrFlagDisableGenerated & connectRef->ovrFlags))
            CFArrayAppendArray( copyArray, array1, CFRangeMake( 0, CFArrayGetCount(array1) ));
        array = copyArray;
    } else
        array = CFRetain(array1);

    scalerInfo = (VDScalerInfoRec *) CFDataGetBytePtr(data);

    nh = (float) scaleBase->timingInfo.detailedInfo.v2.horizontalActive;
    nv = (float) scaleBase->timingInfo.detailedInfo.v2.verticalActive;

#if LOG
    printf("Scaling mode (%f,%f)\n", nh, nv);
#endif

    IOFBInstallScaledResolution( connectRef, scalerInfo, scaleBase, nh, nv, nh / 2.0, nv / 2.0, false );

    displayNot4By3 = (ratioOver(nh / nv, 4.0 / 3.0) > 1.03125);

    count = CFArrayGetCount(array);
    for( i = 0; i < count; i++) {
        CFTypeRef obj;
        IOReturn r;
        IOOptionBits flags;

        obj = CFArrayGetValueAtIndex(array, i);
        if( CFNumberGetTypeID() == CFGetTypeID(obj)) {
            SInt32	value;
            CFNumberGetValue( (CFNumberRef) obj, kCFNumberSInt32Type, &value );
            h     = (float)(value & 0xffff);
            v     = (float)(value >> 16);

            flags = (i < ovrCount) ? kScaleInstallAlways | kScaleInstallNoStretch : 0;

        } else if( CFDataGetTypeID() == CFGetTypeID(obj)) {
            UInt32 * value = (UInt32 *) CFDataGetBytePtr((CFDataRef) obj);
            h     = (float) value[0];
            v     = (float) value[1];
            flags = value[2];

        } else
            continue;
        
        if( v) {

            if( (h != (nh / 2.0)) || (v != (nv / 2.0))) {
                r = IOFBInstallScaledResolution( connectRef, scalerInfo, scaleBase, 
                                                    nh, nv, h, v, flags );
#if LOG
                printf("%f x %f: %d\n", h, v, r);
#endif
            }

        } else {

            if( displayNot4By3) {
                r = IOFBInstallScaledResolution( connectRef, scalerInfo, scaleBase,
                                                    nh, nv, h, h / 4.0 * 3.0, flags );
#if LOG
                printf("%f x %f: %d\n", h, h / 4.0 * 3.0, r);
#endif
            }
            if( h != (nh / 2.0)) {
                r = IOFBInstallScaledResolution( connectRef, scalerInfo, scaleBase,
                                                    nh, nv, h, h / nh * nv, flags );
#if LOG
                printf("%f x %f: %d\n", h, h / nh * nv, r);
#endif
            }
        }
    }

    CFRelease( data );
    CFRelease( array );

    return( err );
}

static Boolean
IOFBStandardEDIDTimings( UInt32   appleTimingID,
                         EDID *   edid,
                         UInt32 * flags )
{
    UInt32			estBit, i;
    CFDataRef			apple2DDC;
    Boolean			supported = false;

    enum { kSetFlags = (kDisplayModeValidFlag | kDisplayModeSafeFlag) };

    typedef struct {
        UInt32	timingID;
        UInt8	spare;
        UInt8	establishedBit;
        UInt16	standardTiming;
    } TimingToEDID;

    const TimingToEDID * lookTiming;
    TimingToEDID * timingToEDID;
    UInt32 numEDIDEntries;

    apple2DDC = CFDictionaryGetValue( gIOGraphicsProperties, CFSTR("appleDDC") );
    if( !apple2DDC)
        return( false );

    timingToEDID = (TimingToEDID *) CFDataGetBytePtr(apple2DDC);
    numEDIDEntries = CFDataGetLength(apple2DDC) / sizeof(TimingToEDID);

    for( lookTiming = timingToEDID;
        (!supported) && ((lookTiming - timingToEDID) < numEDIDEntries);
        lookTiming++ ) {

        if( lookTiming->timingID == appleTimingID) {
            estBit = lookTiming->establishedBit;
            if( estBit != 0xff)
                supported = (0 != (edid->establishedTimings[ estBit / 8 ]
                                    & (1 << (estBit % 8))));

            for( i = 0; (!supported) && (i < 8); i++ )
                supported = (lookTiming->standardTiming
                            == edid->standardTimings[i] );
        }
    }
    if( supported)
        *flags = ((*flags) & ~kDisplayModeSafetyFlags) | kSetFlags;

    return( supported );
}

static kern_return_t
IOFBCreateDisplayModeInformation(
        IOFBConnectRef			connectRef,
	IODisplayModeID			displayMode,
	IOFBDisplayModeDescription *	allInfo,
        UInt32 *			driverFlags )
{
    kern_return_t		kr;
    IOByteCount			len;
    CFDataRef			edidData;
    EDID *			edid = 0;
    CFDataRef			data;
    CFDictionaryRef		ovr = 0;
    CFDictionaryRef		tovr;
    CFDataRef			modetovr;
    DMTimingOverrideRec *	tovrRec;
    IOAppleTimingID		appleTimingID;
    CFNumberRef			num;
    UInt8			manufacturerFlag;

    len = sizeof(IOFBDisplayModeDescription);
    kr = IOConnectMethodScalarIStructureO( connectRef->connect, 5, /*index*/
                    1, &len, displayMode, allInfo );

    appleTimingID = allInfo->timingInfo.appleTimingID;

    if( driverFlags)
        *driverFlags = 0;

    if( kr == kIOReturnSuccess) do {

#if LOG
        printf("%d x %d @ %d (%d,%d): %08lx %08lx\n", 
            allInfo->info.nominalWidth, allInfo->info.nominalHeight,
            allInfo->info.refreshRate >> 16, displayMode, appleTimingID,
            allInfo->info.flags, allInfo->timingInfo.flags);
#endif

        if( driverFlags)
            *driverFlags = allInfo->info.flags;

        switch( appleTimingID ) {
            case timingAppleNTSC_ST:
            case timingAppleNTSC_FF:
            case timingAppleNTSC_STconv:
            case timingAppleNTSC_FFconv:
                allInfo->info.flags |= kDisplayModeTelevisionFlag;
                manufacturerFlag = kAppleNTSCManufacturerFlag;
                break;

            case timingApplePAL_ST:
            case timingApplePAL_FF:
            case timingApplePAL_STconv:
            case timingApplePAL_FFconv:
                allInfo->info.flags |= kDisplayModeTelevisionFlag;
                manufacturerFlag = kApplePALManufacturerFlag;
                break;

            default:
                manufacturerFlag = 0x00;
                break;
        }

        if( connectRef->suppressRefresh
          && (kDisplayModeValidFlag & allInfo->info.flags)
          && (allInfo->info.refreshRate)
          && ((allInfo->info.refreshRate < 0x3b8000) || (allInfo->info.refreshRate > 0x3c8000)))
            connectRef->suppressRefresh = false;

        ovr = connectRef->overrides;
        if( !ovr)
            continue;

        if( kDisplayModeBuiltInFlag & allInfo->info.flags)
            continue;

        tovr = CFDictionaryGetValue( ovr, CFSTR("tovr") );
        if( tovr) {
            if( appleTimingID && (modetovr = CFDictionaryGetValue( tovr, (const void *) appleTimingID ))) {
                tovrRec = (DMTimingOverrideRec *) CFDataGetBytePtr(modetovr);
                allInfo->info.flags &= ~tovrRec->timingOverrideClearFlags;
                allInfo->info.flags |= tovrRec->timingOverrideSetFlags;
                continue;
            }
        }

        if( kOvrFlagDisableNonScaled & connectRef->ovrFlags) {
            if( (displayMode > 0) && (0 == (kDisplayModeDefaultFlag & allInfo->info.flags)))
                allInfo->info.flags &= ~kDisplayModeSafetyFlags;
        }

#if 1
        if( kDisplayModeSafetyFlags & allInfo->info.flags)
            continue;
#endif
        if( displayMode < 0)					// programmed mode
            continue;
        if( appleTimingID == timingApple_FixedRateLCD)		// 2488698
            continue;
        if( appleTimingID == timingApple_0x0_0hz_Offline)
            continue;
#if 1
        if( kDisplayModeNeverShowFlag & allInfo->info.flags)
            continue;
#endif
        if( CFDictionaryGetValue( ovr, CFSTR(kIODisplayIsDigitalKey)))
            // Install no detailed timings from digital displays
            continue;

        edidData = CFDictionaryGetValue( ovr, CFSTR(kIODisplayEDIDKey));
        if( edidData)
            edid = (EDID *) CFDataGetBytePtr(edidData);
        if( appleTimingID && edid) {
            if( IOFBStandardEDIDTimings( appleTimingID, edid, &allInfo->info.flags ))
                continue;
        }

        if( (kDisplayAppleVendorID == connectRef->displayVendor)
            && edid && edid->version
            && ((edid->version > 1) || (edid->revision >= 3))) {

            if( manufacturerFlag & edid->establishedTimings[2]) {
                allInfo->info.flags |= kDisplayModeValidFlag | kDisplayModeSafeFlag;
                continue;
            }
        }

        if( kDisplayModeInterlacedFlag & allInfo->info.flags)
            continue;

        data = CFDictionaryGetValue( ovr, CFSTR("trng") );

        if( data && (kIODetailedTimingValid & allInfo->timingInfo.flags)) {

            Boolean     ok = true;

            num = CFDictionaryGetValue( ovr, CFSTR("sync") );
            if( num) {
                UInt32 hSyncMask, hSyncValue, vSyncMask, vSyncValue;

                CFNumberGetValue( num, kCFNumberSInt32Type, &vSyncValue );
                hSyncMask  = 0xff & (vSyncValue >> 24);
                hSyncValue = 0xff & (vSyncValue >> 16);
                vSyncMask  = 0xff & (vSyncValue >> 8);
                vSyncValue = 0xff & (vSyncValue >> 0);
                ok = (hSyncValue == (allInfo->timingInfo.detailedInfo.v2.horizontalSyncConfig & hSyncMask))
                    && (vSyncValue == (allInfo->timingInfo.detailedInfo.v2.verticalSyncConfig & vSyncMask));
            }

            if( ok)
                ok = IOCheckTimingWithRange( CFDataGetBytePtr(data), &allInfo->timingInfo.detailedInfo.v2 );

            allInfo->info.flags &= ~kDisplayModeSafetyFlags;
            if( ok)
                allInfo->info.flags |= kDisplayModeValidFlag | kDisplayModeSafeFlag;
        }

    } while( false );

    return( kr );
}

kern_return_t
IOFBGetDisplayModeInformation( io_connect_t connect,
	IODisplayModeID		displayMode,
	IODisplayModeInformation * out )
{
    kern_return_t	  kr = kIOReturnSuccess;
    IOFBConnectRef	  connectRef;
    CFDataRef		  data;
    CFDictionaryRef	  dict;
    IOFBDisplayModeDescription allInfo;
    IODisplayModeInformation * info;

    connectRef = IOFBConnectToRef( connect);
    if( !connectRef)
        return( kIOReturnBadArgument );

    dict = CFDictionaryGetValue( connectRef->modes, (const void *) displayMode );
    if( dict && (data = CFDictionaryGetValue( dict, CFSTR(kIOFBModeDMKey) )))
        info = (IODisplayModeInformation *) CFDataGetBytePtr(data);
    else {
        kr = IOFBCreateDisplayModeInformation( connectRef, displayMode, &allInfo, NULL );
        info = &allInfo.info;
    }


    if( kr == kIOReturnSuccess) {
        bcopy( info, out, sizeof( IODisplayModeInformation));
        if( (displayMode == connectRef->defaultMode) && (out->flags & kDisplayModeValidFlag))
            out->flags |= kDisplayModeDefaultFlag;
        else
            out->flags &= ~kDisplayModeDefaultFlag;

        if(true && connectRef->suppressRefresh)
            out->refreshRate = 0;
    }

    return( kr );
}


IOFBConnectRef IOFBConnectToRef( io_connect_t connect )
{
    return((IOFBConnectRef) CFDictionaryGetValue( gConnectRefDict, (void *) connect ));
}

static kern_return_t
IOFramebufferServerOpen( mach_port_t connect )
{
    mach_port_t			masterPort;
    IOFBConnectRef		connectRef, next;
    IODisplayModeID		mode, otherMode;
    IOIndex			depth, minDepth, otherDepth;
    IODisplayModeID		startMode;
    IOIndex			startDepth;
    UInt32			startFlags = 0;
    IOReturn			err;
    IODisplayModeInformation *	otherInfo, info;
    CFDictionaryRef		dict;
    CFDataRef			data;
    CFNumberRef			num;
    struct stat			statResult;
    Boolean			firstBoot;

    if( !gIOGraphicsProperties) {

        data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
                                    (const UInt8 *) gIOGraphicsPropertiesData,
                                    strlen(gIOGraphicsPropertiesData) + 1,
                                    kCFAllocatorNull );

        if( data) {
            gIOGraphicsProperties = CFPropertyListCreateFromXMLData(
                    kCFAllocatorDefault, data,
                    kCFPropertyListImmutable, (CFStringRef *) NULL );
            CFRelease(data);
        }
    }

    do {

        err = kIOReturnNoMemory;

        IOMasterPort( MACH_PORT_NULL, &masterPort );

        if( !gConnectRefDict)
            gConnectRefDict = CFDictionaryCreateMutable(
                kCFAllocatorDefault, (CFIndex) 0,
                (CFDictionaryKeyCallBacks *) 0,
                (CFDictionaryValueCallBacks *) 0 );	//&kCFTypeDictionaryValueCallBacks
        if( !gConnectRefDict)
            return( kIOReturnNoMemory );
    
        connectRef = calloc( 1, sizeof( struct IOFBConnect));
        if( !connectRef)
            continue;
    
        connectRef->connect = connect;
        err = IOConnectGetService( connect, &connectRef->framebuffer );
        if( kIOReturnSuccess != err)
            continue;

        CFDictionarySetValue( gConnectRefDict, (const void *) connect, connectRef );

        num = IORegistryEntryCreateCFProperty( connectRef->framebuffer, CFSTR(kIOFBDependentIDKey),
                                                kCFAllocatorDefault, kNilOptions );
        if( num) { 
            CFNumberGetValue( num, kCFNumberSInt64Type, &connectRef->dependentID );
            CFRelease(num);
        }
        num = IORegistryEntryCreateCFProperty( connectRef->framebuffer, CFSTR(kIOFBDependentIndexKey),
                                                kCFAllocatorDefault, kNilOptions );
        if( num) { 
            CFNumberGetValue( num, kCFNumberSInt32Type, &connectRef->dependentIndex );
            CFRelease(num);
        } else
            connectRef->dependentID = 0;

        // add to dependent list
        if( connectRef->dependentID) {
            for( next = gAllConnects; next; next = next->next) {
                if( next->dependentID == connectRef->dependentID) {
    
                    if( next->nextDependent)
                        connectRef->nextDependent = next->nextDependent;
                    else
                        connectRef->nextDependent = next;
                    next->nextDependent = connectRef;
                    break;
                }
            }
        }
        // add to all list
        connectRef->next = gAllConnects;
        gAllConnects = connectRef;
        // --
    
        connectRef->notifyPort = IONotificationPortCreate( masterPort );
        if( !connectRef->notifyPort)
            return( kIOReturnError );

        IOConnectSetNotificationPort( connect, 0,
                        IONotificationPortGetMachPort( connectRef->notifyPort ), 0);

        err = IOServiceAddInterestNotification(
                connectRef->notifyPort,
                connectRef->framebuffer,
                kIOGeneralInterest,
                &IOFBInterestCallback, connectRef,
                &connectRef->interestNotifier );

        IOFBUpdateConnectState( connectRef );
        err = IOFBRebuild( connectRef );
        if( kIOReturnSuccess != err)
            continue;

    } while( false );

    firstBoot = ((0 != stat( kIOFirstBootFlagPath, &statResult)) && (ENOENT == errno));
    if( firstBoot) {
        int ifd;
        if( (ifd = open( kIOFirstBootFlagPath, O_CREAT | O_RDWR, 0)) >= 0) {
            fchmod(ifd, DEFFILEMODE);
            close(ifd);
        }
    }

    do {

        err = IOFBGetCurrentDisplayModeAndDepth( connect, &mode, &depth );
        startMode = mode;
        startDepth = depth;
        if( err)
            continue;

        err = IOFBGetDisplayModeInformation( connect, startMode, &info);
        if( err)
            continue;

        startFlags = info.flags;
        if( (info.nominalWidth  < kAquaMinWidth)
          || (info.nominalHeight < kAquaMinHeight)) {
            err = kIOReturnNoResources;
            continue;
        }

        if( !connectRef->relaunch) {
            if( connectRef->make4By3 && connectRef->default4By3Mode
                && ratioOver(((float)info.nominalWidth) / ((float)info.nominalHeight), 4.0 / 3.0) > 1.03125) {
                err = kIOReturnNoResources;
                continue;
            }
    
            if( connectRef->defaultToDependent
            && (kIOReturnSuccess == IOFBGetCurrentDisplayModeAndDepth( connectRef->nextDependent->connect,
                                                                            &otherMode, &otherDepth ))
            && (dict = CFDictionaryGetValue( connectRef->nextDependent->modes, (const void *) otherMode ))
            && (data = CFDictionaryGetValue( dict, CFSTR(kIOFBModeDMKey) ))) {
    
                otherInfo = (IODisplayModeInformation *) CFDataGetBytePtr(data);
                if( (otherInfo->nominalWidth  != info.nominalWidth)
                || (otherInfo->nominalHeight != info.nominalHeight)) {
                    err = kIOReturnNoResources;
                    continue;
                }
                startDepth = otherDepth;
            }
        }

        // make sure it does >= 16bpp
        minDepth = IOFBIndexForPixelBits( connectRef, startMode, info.maxDepthIndex, 16 );
        if( minDepth < 0) {
            err = kIOReturnNoResources;
            continue;
        }

        if( firstBoot) {
            // default depth on first boot
            startDepth = minDepth;
            if( IOFBShouldDefaultDeep( connectRef) && (info.maxDepthIndex > minDepth))
                startDepth++;
        } else if( startDepth < minDepth)
            startDepth = minDepth;

    } while( false );

    if( err
        || (startMode == kDisplayModeIDBootProgrammable)
        || ((startFlags & kDisplayModeValidFlag)
                        != kDisplayModeValidFlag) ) {
        // go to default
        if( connectRef->defaultMode) {
            startMode = connectRef->defaultMode;
            startDepth = connectRef->defaultDepth;
        }
        if( connectRef->make4By3 && connectRef->default4By3Mode)
            startMode = connectRef->default4By3Mode;
    }

    if( (startMode != mode) || (startDepth != depth)) {
        IOFBSetDisplayModeAndDepth( connect, startMode, startDepth );
        IOFBSetStartupDisplayModeAndDepth( connect, startMode, startDepth );
    }

    return( kIOReturnSuccess );
}

kern_return_t
IOFBGetConnectState( io_connect_t connect, IOOptionBits * state )
{
    IOFBConnectRef connectRef = IOFBConnectToRef( connect );

    if( !connectRef)
        return( kIOReturnBadArgument );

    *state = connectRef->state;

    return( kIOReturnSuccess );
}


// Mask of pixel formats available in mode and depth

kern_return_t
IOFBGetPixelFormats( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex			depth,
	UInt32 * 		mask )
{
    *mask = 1;
    return( kIOReturnSuccess);
}

kern_return_t
IOFBGetPixelInformation( io_connect_t connect,
	IODisplayModeID 	displayMode,
	IOIndex 		depth,
        IOPixelAperture		aperture,
	IOPixelInformation *	pixelInfo )
{
    IOByteCount	len;
    kern_return_t kr;

    len = sizeof( IOPixelInformation);

    kr = IOConnectMethodScalarIStructureO( connect, 1, /*index*/
                    3, &len,
                    displayMode, depth, aperture,
                    pixelInfo );
#if LOG
    if( kIOReturnSuccess != kr)
        printf("IOFBGetPixelInformation(%ld) %x\n", displayMode, kr);
#endif
    return( kr );
}

kern_return_t
IOFBSetDisplayModeAndDepth( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex 		depth )
{
    return( IOConnectMethodScalarIScalarO( connect, 4, /*index*/
                    2, 0,
                    displayMode, depth ));
}

kern_return_t
IOFBSetStartupDisplayModeAndDepth( io_connect_t connect,
	IODisplayModeID		displayMode,
	IOIndex 		depth )
{
    return( IOConnectMethodScalarIScalarO( connect, 3, /*index*/
                    2, 0,
                    displayMode, depth ));
}

kern_return_t
IOFBSetNewCursor( io_connect_t connect,
	void *			cursor,
	IOIndex			frame,
	IOOptionBits		options )
{
    return( IOConnectMethodScalarIScalarO( connect, 10, /*index*/
                    3, 0,
                    cursor, frame, options ));
}

kern_return_t
IOFBSetCursorVisible( io_connect_t connect,
	int			visible )
{
    return( IOConnectMethodScalarIScalarO( connect, 12, /*index*/
                    1, 0,
                    visible ));
}

kern_return_t
IOFBSetCursorPosition( io_connect_t connect,
	long int		x,
	long int		y )
{
    return( IOConnectMethodScalarIScalarO( connect, 13, /*index*/
                    2, 0,
                    x, y ));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

CFDictionaryRef
IOFBCreateDisplayModeDictionary( io_service_t framebuffer,
	IODisplayModeID		displayMode )
{
    CFDictionaryRef	infoDict;
    CFStringRef		string;
    CFDictionaryRef	modeDict = 0;
    char		keyBuf[12];

    infoDict = IORegistryEntryCreateCFProperty( framebuffer, CFSTR(kIOFramebufferInfoKey),
						kCFAllocatorDefault, kNilOptions );
    if( infoDict ) {
        sprintf( keyBuf, "%lx", displayMode );
        string = CFStringCreateWithCString( kCFAllocatorDefault, keyBuf,
                                            kCFStringEncodingMacRoman );
	if( string) {
            modeDict = CFDictionaryGetValue( infoDict, string );
	    CFRelease( string );
	}
        if( modeDict)
            CFRetain( modeDict );
        CFRelease( infoDict );
    }

    return( modeDict );
}

CFDictionaryRef
IOFBGetPixelInfoDictionary( 
	CFDictionaryRef		modeDictionary,
	IOIndex 		depth,
	IOPixelAperture		aperture )
{
    char		keyBuf[12];
    CFStringRef		string;
    CFDictionaryRef	pixelInfo = 0;

    if( !modeDictionary)
	return( 0 );

    sprintf( keyBuf, "%lx", depth + (aperture << 16) );
    string = CFStringCreateWithCString( kCFAllocatorDefault, keyBuf,
                                        kCFStringEncodingMacRoman );
    if( string) {
        pixelInfo = CFDictionaryGetValue( modeDictionary, string );
        CFRelease( string );
    }

    return( pixelInfo );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOFBGetInterruptSemaphore( io_connect_t connect,
                           IOSelect interruptType,
                           semaphore_t * semaphore )
{
    return( IOConnectMethodScalarIScalarO( connect, 15, /*index*/
                    1, 1,
                    interruptType,
                    semaphore ));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <IOKit/graphics/IOGraphicsInterface.h>

#ifndef NO_CFPLUGIN

struct _BlitterVars {
    IOGraphicsAcceleratorInterface ** interface;
    IOBlitterPtr		copyProc;
    IOBlitterPtr		fillProc;
    IOBlitterPtr		memCopyProc;
    IOBlitSurface		dest;
    void *			sid;
    IOBlitterPtr		copyRegionProc;
};
typedef struct _BlitterVars _BlitterVars;

kern_return_t
IOPSAllocateBlitEngine( io_service_t service,
		void ** blitterRef, int * quality)
{
    IOReturn				err = kIOReturnSuccess;
    _BlitterVars *			vars;
    IOGraphicsAcceleratorInterface **	interface = 0;
    
    vars = (_BlitterVars *) calloc( 1, sizeof( _BlitterVars ));
    if( !vars)
	return( kIOReturnNoMemory);

    do {
        err = IOCreatePlugInInterfaceForService( service,
                            kIOGraphicsAcceleratorTypeID,
                            kIOGraphicsAcceleratorInterfaceID,
                            (IOCFPlugInInterface ***)&interface, (SInt32 *) quality );
        if( err)
	    continue;
        vars->interface = interface;

        if( (*interface)->SetDestination) {
            err = (*interface)->SetDestination(interface,
                                        kIOBlitFramebufferDestination, NULL);
            if( err)
                continue;
        }
        err = (*interface)->GetBlitter(interface,
                                    kIOBlitAllOptions,
                                    (kIOBlitTypeCopyRects | kIOBlitCopyOperation),
                                    kIOBlitSourceDefault,
                                    &vars->copyProc);
        if( err)
            continue;
        err = (*interface)->GetBlitter(interface,
                                    kIOBlitAllOptions,
                                    (kIOBlitTypeRects | kIOBlitCopyOperation),
                                    kIOBlitSourceSolid,
                                    &vars->fillProc);
        if( err)
            continue;


        if( kIOReturnSuccess != (*interface)->GetBlitter(interface,
                                    kIOBlitAllOptions,
                                    (kIOBlitTypeCopyRegion | kIOBlitTypeOperationType0),
                                    kIOBlitSourceFramebuffer,
                                    &vars->copyRegionProc))
            vars->copyRegionProc = 0;

        if( kIOReturnSuccess != (*interface)->GetBlitter(interface,
                                    kIOBlitAllOptions,
                                    (kIOBlitTypeCopyRects | kIOBlitCopyOperation),
                                    kIOBlitSourceMemory,
                                    &vars->memCopyProc))
            vars->memCopyProc = 0;
        
    } while( FALSE );

    if( err) {
        if (interface)
            IODestroyPlugInInterface((IOCFPlugInInterface **)interface);
        free( vars );
        vars = 0;
    }
    
    *blitterRef = (void *) vars;

    return( err);
}

kern_return_t
IOPSBlitReset( void * blitterRef)
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    kern_return_t	err = kIOReturnSuccess;

    if( interface)
        err = (*interface)->Reset(interface, kNilOptions);
    vars->sid = 0;

    return( err );
}

kern_return_t
IOPSBlitDeallocate( void * blitterRef)
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    kern_return_t	err;

    err = IODestroyPlugInInterface((IOCFPlugInInterface **)interface);
    free( vars );

    return( err );
}

kern_return_t
IOPSBlitIdle( void * blitterRef)
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    kern_return_t	err;

    err = (*interface)->WaitComplete(interface, kIOBlitWaitAll2D );

    return( err );
}


kern_return_t
IOFBSynchronize( void * blitterRef,
                UInt32 x, UInt32 y, UInt32 w, UInt32 h, UInt32 options )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface;
    IOReturn		err;

    if( !vars)
        return( kIOReturnBadArgument);
    interface = vars->interface;
    err = (*interface)->Synchronize(interface, options, x, y, w, h );

    return( err );
}

kern_return_t
IOFBBeamPosition( void * blitterRef, UInt32 options, SInt32 * position )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    IOReturn		err;

    err = (*interface)->GetBeamPosition(interface, options, position);

    return( err );
}


kern_return_t
IOPSBlitFill( void * blitterRef,
		int x, int y, int w, int h, int data )
{
    _BlitterVars *		vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    IOReturn			err;
    IOBlitRectangles		rects;

    if( vars->sid) {
        err = (*interface)->SetDestination(interface, kIOBlitFramebufferDestination, 0);
        vars->sid = 0;
        if( err)
            return( err );
    }

    rects.count = 1;
    rects.rects[0].x = x;
    rects.rects[0].y = y;
    rects.rects[0].width = w;
    rects.rects[0].height = h;

    err = (*vars->fillProc)(interface,
                    kNilOptions,
                    (kIOBlitTypeRects | kIOBlitCopyOperation),
                    (kIOBlitSourceSolid | kIOBlitDestFramebuffer),
                    &rects.operation,
                    (void *) data);
    
    if( kIOReturnSuccess == err)
        (*interface)->Flush(interface, kNilOptions);

    return( err );
}

kern_return_t
IOPSBlitInvert( void * blitterRef,
		int x, int y, int w, int h )
{
    _BlitterVars *		vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** interface = vars->interface;
    IOReturn			err;
    IOBlitRectangles		rects;

    if( vars->sid) {
        err = (*interface)->SetDestination(interface, kIOBlitFramebufferDestination, 0);
        vars->sid = 0;
        if( err)
            return( err );
    }

    rects.count = 1;
    rects.rects[0].x = x;
    rects.rects[0].y = y;
    rects.rects[0].width = w;
    rects.rects[0].height = h;

    err = (*vars->fillProc)(interface,
                    kNilOptions,
                    (kIOBlitTypeRects | kIOBlitCopyOperation),
                    (kIOBlitSourceSolid | kIOBlitDestFramebuffer),
                    &rects.operation,
                    (void *) 0xffffffff);
    
    if( kIOReturnSuccess == err)
        (*interface)->Flush(interface, kNilOptions);

    return( err );
}


kern_return_t
IOPSBlitCopy( void * blitterRef,
		int src_x, int src_y, int width, int height,
		int dst_x, int dst_y )
{
    return( IOFBBlitVRAMCopy( blitterRef, src_x, src_y, width, height,
				dst_x, dst_y, 1 * (kIOFBBlitBeamSync) ));
}

kern_return_t
IOFBBlitVRAMCopy( void * blitterRef,
                  int sourceX, int sourceY, int width, int height,
		  int x, int y, IOOptionBits options )
{
    _BlitterVars *			vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** 	interface = vars->interface;
    IOReturn				err;
    IOBlitCopyRectangles		rects;

    if( vars->sid) {
        err = (*interface)->SetDestination(interface, kIOBlitFramebufferDestination, 0);
        vars->sid = 0;
        if( err)
            return( err );
    }

    rects.count = 1;
    rects.rects[0].x = x;
    rects.rects[0].y = y;
    rects.rects[0].width = width;
    rects.rects[0].height = height;
    rects.rects[0].sourceX = sourceX;
    rects.rects[0].sourceY = sourceY;

    err = (*vars->copyProc)(interface,
                    options,
                    (kIOBlitTypeCopyRects | kIOBlitCopyOperation),
                    kIOBlitSourceDefault,
                    &rects.operation,
                    0);
    
    if( kIOReturnSuccess == err)
        (*interface)->Flush(interface, kNilOptions);

    return( err );
}

kern_return_t
IOFBBlitSurfaceCopy( void * blitterRef, IOOptionBits options, void * surfaceID,
                     IOAccelDeviceRegion * region, UInt32 surfaceX, UInt32 surfaceY )
{
    IOReturn				err = kIOReturnSuccess;
    _BlitterVars *			vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** 	interface = vars->interface;
    IOBlitCopyRegion 			op;

    if( 0 == vars->copyRegionProc)
        return( kIOReturnUnsupported );

    if( surfaceID != vars->sid) do {
        if( surfaceID) {
            err = (*interface)->AllocateSurface(interface, kIOBlitHasCGSSurface, &vars->dest, surfaceID);
            if( err)
                continue;
            err = (*interface)->SetDestination(interface, kIOBlitSurfaceDestination, &vars->dest);
        } else
            err = (*interface)->SetDestination(interface, kIOBlitFramebufferDestination, 0);

        if( err)
            continue;

        vars->sid = surfaceID;

    } while( false );

    if( err)
        return( err );

    op.region = region;
    op.deltaX = surfaceX;
    op.deltaY = surfaceY;

    err = (*vars->copyRegionProc)(interface,
                    options,
                    (kIOBlitTypeCopyRegion | kIOBlitTypeOperationType0),
                    kIOBlitSourceFramebuffer,
                    &op.operation,
                    (void *) 0);

    if( kIOReturnSuccess == err)
        (*interface)->Flush(interface, kNilOptions);

    return( err );
}

kern_return_t
IOFBBlitSurfaceSurfaceCopy( void * blitterRef, IOOptionBits options,
                            void * sourceSurfaceID, void * destSurfaceID,
                            IOAccelDeviceRegion * region, UInt32 surfaceX, UInt32 surfaceY )
{
    IOReturn				err = kIOReturnSuccess;
    _BlitterVars *			vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** 	interface = vars->interface;
    IOBlitCopyRegion 			op;

    if( 0 == vars->copyRegionProc)
        return( kIOReturnUnsupported );

    if( destSurfaceID != vars->sid) do {
        if( destSurfaceID) {
            err = (*interface)->AllocateSurface(interface, kIOBlitHasCGSSurface, &vars->dest, destSurfaceID);
            if( err)
                continue;
            err = (*interface)->SetDestination(interface, kIOBlitSurfaceDestination, &vars->dest);
        } else
            err = (*interface)->SetDestination(interface, kIOBlitFramebufferDestination, 0);

        if( err)
            continue;

        vars->sid = destSurfaceID;

    } while( false );

    if( err)
        return( err );

    op.region = region;
    op.deltaX = surfaceX;
    op.deltaY = surfaceY;

    err = (*vars->copyRegionProc)(interface,
                    options,
                    (kIOBlitTypeCopyRegion | kIOBlitTypeOperationType0),
                    kIOBlitSourceCGSSurface,
                    &op.operation,
                    (void *) sourceSurfaceID);

    if( kIOReturnSuccess == err)
        (*interface)->Flush(interface, kNilOptions);

    return( err );
}

#if 0

kern_return_t
IOFBSetupFIFOBurst( void * blitterRef,
		UInt32 x, UInt32 y, UInt32 w, UInt32 h,
		UInt32 options, void ** burstRef )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOReturn		err;
    boolean_t		wait;

    do {
        IOSharedLockLock( &vars->context->contextLock );
        wait = (kIOReturnBusy == (
		err = vars->procs.setupFIFOBurst( vars->chipRef, x, y, w, h,
							options, burstRef )));
        IOSharedLockUnlock( &vars->context->contextLock, wait );
    } while( wait );

    return( err );
}

kern_return_t
IOFBCommitMemory( void * blitterRef,
		vm_address_t start, vm_size_t length, IOOptionBits options,
		void ** memoryRef, IOByteCount * offset )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOReturn		err;
    unsigned int	len;
    int			params[ 3 ];

    params[0] = start;
    params[1] = length;
    params[2] = options;
    len = 2;
    err = io_connect_method_scalarI_scalarO( vars->connect, 2, /*index*/
                    params, 3, params, &len);

    if( kIOReturnSuccess == err) {
	*memoryRef = (void *) params[0];    
	*offset = params[1];    
    }

    return( err );
}

kern_return_t
IOFBReleaseMemory( void * blitterRef, void * memoryRef )
{
    _BlitterVars *	vars = (_BlitterVars *) blitterRef;
    IOReturn		err;
    unsigned int	len;

    IOPSBlitIdle( blitterRef );

    len = 0;
    err = io_connect_method_scalarI_scalarO( vars->connect, 3, /*index*/
                    (int *) &memoryRef, 1, NULL, &len);

    return( err );
}

#endif

kern_return_t
IOFBMemoryCopy( void * blitterRef,
			UInt32 x, UInt32 y,
			UInt32 width, UInt32 height, 
			UInt32 srcByteOffset, UInt32 srcRowBytes,
			SInt32 * token)
{
    _BlitterVars *			vars = (_BlitterVars *) blitterRef;
    IOGraphicsAcceleratorInterface ** 	interface = vars->interface;
    IOReturn				err;
    IOBlitMemory			source;
    IOBlitCopyRectangles		rects;

    if( vars->sid) {
        err = (*interface)->SetDestination(interface, kIOBlitFramebufferDestination, 0);
        vars->sid = 0;
        if( err)
            return( err );
    }

    rects.count = 1;
    rects.rects[0].x = x;
    rects.rects[0].y = y;
    rects.rects[0].width = width;
    rects.rects[0].height = height;
    rects.rects[0].sourceX = 0;
    rects.rects[0].sourceY = 0;

    source.memory.ref = 0;	// !!
    source.byteOffset = srcByteOffset;
    source.rowBytes = srcRowBytes;

    err = (*vars->memCopyProc)(interface,
                    kNilOptions,
                    (kIOBlitTypeCopyRects | kIOBlitCopyOperation),
                    kIOBlitSourceMemory,
                    &rects.operation,
                    (void *) &source);

    return( err );
}

#else /* NO_CFPLUGIN */

/* We need these symbols to exist to prevent link errors in clients.  Have them all return an error. */

kern_return_t
IOPSAllocateBlitEngine( io_connect_t framebuffer, void ** blitterRef, int * quality)
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitReset( void * blitterRef)
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitDeallocate( void * blitterRef )
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitIdle( void * blitterRef )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBWaitForCompletion( void * blitterRef, SInt32 token )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBSynchronize( void * blitterRef, UInt32 x, UInt32 y, UInt32 w, UInt32 h, UInt32 options )
{ return kIOReturnUnsupported; }
     
kern_return_t
IOFBBeamPosition( void * blitterRef, UInt32 options, SInt32 * position )
{ return kIOReturnUnsupported; }
     
kern_return_t
IOPSBlitFill( void * blitterRef, int dst_x, int dst_y, int width, int height, int data )
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitInvert( void * blitterRef, int x, int y, int w, int h )
{ return kIOReturnUnsupported; }

kern_return_t
IOPSBlitCopy( void * blitterRef, int src_x, int src_y, int width, int height, int dst_x, int dst_y )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBBlitVRAMCopy( void * blitterRef, int sourceX, int sourceY, int width, int height, int x, int y, IOOptionBits options )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBMemoryCopy( void * blitterRef, UInt32 x, UInt32 y, UInt32 width, UInt32 height, UInt32 srcByteOffset, UInt32 srcRowBytes, SInt32 * token)
{ return kIOReturnUnsupported; }

kern_return_t
IOFBSetupFIFOBurst( void * blitterRef, UInt32 x, UInt32 y, UInt32 w, UInt32 h, UInt32 options, void ** burstRef )
{ return kIOReturnUnsupported; }

void
IOFBBurstWrite32( void * p1, void * p2, void * p3, void * p4, void * p5, void * p6, void * p7, void * p8 )
{ return kIOReturnUnsupported; }

void
IOFBSetBurstRef( void * burstRef )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBCommitMemory( void * blitterRef, vm_address_t start, vm_size_t length, IOOptionBits options, void ** memoryRef, IOByteCount * offset )
{ return kIOReturnUnsupported; }

kern_return_t
IOFBReleaseMemory( void * blitterRef, void * memoryRef )
{ return kIOReturnUnsupported; }

#endif /* !NO_CFPLUGIN */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <IOKit/i2c/IOI2CInterfacePrivate.h>

struct IOI2CConnect
{
    io_connect_t connect;
};


IOReturn IOI2CCopyInterfaceForID( CFTypeRef identifier, io_service_t * interface )
{
    CFMutableDictionaryRef dict, matching;
    mach_port_t	  	   masterPort;
    kern_return_t	   kr;
    io_iterator_t	   iter;

    IOMasterPort( MACH_PORT_NULL, &masterPort );
    
    matching = IOServiceMatching(kIOI2CInterfaceClassName);
    if(!matching)
        return( kIOReturnNoMemory );

    dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
    if(!dict)
        return( kIOReturnNoMemory );

    CFDictionarySetValue(dict, CFSTR(kIOI2CInterfaceIDKey), identifier);
    CFDictionarySetValue(matching, CFSTR(kIOPropertyMatchKey), dict);
    CFRelease(dict);

    kr = IOServiceGetMatchingServices( masterPort, matching, &iter);
    if( kIOReturnSuccess == kr) {
        *interface = IOIteratorNext( iter );
        IOObjectRelease( iter );
    }

    return( kr );
}

IOReturn IOFBGetI2CInterfaceCount( io_service_t framebuffer, IOItemCount * count )
{
    CFArrayRef	 array;

    array = IORegistryEntryCreateCFProperty( framebuffer, CFSTR(kIOFBI2CInterfaceIDsKey),
                                             kCFAllocatorDefault, kNilOptions );
    if( array) {
        *count = CFArrayGetCount(array);
        CFRelease( array );
    } else
        *count = 0;

    return( kIOReturnSuccess );
}

IOReturn IOFBCopyI2CInterfaceForBus( io_service_t framebuffer, IOOptionBits bus, io_service_t * interface )
{
    IOReturn	 kr = kIOReturnNoDevice;
    CFArrayRef	 array;
    CFIndex	 index;
    CFTypeRef	 ident;

    array = IORegistryEntryCreateCFProperty( framebuffer, CFSTR(kIOFBI2CInterfaceIDsKey),
                                             kCFAllocatorDefault, kNilOptions );
    if( !array)
        return( kIOReturnNoDevice );

    index = bus & kIOI2CBusNumberMask;

    do {
        if( index >= CFArrayGetCount(array)) {
            kr = kIOReturnNoDevice;
            continue;
        }

        ident = CFArrayGetValueAtIndex(array, index);
        kr = IOI2CCopyInterfaceForID( ident, interface );

    } while( false );

    CFRelease( array );

    return( kr );
}

IOReturn IOI2CInterfaceOpen( io_service_t interface, IOOptionBits options,
                             IOI2CConnectRef * connect )
{
    kern_return_t	  kr;
    struct IOI2CConnect * connectRef;

    if( !IOObjectConformsTo(interface, kIOI2CInterfaceClassName))
        return( kIOReturnBadArgument );

    connectRef = calloc(1, sizeof(struct IOI2CConnect));
    if( !connectRef)
        return( kIOReturnNoMemory );

    kr = IOServiceOpen( interface, mach_task_self(), options, &connectRef->connect );

    if( (kr != kIOReturnSuccess) && connectRef) {
        free(connectRef);
        connectRef = NULL;
    }
    *connect = connectRef;

    return( kr );
}

IOReturn IOI2CInterfaceClose( IOI2CConnectRef connect, IOOptionBits options )
{
    kern_return_t kr;

    kr = IOServiceClose( connect->connect );

    free( connect );

    return( kr );
}

IOReturn IOI2CSendRequest( IOI2CConnectRef connect, IOOptionBits options, 
                           IOI2CRequest * request )
{
    kern_return_t kr;
    IOI2CBuffer	  buffer;
    IOByteCount	  outlen;

    if( request->sendBytes > sizeof(buffer.inlineBuffer))
        return( kIOReturnOverrun );
    if( request->replyBytes > sizeof(buffer.inlineBuffer))
        return( kIOReturnOverrun );

    kr = IOConnectMethodScalarIScalarO( connect->connect, 0, 0, 0 );
    if( kIOReturnSuccess != kr)
        return( kr );

    buffer.request = *request;
    buffer.request.replyBuffer = NULL;
    buffer.request.sendBuffer  = NULL;

    if( request->sendBytes)
        bcopy( (void *) request->sendBuffer, &buffer.inlineBuffer[0], request->sendBytes );

    outlen = sizeof( buffer);
    kr = IOConnectMethodStructureIStructureO( connect->connect, 2,
        sizeof( buffer), &outlen, &buffer, &buffer );

    if( buffer.request.replyBytes)
        bcopy( &buffer.inlineBuffer[0], (void *)  request->replyBuffer, buffer.request.replyBytes );
    *request = buffer.request;

    IOConnectMethodScalarIScalarO( connect->connect, 1, 0, 0 );

    return( kr );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

