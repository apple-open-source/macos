/*
 * Copyright (c) 1998-2013 Apple Inc. All rights reserved.
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

#include "DABase.h"

#include "DAInternal.h"

#include <fcntl.h>
#include <paths.h>
#include <sysexits.h>
#include <vproc.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>

static vproc_transaction_t __vproc_transaction       = NULL;
static size_t              __vproc_transaction_count = 0;

__private_extern__ int ___chattr( const char * path, ___attr_t attr, ___attr_t noattr )
{
    /*
     * Change file flags.
     */

    struct __chattrbuf
    {
        uint32_t  size;
        uint8_t   reserved0032[8];
        ___attr_t attr;
        uint8_t   reserved0112[22];
    };

    typedef struct __chattrbuf __chattrbuf;

    struct attrlist attributes;
    __chattrbuf     buffer;
    int             status;

    attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
    attributes.commonattr  = ATTR_CMN_FNDRINFO;
    attributes.dirattr     = 0;
    attributes.fileattr    = 0;
    attributes.forkattr    = 0;
    attributes.volattr     = 0;

    status = getattrlist( path, &attributes, &buffer, sizeof( buffer ), 0 );

    if ( status == 0 )
    {
        buffer.attr = OSSwapHostToBigInt16( ( OSSwapBigToHostInt16( buffer.attr ) & ~noattr ) | attr );

        status = setattrlist( path, &attributes, ( ( uint8_t * ) &buffer ) + sizeof( buffer.size ), sizeof( buffer ) - sizeof( buffer.size ), 0 );
    }

    return status;
}

__private_extern__ int ___isautofs( const char * path )
{
    /*
     * Test for the autofs file system.
     */

    struct statfs * mountList;
    int             mountListCount;
    int             mountListIndex;

    mountListCount = getfsstat( NULL, 0, MNT_NOWAIT );

    if ( mountListCount > 0 )
    {
        mountList = malloc( mountListCount * sizeof( struct statfs ) );

        if ( mountList )
        {
            mountListCount = getfsstat( mountList, mountListCount * sizeof( struct statfs ), MNT_NOWAIT );

            if ( mountListCount > 0 )
            {
                for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
                {
                    if ( strcmp( mountList[mountListIndex].f_fstypename, "autofs" ) == 0 )
                    {
                        if ( strncmp( mountList[mountListIndex].f_mntonname, path, strlen( mountList[mountListIndex].f_mntonname ) ) == 0 )
                        {
                            break;
                        }
                    }
                }
            }

            free( mountList );
        }
    }

    return ( mountListIndex < mountListCount );
}

__private_extern__ int ___mkdir( const char * path, mode_t mode )
{
    /*
     * Make a directory.
     */

    int status;

    status = -1;

    if ( path )
    {
        char * template;

        asprintf( &template, "%s.XXXXXX", path );

        if ( template )
        {
            status = mkdtemp( template ) ? 0 : -1;

            if ( status == 0 )
            {
                status = chmod( template, mode );

                if ( status == 0 )
                {
                    status = rename( template, path );
                }

                if ( status )
                {
                    rmdir( template );
                }
            }

            free( template );
        }
    }

    return status;
}

__private_extern__ void ___vproc_transaction_begin( void )
{
    if ( __vproc_transaction_count == 0 )
    {
        __vproc_transaction = vproc_transaction_begin( NULL );
    }

    __vproc_transaction_count++;
}

__private_extern__ void ___vproc_transaction_end( void )
{
    __vproc_transaction_count--;

    if ( __vproc_transaction_count == 0 )
    {
        vproc_transaction_end( NULL, __vproc_transaction );

        __vproc_transaction = NULL;
    }
}

__private_extern__ const void * ___CFArrayGetValue( CFArrayRef array, const void * value )
{
    /*
     * Retrieves a value in the array which hashes the same as the specified value.
     */

    CFIndex index;

    index = CFArrayGetFirstIndexOfValue( array, CFRangeMake( 0, CFArrayGetCount( array ) ), value );

    return ( index == kCFNotFound ) ? NULL : CFArrayGetValueAtIndex( array, index );
}

__private_extern__ void ___CFArrayIntersect( CFMutableArrayRef array1, CFArrayRef array2 )
{
    /*
     * Forms the intersection with the given array.
     */

    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( array1 );

    for ( index = count - 1; index > -1; index-- )
    {
        const void * value;

        value = CFArrayGetValueAtIndex( array1, index );

        if ( ___CFArrayContainsValue( array2, value ) == FALSE )
        {
            CFArrayRemoveValueAtIndex( array1, index );
        }
    }
}

__private_extern__ CFStringRef ___CFBundleCopyLocalizedStringInDirectory( CFURLRef bundleURL, CFStringRef key, CFStringRef value, CFStringRef table )
{
    /*
     * Returns a localized string from a bundle's strings file without needing to create a
     * CFBundle object.
     */

    CFBundleRef bundle;
    CFStringRef string = NULL;

    bundle = CFBundleCreate( kCFAllocatorDefault, bundleURL );

    if ( bundle )
    {
        _CFBundleSetStringsFilesShared( bundle, FALSE );

        string = CFBundleCopyLocalizedString( bundle, key, value, table );

        CFRelease( bundle );
    }

    return string;
}

__private_extern__ CFURLRef ___CFBundleCopyResourceURLInDirectory( CFURLRef bundleURL, CFStringRef resourcePath )
{
    /*
     * Obtains the location of a resource contained in the specified bundle directory without
     * requiring the creation of a bundle instance.  This variant will accept one argument to
     * describe the subDirName, resourceName, and resourceType.  For instance, a resourcePath
     * of "../hfs.tiff" would break down correctly.
     */

    CFURLRef resourceURL     = NULL;
    CFURLRef resourcePathURL = NULL;

    resourcePathURL = CFURLCreateWithFileSystemPathRelativeToBase( kCFAllocatorDefault, resourcePath, kCFURLPOSIXPathStyle, FALSE, NULL );

    if ( resourcePathURL )
    {
        CFStringRef resourceName;

        resourceName = CFURLCopyLastPathComponent( resourcePathURL );

        if ( resourceName )
        {
            CFURLRef resourceSubDirNameURL;

            resourceSubDirNameURL = CFURLCreateCopyDeletingLastPathComponent( kCFAllocatorDefault, resourcePathURL );

            if ( resourceSubDirNameURL )
            {
                CFStringRef resourceSubDirName;

                resourceSubDirName = CFURLCopyFileSystemPath( resourceSubDirNameURL, kCFURLPOSIXPathStyle );

                if ( resourceSubDirName )
                {
                    resourceURL = CFBundleCopyResourceURLInDirectory( bundleURL, resourceName, NULL, resourceSubDirName );

                    CFRelease( resourceSubDirName );
                }

                CFRelease( resourceSubDirNameURL );
            }

            CFRelease( resourceName );
        }

        CFRelease( resourcePathURL );
    }

    return resourceURL;
}

__private_extern__ CFDataRef ___CFDataCreateFromString( CFAllocatorRef allocator, CFStringRef string )
{
    /*
     * Creates a CFData object using the specified string.  The string is validated to ensure it
     * is in the appropriate form, that is, that it contain a sequence of hexadecimal characters.
     */

    CFMutableDataRef data;

    data = CFDataCreateMutable( allocator, 0 );

    if ( data )
    {
        CFIndex index;
        CFIndex length;

        length = CFStringGetLength( string );

        for ( index = 0; index + 1 < length; index += 2 )
        {
            UInt8   byte;
            UniChar character1;
            UniChar character2;

            character1 = CFStringGetCharacterAtIndex( string, index     );
            character2 = CFStringGetCharacterAtIndex( string, index + 1 );

            if ( isxdigit( character1 ) == 0 )  break;
            if ( isxdigit( character2 ) == 0 )  break;

            character1 = tolower( character1 ) - '0';
            character2 = tolower( character2 ) - '0';

            byte = ( ( ( character1 > 9 ) ? ( character1 + '0' - 'a' + 10 ) : character1 ) << 4 ) |
                   ( ( ( character2 > 9 ) ? ( character2 + '0' - 'a' + 10 ) : character2 )      );

            CFDataAppendBytes( data, &byte, 1 );
        }

        for ( ; index < length; index++ )
        {
            UniChar character;

            character = CFStringGetCharacterAtIndex( string, index );

            if ( isspace( character ) == 0 )  break;
        }

        if ( index < length )
        {
            CFDataSetLength( data, 0 );
        }

        if ( CFDataGetLength( data ) == 0 )
        {
            CFRelease( data );
            data = NULL;
        }
    }

    return data;
}

__private_extern__ CFDictionaryRef ___CFDictionaryCreateFromXMLString( CFAllocatorRef allocator, CFStringRef string )
{
    /*
     * Creates a CFDictionary object using the specified XML string.
     */

    CFDataRef       data;
    CFDictionaryRef dictionary = NULL;

    data = CFStringCreateExternalRepresentation( kCFAllocatorDefault, string, kCFStringEncodingUTF8, 0 );

    if ( data )
    {
        CFTypeRef object;

        object = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, data, kCFPropertyListImmutable, NULL );

        if ( object )
        {
            if ( CFGetTypeID( object ) == CFDictionaryGetTypeID( ) )
            {
                dictionary = CFRetain( object );
            }

            CFRelease( object );
        }

        CFRelease( data );
    }

    return dictionary;
}

__private_extern__ const void * ___CFDictionaryGetAnyValue( CFDictionaryRef dictionary )
{
    /*
     * Retrieves the value associated with the first key from CFDictionaryGetKeysAndValues().
     */

    CFIndex      count;
    const void * value = NULL;

    count = CFDictionaryGetCount( dictionary );

    if ( count )
    {
        CFTypeRef * values;

        values = malloc( count * sizeof( CFDictionaryRef ) );

        if ( values )
        {
            CFDictionaryGetKeysAndValues( dictionary, NULL, values );

            value = values[0];

            free( values );
        }
    }

    return value;
}

__private_extern__ char * ___CFStringCreateCStringWithFormatAndArguments( const char * format, va_list arguments )
{
    /*
     * Creates a C string buffer from a printf-style list.   The string encoding is presumed to
     * be UTF-8.  The result is a reference to a C string buffer or NULL if there was a problem
     * in creating the buffer.  The caller is responsible for releasing the buffer with free().
     */

    char * buffer = NULL;

    if ( format )
    {
        CFStringRef formatAsString;

        formatAsString = CFStringCreateWithCString( kCFAllocatorDefault, format, kCFStringEncodingUTF8 );

        if ( formatAsString )
        {
            CFStringRef bufferAsString;

            bufferAsString = CFStringCreateWithFormatAndArguments( kCFAllocatorDefault, NULL, formatAsString, arguments );

            if ( bufferAsString )
            {
                buffer = ___CFStringCopyCString( bufferAsString );

                CFRelease( bufferAsString );
            }

            CFRelease( formatAsString );
        }
    }

    return buffer;
}

__private_extern__ Boolean ___CFStringGetCString( CFStringRef string, char * buffer, CFIndex length )
{
    /*
     * Copies the character contents of a CFString object to a local C string buffer after
     * converting the characters to UTF-8.  It will copy as many characters as will fit in
     * the provided buffer.
     */

    length--;

    CFStringGetBytes( string, CFRangeMake( 0, CFStringGetLength( string ) ), kCFStringEncodingUTF8, 0, FALSE, ( void * ) buffer, length, &length );

    buffer[length] = 0;

    return length ? TRUE : FALSE;
}

__private_extern__ void ___CFStringInsertFormat( CFMutableStringRef string, CFIndex index, CFStringRef format, ... )
{
    /*
     * Inserts a formatted string at a specified location in the character buffer of a mutable
     * CFString object.
     */

    va_list arguments;

    va_start( arguments, format );

    ___CFStringInsertFormatAndArguments( string, index, format, arguments );

    va_end( arguments );
}

__private_extern__ void ___CFStringInsertFormatAndArguments( CFMutableStringRef string, CFIndex index, CFStringRef format, va_list arguments )
{
    /*
     * Inserts a formatted string at a specified location in the character buffer of a mutable
     * CFString object.
     */

    CFStringRef insert;

    insert = CFStringCreateWithFormatAndArguments( kCFAllocatorDefault, NULL, format, arguments );

    if ( insert )
    {
        CFStringInsert( string, index, insert );

        CFRelease( insert );
    }
}

__private_extern__ void ___CFStringPad( CFMutableStringRef string, CFStringRef pad, CFIndex length, CFIndex index )
{
    /*
     * Enlarges the string represented by a CFString object, padding it with specified characters.
     */

    if ( length > CFStringGetLength( string ) )
    {
        CFStringPad( string, pad, length, index );
    }
}

__private_extern__ CFUUIDRef ___CFUUIDCreateFromName( CFAllocatorRef allocator, CFUUIDRef space, CFDataRef name )
{
    /*
     * Creates a UUID from a unique "name" in the given "name space".  See version 3 UUID.
     */

    CC_MD5_CTX  md5c;
    CFUUIDBytes uuid;

    assert( sizeof( uuid ) == CC_MD5_DIGEST_LENGTH );

    uuid = CFUUIDGetUUIDBytes( space );

    CC_MD5_Init( &md5c );
    CC_MD5_Update( &md5c, &uuid, sizeof( uuid ) );
    CC_MD5_Update( &md5c, CFDataGetBytePtr( name ), CFDataGetLength( name ) );
    CC_MD5_Final( ( void * ) &uuid, &md5c );

    uuid.byte6 = 0x30 | ( uuid.byte6 & 0x0F );
    uuid.byte8 = 0x80 | ( uuid.byte8 & 0x3F );

    return CFUUIDCreateFromUUIDBytes( allocator, uuid );
}

__private_extern__ CFUUIDRef ___CFUUIDCreateFromString( CFAllocatorRef allocator, CFStringRef string )
{
    /*
     * Creates a CFUUID object using the specified string.  The string is validated to ensure it
     * is in the appropriate form.  One would expect CFUUIDCreateFromString() to accomplish this,
     * but it does not at this time.
     */

    UInt32 index;
    UInt32 length;

    length = CFStringGetLength( string );

    for ( index = 0; index < length; index++ )
    {
        UniChar character;

        character = CFStringGetCharacterAtIndex( string, index );

        if ( index < 36 )
        {
            if ( index == 8 || index == 13 || index == 18 || index == 23 )
            {
                if ( character != '-' )  break;
            }
            else
            {
                if ( isxdigit( character ) == 0 )  break;
            }
        }
        else
        {
            if ( isspace( character ) == 0 )  break;
        }
    }

    return ( index < length ) ? NULL : CFUUIDCreateFromString( allocator, string );
}

__private_extern__ CFStringRef ___CFURLCopyRawDeviceFileSystemPath( CFURLRef url, CFURLPathStyle pathStyle )
{
    /*
     * Obtains the path portion of the specified URL, with the last path component prepended
     * with an "r" to indicate the "raw" or "character" device variant of the specified URL.
     */

    CFStringRef path;

    path = CFURLCopyFileSystemPath( url, pathStyle );

    if ( path )
    {
        CFStringRef node;

        node = CFURLCopyLastPathComponent( url );

        if ( node )
        {
            CFMutableStringRef string;

            string = CFStringCreateMutableCopy( CFGetAllocator( url ), 0, path );

            if ( string )
            {
                CFIndex index;

                index = CFStringGetLength( path ) - CFStringGetLength( node );

                CFStringInsert( string, index, CFSTR( "r" ) );

                CFRelease( path );

                path = string;
            }

            CFRelease( node );
        }
    }

    return path;
}

__private_extern__ kern_return_t ___IORegistryEntryGetPath( io_registry_entry_t entry, const io_name_t plane, ___io_path_t path )
{
    /*
     * Create a path for a registry entry.
     */

    IOReturn status;

    status = IORegistryEntryGetPath( entry, plane, path );

    if ( status == kIOReturnBadArgument )
    {
        io_registry_entry_t parent;

        status = IORegistryEntryGetParentEntry( entry, plane, &parent );

        if ( status == kIOReturnSuccess )
        {
            status = ___IORegistryEntryGetPath( parent, plane, path );

            if ( status == kIOReturnSuccess )
            {
                io_name_t name;

                status = IORegistryEntryGetNameInPlane( entry, plane, name );

                if ( status == kIOReturnSuccess )
                {
                    io_name_t location;

                    status = IORegistryEntryGetLocationInPlane( entry, plane, location );

                    if ( status == kIOReturnSuccess )
                    {
                        if ( strlen( path ) + strlen( "/" ) + strlen( name ) + strlen( "@" ) + strlen( location ) < sizeof( ___io_path_t ) )
                        {
                            strlcat( path, "/",      sizeof( ___io_path_t ) );
                            strlcat( path, name,     sizeof( ___io_path_t ) );
                            strlcat( path, "@",      sizeof( ___io_path_t ) );
                            strlcat( path, location, sizeof( ___io_path_t ) );
                        }
                        else
                        {
                            status = kIOReturnBadArgument;
                        }
                    }
                    else
                    {
                        if ( strlen( path ) + strlen( "/" ) + strlen( name ) < sizeof( ___io_path_t ) )
                        {
                            strlcat( path, "/",  sizeof( ___io_path_t ) );
                            strlcat( path, name, sizeof( ___io_path_t ) );

                            status = kIOReturnSuccess;
                        }
                        else
                        {
                            status = kIOReturnBadArgument;
                        }
                    }
                }
            }

            IOObjectRelease( parent );
        }
    }

    return status;
}

__private_extern__ CFArrayRef ___SCDynamicStoreCopyConsoleInformation( SCDynamicStoreRef store )
{
    CFMutableArrayRef userList;

    userList = ( void * ) SCDynamicStoreCopyConsoleInformation( store );

    if ( userList )
    {
        CFMutableArrayRef array;

        array = CFArrayCreateMutableCopy( kCFAllocatorDefault, 0, userList );

        CFRelease( userList );

        userList = array;

        if ( userList )
        {
            CFIndex count;
            CFIndex index;

            count = CFArrayGetCount( userList );

            for ( index = count - 1; index > -1; index-- )
            {
                CFDictionaryRef dictionary;

                dictionary = CFArrayGetValueAtIndex( userList, index );

                if ( CFDictionaryGetValue( dictionary, kSCConsoleSessionLoginDone ) == kCFBooleanFalse )
                {
                    CFArrayRemoveValueAtIndex( userList, index );
                }
            }

            if ( CFArrayGetCount( userList ) == 0 )
            {
                CFRelease( userList );

                userList = NULL;
            }
        }
    }
///w:start
    else
    {
        CFStringRef user;

        user = ___SCDynamicStoreCopyConsoleUser( store, NULL, NULL );

        if ( user )
        {
            CFMutableDictionaryRef dictionary;

            dictionary = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

            if ( dictionary )
            {
                CFDictionarySetValue( dictionary, kSCConsoleSessionUserName, user );

                userList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

                if ( userList )
                {
                    CFArrayAppendValue( userList, dictionary );
                }

                CFRelease( dictionary );
            }

            CFRelease( user );
        }
    }
///w:stop

    return userList;
}

__private_extern__ CFStringRef ___SCDynamicStoreCopyConsoleUser( SCDynamicStoreRef store, uid_t * uid, gid_t * gid )
{
    CFStringRef user;

    if ( gid )
    {
        *gid = ___GID_WHEEL;
    }

    if ( uid )
    {
        *uid = ___UID_ROOT;
    }

    user = SCDynamicStoreCopyConsoleUser( store, uid, gid );
///w:start
    if ( user )
    {
        if ( CFEqual( user, CFSTR( "loginwindow" ) ) )
        {
            if ( gid )
            {
                *gid = ___GID_WHEEL;
            }

            if ( uid )
            {
                *uid = ___UID_ROOT;
            }

            CFRelease( user );

            user = NULL;
        }
    }
///w:stop

    return user;
}
