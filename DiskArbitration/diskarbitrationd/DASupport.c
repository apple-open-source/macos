/*
 * Copyright (c) 1998-2011 Apple Inc. All Rights Reserved.
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

#include "DASupport.h"

#include "vsdb.h"
#include "DABase.h"
#include "DAFileSystem.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAMain.h"
#include "DAThread.h"

#include <dirent.h>
#include <fsproperties.h>
#include <fstab.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/loadable_fs.h>
#include <sys/stat.h>
#include <SystemConfiguration/SystemConfiguration.h>

struct __DAAuthorizeWithCallbackContext
{
    DAAuthorizeCallback callback;
    void *              callbackContext;
    DADiskRef           disk;
    DAAuthorizeOptions  options;
    char *              right;
    DASessionRef        session;
    DAReturn            status;
    gid_t               userGID;
    uid_t               userUID;
};

typedef struct __DAAuthorizeWithCallbackContext __DAAuthorizeWithCallbackContext;

static pthread_mutex_t __gDAAuthorizeWithCallbackLock = PTHREAD_MUTEX_INITIALIZER;

int __DAAuthorizeWithCallback( void * parameter )
{
    __DAAuthorizeWithCallbackContext * context = parameter;

    pthread_mutex_lock( &__gDAAuthorizeWithCallbackLock );

    context->status = DAAuthorize( context->session, context->options, context->disk, context->userUID, context->userGID, context->right );

    pthread_mutex_unlock( &__gDAAuthorizeWithCallbackLock );

    return 0;
}

void __DAAuthorizeWithCallbackCallback( int status, void * parameter )
{
    __DAAuthorizeWithCallbackContext * context = parameter;

    ( context->callback )( context->status, context->callbackContext );

    if ( context->disk    )  CFRelease( context->disk );
    if ( context->session )  CFRelease( context->session );

    free( context->right );
    free( context );
}

DAReturn DAAuthorize( DASessionRef       session,
                      DAAuthorizeOptions options,
                      DADiskRef          disk,
                      uid_t              userUID,
                      gid_t              userGID,
                      const char *       right )
{
    DAReturn status;

    status = kDAReturnNotPrivileged;

    if ( status )
    {
        if ( userUID == ___UID_ROOT )
        {
            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        if ( ___isadmin( userUID ) )
        {
            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        if ( disk )
        {
            uid_t diskUID;

            diskUID = DADiskGetUserRUID( disk );

            if ( diskUID == userUID )
            {
                status = kDAReturnSuccess;
            }

            if ( diskUID == ___UID_UNKNOWN )
            {
                status = kDAReturnSuccess;
            }
        }
    }

    if ( ( options & kDAAuthorizeOptionForce ) )
    {
        status = kDAReturnNotPrivileged;
    }

    if ( status )
    {
        AuthorizationRef authorization;

///w:start
        authorization = NULL;

        if ( session == NULL )
        {
            CFIndex count;
            CFIndex index;

            count = CFArrayGetCount( gDASessionList );

            for ( index = 0; index < count; index++, session = NULL )
            {
                session = ( void * ) CFArrayGetValueAtIndex( gDASessionList, index );

                if ( strcmp( _DASessionGetName( session ), "SystemUIServer" ) == 0 )
                {
                    break;
                }
            }
        }

        if ( session )
///w:stop
        authorization = DASessionGetAuthorization( session );

        if ( authorization )
        {
            AuthorizationFlags  flags;
            AuthorizationItem   item;
            AuthorizationRights rights;

            flags = kAuthorizationFlagExtendRights;

///w:start
//          if ( ( options & kDAAuthorizeOptionForce ) )
//          {
//              flags |= kAuthorizationFlagDestroyRights;
//          }
///w:stop
            if ( ( options & kDAAuthorizeOptionInteract ) )
            {
                flags |= kAuthorizationFlagInteractionAllowed;
            }

            item.flags       = 0;
            item.name        = right;
            item.value       = NULL;
            item.valueLength = 0;

            rights.count = 1;
            rights.items = &item;

            status = AuthorizationCopyRights( authorization, &rights, NULL, flags, NULL );

            if ( status )
            {
                status = kDAReturnNotPrivileged;
            }
        }
    }

    return status;
}

void DAAuthorizeWithCallback( DASessionRef        session,
                              DAAuthorizeOptions  options,
                              DADiskRef           disk,
                              uid_t               userUID,
                              gid_t               userGID,
                              DAAuthorizeCallback callback,
                              void *              callbackContext,
                              const char *        right )
{
    DAReturn status;

    status = DAAuthorize( session, ( options & ~kDAAuthorizeOptionInteract ), disk, userUID, userGID, right );

    if ( status )
    {
        __DAAuthorizeWithCallbackContext * context;

        context = malloc( sizeof( __DAAuthorizeWithCallbackContext ) );

        if ( context )
        {
            if ( disk    )  CFRetain( disk );
            if ( session )  CFRetain( session );

            context->callback        = callback;
            context->callbackContext = callbackContext;
            context->disk            = disk;
            context->options         = options;
            context->right           = strdup( right );
            context->session         = session;
            context->status          = kDAReturnNotPrivileged;
            context->userGID         = userGID;
            context->userUID         = userUID;

            DAThreadExecute( __DAAuthorizeWithCallback, context, __DAAuthorizeWithCallbackCallback, context );
        }
        else
        {
            ( callback )( kDAReturnNotPrivileged, callbackContext );
        }
    }
    else
    {
        ( callback )( kDAReturnSuccess, callbackContext );
    }
}

static struct timespec __gDAFileSystemListTime = { 0, 0 };

const CFStringRef kDAFileSystemKey = CFSTR( "DAFileSystem" );

static void __DAFileSystemProbeListAppendValue( const void * key, const void * value, void * context )
{
    CFMutableDictionaryRef probe;

    probe = CFDictionaryCreateMutableCopy( kCFAllocatorDefault, 0, value );

    if ( probe )
    {
        CFDictionarySetValue( probe, kDAFileSystemKey, context );
        CFArrayAppendValue( gDAFileSystemProbeList, probe );
        CFRelease( probe );
    }
}

static CFComparisonResult __DAFileSystemProbeListCompare( const void * value1, const void * value2, void * context )
{
    CFNumberRef order1 = CFDictionaryGetValue( value1, CFSTR( kFSProbeOrderKey ) );
    CFNumberRef order2 = CFDictionaryGetValue( value2, CFSTR( kFSProbeOrderKey ) );

    if ( order1 == NULL )  return kCFCompareGreaterThan;
    if ( order2 == NULL )  return kCFCompareLessThan;

    return CFNumberCompare( order1, order2, NULL );
}

void DAFileSystemListRefresh( void )
{
    struct stat status;

    /*
     * Determine whether the file system list is up-to-date.
     */

    if ( stat( FS_DIR_LOCATION, &status ) == 0 )
    {
        if ( __gDAFileSystemListTime.tv_sec  != status.st_mtimespec.tv_sec  ||
             __gDAFileSystemListTime.tv_nsec != status.st_mtimespec.tv_nsec )
        {
            CFURLRef base;

            __gDAFileSystemListTime.tv_sec  = status.st_mtimespec.tv_sec;
            __gDAFileSystemListTime.tv_nsec = status.st_mtimespec.tv_nsec;

            /*
             * Clear the file system list.
             */

            CFArrayRemoveAllValues( gDAFileSystemList );
            CFArrayRemoveAllValues( gDAFileSystemProbeList );

            /*
             * Build the file system list.
             */

            base = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, CFSTR( FS_DIR_LOCATION ), kCFURLPOSIXPathStyle, TRUE );

            if ( base )
            {
                DIR * folder;

                /*
                 * Scan the filesystems in the file system folder.
                 */

                folder = opendir( FS_DIR_LOCATION );

                if ( folder )
                {
                    struct dirent * item;

                    DALogDebugHeader( "filesystems have been refreshed." );

                    while ( ( item = readdir( folder ) ) )
                    {
                        char * suffix;

                        suffix = item->d_name + strlen( item->d_name ) - strlen( FS_DIR_SUFFIX );

                        if ( suffix > item->d_name )
                        {
                            if ( strcmp( suffix, FS_DIR_SUFFIX ) == 0 )
                            {
                                CFURLRef path;

                                path = CFURLCreateFromFileSystemRepresentationRelativeToBase( kCFAllocatorDefault,
                                                                                              ( void * ) item->d_name,
                                                                                              strlen( item->d_name ),
                                                                                              TRUE,
                                                                                              base );

                                if ( path )
                                {
                                    DAFileSystemRef filesystem;

                                    /*
                                     * Create a file system object for this file system.
                                     */

                                    filesystem = DAFileSystemCreate( kCFAllocatorDefault, path );

                                    if ( filesystem )
                                    {
                                        CFDictionaryRef probe;

                                        /*
                                         * Add this file system object to our list.
                                         */

                                        DALogDebug( "  created filesystem, id = %@.", filesystem );

                                        CFArrayAppendValue( gDAFileSystemList, filesystem );

                                        probe = DAFileSystemGetProbeList( filesystem );

                                        if ( probe )
                                        {
                                            CFDictionaryApplyFunction( probe, __DAFileSystemProbeListAppendValue, filesystem );
                                        }

                                        CFRelease( filesystem );
                                    }

                                    CFRelease( path );
                                }
                            }
                        }
                    }

                    closedir( folder );
                }

                CFRelease( base );
            }

            /*
             * Order the probe list.
             */

            CFArraySortValues( gDAFileSystemProbeList,
                               CFRangeMake( 0, CFArrayGetCount( gDAFileSystemProbeList ) ),
                               __DAFileSystemProbeListCompare,
                               NULL );
        }
    }
}

static struct timespec __gDAMountMapListTime1 = { 0, 0 };
static struct timespec __gDAMountMapListTime2 = { 0, 0 };

const CFStringRef kDAMountMapMountAutomaticKey = CFSTR( "DAMountAutomatic" );
const CFStringRef kDAMountMapMountOptionsKey   = CFSTR( "DAMountOptions"   );
const CFStringRef kDAMountMapMountPathKey      = CFSTR( "DAMountPath"      );
const CFStringRef kDAMountMapProbeIDKey        = CFSTR( "DAProbeID"        );
const CFStringRef kDAMountMapProbeKindKey      = CFSTR( "DAProbeKind"      );

static CFDictionaryRef __DAMountMapCreate1( CFAllocatorRef allocator, struct fstab * fs )
{
    CFMutableDictionaryRef map = NULL;

    if ( strcmp( fs->fs_type, FSTAB_SW ) )
    {
        char * idAsCString = fs->fs_spec;

        strsep( &idAsCString, "=" );

        if ( idAsCString )
        {
            CFStringRef idAsString;

            idAsString = CFStringCreateWithCString( kCFAllocatorDefault, idAsCString, kCFStringEncodingUTF8 );

            if ( idAsString )
            {
                CFTypeRef id = NULL;

                if ( strcmp( fs->fs_spec, "UUID" ) == 0 )
                {
                    id = _DAFileSystemCreateUUIDFromString( kCFAllocatorDefault, idAsString );
                }
                else if ( strcmp( fs->fs_spec, "LABEL" ) == 0 )
                {
                    id = CFRetain( idAsString );
                }
                else if ( strcmp( fs->fs_spec, "DEVICE" ) == 0 )
                {
                    id = ___CFDictionaryCreateFromXMLString( kCFAllocatorDefault, idAsString );
                }

                if ( id )
                {
                    map = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

                    if ( map )
                    {
                        CFMutableStringRef options;

                        options = CFStringCreateMutable( kCFAllocatorDefault, 0 );

                        if ( options )
                        {
                            char *       argument  = NULL;
                            char *       arguments = fs->fs_mntops;
                            CFBooleanRef automatic = NULL;

                            while ( ( argument = strsep( &arguments, "," ) ) )
                            {
                                if ( strcmp( argument, "auto" ) == 0 )
                                {
                                    automatic = kCFBooleanTrue;
                                }
                                else if ( strcmp( argument, "noauto" ) == 0 )
                                {
                                    automatic = kCFBooleanFalse;
                                }
                                else
                                {
                                    CFStringAppendCString( options, argument, kCFStringEncodingUTF8 );    
                                    CFStringAppendCString( options, ",", kCFStringEncodingUTF8 );
                                }
                            }

                            if ( automatic )
                            {
                                CFDictionarySetValue( map, kDAMountMapMountAutomaticKey, automatic );
                            }

                            if ( CFStringGetLength( options ) )
                            {
                                CFStringTrim( options, CFSTR( "," ) );

                                CFDictionarySetValue( map, kDAMountMapMountOptionsKey, options );
                            }

                            CFRelease( options );
                        }

                        if ( strcmp( fs->fs_file, "none" ) )
                        {
                            CFURLRef path;

                            path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) fs->fs_file, strlen( fs->fs_file ), TRUE );

                            if ( path )
                            {
                                CFDictionarySetValue( map, kDAMountMapMountPathKey, path );

                                CFRelease( path );
                            }
                        }

                        if ( strcmp( fs->fs_vfstype, "auto" ) )
                        {
                            CFStringRef kind;

                            kind = CFStringCreateWithCString( kCFAllocatorDefault, fs->fs_vfstype, kCFStringEncodingUTF8 );

                            if ( kind )
                            {
                                CFDictionarySetValue( map, kDAMountMapProbeKindKey, kind );

                                CFRelease( kind );
                            }
                        }

                        CFDictionarySetValue( map, kDAMountMapProbeIDKey, id );
                    }

                    CFRelease( id );
                }

                CFRelease( idAsString );
            }
        }
    }

    return map;
}

void DAMountMapListRefresh1( void )
{
    struct stat status;

    /*
     * Determine whether the mount map list is up-to-date.
     */

    if ( stat( _PATH_FSTAB, &status ) == 0 )
    {
        if ( __gDAMountMapListTime1.tv_sec  != status.st_mtimespec.tv_sec  ||
             __gDAMountMapListTime1.tv_nsec != status.st_mtimespec.tv_nsec )
        {
            __gDAMountMapListTime1.tv_sec  = status.st_mtimespec.tv_sec;
            __gDAMountMapListTime1.tv_nsec = status.st_mtimespec.tv_nsec;

            /*
             * Clear the mount map list.
             */

            CFArrayRemoveAllValues( gDAMountMapList1 );

            /*
             * Build the mount map list.
             */

            if ( setfsent( ) )
            {
                struct fstab * item;

                while ( ( item = getfsent( ) ) )
                {
                    CFDictionaryRef map;

                    map = __DAMountMapCreate1( kCFAllocatorDefault, item );

                    if ( map )
                    {
                        CFArrayAppendValue( gDAMountMapList1, map );

                        CFRelease( map );
                    }
                }

                endfsent( );
            }
        }
    }
}

static CFDictionaryRef __DAMountMapCreate2( CFAllocatorRef allocator, struct vsdb * vs )
{
    CFStringRef            idAsString;
    CFMutableDictionaryRef map = NULL;

    idAsString = CFStringCreateWithCString( kCFAllocatorDefault, vs->vs_spec, kCFStringEncodingUTF8 );

    if ( idAsString )
    {
        CFTypeRef id;

        id = _DAFileSystemCreateUUIDFromString( kCFAllocatorDefault, idAsString );

        if ( id )
        {
            map = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

            if ( map )
            {
                CFMutableStringRef options;

                options = CFStringCreateMutable( kCFAllocatorDefault, 0 );

                if ( options )
                {
                    if ( ( vs->vs_ops & VSDB_PERM ) )
                    {
                        CFStringAppend( options, CFSTR( "owners" ) );
                        CFStringAppend( options, CFSTR( "," ) );
                    }
                    else
                    {
                        CFStringAppend( options, CFSTR( "noowners" ) );
                        CFStringAppend( options, CFSTR( "," ) );
                    }

                    if ( CFStringGetLength( options ) )
                    {
                        CFStringTrim( options, CFSTR( "," ) );

                        CFDictionarySetValue( map, kDAMountMapMountOptionsKey, options );
                    }

                    CFRelease( options );
                }

                CFDictionarySetValue( map, kDAMountMapProbeIDKey, id );
            }

            CFRelease( id );
        }

        CFRelease( idAsString );
    }

    return map;
}

void DAMountMapListRefresh2( void )
{
    struct stat status;

    /*
     * Determine whether the mount map list is up-to-date.
     */

    if ( stat( _PATH_VSDB, &status ) == 0 )
    {
        if ( __gDAMountMapListTime2.tv_sec  != status.st_mtimespec.tv_sec  ||
             __gDAMountMapListTime2.tv_nsec != status.st_mtimespec.tv_nsec )
        {
            __gDAMountMapListTime2.tv_sec  = status.st_mtimespec.tv_sec;
            __gDAMountMapListTime2.tv_nsec = status.st_mtimespec.tv_nsec;

            /*
             * Clear the mount map list.
             */

            CFArrayRemoveAllValues( gDAMountMapList2 );

            /*
             * Build the mount map list.
             */

            if ( setvsent( ) )
            {
                struct vsdb * item;

                while ( ( item = getvsent( ) ) )
                {
                    CFDictionaryRef map;

                    map = __DAMountMapCreate2( kCFAllocatorDefault, item );

                    if ( map )
                    {
                        CFArrayAppendValue( gDAMountMapList2, map );

                        CFRelease( map );
                    }
                }

                endvsent( );
            }
        }
    }
}

const CFStringRef kDAPreferenceMountDeferExternalKey  = CFSTR( "DAMountDeferExternal"  );
const CFStringRef kDAPreferenceMountDeferInternalKey  = CFSTR( "DAMountDeferInternal"  );
const CFStringRef kDAPreferenceMountDeferRemovableKey = CFSTR( "DAMountDeferRemovable" );
const CFStringRef kDAPreferenceMountTrustExternalKey  = CFSTR( "DAMountTrustExternal"  );
const CFStringRef kDAPreferenceMountTrustInternalKey  = CFSTR( "DAMountTrustInternal"  );
const CFStringRef kDAPreferenceMountTrustRemovableKey = CFSTR( "DAMountTrustRemovable" );

void DAPreferenceListRefresh( void )
{
    SCPreferencesRef preferences;

    /*
     * Clear the preference list.
     */

    CFDictionaryRemoveAllValues( gDAPreferenceList );

    /*
     * Build the preference list.
     */

    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey,  kCFBooleanTrue  );
    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferInternalKey,  kCFBooleanFalse );
    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey, kCFBooleanTrue  );
    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey,  kCFBooleanFalse );
    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustInternalKey,  kCFBooleanTrue  );
    CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustRemovableKey, kCFBooleanFalse );

    preferences = SCPreferencesCreate( kCFAllocatorDefault, CFSTR( "autodiskmount" ), CFSTR( "autodiskmount.xml" ) );

    if ( preferences )
    {
        CFTypeRef value;

        value = SCPreferencesGetValue( preferences, CFSTR( "AutomountDisksWithoutUserLogin" ) );

        if ( value == kCFBooleanTrue )
        {
            CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey,  kCFBooleanFalse );
            CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey, kCFBooleanFalse );
            CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey,  kCFBooleanTrue  );
        }
        else if ( value == kCFBooleanFalse )
        {
            CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey,  kCFBooleanFalse );
            CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey, kCFBooleanTrue  );
            CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey,  kCFBooleanTrue  );
        }

        CFRelease( preferences );
    }

    preferences = SCPreferencesCreate( kCFAllocatorDefault, CFSTR( _kDADaemonName ), CFSTR( _kDADaemonName ".plist" ) );

    if ( preferences )
    {
        CFTypeRef value;

        value = SCPreferencesGetValue( preferences, kDAPreferenceMountDeferExternalKey );

        if ( value )
        {
            if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
            {
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey, value );
            }
        }

        value = SCPreferencesGetValue( preferences, kDAPreferenceMountDeferInternalKey );

        if ( value )
        {
            if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
            {
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferInternalKey, value );
            }
        }

        value = SCPreferencesGetValue( preferences, kDAPreferenceMountDeferRemovableKey );

        if ( value )
        {
            if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
            {
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey, value );
            }
        }

        value = SCPreferencesGetValue( preferences, kDAPreferenceMountTrustExternalKey );

        if ( value )
        {
            if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
            {
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey, value );
            }
        }

        value = SCPreferencesGetValue( preferences, kDAPreferenceMountTrustInternalKey );

        if ( value )
        {
            if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
            {
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustInternalKey, value );
            }
        }

        value = SCPreferencesGetValue( preferences, kDAPreferenceMountTrustRemovableKey );

        if ( value )
        {
            if ( CFGetTypeID( value ) == CFBooleanGetTypeID( ) )
            {
                CFDictionarySetValue( gDAPreferenceList, kDAPreferenceMountTrustRemovableKey, value );
            }
        }

        CFRelease( preferences );
    }
}

struct __DAUnit
{
    DAUnitState state;
};

typedef struct __DAUnit __DAUnit;

Boolean DAUnitGetState( DADiskRef disk, DAUnitState state )
{
    CFNumberRef key;

    key = DADiskGetDescription( disk, kDADiskDescriptionMediaBSDUnitKey );

    if ( key )
    {
        CFMutableDataRef data;

        data = ( CFMutableDataRef ) CFDictionaryGetValue( gDAUnitList, key );

        if ( data )
        {
            __DAUnit * unit;

            unit = ( void * ) CFDataGetMutableBytePtr( data );

            return ( unit->state & state ) ? TRUE : FALSE;
        }
    }

    return FALSE;
}

void DAUnitSetState( DADiskRef disk, DAUnitState state, Boolean value )
{
    CFNumberRef key;

    key = DADiskGetDescription( disk, kDADiskDescriptionMediaBSDUnitKey );

    if ( key )
    {
        CFMutableDataRef data;

        data = ( CFMutableDataRef ) CFDictionaryGetValue( gDAUnitList, key );

        if ( data )
        {
            __DAUnit * unit;

            unit = ( void * ) CFDataGetMutableBytePtr( data );

            unit->state &= ~state;
            unit->state |= value ? state : 0;
        }
        else
        {
            data = CFDataCreateMutable( kCFAllocatorDefault, sizeof( __DAUnit ) );

            if ( data )
            {
                __DAUnit * unit;

                unit = ( void * ) CFDataGetMutableBytePtr( data );

                unit->state = value ? state : 0;

                CFDictionarySetValue( gDAUnitList, key, data );

                CFRelease( data );
            }
        }
    }
}
