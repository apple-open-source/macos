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

#include "DASession.h"

#include "DADisk.h"
#include "DAInternal.h"
#include "DAServerUser.h"

#include <crt_externs.h>
#include <libgen.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#include <servers/bootstrap.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <Security/Authorization.h>

struct __DASession
{
    CFRuntimeBase      _base;
    CFMachPortRef      _client;
    char *             _name;
    pid_t              _pid;
    AuthorizationRef   _rights;
    mach_port_t        _server;
    CFRunLoopSourceRef _source;
};

typedef struct __DASession __DASession;

static CFStringRef __DASessionCopyDescription( CFTypeRef object );
static CFStringRef __DASessionCopyFormattingDescription( CFTypeRef object, CFDictionaryRef );
static void        __DASessionDeallocate( CFTypeRef object );
static Boolean     __DASessionEqual( CFTypeRef object1, CFTypeRef object2 );
static CFHashCode  __DASessionHash( CFTypeRef object );

static const CFRuntimeClass __DASessionClass =
{
    0,
    "DASession",
    NULL,
    NULL,
    __DASessionDeallocate,
    __DASessionEqual,
    __DASessionHash,
    __DASessionCopyFormattingDescription,
    __DASessionCopyDescription
};

static CFTypeID __kDASessionTypeID = _kCFRuntimeNotATypeID;

const CFStringRef kDAApprovalRunLoopMode = CFSTR( "kDAApprovalRunLoopMode" );

__private_extern__ void _DADispatchCallback( DASessionRef    session,
                                             void *          address,
                                             void *          context,
                                             _DACallbackKind kind,
                                             CFTypeRef       argument0,
                                             CFTypeRef       argument1 );

__private_extern__ void _DAInitialize( void );

static CFStringRef __DASessionCopyDescription( CFTypeRef object )
{
    DASessionRef session = ( DASessionRef ) object;

    return CFStringCreateWithFormat( CFGetAllocator( object ),
                                     NULL,
                                     CFSTR( "<DASession %p [%p]>{id = %s [%d]:%d}" ),
                                     object,
                                     CFGetAllocator( object ),
                                     session->_name,
                                     session->_pid,
                                     session->_server );
}

static CFStringRef __DASessionCopyFormattingDescription( CFTypeRef object, CFDictionaryRef options )
{
    DASessionRef session = ( DASessionRef ) object;

    return CFStringCreateWithFormat( CFGetAllocator( object ),
                                     NULL,
                                     CFSTR( "%s [%d]:%d" ),
                                     session->_name,
                                     session->_pid,
                                     session->_server );
}

static DASessionRef __DASessionCreate( CFAllocatorRef allocator )
{
    __DASession * session;

    session = ( void * ) _CFRuntimeCreateInstance( allocator, __kDASessionTypeID, sizeof( __DASession ) - sizeof( CFRuntimeBase ), NULL );

    if ( session )
    {
        session->_client = NULL;
        session->_name   = NULL;
        session->_pid    = 0;
        session->_rights = NULL;
        session->_server = NULL;
        session->_source = NULL;
    }

    return session;
}

static void __DASessionDeallocate( CFTypeRef object )
{
    DASessionRef session = ( DASessionRef ) object;

    if ( session->_client )  CFMachPortInvalidate( session->_client );
    if ( session->_client )  CFRelease( session->_client );
    if ( session->_name   )  free( session->_name );
    if ( session->_rights )  AuthorizationFree( session->_rights, kAuthorizationFlagDefaults );
    if ( session->_server )  mach_port_deallocate( mach_task_self( ), session->_server );
    if ( session->_source )  CFRelease( session->_source );
}

static Boolean __DASessionEqual( CFTypeRef object1, CFTypeRef object2 )
{
    DASessionRef session1 = ( DASessionRef ) object1;
    DASessionRef session2 = ( DASessionRef ) object2;

    return ( session1->_server == session2->_server ) ? TRUE : FALSE;
}

static CFHashCode __DASessionHash( CFTypeRef object )
{
    DASessionRef session = ( DASessionRef ) object;

    return ( CFHashCode ) session->_server;
}

__private_extern__ void _DASessionCallback( CFMachPortRef port, void * message, CFIndex messageSize, void * info )
{
    vm_address_t           _queue;
    mach_msg_type_number_t _queueSize;
    DASessionRef           session = info;
    kern_return_t          status;

    status = _DAServerSessionCopyCallbackQueue( session->_server, &_queue, &_queueSize );

    if ( status == KERN_SUCCESS )
    {
        CFArrayRef queue;

        queue = _DAUnserializeWithBytes( CFGetAllocator( session ), _queue, _queueSize );

        if ( queue )
        {
            CFIndex count;
            CFIndex index;

            count = CFArrayGetCount( queue );

            for ( index = 0; index < count; index++ )
            {
                CFDictionaryRef callback;
                
                callback = CFArrayGetValueAtIndex( queue, index );

                if ( callback )
                {
                    void * address;
                    void * context;

                    CFTypeRef argument0;
                    CFTypeRef argument1;

                    address = ( void * ) ( vm_offset_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackAddressKey );
                    context = ( void * ) ( vm_offset_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackContextKey );

                    argument0 = CFDictionaryGetValue( callback, _kDACallbackArgument0Key );
                    argument1 = CFDictionaryGetValue( callback, _kDACallbackArgument1Key );
                    
                    _DADispatchCallback( session, address, context, ___CFDictionaryGetIntegerValue( callback, _kDACallbackKindKey ), argument0, argument1 );
                }
            }

            CFRelease( queue );
        }

        vm_deallocate( mach_task_self( ), _queue, _queueSize );
    }
}

__private_extern__ mach_port_t _DASessionGetClientPort( DASessionRef session )
{
    return CFMachPortGetPort( session->_client );
}

__private_extern__ mach_port_t _DASessionGetID( DASessionRef session )
{
    return session->_server;
}

__private_extern__ AuthorizationRef _DASessionGetRights( DASessionRef session )
{
    return session->_rights;
}

__private_extern__ void _DASessionInitialize( void )
{
    __kDASessionTypeID = _CFRuntimeRegisterClass( &__DASessionClass );
}

DAApprovalSessionRef DAApprovalSessionCreate( CFAllocatorRef allocator )
{
    return ( void * ) DASessionCreate( allocator );
}

CFTypeID DAApprovalSessionGetTypeID( void )
{
    return DASessionGetTypeID( );
}

void DAApprovalSessionScheduleWithRunLoop( DAApprovalSessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    DASessionScheduleWithRunLoop( ( void * ) session, runLoop, kDAApprovalRunLoopMode );

    DASessionScheduleWithRunLoop( ( void * ) session, runLoop, runLoopMode );
}

void DAApprovalSessionUnscheduleFromRunLoop( DAApprovalSessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    DASessionUnscheduleFromRunLoop( ( void * ) session, runLoop, kDAApprovalRunLoopMode );

    DASessionUnscheduleFromRunLoop( ( void * ) session, runLoop, runLoopMode );
}

DASessionRef DASessionCreate( CFAllocatorRef allocator )
{
    mach_port_t   bootstrapPort;
    mach_port_t   masterPort;
    kern_return_t status;

    /*
     * Initialize the Disk Arbitration framework.
     */

    _DAInitialize( );
    
    /*
     * Obtain the bootstrap port.
     */

    status = task_get_bootstrap_port( mach_task_self( ), &bootstrapPort );

    if ( status == KERN_SUCCESS )
    {
        /*
         * Obtain the Disk Arbitration master port.
         */

        status = bootstrap_look_up( bootstrapPort, _kDAServiceName, &masterPort );

        if ( status == KERN_SUCCESS )
        {
            DASessionRef session;

            /*
             * Create the session.
             */

            session = __DASessionCreate( allocator );

            if ( session )
            {
                CFMachPortRef     client;
                CFMachPortContext clientContext;

                clientContext.version         = 0;
                clientContext.info            = session;
                clientContext.retain          = NULL;
                clientContext.release         = NULL;
                clientContext.copyDescription = NULL;

                /*
                 * Create the session's client port.
                 */

                client = CFMachPortCreate( allocator, _DASessionCallback, &clientContext, NULL );

                if ( client )
                {
                    CFRunLoopSourceRef source;

                    /*
                     * Create the session's client port run loop source.
                     */

                    source = CFMachPortCreateRunLoopSource( allocator, client, 0 );

                    if ( source )
                    {
                        mach_port_limits_t limits = { 0 };

                        limits.mpl_qlimit = 1;

                        /*
                         * Set up the session's client port.
                         */

                        status = mach_port_set_attributes( mach_task_self( ),
                                                           CFMachPortGetPort( client ),
                                                           MACH_PORT_LIMITS_INFO,
                                                           ( mach_port_info_t ) &limits,
                                                           MACH_PORT_LIMITS_INFO_COUNT );

                        if ( status == KERN_SUCCESS )
                        {
                            AuthorizationRef rights;

                            /*
                             * Create the session's authorization reference.
                             */

                            status = AuthorizationCreate( NULL, NULL, kAuthorizationFlagDefaults, &rights );

///w:start
                            if ( status )
                            {
                                rights = NULL;
                                status = errAuthorizationSuccess;
                            }
///w:stop
                            if ( status == errAuthorizationSuccess )
                            {
                                AuthorizationExternalForm _rights;

                                /*
                                 * Create the session's authorization reference representation.
                                 */

///w:start
                                if ( rights == NULL )
                                {
                                    bzero( &_rights, sizeof( _rights ) );
                                }
                                else
///w:stop
                                status = AuthorizationMakeExternalForm( rights, &_rights );

                                if ( status == errAuthorizationSuccess )
                                {
                                    mach_port_t server;

                                    /*
                                     * Create the session at the server.
                                     */

                                    status = _DAServerSessionCreate( masterPort,
                                                                     CFMachPortGetPort( client ),
                                                                     basename( _dyld_get_image_name( 0 ) ),
                                                                     getpid( ),
                                                                     _rights,
                                                                     &server );

                                    if ( status == KERN_SUCCESS )
                                    {
                                        session->_client = client;
                                        session->_name   = strdup( basename( _dyld_get_image_name( 0 ) ) );
                                        session->_pid    = getpid( );
                                        session->_rights = rights;
                                        session->_server = server;
                                        session->_source = source;

                                        return session;
                                    }
                                }

///w:start
                                if ( rights )
///w:stop
                                AuthorizationFree( rights, kAuthorizationFlagDefaults );
                            }
                        }

                        CFRelease( source );
                    }

                    CFRelease( client );
                }

                CFRelease( session );
            }

            mach_port_deallocate( mach_task_self( ), masterPort );
        }

        mach_port_deallocate( mach_task_self( ), bootstrapPort );
    }

    return NULL;
}

CFTypeID DASessionGetTypeID( void )
{
    return __kDASessionTypeID;
}

void DASessionScheduleWithRunLoop( DASessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    CFRunLoopAddSource( runLoop, session->_source, runLoopMode );
}

void DASessionUnscheduleFromRunLoop( DASessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    CFRunLoopRemoveSource( runLoop, session->_source, runLoopMode );
}
