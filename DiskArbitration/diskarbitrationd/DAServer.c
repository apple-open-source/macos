/*
 * Copyright (c) 1998-2017 Apple Inc. All rights reserved.
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

#include "DAServer.h"
#include "DAServerServer.h"
#include "DABase.h"
#include "DACallback.h"
#include "DADialog.h"
#include "DADisk.h"
#include "DAFileSystem.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAMain.h"
#include "DAMount.h"
#include "DAPrivate.h"
#include "DAQueue.h"
#include "DASession.h"
#include "DAStage.h"
#include "DASupport.h"
#include "DAThread.h"

#include <paths.h>
#include <bsm/libbsm.h>
#include <sandbox/private.h>
#include <servers/bootstrap.h>
#include <sys/stat.h>
#include <IOKit/IOMessage.h>
#include <IOKit/storage/IOMedia.h>
#include <os/log.h>
#include <MediaKit/GPTTypes.h>
#include <IOKit/IOBSD.h>
///w:start
#include <dlfcn.h>
#if TARGET_OS_OSX
#include <IOKit/storage/CoreStorage/CoreStorageUserLib.h>
#endif
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>

#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH || TARGET_OS_BRIDGE
#include <Security/Security.h>
#include <Security/SecTask.h>
#endif

#if TARGET_OS_IOS
#include <MobileKeyBag/MobileKeyBag.h>
#include <CoreFoundation/CFNotificationCenter.h>
#endif

void ___os_transaction_begin( void );
void ___os_transaction_end( void );
void ___os_transaction_get( void );
///w:end

#if TARGET_OS_OSX
///w:start
// Dynamic load libCoreStorage.dylib
static CFMutableDictionaryRef (*__CoreStorageCopyVolumeProperties)( CoreStorageLogicalRef volRef ) = NULL;
static bool (*__CoreStorageLockFamily)( CoreStorageFamilyRef lvfRef ) = NULL;
static void *__hlibCoreStorage = NULL;

static void __CoreStorage_init() __attribute__((constructor));
static void __CoreStorage_exit() __attribute__((destructor));

void __CoreStorage_init()
{
    __hlibCoreStorage = dlopen("libCoreStorage.dylib", RTLD_LAZY);
    if ( __hlibCoreStorage )
    {
        *(void **)( &__CoreStorageCopyVolumeProperties ) = dlsym( __hlibCoreStorage, "CoreStorageCopyVolumeProperties" );
        *(void **)( &__CoreStorageLockFamily )           = dlsym( __hlibCoreStorage, "CoreStorageLockFamily" );
    }
}

void __CoreStorage_exit()
{
    if ( __hlibCoreStorage )
    {
        dlclose( __hlibCoreStorage );
    }
}
///w:end
#endif

static dispatch_mach_t     __gDAServerListener      = NULL;
static mach_port_t         __gDAServerPort  = MACH_PORT_NULL;
static mach_msg_header_t * __gDAServerReply = NULL;

static void __DAMediaBusyStateChangedCallback( void * context, io_service_t service, void * argument );
static void __DAMediaPropertyChangedCallback( void * context, io_service_t service, void * argument );

DADiskRef DADiskListGetDisk( const char * diskID )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDADiskList );

    for ( index = 0; index < count; index++ )
    {
        DADiskRef disk;

        disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

        if ( strcmp( DADiskGetID( disk ), diskID ) == 0 )
        {
            return disk;
        }
    }

    return NULL;
}

static DADiskRef __DADiskListGetDiskWithIOMedia( io_service_t media )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDADiskList );

    for ( index = 0; index < count; index++ )
    {
        DADiskRef disk;

        disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

        if ( IOObjectIsEqualTo( DADiskGetIOMedia( disk ), media ) )
        {
            return disk;
        }
    }

    return NULL;
}

static void DADiskSetContainer( DADiskRef disk )
{
    if ( disk )
    {
        io_service_t media;
        io_service_t parent = IO_OBJECT_NULL;
        media = DADiskGetIOMedia( disk );
        IORegistryEntryGetParentEntry( media, kIOServicePlane, &parent );
        media = parent;
        
        while ( media )
        {
            if ( IOObjectConformsTo( media, kIOMediaClass ) )
            {
                CFTypeRef content;

                content = IORegistryEntryCreateCFProperty( media, CFSTR( kIOBSDUnitKey ), CFGetAllocator( disk ), 0 );

                if ( content )
                {
                    UInt32 bsdUnit;
                    CFNumberGetValue( content, kCFNumberSInt32Type, &bsdUnit );
                    if ( bsdUnit != DADiskGetBSDUnit( disk ) )
                    {
                        CFTypeRef object = IORegistryEntryCreateCFProperty( media, CFSTR( kIOBSDNameKey ), CFGetAllocator( disk ), 0 );

                        io_name_t name;
                        char path[PATH_MAX];

                        CFStringGetCString( object, name, sizeof( name ), kCFStringEncodingUTF8 );
                        strlcpy( path, _PATH_DEV, sizeof( path ) );
                        strlcat( path, name,      sizeof( path ) );
                       
                        DADiskSetContainerId(disk, path);
                        
                        IOObjectRelease( media );
                        CFRelease( content );

                        return ;
                    }

                    CFRelease( content );
                }
            }

            IORegistryEntryGetParentEntry( media, kIOServicePlane, &parent );

            IOObjectRelease( media );

            media = parent;
            
            parent = IO_OBJECT_NULL;
        }
    }

}
static void __DAMediaBusyStateChangedCallback( void * context, io_service_t service, void * argument )
{
    DADiskRef disk;

    disk = __DADiskListGetDiskWithIOMedia( service );

    if ( disk )
    {
        if ( argument )
        {
            DADiskSetBusy( disk, CFAbsoluteTimeGetCurrent( ) );
        }
        else
        {
            _DAMediaAppearedCallback( NULL, gDAMediaAppearedNotification );

            DADiskSetBusy( disk, 0 );

            DAStageSignal( );
        }
    }
}

static void __DAMediaChangedCallback( void * context, io_service_t service, natural_t message, void * argument )
{
    switch ( message )
    {
        case kIOMessageServiceBusyStateChange:
        {
            __DAMediaBusyStateChangedCallback( context, service, argument );

            break;
        }
        case kIOMessageServicePropertyChange:
        {
            __DAMediaPropertyChangedCallback( context, service, argument );

            break;
        }
    }
}

static void __DAMediaPropertyChangedCallback( void * context, io_service_t service, void * argument )
{
    DADiskRef   disk;
    bool        volumeNameChanged = false;
    CFStringRef name;

    disk = __DADiskListGetDiskWithIOMedia( service );

    if ( disk )
    {
        CFMutableArrayRef keys;

        keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

        if ( keys )
        {
            CFMutableDictionaryRef properties = NULL;

            IORegistryEntryCreateCFProperties( service, &properties, CFGetAllocator( disk ), 0 );

            if ( properties )
            {
#if TARGET_OS_OSX
                if ( DADiskGetState( disk, kDADiskStateCommandActive ) == FALSE )
                {
                    CFURLRef path;

                    /*
                     * volume name can be changed asynchronously depending on the underlying filesystem implementation.
                     * if the name has changed, try to move the mountpoint
                     */
                    path = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );
                    struct statfs * mountList;
                    int             mountListCount;
                    int             mountListIndex;
                    
                    mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

                    for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
                    {
                        if ( strcmp( _DAVolumeGetID( mountList + mountListIndex ), DADiskGetID( disk ) ) == 0 )
                        {
                            break;
                        }
                    }

                    if ( path && (mountListIndex != mountListCount) )
                    {
                        uuid_t volUUID;
                        name = _DAFileSystemCopyNameAndUUID( DADiskGetFileSystem( disk ), path, &volUUID );
                        CFUUIDRef UUID = NULL;
                        UUID = CFUUIDCreateWithBytes(kCFAllocatorDefault, volUUID[0], volUUID[1], volUUID[2],
                                                     volUUID[3], volUUID[4], volUUID[5], volUUID[6],
                                                     volUUID[7], volUUID[8], volUUID[9], volUUID[10],
                                                     volUUID[11], volUUID[12], volUUID[13], volUUID[14], volUUID[15]);
                        if ( name && UUID )
                        {

                            if ( ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, UUID ) == kCFCompareEqualTo ) &&
                            ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeNameKey, name ) ) )
                            {
                                DALogInfo( "mounted volume name changed for %@ to name %@.", disk, name);
                                DADiskSetDescription( disk, kDADiskDescriptionVolumeNameKey, name );
                                CFArrayAppendValue( keys, kDADiskDescriptionVolumeNameKey );
                                volumeNameChanged = true;
                            }
                        }
                        if ( name ) CFRelease( name );
                        if ( UUID ) CFRelease( UUID );
                    }
///w:start
                    else if ( DAUnitGetState( disk, _kDAUnitStateHasAPFS ) )
                    {
                        io_name_t medianame;
                        
                        kern_return_t status = IORegistryEntryGetName( service, medianame );
                        if ( status == KERN_SUCCESS )
                        {
                            name = CFStringCreateWithCString( kCFAllocatorDefault, medianame, kCFStringEncodingUTF8 );
                            if ( name )
                            {
                                if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeNameKey, name ) )
                                {
                                    DALogInfo( "IOReg volume name changed for %@ to name %@.", disk, name);
                                    DADiskSetDescription( disk, kDADiskDescriptionVolumeNameKey, name );
                                    CFArrayAppendValue( keys, kDADiskDescriptionVolumeNameKey );
                                    DADiskSetDescription( disk, kDADiskDescriptionMediaNameKey, name );
                                    volumeNameChanged = true;
                                }
                                CFRelease( name );
                            }
                        }
                    }
///w:stop
                    if ( ( true == volumeNameChanged ) && path && (mountListIndex != mountListCount) )
                    {
                       
                        CFURLRef mountpoint;
                        if ( CFEqual( CFURLGetString( path ), CFSTR( "file:///" ) ) )
                        {
                            mountpoint = DAMountCreateMountPointWithAction( disk, kDAMountPointActionMove );

                            if ( mountpoint )
                            {
                                DADiskSetBypath( disk, mountpoint );

                                CFRelease( mountpoint );
                            }
                        }
                        else
                        {
                            mountpoint = DAMountCreateMountPointWithAction( disk, kDAMountPointActionMove );

                            if ( mountpoint )
                            {
                                DADiskSetBypath( disk, mountpoint );

                                DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, mountpoint );

                                CFArrayAppendValue( keys, kDADiskDescriptionVolumePathKey );

                                CFRelease( mountpoint );
                            }
                        }
                    }
                }
#endif

                CFTypeRef object;

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaContentKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaContentKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaContentKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaContentKey );
                }

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaEjectableKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaEjectableKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaEjectableKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaEjectableKey );
                }

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaLeafKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaLeafKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaLeafKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaLeafKey );
                }

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaPreferredBlockSizeKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaBlockSizeKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaBlockSizeKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaBlockSizeKey );
                }

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaRemovableKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaRemovableKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaRemovableKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaRemovableKey );
                }

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaSizeKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaSizeKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaSizeKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaSizeKey );
                }

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaUUIDKey ) );
                if ( object )
                {
                    object = ___CFUUIDCreateFromString( kCFAllocatorDefault, object );

                    if ( object )
                    {
                        if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaUUIDKey, object ) )
                        {
                            DADiskSetDescription( disk, kDADiskDescriptionMediaUUIDKey, object );

                            CFArrayAppendValue( keys, kDADiskDescriptionMediaUUIDKey );
                        }
                        CFRelease ( object );
                    }
                }
                
                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaWholeKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaWholeKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaWholeKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaWholeKey );
                }

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaWritableKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaWritableKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaWritableKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaWritableKey );
                }

                __DADiskEncryptionContext *context = malloc( sizeof( __DADiskEncryptionContext ) );
                if ( context )
                {
                    context->disk = disk;
                    CFRetain(context->disk);
                    DAThreadExecute( _DADiskGetEncryptionStatus, context, _DADiskEncryptionStatusCallback, context );
                }

                object = IORegistryEntrySearchCFProperty( service,
                                                          kIOServicePlane,
                                                          CFSTR( "AppleTDMLocked" ),
                                                          kCFAllocatorDefault,
                                                          kIORegistryIterateParents | kIORegistryIterateRecursively );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionDeviceTDMLockedKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionDeviceTDMLockedKey, object );
                    CFArrayAppendValue( keys, kDADiskDescriptionDeviceTDMLockedKey );
                }

                if ( object )
                {
                    CFRelease ( object );
                }

                if ( CFArrayGetCount( keys ) )
                {

                    DALogInfo( "updated disk, id = %@.", disk );

                    if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
                    {
                        DADiskDescriptionChangedCallback( disk, keys );
                    }
                }

                CFRelease( properties );
            }

            CFRelease( keys );
        }
    }
}

static DASessionRef __DASessionListGetSession( mach_port_t sessionID )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDASessionList );

    for ( index = 0; index < count; index++ )
    {
        DASessionRef session;

        session = ( void * ) CFArrayGetValueAtIndex( gDASessionList, index );

        if ( DASessionGetID( session ) == sessionID )
        {
            return session;
        }
    }

    return NULL;
}

#if TARGET_OS_IOS
static void __DAFirstUnlockNotificationCallback( CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo )
{
    DALogInfo( "First unlock notification received" );
    Boolean prevUnlockedState = gDAUnlockedState;
    gDAUnlockedState = TRUE;
    
    if ( prevUnlockedState == FALSE )
    {
        CFIndex count;
        CFIndex index;

        /*
         * Device is unlocked now
         */

        count = CFArrayGetCount( gDADiskList );

        for ( index = 0; index < count; index++ )
        {
            DADiskRef disk;

            disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

            /*
            * Mount this volume.
            */
            if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanTrue )
            {
                if ( DAMountGetPreference( disk, kDAMountPreferenceDisableAutoMount ) == false )
                {
                    if ( DAMountGetPreference( disk, kDAMountPreferenceDefer ) )
                    {
                        DADiskMountWithArguments( disk, NULL, kDADiskMountOptionDefault, NULL, CFSTR( "automatic" ) );
                    }
                }
            }
        }
     }
}


void DARegisterForFirstUnlockNotification( void )
{
    CFNotificationCenterAddObserver( CFNotificationCenterGetDarwinNotifyCenter(),
                                    (void *)nil,
                                    __DAFirstUnlockNotificationCallback,
                                    CFSTR("com.apple.mobile.keybagd.first_unlock"),
                                    NULL,
                                    CFNotificationSuspensionBehaviorDeliverImmediately );

    int lockState = MKBGetDeviceLockState( NULL );
    if ( ( lockState != kMobileKeyBagDisabled ) && ( MKBDeviceUnlockedSinceBoot( ) == false ) )
    {
        DALogInfo(" Device is locked" );
    }
    else
    {
        DALogInfo(" Device is unlocked" );
        gDAUnlockedState = TRUE;
    }
}

#endif

#if TARGET_OS_OSX
void _DAConfigurationCallback( SCDynamicStoreRef session, CFArrayRef keys, void * info )
{
    /*
     * A console user has logged in or logged out.
     */

    CFStringRef previousUser;
    gid_t       previousUserGID;
    uid_t       previousUserUID;
    CFArrayRef  previousUserList;
    CFStringRef user;
    gid_t       userGID;
    uid_t       userUID;
    CFArrayRef  userList;

    DALogInfo( "configd [0] -> %s", gDAProcessNameID );
    
    previousUser     = gDAConsoleUser;
    previousUserGID  = gDAConsoleUserGID;
    previousUserUID  = gDAConsoleUserUID;
    previousUserList = gDAConsoleUserList;

    user     = ___SCDynamicStoreCopyConsoleUser( session, &userUID, &userGID );
///w:start
    if ( user )
    {
        /*
         * Determine whether we need to wait for SystemUIServer.
         */

        if ( session ) /* not SystemUIServer */
        {
            CFIndex count;
            CFIndex index;

            count = 0;

            if ( previousUserList )
            {
                count = CFArrayGetCount( previousUserList );
            }

            for ( index = 0; index < count; index++ )
            {
                CFDictionaryRef dictionary;

                dictionary = ( void * ) CFArrayGetValueAtIndex( previousUserList, index );

                if ( dictionary )
                {
                    CFStringRef string;

                    string = CFDictionaryGetValue( dictionary, kSCConsoleSessionUserName );

                    if ( CFEqual( string, user ) )
                    {
                        break;
                    }
                }
            }

            if ( index == count ) /* not Fast User Switch */
            {
                userList = SCDynamicStoreCopyConsoleInformation( session );

                if ( userList ) /* not macOS Installer */
                {
                    CFRelease( user );
                    CFRelease( userList );

                    return; /* wait */
                }
            }
        }
    }
///w:stop
    userList = ___SCDynamicStoreCopyConsoleInformation( session );

    gDAConsoleUser     = user;
    gDAConsoleUserGID  = userGID;
    gDAConsoleUserUID  = userUID;
    gDAConsoleUserList = userList;

    if ( gDAConsoleUser )
    {
        /*
         * A console user has logged in.
         */

        DALogInfo( " console user = %@ [%d].", gDAConsoleUser, gDAConsoleUserUID );
    }
    else
    {
        CFIndex count;
        CFIndex index;

        /*
         * A console user has logged out.
         */

        DALogInfo( " console user = none." );

        count = 0;

        if ( gDAConsoleUserList )
        {
            count = CFArrayGetCount( gDAConsoleUserList );
        }

        for ( index = 0; index < count; index++ )
        {
            CFDictionaryRef dictionary;

            dictionary = CFArrayGetValueAtIndex( gDAConsoleUserList, index );

            if ( ___CFDictionaryGetIntegerValue( dictionary, kSCConsoleSessionUID ) == previousUserUID )
            {
                break;
            }
        }

        if ( index == count )
        {
            count = CFArrayGetCount( gDADiskList );

            for ( index = 0; index < count; index++ )
            {
                DADiskRef disk;

                disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

                /*
                 * Unmount this volume.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanTrue )
                {
                    Boolean unmount;

                    unmount = FALSE;

                    if ( DADiskGetUserUID( disk ) )
                    {
                        if ( DADiskGetUserUID( disk ) == previousUserUID )
                        {
                            unmount = TRUE;
                        }
                    }

                    if ( unmount )
                    {
                        DADiskUnmount( disk, kDADiskUnmountOptionForce, NULL );
                    }
                }
            }

            for ( index = 0; index < count; index++ )
            {
                DADiskRef disk;

                disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

                /*
                 * Eject this disk.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWholeKey ) == kCFBooleanTrue )
                {
                    Boolean eject;

                    eject = FALSE;

                    if ( DADiskGetUserUID( disk ) )
                    {
                        if ( DADiskGetUserUID( disk ) == previousUserUID )
                        {
                            eject = TRUE;
                        }
                    }

                    if ( eject )
                    {
                        DADiskEject( disk, kDADiskUnmountOptionForce, NULL );
                    }
                }
            }
        }
    }

    if ( gDAConsoleUserList )
    {
        CFIndex count;
        CFIndex index;

        /*
         * A console user is logged in.
         */
        DALogInfo( "console user is logged in" );
        if ( previousUserList == NULL )
        {
            DALogInfo( " console user change: start mounting disks.. " );
        }
       
        count = CFArrayGetCount( gDADiskList );

        for ( index = 0; index < count; index++ )
        {
            DADiskRef disk;

            disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );
///w:start
            /*
             * Set the BSD permissions for this media object.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaTypeKey ) )
            {
                mode_t deviceMode;
                uid_t  deviceUser;

                deviceMode = 0640;
                deviceUser = gDAConsoleUserUID;

                if ( CFArrayGetCount( gDAConsoleUserList ) > 1 )
                {
                    deviceMode = 0666;
                    deviceUser = ___UID_ROOT;
                }

                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
                {
                    deviceMode &= 0444;
                }

                chmod( DADiskGetBSDPath( disk, TRUE  ), deviceMode );
                chmod( DADiskGetBSDPath( disk, FALSE ), deviceMode );

                chown( DADiskGetBSDPath( disk, TRUE  ), deviceUser, -1 );
                chown( DADiskGetBSDPath( disk, FALSE ), deviceUser, -1 );
            }
///w:stop

            /*
             * Mount this volume.
             */

            if ( previousUserList == NULL )
            {
                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanTrue )
                {
                    if ( DAMountGetPreference( disk, kDAMountPreferenceDisableAutoMount ) == false )
                    {
                        if ( DAMountGetPreference( disk, kDAMountPreferenceDefer ) )
                        {
                            DALogInfo( " console user change: mounting deferred disk %@ ", disk);
                            DADiskMountWithArguments( disk, NULL, kDADiskMountOptionDefault, NULL, CFSTR( "automatic" ) );
                        }
                    }
                }
            }
        }
    }
    else
    {
        CFIndex count;
        CFIndex index;

        /*
         * A console user is not logged in.
         */
        
///w:start
        const char * suInstallCookieFile = "/var/db/.SoftwareUpdateAtLogout";
        struct statfs fs     = { 0 };
        int status = statfs( suInstallCookieFile, &fs );
        if (status == 0 )
        {
            DALogInfo( " SU install cookie is present. Ignoring console user logout." );
            goto done;
        }
        else
        {
            DALogInfo( " No console users logged in." );
        }
///w:stop

        DAPreferenceListRefresh( );

        count = CFArrayGetCount( gDADiskList );

        for ( index = 0; index < count; index++ )
        {
            DADiskRef disk;

            disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );
///w:start
            /*
             * Set the BSD permissions for this media object.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaTypeKey ) )
            {
                mode_t deviceMode;
                uid_t  deviceUser;

                deviceMode = 0640;
                deviceUser = ___UID_ROOT;

                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
                {
                    deviceMode &= 0444;
                }

                chmod( DADiskGetBSDPath( disk, TRUE  ), deviceMode );
                chmod( DADiskGetBSDPath( disk, FALSE ), deviceMode );

                chown( DADiskGetBSDPath( disk, TRUE  ), deviceUser, -1 );
                chown( DADiskGetBSDPath( disk, FALSE ), deviceUser, -1 );
            }
///w:stop

            /*
             * Unmount this volume.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanTrue )
            {
                Boolean unmount;
///w:start
                CFStringRef lvfUUID = NULL;
///w:stop
                unmount = FALSE;

                if ( DAMountGetPreference( disk, kDAMountPreferenceDefer ) )
                {
                    if ( DADiskGetState( disk, _kDADiskStateMountAutomaticNoDefer ) == FALSE )
                    {

///w:start
                        CFMutableDictionaryRef lvProps = NULL;
                        CFStringRef lvUUID = NULL;
                        CFBooleanRef encrypted = DADiskGetDescription( disk, kDADiskDescriptionMediaEncryptedKey );
                        CFTypeRef object = DADiskGetDescription( disk, kDADiskDescriptionMediaUUIDKey );

                        if ( ( object ) && ( encrypted == kCFBooleanTrue ) )
                        {
                            lvUUID = CFUUIDCreateString( kCFAllocatorDefault , object );
                        }

                        if ( lvUUID != NULL )
                        {
                            if ( __CoreStorageCopyVolumeProperties != NULL )
                            {
                                lvProps = __CoreStorageCopyVolumeProperties( (CoreStorageLogicalRef)lvUUID );
                            }
                            CFRelease( lvUUID );
                        }

                        if (lvProps )
                        {
                            lvfUUID = CFDictionaryGetValue( lvProps, CFSTR( kCoreStorageLogicalFamilyUUIDKey ) );
                            if ( lvfUUID )
                            {
                                CFRetain( lvfUUID );
                            }
                            CFRelease( lvProps );

                        }
///w:stop
                        unmount = TRUE;
                    }
                }

                if ( unmount )
                {
                    DALogInfo( "console user is not logged in. unmounting disk %@", disk );
                    DADiskUnmount( disk, kDADiskUnmountOptionDefault, NULL );

///w:start
                    if ( lvfUUID )
                    {
                        if ( __CoreStorageLockFamily != NULL )
                        {
                            __CoreStorageLockFamily( (CoreStorageFamilyRef)lvfUUID );
                        }
                        CFRelease( lvfUUID );
                    }
///w:stop

                }
            }
        }
    }

done:
    if ( previousUser )
    {
        CFRelease( previousUser );
    }

    if ( previousUserList )
    {
        CFRelease( previousUserList );
    }

    DAStageSignal( );
}

#endif
void _DAMediaAppearedCallback( void * context, io_iterator_t notification )
{
    /*
     * Process the appearance of media objects in I/O Kit.
     */

    io_service_t media;

    /*
     * Iterate through the media objects.
     */

    while ( ( media = IOIteratorNext( notification ) ) )
    {
        DADiskRef disk;

        /*
         * Determine whether this is a re-registration.
         */

        disk = __DADiskListGetDiskWithIOMedia( media );

        if ( disk )
        {
            __DAMediaPropertyChangedCallback( NULL, media, NULL );
///w:start
            if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanTrue )
            {
                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaLeafKey ) == kCFBooleanFalse )
                {
                    DADiskProbe( disk, NULL );
                }
            }
///w:stop
        }
        else
        {
            io_object_t busyNotification;
            io_object_t propertyNotification;

            /*
             * Create the "media changed" notification.
             */

            busyNotification = IO_OBJECT_NULL;

            IOServiceAddInterestNotification( gDAMediaPort, media, kIOBusyInterest, __DAMediaChangedCallback, NULL, &busyNotification );

            propertyNotification = IO_OBJECT_NULL;

            IOServiceAddInterestNotification( gDAMediaPort, media, kIOGeneralInterest, __DAMediaChangedCallback, NULL, &propertyNotification );

            /*
             * Create a disk object for this media object.
             */


            disk = DADiskCreateFromIOMedia( kCFAllocatorDefault, media );

            if ( disk )
            {
                /*
                 * Determine whether a media object disappearance and appearance occurred.  We must do this
                 * since the I/O Kit appearance queue is separate from the I/O Kit disappearance queue, and
                 * we are in the midst of processing the appearance queue when we see a duplicate, which is
                 * to say, there is a disappearance on the queue we have not processed yet and must process
                 * it first.  The appearances and disappearances within each queue do occur in proper order.
                 */

                if ( ___CFArrayContainsValue( gDADiskList, disk ) )
                {
                    /*
                     * Process the disappearance.
                     */

                    _DAMediaDisappearedCallback( ( void * ) ___CFArrayGetValue( gDADiskList, disk ), IO_OBJECT_NULL );

                    assert( ___CFArrayContainsValue( gDADiskList, disk ) == FALSE );
                }

                /*
                 * Set the "media changed" notification.
                 */

                if ( busyNotification )
                {
                    DADiskSetBusyNotification( disk, busyNotification );
                }

                if ( propertyNotification )
                {
                    DADiskSetPropertyNotification( disk, propertyNotification );
                }

#if TARGET_OS_OSX
                /*
                 * Set the BSD permissions for this media object.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaTypeKey ) )
                {
                    if ( DADiskGetMode( disk ) )
                    {
                        chmod( DADiskGetBSDPath( disk, TRUE  ), DADiskGetMode( disk ) & 0666 );
                        chmod( DADiskGetBSDPath( disk, FALSE ), DADiskGetMode( disk ) & 0666 );
                    }

                    if ( gDAConsoleUserList )
                    {
///w:start
                        mode_t deviceMode;
                        uid_t  deviceUser;

                        deviceMode = 0640;
                        deviceUser = gDAConsoleUserUID;

                        if ( CFArrayGetCount( gDAConsoleUserList ) > 1 )
                        {
                            deviceMode = 0666;
                            deviceUser = ___UID_ROOT;
                        }

                        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
                        {
                            deviceMode &= 0444;
                        }

                        chmod( DADiskGetBSDPath( disk, TRUE  ), deviceMode );
                        chmod( DADiskGetBSDPath( disk, FALSE ), deviceMode );

                        chown( DADiskGetBSDPath( disk, TRUE  ), deviceUser, -1 );
                        chown( DADiskGetBSDPath( disk, FALSE ), deviceUser, -1 );
///w:stop
                    }
                }
                else
                {
                    if ( DADiskGetMode( disk ) )
                    {
                        chmod( DADiskGetBSDPath( disk, TRUE  ), DADiskGetMode( disk ) & 0666 );
                        chmod( DADiskGetBSDPath( disk, FALSE ), DADiskGetMode( disk ) & 0666 );
                    }

                    if ( DADiskGetUserGID( disk ) )
                    {
                        chown( DADiskGetBSDPath( disk, TRUE  ), -1, DADiskGetUserGID( disk ) );
                        chown( DADiskGetBSDPath( disk, FALSE ), -1, DADiskGetUserGID( disk ) );
                    }

                    if ( DADiskGetUserUID( disk ) )
                    {
                        chown( DADiskGetBSDPath( disk, TRUE  ), DADiskGetUserUID( disk ), -1 );
                        chown( DADiskGetBSDPath( disk, FALSE ), DADiskGetUserUID( disk ), -1 );
                    }
                }
#endif
                /*
                 * Set the BSD link for this media object.
                 */

                if ( DADiskGetBSDLink( disk, TRUE ) )
                {
                    int status;

                    status = strncmp( DADiskGetBSDLink( disk, TRUE ), _PATH_DEV "disk", strlen( _PATH_DEV "disk" ) );

                    if ( status )
                    {
                        status = link( DADiskGetBSDPath( disk, TRUE ), DADiskGetBSDLink( disk, TRUE ) );

                        if ( status == 0 )
                        {
                            status = link( DADiskGetBSDPath( disk, FALSE ), DADiskGetBSDLink( disk, FALSE ) );

                            if ( status )
                            {
                                unlink( DADiskGetBSDLink( disk, TRUE ) );
                            }
                        }
                    }

                    if ( status )
                    {

                        DALogError( "unable to link %@ to %s.", disk, DADiskGetBSDLink( disk, TRUE ) );

                        DADiskSetBSDLink( disk, TRUE,  NULL );
                        DADiskSetBSDLink( disk, FALSE, NULL );
                    }
                }

                /*
                 * Skip the "mount" stage if the unit has quiesced.
                 */

///w:23678897:start
                CFStringRef content;

                content = DADiskGetDescription( disk, kDADiskDescriptionMediaContentKey );

                if ( CFEqual( content, CFSTR( "41504653-0000-11AA-AA11-00306543ECAC" ) ) )
                {
                    DAUnitSetState( disk, _kDAUnitStateHasAPFS, TRUE );
#if TARGET_OS_IOS
                    DADiskSetContainer( disk );
#endif
                }
                
                if ( DAUnitGetState( disk, _kDAUnitStateHasAPFS ) )
                {
///w:23678897:stop
                if ( DAUnitGetState( disk, kDAUnitStateHasQuiescedNoTimeout ) )
                {
                    DADiskSetState( disk, kDADiskStateStagedMount, TRUE );
                }
///w:23678897:start
                }
///w:23678897:stop

                /*
                 * Add the disk object to our tables.
                 */


                DALogInfo( "created disk, id = %@.", disk );

                DAUnitSetState( disk, kDAUnitStateStagedUnreadable, FALSE );

                CFArrayInsertValueAtIndex( gDADiskList, 0, disk );
                
                {
                    __DADiskEncryptionContext *context = malloc( sizeof( __DADiskEncryptionContext ) );
                    if ( context )
                    {
                        context->disk = disk;
                        CFRetain(context->disk);
                        DAThreadExecute( _DADiskGetEncryptionStatus, context, _DADiskEncryptionStatusCallback, context );
                    }
                }
                
#if !TARGET_OS_OSX
                if ( ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanFalse)  ||
                    ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue ) )
                {
                    ___os_transaction_begin();
                }
#endif

                CFRelease( disk );
            }

            if ( busyNotification )
            {
                IOObjectRelease( busyNotification );
            }

            if ( propertyNotification )
            {
                IOObjectRelease( propertyNotification );
            }
        }

        IOObjectRelease( media );
    }

    DAStageSignal( );
}

void _DAMediaDisappearedCallback( void * context, io_iterator_t notification )
{
    /*
     * Process the disappearance of media objects in I/O Kit.
     */

    io_service_t media;
    SInt32 prevDeviceUnit = -1;
    CFMutableArrayRef diskInfoArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    /*
     * Iterate through the media objects.
     */

    if ( context )
    {
        media = DADiskGetIOMedia( context );
    }
    else
    {
        media = IOIteratorNext( notification );
    }

    for ( ; media ; media = IOIteratorNext( notification ) )
    {
        DADiskRef disk;

        /*
         * Obtain the disk object for this media object.
         */

        disk = __DADiskListGetDiskWithIOMedia( media );

        /*
         * Determine whether a media object appearance and disappearance occurred.  We must do this
         * since the I/O Kit appearance queue is separate from the I/O Kit disappearance queue, and
         * we are in the midst of processing the disappearance queue when we see one missing, which
         * is to say, there is an appearance on the queue we haven't processed yet and must process
         * it first.  The appearances and disappearances within each queue do occur in proper order.
         */

        if ( disk == NULL )
        {
            /*
             * Process the appearance.
             */

            assert( context == NULL );

            _DAMediaAppearedCallback( NULL, gDAMediaAppearedNotification );

            disk = __DADiskListGetDiskWithIOMedia( media );
        }

        if ( disk )
        {
            /*
             * Remove the disk object from our tables.
             */

            DALogInfo( "removed disk, id = %@.", disk );

            if ( DADiskGetBSDLink( disk, TRUE ) )
            {
                unlink( DADiskGetBSDLink( disk, TRUE ) );
            }

            if ( DADiskGetBSDLink( disk, FALSE ) )
            {
                unlink( DADiskGetBSDLink( disk, FALSE ) );
            }

            DAQueueReleaseDisk( disk );

            if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
            {
                DADiskDisappearedCallback( disk );
            }
            
            if ( DADiskGetState( disk, kDADiskStateStagedMount ) )
            {
                DADiskSetState( disk, kDADiskStateStagedAppear, TRUE );

                DADiskUnmount( disk, kDADiskUnmountOptionForce, NULL );

                /*
                 Determine whether the disk is mountable.
                 */
                Boolean  dialog = TRUE;

                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanFalse )
                {
                    dialog = FALSE;
                }

                /*
                 * Determine whether the disk is mounted.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) == NULL )
                {
                    dialog = FALSE;
                }

                 if ( ( dialog == TRUE ) && DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanTrue )
                {
                    CFURLRef mountpoint;
                    char *   path;

                    mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

                    path = ___CFURLCopyFileSystemRepresentation( mountpoint );

                    if ( path )
                    {
                        struct statfs fs;
                        int           status;

                        status = ___statfs( path, &fs, MNT_NOWAIT );

                        if ( status == 0 )
                        {
                            if ( ( fs.f_flags & MNT_RDONLY ) )
                            {
                                dialog = FALSE;
                            }
                        }

                        free( path );
                    }

                    if ( dialog )
                    {
                        CFDataRef serialization;

                        serialization = DADiskGetSerialization( disk );

                        if ((prevDeviceUnit == -1) || prevDeviceUnit == DADiskGetBSDUnit( disk ))
                        {
                            CFArrayAppendValue(diskInfoArray, serialization);
                            prevDeviceUnit = DADiskGetBSDUnit( disk );
                        }
                        else
                        {
                            DADialogShowDeviceRemoval(  diskInfoArray );
                            CFArrayAppendValue(diskInfoArray, serialization);
                            prevDeviceUnit = DADiskGetBSDUnit( disk );
                        }
                    }
                }
            }

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWholeKey ) == kCFBooleanTrue )
            {
///w:23678897:start
                DAUnitSetState( disk, _kDAUnitStateHasAPFS, FALSE );
///w:23678897:stop
                DAUnitSetState( disk, kDAUnitStateHasQuiesced,          FALSE );
                DAUnitSetState( disk, kDAUnitStateHasQuiescedNoTimeout, FALSE );
            }

            DADiskSetState( disk, kDADiskStateZombie, TRUE );
            
#if !TARGET_OS_OSX
            if ( ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanFalse)  ||
                ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue ) )
            {
                ___os_transaction_end();
                __DASetIdleTimer();
            }
#endif

            ___CFArrayRemoveValue( gDADiskList, disk );
        }

        if ( context )
        {
            break;
        }

        IOObjectRelease( media );
    }
    if ( 0 != CFArrayGetCount(  diskInfoArray) )
    {
        DADialogShowDeviceRemoval(  diskInfoArray);
    }

    CFRelease( diskInfoArray );
    DAStageSignal( );
}

void DAServerMachHandler( void *context, dispatch_mach_reason_t reason, dispatch_mach_msg_t msg, mach_error_t error )
{
    static const struct mig_subsystem * const subsystems[] = {
                    (mig_subsystem_t)&DAServer_subsystem,
            };
  
    mach_port_t serverPort = context;
    mach_msg_header_t *hdr;

    switch (reason) {
            case DISPATCH_MACH_MESSAGE_RECEIVED:
                hdr = dispatch_mach_msg_get_msg(msg, NULL);
            
                if ( !dispatch_mach_mig_demux( NULL, subsystems, 1, msg ))
                {
                    if (MACH_NOTIFY_NO_SENDERS == hdr->msgh_id)
                    {
                        _DAServerSessionCancel( serverPort );
                    }
                    mach_msg_destroy(dispatch_mach_msg_get_msg( msg, NULL ));
                }
            break;
            
            case DISPATCH_MACH_CANCELED:
                _DAServerSessionRelease( serverPort );
            break;

            default:
            break;
    }
}

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
kern_return_t _DAServermkdir( mach_port_t _session, ___path_t _path, audit_token_t _token )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            /*
             * Get the path prefix to be used in realpath
             */
            char dirPath[MAXPATHLEN];
            char resolvedPath[MAXPATHLEN];
            char dirName[MAXPATHLEN];
            size_t len = strlen(_path);
             
            if ( len >= MAXPATHLEN )
            {
                goto exit;
            }
            char *newPath = strrchr( _path , '/' );
            if ( newPath )
            {
                len =  newPath  - _path;
            }
            else
            {
                goto exit;
            }
            strlcpy( dirPath, _path, len + 1 );
            strlcpy( dirName, _path + len + 1, MAXPATHLEN);

            /*
             * Get the real path to compare with kDAMainMountPointFolder
             */
            if (NULL == realpath( dirPath, resolvedPath ))
            {
                goto exit;
            }

            /*
            * Determine whether the mount point path is within the mount point folder.
            */
            if ( strncmp( resolvedPath, kDAMainMountPointFolder, strlen( kDAMainMountPointFolder ) ) == 0 )
            {
                if ( strlen( resolvedPath ) == strlen( kDAMainMountPointFolder ) )
                {
                    /*
                     * Create the mount point.
                     */
                    if ( strlcat( resolvedPath, "/", MAXPATHLEN ) >= MAXPATHLEN )
                    {
                        goto exit;
                    }
                    if ( strlcat( resolvedPath, dirName, MAXPATHLEN ) >= MAXPATHLEN )
                    {
                        goto exit;
                    }
                    status = mkdir( resolvedPath, 0111 );

                    if ( status == 0 )
                    {
                        lchown( resolvedPath, audit_token_to_euid( _token ), -1 );
                    }
                    else
                    {
                        status = unix_err( errno );
                    }
                }
            }

        }
    }
exit:
    return status;
}

kern_return_t _DAServerrmdir( mach_port_t _session, ___path_t _path, audit_token_t _token )
{
    kern_return_t status;
    struct stat path_info = {0};

    status = kDAReturnBadArgument;

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {

            /*
             * Get the path prefix to be used in realpath
             */
            char dirPath[MAXPATHLEN];
            char resolvedPath[MAXPATHLEN];
            char dirName[MAXPATHLEN];
            size_t len = strlen(_path);
            
            if ( len >= MAXPATHLEN )
            {
                goto exit;
            }
            char *newPath = strrchr( _path , '/' );
            if ( newPath )
            {
                len =  newPath  - _path;
            }
            else
            {
                goto exit;
            }
            strlcpy( dirPath, _path, len + 1 );
            strlcpy( dirName, _path + len + 1, MAXPATHLEN);
            
            /*
             * Get the real path to compare with kDAMainMountPointFolder
             */
            if (NULL == realpath( dirPath, resolvedPath ))
            {
                goto exit;
            }

            /*
             * Determine whether the mount point path is within the mount point folder.
             */
            if ( strncmp( resolvedPath, kDAMainMountPointFolder, strlen( kDAMainMountPointFolder ) ) == 0 )
            {
                if (  strlen( resolvedPath ) ==  strlen( kDAMainMountPointFolder ) )
                {
                    /*
                     * Remove the mount point.
                     */
                    if ( strlcat( resolvedPath, "/", MAXPATHLEN ) >= MAXPATHLEN )
                    {
                        goto exit;
                    }
                    if ( strlcat( resolvedPath, dirName, MAXPATHLEN ) >= MAXPATHLEN )
                    {
                        goto exit;
                    }
                    status = stat( resolvedPath, &path_info);

                    if ( status != 0 )
                    {
                        status = unix_err( errno );
                    } else {
                        if ( ( audit_token_to_euid( _token ) == 0 || audit_token_to_euid( _token ) == path_info.st_uid ) &&
                             S_ISDIR( path_info.st_mode )  )// only allow root or owner to delete directory
                        {
                            status = rmdir( resolvedPath );

                            if ( status != 0 )
                            {
                                status = unix_err( errno );
                            }
                        } else {
                            status = kDAReturnNotPrivileged;
                        }
                    }
                }
            }
        }
    }

exit:
    return status;
}
#endif

kern_return_t _DAServerDiskCopyDescription( mach_port_t _session, caddr_t _disk, vm_address_t * _description, mach_msg_type_number_t * _descriptionSize )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            disk = DADiskListGetDisk( _disk );

            if ( disk )
            {
                CFDataRef description;

                description = DADiskGetSerialization( disk );

                if ( description )
                {
                    *_description = ___CFDataCopyBytes( description, _descriptionSize );

                    if ( *_description )
                    {
                        DALogDebug( "  copied disk description, id = %@.", disk );

                        status = kDAReturnSuccess;
                    }
                }
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to copy disk description, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerDiskGetOptions( mach_port_t _session, caddr_t _disk, int32_t * _options )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            disk = DADiskListGetDisk( _disk );

            if ( disk )
            {
                *_options = DADiskGetOptions( disk );

                DALogDebug( "  got disk options, id = %@, options = 0x%08X.", disk, *_options );

                status = kDAReturnSuccess;
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to get disk options, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerDiskGetUserUID( mach_port_t _session, caddr_t _disk, uid_t * _userUID )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            disk = DADiskListGetDisk( _disk );

            if ( disk )
            {
                *_userUID = DADiskGetUserUID( disk );

                status = kDAReturnSuccess;
            }
        }
    }

    return status;
}

kern_return_t _DAServerDiskIsClaimed( mach_port_t _session, caddr_t _disk, boolean_t * _claimed )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            disk = DADiskListGetDisk( _disk );

            if ( disk )
            {
                *_claimed = DADiskGetClaim( disk ) ? TRUE : FALSE;

                DALogDebug( "  got disk claim state, id = %@, claimed = %s.", disk, *_claimed ? "true" : "false" );

                status = kDAReturnSuccess;
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to get disk claim state, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerDiskSetAdoption( mach_port_t _session, caddr_t _disk, boolean_t _adoption, audit_token_t _token )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            disk = DADiskListGetDisk( _disk );

            if ( disk )
            {
                status = DAAuthorize( session, _kDAAuthorizeOptionDefault, disk, audit_token_to_euid( _token ), audit_token_to_egid( _token ), _kDAAuthorizeRightAdopt );

                if ( status == kDAReturnSuccess )
                {
                    DALogDebug( "  set disk adoption, id = %@, adoption = %s.", disk, _adoption ? "true" : "false" );

                    status = _DADiskSetAdoption( disk, _adoption );
                }
            }
        }
    }

    if ( status )
    {
         DALogDebug( "unable to set disk adoption, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerDiskSetEncoding( mach_port_t _session, caddr_t _disk, int32_t encoding, audit_token_t _token )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            disk = DADiskListGetDisk( _disk );

            if ( disk )
            {
                status = DAAuthorize( session, _kDAAuthorizeOptionIsOwner, disk, audit_token_to_euid( _token ), audit_token_to_egid( _token ), _kDAAuthorizeRightEncode );

                if ( status == kDAReturnSuccess )
                {
                    DALogDebug( "  set disk encoding, id = %@, encoding = %d.", disk, encoding );

                    status = _DADiskSetEncoding( disk, encoding );
                }
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to set disk encoding, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerDiskSetOptions( mach_port_t _session, caddr_t _disk, int32_t _options, int32_t _value )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            disk = DADiskListGetDisk( _disk );

            if ( disk )
            {
                DALogDebug( "  set disk options, id = %@, options = 0x%08X, value = %s.", disk, _options, _value ? "true" : "false" );

                DADiskSetOptions( disk, _options, _value );

                status = kDAReturnSuccess;
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to set disk options, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerDiskUnclaim( mach_port_t _session, caddr_t _disk )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            disk = DADiskListGetDisk( _disk );

            if ( disk )
            {
                DACallbackRef callback;

                callback = DADiskGetClaim( disk );

                if ( callback )
                {
                    if ( DACallbackGetSession( callback ) == session )
                    {
                        DALogDebug( "  unclaimed disk, id = %@.", disk );

                        DADiskSetClaim( disk, NULL );
                        
                        status = kDAReturnSuccess;
                    }
                }
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to unclaim disk, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerSessionCopyCallbackQueue( mach_port_t _session, vm_address_t * _queue, mach_msg_type_number_t * _queueSize )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            CFMutableArrayRef callbacks;

            DALogDebugHeader( "%s -> %@", gDAProcessNameID, session );

            callbacks = DASessionGetCallbackQueue( session );

            if ( callbacks )
            {
                CFIndex   count;
                CFIndex   index;
                CFDataRef queue;

                count = CFArrayGetCount( callbacks );

                for ( index = 0; index < count; index++ )
                {
                    DACallbackRef callback;

                    callback = ( void * ) CFArrayGetValueAtIndex( callbacks, index );

                    DACallbackSetDisk( callback, NULL );

                    DACallbackSetMatch( callback, NULL );

                    DACallbackSetSession( callback, NULL );
                }

                queue = _DASerialize( kCFAllocatorDefault, callbacks );

                if ( queue )
                {
                    *_queue = ___CFDataCopyBytes( queue, _queueSize );

                    if ( *_queue )
                    {
                        DALogDebug( "  dispatched callback queue." );

                        status = kDAReturnSuccess;
                    }

                    CFRelease( queue );
                }

                CFArrayRemoveAllValues( callbacks );
            }

            DASessionSetState( session, kDASessionStateTimeout, FALSE );
        }
    }

    if ( status )
    {
        DALogDebug( "unable to copy callback queue (status code 0x%08X).", status );
    }

    return status;
}

#define ENTITLEMENT_TARGETS   TARGET_FSKIT

#if ENTITLEMENT_TARGETS
bool _DAServerCheckEntitlement( audit_token_t _token,
                                CFStringRef entitlement_name )
{
    bool    rv = false;

    SecTaskRef secTask = NULL;
    CFTypeRef val = NULL;

    secTask = SecTaskCreateWithAuditToken( kCFAllocatorDefault, _token );
    if (secTask)
    {
        val = SecTaskCopyValueForEntitlement( secTask, entitlement_name, NULL );
        if ( val )
        {
            rv = CFEqual( val, kCFBooleanTrue );
            CFRelease ( val );
        }
        CFRelease( secTask);
    }

    return rv;
}
#endif /* ENTITLEMENT_TARGETS */

kern_return_t _DAServerSessionCreate( mach_port_t   _session,
                                      caddr_t       _name,
                                      audit_token_t _token,
                                      mach_port_t * _server )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "%s [%d] -> %s", _name, audit_token_to_pid( _token ), gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;
#ifdef DA_FSKIT
        bool is_fskitd;
#endif /* DA_FSKIT */
      

#ifdef DA_FSKIT
        is_fskitd = _DAServerCheckEntitlement(_token, CFSTR("com.apple.private.diskarbitrationd.is_fskitd"));
#endif /* DA_FSKIT */

        /*
         * Create the session.
         */

        session = DASessionCreate( kCFAllocatorDefault, _name, audit_token_to_pid( _token ) );

        if ( session )
        {
            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            DALogDebug( "  created session, id = %@.", session );

            *_server = DASessionGetServerPort( session );

            /*
             * Add the session object to our tables.
             */

            ___os_transaction_begin( );

            CFArrayAppendValue( gDASessionList, session );

            /*
             * Add the session to our run loop.
             */

            DASessionScheduleWithDispatch( session ); 

#ifdef DA_FSKIT
            DASessionSetIsFSKitd( session, is_fskitd );
#endif

            CFRelease( session );

            status = kDAReturnSuccess;
        }
    }

exit:
    if ( status )
    {
        DALogDebug( "unable to create session, id = %s [%d].", _name, audit_token_to_pid( _token ) );
    }

    return status;
}

kern_return_t _DAServerSessionQueueRequest( mach_port_t            _session,
                                            uint32_t                _kind,
                                            caddr_t                _argument0,
                                            int32_t                _argument1,
                                            vm_address_t           _argument2,
                                            mach_msg_type_number_t _argument2Size,
                                            vm_address_t           _argument3,
                                            mach_msg_type_number_t _argument3Size,
                                            mach_vm_offset_t       _address,
                                            mach_vm_offset_t       _context,
                                            audit_token_t          _token )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DADiskRef disk;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            disk = DADiskListGetDisk( _argument0 );

            if ( disk )
            {
                CFTypeRef     argument2 = NULL;
                CFTypeRef     argument3 = NULL;
                DACallbackRef callback;
                DARequestRef  request;

                DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

                if ( _argument2 )
                {
                    argument2 = _DAUnserializeWithBytes( kCFAllocatorDefault, _argument2, _argument2Size );
                }

                if ( _argument3 )
                {
                    argument3 = _DAUnserializeWithBytes( kCFAllocatorDefault, _argument3, _argument3Size );
                }

                callback = DACallbackCreate( kCFAllocatorDefault, session, _address, _context, _kind, 0, NULL, NULL );
                
                request = DARequestCreate( kCFAllocatorDefault, _kind, disk, _argument1, argument2, argument3, audit_token_to_euid( _token ), audit_token_to_egid( _token ), callback );

                if ( request )
                {
                    switch ( _kind )
                    {
                        case _kDADiskEject:
                        {
                            status = DAAuthorize( session, _kDAAuthorizeOptionIsOwner, disk, audit_token_to_euid( _token ), audit_token_to_egid( _token ),  _kDAAuthorizeRightUnmount );

                            break;
                        }
                        case _kDADiskMount:
                        {
                            status = DAAuthorize( session, _kDAAuthorizeOptionIsOwner, disk, audit_token_to_euid( _token ), audit_token_to_egid( _token ), _kDAAuthorizeRightMount );

                            if ( status == 0 )
                            {
                                CFStringRef content;

                                content = DADiskGetDescription( disk, kDADiskDescriptionMediaContentKey );

                                if ( CFEqual( content, CFSTR( "C12A7328-F81F-11D2-BA4B-00A0C93EC93B" ) ) )
                                {
                                    if ( audit_token_to_euid( _token ) )
                                    {
                                        if ( audit_token_to_euid( _token ) != DADiskGetUserUID( disk ) )
                                        {
                                            status = kDAReturnNotPermitted;
                                        }
                                    }
                                }
                            }

                            if ( status == 0 )
                            {
                                CFTypeRef mountpoint;

                                mountpoint = argument2;

                                if ( mountpoint )
                                {
                                    mountpoint = CFURLCreateWithString( kCFAllocatorDefault, mountpoint, NULL );
                                }

                                if ( mountpoint )
                                {
                                    char * mntpath;
                                    struct stat path_info = {0};

                                    
                                    mntpath = ___CFURLCopyFileSystemRepresentation( mountpoint );
                                    
                                    char path[MAXPATHLEN];
                                    if ( mntpath )
                                    {
                                        if ( ( _argument1 & kDADiskMountOptionNoFollow ) == 0 )
                                        {
                                            if ( realpath( mntpath, path ) )
                                            {

                                                CFTypeRef mountpath = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                                                if ( mountpath )
                                                {
                                                    DARequestSetArgument2( request, CFURLGetString( mountpath ) );
                                                    CFRelease ( mountpath );
                                                }
                                            }
                                            else
                                            {
                                                status = kDAReturnBadArgument;
                                            }
                                        }
                                        else
                                        {
                                            strlcpy( path, mntpath, MAXPATHLEN);
                                        }
                      
                                    }
                                    else
                                    {
                                        status = kDAReturnBadArgument;
                                    }

                                    if ( status == 0 )
                                    {
#if TARGET_OS_OSX
                                        status = sandbox_check_by_audit_token(_token, "file-mount", SANDBOX_FILTER_PATH | SANDBOX_CHECK_ALLOW_APPROVAL | SANDBOX_CHECK_CANONICAL, path);
                                        if ( status )
                                        {
                                            status = kDAReturnNotPrivileged;
                                        }
#endif
                                        if ( status == 0 && audit_token_to_euid( _token ) )
                                        {
                                            int ret = fstatat( AT_FDCWD , path, &path_info , AT_SYMLINK_NOFOLLOW_ANY );
                                            if ( ret != 0 )
                                            {
                                                status = unix_err( errno );
                                            } else if ( audit_token_to_euid( _token ) != path_info.st_uid  )
                                            {
                                                status = kDAReturnNotPrivileged;
                                            }
                                        }
                                        
                                        free( mntpath );
                                        
                                    }
                                    if ( audit_token_to_euid( _token ) )
                                    {
                                        if ( audit_token_to_euid( _token ) != DADiskGetUserUID( disk ) )
                                        {
                                            status = kDAReturnNotPrivileged;
                                        }
                                    }

                                    CFRelease( mountpoint );
                                }
                                
#if TARGET_OS_OSX
                                if ( argument3 )
                                {
                                    if ( DAMountContainsArgument( argument3, kDAFileSystemMountArgumentNoOwnership ) == TRUE &&  DAMountGetPreference( disk, kDAMountPreferenceTrust ) == TRUE )
                                    {
                                        if ( audit_token_to_euid( _token ) )
                                        {
                                            status = kDAReturnNotPrivileged;
                                        }
                                    }
                                    if ( ( ( DAMountContainsArgument( argument3, kDAFileSystemMountArgumentSetUserID ) == TRUE ) ||
                                           ( DAMountContainsArgument( argument3, kDAFileSystemMountArgumentDevice ) == TRUE ) )
                                            &&  DAMountGetPreference( disk, kDAMountPreferenceTrust ) == FALSE )
                                    {
                                        if ( audit_token_to_euid( _token ) )
                                        {
                                            status = kDAReturnNotPrivileged;
                                        }
                                    }
                                }
#endif
                            }

                            break;
                        }
                        case _kDADiskProbe:
                        {
                            if (audit_token_to_pid(_token) == getpid()) {
                                // We sent ourselves a request. That's fine
                                status = kDAReturnSuccess;
                                DALog("_kDADiskProbe authorized ourself");
                            } else {
                                // Entitlement check
                                status = DASessionGetIsFSKitd( session ) ? kDAReturnSuccess : kDAReturnNotPrivileged;
                                DALog("_kDADiskProbe checking request from pid %u, replying %d",
                                      audit_token_to_pid(_token), status);
                                break;
                            }
                            break;
                        }
                        case _kDADiskRename:
                        {
                            status = DAAuthorize( session, _kDAAuthorizeOptionIsOwner, disk, audit_token_to_euid( _token ), audit_token_to_egid( _token ), _kDAAuthorizeRightRename );

                            break;
                        }
                        case _kDADiskUnmount:
                        {
                            status = DAAuthorize( session, _kDAAuthorizeOptionIsOwner, disk, audit_token_to_euid( _token ), audit_token_to_egid( _token ),  _kDAAuthorizeRightUnmount );

                            break;
                        }
                        case _kDADiskSetFSKitAdditions:
                        {
                            // Entitlement check
                            status = DASessionGetIsFSKitd( session ) ? kDAReturnSuccess : kDAReturnNotPrivileged;
                            break;
                        }
                        default:
                        {
                            status = kDAReturnSuccess;

                            break;
                        }
                    }

                    if ( status == kDAReturnSuccess )
                    {
                        DAQueueRequest( request );

                        DALogDebug( "  queued solicitation, id = %016llX:%016llX, kind = %s, disk = %@, options = 0x%08X.",
                                    _address,
                                    _context,
                                    _DARequestKindGetName( _kind ),
                                    disk,
                                    _argument1 );
                    }

                    CFRelease( request );
                }

                if ( callback )
                {
                    CFRelease( callback );
                }

                if ( argument2 )
                {
                    CFRelease( argument2 );
                }

                if ( argument3 )
                {
                    CFRelease( argument3 );
                }
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to queue solicitation, id = %016llX:%016llX, kind = %s, disk = %s (status code 0x%08X).",
                    _address,
                    _context,
                    _DACallbackKindGetName( _kind ),
                    _argument0,
                    status );
    }

    return status;
}

kern_return_t _DAServerSessionQueueResponse( mach_port_t            _session,
                                             mach_vm_offset_t       _address,
                                             mach_vm_offset_t       _context,
                                             uint32_t                _kind,
                                             caddr_t                _disk,
                                             vm_address_t           _response,
                                             mach_msg_type_number_t _responseSize,
                                             int32_t                _responseID )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            CFTypeRef response = NULL;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            if ( _response )
            {
                response = _DAUnserializeWithBytes( kCFAllocatorDefault, _response, _responseSize );
            }

            if ( _DAResponseDispatch( response, _responseID ) == FALSE )
            {
                DALogDebug( "  dispatched response, id = %016llX:%016llX, kind = %s, disk = %s, orphaned.", _address, _context, _DACallbackKindGetName( _kind ), _disk );
            }

            if ( response )
            {
                CFRelease( response );
            }

            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        DALogDebug( "unable to dispatch response, id = %016llX:%016llX, disk = %s (status code 0x%08X).", _address, _context, _disk, status );
    }

    return status;
}

kern_return_t _DAServerSessionRegisterCallback( mach_port_t            _session,
                                                mach_vm_offset_t       _address,
                                                mach_vm_offset_t       _context,
                                                uint32_t                _kind,
                                                int32_t                _order,
                                                vm_address_t           _match,
                                                mach_msg_type_number_t _matchSize,
                                                vm_address_t           _watch,
                                                mach_msg_type_number_t _watchSize )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DACallbackRef   callback;
            CFDictionaryRef match = NULL;
            CFArrayRef      watch = NULL;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            if ( _kDADiskLastKind < _kind )
            {
                goto exit;
            }

            if ( _match )
            {
                match = _DAUnserializeDiskDescriptionWithBytes( kCFAllocatorDefault, _match, _matchSize );
            }

            if ( _watch )
            {
                watch = _DAUnserializeWithBytes( kCFAllocatorDefault, _watch, _watchSize );
            }

            callback = DACallbackCreate( kCFAllocatorDefault, session, _address, _context, _kind, _order, match, watch );

            if ( callback )
            {
                DASessionRegisterCallback( session, callback );

                DALogDebug( "  registered callback, id = %016llX:%016llX, kind = %s.", _address, _context, _DACallbackKindGetName( _kind ) );

                if ( DACallbackGetKind( callback ) == _kDADiskAppearedCallback )
                {
                    CFIndex count;
                    CFIndex index;
                   
                    count = CFArrayGetCount( gDADiskList );

                    for ( index = 0; index < count; index++ )
                    {
                        DADiskRef disk;

                        disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

                        if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
                        {
                            DAQueueCallback( callback, disk, NULL );
                        }
                    }

                    DAQueueCallbacks( session, _kDADiskListCompleteCallback, NULL, NULL );

                    if ( gDAIdle )
                    {
                        DAQueueCallbacks( session, _kDAIdleCallback, NULL, NULL );

                        DASessionSetState( session, kDASessionStateIdle, TRUE );
                    }
                }
                else if ( DACallbackGetKind( callback ) == _kDAIdleCallback )
                {
                    if ( gDAIdle )
                    {
                        DAQueueCallback( callback, NULL, NULL );

                        DASessionSetState( session, kDASessionStateIdle, TRUE );
                    }
                    else
                    {
                        DASessionSetState( session, kDASessionStateIdle, FALSE );
                    }
                }
///w:start
#if TARGET_OS_OSX
                else if ( DACallbackGetKind( callback ) == _kDADiskEjectApprovalCallback )
                {
                    if ( strcmp( _DASessionGetName( session ), "SystemUIServer" ) == 0 )
                    {
                        CFStringRef key;

                        key = SCDynamicStoreKeyCreateConsoleUser( kCFAllocatorDefault );

                        if ( key )
                        {
                            CFMutableArrayRef keys;

                            keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

                            if ( keys )
                            {
                                CFArrayAppendValue( keys, key );

                                _DAConfigurationCallback( NULL, keys, NULL );

                                CFRelease( keys );
                            }

                            CFRelease( key );
                        }
                    }
                }
///w:stop
#endif

                CFRelease( callback );

                status = kDAReturnSuccess;
            }

            if ( match )
            {
                CFRelease( match );
            }

            if ( watch )
            {
                CFRelease( watch );
            }
        }
    }

exit:
    if ( status )
    {
        DALogDebug( "unable to register callback, id = %016llX:%016llX, kind = %s (status code 0x%08X).", _address, _context, _DACallbackKindGetName( _kind ), status );
    }

    return status;
}

kern_return_t _DAServerSessionRelease( mach_port_t _session )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            CFMutableArrayRef callbacks;
            
            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            DALogDebug( "  removed session, id = %@.", session );

            callbacks = DASessionGetCallbackQueue( session );

            if ( callbacks )
            {
                CFArrayRemoveAllValues( callbacks );
            }

            callbacks = DASessionGetCallbackRegister( session );

            if ( callbacks )
            {
                CFArrayRemoveAllValues( callbacks );
            }

            DAQueueReleaseSession( session );

            /*
             * Remove the session object from our tables.
             */

            DASessionSetState( session, kDASessionStateZombie, TRUE );


            if ( DASessionGetKeepAlive( session ) == false )
            {
                ___os_transaction_end( );

                __DASetIdleTimer();
            }

            ___CFArrayRemoveValue( gDASessionList, session );
            
            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        DALogDebug( "unable to release session, id = ? [?]:%d.", _session );
    }

    return status;
}


kern_return_t _DAServerSessionCancel( mach_port_t _session )

{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DASessionCancelChannel( session );
            status = kDAReturnSuccess;
        }
    }
}

#if TARGET_OS_OSX
kern_return_t _DAServerSessionSetAuthorization( mach_port_t _session, AuthorizationExternalForm _authorization )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            AuthorizationRef authorization;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            status = AuthorizationCreateFromExternalForm( &_authorization, &authorization );

            if ( status == errAuthorizationSuccess )
            {
                DASessionSetAuthorization( session, authorization );

                DALogDebug( "  set authorization, id = %@.", session );

                status = kDAReturnSuccess;
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to set authorization, id = ? [?]:%d.", _session );
    }

    return status;
}
#endif

kern_return_t _DAServerSessionSetKeepAlive(  mach_port_t _session )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );
            DALogDebug( "  set keepalive, id = %@.", session );
            DASessionSetKeepAlive( session, true );
            ___os_transaction_end( );
            __DASetIdleTimer();
            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        DALogDebug( "unable to set keep alive,  (status code 0x%08X).", status );
    }

    return status;
}

kern_return_t _DAServerSessionSetClientPort( mach_port_t _session, mach_port_t _client )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            DASessionSetClientPort( session, _client );

            DALogDebug( "  set client port, id = %@.", session );

            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        DALogDebug( "unable to set client port, id = ? [?]:%d.", _session );
    }

    return status;
}

kern_return_t _DAServerSessionUnregisterCallback( mach_port_t _session, mach_vm_offset_t _address, mach_vm_offset_t _context )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "? [?]:%d -> %s", _session, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        session = __DASessionListGetSession( _session );

        if ( session )
        {
            DACallbackRef callback;

            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            callback = DACallbackCreate( kCFAllocatorDefault, session, _address, _context, 0, 0, NULL, NULL );

            if ( callback )
            {
                DAQueueUnregisterCallback( callback );

                DASessionUnregisterCallback( session, callback );

                DALogDebug( "  unregistered callback, id = %016llX:%016llX.", _address, _context );

                CFRelease( callback );

                status = kDAReturnSuccess;
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to unregister callback, id = %016llX:%016llX (status code 0x%08X).", _address, _context, status );
    }

    return status;
}

#if TARGET_OS_OSX

void _DAVolumeMountedMachHandler( void *context, dispatch_mach_reason_t reason,
                                     dispatch_mach_msg_t msg, mach_error_t err )
{
    if (reason == DISPATCH_MACH_MESSAGE_RECEIVED)
    {
        mach_msg_header_t   *header = dispatch_mach_msg_get_msg(msg, NULL);
        _DAVolumeMountedCallback();
        mach_msg_destroy( header );
    }
}

void _DAVolumeUnmountedMachHandler( void *context, dispatch_mach_reason_t reason,
                                     dispatch_mach_msg_t msg, mach_error_t err )
{
    if (reason == DISPATCH_MACH_MESSAGE_RECEIVED)
    {
        mach_msg_header_t   *header = dispatch_mach_msg_get_msg(msg, NULL);
        _DAVolumeUnmountedCallback();
        mach_msg_destroy( header );
    }
}

void _DAVolumeUpdatedMachHandler( void *context, dispatch_mach_reason_t reason,
                                     dispatch_mach_msg_t msg, mach_error_t err )
{
    if (reason == DISPATCH_MACH_MESSAGE_RECEIVED)
    {
        mach_msg_header_t   *header = dispatch_mach_msg_get_msg(msg, NULL);
        _DAVolumeUpdatedCallback();
        mach_msg_destroy( header );
    }
}
#endif

void _DADiskCreateFromFSStat(struct statfs *fs)
{
    DADiskRef disk;
    disk = DADiskCreateFromVolumePath( kCFAllocatorDefault, fs );

    if ( disk )
    {
        
        DALogInfo( "created disk, id = %@.", disk );
        
        /*
         * If this is a snapshot mount, set the NoDefer flag based on its live volume.
         */
        if ( ( fs->f_flags & MNT_SNAPSHOT ) )
        {
            char *endStr = strrchr( fs->f_mntfromname, '@' ) + 1;
            if ( endStr && ( strncmp( endStr, _PATH_DEV "disk", strlen( _PATH_DEV "disk" ) ) == 0 ) )
            {
                DADiskRef liveDisk = DADiskListGetDisk( endStr );
                
                if ( liveDisk && ( DADiskGetState( liveDisk, _kDADiskStateMountAutomaticNoDefer ) == FALSE ) )
                {
                    DADiskSetState( disk, _kDADiskStateMountAutomaticNoDefer, FALSE );
                }
            }
        }
        
        CFArrayInsertValueAtIndex( gDADiskList, 0, disk );
        CFRelease( disk );

    }

}
    
void _DAVolumeMountedCallback(  )
{
  
        struct statfs * mountList;
        int             mountListCount;
        int             mountListIndex;

        mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

        for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
        {
            DADiskRef disk;
            
            disk = DADiskListGetDisk( _DAVolumeGetID( mountList + mountListIndex ) );

            if ( disk )
            {
                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) == NULL )
                {
///w:start
                    if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanFalse )
                    {
                        DADiskProbe( disk, NULL );
                    }
///w:stop
                    DADiskRefresh( disk, NULL );
                }
            }
            else
            {
///w:start
                if ( strncmp( mountList[mountListIndex].f_mntfromname, _PATH_DEV "disk", strlen( _PATH_DEV "disk" ) ) )
///w:stop
                    if ( ( mountList[mountListIndex].f_flags & MNT_UNION ) == 0 )
                    {
                        if ( strcmp( mountList[mountListIndex].f_fstypename, "devfs" ) )
                        {
                            _DADiskCreateFromFSStat( &mountList[mountListIndex] );
                            DAStageSignal( );
                        }
                    }
            }
        }
      
}

void _DAVolumeUnmountedCallback(  )
{
    CFIndex count;
    CFIndex index;



        count = CFArrayGetCount( gDADiskList );

        for ( index = 0; index < count; index++ )
        {
            DADiskRef disk;

            disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

            if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) )
            {
                DADiskRefresh( disk, NULL );
            }
        }
        
}

void _DAVolumeUpdatedCallback(  )
{
    CFIndex count;
    CFIndex index;

 
        
        count = CFArrayGetCount( gDADiskList );

        for ( index = 0; index < count; index++ )
        {
            DADiskRef disk;

            disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

            if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) )
            {
                DADiskRefresh( disk, NULL );
            }
        }
}

dispatch_workloop_t DAServerWorkLoop( void )
{
    /*
     * Create a dispatch work loop for DAServer remote procedure calls.
     */

    static dispatch_once_t          once;
    static dispatch_workloop_t      workloop;

    dispatch_once(&once, ^{
                   workloop = dispatch_workloop_create_inactive( "DAServer" );
                   dispatch_set_qos_class_fallback( workloop, QOS_CLASS_USER_INITIATED );
                   dispatch_activate( workloop );
    });

    return workloop;
}

void DAServerInit ( void )
{
   /*
    * Initialize our minimal state.
    */

    /*
    * Register the Disk Arbitration master port.
    */

    if ( __gDAServerPort == MACH_PORT_NULL )
    {
        ( void ) bootstrap_check_in( bootstrap_port, _kDADaemonName, &__gDAServerPort );
    }

    if ( __gDAServerPort )
    {
        /*
        * Create the Disk Arbitration master port.
        */

        __gDAServerListener = dispatch_mach_create_f("diskarbitrationd",
                                                                    DAServerWorkLoop(),
                                                                    NULL,
                                                                    DAServerMachHandler);
        dispatch_mach_connect(__gDAServerListener, __gDAServerPort, MACH_PORT_NULL, NULL);

    }
}

