/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include <paths.h>
#include <unistd.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOMessage.h>
#include <IOKit/storage/IOMedia.h>
///w:start
#include <sys/stat.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>
///w:stop

static CFMachPortRef       __gDAServer      = NULL;
static mach_port_t         __gDAServerPort  = NULL;
static mach_msg_header_t * __gDAServerReply = NULL;

static DADiskRef __DADiskListGetDisk( const char * diskID )
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
            DADiskSetBusy( disk, 0 );
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
    }
}

static void __DAMediaDisappearedUnmountCallback( int status, void * context )
{
    CFURLRef path = context;

    DAMountRemoveMountPoint( path );

    CFRelease( path );
}

static void __DAMediaPropertyChangedCallback( void * context, io_service_t service )
{
    DADiskRef disk;

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
                CFTypeRef object;

                object = CFDictionaryGetValue( properties, CFSTR( kIOMediaContentKey ) );

                if ( DADiskCompareDescription( disk, kDADiskDescriptionMediaContentKey, object ) )
                {
                    DADiskSetDescription( disk, kDADiskDescriptionMediaContentKey, object );

                    CFArrayAppendValue( keys, kDADiskDescriptionMediaContentKey );
                }

                if ( CFArrayGetCount( keys ) )
                {
                    DALogDebugHeader( "iokit [0] -> %s", gDAProcessNameID );

                    DALogDebug( "  updated disk, id = %@.", disk );

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

void _DAConfigurationCallback( SCDynamicStoreRef session, CFArrayRef keys, void * info )
{
    CFStringRef key;

    DALogDebugHeader( "configd [0] -> %s", gDAProcessNameID );

    key = SCDynamicStoreKeyCreateConsoleUser( kCFAllocatorDefault );

    assert( key );

    if ( ___CFArrayContainsValue( keys, key ) )
    {
        /*
         * A console user has logged in or logged out.
         */

        gid_t userGID;
        uid_t userUID;

        userGID = gDAConsoleUserGID;
        userUID = gDAConsoleUserUID;

        if ( gDAConsoleUser )
        {
            CFRelease( gDAConsoleUser );
        }

        gDAConsoleUser = SCDynamicStoreCopyConsoleUser( session, &gDAConsoleUserUID, &gDAConsoleUserGID );

        if ( gDAConsoleUser )
        {
            CFIndex count;
            CFIndex index;
///w:start
            CFArrayRef users;

            users = SCDynamicStoreCopyConsoleInformation( session );
///w:stop

            DALogDebug( "  console user = %@ [%d].", gDAConsoleUser, gDAConsoleUserUID );

            /*
             * A console user has logged in.
             */

            count = CFArrayGetCount( gDADiskList );

            for ( index = 0; index < count; index++ )
            {
                DADiskRef disk;

                disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );
///w:start
                /*
                 * Set the BSD permissions for this media object.
                 */
            
                if ( DADiskGetUserRUID( disk ) == ___UID_UNKNOWN )
                {
                    mode_t deviceMode;
                    uid_t  deviceUser;

                    deviceMode = 0640;
                    deviceUser = gDAConsoleUserUID;

                    if ( users )
                    {
                        if ( CFArrayGetCount( users ) > 1 )
                        {
                            deviceMode = 0666;
                            deviceUser = ___UID_ROOT;
                        }
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

                if ( users )
                {
                    if ( session )
                    {
                        continue;
                    }
                }
///w:stop

                /*
                 * Mount this volume.
                 */
            
                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanTrue )
                {
                    if ( DAMountGetPreference( disk, kDAMountPreferenceDefer ) )
                    {
                        DADiskMount( disk, NULL, CFSTR( "automatic" ), NULL );
                    }
                }
            }

            DAStageSignal( );
///w:start
            if ( users )
            {
                CFRelease( users );
            }
///w:stop
        }
        else
        {
            CFIndex count;
            CFIndex index;

            DALogDebug( "  console user = none." );

            /*
             * A console user has logged out.
             */

            count = CFArrayGetCount( gDADiskList );

            for ( index = 0; index < count; index++ )
            {
                DADiskRef disk;

                disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );
///w:start
                /*
                 * Set the BSD permissions for this media object.
                 */

                if ( DADiskGetUserRUID( disk ) == ___UID_UNKNOWN )
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
                    if ( DADiskGetOption( disk, kDADiskOptionEjectUponLogout ) )
                    {
                        DADiskUnmount( disk, NULL );

                        DADiskEject( disk, NULL );
                    }
                    else if ( DAMountGetPreference( disk, kDAMountPreferenceDefer ) )
                    {
                        DADiskUnmount( disk, NULL );
                    }
                    else
                    {
                        if ( DADiskGetUserEUID( disk ) )
                        {
                            if ( DADiskGetUserEUID( disk ) == userUID )
                            {
                                DADiskUnmount( disk, NULL );
                            }
                        }
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
                    if ( DADiskGetOption( disk, kDADiskOptionEjectUponLogout ) )
                    {
                        DADiskEject( disk, NULL );
                    }
                }
            }

            DAStageSignal( );
        }
    }

    CFRelease( key );
}

void _DAMediaAppearedCallback( void * context, io_iterator_t notification )
{
    /*
     * Process the appearance of media objects in I/O Kit.
     */

    io_service_t media;
///w:start
    CFArrayRef users;

    users = SCDynamicStoreCopyConsoleInformation( NULL );
///w:stop

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
            __DAMediaPropertyChangedCallback( NULL, media );

            DAUnitSetState( disk, kDAUnitStateEjected, FALSE );
        }
        else
        {
            /*
             * Create a disk object for this media object.
             */

            DALogDebugHeader( "iokit [0] -> %s", gDAProcessNameID );

            disk = DADiskCreateFromIOMedia( kCFAllocatorDefault, media );

            if ( disk )
            {
                int         busy;
                io_object_t busyNotification;

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

                    assert( context == NULL );

                    _DAMediaDisappearedCallback( disk, gDAMediaDisappearedNotification );

                    assert( ___CFArrayContainsValue( gDADiskList, disk ) == FALSE );
                }

                /*
                 * Create the "media changed" notification.
                 */

                busyNotification = NULL;

                IOServiceAddInterestNotification( gDAMediaPort, media, kIOBusyInterest, __DAMediaChangedCallback, NULL, &busyNotification );

                if ( busyNotification )
                {
                    DADiskSetBusyNotification( disk, busyNotification );

                    IOObjectRelease( busyNotification );
                }

                /*
                 * Set the busy state.
                 */

                busy = 0;

                IOServiceGetBusyState( media, &busy );

                if ( busy )
                {
                    DADiskSetBusy( disk, CFAbsoluteTimeGetCurrent( ) );
                }

                /*
                 * Set the BSD permissions for this media object.
                 */

                if ( DADiskGetUserRUID( disk ) == ___UID_UNKNOWN )
                {
                    if ( gDAConsoleUser )
                    {
///w:start
                        mode_t deviceMode;
                        uid_t  deviceUser;

                        deviceMode = 0640;
                        deviceUser = gDAConsoleUserUID;

                        if ( users )
                        {
                            if ( CFArrayGetCount( users ) > 1 )
                            {
                                deviceMode = 0666;
                                deviceUser = ___UID_ROOT;
                            }
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

                        ___DADisplayUpdateActivity( );
                    }
                }
                else
                {
                    if ( DADiskGetMode( disk ) )
                    {
                        chmod( DADiskGetBSDPath( disk, TRUE  ), DADiskGetMode( disk ) & 0666 );
                        chmod( DADiskGetBSDPath( disk, FALSE ), DADiskGetMode( disk ) & 0666 );
                    }

                    if ( DADiskGetUserRGID( disk ) )
                    {
                        chown( DADiskGetBSDPath( disk, TRUE  ), -1, DADiskGetUserRGID( disk ) );
                        chown( DADiskGetBSDPath( disk, FALSE ), -1, DADiskGetUserRGID( disk ) );
                    }

                    if ( DADiskGetUserRUID( disk ) )
                    {
                        chown( DADiskGetBSDPath( disk, TRUE  ), DADiskGetUserRUID( disk ), -1 );
                        chown( DADiskGetBSDPath( disk, FALSE ), DADiskGetUserRUID( disk ), -1 );
                    }
                }

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
                        DALogDebugHeader( "iokit [0] -> %s", gDAProcessNameID );

                        DALogError( "unable to link %@ to %s.", disk, DADiskGetBSDLink( disk, TRUE ) );

                        DADiskSetBSDLink( disk, TRUE,  NULL );
                        DADiskSetBSDLink( disk, FALSE, NULL );
                    }
                }

                /*
                 * Add the disk object to our tables.
                 */

                DALogDebugHeader( "iokit [0] -> %s", gDAProcessNameID );

                DALogDebug( "  created disk, id = %@.", disk );

                DAUnitSetState( disk, kDAUnitStateStagedUnreadable, FALSE );

                DAUnitSetState( disk, kDAUnitStateEjected, FALSE );

                CFArrayInsertValueAtIndex( gDADiskList, 0, disk );

                CFRelease( disk );
            }

            if ( context )
            {
                io_service_t service;

                service = ( io_service_t ) context;

                if ( IOObjectIsEqualTo( media, service ) )
                {
                    notification = NULL;
                }
            }
        }

        IOObjectRelease( media );
    }

    DAStageSignal( );
///w:start
    if ( users )
    {
        CFRelease( users );
    }
///w:stop
}

void _DAMediaDisappearedCallback( void * context, io_iterator_t notification )
{
    /*
     * Process the disappearance of media objects in I/O Kit.
     */

    io_service_t media;

    /*
     * Iterate through the media objects.
     */

    while ( ( media = IOIteratorNext( notification ) ) )
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

            if ( context == NULL )
            {
                _DAMediaAppearedCallback( ( void * ) media, gDAMediaAppearedNotification );

                disk = __DADiskListGetDiskWithIOMedia( media );
            }
        }

        if ( disk )
        {
            CFURLRef path;

            /*
             * Remove the disk object from our tables.
             */

            DALogDebugHeader( "iokit [0] -> %s", gDAProcessNameID );

            DALogDebug( "  removed disk, id = %@.", disk );

            if ( DADiskGetBSDLink( disk, TRUE ) )
            {
                unlink( DADiskGetBSDLink( disk, TRUE  ) );
                unlink( DADiskGetBSDLink( disk, FALSE ) );
            }

            path = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

            if ( path )
            {
                CFRetain( path );

                DADialogShowDeviceRemoval( );

                DAFileSystemUnmountWithArguments( DADiskGetFileSystem( disk ),
                                                  path,
                                                  __DAMediaDisappearedUnmountCallback,
                                                  ( void * ) path,
                                                  kDAFileSystemUnmountArgumentForce,
                                                  NULL );
            }

            if ( _gDAClassic == disk )
            {
                CFRelease( _gDAClassic );

                _gDAClassic = NULL;
            }

            DAQueueReleaseDisk( disk );

            if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
            {
                DADiskDisappearedCallback( disk );
            }

            DADiskSetState( disk, kDADiskStateZombie, TRUE );

            if ( context )
            {
                if ( CFEqual( disk, context ) )
                {
                    notification = NULL;
                }
            }

            ___CFArrayRemoveValue( gDADiskList, disk );
        }

        IOObjectRelease( media );
    }

    DAStageSignal( );
}

void _DANotifyCallback( CFMachPortRef port, void * parameter, CFIndex messageSize, void * info )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDADiskList );

    for ( index = 0; index < count; index++ )
    {
        DADiskRef disk;

        disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaPathKey ) == NULL )
        {
            DADiskRefresh( disk, NULL );
        }
    }
}

void _DAServerCallback( CFMachPortRef port, void * parameter, CFIndex messageSize, void * info )
{
    mach_msg_header_t * message = parameter;

    if ( message->msgh_id == MACH_NOTIFY_DEAD_NAME )
    {
        mach_dead_name_notification_t * notification = parameter;

        _DAServerSessionRelease( message->msgh_local_port );

        mach_port_deallocate( mach_task_self( ), notification->not_port );
    }
    else if ( DAServer_server( message, __gDAServerReply ) )
    {
        kern_return_t status;

        status = ( __gDAServerReply->msgh_bits & MACH_MSGH_BITS_COMPLEX )
                 ? KERN_SUCCESS
                 : ( ( mig_reply_error_t * ) __gDAServerReply )->RetCode;

        /*
         * Any resources present in the request message are the responsibility of the service
         * function, if it is successful in responding to the request.  Success is defined in
         * two ways:
         *
         * o KERN_SUCCESS:  This says that the userÕs request was processed, and all incoming
         *                  resources have been recorded or deallocated by the server routine.
         * o MIG_NO_REPLY:  This says that the userÕs request was accepted,  and all incoming
         *                  resources have been recorded or deallocated by the server routine.
         *
         * A reply should always be returned for any message received, unless the return code
         * from the server was MIG_NO_REPLY or the request message does not have a reply port.
         */

        if ( status != MIG_NO_REPLY )
        {
            if ( status != KERN_SUCCESS )
            {
                message->msgh_remote_port = MACH_PORT_NULL;

                mach_msg_destroy( message );
            }

            if ( __gDAServerReply->msgh_remote_port )
            {
                status = mach_msg_send( __gDAServerReply );

                if ( status == MACH_SEND_INVALID_DEST )
                {
                    mach_msg_destroy( __gDAServerReply );
                }
            }
        }
    }
}

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

            disk = __DADiskListGetDisk( _disk );

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

            disk = __DADiskListGetDisk( _disk );

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

kern_return_t _DAServerDiskGetUserRUID( mach_port_t _session, caddr_t _disk, uid_t * _userUID )
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

            disk = __DADiskListGetDisk( _disk );

            if ( disk )
            {
                *_userUID = DADiskGetUserRUID( disk );

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

            disk = __DADiskListGetDisk( _disk );

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

kern_return_t _DAServerDiskRefresh( mach_port_t _session, caddr_t _disk )
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

            disk = __DADiskListGetDisk( _disk );

            if ( disk )
            {
                if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
                {
                    if ( DADiskGetOption( disk, kDADiskOptionPrivate ) == FALSE )
                    {
                        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanTrue )
                        {
                            DALogDebug( "  refreshed disk, id = %@.", disk );

                            status = _DADiskRefresh( disk );
                        }
                    }
                }
            }
            else
            {
                struct statfs * mountList;
                int             mountListCount;
                int             mountListIndex;

                mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

                for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
                {
                    if ( strcmp( mountList[mountListIndex].f_mntonname, _disk ) == 0 )
                    {
                        break;
                    }
                }

                if ( mountListIndex < mountListCount )
                {
                    CFURLRef path;

                    path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault,
                                                                    mountList[mountListIndex].f_mntonname,
                                                                    strlen( mountList[mountListIndex].f_mntonname ),
                                                                    TRUE );

                    if ( path )
                    {
                        disk = DADiskCreateFromVolumePath( kCFAllocatorDefault, path );

                        if ( disk )
                        {
                            DALogDebug( "  created disk, id = %@.", disk );

                            CFArrayInsertValueAtIndex( gDADiskList, 0, disk );

                            DAStageSignal( );

                            status = kDAReturnSuccess;

                            CFRelease( disk );
                        }

                        CFRelease( path );
                    }
                }
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to refresh disk, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerDiskSetAdoption( mach_port_t _session, caddr_t _disk, boolean_t _adoption, security_token_t _token )
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

            disk = __DADiskListGetDisk( _disk );

            if ( disk )
            {
                if ( _adoption )
                {
                    status = DAAuthorize( session, kDAAuthorizeOptionDefault, NULL, _token.val[0], _token.val[1], _kDAAuthorizeRightAdopt );
                }
                else
                {
                    status = DAAuthorize( session, kDAAuthorizeOptionDefault, disk, _token.val[0], _token.val[1], _kDAAuthorizeRightAdopt );
                }

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

kern_return_t _DAServerDiskSetClassic( mach_port_t _session, caddr_t _disk )
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

            disk = __DADiskListGetDisk( _disk );

            if ( disk )
            {
                if ( _gDAClassic )
                {
                    CFRelease( _gDAClassic );
                }

                CFRetain( disk );

                _gDAClassic = disk;

                DALogDebug( "  set disk classic, id = %@.", disk );

                DADiskClassicCallback( disk );

                status = kDAReturnSuccess;
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to set disk classic, id = %s (status code 0x%08X).", _disk, status );
    }

    return status;
}

kern_return_t _DAServerDiskSetEncoding( mach_port_t _session, caddr_t _disk, int32_t encoding, security_token_t _token )
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

            disk = __DADiskListGetDisk( _disk );

            if ( disk )
            {
                status = DAAuthorize( session, kDAAuthorizeOptionDefault, disk, _token.val[0], _token.val[1], _kDAAuthorizeRightEncode );

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

            disk = __DADiskListGetDisk( _disk );

            if ( disk )
            {
                DALogDebug( "  set disk options, id = %@, options = 0x%08X, value = %s.", disk, _options, _value ? "true" : "false" );

                if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
                {
                    if ( ( _options & kDADiskOptionPrivate ) )
                    {
                        if ( _value )
                        {
                            if ( DADiskGetOption( disk, kDADiskOptionPrivate ) == FALSE )
                            {
                                DADiskDisappearedCallback( disk );

                                DAStageSignal( );
                            }
                        }
                        else
                        {
                            if ( DADiskGetOption( disk, kDADiskOptionPrivate ) )
                            {
                                DADiskSetOption( disk, kDADiskOptionPrivate, FALSE );

                                DADiskAppearedCallback( disk );

                                DAStageSignal( );
                            }
                        }
                    }
                }

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

            disk = __DADiskListGetDisk( _disk );

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

                    DACallbackSetSession( callback, NULL );
///w:start
                    CFDictionaryRemoveValue( ( void * ) callback, _kDACallbackMatchKey );
///w:stop
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

kern_return_t _DAServerSessionCreate( mach_port_t               _session,
                                      mach_port_t               _client,
                                      caddr_t                   _name,
                                      pid_t                     _pid,
                                      AuthorizationExternalForm _rights,
                                      mach_port_t *             _server )
{
    kern_return_t status;

    status = kDAReturnBadArgument;

    DALogDebugHeader( "%s [%d] -> %s", _name, _pid, gDAProcessNameID );

    if ( _session )
    {
        DASessionRef session;

        /*
         * Create the session.
         */

        session = DASessionCreate( kCFAllocatorDefault, _client, _name, _pid, _rights );

        if ( session )
        {
            DALogDebugHeader( "%@ -> %s", session, gDAProcessNameID );

            DALogDebug( "  created session, id = %@.", session );

            *_server = DASessionGetServerPort( session );

            /*
             * Add the session object to our tables.
             */

            CFArrayAppendValue( gDASessionList, session );

            /*
             * Add the session to our run loop.
             */

            DASessionScheduleWithRunLoop( session, CFRunLoopGetCurrent( ), kCFRunLoopDefaultMode ); 

            CFRelease( session );

            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        DALogDebug( "unable to create session, id = %s [%d].", _name, _pid );
    }

    return status;
}

kern_return_t _DAServerSessionQueueRequest( mach_port_t            _session,
                                            int32_t                _kind,
                                            caddr_t                _argument0,
                                            int32_t                _argument1,
                                            vm_address_t           _argument2,
                                            mach_msg_type_number_t _argument2Size,
                                            vm_address_t           _argument3,
                                            mach_msg_type_number_t _argument3Size,
                                            vm_offset_t            _address,
                                            vm_offset_t            _context,
                                            security_token_t       _token )
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

            disk = __DADiskListGetDisk( _argument0 );

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
                
                request = DARequestCreate( kCFAllocatorDefault, _kind, disk, _argument1, argument2, argument3, _token.val[0], _token.val[1], callback );

                if ( request )
                {
                    switch ( _kind )
                    {
                        case _kDADiskEject:
                        {
                            status = DAAuthorize( session, kDAAuthorizeOptionDefault, disk, _token.val[0], _token.val[1], _kDAAuthorizeRightUnmount );

                            break;
                        }
                        case _kDADiskMount:
                        {
                            status = DAAuthorize( session, kDAAuthorizeOptionDefault, disk, _token.val[0], _token.val[1], _kDAAuthorizeRightMount );

                            break;
                        }
                        case _kDADiskRename:
                        {
                            status = DAAuthorize( session, kDAAuthorizeOptionDefault, disk, _token.val[0], _token.val[1], _kDAAuthorizeRightRename );

                            break;
                        }
                        case _kDADiskUnmount:
                        {
                            status = DAAuthorize( session, kDAAuthorizeOptionDefault, disk, _token.val[0], _token.val[1], _kDAAuthorizeRightUnmount );

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

                        DALogDebug( "  queued solicitation, id = %08X:%08X, kind = %s, disk = %@, options = 0x%08X.",
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
        DALogDebug( "unable to queue solicitation, id = %08X:%08X, kind = %s, disk = %s (status code 0x%08X).",
                    _address,
                    _context,
                    _DACallbackKindGetName( _kind ),
                    _argument0,
                    status );
    }

    return status;
}

kern_return_t _DAServerSessionQueueResponse( mach_port_t            _session,
                                             vm_offset_t            _address,
                                             vm_offset_t            _context,
                                             int32_t                _kind,
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
                DALogDebug( "  dispatched response, id = %08X:%08X, kind = %s, disk = %s, orphaned.", _address, _context, _DACallbackKindGetName( _kind ), _disk );
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
        DALogDebug( "unable to dispatch response, id = %08X:%08X, disk = %s (status code 0x%08X).", _address, _context, _disk, status );
    }

    return status;
}

kern_return_t _DAServerSessionRegisterCallback( mach_port_t            _session,
                                                vm_offset_t            _address,
                                                vm_offset_t            _context,
                                                int32_t                _kind,
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

                DALogDebug( "  registered callback, id = %08X:%08X, kind = %s.", _address, _context, _DACallbackKindGetName( _kind ) );

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

                    if ( gDAIdle )
                    {
                        DAQueueCallbacks( session, _kDAIdleCallback, NULL, NULL );

                        DASessionSetState( session, kDASessionStateIdle, TRUE );
                    }
                }
                else if ( DACallbackGetKind( callback ) == _kDADiskClassicCallback )
                {
                    if ( _gDAClassic )
                    {
                        DAQueueCallback( callback, _gDAClassic, NULL );
                    }
                }
                else if ( DACallbackGetKind( callback ) == _kDAIdleCallback )
                {
                    if ( gDAIdle )
                    {
                        DAQueueCallback( callback, NULL, NULL );

                        DASessionSetState( session, kDASessionStateIdle, TRUE );
                    }
                }
///w:start
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

    if ( status )
    {
        DALogDebug( "unable to register callback, id = %08X:%08X, kind = %s (status code 0x%08X).", _address, _context, _DACallbackKindGetName( _kind ), status );
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
             * Remove the session from our run loop.
             */

            DASessionUnscheduleFromRunLoop( session, CFRunLoopGetCurrent( ), kCFRunLoopDefaultMode ); 

            /*
             * Remove the session object from our tables.
             */

            DASessionSetState( session, kDASessionStateZombie, TRUE );

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

kern_return_t _DAServerSessionUnregisterCallback( mach_port_t _session, vm_offset_t _address, vm_offset_t _context )
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
                DASessionUnregisterCallback( session, callback );

                DALogDebug( "  unregistered callback, id = %08X:%08X.", _address, _context );

                CFRelease( callback );

                status = kDAReturnSuccess;
            }
        }
    }

    if ( status )
    {
        DALogDebug( "unable to unregister callback, id = %08X:%08X (status code 0x%08X).", _address, _context, status );
    }

    return status;
}

CFRunLoopSourceRef DAServerCreateRunLoopSource( CFAllocatorRef allocator, CFIndex order )
{
    /*
     * Create a CFRunLoopSource for DAServer remote procedure calls.
     */

    CFRunLoopSourceRef source = NULL;

    /*
     * Initialize our minimal state.
     */

    if ( __gDAServer == NULL )
    {
        mach_port_t   bootstrapPort;
        mach_port_t   privatePort;
        kern_return_t status;

        /*
         * Obtain the bootstrap port.
         */

        status = task_get_bootstrap_port( mach_task_self( ), &bootstrapPort );

        if ( status == KERN_SUCCESS )
        {
            /*
             * Register the Disk Arbitration master port.
             */
            
            if ( __gDAServerPort == NULL )
            {
                status = bootstrap_create_server( bootstrapPort, "/usr/sbin/diskarbitrationd", getuid( ), FALSE, &privatePort );

                if ( status == KERN_SUCCESS )
                {
                    mach_port_t port;

                    status = bootstrap_create_service( privatePort, _kDAServiceName, &port );

                    if ( status == KERN_SUCCESS )
                    {
                        status = bootstrap_check_in( privatePort, _kDAServiceName, &__gDAServerPort );

                        mach_port_deallocate( mach_task_self( ), port );
                    }

                    mach_port_deallocate( mach_task_self( ), privatePort );
                }
            }

            mach_port_deallocate( mach_task_self( ), bootstrapPort );
        }

        if ( __gDAServerPort )
        {
            /*
             * Create the Disk Arbitration master port.
             */

            __gDAServer = CFMachPortCreateWithPort( allocator, __gDAServerPort, _DAServerCallback, NULL, NULL );

            if ( __gDAServer )
            {
                __gDAServerReply = malloc( DAServer_subsystem.maxsize );

                assert( __gDAServerReply );
            }
        }
    }

    /*
     * Obtain the CFRunLoopSource for our CFMachPort.
     */

    if ( __gDAServer )
    {
        source = CFMachPortCreateRunLoopSource( allocator, __gDAServer, order );
    }

    return source;
}

DAServerStatus DAServerInitialize( void )
{
    mach_port_t   bootstrapPort;
    kern_return_t status;

    /*
     * Obtain the bootstrap port.
     */

    status = task_get_bootstrap_port( mach_task_self( ), &bootstrapPort );

    if ( status == KERN_SUCCESS )
    {
        /*
         * Obtain the Disk Arbitration master port.
         */

        status = bootstrap_check_in( bootstrapPort, _kDAServiceName, &__gDAServerPort );

        switch ( status )
        {
            case BOOTSTRAP_SUCCESS:
            {
                return kDAServerStatusInitialize;
            }
            case BOOTSTRAP_UNKNOWN_SERVICE:
            {
                return kDAServerStatusInactive;
            }
            default:
            {
                return kDAServerStatusActive;
            }
        }

        mach_port_deallocate( mach_task_self( ), bootstrapPort );
    }
    else
    {
        return kDAServerStatusInactive;
    }
}
