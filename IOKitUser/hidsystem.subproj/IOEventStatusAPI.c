/*
 * Copyright (c) 1991 NeXT Computer, Inc.
 * Copyright (c) 1996-2000 Apple Computer, Inc. All rights reserved.
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
/******************************************************************************
	event_status_driver_api.c
	API for the events status driver.
	This file implements public API.
	mpaque 11Oct91
	
	Modified:
	29 June 1992 Mike Paquette at NeXT
		Implemented API for the new Mach based Event Driver.
	
******************************************************************************/

#include <mach/mach.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/hidsystem/event_status_driver.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

#include <math.h>
#include <limits.h>

/* Definitions specific to the Mach based event driver */
#define BRIGHT_MAX 64

static void secs_to_packed_nsecs(double secs, UInt64 *nsecs)
{
    *nsecs = (1000.0 * 1000.0 * 1000.0 * secs);
}

static double packed_nsecs_to_secs(UInt64 nsecs)
{
    return( ((double)nsecs) / 1000.0 / 1000.0 / 1000.0);
}

/* Open and Close */
NXEventHandle NXOpenEventStatus(void)
{
    NXEventHandle 		handle = MACH_PORT_NULL;
    register kern_return_t	kr;
    io_service_t		service = MACH_PORT_NULL;
    mach_port_t			masterPort;

    do {

	kr = IOMasterPort( MACH_PORT_NULL, &masterPort );
	if( kr != KERN_SUCCESS)
	    break;

        service = IORegistryEntryFromPath( masterPort,
                    kIOServicePlane ":/IOResources/IOHIDSystem" );
	if( !service)
	    break;

        kr = IOServiceOpen( service,
			mach_task_self(),
			kIOHIDParamConnectType,
			&handle);

        IOObjectRelease( service );

    } while( false );

    return( handle );
}

void NXCloseEventStatus(NXEventHandle handle)
{
    IOServiceClose( handle );
}

/* Status query */
NXEventSystemInfoType NXEventSystemInfo(NXEventHandle handle,
					char *flavor,
					NXEventSystemInfoType evs_info,
					unsigned int *evs_info_cnt)
{
    kern_return_t kr;
    NXEventSystemDevice * info = (NXEventSystemDevice *) evs_info;
    int	maxDeviceCount = (*evs_info_cnt) * sizeof( int) / sizeof( NXEventSystemDevice);
    int deviceCount = 0;

    io_iterator_t iter;
    io_registry_entry_t hidsystem;
    io_registry_entry_t hiddevice;

    CFDictionaryRef	dict;
    CFNumberRef		num;
    SInt32		val;

    // Translate the one existing old case to new format
    if ( ((int)flavor) == __OLD_NX_EVS_DEVICE_INFO )
            flavor = NX_EVS_DEVICE_INFO;

    if( strcmp( flavor, NX_EVS_DEVICE_INFO))
        kr = kIOReturnUnsupported;

   do {
	kr = IOConnectGetService( handle, &hidsystem );
	if( KERN_SUCCESS != kr )
	    break;

	kr = IORegistryEntryGetParentIterator( hidsystem,
			kIOServicePlane, &iter );

        IOObjectRelease( hidsystem );

	if( KERN_SUCCESS != kr )
	    break;

	while( (deviceCount < maxDeviceCount)
	    && (hiddevice = IOIteratorNext( iter ))) {

	    kr = IORegistryEntryCreateCFProperties(hiddevice, &dict,
                                    kCFAllocatorDefault, kNilOptions);
	    IOObjectRelease( hiddevice );
            if( KERN_SUCCESS != kr )
                continue;

	    if( (num = CFDictionaryGetValue( dict, CFSTR(kIOHIDKindKey )))) {

		CFNumberGetValue( num, kCFNumberSInt32Type, &val );
                info[ deviceCount ].dev_type = val;

                if( (num = CFDictionaryGetValue( dict, CFSTR(kIOHIDInterfaceIDKey ))))
                    CFNumberGetValue( num, kCFNumberSInt32Type, &val );
		else
		    val = 0;
                info[ deviceCount ].interface = val;

                if( (num = CFDictionaryGetValue( dict, CFSTR(kIOHIDSubinterfaceIDKey ))))
                    CFNumberGetValue( num, kCFNumberSInt32Type, &val );
		else
		    val = 0;
                info[ deviceCount ].id = val;

                info[ deviceCount ].interface_addr = 0;

		deviceCount++;
	    }
	    CFRelease(dict);
	}
	IOObjectRelease( iter );

    } while( false );

    if ( kr == KERN_SUCCESS )
	*evs_info_cnt = (deviceCount * sizeof( NXEventSystemDevice) / sizeof( int));
    else
	evs_info = (NXEventSystemInfoType) 0;

    return evs_info;
}

kern_return_t IOHIDSetParameter( io_connect_t handle, CFStringRef key,
                const void * bytes, IOByteCount size )
{
    kern_return_t		kr = kIOReturnNoMemory;
    CFNumberType		numberType;
    CFNumberRef			numberRef;

    if (!bytes || !size)
        return kIOReturnError;

    switch ( size )
    {
        case sizeof(UInt64):
            numberType = kCFNumberSInt64Type;
            break;
        case sizeof(UInt32):
            numberType = kCFNumberSInt32Type;
            break;
        case sizeof(UInt16):
            numberType = kCFNumberSInt16Type;
            break;
        case sizeof(UInt8):
            numberType = kCFNumberSInt8Type;
            break;
        default:
            numberType = kCFNumberSInt32Type;
            break;
    }

    numberRef = CFNumberCreate(kCFAllocatorDefault, numberType, bytes);
    if( numberRef)
    {
        kr = IOConnectSetCFProperty( handle, key, numberRef );
        CFRelease(numberRef);
    }


    return( kr );
}

kern_return_t IOHIDSetCFTypeParameter( io_connect_t handle, CFStringRef key, CFTypeRef parameter)
{
    kern_return_t		kr = kIOReturnError;

    if ( parameter )
        kr = IOConnectSetCFProperty( handle, key, parameter );
    else
        kr = kIOReturnBadArgument;
    
    return( kr );
}

#ifndef kIOHIDParametersKey
#define kIOHIDParametersKey		"HIDParameters"
#endif

kern_return_t IOHIDGetParameter( io_connect_t handle, CFStringRef key,
                IOByteCount maxSize, void * bytes, IOByteCount * actualSize )
{
    kern_return_t	kr;
    io_service_t	hidsystem;
    CFDataRef		data;
    CFNumberRef		number;
    CFNumberType	numberType;
    CFTypeRef		typeRef = NULL;
    int 		copySize;

    if (!bytes || !maxSize)
        return kIOReturnBadArgument;

    kr = IOHIDCopyCFTypeParameter(handle, key, &typeRef);

    if ( kr != kIOReturnSuccess )
        return kr;

    if( !typeRef )
        return kIOReturnBadArgument;

    if (CFGetTypeID(typeRef) == CFDataGetTypeID())
    {
        data = (CFDataRef)typeRef;
        copySize = CFDataGetLength(data);
        *actualSize = copySize;
        if( maxSize < copySize)
            copySize = maxSize;
        bcopy( CFDataGetBytePtr(data), bytes, copySize );
    }
    else if (CFGetTypeID(typeRef) == CFNumberGetTypeID())
    {
        number = (CFNumberRef)typeRef;
        copySize = CFNumberGetByteSize(number);
        *actualSize = copySize;
        if( maxSize < copySize)
            copySize = maxSize;

        switch ( copySize )
        {
            case sizeof(UInt64):
                numberType = kCFNumberSInt64Type;
                break;
            case sizeof(UInt32):
                numberType = kCFNumberSInt32Type;
                break;
            case sizeof(UInt16):
                numberType = kCFNumberSInt16Type;
                break;
            case sizeof(UInt8):
                numberType = kCFNumberSInt8Type;
                break;
            default:
                numberType = kCFNumberSInt32Type;
                break;
        }

        CFNumberGetValue( number, numberType, bytes );
    }
    else
    {
        kr = kIOReturnBadArgument;
    }

    CFRelease(typeRef);
    
    return( kr );

}

kern_return_t IOHIDCopyCFTypeParameter( io_connect_t handle, CFStringRef key, CFTypeRef * parameter )
{
    kern_return_t	kr;
    io_service_t	hidsystem;
    CFDictionaryRef	paramDict;
    CFDictionaryRef	dict;
    CFTypeRef		tempParameter = NULL;

    if (!parameter)
        return kIOReturnError;
    
    kr = IOConnectGetService( handle, &hidsystem );
    if( KERN_SUCCESS != kr )
        return( kr );

    kr = IORegistryEntryCreateCFProperties( hidsystem, &dict,
                                            kCFAllocatorDefault, kNilOptions);
    IOObjectRelease( hidsystem );

    if( kr != KERN_SUCCESS)
        return( kr );

    if( (paramDict = CFDictionaryGetValue( dict, CFSTR(kIOHIDParametersKey))))
        tempParameter = CFDictionaryGetValue( paramDict, key);
    if( !tempParameter )
        tempParameter = CFDictionaryGetValue( dict, key);

    if ( tempParameter )
        CFRetain(tempParameter);
    else
        kr = kIOReturnBadArgument;
    
    *parameter = tempParameter;

    CFRelease(dict);

    return( kr );
}


/*
#define NXEvSetParameterInt( h,n,p,sz )	 IOHIDSetParameter( h,n,	\
					(unsigned char *)p,sz * 4 )
#define NXEvGetParameterInt( h,n,mx,p,sz )	 IOHIDGetParameter( h,n, mx * 4, \
					(unsigned char *)p,sz ); \
			*sz /= 4
#define NXEvSetParameterChar( h,n,p,sz ) IOHIDSetParameter( h,n,p,sz )
*/
#define NXEvGetParameterChar( h,n,mx,p,sz ) IOHIDGetParameter( h, n,mx,p,sz )

static inline int NXEvSetParameterChar( io_connect_t 	handle,
                                 CFStringRef 	key,
                                 const void * 	bytes,
                                 IOByteCount 	size )
{
    kern_return_t		kr = kIOReturnNoMemory;
    CFDataRef			data;

    do {
        data = CFDataCreate( kCFAllocatorDefault, bytes, size );
        if( !data)
            continue;
        kr = IOHIDSetCFTypeParameter( handle, key, data );
        CFRelease(data);

    } while( false );

    return( kr );
}

static inline int NXEvSetParameterInt(	NXEventHandle handle,
                                CFStringRef parameterName,
                                unsigned int *parameterArray,
                                unsigned int count)
{

    CFMutableArrayRef 	arrayRef;
    CFNumberRef 	numberRef;
    IOReturn		ret;
    int 		i;


    if (!parameterArray || !count)
        return kIOReturnError;

    if (count > 1)
    {
        arrayRef = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
        if ( !arrayRef )
            return kIOReturnNoMemory;

        for (i=0; i<count; i++)
        {
            numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &(parameterArray[i]));

            if (numberRef)
            {
                CFArraySetValueAtIndex(arrayRef, i, numberRef);
                CFRelease(numberRef);
            }
        }

        ret = IOHIDSetCFTypeParameter(handle, parameterName, arrayRef);

        CFRelease(arrayRef);
    }
    else
    {
        numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &(parameterArray[0]));

        if (numberRef)
        {
            ret = IOHIDSetCFTypeParameter(handle, parameterName, numberRef);
            CFRelease(numberRef);
        }

    }
    return ret;
}

static inline int NXEvGetParameterInt(	NXEventHandle handle,
                                CFStringRef parameterName,
                                unsigned int maxCount,
                                unsigned int *parameterArray,
                                unsigned int *returnedCount)
{

    CFArrayRef 		arrayRef;
    CFNumberRef 	numberRef;
    CFTypeRef		typeRef;
    IOReturn		ret;
    int 		i;


    ret = IOHIDCopyCFTypeParameter(handle, parameterName, &typeRef);

    if ( ret != kIOReturnSuccess )
        return ret;

    if ( !typeRef || (CFGetTypeID(typeRef) != CFArrayGetTypeID()) || !parameterArray || !maxCount)
        return kIOReturnBadArgument;

    arrayRef = (CFArrayRef)typeRef;

    *returnedCount = CFArrayGetCount(arrayRef);

    if( maxCount < *returnedCount)
        *returnedCount = maxCount;

    for (i=0; i<*returnedCount; i++)
    {
        numberRef = CFArrayGetValueAtIndex(arrayRef, i);

        if (numberRef)
            CFNumberGetValue(numberRef, kCFNumberIntType, &(parameterArray[i]));
    }

    CFRelease(arrayRef);

    return ret;
}


/* Keyboard */

void NXResetKeyboard(NXEventHandle handle)
{
	unsigned int params[EVSIORKBD_SIZE];
    
	NXEvSetParameterInt( handle, CFSTR(EVSIORKBD), params, EVSIORKBD_SIZE);
}

void NXSetKeyRepeatInterval(NXEventHandle handle, double rate)
{
	UInt64 params;
	secs_to_packed_nsecs( rate, &params );
	IOHIDSetParameter(	handle, CFSTR(EVSIOSKR), &params, sizeof(UInt64));
}

double NXKeyRepeatInterval(NXEventHandle handle)
{
	UInt64 params;
	IOByteCount rcnt = EVSIOCKR_SIZE;
	int r;

        r = IOHIDGetParameter( handle, CFSTR(EVSIOSKR), sizeof(params),
				&params, &rcnt );
	if ( r != kIOReturnSuccess )
		return 0.0;

	return packed_nsecs_to_secs( params );
}

void NXSetKeyRepeatThreshold(NXEventHandle handle, double threshold)
{
        UInt64 params;
        secs_to_packed_nsecs( threshold, &params );
        IOHIDSetParameter( handle, CFSTR(EVSIOSIKR), &params, sizeof(params));
}

double NXKeyRepeatThreshold(NXEventHandle handle)
{
	UInt64 params;
	IOByteCount rcnt = EVSIOCKR_SIZE;
	int r;

	r = IOHIDGetParameter( handle, CFSTR(EVSIOSIKR), sizeof(UInt64),
				&params, &rcnt );
	if ( r != kIOReturnSuccess )
		return 0.0;
	return packed_nsecs_to_secs( params );
}

NXKeyMapping * NXSetKeyMapping(NXEventHandle handle, NXKeyMapping *keymap)
{
	int r;

	if ( keymap->size > EVSIOSKM_SIZE )
	    	return (NXKeyMapping *)0;
	r = NXEvSetParameterChar(handle, CFSTR(EVSIOSKM),
				(unsigned char *) keymap->mapping,
				keymap->size);
	if ( r != kIOReturnSuccess )
	    return (NXKeyMapping *)0;
	return keymap;
}

int NXKeyMappingLength(NXEventHandle handle)
{
	int r;
	IOByteCount size;

	r = NXEvGetParameterChar( handle, CFSTR(EVSIOCKM), 0,
				(unsigned char *) 0,
				&size );
	if ( r != kIOReturnSuccess )
		size = 0;
	return size;
}

NXKeyMapping * NXGetKeyMapping(NXEventHandle handle, NXKeyMapping *keymap)
{
	int r;

	r = NXEvGetParameterChar( handle, CFSTR(EVSIOCKM), keymap->size,
				(unsigned char *) keymap->mapping,
				(IOByteCount *) &keymap->size );
	if ( r != kIOReturnSuccess )
    		return (NXKeyMapping *)0;
	return keymap;
}

/* Mouse */

void NXResetMouse(NXEventHandle handle)
{
	unsigned int params[EVSIORMS_SIZE];
    
	NXEvSetParameterInt(handle, CFSTR(EVSIORMS), params, EVSIORMS_SIZE);
}

void NXSetClickTime(NXEventHandle handle, double secs)
{
	UInt64 params;

	secs_to_packed_nsecs( secs, &params );
        IOHIDSetParameter(handle, CFSTR(EVSIOSCT), &params, sizeof(params));
}

double NXClickTime(NXEventHandle handle)
{
	UInt64 params;
	IOByteCount rcnt = EVSIOCCT_SIZE;
	int r;

	r = IOHIDGetParameter(handle, CFSTR(EVSIOCCT), sizeof(UInt64),
				&params, &rcnt );
	if ( r != kIOReturnSuccess )
		return 0.0;
	return packed_nsecs_to_secs( params );
}

void NXSetClickSpace(NXEventHandle handle, _NXSize_ *area)
{
	unsigned int params[EVSIOSCS_SIZE];

	params[EVSIOSCS_X] = (unsigned int)(area->width);
	params[EVSIOSCS_Y] = (unsigned int)(area->height);

	NXEvSetParameterInt(handle, CFSTR(EVSIOSCS), params, EVSIOSCS_SIZE);
}

void NXGetClickSpace(NXEventHandle handle, _NXSize_ *area)
{
	unsigned int params[EVSIOCCS_SIZE];
	IOByteCount rcnt = EVSIOCCS_SIZE;

	NXEvGetParameterInt(handle, CFSTR(EVSIOCCS), EVSIOCCS_SIZE,
				params, &rcnt );
	area->width = params[EVSIOCCS_X];
	area->height = params[EVSIOCCS_Y];
}

void NXSetMouseScaling(NXEventHandle handle, NXMouseScaling *scaling)
{
	double accl;

	accl = scaling->scaleFactors[scaling->numScaleLevels - 1] - 1.0;
	if( accl > 18.0)
	    accl = 1.0;
	else
	    accl /= 18.0;

	IOHIDSetMouseAcceleration( handle, accl );
}

void NXGetMouseScaling(NXEventHandle handle, NXMouseScaling *scaling)
{
	kern_return_t	kr;
	double		acceleration;
	int		factor, i;
	const unsigned char * table;
	const unsigned char * nextTable;
	static const unsigned char nextToAccl[] = {
		1,  1, 1,
		5,  1, 1, 6, 2, 7, 3, 8, 5, 9, 7,
		5,  2, 2, 3, 4, 4, 6, 5, 8, 6, 10,
		5,  2, 2, 3, 6, 4, 10, 5, 15, 6, 22,
		0 };

	kr = IOHIDGetMouseAcceleration( handle, &acceleration );
	if ( kr != kIOReturnSuccess ) {
		scaling->numScaleLevels = 0;
		return;
	}

	factor = (int) (18.0 * acceleration + 1.0);
	table = nextToAccl;
	do {
	    nextTable = table + (table[0] * 2) + 1;
	    if( factor <= nextTable[-1])
		break;
	    if( nextTable[0])
                table = nextTable;
	    else
		break;
	} while( true );

        scaling->numScaleLevels = table[0];
	for( i = 0; i < scaling->numScaleLevels; i++ )
	{
            scaling->scaleThresholds[i] = table[ i * 2 + 1 ];
            scaling->scaleFactors[i] = table[ i * 2 + 2 ];
	}
}

kern_return_t IOHIDGetScrollAcceleration( io_connect_t handle, double * acceleration )
{
    kern_return_t	kr;
    unsigned int	fixed;
    IOByteCount	rsize;

    kr = IOHIDGetParameter( handle, CFSTR(kIOHIDScrollAccelerationKey),
                            sizeof( fixed), (unsigned char *) &fixed, &rsize );

    if( kr == kIOReturnSuccess)
        *acceleration = ((double) fixed) / 65536.0;

    return( kr );
}

kern_return_t IOHIDSetScrollAcceleration( io_connect_t handle, double acceleration )
{
    unsigned int	fixed;

    fixed = (unsigned int) (acceleration * 65536.0);

    return( IOHIDSetParameter(handle, CFSTR(kIOHIDScrollAccelerationKey),
                            (unsigned char *) &fixed, sizeof(fixed)) );
}

kern_return_t IOHIDGetMouseButtonMode( io_connect_t handle, int * mode )
{
    IOByteCount	rsize;

    return( IOHIDGetParameter( handle, CFSTR(kIOHIDPointerButtonMode),
                            sizeof( *mode), (unsigned char *) mode, &rsize ));
}

kern_return_t IOHIDSetMouseButtonMode( io_connect_t handle, int mode )
{
    return( IOHIDSetParameter(handle, CFSTR(kIOHIDPointerButtonMode),
                            (unsigned char *) &mode, sizeof(mode)) );
}

kern_return_t IOHIDGetMouseAcceleration( io_connect_t handle, double * acceleration )
{
	kern_return_t	kr;
	unsigned int	fixed;
	IOByteCount	rsize;

	kr = IOHIDGetParameter( handle, CFSTR(kIOHIDPointerAccelerationKey),
				sizeof( fixed), (unsigned char *) &fixed, &rsize );

	if( kr == kIOReturnSuccess)
            *acceleration = ((double) fixed) / 65536.0;

        return( kr );
}

kern_return_t IOHIDSetMouseAcceleration( io_connect_t handle, double acceleration )
{
	unsigned int	fixed;

	fixed = (unsigned int) (acceleration * 65536.0);

	return( IOHIDSetParameter(handle, CFSTR(kIOHIDPointerAccelerationKey),
				(unsigned char *) &fixed, sizeof( fixed)) );
}

kern_return_t IOHIDGetAccelerationWithKey( io_connect_t handle, CFStringRef key, double * acceleration )
{
	kern_return_t	kr;
	unsigned int	fixed;
	IOByteCount	rsize;

	kr = IOHIDGetParameter( handle, key,
				sizeof( fixed), (unsigned char *) &fixed, &rsize );

	if( kr == kIOReturnSuccess)
            *acceleration = ((double) fixed) / 65536.0;

        return( kr );
}

kern_return_t IOHIDSetAccelerationWithKey( io_connect_t handle, CFStringRef key, double acceleration )
{
	unsigned int	fixed;

	fixed = (unsigned int) (acceleration * 65536.0);

	return( IOHIDSetParameter(handle, key,
				(unsigned char *) &fixed, sizeof( fixed)) );
}


/* Screen Brightness and Auto-dimming */

void NXSetAutoDimThreshold(NXEventHandle handle, double threshold)
{
	UInt64 params;

	secs_to_packed_nsecs( threshold, &params );
	IOHIDSetParameter(handle, CFSTR(EVSIOSADT), &params, sizeof(UInt64));
}

double NXAutoDimThreshold(NXEventHandle handle)
{
	UInt64 params;
        IOByteCount rcnt = sizeof(UInt64);
	int r;

	r = IOHIDGetParameter(handle, CFSTR(EVSIOCADT), sizeof(UInt64),
				&params, &rcnt );
	if ( r != kIOReturnSuccess )
		return 0.0;
	return packed_nsecs_to_secs( params );
}

double NXAutoDimTime(NXEventHandle handle)
{
        UInt64 params;
        IOByteCount rcnt = sizeof(UInt64);
	int r;

        r = IOHIDGetParameter( handle, CFSTR(EVSIOGDADT), sizeof(UInt64),
				&params, &rcnt );
	if ( r != kIOReturnSuccess )
		return 0.0;
	return packed_nsecs_to_secs( params );
}

double NXIdleTime(NXEventHandle handle)
{
        UInt64 params;
        IOByteCount rcnt = sizeof(UInt64);
        int r;

        r = IOHIDGetParameter( handle, CFSTR(EVSIOIDLE), sizeof(UInt64),
                                &params, &rcnt );
        if ( r != kIOReturnSuccess )
                return 0.0;
        return packed_nsecs_to_secs( params );

}

void NXSetAutoDimState(NXEventHandle handle, boolean_t dimmed)
{
	UInt32 params = dimmed;

	IOHIDSetParameter(handle, CFSTR(EVSIOSADS), &params, sizeof(UInt32));
}

boolean_t NXAutoDimState(NXEventHandle handle)
{
        UInt32 params;
	IOByteCount rcnt = sizeof(UInt32);
	int r;

        r = IOHIDGetParameter( handle, CFSTR(EVSIOCADS), sizeof(UInt32),
				&params, &rcnt );
	if ( r != kIOReturnSuccess )
		return false;
	return (params != 0);
}

void NXSetAutoDimBrightness(NXEventHandle handle, double brightness)
{
	UInt32 params;

	params = (int) (brightness * BRIGHT_MAX);
	IOHIDSetParameter(handle, CFSTR(EVSIOSADB), &params, sizeof(UInt32));
}

double NXAutoDimBrightness(NXEventHandle handle)
{
	UInt32 params;
        IOByteCount rcnt = sizeof(UInt32);
	int r;

        r = IOHIDGetParameter( handle, CFSTR(EVSIOSADB), sizeof(UInt32),
				&params, &rcnt );
	if ( r != kIOReturnSuccess )
		return 1.0;
	return ((double)params
		/ (double)BRIGHT_MAX);
}

void NXSetScreenBrightness(NXEventHandle handle, double brightness)
{
	UInt32 params;
	params = (int) (brightness * BRIGHT_MAX);
        IOHIDSetParameter(handle, CFSTR(EVSIOSB), &params, sizeof(UInt32));
}

double NXScreenBrightness(NXEventHandle handle)
{
	UInt32 params;
        IOByteCount rcnt = sizeof(UInt32);
	int r;

        r = IOHIDGetParameter( handle, CFSTR(EVSIOSB), sizeof(UInt32),
				&params, &rcnt );
	if ( r != kIOReturnSuccess )
		return 1.0;
	return ((double)params / (double)BRIGHT_MAX);
}

/* Generic entry points */

#ifdef _undef

int NXEvSetParameterInt(NXEventHandle handle,
			char *parameterName,
			unsigned int *parameterArray,
			unsigned int count)
{
    return( kIOReturnUnsupported );
}

int NXEvSetParameterChar(NXEventHandle handle,
			char *parameterName,
			unsigned char *parameterArray,
			unsigned int count)
{
    return( kIOReturnUnsupported );
}

int NXEvGetParameterInt(NXEventHandle handle,
			char *parameterName,
			unsigned int maxCount,
			unsigned int *parameterArray,
			unsigned int *returnedCount)
{
    return( kIOReturnUnsupported );
}

int NXEvGetParameterChar(NXEventHandle handle,
			char *parameterName,
			unsigned int maxCount,
			unsigned char *parameterArray,
			unsigned int *returnedCount)
{
    return( kIOReturnUnsupported );
}

#endif


