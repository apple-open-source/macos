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

#include "DiskArbitration.h"

#include "DAInternal.h"
#include "DAServerUser.h"

#include <pthread.h>

CFDictionaryRef kDADiskDescriptionMatchMediaUnformatted   = NULL;
CFDictionaryRef kDADiskDescriptionMatchMediaWhole         = NULL;
CFDictionaryRef kDADiskDescriptionMatchVolumeMountable    = NULL;
CFDictionaryRef kDADiskDescriptionMatchVolumeUnrecognized = NULL;

CFArrayRef kDADiskDescriptionWatchVolumeName = NULL;
CFArrayRef kDADiskDescriptionWatchVolumePath = NULL;

__private_extern__ DADiskRef        _DADiskCreateFromSerialization( CFAllocatorRef allocator, DASessionRef session, CFDataRef serialization );
__private_extern__ char *           _DADiskGetID( DADiskRef disk );
__private_extern__ DASessionRef     _DADiskGetSession( DADiskRef disk );
__private_extern__ mach_port_t      _DADiskGetSessionID( DADiskRef disk );
__private_extern__ void             _DADiskInitialize( void );
__private_extern__ void             _DADiskSetDescription( DADiskRef disk, CFDictionaryRef description );
__private_extern__ mach_port_t      _DASessionGetID( DASessionRef session );
__private_extern__ AuthorizationRef _DASessionGetRights( DASessionRef session );
__private_extern__ void             _DASessionInitialize( void );

static void __DAInitialize( void )
{
    CFMutableDictionaryRef match;
    CFMutableArrayRef      watch;

    /*
     * Initialize classes.
     */

    _DADiskInitialize( );

    _DASessionInitialize( );

    /*
     * Create the disk match description for unformatted media.
     */

    match = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    assert( match );

    ___CFDictionarySetIntegerValue( match, kDADiskDescriptionMediaSizeKey, 0 );

    kDADiskDescriptionMatchMediaUnformatted = match;

    /*
     * Create the disk match description for whole media.
     */

    match = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    assert( match );

    CFDictionarySetValue( match, kDADiskDescriptionMediaWholeKey, kCFBooleanTrue );

    kDADiskDescriptionMatchMediaWhole = match;

    /*
     * Create the disk match description for mountable volumes.
     */

    match = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    assert( match );

    CFDictionarySetValue( match, kDADiskDescriptionVolumeMountableKey, kCFBooleanTrue );

    kDADiskDescriptionMatchVolumeMountable = match;

    /*
     * Create the disk match description for unrecognized volumes.
     */

    match = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    assert( match );

    CFDictionarySetValue( match, kDADiskDescriptionVolumeMountableKey, kCFBooleanFalse );

    kDADiskDescriptionMatchVolumeUnrecognized = match;

    /*
     * Create the disk watch description for volume name changes.
     */

    watch = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( watch );

    CFArrayAppendValue( watch, kDADiskDescriptionVolumeNameKey );

    kDADiskDescriptionWatchVolumeName = watch;

    /*
     * Create the disk watch description for volume path changes.
     */

    watch = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    assert( watch );

    CFArrayAppendValue( watch, kDADiskDescriptionVolumePathKey );

    kDADiskDescriptionWatchVolumePath = watch;
}

static DAReturn __DAQueueRequest( DASessionRef   session,
                                  _DARequestKind kind,
                                  DADiskRef      argument0,
                                  CFIndex        argument1,
                                  CFTypeRef      argument2,
                                  CFTypeRef      argument3,
                                  void *         address,
                                  void *         context )
{
    DAReturn status;

    status = kDAReturnBadArgument;

    if ( session )
    {
        CFDataRef _argument2 = NULL;
        CFDataRef _argument3 = NULL;

        if ( argument2 )  _argument2 = _DASerialize( kCFAllocatorDefault, argument2 );
        if ( argument3 )  _argument3 = _DASerialize( kCFAllocatorDefault, argument3 );

        status = _DAServerSessionQueueRequest( _DASessionGetID( session ),
                                               ( int32_t                ) kind,
                                               ( caddr_t                ) _DADiskGetID( argument0 ),
                                               ( int32_t                ) argument1,
                                               ( vm_address_t           ) ( _argument2 ? CFDataGetBytePtr( _argument2 ) : 0 ),
                                               ( mach_msg_type_number_t ) ( _argument2 ? CFDataGetLength(  _argument2 ) : 0 ),
                                               ( vm_address_t           ) ( _argument3 ? CFDataGetBytePtr( _argument3 ) : 0 ),
                                               ( mach_msg_type_number_t ) ( _argument3 ? CFDataGetLength(  _argument3 ) : 0 ),
                                               ( vm_offset_t            ) address,
                                               ( vm_offset_t            ) context );

        if ( _argument2 )  CFRelease( _argument2 );
        if ( _argument3 )  CFRelease( _argument3 );
    }

    return status;
}
                              
static void __DAQueueResponse( DASessionRef    session,
                               void *          address,
                               void *          context,
                               _DACallbackKind kind,
                               DADiskRef       disk,
                               CFTypeRef       response,
                               SInt32          responseID )
{
    CFDataRef _response = NULL;

    if ( response )  _response = _DASerialize( kCFAllocatorDefault, response );

    _DAServerSessionQueueResponse( _DASessionGetID( session ),
                                   ( vm_offset_t            ) address,
                                   ( vm_offset_t            ) context,
                                   ( int32_t                ) kind,
                                   ( caddr_t                ) _DADiskGetID( disk ),
                                   ( vm_address_t           ) ( _response ? CFDataGetBytePtr( _response ) : 0 ),
                                   ( mach_msg_type_number_t ) ( _response ? CFDataGetLength(  _response ) : 0 ),
                                   ( int32_t                ) responseID );

    if ( _response )  CFRelease( _response );
}

__private_extern__ DAReturn _DAAuthorize( DASessionRef session, DADiskRef disk, const char * right )
{
    DAReturn status;

    status = kDAReturnNotPrivileged;

    if ( status )
    {
        if ( geteuid( ) == ___UID_ROOT )
        {
            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        if ( ___isadmin( geteuid( ) ) )
        {
            status = kDAReturnSuccess;
        }
    }

    if ( status )
    {
        if ( disk )
        {
            uid_t diskUID;

            status = _DAServerDiskGetUserRUID( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), &diskUID );

            if ( status )
            {
                return status;
            }

            status = kDAReturnNotPrivileged;

            if ( diskUID == geteuid( ) )
            {
                status = kDAReturnSuccess;
            }

            if ( diskUID == ___UID_UNKNOWN )
            {
                status = kDAReturnSuccess;
            }
        }
    }

///w:start
    if ( _DASessionGetRights( session ) == NULL )
    {
        status = kDAReturnSuccess;
    }
///w:stop
    if ( status )
    {
        AuthorizationItem   item;
        AuthorizationFlags  flags;
        AuthorizationRights rights;

        item.flags       = 0;
        item.name        = right;
        item.value       = NULL;
        item.valueLength = 0;

        flags = kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagPreAuthorize;

        rights.count = 1;
        rights.items = &item;

        status = AuthorizationCopyRights( _DASessionGetRights( session ), &rights, NULL, flags, NULL ); 

        if ( status )
        {
            status = kDAReturnNotPrivileged;
        }
    }

    return status;
}

__private_extern__ void _DADispatchCallback( DASessionRef    session,
                                             void *          address,
                                             void *          context,
                                             _DACallbackKind kind,
                                             CFTypeRef       argument0,
                                             CFTypeRef       argument1 )
{
    DADiskRef disk     = NULL;
    CFTypeRef response = NULL;
    
    if ( argument0 )
    {
        disk = _DADiskCreateFromSerialization( CFGetAllocator( session ), session, argument0 );
    }

    switch ( kind )
    {
        case _kDADiskAppearedCallback:
        {
            ( ( DADiskAppearedCallback ) address )( disk, context );

            break;
        }
        case _kDADiskClaimCallback:
        {
            ( ( DADiskClaimCallback ) address )( disk, argument1, context );

            break;
        }
        case _kDADiskClaimReleaseCallback:
        {
            response = ( ( DADiskClaimReleaseCallback ) address )( disk, context );

            response = response ? response : kCFNull;

            break;
        }
        case _kDADiskClassicCallback:
        {
            ( ( _DADiskClassicCallback ) address )( disk, context );

            break;
        }
        case _kDADiskDescriptionChangedCallback:
        {
            ( ( DADiskDescriptionChangedCallback ) address )( disk, argument1, context );

            break;
        }
        case _kDADiskDisappearedCallback:
        {
            ( ( DADiskDisappearedCallback ) address )( disk, context );

            break;
        }
        case _kDADiskEjectCallback:
        {
            ( ( DADiskEjectCallback ) address )( disk, argument1, context );

            break;
        }
        case _kDADiskEjectApprovalCallback:
        {
            response = ( ( DADiskClaimReleaseCallback ) address )( disk, context );

            response = response ? response : kCFNull;
            
            break;
        }
        case _kDADiskMountCallback:
        {
            ( ( DADiskMountCallback ) address )( disk, argument1, context );

            break;
        }
        case _kDADiskMountApprovalCallback:
        {
            response = ( ( DADiskClaimReleaseCallback ) address )( disk, context );

            response = response ? response : kCFNull;
            
            break;
        }
        case _kDADiskPeekCallback:
        {
            ( ( DADiskPeekCallback ) address )( disk, context );

            response = kCFNull;

            break;
        }
        case _kDADiskRenameCallback:
        {
            ( ( DADiskRenameCallback ) address )( disk, argument1, context );

            break;
        }
        case _kDADiskUnmountCallback:
        {
            ( ( DADiskUnmountCallback ) address )( disk, argument1, context );

            break;
        }
        case _kDADiskUnmountApprovalCallback:
        {
            response = ( ( DADiskClaimReleaseCallback ) address )( disk, context );

            response = response ? response : kCFNull;
            
            break;
        }
        case _kDAIdleCallback:
        {
            ( ( DAIdleCallback ) address )( context );

            break;
        }
    }

    if ( response )
    {
        SInt32 responseID;

        responseID = ___CFNumberGetIntegerValue( argument1 );

        if ( response == kCFNull )
        {
            response = NULL;
        }

        __DAQueueResponse( session, address, context, kind, disk, response, responseID );

        if ( response )
        {
            CFRelease( response );
        }
    }

    if ( disk )
    {
        _DADiskSetDescription( disk, NULL );

        CFRelease( disk );
    }
}

__private_extern__ void _DAInitialize( void )
{
    static pthread_once_t initialize = PTHREAD_ONCE_INIT;
    
    pthread_once( &initialize, __DAInitialize );
}

__private_extern__ void _DARegisterCallback( DASessionRef    session,
                                             void *          address,
                                             void *          context,
                                             _DACallbackKind kind,
                                             CFIndex         order,
                                             CFDictionaryRef match,
                                             CFArrayRef      watch )
{
    CFDataRef _match = NULL;
    CFDataRef _watch = NULL;

    if ( match )  _match = _DASerializeDiskDescription( kCFAllocatorDefault, match );
    if ( watch )  _watch = _DASerialize( kCFAllocatorDefault, watch );

    _DAServerSessionRegisterCallback( _DASessionGetID( session ),
                                      ( vm_offset_t            ) address,
                                      ( vm_offset_t            ) context,
                                      ( int32_t                ) kind,
                                      ( int32_t                ) order,
                                      ( vm_address_t           ) ( _match ? CFDataGetBytePtr( _match ) : 0 ),
                                      ( mach_msg_type_number_t ) ( _match ? CFDataGetLength(  _match ) : 0 ),
                                      ( vm_address_t           ) ( _watch ? CFDataGetBytePtr( _watch ) : 0 ),
                                      ( mach_msg_type_number_t ) ( _watch ? CFDataGetLength(  _watch ) : 0 ) );

    if ( _match )  CFRelease( _match );
    if ( _watch )  CFRelease( _watch );
}

__private_extern__ void _DAUnregisterCallback( DASessionRef session, void * address, void * context )
{
    _DAServerSessionUnregisterCallback( _DASessionGetID( session ), ( vm_offset_t ) address, ( vm_offset_t ) context );
}

void DADiskClaim( DADiskRef                  disk,
                  DADiskClaimOptions         options,
                  DADiskClaimReleaseCallback release,
                  void *                     releaseContext,
                  DADiskClaimCallback        callback,
                  void *                     callbackContext )
{
    CFNumberRef _release;
    CFNumberRef _releaseContext;
    DAReturn    status;

    _release        = ___CFNumberCreateWithIntegerValue( kCFAllocatorDefault, ( vm_offset_t ) release        );
    _releaseContext = ___CFNumberCreateWithIntegerValue( kCFAllocatorDefault, ( vm_offset_t ) releaseContext );

    status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskClaim, disk, options, _release, _releaseContext, callback, callbackContext );

    if ( _release        )  CFRelease( _release        );
    if ( _releaseContext )  CFRelease( _releaseContext );

    if ( status )
    {
        DADissenterRef dissenter;

        dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

        ( callback )( disk, dissenter, callbackContext );

        CFRelease( dissenter );
    }
}

void DADiskEject( DADiskRef disk, DADiskEjectOptions options, DADiskEjectCallback callback, void * context )
{
    DAReturn status;

    status = _DAAuthorize( _DADiskGetSession( disk ), disk, _kDAAuthorizeRightUnmount );

    if ( status == kDAReturnSuccess )
    {
        status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskEject, disk, options, NULL, NULL, callback, context );
    }

    if ( status )
    {
        DADissenterRef dissenter;

        dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

        ( callback )( disk, dissenter, context );

        CFRelease( dissenter );
    }
}

DADiskOptions DADiskGetOptions( DADiskRef disk )
{
    int32_t options = 0;

    _DAServerDiskGetOptions( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), &options );

    return options;
}

Boolean DADiskIsClaimed( DADiskRef disk )
{
    boolean_t claimed = FALSE;

    _DAServerDiskIsClaimed( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), &claimed );

    return claimed;
}

void DADiskMount( DADiskRef disk, CFURLRef path, DADiskMountOptions options, DADiskMountCallback callback, void * context )
{
    DADiskMountWithArguments( disk, path, options, callback, context, NULL );
}

void DADiskMountWithArguments( DADiskRef           disk,
                               CFURLRef            path,
                               DADiskMountOptions  options,
                               DADiskMountCallback callback,
                               void *              context,
                               CFStringRef         arguments[] )
{
    CFMutableStringRef argument;
    DAReturn           status;

    argument = NULL;

    if ( arguments )
    {
        if ( arguments[0] )
        {
            if ( arguments[1] )
            {
                argument = CFStringCreateMutableCopy( kCFAllocatorDefault, 0, arguments[0] );

                if ( argument )
                {
                    CFIndex index;

                    for ( index = 1; arguments[index]; index++ )
                    {
                        CFStringAppend( argument, CFSTR( "," ) );
                        CFStringAppend( argument, arguments[index] );
                    }
                }
            }
            else
            {
                argument = ( void * ) CFRetain( arguments[0] );
            }
        }
    }

    if ( path )
    {
        if ( CFURLGetBaseURL( path ) )
        {
            path = CFURLCopyAbsoluteURL( path );
        }
        else
        {
            CFRetain( path );
        }
    }

    status = _DAAuthorize( _DADiskGetSession( disk ), disk, _kDAAuthorizeRightMount );

    if ( status == kDAReturnSuccess )
    {
        status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskMount, disk, options, path ? CFURLGetString( path ) : NULL, argument, callback, context );
    }

    if ( argument )
    {
        CFRelease( argument );
    }

    if ( path )
    {
        CFRelease( path );
    }

    if ( status )
    {
        DADissenterRef dissenter;

        dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

        ( callback )( disk, dissenter, context );

        CFRelease( dissenter );
    }
}

void DADiskRename( DADiskRef disk, CFStringRef name, DADiskRenameOptions options, DADiskRenameCallback callback, void * context )
{
    DAReturn status;

    status = kDAReturnBadArgument;

    if ( name )
    {
        if ( CFGetTypeID( name ) == CFStringGetTypeID( ) )
        {
            status = _DAAuthorize( _DADiskGetSession( disk ), disk, _kDAAuthorizeRightRename );

            if ( status == kDAReturnSuccess )
            {
                status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskRename, disk, options, name, NULL, callback, context );
            }
        }
    }

    if ( status )
    {
        DADissenterRef dissenter;

        dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

        ( callback )( disk, dissenter, context );

        CFRelease( dissenter );
    }
}

DAReturn DADiskSetOptions( DADiskRef disk, DADiskOptions options, Boolean value )
{
    return _DAServerDiskSetOptions( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), options, value );
}

void DADiskUnmount( DADiskRef disk, DADiskUnmountOptions options, DADiskUnmountCallback callback, void * context )
{
    DAReturn status;

    status = _DAAuthorize( _DADiskGetSession( disk ), disk, _kDAAuthorizeRightUnmount );

    if ( status == kDAReturnSuccess )
    {
        status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskUnmount, disk, options, NULL, NULL, callback, context );
    }

    if ( status )
    {
        DADissenterRef dissenter;

        dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

        ( callback )( disk, dissenter, context );

        CFRelease( dissenter );
    }
}

void DARegisterDiskAppearedCallback( DASessionRef           session,
                                     CFDictionaryRef        match,
                                     DADiskAppearedCallback callback,
                                     void *                 context )
{
    _DARegisterCallback( session, callback, context, _kDADiskAppearedCallback, 0, match, NULL );
}

void DARegisterDiskDescriptionChangedCallback( DASessionRef                     session,
                                               CFDictionaryRef                  match,
                                               CFArrayRef                       watch,
                                               DADiskDescriptionChangedCallback callback,
                                               void *                           context )
{
    _DARegisterCallback( session, callback, context, _kDADiskDescriptionChangedCallback, 0, match, watch );
}

void DARegisterDiskDisappearedCallback( DASessionRef              session,
                                        CFDictionaryRef           match,
                                        DADiskDisappearedCallback callback,
                                        void *                    context )
{
    _DARegisterCallback( session, callback, context, _kDADiskDisappearedCallback, 0, match, NULL );
}

void DARegisterDiskEjectApprovalCallback( DAApprovalSessionRef        session,
                                          CFDictionaryRef             match,
                                          DADiskEjectApprovalCallback callback,
                                          void *                      context )
{
    _DARegisterCallback( ( void * ) session, callback, context, _kDADiskEjectApprovalCallback, 0, match, NULL );
}

void DARegisterDiskPeekCallback( DASessionRef        session,
                                 CFDictionaryRef     match,
                                 CFIndex             order,
                                 DADiskPeekCallback  callback,
                                 void *              context )
{
    _DARegisterCallback( session, callback, context, _kDADiskPeekCallback, order, match, NULL );
}

void DARegisterDiskMountApprovalCallback( DAApprovalSessionRef        session,
                                          CFDictionaryRef             match,
                                          DADiskMountApprovalCallback callback,
                                          void *                      context )
{
    _DARegisterCallback( ( void * ) session, callback, context, _kDADiskMountApprovalCallback, 0, match, NULL );
}

void DARegisterDiskUnmountApprovalCallback( DAApprovalSessionRef          session,
                                            CFDictionaryRef               match,
                                            DADiskUnmountApprovalCallback callback,
                                            void *                        context )
{
    _DARegisterCallback( ( void * ) session, callback, context, _kDADiskUnmountApprovalCallback, 0, match, NULL );
}

void DADiskUnclaim( DADiskRef disk )
{
    _DAServerDiskUnclaim( _DADiskGetSessionID( disk ), _DADiskGetID( disk ) );
}

void DAUnregisterCallback( DASessionRef session, void * callback, void * context )
{
    _DAUnregisterCallback( session, callback, context );
}

void DAUnregisterApprovalCallback( DAApprovalSessionRef session, void * callback, void * context )
{
    _DAUnregisterCallback( ( void * ) session, callback, context );
}
