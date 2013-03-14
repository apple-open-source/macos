/*
 * Copyright (c) 1998-2012 Apple Inc. All rights reserved.
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

#include "DAMount.h"

#include "DABase.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAMain.h"
#include "DASupport.h"

#include <libgen.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

struct __DAMountCallbackContext
{
    DAMountCallback callback;
    void *          callbackContext;
    DADiskRef       disk;
    CFURLRef        mountpoint;
    CFStringRef     options;
};

typedef struct __DAMountCallbackContext __DAMountCallbackContext;

static void __DAMountWithArgumentsCallbackStage1( int status, void * context );
static void __DAMountWithArgumentsCallbackStage2( int status, void * context );

static void __DAMountWithArgumentsCallback( int status, void * parameter )
{
    /*
     * Process the mount request completion.
     */

    __DAMountCallbackContext * context = parameter;

    if ( context->callback )
    {
        ( context->callback )( status, context->mountpoint, context->callbackContext );
    }

    CFRelease( context->disk       );
    CFRelease( context->mountpoint );
    CFRelease( context->options    );

    free( context );
}

static void __DAMountWithArgumentsCallbackStage1( int status, void * parameter )
{
    /*
     * Process the mount command's completion.
     */

    __DAMountCallbackContext * context = parameter;

    if ( status == 0 )
    {
        _DAMountCreateTrashFolder( context->disk, context->mountpoint );

        if ( DADiskGetState( context->disk, kDADiskStateRequireRepairQuotas ) )
        {
            /*
             * Execute the "repair quotas" command.
             */

            DAFileSystemRepairQuotas( DADiskGetFileSystem( context->disk ),
                                      context->mountpoint,
                                      __DAMountWithArgumentsCallbackStage2,
                                      context );

            return;
        }
    }
    else
    {
         DAMountRemoveMountPoint( context->mountpoint );
    }

    __DAMountWithArgumentsCallback( status, context );
}

static void __DAMountWithArgumentsCallbackStage2( int status, void * parameter )
{
    /*
     * Process the "repair quotas" command's completion.
     */

    __DAMountCallbackContext * context = parameter;

    if ( status )
    {
        DALogError( "unable to repair quotas on disk %@ (status code 0x%08X).", context->disk, status );
    }
    else
    {
        DADiskSetState( context->disk, kDADiskStateRequireRepairQuotas, FALSE );
    }

    __DAMountWithArgumentsCallback( 0, context );
}

void _DAMountCreateTrashFolder( DADiskRef disk, CFURLRef mountpoint )
{
    /*
     * Create the trash folder in which the user trashes will be stored.
     */

    /*
     * Determine whether the disk is writable.
     */

    if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanTrue )
    {
        char path[MAXPATHLEN];

        /*
         * Obtain the mount point path.
         */

        if ( CFURLGetFileSystemRepresentation( mountpoint, TRUE, ( void * ) path, sizeof( path ) ) )
        {
            struct stat status;

            /*
             * Determine whether the trash folder exists.
             */

            strlcat( path, "/.Trashes", sizeof( path ) );

            if ( stat( path, &status ) )
            {
                /*
                 * Create the trash folder.
                 */

                if ( ___mkdir( path, 01333 ) == 0 )
                {
                    /*
                     * Correct the trash folder's attributes.
                     */

                    ___chattr( path, ___ATTR_INVISIBLE, 0 );
                }
            }
        }
    }
}

void DAMount( DADiskRef disk, CFURLRef mountpoint, DAMountCallback callback, void * callbackContext )
{
    /*
     * Mount the specified volume.  A status of 0 indicates success.
     */

    return DAMountWithArguments( disk, mountpoint, callback, callbackContext, NULL );

}

Boolean DAMountContainsArgument( CFStringRef arguments, CFStringRef argument )
{
    CFRange range;

    range = CFStringFind( arguments, argument, 0 );

    if ( range.length )
    {
        if ( range.location )
        {
            if ( CFStringGetCharacterAtIndex( arguments, range.location - 1 ) != ',' )
            {
                range.length = 0;
            }
        }

        if ( range.location + range.length < CFStringGetLength( arguments ) )
        {
            if ( CFStringGetCharacterAtIndex( arguments, range.location + range.length ) != ',' )
            {
                range.length = 0;
            }
        }
    }

    return range.length ? TRUE : FALSE;
}

CFURLRef DAMountCreateMountPoint( DADiskRef disk )
{
    return DAMountCreateMountPointWithAction( disk, kDAMountPointActionMake );
}

CFURLRef DAMountCreateMountPointWithAction( DADiskRef disk, DAMountPointAction action )
{
    FILE *      file;
    CFIndex     index;
    CFURLRef    mountpoint;
    char        name[MAXPATHLEN];
    char        path[MAXPATHLEN];
    struct stat status;
    CFStringRef string;

    mountpoint = NULL;

    /*
     * Obtain the volume name.
     */

    string = DADiskGetDescription( disk, kDADiskDescriptionVolumeNameKey );

    if ( string )
    {
        if ( CFStringGetLength( string ) )
        {
            CFRetain( string );
        }
        else
        {
            string = NULL;
        }
    }

    if ( string == NULL )
    {
        string = ___CFBundleCopyLocalizedStringInDirectory( gDABundlePath, CFSTR( "Untitled" ), CFSTR( "Untitled" ), NULL );
    }

    if ( ___CFStringGetCString( string, name, MNAMELEN - 20 ) )
    {
        /*
         * Adjust the volume name.
         */

        while ( strchr( name, '/' ) )
        {
            *strchr( name, '/' ) = ':';
        }

        /*
         * Create the mount point path.
         */

        for ( index = 0; index < 100; index++ )
        {
            if ( index == 0 )
            {
                snprintf( path, sizeof( path ), "%s/%s", kDAMainMountPointFolder, name );
            }
            else
            {
                snprintf( path, sizeof( path ), "%s/%s %lu", kDAMainMountPointFolder, name, index );
            }

            if ( stat( path, &status ) )
            {
                if ( errno == ENOENT )
                {
                    switch ( action )
                    {
                        case kDAMountPointActionLink:
                        {
                            /*
                             * Link the mount point.
                             */

                            CFURLRef url;

                            url = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

                            if ( url )
                            {
                                char source[MAXPATHLEN];

                                if ( CFURLGetFileSystemRepresentation( url, TRUE, ( void * ) source, sizeof( source ) ) )
                                {
                                    if ( symlink( source, path ) == 0 )
                                    {
                                        mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                                    }
                                }
                            }

                            break;
                        }
                        case kDAMountPointActionMake:
                        {
                            /*
                             * Create the mount point.
                             */

                            if ( mkdir( path, 0111 ) == 0 )
                            {
                                if ( DADiskGetUserUID( disk ) )
                                {
                                    chown( path, DADiskGetUserUID( disk ), -1 );
                                }

                                mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );

                                /*
                                 * Create the mount point cookie file.
                                 */

                                strlcat( path, "/",                               sizeof( path ) );
                                strlcat( path, kDAMainMountPointFolderCookieFile, sizeof( path ) );

                                file = fopen( path, "w" );

                                if ( file )
                                {
                                    fclose( file );
                                }
                            }

                            break;
                        }
                        case kDAMountPointActionMove:
                        {
                            /*
                             * Move the mount point.
                             */

                            CFURLRef url;

                            url = DADiskGetBypath( disk );

                            if ( url )
                            {
                                char source[MAXPATHLEN];

                                if ( CFURLGetFileSystemRepresentation( url, TRUE, ( void * ) source, sizeof( source ) ) )
                                {
                                    if ( rename( source, path ) == 0 )
                                    {
                                        mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                                    }
                                }
                            }

                            break;
                        }
                        case kDAMountPointActionNone:
                        {
                            mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );

                            break;
                        }
                    }

                    if ( mountpoint )
                    {
                        break;
                    }
                }
            }
        }
    }

    CFRelease( string );

    return mountpoint;
}

Boolean DAMountGetPreference( DADiskRef disk, DAMountPreference preference )
{
    CFBooleanRef value;

    switch ( preference )
    {
        case kDAMountPreferenceDefer:
        {
            /*
             * Determine whether the media is removable.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue )
            {
                value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey );

                value = value ? value : kCFBooleanTrue;
            }
            else
            {
                /*
                 * Determine whether the device is internal.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferInternalKey );

                    value = value ? value : kCFBooleanFalse;
                }
                else
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey );

                    value = value ? value : kCFBooleanTrue;
                }
            }

            break;
        }
        case kDAMountPreferenceTrust:
        {
            /*
             * Determine whether the media is removable.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue )
            {
                value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustRemovableKey );

                value = value ? value : kCFBooleanFalse;
            }
            else
            {
                /*
                 * Determine whether the device is internal.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustInternalKey );

                    value = value ? value : kCFBooleanTrue;
                }
                else
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey );

                    value = value ? value : kCFBooleanFalse;
                }
            }

            break;
        }
        case kDAMountPreferenceWrite:
        {
            value = kCFBooleanTrue;
///w:start
            if ( DADiskGetState( disk, _kDADiskStateMountPreferenceNoWrite ) )
            {
                DADiskSetState( disk, _kDADiskStateMountPreferenceNoWrite, FALSE );

                value = kCFBooleanFalse;
            }
///w:stop

            break;
        }
        default:
        {
            value = kCFBooleanFalse;

            break;            
        }
    }

    assert( value );

    return CFBooleanGetValue( value );
}

void DAMountRemoveMountPoint( CFURLRef mountpoint )
{
    char path[MAXPATHLEN];

    /*
     * Obtain the mount point path.
     */

    if ( CFURLGetFileSystemRepresentation( mountpoint, TRUE, ( void * ) path, sizeof( path ) ) )
    {
        if ( ___isautofs( path ) == 0 )
        {
            Boolean     remove;
            struct stat status;

            remove = FALSE;

            if ( strcmp( dirname( path ), kDAMainMountPointFolder ) == 0 )
            {
                remove = TRUE;
            }

            /*
             * Determine whether the mount point cookie file exists.
             */

            strlcat( path, "/",                               sizeof( path ) );
            strlcat( path, kDAMainMountPointFolderCookieFile, sizeof( path ) );

            if ( stat( path, &status ) == 0 )
            {
                /*
                 * Remove the mount point cookie file.
                 */

                if ( unlink( path ) == 0 )
                {
                    remove = TRUE;
                }
            }

            if ( remove )
            {
                /*
                 * Remove the mount point.
                 */

                rmdir( dirname( path ) );
            }
        }
    }
}

void DAMountWithArguments( DADiskRef disk, CFURLRef mountpoint, DAMountCallback callback, void * callbackContext, ... )
{
    /*
     * Mount the specified volume.  A status of 0 indicates success.  All arguments in
     * the argument list shall be of type CFStringRef.  The argument list must be NULL
     * terminated.
     */

    CFStringRef                argument   = NULL;
    va_list                    arguments;
    CFBooleanRef               automatic  = kCFBooleanTrue;
    __DAMountCallbackContext * context    = NULL;
    CFIndex                    count      = 0;
    DAFileSystemRef            filesystem = DADiskGetFileSystem( disk );
    CFIndex                    index      = 0;
    CFDictionaryRef            map        = NULL;
    CFMutableStringRef         options    = NULL;
    int                        status     = 0;

    /*
     * Initialize our minimal state.
     */

    if ( mountpoint )
    {
        CFRetain( mountpoint );
    }

    /*
     * Prepare the mount context.
     */

    context = malloc( sizeof( __DAMountCallbackContext ) );

    if ( context == NULL )
    {
        status = ENOMEM;

        goto DAMountWithArgumentsErr;
    }

    /*
     * Prepare the mount options.
     */

    options = CFStringCreateMutable( kCFAllocatorDefault, 0 );

    if ( options == NULL )
    {
        status = ENOMEM;

        goto DAMountWithArgumentsErr;
    }

    va_start( arguments, callbackContext );

    while ( ( argument = va_arg( arguments, CFStringRef ) ) )
    {
        CFStringAppend( options, argument );
        CFStringAppend( options, CFSTR( "," ) );
    }

    va_end( arguments );

    CFStringTrim( options, CFSTR( "," ) );

    if ( CFEqual( options, CFSTR( "automatic" ) ) )
    {
        automatic = NULL;

        CFStringReplaceAll( options, CFSTR( "" ) );
    }

    /*
     * Determine whether the volume is to be updated.
     */

    if ( DAMountContainsArgument( options, kDAFileSystemMountArgumentUpdate ) )
    {
        if ( mountpoint )
        {
            status = EINVAL;

            goto DAMountWithArgumentsErr;
        }

        mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

        if ( mountpoint == NULL )
        {
            status = EINVAL;

            goto DAMountWithArgumentsErr;
        }

        CFRetain( mountpoint );
    }

    /*
     * Determine whether the volume is clean.
     */

    if ( automatic == NULL )
    {
        if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
        {
            CFStringInsert( options, 0, CFSTR( "," ) );
            CFStringInsert( options, 0, kDAFileSystemMountArgumentNoWrite );
        }
    }

    /*
     * Scan the mount map list.
     */

    count = CFArrayGetCount( gDAMountMapList1 );

    for ( index = 0; index < count; index++ )
    {
        map = CFArrayGetValueAtIndex( gDAMountMapList1, index );

        if ( map )
        {
            CFTypeRef   id;
            CFStringRef kind;

            id   = CFDictionaryGetValue( map, kDAMountMapProbeIDKey );
            kind = CFDictionaryGetValue( map, kDAMountMapProbeKindKey );

            if ( kind )
            {
                /*
                 * Determine whether the volume kind matches.
                 */

                if ( CFEqual( kind, DAFileSystemGetKind( filesystem ) ) == FALSE )
                {
                    continue;
                }
            }

            if ( CFGetTypeID( id ) == CFUUIDGetTypeID( ) )
            {
                /*
                 * Determine whether the volume UUID matches.
                 */

                if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, id ) == kCFCompareEqualTo )
                {
                    break;
                }
            }
            else if ( CFGetTypeID( id ) == CFStringGetTypeID( ) )
            {
                /*
                 * Determine whether the volume name matches.
                 */

                if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeNameKey, id ) == kCFCompareEqualTo )
                {
                    break;
                }
            }
            else if ( CFGetTypeID( id ) == CFDictionaryGetTypeID( ) )
            {
                boolean_t match = FALSE;

                /*
                 * Determine whether the device description matches.
                 */

                IOServiceMatchPropertyTable( DADiskGetIOMedia( disk ), id, &match );

                if ( match )
                {
                    break;
                }
            }
        }
    }

    /*
     * Process the map.
     */

    if ( index < count )
    {
        CFStringRef string;

        /*
         * Determine whether the volume is to be mounted.
         */

        if ( automatic == NULL )
        {
            automatic = CFDictionaryGetValue( map, kDAMountMapMountAutomaticKey );

            if ( automatic == kCFBooleanTrue )
            {
                DADiskSetOption( disk, kDADiskOptionMountAutomatic,        TRUE );
                DADiskSetOption( disk, kDADiskOptionMountAutomaticNoDefer, TRUE );
            }
        }

        /*
         * Prepare the mount options.
         */

        string = CFDictionaryGetValue( map, kDAMountMapMountOptionsKey );

        if ( string )
        {
            CFStringInsert( options, 0, CFSTR( "," ) );
            CFStringInsert( options, 0, string );
        }

        /*
         * Prepare the mount point.
         */

        if ( mountpoint == NULL )
        {
            mountpoint = CFDictionaryGetValue( map, kDAMountMapMountPathKey );

            if ( mountpoint )
            {
                CFRetain( mountpoint );
            }
        }
    }

    /*
     * Scan the mount map list.
     */

    count = CFArrayGetCount( gDAMountMapList2 );

    for ( index = 0; index < count; index++ )
    {
        map = CFArrayGetValueAtIndex( gDAMountMapList2, index );

        if ( map )
        {
            CFTypeRef id;

            id = CFDictionaryGetValue( map, kDAMountMapProbeIDKey );

            /*
             * Determine whether the volume UUID matches.
             */

            if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, id ) == kCFCompareEqualTo )
            {
                break;
            }
        }
    }

    /*
     * Process the map.
     */

    if ( index < count )
    {
        CFStringRef string;

        /*
         * Prepare the mount options.
         */

        string = CFDictionaryGetValue( map, kDAMountMapMountOptionsKey );

        if ( string )
        {
            CFStringInsert( options, 0, CFSTR( "," ) );
            CFStringInsert( options, 0, string );
        }
    }

    /*
     * Determine whether the volume is to be mounted.
     */

    if ( automatic == NULL )
    {
        if ( DADiskGetOption( disk, kDADiskOptionMountAutomatic ) )
        {
            if ( DADiskGetOption( disk, kDADiskOptionMountAutomaticNoDefer ) )
            {
                automatic = kCFBooleanTrue;
            }
        }
        else
        {
            automatic = kCFBooleanFalse;
        }

        if ( automatic == NULL )
        {
            if ( gDAConsoleUserList == NULL )
            {
                if ( DAMountGetPreference( disk, kDAMountPreferenceDefer ) )
                {
                    automatic = kCFBooleanFalse;
                }
            }
        }
    }

    if ( automatic == kCFBooleanFalse )
    {
        status = ECANCELED;

        goto DAMountWithArgumentsErr;
    }

    /*
     * Prepare the mount options.
     */

    if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
    {
        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoWrite );
    }

    if ( DAMountGetPreference( disk, kDAMountPreferenceWrite ) == FALSE )
    {
        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoWrite );
    }

    if ( DAMountGetPreference( disk, kDAMountPreferenceTrust ) == FALSE )
    {
        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoSetUserID );

        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoOwnership );

        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoDevice );
    }
///w:start
    if ( CFEqual( DAFileSystemGetKind( filesystem ), CFSTR( "hfs" ) ) )
    {
        ___CFStringInsertFormat( options, 0, CFSTR( "-m=%o," ), 0755 );

        if ( DADiskGetUserGID( disk ) )
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-g=%d," ), DADiskGetUserGID( disk ) );
        }
        else
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-g=%d," ), ___GID_UNKNOWN );
        }

        if ( DADiskGetUserUID( disk ) )
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-u=%d," ), DADiskGetUserUID( disk ) );
        }
        else
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-u=%d," ), ___UID_UNKNOWN );
        }
    }
///w:stop

    CFStringTrim( options, CFSTR( "," ) );

    /*
     * Create the mount point, in case one needs to be created.
     */

    if ( mountpoint == NULL )
    {
        mountpoint = DAMountCreateMountPointWithAction( disk, kDAMountPointActionMake );

        if ( mountpoint == NULL )
        {
            status = ENOSPC;

            goto DAMountWithArgumentsErr;
        }
    }

    /*
     * Mount the volume.
     */

    CFRetain( disk );

    context->callback        = callback;
    context->callbackContext = callbackContext;
    context->disk            = disk;
    context->mountpoint      = mountpoint;
    context->options         = options;

    DAFileSystemMountWithArguments( DADiskGetFileSystem( disk ),
                                    DADiskGetDevice( disk ),
                                    mountpoint,
                                    DADiskGetUserUID( disk ),
                                    DADiskGetUserGID( disk ),
                                    __DAMountWithArgumentsCallbackStage1,
                                    context,
                                    options,
                                    NULL );

DAMountWithArgumentsErr:

    if ( status )
    {
        if ( context )
        {
            free( context );
        }

        if ( mountpoint )
        {
            CFRelease( mountpoint );
        }

        if ( options )
        {
            CFRelease( options );
        }

        if ( callback )
        {
            ( callback )( status, NULL, callbackContext );
        }
    }
}
