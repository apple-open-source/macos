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
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsEngine.h>
#include <IOKit/iokitmig.h>

#include <IOKit/ndrvsupport/IOMacOSVideo.h>

#define DEBUGPARAMS	0

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOReturn
readFile(const char *path, vm_offset_t * objAddr, vm_size_t * objSize)
{
    int fd;
    int err;
    struct stat stat_buf;

    *objAddr = 0;
    *objSize = 0;

    if((fd = open(path, O_RDONLY)) == -1)
	return errno;

    do {
	if(fstat(fd, &stat_buf) == -1) {
	    err = errno;
	    continue;
	}
	*objSize = stat_buf.st_size;

	if( KERN_SUCCESS != map_fd(fd, 0, objAddr, TRUE, *objSize)) {
            *objAddr = 0;
            *objSize = 0;
	    err = errno;
	    continue;
	}

	err = 0;

    } while( false );

    close(fd);

    return( err );
}

#if !defined(DARWIN)

static void
UnscrambleBytes( UInt8 * data, CFIndex dataSize, UInt32 key )
{
    UInt32	index;
    UInt8 *	keyBytes = (UInt8 *) &key;

    for ( index = 0; index < dataSize; index++ )
        data[index] =
            ~data[index] ^ keyBytes[ index % sizeof(UInt32) ];
}

#endif

static void
AddLocalString( CFMutableDictionaryRef displayDict, CFStringRef displayKey,
                const char * cString, CFStringRef localKey, CFStringEncoding encoding )
{                
    CFMutableDictionaryRef	dict;
    CFStringRef 		string;

    string = CFStringCreateWithCString( kCFAllocatorDefault, cString,
                                        encoding );

    dict = (CFMutableDictionaryRef) CFDictionaryGetValue(displayDict, displayKey);
    if( !dict)
        dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue( dict, localKey, string );
    CFRelease( string );
    if( !CFDictionaryGetValue( displayDict, displayKey )) {
        CFDictionarySetValue( displayDict, displayKey, dict );
        CFRelease( dict );
    }
}

static CFDictionaryRef
IODisplayCreateOverrides( IODisplayVendorID vendor, IODisplayProductID product,
                            UInt32 serialNumber, CFAbsoluteTime manufactureDate )
{

    char		path[256];
    vm_offset_t 	bytes;
    vm_size_t		byteLen;
    CFDataRef		data;
    CFTypeRef		obj = 0;
    CFDictionaryRef	dict = 0;
    IOReturn		err;

    sprintf( path, "/System/Library/Displays/Overrides"
                    "/" kDisplayVendorID "-%lx"
                    "/" kDisplayProductID "-%lx",
                    vendor, product );
    err = readFile( path, &bytes, &byteLen );

    if( kIOReturnSuccess == err) {
    
        data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
                                    (const UInt8 *) bytes, byteLen, kCFAllocatorNull );
        if( data) {
            obj = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, data,
                                                kCFPropertyListImmutable,
                                                (CFStringRef *) NULL );
            CFRelease( data );
#if !defined(DARWIN)
            if( !obj) {
                UnscrambleBytes( (UInt8 *) bytes, byteLen,
                                ((vendor & 0xffff) << 16) | (product & 0xffff));
                data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
                                    (const UInt8 *) bytes, byteLen, kCFAllocatorNull );
                if( data) {
                    obj = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, data,
                                                            kCFPropertyListImmutable,
                                                            (CFStringRef *) NULL );
                    CFRelease( data );
                }
            }
#endif
        }
        vm_deallocate( mach_task_self(), bytes, byteLen );
    }

    if( obj) {
        if( CFDictionaryGetTypeID() == CFGetTypeID( obj )) {
            dict = obj;
        } else if( CFArrayGetTypeID() == CFGetTypeID( obj )) {
            // match serial numbers etc
        }
    }

    return( dict );
}

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

static void
EDIDInfo( struct EDID * edid,
            IODisplayVendorID * vendor, IODisplayProductID * product,
            UInt32 * serialNumber, CFAbsoluteTime * manufactureDate )
{
    SInt32		sint;

    if( vendor)
        *vendor = (edid->vendorProduct[0] << 8) | edid->vendorProduct[1];
    if( product)
        *product = (edid->vendorProduct[3] << 8) | edid->vendorProduct[2];

    if( serialNumber) {
        sint = (edid->serialNumber[3] << 24)
             | (edid->serialNumber[2] << 16)
             | (edid->serialNumber[1] << 8)
             | (edid->serialNumber[0]);
        if( sint == 0x01010101)
            sint = 0;
        *serialNumber = sint;
    }

    if( false && manufactureDate ) {

        CFGregorianDate gDate;
        CFTimeZoneRef	tz;

        gDate.year	= edid->yearOfManufacture + 1990;
        gDate.month	= 0;
        gDate.day	= edid->weekOfManufacture * 7;
        gDate.hour	= 0;
        gDate.minute	= 0;
        gDate.second	= 0.0;

        tz = CFTimeZoneCopySystem();
        *manufactureDate = CFGregorianDateGetAbsoluteTime( gDate, tz);
        CFRelease(tz);
    }
}

static Boolean
EDIDName( EDID * edid, char * name )
{
    char *	oname = name;
    EDIDDesc *	desc;
    int		i,j;
    Boolean	ok;
    char	c;

    if( !edid || (edid->version < 1) || (edid->revision < 1))
        return( false );

    desc = edid->descriptors;
    for( i = 0; i < 4; i++, desc++) {
        if( desc->general.flag1 || desc->general.flag2 || desc->general.flag3)
            continue;
        if( 0xfc != desc->general.type)
            continue;

        for( j = 0; j < sizeof(desc->general.data); j++) {
            c = desc->general.data[j];
            if( c != 0x0a)
                *oname++ = c;
            else
                break;
        }
    }
    ok = (oname != name);
    if( ok)
        *oname++ = 0;

    return( ok );
}

static void
MaxTimingRangeRec( VDDisplayTimingRangeRec * range )
{
    bzero( range, sizeof( VDDisplayTimingRangeRec) );

    range->csTimingRangeSyncFlags		= 0xffffffff;
    range->csTimingRangeSignalLevels		= 0xffffffff;

    range->csMaxFrameRate			= 0xffffffff;
    range->csMaxLineRate			= 0xffffffff;
    range->csMaxPixelClock			= 0xffffffff;
    range->csMaxPixelError			= 0xffffffff;

    range->csMaxHorizontalTotal			= 0xffffffff;
    range->csMaxVerticalTotal			= 0xffffffff;
    range->csMaxHorizontalActiveClocks		= 0xffffffff;
    range->csMaxHorizontalBlankingClocks	= 0xffffffff;
    range->csMaxHorizontalSyncOffsetClocks	= 0xffffffff;
    range->csMaxHorizontalPulseWidthClocks	= 0xffffffff;
    range->csMaxVerticalActiveClocks		= 0xffffffff;
    range->csMaxVerticalBlankingClocks		= 0xffffffff;
    range->csMaxVerticalSyncOffsetClocks	= 0xffffffff;
    range->csMaxVerticalPulseWidthClocks	= 0xffffffff;
    range->csMaxHorizontalBorderLeft		= 0xffffffff;
    range->csMaxHorizontalBorderRight		= 0xffffffff;
    range->csMaxVerticalBorderTop		= 0xffffffff;
    range->csMaxVerticalBorderBottom		= 0xffffffff;

    range->csCharSizeHorizontalActive		= 1;
    range->csCharSizeHorizontalBlanking		= 1;
    range->csCharSizeHorizontalSyncOffset	= 1;
    range->csCharSizeHorizontalSyncPulse	= 1;
    range->csCharSizeVerticalBlanking		= 1;
    range->csCharSizeVerticalSyncOffset		= 1;
    range->csCharSizeVerticalSyncPulse		= 1;
    range->csCharSizeHorizontalBorderLeft	= 1;
    range->csCharSizeHorizontalBorderRight	= 1;
    range->csCharSizeVerticalBorderTop		= 1;
    range->csCharSizeVerticalBorderBottom	= 1;
    range->csCharSizeHorizontalTotal		= 1;
    range->csCharSizeVerticalTotal		= 1;
}

static Boolean
EDIDDescToDisplayTimingRangeRec( EDID * edid, EDIDGeneralDesc * desc,
                             VDDisplayTimingRangeRec * range )
{
    UInt8 byte;

    if( !edid || (edid->version < 1) || (edid->revision < 1))
        return( false );

    if( desc->flag1 || desc->flag2 || desc->flag3)
        return( false );
    if( 0xfd != desc->type)
        return( false );

    MaxTimingRangeRec( range );

    byte = edid->displayParams[0];
    range->csTimingRangeSignalLevels = 1 << ((byte >> 5) & 3);
    range->csTimingRangeSyncFlags = ((byte & 1) ? kRangeSupportsVSyncSerrationMask : 0)
                                  | ((byte & 2) ? kRangeSupportsSyncOnGreenMask : 0)
                                  | ((byte & 4) ? kRangeSupportsCompositeSyncMask : 0)
                                  | ((byte & 8) ? kRangeSupportsVSyncSerrationMask : 0);

    range->csMinFrameRate  = desc->data[0];
    range->csMaxFrameRate  = desc->data[1];
    range->csMinLineRate   = desc->data[2] * 1000;
    range->csMaxLineRate   = desc->data[3] * 1000;
    range->csMaxPixelClock = desc->data[4] * 10000000ULL;

    return( true );
}

static IOReturn
EDIDDescToDetailedTimingRec( EDID * edid, EDIDDetailedTimingDesc * desc,
                            VDDetailedTimingRec * timing )
{

    bzero( timing, sizeof( VDDetailedTimingRec) );
    timing->csTimingSize = sizeof( VDDetailedTimingRec);

    if( !desc->clock)
        return( kIOReturnBadArgument );

    timing->csSignalConfig		= (edid->displayParams[0] & 16)
                                        ? kAnalogSetupExpectedMask : 0;
    timing->csSignalLevels		= (edid->displayParams[0] >> 5) & 3;

    timing->csPixelClock 		= ((UInt64) ((desc->clock & 0xff) << 8)
                                                   | (desc->clock >> 8))
                                        * 10000ULL;

    timing->csHorizontalActive		= desc->horizActive
                                        | ((desc->horizHigh & 0xf0) << 4);
    timing->csHorizontalBlanking	= desc->horizBlanking
                                        | ((desc->horizHigh & 0x0f) << 8);

    timing->csVerticalActive 		= desc->verticalActive
                                        | ((desc->verticalHigh & 0xf0) << 4);
    timing->csVerticalBlanking  	= desc->verticalBlanking
                                        | ((desc->verticalHigh & 0x0f) << 8);

    timing->csHorizontalSyncOffset	= desc->horizSyncOffset
                                        | ((desc->syncHigh & 0xc0) << 2);
    timing->csHorizontalSyncPulseWidth	= desc->horizSyncWidth
                                        | ((desc->syncHigh & 0x30) << 4);

    timing->csVerticalSyncOffset	= ((desc->verticalSyncOffsetWidth & 0xf0) >> 4)
                                        | ((desc->syncHigh & 0x0c) << 4);
    timing->csVerticalSyncPulseWidth	= ((desc->verticalSyncOffsetWidth & 0x0f) >> 0)
                                        | ((desc->syncHigh & 0x03) << 4);

    timing->csHorizontalBorderLeft	= desc->horizBorder;
    timing->csHorizontalBorderRight	= desc->horizBorder;
    timing->csVerticalBorderTop		= desc->verticalBorder;
    timing->csVerticalBorderBottom	= desc->verticalBorder;

    timing->csHorizontalSyncConfig	= (desc->flags & 2)
                                        ? kSyncPositivePolarityMask : 0;
    timing->csHorizontalSyncLevel	= 0;
    timing->csVerticalSyncConfig	= (desc->flags & 4)
                                        ? kSyncPositivePolarityMask : 0;
    timing->csVerticalSyncLevel		= 0;

    return( kIOReturnSuccess );
}

static Boolean
CheckTimingWithRange( VDDisplayTimingRangeRec * range,
                        VDDetailedTimingRec * timing )
{
    UInt64	pixelClock;
    UInt64	rate;
    UInt32	hTotal, vTotal;

//    if( 0 == (range->csTimingRangeSyncFlags & (1 << (timing->csSignalLevels))))
//        return( false);

    if( 0 == (range->csTimingRangeSignalLevels & (1 << (timing->csSignalLevels))))
        return( false);

    pixelClock = timing->csPixelClock;
    hTotal = (timing->csHorizontalActive + timing->csHorizontalBlanking);
    vTotal = (timing->csVerticalActive + timing->csVerticalBlanking);

    if( (pixelClock > range->csMaxPixelClock)
     || (pixelClock < range->csMinPixelClock))
        return( false);

    // line rate
    rate = pixelClock / hTotal;
    if( (rate > range->csMaxLineRate)
     || (rate < range->csMinLineRate))
        return( false);

    // frame rate
    rate = pixelClock / (hTotal * vTotal);
    if( (rate > range->csMaxFrameRate)
     || (rate < range->csMinFrameRate))
        return( false);

    if( hTotal > range->csMaxHorizontalTotal)
        return( false);
    if( vTotal > range->csMaxVerticalTotal)
        return( false);

    if( (timing->csHorizontalActive > range->csMaxHorizontalActiveClocks)
     || (timing->csHorizontalActive < range->csMinHorizontalActiveClocks))
        return( false);
    if( (timing->csVerticalActive > range->csMaxVerticalActiveClocks)
     || (timing->csVerticalActive < range->csMinVerticalActiveClocks))
        return( false);

/*
    if( (timing->csHorizontalBlanking > range->csMaxHorizontalBlankingClocks)
     || (timing->csHorizontalBlanking < range->csMinHorizontalBlankingClocks))
        return( false);
    if( (timing->csVerticalBlanking > range->csMaxVerticalBlankingClocks)
     || (timing->csVerticalBlanking < range->csMinVerticalBlankingClocks))
        return( false);
*/
    if( (timing->csHorizontalSyncOffset > range->csMaxHorizontalSyncOffsetClocks)
     || (timing->csHorizontalSyncOffset < range->csMinHorizontalSyncOffsetClocks))
        return( false);
    if( (timing->csHorizontalSyncPulseWidth > range->csMaxHorizontalPulseWidthClocks)
     || (timing->csHorizontalSyncPulseWidth < range->csMinHorizontalPulseWidthClocks))
        return( false);

    if( (timing->csVerticalSyncOffset > range->csMaxVerticalSyncOffsetClocks)
     || (timing->csVerticalSyncOffset < range->csMinVerticalSyncOffsetClocks))
        return( false);
    if( (timing->csVerticalSyncPulseWidth > range->csMaxVerticalPulseWidthClocks)
     || (timing->csVerticalSyncPulseWidth < range->csMinVerticalPulseWidthClocks))
        return( false);

    if( (timing->csHorizontalBorderLeft > range->csMaxHorizontalBorderLeft)
     || (timing->csHorizontalBorderLeft < range->csMinHorizontalBorderLeft))
        return( false);
    if( (timing->csHorizontalBorderRight > range->csMaxHorizontalBorderRight)
     || (timing->csHorizontalBorderRight < range->csMinHorizontalBorderRight))
        return( false);
    if( (timing->csVerticalBorderTop > range->csMaxVerticalBorderTop)
     || (timing->csVerticalBorderTop < range->csMinVerticalBorderTop))
        return( false);
    if( (timing->csVerticalBorderBottom > range->csMaxVerticalBorderBottom)
     || (timing->csVerticalBorderBottom < range->csMinVerticalBorderBottom))
        return( false);

    if( timing->csHorizontalActive & (range->csCharSizeHorizontalActive - 1))
        return( false);
    if( timing->csHorizontalBlanking & (range->csCharSizeHorizontalBlanking - 1))
        return( false);
    if( timing->csHorizontalSyncOffset & (range->csCharSizeHorizontalSyncOffset - 1))
        return( false);
    if( timing->csHorizontalSyncPulseWidth & (range->csCharSizeHorizontalSyncPulse - 1))
        return( false);
    if( timing->csVerticalBlanking & (range->csCharSizeVerticalBlanking - 1))
        return( false);
    if( timing->csVerticalSyncOffset & (range->csCharSizeVerticalSyncOffset - 1))
        return( false);
    if( timing->csVerticalSyncPulseWidth & (range->csCharSizeVerticalSyncPulse - 1))
        return( false);
    if( timing->csHorizontalBorderLeft & (range->csCharSizeHorizontalBorderLeft - 1))
        return( false);
    if( timing->csHorizontalBorderRight & (range->csCharSizeHorizontalBorderRight - 1))
        return( false);
    if( timing->csVerticalBorderTop & (range->csCharSizeVerticalBorderTop - 1))
        return( false);
    if( timing->csVerticalBorderBottom & (range->csCharSizeVerticalBorderBottom - 1))
        return( false);
    if( hTotal & (range->csCharSizeHorizontalTotal - 1))
        return( false);
    if( vTotal & (range->csCharSizeVerticalTotal - 1))
        return( false);

    return( true );
}

static CFDataRef
PreflightDetailedTiming( io_connect_t connect,
                            VDDetailedTimingRec * timing,
                            VDDisplayTimingRangeRec * fbRange,
                            VDDisplayTimingRangeRec * displayRange )
{
    IOReturn 			err;
    unsigned int		len;
    CFDataRef			data = 0;

    do {
        if( !CheckTimingWithRange( fbRange, timing))
            continue;
    
        len = sizeof( VDDetailedTimingRec);
        err = io_connect_method_structureI_structureO( connect, 17, /*index*/
                        (void *) timing, len, (void *) timing, &len);
    
#if LOG
        printf("err(%x), clocks (%qx, %qx, %qx), %ld, %ld, %ld\n", err,
            timing->csPixelClock,
            timing->csMinPixelClock, timing->csMaxPixelClock,
            timing->csSignalConfig,
            timing->csHorizontalSyncConfig, timing->csVerticalSyncConfig );
#endif
    
        if( kIOReturnSuccess != err)
            continue;

        if( !CheckTimingWithRange( displayRange, timing))
            continue;

        data = CFDataCreate( kCFAllocatorDefault,
                        (UInt8 *) timing, sizeof(VDDetailedTimingRec));

    } while( false );

    return( data );
}

static CFDataRef
EDIDDescToDetailedTiming( io_connect_t connect, 
                            EDID * edid, EDIDDetailedTimingDesc * desc,
                            VDDisplayTimingRangeRec * fbRange,
                            VDDisplayTimingRangeRec * displayRange )
{
    IOReturn		err;
    VDDetailedTimingRec	timing;

    err = EDIDDescToDetailedTimingRec( edid, desc, &timing );
    if( kIOReturnSuccess != err)
        return( 0 );

    return( PreflightDetailedTiming( connect, &timing, fbRange, displayRange ));
}

void
IODisplayInstallDetailedTimings( io_connect_t connect )
{
    IOReturn			err;
    int				i;
    io_service_t		service = 0;
    EDID *			edid;
    CFDictionaryRef		dict = 0;
    CFDataRef			fbRange = 0;
    CFDataRef			data;
    CFMutableArrayRef		finalArray = 0;
    CFArrayRef			array;
    CFIndex			count;
    VDDisplayTimingRangeRec	displayRange;
    Boolean			ok;

    do {

        err = IOConnectGetService( connect, &service );
        if( kIOReturnSuccess != err)
            continue;

        array = (CFArrayRef) IORegistryEntryCreateCFProperty( service, 
                                    CFSTR(kIOFBDetailedTimingsKey),
                                    kCFAllocatorDefault, kNilOptions);
        if( array) {
            CFRelease( array );
            continue;
        }

        dict = IODisplayCreateInfoDictionary( service, kNilOptions );
        if( !dict)
            continue;

        data = CFDictionaryGetValue( dict, CFSTR(kIODisplayEDIDKey) );
        if( !data || (CFDataGetLength(data) < sizeof( EDID)) )
            continue;
        edid = (EDID *) CFDataGetBytePtr( data );

        finalArray = CFArrayCreateMutable( kCFAllocatorDefault, 0,
                                        &kCFTypeArrayCallBacks);
        if( !finalArray)
            continue;

        // Install no detailed timings from digital displays
        if( 0x80 & edid->displayParams[0])
            continue;

        fbRange = (CFDataRef) IORegistryEntryCreateCFProperty( service, 
                                    CFSTR(kIOFBTimingRangeKey),
                                    kCFAllocatorDefault, kNilOptions);
        if( !fbRange || (CFDataGetLength(fbRange)) < sizeof(VDDisplayTimingRangeRec))
            continue;

        // EDID timing range
        MaxTimingRangeRec( &displayRange );
	for( i = 0, ok = false; (!ok) && i < 4; i++ )
            ok = EDIDDescToDisplayTimingRangeRec( edid, 
                                &edid->descriptors[i].general,
                                &displayRange );

        // override timing recs
        array = (CFArrayRef) CFDictionaryGetValue( dict, CFSTR("dspc"));
        if( array)
            count = CFArrayGetCount(array);
        else
            count = 0;
        for( i = 0; i < count; i++ ) {
            data = CFArrayGetValueAtIndex(array, i);
            if( !data || (sizeof(EDIDDetailedTimingDesc) != CFDataGetLength(data)))
                continue;

            data = EDIDDescToDetailedTiming( connect,
                            edid, (EDIDDetailedTimingDesc *) CFDataGetBytePtr(data),
                            (VDDisplayTimingRangeRec *) CFDataGetBytePtr(fbRange),
                            &displayRange );
            if( data) {
                CFArrayAppendValue( finalArray, data );
                CFRelease( data );
            }
        }

        // EDID timing recs
	for( i = 0; i < 4; i++ ) {
            data = EDIDDescToDetailedTiming( connect,
                                edid,
                                &edid->descriptors[i].timing,
                                (VDDisplayTimingRangeRec *) CFDataGetBytePtr(fbRange),
                                &displayRange );
            if( data) {
                CFArrayAppendValue( finalArray, data );
                CFRelease( data );
            }
        }

        count = CFArrayGetCount(finalArray);
        if( !count)
            continue;

        err = IOConnectSetCFProperty( connect,
                        CFSTR(kIOFBDetailedTimingsKey), finalArray );
#if LOG
        printf("IORegistryEntrySetCFProperty(%x)\n", err);
        CFShow(finalArray);

        for( i = 0; i < count; i++ ) {
            IOPixelInformation info;

            err = IOFBGetPixelInformation( connect, kDisplayModeIDReservedBase + i,
                                            0, kIOFBSystemAperture, &info );
            printf("IOFBGetPixelInformation(%x), %ld, %ld\n", err,
                    info.activeWidth, info.activeHeight);

        }
#endif

    } while( false );

    if( dict)
        CFRelease(dict);
    if( finalArray)
        CFRelease(finalArray);
    if( fbRange)
        CFRelease(fbRange);
    if( service)
        IOObjectRelease(service);
}

SInt32
IODisplayMatchDictionaries(
        CFDictionaryRef		matching1,
        CFDictionaryRef		matching2,
	IOOptionBits		options )
{
    CFNumberRef		num1, num2;
    CFStringRef		str1, str2;
    SInt32		matches = 0;

    if( !matching1 || !matching2)
        return( -1 );

    do {
        num1 = CFDictionaryGetValue( matching1, CFSTR(kDisplayVendorID) );
        num2 = CFDictionaryGetValue( matching2, CFSTR(kDisplayVendorID) );
        if( !num1 || !num2)
            continue;
        if( !CFEqual( num1, num2))
            continue;

        num1 = CFDictionaryGetValue( matching1, CFSTR(kDisplayProductID) );
        num2 = CFDictionaryGetValue( matching2, CFSTR(kDisplayProductID) );
        if( !num1 || !num2)
            continue;
        if( !CFEqual( num1, num2))
            continue;

        num1 = CFDictionaryGetValue( matching1, CFSTR(kDisplaySerialNumber) );
        num2 = CFDictionaryGetValue( matching2, CFSTR(kDisplaySerialNumber) );
        if( num1 && num2 && (!CFEqual( num1, num2)))
            continue;

        str1 = CFDictionaryGetValue( matching1, CFSTR(kIODisplayLocationKey) );
        str2 = CFDictionaryGetValue( matching2, CFSTR(kIODisplayLocationKey) );
        if( str1 && str2 && (!CFEqual( str1, str2)))
            continue;

        matches = 1000;

    } while( false );

    return( matches );
}

io_service_t
IODisplayForFramebuffer(
	io_service_t		framebuffer,
	IOOptionBits		options )
{
    IOReturn		kr;
    io_iterator_t	iter;
    io_service_t	service = 0;

    if( IOObjectConformsTo( framebuffer, "IODisplay"))
        return( framebuffer );

    kr = IORegistryEntryCreateIterator( framebuffer, kIOServicePlane,
					kIORegistryIterateRecursively, &iter);
    if( kr != kIOReturnSuccess )
	return( 0 );

    for( ;
	(service = IOIteratorNext( iter));
	IOObjectRelease(service)) {

        if( IOObjectConformsTo( service, "IODisplay"))
            break;
    }
    IOObjectRelease( iter );

    return( service );
}

CFDictionaryRef
IODisplayCreateInfoDictionary(
	io_service_t		framebuffer,
	IOOptionBits		options )
{
    IOReturn		kr;
    io_service_t	service = 0;
    CFDataRef		data = 0;
    CFNumberRef		num;
    CFMutableDictionaryRef	dict;
    CFDictionaryRef	regDict;
    CFDictionaryRef	ovrDict = 0;
    SInt32		sint;
    UInt8		low;
    float		fnum;
    EDID *		edid = 0;
    IODisplayVendorID	vendor = 0;
    IODisplayProductID	product = 0;
    UInt32		serialNumber = 0;
    CFAbsoluteTime	manufactureDate;
    io_string_t		path;

    bzero( &manufactureDate, sizeof(manufactureDate) );

    if( !(service = IODisplayForFramebuffer( framebuffer, options)))
	return( 0 );

    do {

        regDict = 0;
        kr = IORegistryEntryCreateCFProperties( service, &regDict,
                                                kCFAllocatorDefault, kNilOptions );
        if( kr != kIOReturnSuccess)
            continue;

        num = CFDictionaryGetValue( regDict, CFSTR(kDisplayVendorID) );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, &vendor );
        num = CFDictionaryGetValue( regDict, CFSTR(kDisplayProductID) );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, &product );

        num = CFDictionaryGetValue( regDict, CFSTR("AppleDisplayType") );
        if( num) {
            CFNumberGetValue( num, kCFNumberSInt32Type, &sint );
            if( sint == 10) {
                vendor = kDisplayVendorIDUnknown;
                product = kDisplayProductIDGeneric;
            }
        }

        data = CFDictionaryGetValue( regDict, CFSTR(kIODisplayEDIDKey) );

        if( !data)
            continue;
        edid = (EDID *) CFDataGetBytePtr( data );
        if( vendor && product)
            EDIDInfo( edid, 0, 0, &serialNumber, &manufactureDate );
        else
            EDIDInfo( edid, &vendor, &product, &serialNumber, &manufactureDate );

    } while( false );

    // <hack>
    if( !vendor && !product) {
        vendor = kDisplayVendorIDUnknown;
        product = kDisplayProductIDGeneric;
    } // </hack>

    if( 0 == (options & kIODisplayMatchingInfo))
        ovrDict = IODisplayCreateOverrides( vendor, product,
                                            serialNumber, manufactureDate );
    if( ovrDict)
        dict = CFDictionaryCreateMutableCopy( kCFAllocatorDefault, 0, ovrDict);
    else
        dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);

#define makeInt( key, value )	\
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &value );	\
	CFDictionarySetValue( dict, key,  num );					\
	CFRelease( num );

#define makeFloat( key )	\
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberFloatType, &fnum  );	\
	CFDictionarySetValue( dict, key, num );						\
	CFRelease( num );

    do {
        if( !dict)
	    continue;

	makeInt( CFSTR( kDisplayVendorID ), vendor );
	makeInt( CFSTR( kDisplayProductID ), product );

	if( serialNumber) {
            makeInt( CFSTR( kDisplaySerialNumber ), serialNumber );
        }

        kr = IORegistryEntryGetPath( service, kIOServicePlane, path );
        if( KERN_SUCCESS == kr) {
            CFStringRef string;
            string = CFStringCreateWithCString( kCFAllocatorDefault, path,
                                        kCFStringEncodingMacRoman );
            if( string) {
                CFDictionaryAddValue( dict, CFSTR(kIODisplayLocationKey), string);
                CFRelease(string);
            }
        }

        // -- that's all for matching --
        if( options & kIODisplayMatchingInfo)
            continue;

        // if !exist add display edid
        if( data)
            CFDictionaryAddValue( dict, CFSTR(kIODisplayEDIDKey), data);
        // get final edid
        data = CFDictionaryGetValue( dict, CFSTR(kIODisplayEDIDKey));
        if( data)
            edid = (EDID *) CFDataGetBytePtr( data );
            // no point in serial# / manufacture date from override
        else
            edid = 0;

        // if !exist add name
        if( !CFDictionaryGetValue( dict, CFSTR(kDisplayProductName))) {

            char sbuf[ 128 ];
            char * name;

            if( EDIDName(edid, sbuf))
                name = sbuf;
            else
                name = "Unknown Display";

            AddLocalString( dict, CFSTR(kDisplayProductName),
                                name,
                                CFSTR("en"), kCFStringEncodingMacRoman );
        }

        if( !edid)
            continue;

	sint = edid->weekOfManufacture;
	makeInt( CFSTR( kDisplayWeekOfManufacture ), sint );
	sint = edid->yearOfManufacture + 1990;
	makeInt( CFSTR( kDisplayYearOfManufacture ), sint );

	sint = edid->displayParams[1] * 10;
	makeInt( CFSTR( kDisplayHorizontalImageSize ), sint );
	sint = edid->displayParams[2] * 10;
	makeInt( CFSTR( kDisplayVerticalImageSize ), sint );

        // color info
        low = edid->colorCharacteristics[0];

        fnum = (edid->colorCharacteristics[2] << 2) | ((low >> 6) & 3);
        fnum /= (1 << 10);
        makeFloat( CFSTR( kDisplayRedPointX ) );
        fnum = (edid->colorCharacteristics[3] << 2) | ((low >> 4) & 3);
        fnum /= (1 << 10);
        makeFloat( CFSTR( kDisplayRedPointY ) );

        fnum = (edid->colorCharacteristics[4] << 2) | ((low >> 2) & 3);
        fnum /= (1 << 10);
        makeFloat( CFSTR( kDisplayGreenPointX ) );
        fnum = (edid->colorCharacteristics[5] << 2) | ((low >> 0) & 3);
        fnum /= (1 << 10);
        makeFloat( CFSTR( kDisplayGreenPointY ) );

        low = edid->colorCharacteristics[1];

        fnum = (edid->colorCharacteristics[6] << 2) | ((low >> 6) & 3);
        fnum /= (1 << 10);
        makeFloat( CFSTR( kDisplayBluePointX ) );
        fnum = (edid->colorCharacteristics[7] << 2) | ((low >> 4) & 3);
        fnum /= (1 << 10);
        makeFloat( CFSTR( kDisplayBluePointY ) );

        fnum = (edid->colorCharacteristics[8] << 2) | ((low >> 2) & 3);
        fnum /= (1 << 10);
        makeFloat( CFSTR( kDisplayWhitePointX ) );
        fnum = (edid->colorCharacteristics[9] << 2) | ((low >> 0) & 3);
        fnum /= (1 << 10);
        makeFloat( CFSTR( kDisplayWhitePointY ) );

        fnum = edid->displayParams[3];
        fnum = (fnum + 100.0) / 100.0;
        makeFloat( CFSTR( kDisplayWhiteGamma ) );

    } while( false );

    if( regDict)
        CFRelease( regDict );
    if( ovrDict)
        CFRelease( ovrDict );

    return( dict );
}

IOReturn
IODisplayCopyParameters(
	io_service_t	  service,
	IOOptionBits	  options,
        CFDictionaryRef * params )
{
    if( !(service = IODisplayForFramebuffer( service, options)))
	return( kIOReturnUnsupported );

    *params = IORegistryEntryCreateCFProperty( service, CFSTR(kIODisplayParametersKey),
                                            kCFAllocatorDefault, kNilOptions );

    return( *params ? kIOReturnSuccess : kIOReturnUnsupported );
}

IOReturn
IODisplayCopyFloatParameters(
	io_service_t	  service,
	IOOptionBits	  options,
        CFDictionaryRef * params )
{
    return( kIOReturnUnsupported );
}

IOReturn
IODisplayGetIntegerRangeParameter(
	io_service_t	service,
	IOOptionBits	options,
        CFStringRef	parameterName,
        SInt32 *	value,
        SInt32 *	min,
        SInt32 *	max )
{
    IOReturn		err;
    CFDictionaryRef	params;
    CFDictionaryRef	param;
    CFNumberRef		num;

#if DEBUGPARAMS
    const char *	cStr = 0;
    
    if( (cStr = CFStringGetCStringPtr( parameterName, kCFStringEncodingMacRoman))
    && (cStr = getenv(cStr)))
        parameterName =  CFStringCreateWithCString( kCFAllocatorDefault, cStr,
                                                    kCFStringEncodingMacRoman );
#endif

    do {
        err = IODisplayCopyParameters( service, options, &params );
        if( err != kIOReturnSuccess)
            continue;

        param = CFDictionaryGetValue( params, parameterName );

        if( !param) {
            err = kIOReturnUnsupported;
            continue;
        }
        if( value && (num = CFDictionaryGetValue( param, CFSTR(kIODisplayValueKey))))
            CFNumberGetValue( num, kCFNumberSInt32Type, value );
        if( min && (num = CFDictionaryGetValue( param, CFSTR(kIODisplayMinValueKey))))
            CFNumberGetValue( num, kCFNumberSInt32Type, min );
        if( max && (num = CFDictionaryGetValue( param, CFSTR(kIODisplayMaxValueKey))))
            CFNumberGetValue( num, kCFNumberSInt32Type, max );

    } while( false );

    if( params)
        CFRelease(params);

#if DEBUGPARAMS
    if( cStr)
        CFRelease(parameterName);
#endif

    return( err );
}

IOReturn
IODisplayGetFloatParameter(
	io_service_t	service,
	IOOptionBits	options,
        CFStringRef	parameterName,
        float *		value )
{
    IOReturn	err;
    SInt32	ivalue, min, max;

    err = IODisplayGetIntegerRangeParameter( service, options, parameterName,
                                             &ivalue, &min, &max );
    if( err)
        return( err);

    if( min == max)
        *value = 0;
    else
        *value = (((float) ivalue) - ((float) min)) / (((float) max) - ((float) min));

    return( err );
}


IOReturn
IODisplaySetParameters(
	io_service_t	service,
	IOOptionBits	options,
        CFDictionaryRef	params )
{
    IOReturn err;

    if( !(service = IODisplayForFramebuffer( service, options)))
	return( kIOReturnUnsupported );

    err = IORegistryEntrySetCFProperties( service, params );

    return( err );
}

IOReturn
IODisplaySetIntegerParameter(
	io_service_t	service,
	IOOptionBits	options,
        CFStringRef	parameterName,
        SInt32 		value )
{
    IOReturn		err;
    CFDictionaryRef	dict;
    CFNumberRef		num;

#if DEBUGPARAMS
    const char *	cStr;

    if( (cStr = CFStringGetCStringPtr( parameterName, kCFStringEncodingMacRoman))
     && (cStr = getenv(cStr)))
        parameterName =  CFStringCreateWithCString( kCFAllocatorDefault, cStr,
                                                    kCFStringEncodingMacRoman );
#endif

    num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &value );
    if( !num)
        return( kIOReturnNoMemory );

    dict = CFDictionaryCreate( kCFAllocatorDefault,
                                (const void **) &parameterName, (const void **) &num, 1,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks );
    CFRelease(num);
    if( !dict)
        return( kIOReturnNoMemory );

    err = IODisplaySetParameters( service, kNilOptions, dict );

    CFRelease(dict);

#if DEBUGPARAMS
    if( cStr)
        CFRelease(parameterName);
#endif

    return( err );
}

IOReturn
IODisplaySetFloatParameter(
	io_service_t	service,
	IOOptionBits	options,
        CFStringRef	parameterName,
        float 		value )
{
    IOReturn	err;
    SInt32	ivalue, min, max;

    err = IODisplayGetIntegerRangeParameter( service, options, parameterName,
                                             NULL, &min, &max );
    if( err)
        return( err);

    ivalue = (value * (((float) max) - ((float) min)) + ((float) min));

    err = IODisplaySetIntegerParameter( service, options, parameterName, ivalue );

    return( err );
}

IOReturn
IODisplayCommitParameters(
	io_service_t	service,
	IOOptionBits	options )
{
    return( IODisplaySetIntegerParameter( service, options,
                CFSTR(kIODisplayParametersCommitKey), 1));
}

#undef IOCreateDisplayInfoDictionary
CFDictionaryRef
IOCreateDisplayInfoDictionary(
	io_service_t		framebuffer,
	IOOptionBits		options )
{
    return( IODisplayCreateInfoDictionary( framebuffer, options));
}

