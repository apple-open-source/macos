/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
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

#include "DiskArbitration.h"

#include "DiskArbitrationPrivate.h"
#include "DAInternal.h"
#include "DAServer.h"

#include <pthread.h>
#include <unistd.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
#include <Security/Authorization.h>
#endif
#include <os/log.h>

CFDictionaryRef kDADiskDescriptionMatchMediaUnformatted   = NULL;
CFDictionaryRef kDADiskDescriptionMatchMediaWhole         = NULL;
CFDictionaryRef kDADiskDescriptionMatchVolumeMountable    = NULL;
CFDictionaryRef kDADiskDescriptionMatchVolumeUnrecognized = NULL;

CFArrayRef kDADiskDescriptionWatchVolumeName = NULL;
CFArrayRef kDADiskDescriptionWatchVolumePath = NULL;

__private_extern__ char *      _DADiskGetID( DADiskRef disk );
__private_extern__ mach_port_t _DADiskGetSessionID( DADiskRef disk );
__private_extern__ void        _DADiskInitialize( void );
__private_extern__ void        _DADiskSetDescription( DADiskRef disk, CFDictionaryRef description );
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
__private_extern__ AuthorizationRef _DASessionGetAuthorization( DASessionRef session );
#endif
__private_extern__ mach_port_t      _DASessionGetID( DASessionRef session );
__private_extern__ void             _DASessionInitialize( void );

/*
 * Helper functions used by framework for storing callback information in the session's register dictionary
 *
 */
__private_extern__ CFMutableDictionaryRef DACallbackCreate( CFAllocatorRef   allocator,
                                                           mach_vm_offset_t address,
                                                           mach_vm_offset_t context,
                                                           _DACallbackKind kind,
                                                           CFIndex         order,
                                                           CFDataRef match,
                                                           CFDataRef      watch,
                                                           bool block);
__private_extern__ SInt32 DAAddCallbackToSession(DASessionRef session, CFMutableDictionaryRef callback);
__private_extern__ void DARemoveCallbackFromSessionWithKey(DASessionRef session, SInt32 index);
__private_extern__ SInt32 DARemoveCallbackFromSession(DASessionRef session, mach_vm_offset_t address, mach_vm_offset_t context);
__private_extern__ CFMutableDictionaryRef DAGetCallbackFromSession(DASessionRef session, SInt32 index);
__private_extern__ DAReturn _DASessionRecreate( DASessionRef session );
__private_extern__ bool _DASessionIsKeepAlive( DASessionRef session );


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
                                  void *         context,
                                  bool           block )
{
    DAReturn status;

    status = kDAReturnBadArgument;

    if ( session )
    {
        CFDataRef _argument2 = NULL;
        CFDataRef _argument3 = NULL;

        if ( _DASessionGetID( session ) == NULL  && _DASessionIsKeepAlive( session ) )
        {
            if ( _DASessionRecreate (session) != kDAReturnSuccess )
            {
                goto exit;
            }
        }
        
        if ( argument2 )  _argument2 = _DASerialize( kCFAllocatorDefault, argument2 );
        if ( argument3 )  _argument3 = _DASerialize( kCFAllocatorDefault, argument3 );

        /*
         * store the callback and context in the session instead of passing it directly over to the server for security reasons.
         * pass the handle to the callback object to the server which will be used to lookup the correct callback
         */
        CFMutableDictionaryRef   callback =  DACallbackCreate(kCFAllocatorDefault, address, context, UINT32_MAX, NULL, NULL, NULL, block );
        SInt32 index = DAAddCallbackToSession(session, callback);
        CFRelease(callback);
        status = _DAServerSessionQueueRequest( _DASessionGetID( session ),
                                               ( uint32_t                ) kind,
                                               ( caddr_t                ) _DADiskGetID( argument0 ),
                                               ( int32_t                ) argument1,
                                               ( vm_address_t           ) ( _argument2 ? CFDataGetBytePtr( _argument2 ) : 0 ),
                                               ( mach_msg_type_number_t ) ( _argument2 ? CFDataGetLength(  _argument2 ) : 0 ),
                                               ( vm_address_t           ) ( _argument3 ? CFDataGetBytePtr( _argument3 ) : 0 ),
                                               ( mach_msg_type_number_t ) ( _argument3 ? CFDataGetLength(  _argument3 ) : 0 ),
                                               ( uintptr_t              ) index,
                                               ( uintptr_t              ) index );

        if ( _argument2 )  CFRelease( _argument2 );
        if ( _argument3 )  CFRelease( _argument3 );
    }
exit:
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

    if ( _DASessionGetID( session ) == NULL  && _DASessionIsKeepAlive( session ) )
    {
        if ( _DASessionRecreate (session) != kDAReturnSuccess )
        {
            return;
        }
    }
    
    if ( response )  _response = _DASerialize( kCFAllocatorDefault, response );
    

    _DAServerSessionQueueResponse( _DASessionGetID( session ),
                                   ( uintptr_t              ) address,
                                   ( uintptr_t              ) context,
                                   ( uint32_t                ) kind,
                                   ( caddr_t                ) _DADiskGetID( disk ),
                                   ( vm_address_t           ) ( _response ? CFDataGetBytePtr( _response ) : 0 ),
                                   ( mach_msg_type_number_t ) ( _response ? CFDataGetLength(  _response ) : 0 ),
                                   ( int32_t                ) responseID );

    if ( _response )  CFRelease( _response );
}

__private_extern__ DAReturn _DAAuthorize( DASessionRef session, _DAAuthorizeOptions options, DADiskRef disk, const char * right )
{
    DAReturn status;

    status = kDAReturnNotPrivileged;
#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH || TARGET_OS_BRIDGE
    status = kDAReturnSuccess;
#endif

    if ( status )
    {
        if ( ( options & _kDAAuthorizeOptionIsOwner ) )
        {
            uid_t diskUID;

            status = _DAServerDiskGetUserUID( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), &diskUID );

            if ( status )
            {
                return status;
            }

            status = kDAReturnNotPrivileged;

            if ( diskUID == geteuid( ) )
            {
                status = kDAReturnSuccess;
            }
        }
    }

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    if ( status )
    {
        AuthorizationRef authorization;

        authorization = _DASessionGetAuthorization( session );

        if ( authorization )
        {
            CFDictionaryRef description;

            description = DADiskCopyDescription( disk );

            if ( description )
            {
                AuthorizationFlags  flags;
                AuthorizationItem   item;
                char *              name;
                AuthorizationRights rights;

                flags = kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagPreAuthorize;

                if ( CFDictionaryGetValue( description, kDADiskDescriptionVolumeNetworkKey ) == kCFBooleanTrue )
                {
                    asprintf( &name, "system.volume.network.%s", right );
                }
                else
                {
                    CFTypeRef object;

                    object = CFDictionaryGetValue( description, kDADiskDescriptionDeviceProtocolKey );

                    if ( object && CFEqual( object, CFSTR( kIOPropertyPhysicalInterconnectTypeVirtual ) ) )
                    {
                        asprintf( &name, "system.volume.virtual.%s", right );
                    }
                    else
                    {
                        if ( CFDictionaryGetValue( description, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue )
                        {
                            if ( CFDictionaryGetValue( description, kDADiskDescriptionMediaTypeKey ) )
                            {
                                asprintf( &name, "system.volume.optical.%s", right );
                            }
                            else
                            {
                                asprintf( &name, "system.volume.removable.%s", right );
                            }
                        }
                        else
                        {
                            if ( CFDictionaryGetValue( description, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                            {
                                asprintf( &name, "system.volume.internal.%s", right );
                            }
                            else
                            {
                                asprintf( &name, "system.volume.external.%s", right );
                            }
                        }
                    }
                }

                if ( name )
                {
                    item.flags       = 0;
                    item.name        = name;
                    item.value       = NULL;
                    item.valueLength = 0;

                    rights.count = 1;
                    rights.items = &item;

                    status = AuthorizationCopyRights( authorization, &rights, NULL, flags, NULL ); 

                    if ( status )
                    {
                        status = kDAReturnNotPrivileged;
                    }

                    free( name );
                }

                CFRelease( description );
            }
        }
    }
#endif
    return status;
}

static bool DARequestKindWithResponse(_DARequestKind kind)
{
    bool daRequest = false;
    switch ( kind )
    {
        case _kDADiskClaimCallback:
        case _kDADiskEjectCallback:
        case _kDADiskMountCallback:
        case _kDADiskProbeCallback:
        case _kDADiskRenameCallback:
        case _kDADiskUnmountCallback:
        case _kDADiskSetFSKitAdditionsCallback:
        {
            daRequest = true;
            break;
        }
        default:
            break;
    }
    return daRequest;
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
   
    /*
     * address is the handle to the callbacks dictionary stored in the session.
     * Use it as the key to find the actual callback address and context
     */
    SInt32 index = address;

    CFMutableDictionaryRef callback = DAGetCallbackFromSession(session, index);
    if (NULL == callback)
    {
        goto exit;
    }
    address = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackAddressKey );
    context = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackContextKey );
    int block = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackBlockKey );
    
    if (NULL == address)
    {
        goto exit;
    }
    switch ( kind )
    {
        case _kDADiskAppearedCallback:
        {
            if ( block )
            {
                ( ( DADiskAppearedCallbackBlock ) address )( disk );
            }
            else
            {
                ( ( DADiskAppearedCallback ) address )( disk, context );
            }
            

            break;
        }
        case _kDADiskClaimCallback:
        {
            if ( block )
            {
                ( ( DADiskClaimCallbackBlock ) address )( disk, argument1 );
            }
            else
            {
                ( ( DADiskClaimCallback ) address )( disk, argument1, context );
            }

            break;
        }
        case _kDADiskClaimReleaseCallback:
        {
            if ( block )
            {
                response = ( ( DADiskClaimReleaseCallbackBlock ) address )( disk );
            }
            else
            {
                response = ( ( DADiskClaimReleaseCallback ) address )( disk, context );
            }
            response = response ? response : kCFNull;

            break;
        }
        case _kDADiskDescriptionChangedCallback:
        {
            if ( block )
            {
                ( ( DADiskDescriptionChangedCallbackBlock ) address )( disk, argument1 );
            }
            else
            {
                ( ( DADiskDescriptionChangedCallback ) address )( disk, argument1, context );
            }

            break;
        }
        case _kDADiskDisappearedCallback:
        {
            if ( block )
            {
                ( ( DADiskDisappearedCallbackBlock ) address )( disk );
            }
            else
            {
                ( ( DADiskDisappearedCallback ) address )( disk, context );
            }
            
            break;
        }
        case _kDADiskEjectCallback:
        {
            if ( block )
            {
                ( ( DADiskEjectCallbackBlock ) address )( disk, argument1 );
            }
            else
            {
                ( ( DADiskEjectCallback ) address )( disk, argument1, context );
            }
            break;
        }

        case _kDADiskEjectApprovalCallback:
        {
            if ( block )
            {
                response = ( ( DADiskEjectApprovalCallbackBlock ) address )( disk );
            }
            else
            {
                response = ( ( DADiskEjectApprovalCallback ) address )( disk, context );
            }
            
            response = response ? response : kCFNull;
            
            break;
        }

        case _kDADiskMountCallback:
        {
            if ( block )
            {
                ( ( DADiskMountCallbackBlock ) address )( disk, argument1 );
            }
            else
            {
                ( ( DADiskMountCallback ) address )( disk, argument1, context );
            }

            break;
        }

        case _kDADiskMountApprovalCallback:
        {
            if ( block )
            {
                response = ( ( DADiskMountApprovalCallbackBlock ) address )( disk );
            }
            else
            {
                response = ( ( DADiskMountApprovalCallback ) address )( disk, context );
            }

            response = response ? response : kCFNull;
            
            break;
        }

        case _kDADiskPeekCallback:
        {
            if ( block )
            {
                ( ( DADiskPeekCallbackBlock ) address )( disk );
            }
            else
            {
                ( ( DADiskPeekCallback ) address )( disk, context );
                
            }
            
            response = kCFNull;

            break;
        }
        case _kDADiskProbeCallback:
        {
#ifdef DA_FSKIT
            if ( block )
            {
                ( ( DADiskProbeCallbackBlock ) address )( argument1 );
            }
            else
            {
                // This case isn't supported - the only way probe gets issued with a reply is via the block interface
            }
#endif /* DA_FSKIT */

            break;
        }
        case _kDADiskRenameCallback:
        {
            if ( block )
            {
                ( ( DADiskRenameCallbackBlock ) address )( disk, argument1 );
            }
            else
            {
                ( ( DADiskRenameCallback ) address )( disk, argument1, context );
            }

            break;
        }
        case _kDADiskUnmountCallback:
        {
            if ( block )
            {
                ( ( DADiskUnmountCallbackBlock ) address )( disk, argument1 );
            }
            else
            {
                ( ( DADiskUnmountCallback ) address )( disk, argument1, context );
            }

            break;
        }

        case _kDADiskUnmountApprovalCallback:
        {
            if ( block )
            {
                response = ( ( DADiskUnmountApprovalCallbackBlock ) address )( disk );
            }
            else
            {
                response = ( ( DADiskUnmountApprovalCallback ) address )( disk, context );
            }
            
            response = response ? response : kCFNull;
            
            break;
        }

        case _kDAIdleCallback:
        {
            
            if  ( block )
            {
                ( ( DAIdleCallbackBlock ) address )( );
            }
            else
            {
                ( ( DAIdleCallback ) address )( context );
            }
            

            break;
        }
       case _kDADiskListCompleteCallback:
       {
            ( ( DADiskListCompleteCallback ) address )( context );

            break;
       }
        case _kDADiskSetFSKitAdditionsCallback:
#ifdef DA_FSKIT
        {
            ( ( DADiskSetFSKitAdditionsCallbackBlock ) address )( argument1 );
        }
#endif /* DA_FSKIT */

    }

    if ( response )
    {
        SInt32 responseID;

        responseID = ___CFNumberGetIntegerValue( argument1 );

        if ( response == kCFNull )
        {
            response = NULL;
        }

        __DAQueueResponse( session, index, index, kind, disk, response, responseID );

        if ( response )
        {
            CFRelease( response );
        }
    }
 
exit:
    /*
     * Remove the callback address and context from the session's register dict table
     * this is only done for request type kind callbacks like mount, unmount etc.
     */
    if (DARequestKindWithResponse(kind))
    {
        DARemoveCallbackFromSessionWithKey(session, index);
    }

    if ( disk )
    {
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
                                             CFArrayRef      watch,
                                             bool block )
{
    if ( session )
    {
        CFDataRef _match = NULL;
        CFDataRef _watch = NULL;
        
    if ( _DASessionGetID( session ) == NULL  && _DASessionIsKeepAlive( session ) )
    {
        if ( _DASessionRecreate (session) != kDAReturnSuccess )
        {
            return;
        }
    }

        if ( match )  _match = _DASerializeDiskDescription( kCFAllocatorDefault, match );
        if ( watch )  _watch = _DASerialize( kCFAllocatorDefault, watch );
        
        /*
         * store the callback and context in the session instead of passing it directly over to the server for security reasons.
         * pass the handle to the callback object to the server which will be used to lookup the correct callback
         */
        CFMutableDictionaryRef   callback =  DACallbackCreate(kCFAllocatorDefault, address, context, kind, order, _match, _watch, block);
        SInt32 index = DAAddCallbackToSession(session, callback);
        CFRelease(callback);
        _DAServerSessionRegisterCallback( _DASessionGetID( session ),
                                          ( uintptr_t              ) index,
                                          ( uintptr_t              ) index,
                                          ( uint32_t                ) kind,
                                          ( int32_t                ) order,
                                          ( vm_address_t           ) ( _match ? CFDataGetBytePtr( _match ) : 0 ),
                                          ( mach_msg_type_number_t ) ( _match ? CFDataGetLength(  _match ) : 0 ),
                                          ( vm_address_t           ) ( _watch ? CFDataGetBytePtr( _watch ) : 0 ),
                                          ( mach_msg_type_number_t ) ( _watch ? CFDataGetLength(  _watch ) : 0 ) );

        if ( _match )  CFRelease( _match );
        if ( _watch )  CFRelease( _watch );
    }
}

__private_extern__ void _DAUnregisterCallback( DASessionRef session, void * address, void * context )
{
    if ( session )
    {
        /*
         * Remove the callback address and context from the session's register dict table
         * pass the handle to the callback object to the server to unregister the callback
         * since only the keys are passed to the server to avoid security issues.
         */

    if ( _DASessionGetID( session ) == NULL  && _DASessionIsKeepAlive( session ) )
    {
        if ( _DASessionRecreate (session) != kDAReturnSuccess )
        {
            return;
        }
    }

        SInt32 matchingIndex = DARemoveCallbackFromSession(session, address, context);
        _DAServerSessionUnregisterCallback( _DASessionGetID( session ), ( uintptr_t ) matchingIndex, ( uintptr_t ) matchingIndex );
    }
}

__private_extern__ void DADiskClaimCommon ( DADiskRef                  disk,
                        DADiskClaimOptions         options,
                        DADiskClaimReleaseCallback release,
                        void *                     releaseContext,
                        DADiskClaimCallback        callback,
                        void *                     callbackContext,
                        bool                       block )
{
    CFNumberRef _release;
    CFNumberRef _releaseContext;
    DAReturn    status;

    status = kDAReturnBadArgument;

    if ( disk )
    {
        /*
         * store the callback and context in the session instead of passing it directly over to the server for security reasons.
         * pass the handle to the callback object to the server which will be used to lookup the correct callback
         */
        CFMutableDictionaryRef   releaseCallback =  DACallbackCreate(kCFAllocatorDefault, release, releaseContext, UINT32_MAX, NULL, NULL, NULL, block );
        SInt32 index = DAAddCallbackToSession(_DADiskGetSession( disk ), releaseCallback);
        CFRelease(releaseCallback);
        _release        = ___CFNumberCreateWithIntegerValue( kCFAllocatorDefault,  index );
        _releaseContext = ___CFNumberCreateWithIntegerValue( kCFAllocatorDefault,  index );

        status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskClaim, disk, options, _release, _releaseContext, callback, callbackContext, block );

        if ( _release        )  CFRelease( _release        );
        if ( _releaseContext )  CFRelease( _releaseContext );
    }

    if ( status )
    {
        if ( callback )
        {
            DADissenterRef dissenter;

            dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

            if ( block )
            {
                DADiskClaimCallbackBlock callbackBlock = (DADiskClaimCallbackBlock) callback;
                ( callbackBlock )( disk, dissenter );
                Block_release ( callbackBlock );
            }
            else
            {
                ( callback )( disk, dissenter, callbackContext );
            }

            CFRelease( dissenter );
        }
    }
}

void DADiskClaim( DADiskRef                  disk,
                  DADiskClaimOptions         options,
                  DADiskClaimReleaseCallback release,
                  void *                     releaseContext,
                  DADiskClaimCallback        callback,
                  void *                     callbackContext )
{
    DADiskClaimCommon( disk, options, release, releaseContext, callback, callbackContext, false );
}

__private_extern__ void DADiskEjectCommon( DADiskRef disk, DADiskEjectOptions options, DADiskEjectCallback callback, void * context, bool block )
{
    DAReturn status;

    status = kDAReturnBadArgument;

    if ( disk )
    {
        status = _DAAuthorize( _DADiskGetSession( disk ), _kDAAuthorizeOptionIsOwner, disk, _kDAAuthorizeRightUnmount );

        if ( status == kDAReturnSuccess )
        {
            status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskEject, disk, options, NULL, NULL, callback, context, block );
        }
    }

    if ( status )
    {
        if ( callback )
        {
            DADissenterRef dissenter;

            dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

            if ( block )
            {
                DADiskEjectCallbackBlock callbackBlock = (DADiskEjectCallbackBlock) callback;
                ( callbackBlock )(  disk, dissenter );
                Block_release ( callbackBlock );
            }
            else
            {
                ( callback )(  disk, dissenter, context );
            }

            CFRelease( dissenter );
        }
    }
}

void DADiskEject( DADiskRef disk, DADiskEjectOptions options, DADiskEjectCallback callback, void * context )
{
    DADiskEjectCommon(disk, options, callback, context, false );
}

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
DADiskOptions DADiskGetOptions( DADiskRef disk )
{
    int32_t options;

    options = kDADiskOptionDefault;

    if ( disk )
    {
        _DAServerDiskGetOptions( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), &options );
    }

    return options;
}
#endif

Boolean DADiskIsClaimed( DADiskRef disk )
{
    boolean_t claimed;

    claimed = FALSE;

    if ( disk )
    {
        
        DASessionRef session = _DADiskGetSession( disk);
        if ( _DADiskGetSessionID( disk ) == NULL  && _DASessionIsKeepAlive( session ) )
        {
            if ( _DASessionRecreate (session) != kDAReturnSuccess )
            {
                goto exit;
            }
        }

        _DAServerDiskIsClaimed( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), &claimed );
    }
exit:
    return claimed;
}

void DADiskMount( DADiskRef disk, CFURLRef path, DADiskMountOptions options, DADiskMountCallback callback, void * context )
{
    DADiskMountWithArguments( disk, path, options, callback, context, NULL );
}

__private_extern__ void DADiskMountWithArgumentsCommon( DADiskRef           disk,
                               CFURLRef            path,
                               DADiskMountOptions  options,
                               DADiskMountCallback callback,
                               void *              context,
                               CFStringRef         arguments[],
                               bool                block )
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
        CFURLRef id;
        id = CFURLCopyAbsoluteURL( path );
        if ( id )
        {
            path = id;
        }
        else
        {
            CFRetain( path );
        }
    }

    status = kDAReturnBadArgument;

    if ( disk )
    {
        status = _DAAuthorize( _DADiskGetSession( disk ), _kDAAuthorizeOptionIsOwner, disk, _kDAAuthorizeRightMount );

        if ( status == kDAReturnSuccess )
        {
            status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskMount, disk, options, path ? CFURLGetString( path ) : NULL, argument, callback, context, block );
        }
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
        if ( callback )
        {
            DADissenterRef dissenter;

            dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );
            if ( block )
            {
                 DADiskMountCallbackBlock callbackBlock = (DADiskMountCallbackBlock) callback;
                 ( callbackBlock )( disk, dissenter );
                 Block_release ( callbackBlock );
            }
            else
            {
                ( callback )( disk, dissenter, context );
            }

            CFRelease( dissenter );
        }
    }
}

void DADiskMountWithArguments( DADiskRef           disk,
                               CFURLRef            path,
                               DADiskMountOptions  options,
                               DADiskMountCallback callback,
                               void *              context,
                               CFStringRef         arguments[] )
{
    DADiskMountWithArgumentsCommon( disk, path, options, callback, context, arguments, false );
}

__private_extern__ void DADiskRenameCommon( DADiskRef disk, CFStringRef name, DADiskRenameOptions options, DADiskRenameCallback callback, void * context, bool block )
{
    DAReturn status;

    status = kDAReturnBadArgument;

    if ( disk )
    {
        if ( name )
        {
            if ( CFGetTypeID( name ) == CFStringGetTypeID( ) )
            {
                status = _DAAuthorize( _DADiskGetSession( disk ), _kDAAuthorizeOptionIsOwner, disk, _kDAAuthorizeRightRename );

                if ( status == kDAReturnSuccess )
                {
                    status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskRename, disk, options, name, NULL, callback, context, block );
                }
            }
        }
    }

    if ( status )
    {
        if ( callback )
        {
            DADissenterRef dissenter;

            dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

            if ( block )
            {
                DADiskRenameCallbackBlock callbackBlock = (DADiskRenameCallbackBlock) callback;
                ( callbackBlock )(  disk, dissenter );
                Block_release ( callbackBlock );
            }
            else
            {
                ( callback )(  disk, dissenter, context );
            }

            CFRelease( dissenter );
        }
    }
}

#ifdef DA_FSKIT
__private_extern__ void DADiskProbeWithBlockCommon ( DADiskRef                             disk,
                                                    DADiskProbeCallbackBlock __nullable   callback )
{
    DAReturn    status;

    status = kDAReturnBadArgument;

    if ( disk )
    {
        status = __DAQueueRequest( _DADiskGetSession( disk ) , _kDADiskProbe, disk, NULL, NULL, NULL, callback, NULL, true );
    }

    if ( status )
    {
        if ( callback )
        {
            // We know this callback is a block, so access directly
            callback( status );
            Block_release( callback );
        }
    }
}
#endif /* DA_FSKIT */

void DADiskRename( DADiskRef disk, CFStringRef name, DADiskRenameOptions options, DADiskRenameCallback callback, void * context )
{
    DADiskRenameCommon( disk, name, options, callback, context, false );
}
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
DAReturn DADiskSetOptions( DADiskRef disk, DADiskOptions options, Boolean value )
{
    DAReturn status;

    status = kDAReturnBadArgument;

    if ( disk )
    {
        status = _DAServerDiskSetOptions( _DADiskGetSessionID( disk ), _DADiskGetID( disk ), options, value );
    }

    return status;
}
#endif

#if TARGET_OS_OSX || TARGET_OS_IOS
__private_extern__
DAReturn DADiskSetFSKitAdditionsCommon( DADiskRef                             disk,
                                        CFDictionaryRef __nullable             additions,
                                        DADiskSetFSKitAdditionsCallbackBlock   callback )
{
    DAReturn    status;

    status = kDAReturnBadArgument;

    if ( disk )
    {

        status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskSetFSKitAdditions, disk, NULL, additions, NULL, callback, NULL, true );

    }

    if ( status )
    {
        if ( callback )
        {
            // We know this callback is a block, so access directly
            callback( status );
            Block_release( callback );
        }
    }
}

#endif /* TARGET_OS_OSX || TARGET_OS_IOS */

__private_extern__ void DADiskUnmountCommon( DADiskRef disk, DADiskUnmountOptions options, DADiskUnmountCallback callback, void * context, bool block )
{
    DAReturn status;

    status = kDAReturnBadArgument;

    if ( disk )
    {
        status = _DAAuthorize( _DADiskGetSession( disk ), _kDAAuthorizeOptionIsOwner, disk, _kDAAuthorizeRightUnmount );

        if ( status == kDAReturnSuccess )
        {
            status = __DAQueueRequest( _DADiskGetSession( disk ), _kDADiskUnmount, disk, options, NULL, NULL, callback, context, block );
        }
    }

    if ( status )
    {
        if ( callback )
        {
            DADissenterRef dissenter;

            dissenter = DADissenterCreate( kCFAllocatorDefault, status, NULL );

            if ( block )
            {
                DADiskUnmountCallbackBlock callbackBlock = ( DADiskUnmountCallbackBlock ) callback;
                ( callbackBlock )(  disk, dissenter );
                Block_release ( callbackBlock);
            }
            else
            {
                ( callback )(  disk, dissenter, context );
            }

            CFRelease( dissenter );
        }
    }
}

void DADiskUnmount( DADiskRef disk, DADiskUnmountOptions options, DADiskUnmountCallback callback, void * context )
{
    DADiskUnmountCommon( disk, options, callback, context, false );
}

void DARegisterDiskAppearedCallback( DASessionRef           session,
                                     CFDictionaryRef        match,
                                     DADiskAppearedCallback callback,
                                     void *                 context )
{
    _DARegisterCallback( session, callback, context, _kDADiskAppearedCallback, 0, match, NULL, false );
}

void DARegisterDiskDescriptionChangedCallback( DASessionRef                     session,
                                               CFDictionaryRef                  match,
                                               CFArrayRef                       watch,
                                               DADiskDescriptionChangedCallback callback,
                                               void *                           context )
{
    _DARegisterCallback( session, callback, context, _kDADiskDescriptionChangedCallback, 0, match, watch, false );
}

void DARegisterDiskDisappearedCallback( DASessionRef              session,
                                        CFDictionaryRef           match,
                                        DADiskDisappearedCallback callback,
                                        void *                    context )
{
    _DARegisterCallback( session, callback, context, _kDADiskDisappearedCallback, 0, match, NULL, false );
}

void DARegisterDiskEjectApprovalCallback( DASessionRef                session,
                                          CFDictionaryRef             match,
                                          DADiskEjectApprovalCallback callback,
                                          void *                      context )
{
    _DARegisterCallback( session, callback, context, _kDADiskEjectApprovalCallback, 0, match, NULL, false );
}

void DARegisterDiskPeekCallback( DASessionRef        session,
                                 CFDictionaryRef     match,
                                 CFIndex             order,
                                 DADiskPeekCallback  callback,
                                 void *              context )
{
    _DARegisterCallback( session, callback, context, _kDADiskPeekCallback, order, match, NULL, false );
}

void DARegisterDiskMountApprovalCallback( DASessionRef                session,
                                          CFDictionaryRef             match,
                                          DADiskMountApprovalCallback callback,
                                          void *                      context )
{
    _DARegisterCallback( session, callback, context, _kDADiskMountApprovalCallback, 0, match, NULL, false );
}

void DARegisterDiskUnmountApprovalCallback( DASessionRef                  session,
                                            CFDictionaryRef               match,
                                            DADiskUnmountApprovalCallback callback,
                                            void *                        context )
{
    _DARegisterCallback( session, callback, context, _kDADiskUnmountApprovalCallback, 0, match, NULL, false );
}


void DADiskUnclaim( DADiskRef disk )
{
    if ( disk )
    {
        DASessionRef session = _DADiskGetSession( disk);
        if ( _DADiskGetSessionID( disk ) == NULL  && _DASessionIsKeepAlive( session ) )
        {
            if ( _DASessionRecreate (session) != kDAReturnSuccess )
            {
                return;
            }
        }
        _DAServerDiskUnclaim( _DADiskGetSessionID( disk ), _DADiskGetID( disk ) );
    }
}

void DAUnregisterCallback( DASessionRef session, void * callback, void * context )
{
    _DAUnregisterCallback( session, callback, context );
}

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
void DAUnregisterApprovalCallback( DASessionRef session, void * callback, void * context )
{
    _DAUnregisterCallback( session, callback, context );
}
#endif
