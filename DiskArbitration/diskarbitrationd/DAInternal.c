/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include "DAInternal.h"

#include <grp.h>
#include <pwd.h>
#include <mach/mach.h>
#include <DiskArbitration/DiskArbitration.h>

__private_extern__ const char * _kDAAuthorizeRightAdopt   = "system.volume.adopt";
__private_extern__ const char * _kDAAuthorizeRightEncode  = "system.volume.encode";
__private_extern__ const char * _kDAAuthorizeRightMount   = "system.volume.mount";
__private_extern__ const char * _kDAAuthorizeRightRename  = "system.volume.rename";
__private_extern__ const char * _kDAAuthorizeRightUnmount = "system.volume.unmount";

__private_extern__ const CFStringRef _kDACallbackAddressKey       = CFSTR( "DACallbackAddress"   );
__private_extern__ const CFStringRef _kDACallbackArgument0Key     = CFSTR( "DACallbackArgument0" );
__private_extern__ const CFStringRef _kDACallbackArgument1Key     = CFSTR( "DACallbackArgument1" );
__private_extern__ const CFStringRef _kDACallbackContextKey       = CFSTR( "DACallbackContext"   );
__private_extern__ const CFStringRef _kDACallbackDiskKey          = CFSTR( "DACallbackDisk"      );
__private_extern__ const CFStringRef _kDACallbackKindKey          = CFSTR( "DACallbackKind"      );
__private_extern__ const CFStringRef _kDACallbackMatchKey         = CFSTR( "DACallbackMatch"     );
__private_extern__ const CFStringRef _kDACallbackOrderKey         = CFSTR( "DACallbackOrder"     );
__private_extern__ const CFStringRef _kDACallbackSessionKey       = CFSTR( "DACallbackSession"   );
__private_extern__ const CFStringRef _kDACallbackTimeKey          = CFSTR( "DACallbackTime"      );
__private_extern__ const CFStringRef _kDACallbackWatchKey         = CFSTR( "DACallbackWatch"     );

__private_extern__ const CFStringRef _kDADiskIDKey                = CFSTR( "DADiskID"            );

__private_extern__ const CFStringRef _kDADissenterStatusKey       = CFSTR( "DAStatus"            );
__private_extern__ const CFStringRef _kDADissenterStatusStringKey = CFSTR( "DAStatusString"      );

__private_extern__ const CFStringRef _kDARequestArgument1Key      = CFSTR( "DARequestArgument1"  );
__private_extern__ const CFStringRef _kDARequestArgument2Key      = CFSTR( "DARequestArgument2"  );
__private_extern__ const CFStringRef _kDARequestArgument3Key      = CFSTR( "DARequestArgument3"  );
__private_extern__ const CFStringRef _kDARequestCallbackKey       = CFSTR( "DARequestCallback"   );
__private_extern__ const CFStringRef _kDARequestDiskKey           = CFSTR( "DARequestDisk"       );
__private_extern__ const CFStringRef _kDARequestDissenterKey      = CFSTR( "DARequestDissenter"  );
__private_extern__ const CFStringRef _kDARequestKindKey           = CFSTR( "DARequestKind"       );
__private_extern__ const CFStringRef _kDARequestLinkKey           = CFSTR( "DARequestLink"       );
__private_extern__ const CFStringRef _kDARequestStateKey          = CFSTR( "DARequestState"      );
__private_extern__ const CFStringRef _kDARequestUserGIDKey        = CFSTR( "DARequestUserEGID"   );
__private_extern__ const CFStringRef _kDARequestUserUIDKey        = CFSTR( "DARequestUserEUID"   );

const CFStringRef kDADiskDescriptionVolumeKindKey      = CFSTR( "DAVolumeKind"      );
const CFStringRef kDADiskDescriptionVolumeMountableKey = CFSTR( "DAVolumeMountable" );
const CFStringRef kDADiskDescriptionVolumeNameKey      = CFSTR( "DAVolumeName"      );
const CFStringRef kDADiskDescriptionVolumeNetworkKey   = CFSTR( "DAVolumeNetwork"   );
const CFStringRef kDADiskDescriptionVolumePathKey      = CFSTR( "DAVolumePath"      );
const CFStringRef kDADiskDescriptionVolumeUUIDKey      = CFSTR( "DAVolumeUUID"      );

const CFStringRef kDADiskDescriptionMediaBlockSizeKey  = CFSTR( "DAMediaBlockSize"  );
const CFStringRef kDADiskDescriptionMediaBSDMajorKey   = CFSTR( "DAMediaBSDMajor"   );
const CFStringRef kDADiskDescriptionMediaBSDMinorKey   = CFSTR( "DAMediaBSDMinor"   );
const CFStringRef kDADiskDescriptionMediaBSDNameKey    = CFSTR( "DAMediaBSDName"    );
const CFStringRef kDADiskDescriptionMediaBSDUnitKey    = CFSTR( "DAMediaBSDUnit"    );
const CFStringRef kDADiskDescriptionMediaContentKey    = CFSTR( "DAMediaContent"    );
const CFStringRef kDADiskDescriptionMediaEjectableKey  = CFSTR( "DAMediaEjectable"  );
const CFStringRef kDADiskDescriptionMediaIconKey       = CFSTR( "DAMediaIcon"       );
const CFStringRef kDADiskDescriptionMediaKindKey       = CFSTR( "DAMediaKind"       );
const CFStringRef kDADiskDescriptionMediaLeafKey       = CFSTR( "DAMediaLeaf"       );
const CFStringRef kDADiskDescriptionMediaNameKey       = CFSTR( "DAMediaName"       );
const CFStringRef kDADiskDescriptionMediaPathKey       = CFSTR( "DAMediaPath"       );
const CFStringRef kDADiskDescriptionMediaRemovableKey  = CFSTR( "DAMediaRemovable"  );
const CFStringRef kDADiskDescriptionMediaSizeKey       = CFSTR( "DAMediaSize"       );
const CFStringRef kDADiskDescriptionMediaTypeKey       = CFSTR( "DAMediaType"       );
const CFStringRef kDADiskDescriptionMediaWholeKey      = CFSTR( "DAMediaWhole"      );
const CFStringRef kDADiskDescriptionMediaWritableKey   = CFSTR( "DAMediaWritable"   );

const CFStringRef kDADiskDescriptionDeviceGUIDKey      = CFSTR( "DADeviceGUID"      );
const CFStringRef kDADiskDescriptionDeviceInternalKey  = CFSTR( "DADeviceInternal"  );
const CFStringRef kDADiskDescriptionDeviceModelKey     = CFSTR( "DADeviceModel"     );
const CFStringRef kDADiskDescriptionDevicePathKey      = CFSTR( "DADevicePath"      );
const CFStringRef kDADiskDescriptionDeviceProtocolKey  = CFSTR( "DADeviceProtocol"  );
const CFStringRef kDADiskDescriptionDeviceRevisionKey  = CFSTR( "DADeviceRevision"  );
const CFStringRef kDADiskDescriptionDeviceUnitKey      = CFSTR( "DADeviceUnit"      );
const CFStringRef kDADiskDescriptionDeviceVendorKey    = CFSTR( "DADeviceVendor"    );

const CFStringRef kDADiskDescriptionBusNameKey         = CFSTR( "DABusName"         );
const CFStringRef kDADiskDescriptionBusPathKey         = CFSTR( "DABusPath"         );

const CFStringRef kDADiskDescriptionAppearanceTimeKey  = CFSTR( "DAAppearanceTime"  );

static const char * __kDAKindNameList[] =
{
    "disk appeared",
    "disk claim",
    "disk claim release",
    "disk classic",
    "disk description changed",
    "disk disappeared",
    "disk eject",
    "disk eject approval",
    "disk mount",
    "disk mount approval",
    "disk peek",
    "disk refresh",
    "disk rename",
    "disk unmount",
    "disk unmount approval",
    "idle"
};

extern CFIndex __CFBinaryPlistWriteToStream( CFPropertyListRef plist, CFTypeRef stream );

__private_extern__ int ___isadmin( uid_t uid )
{
    struct group * group;

    group = getgrgid( ___GID_ADMIN );

    if ( group )
    {
        char * user;

        user = user_from_uid( uid, 1 );

        if ( user )
        {
            char ** users;

            for ( users = group->gr_mem; *users; users++ )
            {
                if ( strcmp( *users, user ) == 0 )
                {
                    return 1;
                }
            }
        }
    }

    return 0;
}

__private_extern__ int ___statfs( const char * path, struct statfs * buf, int flags )
{
    struct statfs * mountList;
    int             mountListCount;
    int             mountListIndex;

    mountListCount = getmntinfo( &mountList, flags );

    for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
    {
        if ( strcmp( mountList[mountListIndex].f_mntonname, path ) == 0 )
        {
            *buf = mountList[mountListIndex];

            return 0;
        }
    }

    return -1;
}

__private_extern__ Boolean ___CFArrayContainsValue( CFArrayRef array, const void * value )
{
    return CFArrayContainsValue( array, CFRangeMake( 0, CFArrayGetCount( array ) ), value );
}

__private_extern__ void ___CFArrayRemoveValue( CFMutableArrayRef array, const void * value )
{
    CFIndex index;

    index = CFArrayGetFirstIndexOfValue( array, CFRangeMake( 0, CFArrayGetCount( array ) ), value );

    if ( index != kCFNotFound )
    {
        CFArrayRemoveValueAtIndex( array, index );
    }
}

__private_extern__ vm_address_t ___CFDataCopyBytes( CFDataRef data, vm_size_t * length )
{
    vm_address_t bytes = NULL;

    *length = CFDataGetLength( data );

    vm_allocate( mach_task_self( ), &bytes, *length, TRUE );

    if ( bytes )
    {
        bcopy( CFDataGetBytePtr( data ), ( void * ) bytes, *length );
    }

    return bytes;
}

__private_extern__ SInt64 ___CFDictionaryGetIntegerValue( CFDictionaryRef dictionary, const void * key )
{
    CFNumberRef number;
    SInt64      value  = 0;

    number = CFDictionaryGetValue( dictionary, key );

    if ( number )
    {
        CFNumberGetValue( number, kCFNumberSInt64Type, &value );
    }

    return value;
}

__private_extern__ void ___CFDictionarySetIntegerValue( CFMutableDictionaryRef dictionary, const void * key, SInt64 value )
{
    CFNumberRef number;

    number = CFNumberCreate( CFGetAllocator( dictionary ), kCFNumberSInt64Type, &value );

    if ( number )
    {
        CFDictionarySetValue( dictionary, key, number );

        CFRelease( number );
    }
}

__private_extern__ CFNumberRef ___CFNumberCreateWithIntegerValue( CFAllocatorRef allocator, SInt64 value )
{
    CFNumberRef number;

    number = CFNumberCreate( allocator, kCFNumberSInt64Type, &value );

    return number;
}

__private_extern__ SInt64 ___CFNumberGetIntegerValue( CFNumberRef number )
{
    SInt64 value = 0;

    CFNumberGetValue( number, kCFNumberSInt64Type, &value );

    return value;
}

__private_extern__ char * ___CFStringCopyCString( CFStringRef string )
{
    /*
     * Creates a C string buffer from a CFString object.  The string encoding is presumed to be
     * UTF-8.  The result is a reference to a C string buffer or NULL if there was a problem in
     * creating the buffer.  The caller is responsible for releasing the buffer with free().
     */

    char * buffer = NULL;

    if ( string )
    {
        CFIndex length;
        CFRange range;

        range = CFRangeMake( 0, CFStringGetLength( string ) );

        if ( CFStringGetBytes( string, range, kCFStringEncodingUTF8, 0, FALSE, NULL, 0, &length ) )
        {
            buffer = malloc( length + 1 );

            if ( buffer )
            {
                CFStringGetBytes( string, range, kCFStringEncodingUTF8, 0, FALSE, buffer, length, NULL );

                buffer[length] = 0;
            }
        }
    }

    return buffer;
}

__private_extern__ char * ___CFURLCopyFileSystemRepresentation( CFURLRef url )
{
    /*
     * Creates a buffer with the file system's native string representation of URL's path.
     * The result is a reference to a C buffer or NULL if there was a problem in creating
     * the buffer.  The caller is responsible for releasing the buffer with free().
     */

    char * path = NULL;

    if ( url )
    {
        CFStringRef string;

        string = CFURLCopyFileSystemPath( url, kCFURLPOSIXPathStyle );

        if ( string )
        {
            path = ___CFStringCopyCString( string );

            CFRelease( string );
        }
    }

    return path;
}

__private_extern__ CFDataRef _DASerialize( CFAllocatorRef allocator, CFTypeRef object )
{
    CFDataRef data;

    data = CFDataCreateMutable( allocator, 0 );

    if ( data )
    {
        if ( __CFBinaryPlistWriteToStream( object, data ) == 0 )
        {
            CFRelease( data );

            data = NULL;
        }
    }

    return data;
}

__private_extern__ const char * _DACallbackKindGetName( _DACallbackKind kind )
{
    return __kDAKindNameList[kind];
}

__private_extern__ const char * _DARequestKindGetName( _DARequestKind kind )
{
    return __kDAKindNameList[kind];
}

__private_extern__ CFDataRef _DASerializeDiskDescription( CFAllocatorRef allocator, CFDictionaryRef description )
{
    CFDataRef data = NULL;
    CFURLRef  path;
    CFUUIDRef uuid;

    path = CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey );
    uuid = CFDictionaryGetValue( description, kDADiskDescriptionVolumeUUIDKey );
    
    if ( path || uuid )
    {
        CFMutableDictionaryRef copy;

        copy = CFDictionaryCreateMutableCopy( allocator, 0, description );

        if ( copy )
        {
            if ( path )
            {
                CFStringRef string;

                string = CFURLCopyFileSystemPath( path, kCFURLPOSIXPathStyle );

                if ( string )
                {
                    CFDictionarySetValue( copy, kDADiskDescriptionVolumePathKey, string );

                    CFRelease( string );
                }
            }

            if ( uuid )
            {
                CFStringRef string;

                string = CFUUIDCreateString( allocator, uuid );

                if ( string )
                {
                    CFDictionarySetValue( copy, kDADiskDescriptionVolumeUUIDKey, string );

                    CFRelease( string );
                }
            }

            data = _DASerialize( allocator, copy );

            CFRelease( copy );
        }
    }
    else
    {
        data = _DASerialize( allocator, description );
    }

    return data;
}

__private_extern__ CFTypeRef _DAUnserialize( CFAllocatorRef allocator, CFDataRef data )
{
    return CFPropertyListCreateFromXMLData( allocator, data, kCFPropertyListImmutable, NULL );
}

__private_extern__ CFMutableDictionaryRef _DAUnserializeDiskDescription( CFAllocatorRef allocator, CFDataRef data )
{
    CFMutableDictionaryRef description;

    description = ( void * ) CFPropertyListCreateFromXMLData( allocator, data, kCFPropertyListMutableContainers, NULL );

    if ( description )
    {
        CFStringRef string;

        string = CFDictionaryGetValue( description, kDADiskDescriptionVolumePathKey );

        if ( string )
        {
            CFURLRef path;

            path = CFURLCreateWithFileSystemPath( allocator, string, kCFURLPOSIXPathStyle, TRUE );

            if ( path )
            {
                CFDictionarySetValue( description, kDADiskDescriptionVolumePathKey, path );

                CFRelease( path );
            }
        }

        string = CFDictionaryGetValue( description, kDADiskDescriptionVolumeUUIDKey );

        if ( string )
        {
            CFUUIDRef uuid;

            uuid = CFUUIDCreateFromString( allocator, string );

            if ( uuid )
            {
                CFDictionarySetValue( description, kDADiskDescriptionVolumeUUIDKey, uuid );

                CFRelease( uuid );
            }
        }
    }

    return description;
}

__private_extern__ CFTypeRef _DAUnserializeWithBytes( CFAllocatorRef allocator, vm_address_t bytes, vm_size_t length )
{
    CFTypeRef object = NULL;

    if ( bytes )
    {
        CFDataRef data;

        data = CFDataCreateWithBytesNoCopy( allocator, ( void * ) bytes, length, kCFAllocatorNull );

        if ( data )
        {
            object = _DAUnserialize( allocator, data );

            CFRelease( data );
        }
    }

    return object;
}

__private_extern__ CFMutableDictionaryRef _DAUnserializeDiskDescriptionWithBytes( CFAllocatorRef allocator, vm_address_t bytes, vm_size_t length )
{
    CFMutableDictionaryRef description = NULL;

    if ( bytes )
    {
        CFDataRef data;

        data = CFDataCreateWithBytesNoCopy( allocator, ( void * ) bytes, length, kCFAllocatorNull );

        if ( data )
        {
            description = _DAUnserializeDiskDescription( allocator, data );

            CFRelease( data );
        }
    }

    return description;
}
