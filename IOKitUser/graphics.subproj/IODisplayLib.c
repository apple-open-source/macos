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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundlePriv.h>

#include <IOKit/IOKitLib.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsLibPrivate.h>
#include <IOKit/graphics/IOGraphicsEngine.h>
#include <IOKit/iokitmig.h>

#include "IOGraphicsLibInternal.h"

#define DEBUGPARAMS		0
#define LOG			0


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__private_extern__ IOReturn
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
        if (0 == (stat_buf.st_mode & S_IFREG)) 
        {
            *objAddr = 0;
            *objSize = 0;
            err = kIOReturnNotReadable;
            continue;
        }
	*objSize = stat_buf.st_size;

	if( KERN_SUCCESS != map_fd(fd, 0, objAddr, TRUE, *objSize)) {
            *objAddr = 0;
            *objSize = 0;
	    err = errno;
	    continue;
	}

	err = kIOReturnSuccess;

    } while( false );

    close(fd);

    return( err );
}


__private_extern__ CFMutableDictionaryRef
readPlist( const char * path, UInt32 key )
{
    IOReturn			err;
    vm_offset_t 		bytes;
    vm_size_t			byteLen;
    CFDataRef			data;
    CFMutableDictionaryRef	obj = 0;

    err = readFile( path, &bytes, &byteLen );

    if( kIOReturnSuccess != err) 
	return (0);
    
    data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
				(const UInt8 *) bytes, byteLen, kCFAllocatorNull );
    if( data) {
	obj = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, data,
					    kCFPropertyListMutableContainers,
					    (CFStringRef *) NULL );
	CFRelease( data );
    }
    vm_deallocate( mach_task_self(), bytes, byteLen );

    return (obj);
}

static CFMutableDictionaryRef
IODisplayCreateOverrides( IOOptionBits options, 
                            IODisplayVendorID vendor, IODisplayProductID product,
                            UInt32 serialNumber, CFAbsoluteTime manufactureDate )
{

    char			path[256];
    CFTypeRef			obj = 0;
    CFMutableDictionaryRef	dict = 0;

    if( 0 == (options & kIODisplayMatchingInfo)) {

        sprintf( path, "/System/Library/Displays/Overrides"
                        "/" kDisplayVendorID "-%lx"
                        "/" kDisplayProductID "-%lx",
                        vendor, product );
    
        obj = readPlist( path, ((vendor & 0xffff) << 16) | (product & 0xffff) );
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
	else if (edid)
	    name = "Unknown Display";
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
MaxTimingRangeRec( IODisplayTimingRange * range )
{
    bzero( range, sizeof( IODisplayTimingRange) );

    range->supportedSyncFlags			= 0xffffffff;
    range->supportedSignalLevels		= 0xffffffff;

    range->maxFrameRate				= 0xffffffff;
    range->maxLineRate				= 0xffffffff;
    range->maxPixelClock			= 0xffffffff;
    range->maxPixelError			= 0xffffffff;

    range->maxHorizontalTotal			= 0xffffffff;
    range->maxVerticalTotal			= 0xffffffff;
    range->maxHorizontalActiveClocks		= 0xffffffff;
    range->maxHorizontalBlankingClocks		= 0xffffffff;
    range->maxHorizontalSyncOffsetClocks	= 0xffffffff;
    range->maxHorizontalPulseWidthClocks	= 0xffffffff;
    range->maxVerticalActiveClocks		= 0xffffffff;
    range->maxVerticalBlankingClocks		= 0xffffffff;
    range->maxVerticalSyncOffsetClocks		= 0xffffffff;
    range->maxVerticalPulseWidthClocks		= 0xffffffff;
    range->maxHorizontalBorderLeft		= 0xffffffff;
    range->maxHorizontalBorderRight		= 0xffffffff;
    range->maxVerticalBorderTop			= 0xffffffff;
    range->maxVerticalBorderBottom		= 0xffffffff;

    range->charSizeHorizontalActive		= 1;
    range->charSizeHorizontalBlanking		= 1;
    range->charSizeHorizontalSyncOffset		= 1;
    range->charSizeHorizontalSyncPulse		= 1;
    range->charSizeVerticalBlanking		= 1;
    range->charSizeVerticalSyncOffset		= 1;
    range->charSizeVerticalSyncPulse		= 1;
    range->charSizeHorizontalBorderLeft		= 1;
    range->charSizeHorizontalBorderRight	= 1;
    range->charSizeVerticalBorderTop		= 1;
    range->charSizeVerticalBorderBottom		= 1;
    range->charSizeHorizontalTotal		= 1;
    range->charSizeVerticalTotal		= 1;
}

static Boolean
EDIDDescToDisplayTimingRangeRec( EDID * edid, EDIDGeneralDesc * desc,
                                 IODisplayTimingRange * range )
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
    range->supportedSignalLevels  = 1 << ((byte >> 5) & 3);
    range->supportedSyncFlags	  = ((byte & 1) ? kIORangeSupportsVSyncSerration : 0)
                                  | ((byte & 2) ? kIORangeSupportsSyncOnGreen : 0)
                                  | ((byte & 4) ? kIORangeSupportsCompositeSync : 0)
                                  | ((byte & 8) ? kIORangeSupportsVSyncSerration : 0);

    range->minFrameRate  = desc->data[0];
    range->maxFrameRate  = desc->data[1];
    range->minLineRate   = desc->data[2] * 1000;
    range->maxLineRate   = desc->data[3] * 1000;
    range->maxPixelClock = desc->data[4] * 10000000ULL;

    range->minHorizontalActiveClocks = 640;
    range->minVerticalActiveClocks   = 480;

    if( range->minLineRate)
        range->maxHorizontalActiveClocks = range->maxPixelClock / range->minLineRate;
    if( range->minFrameRate)
        range->maxVerticalActiveClocks   = range->maxPixelClock
                                            / (range->minHorizontalActiveClocks * range->minFrameRate);

    return( true );
}

static kern_return_t
DecodeStandardTiming( EDID * edid, UInt16 standardTiming, 
		      UInt32 * oWidth, UInt32 * height, float * refreshRate)
{
    UInt32 width;

    if( 0x0101 == standardTiming)
	return (kIOReturnBadArgument);

    width = ((standardTiming >> 8) + 31) << 3;
    *oWidth = width;
    switch( (standardTiming >> 6) & 3) {
	case 0:
	    if ((edid->version > 1) || (edid->revision >= 3))
		*height = (10 * width) / 16;
	    else
		*height = width;
	    break;
	case 2:
	    *height = (4 * width) / 5;
	    break;
	case 3:
	    *height = (9 * width) / 16;
	    break;
	default:
	case 1:
	    *height = (3 * width) / 4;
	    break;
    }

    if (refreshRate)
	*refreshRate = (float) ((standardTiming & 31) + 60);

    return (kIOReturnSuccess);
}

static IOReturn
EDIDDescToDetailedTiming( EDID * edid, EDIDDetailedTimingDesc * desc,
                            IODetailedTimingInformation * timing )
{

    bzero( timing, sizeof( IODetailedTimingInformation) );
    timing->__reservedA[0] = sizeof( IODetailedTimingInformation);	// csTimingSize

    if( !desc->clock)
        return( kIOReturnBadArgument );

    timing->signalConfig		= (edid->displayParams[0] & 16)
                                        ? kIOAnalogSetupExpected : 0;
    timing->signalLevels		= (edid->displayParams[0] >> 5) & 3;

    timing->pixelClock 			= ((UInt64) OSReadLittleInt16(&desc->clock, 0))
                                        * 10000ULL;

    timing->horizontalActive		= desc->horizActive
                                        | ((desc->horizHigh & 0xf0) << 4);
    timing->horizontalBlanking		= desc->horizBlanking
                                        | ((desc->horizHigh & 0x0f) << 8);

    timing->verticalActive 		= desc->verticalActive
                                        | ((desc->verticalHigh & 0xf0) << 4);
    timing->verticalBlanking  		= desc->verticalBlanking
                                        | ((desc->verticalHigh & 0x0f) << 8);

    timing->horizontalSyncOffset	= desc->horizSyncOffset
                                        | ((desc->syncHigh & 0xc0) << 2);
    timing->horizontalSyncPulseWidth	= desc->horizSyncWidth
                                        | ((desc->syncHigh & 0x30) << 4);

    timing->verticalSyncOffset		= ((desc->verticalSyncOffsetWidth & 0xf0) >> 4)
                                        | ((desc->syncHigh & 0x0c) << 4);
    timing->verticalSyncPulseWidth	= ((desc->verticalSyncOffsetWidth & 0x0f) >> 0)
                                        | ((desc->syncHigh & 0x03) << 4);

    timing->horizontalBorderLeft	= desc->horizBorder;
    timing->horizontalBorderRight	= desc->horizBorder;
    timing->verticalBorderTop		= desc->verticalBorder;
    timing->verticalBorderBottom	= desc->verticalBorder;

    timing->horizontalSyncConfig	= (desc->flags & 2)
                                        ? kIOSyncPositivePolarity : 0;
    timing->horizontalSyncLevel		= 0;
    timing->verticalSyncConfig		= (desc->flags & 4)
                                        ? kIOSyncPositivePolarity : 0;
    timing->verticalSyncLevel		= 0;

    return( kIOReturnSuccess );
}

static void
TimingToHost( const IODetailedTimingInformationV2 * _t1, IODetailedTimingInformationV2 * t2 )
{
    IODetailedTimingInformationV2 * t1 = (IODetailedTimingInformationV2 *) _t1;

    bcopy(t1, t2, sizeof(IODetailedTimingInformationV2));

    t2->scalerFlags      	 = OSReadBigInt32(&t1->scalerFlags, 0);
    t2->horizontalScaled 	 = OSReadBigInt32(&t1->horizontalScaled, 0);
    t2->verticalScaled   	 = OSReadBigInt32(&t1->verticalScaled, 0);
    t2->signalConfig     	 = OSReadBigInt32(&t1->signalConfig, 0);
    t2->signalLevels     	 = OSReadBigInt32(&t1->signalLevels, 0);

    t2->pixelClock    		 = OSReadBigInt64(&t1->pixelClock, 0);
    t2->minPixelClock		 = OSReadBigInt64(&t1->minPixelClock, 0);
    t2->maxPixelClock		 = OSReadBigInt64(&t1->maxPixelClock, 0);

    t2->horizontalActive	 = OSReadBigInt32(&t1->horizontalActive, 0);
    t2->horizontalBlanking	 = OSReadBigInt32(&t1->horizontalBlanking, 0);
    t2->horizontalSyncOffset	 = OSReadBigInt32(&t1->horizontalSyncOffset, 0);
    t2->horizontalSyncPulseWidth = OSReadBigInt32(&t1->horizontalSyncPulseWidth, 0);

    t2->verticalActive		 = OSReadBigInt32(&t1->verticalActive, 0);
    t2->verticalBlanking	 = OSReadBigInt32(&t1->verticalBlanking, 0);
    t2->verticalSyncOffset	 = OSReadBigInt32(&t1->verticalSyncOffset, 0);
    t2->verticalSyncPulseWidth	 = OSReadBigInt32(&t1->verticalSyncPulseWidth, 0);

    t2->horizontalBorderLeft 	 = OSReadBigInt32(&t1->horizontalBorderLeft, 0);
    t2->horizontalBorderRight 	 = OSReadBigInt32(&t1->horizontalBorderRight, 0);
    t2->verticalBorderTop 	 = OSReadBigInt32(&t1->verticalBorderTop, 0);
    t2->verticalBorderBottom 	 = OSReadBigInt32(&t1->verticalBorderBottom, 0);
    t2->horizontalSyncConfig 	 = OSReadBigInt32(&t1->horizontalSyncConfig, 0);
    t2->horizontalSyncLevel 	 = OSReadBigInt32(&t1->horizontalSyncLevel, 0);
    t2->verticalSyncConfig 	 = OSReadBigInt32(&t1->verticalSyncConfig, 0);
    t2->verticalSyncLevel 	 = OSReadBigInt32(&t1->verticalSyncLevel, 0);
}

static IOReturn
StandardResolutionToDetailedTiming( IOFBConnectRef connectRef, EDID * edid, 
				    IOFBResolutionSpec * spec,
				    IOTimingInformation * timing )
{
    CFDictionaryRef stdModes, timingIDs, dict;
    const void *    key;
    CFDataRef	    data;

    stdModes = CFDictionaryGetValue(connectRef->iographicsProperties, CFSTR("std-modes"));
    if (!stdModes)
	return (kIOReturnUnsupportedMode);

    key = (const void *) spec->timingID;
    if (!key)
    {
	timingIDs = CFDictionaryGetValue(connectRef->iographicsProperties, CFSTR("timing-ids"));
	if (!timingIDs)
	    return (kIOReturnUnsupportedMode);
	key = (const void *)((spec->width << 20) | (spec->height << 8) | ((UInt32)(spec->refreshRate + 0.5)));
	key = CFDictionaryGetValue(timingIDs, key);
    }
    dict = CFDictionaryGetValue(stdModes, key);

    if (!dict)
	return (kIOReturnUnsupportedMode);
    data = CFDictionaryGetValue(dict, CFSTR(kIOFBModeTMKey));
    if (!data)
	return (kIOReturnUnsupportedMode);

    TimingToHost( (const IODetailedTimingInformationV2 *) CFDataGetBytePtr(data), &timing->detailedInfo.v2 );

    timing->appleTimingID = (UInt32) key;

    return (kIOReturnSuccess);
}

static IOReturn
GTFToDetailedTiming( IOFBConnectRef connectRef, EDID * edid, 
		     IOFBResolutionSpec  * spec, UInt32 characterSize,
		     IODetailedTimingInformation * timing )
{
    SInt32	curve;
    int		vSyncRqd	= 3;
    float	hSyncPct	= 8.0/100.0;
    float	minVSyncBP	= 550e-6;    // s
    int		minPorchRnd	= 1;

    float	interlace	= spec->needInterlace ? 0.5 : 0.0;
    float	interlaceFactor = spec->needInterlace ? 2.0 : 1.0;
    float	vFieldRateRqd	= spec->refreshRate * interlaceFactor;

    // 1.
    int		hPixelsRnd	= roundf(spec->width / characterSize) * characterSize;
    int		vLinesRnd	= roundf(spec->height / interlaceFactor);
    
    // 4,5,15,16.
    int		topMargin	= 0;
    int		bottomMargin	= 0;
    int		leftMargin	= 0;
    int		rightMargin	= 0;

    // 7.
    float	hPeriodEst	= ((1 / vFieldRateRqd) - (minVSyncBP)) 
				/ (vLinesRnd + (2 * topMargin) + minPorchRnd + interlace);
    // 8.
    int		vSyncBP		= roundf( minVSyncBP / hPeriodEst );
    // 10.
    float	totalVLines	= vLinesRnd + topMargin + bottomMargin + vSyncBP + interlace + minPorchRnd;
    // 11.
    float	vFieldRateEst	= 1 / hPeriodEst / totalVLines;
    // 12.
    float	hPeriod		= hPeriodEst / (vFieldRateRqd / vFieldRateEst);
    // 17.
    int		totalActivePixels = hPixelsRnd + leftMargin + rightMargin;

    //
    for (curve = (connectRef->numGTFCurves - 1); curve >= 0; curve--)
    {
	if ((1 / hPeriod) > connectRef->gtfCurves[curve].startHFrequency)
	    break;
    }

    float	cPrime		= ((((float) connectRef->gtfCurves[curve].c) - ((float) connectRef->gtfCurves[curve].j)) 
				* ((float) connectRef->gtfCurves[curve].k) / 256.0) + ((float) connectRef->gtfCurves[curve].j);
    float	mPrime		= ((float) connectRef->gtfCurves[curve].k) / 256.0 * ((float) connectRef->gtfCurves[curve].m);

    // 18.
    float	idealDutyCycle	= cPrime - (mPrime * hPeriod * 1e6 / 1000.0);
    // 19.
    int		hBlankPixels	= roundf((totalActivePixels * idealDutyCycle / (100.0 - idealDutyCycle)
				/ (2 * characterSize))) * 2 * characterSize;
    // 20.
    int		totalPixels	= totalActivePixels + hBlankPixels;
    // 21.
    float	pixelFreq	= totalPixels / hPeriod;

    // stage 2 - 17.
    int		hSyncPixels	= roundf( hSyncPct * totalPixels / characterSize) * characterSize;
    int		hFPPixels	= (hBlankPixels / 2) - hSyncPixels;
    // 30.
    float	vOddBlankingLines = vSyncBP + minPorchRnd;
    // 36.
    float	vOddFPLines	= minPorchRnd + interlace;

    // --
    bzero( timing, sizeof( IODetailedTimingInformation) );
    timing->__reservedA[0] = sizeof( IODetailedTimingInformation);	// csTimingSize

    if (edid)
    {
        timing->signalConfig		= (edid->displayParams[0] & 16)
                                        ? kIOAnalogSetupExpected : 0;
        timing->signalLevels		= (edid->displayParams[0] >> 5) & 3;
    }
    else
    {
        timing->signalConfig		= kIOAnalogSetupExpected;
        timing->signalLevels		= kIOAnalogSignalLevel_0700_0300;
    }

    timing->pixelClock 			= pixelFreq;

    timing->horizontalActive		= totalActivePixels;
    timing->horizontalBlanking		= hBlankPixels;

    timing->verticalActive 		= vLinesRnd;
    timing->verticalBlanking  		= vOddBlankingLines;

    timing->horizontalSyncOffset	= hFPPixels;
    timing->horizontalSyncPulseWidth	= hSyncPixels;

    timing->verticalSyncOffset		= vOddFPLines;
    timing->verticalSyncPulseWidth	= vSyncRqd;

    timing->horizontalBorderLeft	= leftMargin;
    timing->horizontalBorderRight	= rightMargin;
    timing->verticalBorderTop		= topMargin;
    timing->verticalBorderBottom	= bottomMargin;

    timing->horizontalSyncConfig	= (curve == 0)
                                        ? 0 : kIOSyncPositivePolarity;
    timing->horizontalSyncLevel		= 0;
    timing->verticalSyncConfig		= (curve == 0)
                                        ? kIOSyncPositivePolarity : 0;
    timing->verticalSyncLevel		= 0;

    return( kIOReturnSuccess );
}

static Boolean
CheckTimingWithRange( IODisplayTimingRange * range,
                        IODetailedTimingInformation * timing )
{
    UInt64	pixelClock;
    UInt64	rate;
    UInt64	hTotal, vTotal;

    if( kIODigitalSignal & timing->signalConfig)
        return( false);

//    if( 0 == (range->supportedSyncFlags & (1 << (timing->signalLevels))))
//        return( false);
//    if( 0 == (range->supportedSignalLevels & (1 << (timing->signalLevels))))
//        return( false);

    pixelClock = timing->pixelClock;
    hTotal  = timing->horizontalActive;
    hTotal += timing->horizontalBlanking;
    vTotal  = timing->verticalActive;
    vTotal += timing->verticalBlanking;

    if( (pixelClock > range->maxPixelClock)
     || (pixelClock < range->minPixelClock))
        return( false);

    // line rate
    rate = pixelClock / hTotal;
    if( (rate > range->maxLineRate)
     || (rate < range->minLineRate))
        return( false);

    // frame rate
    rate = pixelClock / (hTotal * vTotal);
    if( (rate > range->maxFrameRate)
     || (rate < range->minFrameRate))
        return( false);

    if( hTotal > range->maxHorizontalTotal)
        return( false);
    if( vTotal > range->maxVerticalTotal)
        return( false);

    if( (timing->horizontalActive > range->maxHorizontalActiveClocks)
     || (timing->horizontalActive < range->minHorizontalActiveClocks))
        return( false);
    if( (timing->verticalActive > range->maxVerticalActiveClocks)
     || (timing->verticalActive < range->minVerticalActiveClocks))
        return( false);

/*
    if( (timing->horizontalBlanking > range->maxHorizontalBlankingClocks)
     || (timing->horizontalBlanking < range->minHorizontalBlankingClocks))
        return( false);
    if( (timing->verticalBlanking > range->maxVerticalBlankingClocks)
     || (timing->verticalBlanking < range->minVerticalBlankingClocks))
        return( false);
*/
    if( (timing->horizontalSyncOffset > range->maxHorizontalSyncOffsetClocks)
     || (timing->horizontalSyncOffset < range->minHorizontalSyncOffsetClocks))
        return( false);
    if( (timing->horizontalSyncPulseWidth > range->maxHorizontalPulseWidthClocks)
     || (timing->horizontalSyncPulseWidth < range->minHorizontalPulseWidthClocks))
        return( false);

    if( (timing->verticalSyncOffset > range->maxVerticalSyncOffsetClocks)
     || (timing->verticalSyncOffset < range->minVerticalSyncOffsetClocks))
        return( false);
    if( (timing->verticalSyncPulseWidth > range->maxVerticalPulseWidthClocks)
     || (timing->verticalSyncPulseWidth < range->minVerticalPulseWidthClocks))
        return( false);

    if( (timing->horizontalBorderLeft > range->maxHorizontalBorderLeft)
     || (timing->horizontalBorderLeft < range->minHorizontalBorderLeft))
        return( false);
    if( (timing->horizontalBorderRight > range->maxHorizontalBorderRight)
     || (timing->horizontalBorderRight < range->minHorizontalBorderRight))
        return( false);
    if( (timing->verticalBorderTop > range->maxVerticalBorderTop)
     || (timing->verticalBorderTop < range->minVerticalBorderTop))
        return( false);
    if( (timing->verticalBorderBottom > range->maxVerticalBorderBottom)
     || (timing->verticalBorderBottom < range->minVerticalBorderBottom))
        return( false);

    if( timing->horizontalActive & (range->charSizeHorizontalActive - 1))
        return( false);
    if( timing->horizontalBlanking & (range->charSizeHorizontalBlanking - 1))
        return( false);
    if( timing->horizontalSyncOffset & (range->charSizeHorizontalSyncOffset - 1))
        return( false);
    if( timing->horizontalSyncPulseWidth & (range->charSizeHorizontalSyncPulse - 1))
        return( false);
    if( timing->verticalBlanking & (range->charSizeVerticalBlanking - 1))
        return( false);
    if( timing->verticalSyncOffset & (range->charSizeVerticalSyncOffset - 1))
        return( false);
    if( timing->verticalSyncPulseWidth & (range->charSizeVerticalSyncPulse - 1))
        return( false);
    if( timing->horizontalBorderLeft & (range->charSizeHorizontalBorderLeft - 1))
        return( false);
    if( timing->horizontalBorderRight & (range->charSizeHorizontalBorderRight - 1))
        return( false);
    if( timing->verticalBorderTop & (range->charSizeVerticalBorderTop - 1))
        return( false);
    if( timing->verticalBorderBottom & (range->charSizeVerticalBorderBottom - 1))
        return( false);
    if( hTotal & (range->charSizeHorizontalTotal - 1))
        return( false);
    if( vTotal & (range->charSizeVerticalTotal - 1))
        return( false);

    return( true );
}

static Boolean
HasEstablishedTiming( IOFBConnectRef connectRef, UInt32 appleTimingID )
{
    CFDataRef data;
    UInt32 *  establishedIDs;
    UInt32    i;

    if (kIOTimingIDInvalid == appleTimingID)
	return (false);

    data = CFDictionaryGetValue(connectRef->iographicsProperties, CFSTR("established-ids"));
    if (data)
	establishedIDs = (UInt32 *) CFDataGetBytePtr(data);
    else
	establishedIDs = 0;

    for( i = 0; 
	 establishedIDs && (i < 24) && (appleTimingID != OSReadBigInt32(&establishedIDs[i], 0));
	 i++ )	{}

    return( i < 24 );
}

__private_extern__ IOReturn
IOCheckTimingWithDisplay( IOFBConnectRef connectRef,
			  const IOTimingInformation * timing,
			  IOOptionBits modeGenFlags )
{
    IOReturn	result;
    CFDataRef	edidData;
    CFDataRef	data;

    do 
    {
        if (connectRef->fbRange && !(kIOFBDriverMode & modeGenFlags))
	{
	    unsigned int len;

	    if (!CheckTimingWithRange(connectRef->fbRange, 
					(IODetailedTimingInformation *) &timing->detailedInfo.v2))
	    {
#if LOG
		printf("Out of fb\n");
#endif
		result = kIOReturnUnsupportedMode;
		continue;
	    }
	    len = sizeof(IODetailedTimingInformation);
	    result = io_connect_method_structureI_structureO( connectRef->connect, 17, /*index*/
			    (void *) &timing->detailedInfo.v2, len, (void *) &timing->detailedInfo.v2, &len);
	    if (kIOReturnSuccess != result)
	    {
#if LOG
		printf("preflight (%x)\n", result);
#endif
		result = kIOReturnUnsupportedMode;
		continue;
	    }
	}

	result = kIOReturnNotFound;
	if(!connectRef->overrides)
	    continue;

	if (kIOFBDriverMode & modeGenFlags)
	{
	    edidData = CFDictionaryGetValue(connectRef->overrides, CFSTR(kIODisplayEDIDKey));
	    if (edidData && HasEstablishedTiming(connectRef, timing->appleTimingID))
		continue;
	}

	if (kIODetailedTimingValid & timing->flags)
	{
	    CFNumberRef num;

	    num = CFDictionaryGetValue( connectRef->overrides, CFSTR("sync") );
	    if( num)
	    {
		UInt32 hSyncMask, hSyncValue, vSyncMask, vSyncValue;
    
		CFNumberGetValue( num, kCFNumberSInt32Type, &vSyncValue );
		hSyncMask  = 0xff & (vSyncValue >> 24);
		hSyncValue = 0xff & (vSyncValue >> 16);
		vSyncMask  = 0xff & (vSyncValue >> 8);
		vSyncValue = 0xff & (vSyncValue >> 0);
		if ((hSyncValue != (timing->detailedInfo.v2.horizontalSyncConfig & hSyncMask))
		 || (vSyncValue != (timing->detailedInfo.v2.verticalSyncConfig   & vSyncMask)))
		{
		    result = kIOReturnUnsupportedMode;
		    continue;
		}
	    }
    
	    data = CFDictionaryGetValue(connectRef->overrides, CFSTR("trng"));
	    if (data && ((kIOFBGTFMode | kIOFBStdMode | kIOFBDriverMode) & modeGenFlags))
	    {
		result = CheckTimingWithRange((IODisplayTimingRange *) CFDataGetBytePtr(data),
					      (IODetailedTimingInformation *) &timing->detailedInfo.v2)
			? kIOReturnSuccess : kIOReturnUnsupportedMode;
	    }
	}
    }
    while (false);

    return (result);
}

static kern_return_t
InstallTiming( IOFBConnectRef             connectRef, 
		IOTimingInformation	* timing,
		IOOptionBits              dmFlags,
		IOOptionBits              modeGenFlags )
{
    IOReturn			err;
    IODisplayModeInformation	dmInfo;

    err = IOCheckTimingWithDisplay( connectRef, timing, modeGenFlags );
    if(kIOReturnUnsupportedMode == err)
        return( err );

    if ((kIOFBEDIDStdEstMode | kIOFBEDIDDetailedMode) & modeGenFlags)
    {
	if ((0xffffffff == connectRef->dimensions.width)
	|| (timing->detailedInfo.v2.horizontalActive > connectRef->dimensions.width))
	    connectRef->dimensions.width = timing->detailedInfo.v2.horizontalActive;
	if ((0xffffffff == connectRef->dimensions.height)
	|| (timing->detailedInfo.v2.verticalActive > connectRef->dimensions.height))
	    connectRef->dimensions.height = timing->detailedInfo.v2.verticalActive;
    }
    else
    {
	if( (timing->detailedInfo.v2.horizontalActive > connectRef->dimensions.width)
	    || (timing->detailedInfo.v2.verticalActive > connectRef->dimensions.height)) {
	    dmFlags |= connectRef->dimensions.setFlags;
	    dmFlags &= ~connectRef->dimensions.clearFlags;
	}
	if (kIOReturnSuccess != err)
	    dmFlags &= ~kDisplayModeSafeFlag;
    }

    bzero( &dmInfo, sizeof( dmInfo ));
    dmInfo.flags = dmFlags;
    err = IOFBInstallMode( connectRef, 0xffffffff, &dmInfo, timing, 0, modeGenFlags );

    return( err );
}

static kern_return_t
InstallFromEDIDDesc( IOFBConnectRef connectRef, 
                            EDID * edid, EDIDDetailedTimingDesc * desc )
{
    IOReturn		err;
    IOTimingInformation	timing;

    bzero( &timing, sizeof( timing ));
    timing.flags = kIODetailedTimingValid;

    err = EDIDDescToDetailedTiming( edid, desc, (IODetailedTimingInformation *) &timing.detailedInfo.v2 );
    if( kIOReturnSuccess != err)
        return( err );

    if (!connectRef->defaultWidth)
    {
	connectRef->defaultWidth       = timing.detailedInfo.v2.horizontalActive;
	connectRef->defaultHeight      = timing.detailedInfo.v2.verticalActive;
	connectRef->defaultImageWidth  = desc->horizImageSize    | ((desc->imageSizeHigh & 0xf0) << 4);
	connectRef->defaultImageHeight = desc->verticalImageSize | ((desc->imageSizeHigh & 0x0f) << 8);
    }
    err = InstallTiming( connectRef, &timing,
			    kDisplayModeValidFlag | kDisplayModeSafeFlag,
			    kIOFBEDIDDetailedMode );

    return( err );
}

static kern_return_t
InstallFromTimingOverride( IOFBConnectRef connectRef, IODetailedTimingInformation * desc)
{
    IOReturn		err;
    IOTimingInformation	timing;

    bzero( &timing, sizeof( timing ));
    timing.flags = kIODetailedTimingValid;

    TimingToHost( desc, &timing.detailedInfo.v2 );

    if (!connectRef->defaultWidth)
    {
	connectRef->defaultWidth       = timing.detailedInfo.v2.horizontalActive;
	connectRef->defaultHeight      = timing.detailedInfo.v2.verticalActive;
	// doh!:
	connectRef->defaultImageWidth  = timing.detailedInfo.v2.horizontalActive;
	connectRef->defaultImageHeight = timing.detailedInfo.v2.verticalActive;
    }
    err = InstallTiming( connectRef, &timing,
			    kDisplayModeValidFlag | kDisplayModeSafeFlag,
			    kIOFBEDIDDetailedMode );

    return( err );
}

static IOReturn
InstallTimingForResolution( IOFBConnectRef connectRef, EDID * edid,
			    IOFBResolutionSpec * spec,
			    IOOptionBits dmFlags, IOOptionBits modeGenFlags )
{
    IOReturn		err;
    CFNumberRef		num;
    IOTimingInformation	timing;

    bzero( &timing, sizeof( timing ));
    timing.flags = kIODetailedTimingValid;

    do
    {
	err = StandardResolutionToDetailedTiming( connectRef, edid, spec, &timing );

	if (kIOReturnSuccess == err)
	{
#if LOG
	    printf("Using std-modes for %ldx%ld@%ld, %ld\n",
		    spec->width, spec->height, (UInt32)(spec->refreshRate + 0.5), timing.appleTimingID);
#endif
	    if (kIOFBGTFMode & modeGenFlags)
	    {
		modeGenFlags = kIOFBStdMode;
		dmFlags      = kDisplayModeValidFlag | kDisplayModeSafeFlag;
	    }
	}
	else
	{
#if LOG
	    printf("Not using std-modes for %ldx%ld@%ld, %ld\n",
		    spec->width, spec->height, (UInt32)(spec->refreshRate + 0.5), timing.appleTimingID);
#endif
	    if (connectRef->overrides && CFDictionaryGetValue(connectRef->overrides, CFSTR(kIODisplayIsDigitalKey)))
	    {
		err = kIOReturnUnsupportedMode;
		continue;
	    }
	    err = GTFToDetailedTiming( connectRef, edid, spec, 8,
					  (IODetailedTimingInformation *) &timing.detailedInfo.v2 );
	    if( kIOReturnSuccess != err)
		continue;
	}

	if( connectRef->overrides
	 && (num = CFDictionaryGetValue( connectRef->overrides, CFSTR("sync") )))
	 {
	    UInt32 hSyncMask, hSyncValue, vSyncMask, vSyncValue;

	    CFNumberGetValue( num, kCFNumberSInt32Type, &vSyncValue );
	    hSyncMask  = 0xff & (vSyncValue >> 24);
	    hSyncValue = 0xff & (vSyncValue >> 16);
	    vSyncMask  = 0xff & (vSyncValue >> 8);
	    vSyncValue = 0xff & (vSyncValue >> 0);
	    if ((hSyncValue != (timing.detailedInfo.v2.horizontalSyncConfig & hSyncMask))
	     || (vSyncValue != (timing.detailedInfo.v2.verticalSyncConfig   & vSyncMask)))
	    {
		err = kIOReturnUnsupportedMode;
		continue;
	    }
	}
    
	err = InstallTiming( connectRef, &timing, dmFlags, modeGenFlags );
    }
    while (false);
    
    return (err);
}

static IOReturn
InstallGTFResolution( IOFBConnectRef connectRef, EDID * edid,
                      float h, float v, float nativeAspect )
{
    IOReturn	 	err = kIOReturnSuccess;
    CFArrayRef	 	array;
    CFTypeRef	 	obj;
    CFIndex	 	count, i;
    IOOptionBits	dmFlags;
    IOFBResolutionSpec	spec = { 0 };
    UInt32       	width  = (UInt32) h;
    UInt32       	height = (UInt32) v;		    // rounding?
    Boolean	 	gtfDisplay;

    if (width > connectRef->dimensions.width)
        return (kIOReturnNoSpace);
    if (height > connectRef->dimensions.height)
        return (kIOReturnNoSpace);

    array = CFDictionaryGetValue( connectRef->iographicsProperties, CFSTR("gtf-refresh-rates") );
    count = array ? CFArrayGetCount(array) : 0;

    gtfDisplay = (edid && (0 != (edid->displayParams[4] & 1)));

    dmFlags = kDisplayModeValidFlag;
    if (gtfDisplay)
	dmFlags |= kDisplayModeSafeFlag;
    if( !gtfDisplay || (ratioOver( nativeAspect, h / v ) > 1.03125))
        dmFlags |= kDisplayModeNotPresetFlag;

    spec.width         = width;
    spec.height        = height;
    spec.needInterlace = false;

    for (i = 0; i < count; i++)
    {
        obj = CFArrayGetValueAtIndex(array, i);
        if( CFNumberGetTypeID() != CFGetTypeID(obj))
            continue;
        CFNumberGetValue( (CFNumberRef) obj, kCFNumberFloatType, &spec.refreshRate );

	err = InstallTimingForResolution( connectRef, edid,
                                          &spec, dmFlags, kIOFBGTFMode );
    }
    
    return (err);
}

static void
InstallStandardEstablishedTiming(
		    IOFBConnectRef connectRef, EDID * edid,
		    IOFBResolutionSpec * spec )

{
    InstallTimingForResolution( connectRef, edid, spec,
                                kDisplayModeValidFlag | kDisplayModeSafeFlag, kIOFBEDIDStdEstMode );
}

static void
InstallStandardEstablishedTimings( IOFBConnectRef connectRef, EDID * edid  )
{
    CFDataRef		data;
    IOFBResolutionSpec	spec = { 0 };
    UInt32 *		establishedIDs;
    UInt32		i;

    data = CFDictionaryGetValue(connectRef->iographicsProperties, CFSTR("established-ids"));
    if (data)
	establishedIDs = (UInt32 *) CFDataGetBytePtr(data);
    else
	establishedIDs = 0;

    spec.needInterlace = false;

    for( i = 7; establishedIDs && (i < 24); i++ )
    {
        if (0 != (edid->establishedTimings[ 2 - (i / 8) ] & (1 << (i % 8))))
	{
	    spec.timingID = OSReadBigInt32(&establishedIDs[i], 0);
	    if (spec.timingID != kIOTimingIDInvalid)
		InstallStandardEstablishedTiming(connectRef, edid, &spec);
	}
    }

    for( i = 0; i < 8; i++ )
    {
	spec.timingID = kIOTimingIDInvalid;
	if (kIOReturnSuccess != DecodeStandardTiming(edid, OSReadBigInt16(&edid->standardTimings[i], 0),
							&spec.width, &spec.height, &spec.refreshRate))
	    continue;
	InstallStandardEstablishedTiming(connectRef, edid, &spec);
    }
}

static Boolean
IODisplayConsiderAspect( float w, float h, float * aspectWidth, float * aspectHeight )
{
    float ratio;

    if (!w || !h)
	return (false);

    ratio = w / h;

    if ((ratio > 1.85) || (ratio < 1.2))
    {
	*aspectWidth = w;
	*aspectHeight = h;
	return (true);
    }

    if (ratio > 1.65)
    {
	*aspectWidth = 16.0;
	*aspectHeight = 9.0;
	return (true);
    }

    if (ratio > 1.55)
    {
	*aspectWidth = 16.0;
	*aspectHeight = 10.0;
	return (true);
    }

    if (ratio > 1.45)
    {
	*aspectWidth = 3.0;
	*aspectHeight = 2.0;
	return (true);
    }

    return (false);
}

static void
IODisplayGetAspect( IOFBConnectRef connectRef, 
                    EDID * edid,
                    float * aspectWidth, float * aspectHeight )
{
    CFDictionaryRef ovr;
    CFNumberRef     imageH, imageV;
    float	    w, h;

    *aspectWidth = 4.0;
    *aspectHeight = 3.0;

    ovr = connectRef->overrides;

    do
    {
	if (IODisplayConsiderAspect(connectRef->defaultWidth, connectRef->defaultHeight,
				    aspectWidth, aspectHeight))
	    break;

	if (IODisplayConsiderAspect(connectRef->defaultImageWidth, connectRef->defaultImageHeight,
				    aspectWidth, aspectHeight))
	    break;

	if( ovr)
	{
	    imageH = CFDictionaryGetValue( ovr, CFSTR(kDisplayHorizontalImageSize) );
	    imageV = CFDictionaryGetValue( ovr, CFSTR(kDisplayVerticalImageSize) );
	    if (imageH && imageV)
	    {
		CFNumberGetValue( imageH, kCFNumberFloatType, &w );
		CFNumberGetValue( imageV, kCFNumberFloatType, &h );
		if (IODisplayConsiderAspect(w, h, aspectWidth, aspectHeight))
		    break;
	    } 
	}
    }
    while (false);
}


static kern_return_t
InstallGTFTimings( IOFBConnectRef connectRef, EDID * edid )
{
    IOReturn		err = kIOReturnSuccess;
    UInt32		i;
    CFArrayRef		array;
    CFIndex		count;
    CFStringRef		key;
    float		h, v, nh, nv, nativeAspect;
    Boolean		wide, displayNot4By3;

    // arb timings

    IODisplayGetAspect( connectRef, edid, &nh, &nv );

    nativeAspect = nh / nv;
    wide = (nativeAspect > 1.45);
    displayNot4By3 = (ratioOver(nativeAspect, 4.0 / 3.0) > 1.03125);

    key = wide ? CFSTR("gtf-resolutions-wide") : CFSTR("gtf-resolutions");
    array = CFDictionaryGetValue( connectRef->iographicsProperties, key );
    count = array ? CFArrayGetCount(array) : 0;

    for( i = 0; i < count; i++)
    {
        CFTypeRef obj;

        obj = CFArrayGetValueAtIndex(array, i);
        if( CFNumberGetTypeID() == CFGetTypeID(obj)) {
            SInt32	value;
            CFNumberGetValue( (CFNumberRef) obj, kCFNumberSInt32Type, &value );
            h = (float) (value & 0xffff);
            v = (float) (value >> 16);

        } else
            continue;

	if (v)
        {
            err = InstallGTFResolution( connectRef, edid,
                                        h, v, nativeAspect );
        }
        else
        {
            if (displayNot4By3)
                err = InstallGTFResolution( connectRef, edid,
                                            h, (h * 3.0) / 4.0, nativeAspect );

            err = InstallGTFResolution( connectRef, edid,
                                        h, (h * nv) / nh, nativeAspect );
        }
    }

    return( err );
}

__private_extern__ void
IODisplayInstallTimings( IOFBConnectRef connectRef )
{
    int				i;
    io_service_t		service = connectRef->framebuffer;
    EDID *			edid = 0;
    CFDataRef			fbRange;
    CFDataRef			data;
    CFArrayRef			array;
    CFIndex			count;

    static const GTFTimingCurve defaultGTFCurves[] = {
	{ 0,          40, 600, 128, 20 },
	{ 0xffffffff,  0,   0,   0,  0 }
    };

    // controller timing range
    fbRange = (CFDataRef) IORegistryEntryCreateCFProperty( service, 
							    CFSTR(kIOFBTimingRangeKey),
							    kCFAllocatorDefault, kNilOptions);
    if (fbRange && CFDataGetLength(fbRange) >= sizeof(IODisplayTimingRange))
	connectRef->fbRange = (IODisplayTimingRange *) CFDataGetBytePtr(fbRange);

    connectRef->numGTFCurves = 1;
    bcopy(&defaultGTFCurves, &connectRef->gtfCurves, sizeof(connectRef->gtfCurves));

    do 
    {
        // EDID timings

        data = CFDictionaryGetValue( connectRef->overrides, CFSTR(kIODisplayEDIDKey) );
        if( !data || (CFDataGetLength(data) < sizeof( EDID)) )
            continue;

        edid = (EDID *) CFDataGetBytePtr( data );

        // max dimensions
        connectRef->dimensions.setFlags = kDisplayModeNotPresetFlag;
        if( CFDictionaryGetValue( connectRef->overrides, CFSTR(kIODisplayIsDigitalKey)))
            connectRef->dimensions.clearFlags = 
		kDisplayModeValidFlag | kDisplayModeSafeFlag | kDisplayModeDefaultFlag;
        else
            connectRef->dimensions.clearFlags =
		kDisplayModeSafeFlag | kDisplayModeDefaultFlag;

        // override timing recs (18-byte)
        array = (CFArrayRef) CFDictionaryGetValue( connectRef->overrides, CFSTR("dspc"));
        if( array)
            count = CFArrayGetCount(array);
        else
            count = 0;
        for( i = 0; i < count; i++ ) {
            data = CFArrayGetValueAtIndex(array, i);
            if( !data || (sizeof(EDIDDetailedTimingDesc) != CFDataGetLength(data)))
                continue;

            InstallFromEDIDDesc( connectRef,
 				 edid, (EDIDDetailedTimingDesc *) CFDataGetBytePtr(data) );
        }

        // EDID timing recs
	for( i = 0; i < 4; i++ )
	{
            if( i && (0 == bcmp( &edid->descriptors[0].timing,
                                 &edid->descriptors[i].timing,
                                 sizeof( EDIDDetailedTimingDesc))))
                continue;
            InstallFromEDIDDesc( connectRef,
                                 edid,
                                 &edid->descriptors[i].timing );
        }

        InstallStandardEstablishedTimings( connectRef, edid );
    }
    while( false );

    // override timing recs
    array = (CFArrayRef) CFDictionaryGetValue( connectRef->overrides, CFSTR("tspc"));
    if (array)
	count = CFArrayGetCount(array);
    else
	count = 0;
    for (i = 0; i < count; i++ ) {
	data = CFArrayGetValueAtIndex(array, i);
	if( !data || (sizeof(IODetailedTimingInformation) != CFDataGetLength(data)))
	    continue;

	InstallFromTimingOverride(connectRef, 
		(IODetailedTimingInformation *) CFDataGetBytePtr(data));
    }

    if (CFDictionaryGetValue(connectRef->overrides, CFSTR("trng"))
    || ((!CFDictionaryGetValue(connectRef->overrides, CFSTR(kIODisplayIsDigitalKey)))
	&& ((connectRef->displayVendor != kDisplayVendorIDUnknown)
	 || (connectRef->displayProduct == kDisplayProductIDGeneric))
	&& (!edid || ((0xffffffff != connectRef->dimensions.width)
		    && (0xffffffff != connectRef->dimensions.height)))))
    {
	// have range limits, or analog-VGA/nonsensed
	InstallGTFTimings( connectRef, edid );
    }

    connectRef->fbRange = 0;
    if (fbRange)
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
_IODisplayCreateInfoDictionary(
    IOFBConnectRef		connectRef,
    io_service_t		framebuffer,
    IOOptionBits		options )
{
    IOReturn			kr;
    io_service_t		service = 0;
    CFDataRef			data = 0;
    CFNumberRef			num;
    CFMutableDictionaryRef	dict = 0;
    CFMutableDictionaryRef	regDict;
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
    IODisplayTimingRange	displayRange;

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

//	if (!connectRef || !connectRef->dependentID || connectRef->dependentIndex)
	{
	    vm_offset_t 		bytes;
	    vm_size_t			byteLen;
    
	    if (kIOReturnSuccess == readFile( "/testedid", &bytes, &byteLen ))
	    {
		data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
						    (const void *) bytes, byteLen, kCFAllocatorNull );
//		vm_deallocate( mach_task_self(), bytes, byteLen );
	    }
	    else
		data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
						    spoofEDID, 128, kCFAllocatorNull );

	}

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
            sint = OSReadBigInt32((void *) CFDataGetBytePtr(data), 0);
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

    ivalue = roundf((value * (((float) max) - ((float) min)) + ((float) min)));

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
    return( _IODisplayCreateInfoDictionary( NULL, framebuffer, options));
}

CFDictionaryRef
IODisplayCreateInfoDictionary(
	io_service_t		framebuffer,
	IOOptionBits		options )
{
    return( _IODisplayCreateInfoDictionary( NULL, framebuffer, options));
}
