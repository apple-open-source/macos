/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <mach/mach.h>
#include <mach/thread_switch.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundlePriv.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsEngine.h>
#include <IOKit/ndrvsupport/IOMacOSVideo.h>
#include <IOKit/iokitmig.h>

#include "IOGraphicsLibPrivate.h"

#define DEBUGPARAMS	0

#define MACOS9_RANGE_LIMIT	0


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


static CFMutableDictionaryRef
IODisplayCreateOverrides( IOOptionBits options, 
                            IODisplayVendorID vendor, IODisplayProductID product,
                            UInt32 serialNumber, CFAbsoluteTime manufactureDate )
{

    char			path[256];
    vm_offset_t 		bytes;
    vm_size_t			byteLen;
    CFDataRef			data;
    CFTypeRef			obj = 0;
    CFMutableDictionaryRef	dict = 0;
    IOReturn			err;

    if( 0 == (options & kIODisplayMatchingInfo)) {

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
            }
            vm_deallocate( mach_task_self(), bytes, byteLen );
        }

        if( obj) {
            if( CFDictionaryGetTypeID() == CFGetTypeID( obj )) {
                dict = CFDictionaryCreateMutableCopy( kCFAllocatorDefault, 0, obj);
                CFRelease( obj );
            } else if( CFArrayGetTypeID() == CFGetTypeID( obj )) {
                // match serial numbers etc
            }
        }
    }
    if( !dict)
        dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);

    if( dict) do {
        CFStringRef string;
        CFURLRef   url;
        CFBundleRef bdl;
    
        sprintf( path, "/System/Library/Displays/Overrides");
//                            "/" kDisplayVendorID "-%lx", vendor );
    
        string = CFStringCreateWithCString( kCFAllocatorDefault, path,
                                            kCFStringEncodingMacRoman );
        if( !string)
            continue;
        url = CFURLCreateWithString( kCFAllocatorDefault, string, NULL);
	CFRelease(string);
        if( !url)
            continue;
        bdl = CFBundleCreate( kCFAllocatorDefault, url);
        if( bdl) {
            CFDictionarySetValue( dict, CFSTR(kDisplayBundleKey), bdl);
            CFRelease(bdl);
        }
	CFRelease(url);
    
    } while( false );

    return( dict );
}

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

struct MakeOneLocalContext {
    CFBundleRef		   bdl;
    CFMutableDictionaryRef dict;
    CFStringRef		   key;
};

static void MakeOneLocalization( const void * item, void * context )
{
    struct MakeOneLocalContext * ctx = (struct MakeOneLocalContext *) context;
    CFStringRef		value = NULL;
    CFDictionaryRef	stringTable = NULL;
    CFURLRef		url;
    CFDataRef		tableData = NULL;
    CFStringRef		errStr;
    SInt32		errCode;

    url = CFBundleCopyResourceURLForLocalization( ctx->bdl,
                                CFSTR("Localizable"), CFSTR("strings"), NULL, item );
    if (url && CFURLCreateDataAndPropertiesFromResource( kCFAllocatorDefault,
                                url, &tableData, NULL, NULL, &errCode)) {
        stringTable = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
                                tableData, kCFPropertyListImmutable, &errStr);
        if (errStr)
            CFRelease( errStr);
        CFRelease( tableData);
    }
    if( url)
        CFRelease(url);
    if( stringTable)
        value = CFDictionaryGetValue(stringTable, ctx->key);
    if (!value)
        value = ctx->key;

    {
        SInt32		 languageCode, regionCode, scriptCode;
        CFStringEncoding stringEncoding;
        if( CFBundleGetLocalizationInfoForLocalization( item, &languageCode, &regionCode,
                                                        &scriptCode, &stringEncoding )) {
            item = CFBundleCopyLocalizationForLocalizationInfo( languageCode, regionCode,
                                                        scriptCode, stringEncoding );
        } else
            item = CFRetain(item);
    }

    CFDictionarySetValue( ctx->dict, item, value );
    CFRelease( item );

    if( stringTable)
        CFRelease( stringTable );
}

static void GenerateProductName( CFMutableDictionaryRef dict,
                                        EDID * edid, SInt32 displayType, IOOptionBits options )
{
    CFStringRef		key;
    CFBundleRef		bdl;
    CFArrayRef		localizations;
    struct MakeOneLocalContext ctx;
    static const char * type2Name[] = {
        NULL,				// 000 kUnknownConnect
        NULL,				// 001 kUnknownConnect
        "Color LCD",			// 002 kPanelTFTConnect
        NULL,				// 003 kFixedModeCRTConnect
        "Multiple Scan Display",	// 004 kMultiModeCRT1Connect
        "Multiple Scan Display",	// 005 kMultiModeCRT2Connect
        "Multiple Scan Display",	// 006 kMultiModeCRT3Connect
        "Multiple Scan Display",	// 007 kMultiModeCRT4Connect
        NULL,				// 008 kModelessConnect
        "Full-Page Display",		// 009 kFullPageConnect
        "VGA Display",			// 010 kVGAConnect
        "Television",			// 011 kNTSCConnect
        "Television",			// 012 kPALConnect
        NULL,				// 013 kHRConnect
        "Color LCD",			// 014 kPanelFSTNConnect
        "Two-Page Display",		// 015 kMonoTwoPageConnect
        "Two-Page Display",		// 016 kColorTwoPageConnect
        NULL,				// 017 kColor16Connect
        NULL,				// 018 kColor19Connect
        NULL,				// 019 kGenericCRT
        "Color LCD",			// 020 kGenericLCD
        NULL,				// 021 kDDCConnect
        NULL				// 022 kNoConnect
    };

    key = CFDictionaryGetValue( dict, CFSTR(kDisplayProductName));
    if( key) {
        if( CFStringGetTypeID() != CFGetTypeID( key ))
            return;
        CFRetain(key);
    }
    bdl = (CFBundleRef) CFDictionaryGetValue( dict, CFSTR(kDisplayBundleKey));

    if( !key) {
        char sbuf[ 128 ];
        const char * name = NULL;

        if( EDIDName(edid, sbuf))
            name = sbuf;
        else {

            if( displayType < (sizeof( type2Name) / sizeof(type2Name[0])))
                name = type2Name[displayType];
            if( !name)
                name = "Unknown Display";
        }

        key = CFStringCreateWithCString( kCFAllocatorDefault, name,
                                            kCFStringEncodingMacRoman );
        if( !key)
            return;
    }

    if( bdl) {
        ctx.bdl = bdl;
        ctx.dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);
        ctx.key = key;
    
	localizations = CFBundleCopyBundleLocalizations( bdl);
	if( kIODisplayOnlyPreferredName & options) {
	    CFArrayRef temp = localizations;
	    localizations = CFBundleCopyPreferredLocalizationsFromArray( temp );
	    CFRelease( temp );
	}

	CFArrayApplyFunction( localizations,
			    CFRangeMake(0, CFArrayGetCount(localizations)),
			    &MakeOneLocalization,
			    &ctx);
        CFDictionarySetValue( dict, CFSTR(kDisplayProductName), ctx.dict);
    
        CFRelease( localizations );
        CFRelease( ctx.dict );
    }
    CFRelease( key );
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

//-- these from DM:
    range->csMinHorizontalActiveClocks = 640;
    range->csMinVerticalActiveClocks   = 480;
#if MACOS9_RANGE_LIMIT
    if( range->csMaxLineRate)
        range->csMaxHorizontalActiveClocks = range->csMaxPixelClock / range->csMaxLineRate;
    if( range->csMaxFrameRate)
        range->csMaxVerticalActiveClocks   = range->csMaxPixelClock
                                            / (range->csMinHorizontalActiveClocks * range->csMaxFrameRate);
#else
    if( range->csMinLineRate)
        range->csMaxHorizontalActiveClocks = range->csMaxPixelClock / range->csMinLineRate;
    if( range->csMinFrameRate)
        range->csMaxVerticalActiveClocks   = range->csMaxPixelClock
                                            / (range->csMinHorizontalActiveClocks * range->csMinFrameRate);
#endif
//--
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
    UInt64	hTotal, vTotal;

    if( kDigitalSignalMask & timing->csSignalConfig)
        return( false);

//    if( 0 == (range->csTimingRangeSyncFlags & (1 << (timing->csSignalLevels))))
//        return( false);
//    if( 0 == (range->csTimingRangeSignalLevels & (1 << (timing->csSignalLevels))))
//        return( false);

    pixelClock = timing->csPixelClock;
    hTotal  = timing->csHorizontalActive;
    hTotal += timing->csHorizontalBlanking;
    vTotal  = timing->csVerticalActive;
    vTotal += timing->csVerticalBlanking;

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

Boolean
IOCheckTimingWithRange( const void * range,
                        const IODetailedTimingInformationV2 * timing )
{
    return( CheckTimingWithRange( (VDDisplayTimingRangeRec *) range, (VDDetailedTimingRec *) timing));
}

static kern_return_t
PreflightDetailedTiming( IOFBConnectRef connectRef,
                            VDDetailedTimingRec * timing,
                            VDDisplayTimingRangeRec * fbRange,
                            VDDisplayTimingRangeRec * displayRange )
{
    IOReturn 			err = kIOReturnUnsupportedMode;
    unsigned int		len;

    do {
        if( !CheckTimingWithRange( fbRange, timing))
            continue;

        len = sizeof( VDDetailedTimingRec);
        err = io_connect_method_structureI_structureO( connectRef->connect, 17, /*index*/
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

        err = CheckTimingWithRange( displayRange, timing) ? kIOReturnSuccess : kIOReturnUnsupportedMode;

    } while( false );

    return( err );
}

static kern_return_t
InstallFromEDIDDesc( IOFBConnectRef connectRef, 
                            EDID * edid, EDIDDetailedTimingDesc * desc,
                            VDDisplayTimingRangeRec * fbRange,
                            VDDisplayTimingRangeRec * displayRange )
{
    IOReturn		err;
    IOTimingInformation	timing;

    bzero( &timing, sizeof( timing ));
    timing.flags = kIODetailedTimingValid;

    err = EDIDDescToDetailedTimingRec( edid, desc, (VDDetailedTimingRec *) &timing.detailedInfo.v2 );
    if( kIOReturnSuccess != err)
        return( err );

    err = PreflightDetailedTiming( connectRef, (VDDetailedTimingRec *) &timing.detailedInfo.v2,
                                    fbRange, displayRange );
    if( kIOReturnSuccess != err)
        return( err );

    err = IOFBInstallMode( connectRef, 0xffffffff, NULL, &timing, 0 );

    return( err );
}

void
IODisplayInstallDetailedTimings( IOFBConnectRef connectRef )
{
    int				i;
    io_service_t		service = connectRef->framebuffer;
    EDID *			edid;
    CFDictionaryRef		dict = 0;
    CFDataRef			fbRange = 0;
    CFDataRef			data;
    CFArrayRef			array;
    CFIndex			count;
    VDDisplayTimingRangeRec *	displayRange;
    VDDisplayTimingRangeRec	localDisplayRange;

    do {
        dict = IODisplayCreateInfoDictionary( service, kNilOptions );
        if( !dict)
            continue;

        data = CFDictionaryGetValue( dict, CFSTR(kIODisplayEDIDKey) );
        if( !data || (CFDataGetLength(data) < sizeof( EDID)) )
            continue;
        edid = (EDID *) CFDataGetBytePtr( data );

        // Install no detailed timings from digital displays
        if( 0x80 & edid->displayParams[0])
            continue;

        fbRange = (CFDataRef) IORegistryEntryCreateCFProperty( service, 
                                    CFSTR(kIOFBTimingRangeKey),
                                    kCFAllocatorDefault, kNilOptions);
        if( !fbRange || (CFDataGetLength(fbRange)) < sizeof(VDDisplayTimingRangeRec))
            continue;

        // EDID timing range
        data = (CFDataRef) CFDictionaryGetValue( dict, CFSTR("trng"));
        if( data)
            displayRange = (VDDisplayTimingRangeRec *) CFDataGetBytePtr(data);
        else {
            MaxTimingRangeRec( &localDisplayRange );
            displayRange = &localDisplayRange;
        }

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

            InstallFromEDIDDesc( connectRef,
                            edid, (EDIDDetailedTimingDesc *) CFDataGetBytePtr(data),
                            (VDDisplayTimingRangeRec *) CFDataGetBytePtr(fbRange),
                            displayRange );
        }

        // EDID timing recs
	for( i = 0; i < 4; i++ ) {

            if( i && (0 == bcmp( &edid->descriptors[0].timing,
                                 &edid->descriptors[i].timing,
                                 sizeof( EDIDDetailedTimingDesc))))
                continue;
            InstallFromEDIDDesc( connectRef,
                                edid,
                                &edid->descriptors[i].timing,
                                (VDDisplayTimingRangeRec *) CFDataGetBytePtr(fbRange),
                                displayRange );
        }

    } while( false );

    if( dict)
        CFRelease(dict);
    if( fbRange)
        CFRelease(fbRange);
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

enum {
    /* Used by default calibrator (should we show brightness panel) */
    kDisplayGestaltBrightnessAffectsGammaMask	= (1 << 0),
    kDisplayGestaltViewAngleAffectsGammaMask	= (1 << 1)
};

CFDictionaryRef
IODisplayCreateInfoDictionary(
	io_service_t		framebuffer,
	IOOptionBits		options )
{
    IOReturn			kr;
    io_service_t		service = 0;
    CFDataRef			data = 0;
    CFNumberRef			num;
    CFMutableDictionaryRef	dict = 0;
    CFDictionaryRef		regDict;
    CFTypeRef			obj;
    SInt32			sint;
    UInt8			low;
    float			fnum;
    EDID *			edid = 0;
    IODisplayVendorID		vendor = 0;
    IODisplayProductID		product = 0;
    SInt32			displayType = 0;
    UInt32			serialNumber = 0;
    CFAbsoluteTime		manufactureDate;
    io_string_t			path;
    int				i;
    VDDisplayTimingRangeRec	displayRange;

    bzero( &manufactureDate, sizeof(manufactureDate) );

    if( !(service = IODisplayForFramebuffer( framebuffer, options))) {
	dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks );
        if( dict)
            CFDictionarySetValue( dict, CFSTR(kIODisplayLocationKey), CFSTR("unknown"));
        return( dict );
    }

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

        num = CFDictionaryGetValue( regDict, CFSTR(kAppleDisplayTypeKey) );
        if( num) {
            CFNumberGetValue( num, kCFNumberSInt32Type, &displayType );
            if( (vendor == kDisplayVendorIDUnknown) && (displayType == 10))
                product = kDisplayProductIDGeneric;
        }

        data = CFDictionaryGetValue( regDict, CFSTR(kIODisplayEDIDKey) );

#if SPOOF_EDID
#warning             ****************
#warning             ** SPOOF_EDID **
#warning             ****************
        data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
                                            spoofEDID, 128, kCFAllocatorNull );
        vendor = product = 0;
#endif
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

    dict = IODisplayCreateOverrides( options, vendor, product,
                                        serialNumber, manufactureDate );

#define makeInt( key, value )	\
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &value );	\
	CFDictionaryAddValue( dict, key,  num );					\
	CFRelease( num );

#define addFloat( key )	\
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberFloatType, &fnum  );	\
	CFDictionaryAddValue( dict, key, num );						\
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

        obj = CFDictionaryGetValue( regDict, CFSTR(kIODisplayConnectFlagsKey) );
        if( obj)
            CFDictionarySetValue( dict, CFSTR(kIODisplayConnectFlagsKey), obj );

        if( IOObjectConformsTo( service, "IOBacklightDisplay"))
            CFDictionarySetValue( dict, CFSTR(kIODisplayHasBacklightKey), kCFBooleanTrue );


        if( (obj = IORegistryEntryCreateCFProperty( framebuffer, CFSTR("graphic-options"), 
                                                    kCFAllocatorDefault, kNilOptions))) {
            CFDictionaryAddValue( dict, CFSTR("graphic-options"), obj );
            CFRelease(obj);
        }

        data = CFDictionaryGetValue( dict, CFSTR("dmdg") );
        if( data)
            sint = *((SInt32 *) CFDataGetBytePtr(data));
        else
            sint = kDisplayGestaltBrightnessAffectsGammaMask;

        if( kDisplayGestaltBrightnessAffectsGammaMask & sint)
            CFDictionaryAddValue( dict, CFSTR(kDisplayBrightnessAffectsGamma), kCFBooleanTrue );
        if( kDisplayGestaltViewAngleAffectsGammaMask & sint)
            CFDictionaryAddValue( dict, CFSTR(kDisplayViewAngleAffectsGamma), kCFBooleanTrue );

        GenerateProductName( dict, edid, displayType, options );

        if( !edid)
            continue;

        if( 0x80 & edid->displayParams[0]) {
            CFDictionarySetValue( dict, CFSTR(kIODisplayIsDigitalKey), kCFBooleanTrue );

            if( kDisplayAppleVendorID == vendor) {
                CFDictionaryAddValue( dict, CFSTR(kDisplayFixedPixelFormat), kCFBooleanTrue );
                sint = kDisplaySubPixelLayoutRGB;
                makeInt( CFSTR( kDisplaySubPixelLayout ), sint );

                CFDictionaryRemoveValue( dict, CFSTR(kDisplayBrightnessAffectsGamma) );
                CFDictionarySetValue( dict, CFSTR(kDisplayViewAngleAffectsGamma), kCFBooleanTrue );
            }
        }

        if( !CFDictionaryGetValue( dict, CFSTR("trng"))) {
            // EDID timing range
            for( i = 0; i < 4; i++ ) {
                if( EDIDDescToDisplayTimingRangeRec( edid, 
                                    &edid->descriptors[i].general,
                                    &displayRange )) {
                                    
                    data = CFDataCreate( kCFAllocatorDefault,
                                (UInt8 *) &displayRange, sizeof(displayRange));
                    if( data) {
                        CFDictionarySetValue(dict, CFSTR("trng"), data);
                        CFRelease(data);
                    }
                    break;
                }
            }
        }

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
        addFloat( CFSTR( kDisplayRedPointX ) );
        fnum = (edid->colorCharacteristics[3] << 2) | ((low >> 4) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayRedPointY ) );

        fnum = (edid->colorCharacteristics[4] << 2) | ((low >> 2) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayGreenPointX ) );
        fnum = (edid->colorCharacteristics[5] << 2) | ((low >> 0) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayGreenPointY ) );

        low = edid->colorCharacteristics[1];

        fnum = (edid->colorCharacteristics[6] << 2) | ((low >> 6) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayBluePointX ) );
        fnum = (edid->colorCharacteristics[7] << 2) | ((low >> 4) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayBluePointY ) );

        fnum = (edid->colorCharacteristics[8] << 2) | ((low >> 2) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayWhitePointX ) );
        fnum = (edid->colorCharacteristics[9] << 2) | ((low >> 0) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayWhitePointY ) );

        fnum = edid->displayParams[3];
        fnum = (fnum + 100.0) / 100.0;
        addFloat( CFSTR( kDisplayWhiteGamma ) );

    } while( false );

    if( regDict)
        CFRelease( regDict );
        
    return( dict );
}

IOReturn
IODisplayCopyParameters(
	io_service_t	  service,
	IOOptionBits	  options,
        CFDictionaryRef * params )
{
    if( (service = IODisplayForFramebuffer( service, options)))
        *params = IORegistryEntryCreateCFProperty( service, CFSTR(kIODisplayParametersKey),
                                                    kCFAllocatorDefault, kNilOptions );
    else
        *params = 0;

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

