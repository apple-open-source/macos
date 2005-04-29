/*
 * Copyright (c) 1998-2005 Apple Computer, Inc. All rights reserved.
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

#include "DASession.h"

#include "DACallback.h"
#include "DAServer.h"

#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

struct __DASession
{
    CFRuntimeBase      _base;
    mach_port_t        _client;
    char *             _name;
    pid_t              _pid;
    DASessionOptions   _options;
    CFMutableArrayRef  _queue;
    CFMutableArrayRef  _register;
    AuthorizationRef   _rights;
    CFMachPortRef      _server;
    CFRunLoopSourceRef _source;
    DASessionState     _state;
};

typedef struct __DASession __DASession;

static CFStringRef  __DASessionCopyDescription( CFTypeRef object );
static CFStringRef  __DASessionCopyFormattingDescription( CFTypeRef object, CFDictionaryRef options );
static void         __DASessionDeallocate( CFTypeRef object );
static Boolean      __DASessionEqual( CFTypeRef object1, CFTypeRef object2 );
static CFHashCode   __DASessionHash( CFTypeRef object );

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
                                     CFMachPortGetPort( session->_server ) );
}

static CFStringRef __DASessionCopyFormattingDescription( CFTypeRef object, CFDictionaryRef options )
{
    DASessionRef session = ( DASessionRef ) object;

    return CFStringCreateWithFormat( CFGetAllocator( object ),
                                     NULL,
                                     CFSTR( "%s [%d]:%d" ),
                                     session->_name,
                                     session->_pid,
                                     CFMachPortGetPort( session->_server ) );
}

static DASessionRef __DASessionCreate( CFAllocatorRef allocator )
{
    __DASession * session;

    session = ( void * ) _CFRuntimeCreateInstance( allocator, __kDASessionTypeID, sizeof( __DASession ) - sizeof( CFRuntimeBase ), NULL );

    if ( session )
    {
        session->_client   = MACH_PORT_NULL;
        session->_name     = NULL;
        session->_pid      = 0;
        session->_options  = 0;
        session->_queue    = CFArrayCreateMutable( allocator, 0, &kCFTypeArrayCallBacks );
        session->_register = CFArrayCreateMutable( allocator, 0, &kCFTypeArrayCallBacks );
        session->_rights   = NULL;
        session->_server   = NULL;
        session->_source   = NULL;
        session->_state    = 0;

        assert( session->_queue    );
        assert( session->_register );
    }

    return session;
}

static void __DASessionDeallocate( CFTypeRef object )
{
    DASessionRef session = ( DASessionRef ) object;

    if ( session->_client   )  mach_port_deallocate( mach_task_self( ), session->_client );
    if ( session->_name     )  free( session->_name );
    if ( session->_queue    )  CFRelease( session->_queue );
    if ( session->_register )  CFRelease( session->_register );
    if ( session->_rights   )  AuthorizationFree( session->_rights, kAuthorizationFlagDefaults );
    if ( session->_server   )  CFMachPortInvalidate( session->_server );
    if ( session->_server   )  CFRelease( session->_server );
    if ( session->_source   )  CFRelease( session->_source );
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

    return ( CFHashCode ) CFMachPortGetPort( session->_server );
}

///w:start
const char * _DASessionGetName( DASessionRef session )
{
    return session->_name;
}
///w:stop
DASessionRef DASessionCreate( CFAllocatorRef allocator, mach_port_t _client, const char * _name, pid_t _pid, AuthorizationExternalForm _rights )
{
    DASessionRef session;

    /*
     * Create the session.
     */

    session = __DASessionCreate( allocator );

    if ( session )
    {
        CFMachPortRef     server;
        CFMachPortContext serverContext;

        serverContext.version         = 0;
        serverContext.info            = session;
        serverContext.retain          = NULL;
        serverContext.release         = NULL;
        serverContext.copyDescription = NULL;

        /*
         * Create the session's server port.
         */

        server = CFMachPortCreate( allocator, _DAServerCallback, &serverContext, NULL );

        if ( server )
        {
            CFRunLoopSourceRef source;

            /*
             * Create the session's server port run loop source.
             */

            source = CFMachPortCreateRunLoopSource( allocator, server, 0 );

            if ( source )
            {
                mach_port_t   port;
                kern_return_t status;

                /*
                 * Set up the session's server port.
                 */

                status = mach_port_request_notification( mach_task_self( ),
                                                         _client,
                                                         MACH_NOTIFY_DEAD_NAME,
                                                         TRUE,
                                                         CFMachPortGetPort( server ),
                                                         MACH_MSG_TYPE_MAKE_SEND_ONCE,
                                                         &port );

                if ( status == KERN_SUCCESS )
                {
                    AuthorizationRef rights;

                    assert( port == MACH_PORT_NULL );

                    /*
                     * Create the session's authorization reference.
                     */

                    status = AuthorizationCreateFromExternalForm( &_rights, &rights );

///w:start
                    if ( status )
                    {
                        extern Boolean _gDAAuthorize;

                        if ( _gDAAuthorize == FALSE )
                        {
                            rights = NULL;
                            status = errAuthorizationSuccess;
                        }
                    }
///w:stop
                    if ( status == errAuthorizationSuccess )
                    {
                        session->_client = _client;
                        session->_name   = strdup( _name );
                        session->_pid    = _pid;
                        session->_rights = rights;
                        session->_server = server;
                        session->_source = source;

                        return session;
                    }
                }

                CFRelease( source );
            }

            CFMachPortInvalidate( server );

            CFRelease( server );
        }

        CFRelease( session );
    }

    return NULL;
}

CFMutableArrayRef DASessionGetCallbackQueue( DASessionRef session )
{
    return session->_queue;
}

CFMutableArrayRef DASessionGetCallbackRegister( DASessionRef session )
{
    return session->_register;
}

mach_port_t DASessionGetClientPort( DASessionRef session )
{
    return session->_client;
}

mach_port_t DASessionGetID( DASessionRef session )
{
    return CFMachPortGetPort( session->_server );
}

Boolean DASessionGetOption( DASessionRef session, DASessionOption option )
{
    return ( session->_options & option ) ? TRUE : FALSE;
}

DASessionOptions DASessionGetOptions( DASessionRef session )
{
    return session->_options;
}

AuthorizationRef DASessionGetRights( DASessionRef session )
{
    return session->_rights;
}

mach_port_t DASessionGetServerPort( DASessionRef session )
{
    return CFMachPortGetPort( session->_server );
}

Boolean DASessionGetState( DASessionRef session, DASessionState state )
{
    return ( session->_state & state ) ? TRUE : FALSE;
}

CFTypeID DASessionGetTypeID( void )
{
    return __kDASessionTypeID;
}

void DASessionInitialize( void )
{
    __kDASessionTypeID = _CFRuntimeRegisterClass( &__DASessionClass );
}

void DASessionQueueCallback( DASessionRef session, DACallbackRef callback )
{
    session->_state &= ~kDASessionStateIdle;
    
    CFArrayAppendValue( session->_queue, callback );

    if ( CFArrayGetCount( session->_queue ) == 1 )
    {
        mach_msg_header_t message;
        kern_return_t     status;

        message.msgh_bits        = MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, 0 );
        message.msgh_id          = 0;
        message.msgh_local_port  = MACH_PORT_NULL;
        message.msgh_remote_port = session->_client;
        message.msgh_reserved    = 0;
        message.msgh_size        = sizeof( message );

        status = mach_msg( &message, MACH_SEND_MSG | MACH_SEND_TIMEOUT, message.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL );

        if ( status == MACH_SEND_TIMED_OUT )
        {
            mach_msg_destroy( &message );
        }
    }
}

void DASessionRegisterCallback( DASessionRef session, DACallbackRef callback )
{
    CFArrayAppendValue( session->_register, callback );
}

void DASessionScheduleWithRunLoop( DASessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    CFRunLoopAddSource( runLoop, session->_source, runLoopMode );
}

void DASessionSetOption( DASessionRef session, DASessionOption option, Boolean value )
{
    DASessionSetOptions( session, option, value );
}

void DASessionSetOptions( DASessionRef session, DASessionOptions options, Boolean value )
{
    session->_options &= ~options;
    session->_options |= value ? options : 0;
}

void DASessionSetState( DASessionRef session, DASessionState state, Boolean value )
{
    session->_state &= ~state;
    session->_state |= value ? state : 0;
}

void DASessionUnregisterCallback( DASessionRef session, DACallbackRef callback )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( session->_register );

    for ( index = count - 1; index > -1; index-- )
    {
        DACallbackRef item;

        item = ( void * ) CFArrayGetValueAtIndex( session->_register, index );

        if ( DACallbackGetAddress( item ) == DACallbackGetAddress( callback ) )
        {
            if ( DACallbackGetContext( item ) == DACallbackGetContext( callback ) )
            {
                CFArrayRemoveValueAtIndex( session->_register, index );
            }
        }
    }

    count = CFArrayGetCount( session->_queue );

    for ( index = count - 1; index > -1; index-- )
    {
        DACallbackRef item;

        item = ( void * ) CFArrayGetValueAtIndex( session->_queue, index );

        if ( DACallbackGetAddress( item ) == DACallbackGetAddress( callback ) )
        {
            if ( DACallbackGetContext( item ) == DACallbackGetContext( callback ) )
            {
                CFArrayRemoveValueAtIndex( session->_queue, index );
            }
        }
    }
}

void DASessionUnscheduleFromRunLoop( DASessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    CFRunLoopRemoveSource( runLoop, session->_source, runLoopMode );
}
