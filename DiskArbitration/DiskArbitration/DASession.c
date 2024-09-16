/*
 * Copyright (c) 1998-2015 Apple Inc. All rights reserved.
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

#include "DADisk.h"
#include "DAInternal.h"
#include "DAServer.h"
#include "DiskArbitrationPrivate.h"

#include <bootstrap_priv.h>
#include <crt_externs.h>
#include <libgen.h>
#include <pthread.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <servers/bootstrap.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
#include <Security/Authorization.h>
#endif
#include <os/log.h>
#include <libkern/OSAtomic.h>
#include <notify.h>

struct __DASession
{
    CFRuntimeBase           _base;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    AuthorizationRef        _authorization;
#endif
    CFMachPortRef           _client;
    char *                  _name;
    pid_t                    _pid;
    mach_port_t             _server;
    CFRunLoopSourceRef      _source;
    dispatch_source_t       _source2;
    UInt32                  _sourceCount;
    CFMutableDictionaryRef  _register;
    SInt32                  _registerIndex;
    pthread_mutex_t         _registerLock;
    bool                    _keepAlive;
    int                     _token;
    dispatch_queue_t        _queue;
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

static pthread_mutex_t __gDASessionSetAuthorizationLock = PTHREAD_MUTEX_INITIALIZER;

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
const CFStringRef kDAApprovalRunLoopMode = CFSTR( "kDAApprovalRunLoopMode" );
#endif

static uint32_t           sessionCount = 0;

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
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        session->_authorization = NULL;
#endif
        session->_client        = NULL;
        session->_name          = NULL;
        session->_pid           = 0;
        session->_server        = MACH_PORT_NULL;
        session->_source        = NULL;
        session->_source2       = NULL;
        session->_sourceCount   = 0;
        session->_register      = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
        session->_registerIndex = 0;
        session->_keepAlive     = false;
        session->_token         = -1;
        pthread_mutex_init( &session->_registerLock, NULL );

        assert( session->_register );
        
        uint32_t newSessionCount = OSAtomicIncrement32( &sessionCount );
        if ( 0 == ( newSessionCount % 1000 ) )
        {
            os_log_fault(OS_LOG_DEFAULT,"Now using %d DASessionRef objects", newSessionCount);
        }
        
    }

    return session;
}

static void __DASessionReleaseCallbackBlocks( DASessionRef session )
{
    CFIndex count = CFDictionaryGetCount( session->_register );
    const void** keys = ( const void** ) malloc( sizeof(void*)*count );
    const void** values = ( const void** ) malloc( sizeof(void*)*count );
    SInt32 queueIndex;

    CFDictionaryGetKeysAndValues( session->_register, keys, values );
    for ( queueIndex = 0; queueIndex < count; queueIndex++ )
    {
        CFMutableDictionaryRef callback = ( CFMutableDictionaryRef )( values[queueIndex] );
        void *currentaddress = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackAddressKey );
        int block = ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackBlockKey );
        if ( block )
        {
            Block_release ( currentaddress );
        }
    }
    free ( keys );
    free ( values );
}

static void __DASessionDeallocate( CFTypeRef object )
{
    DASessionRef session = ( DASessionRef ) object;

    assert( session->_client  == NULL );
    assert( session->_source  == NULL );
    assert( session->_source2 == NULL );

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    if ( session->_authorization )  AuthorizationFree( session->_authorization, kAuthorizationFlagDefaults );
#endif
    if ( session->_name          )  free( session->_name );
    if ( session->_token  != -1  )  notify_cancel( session->_token );
    if ( session->_server        )  mach_port_deallocate( mach_task_self( ), session->_server );
   
    if ( session->_register )
    {
        __DASessionReleaseCallbackBlocks( session );
        CFRelease( session->_register );
    }
    OSAtomicDecrement32( &sessionCount );
    pthread_mutex_destroy( &session->_registerLock );
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

                    address = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackAddressKey );
                    context = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackContextKey );

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
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
__private_extern__ AuthorizationRef _DASessionGetAuthorization( DASessionRef session )
{
    AuthorizationRef authorization;

    pthread_mutex_lock( &__gDASessionSetAuthorizationLock );

    authorization = session->_authorization;

    if ( authorization == NULL )
    {
        kern_return_t status;

        /*
         * Create the session's authorization reference.
         */

        status = AuthorizationCreate( NULL, NULL, kAuthorizationFlagDefaults, &authorization );

        if ( status == errAuthorizationSuccess )
        {
            AuthorizationExternalForm _authorization;

            /*
             * Create the session's authorization reference representation.
             */

            status = AuthorizationMakeExternalForm( authorization, &_authorization );

            if ( status == errAuthorizationSuccess )
            {
                _DAServerSessionSetAuthorization( session->_server, _authorization );

                session->_authorization = authorization;
            }
            else
            {
                AuthorizationFree( authorization, kAuthorizationFlagDefaults );

                authorization = NULL;
            }
        }
    }

    pthread_mutex_unlock( &__gDASessionSetAuthorizationLock );

    return authorization;
}
#endif
#ifndef __LP64__

__private_extern__ mach_port_t _DASessionGetClientPort( DASessionRef session )
{
    mach_port_t client;

    client = MACH_PORT_NULL;

    if ( session->_client )
    {
        client = CFMachPortGetPort( session->_client );
    }

    return client;
}

#endif /* !__LP64__ */

__private_extern__ mach_port_t _DASessionGetID( DASessionRef session )
{
    return session->_server;
}

__private_extern__ bool _DASessionIsKeepAlive( DASessionRef session )
{
    return ( session->_keepAlive == true );
}

__private_extern__ void _DASessionInitialize( void )
{
    __kDASessionTypeID = _CFRuntimeRegisterClass( &__DASessionClass );
}

__private_extern__ void _DASessionScheduleWithRunLoop( DASessionRef session )
{
    session->_sourceCount++;

    if ( session->_sourceCount == 1 )
    {
        mach_port_t   clientPort;
        kern_return_t status;

        /*
         * Create the session's client port.
         */

        status = mach_port_allocate( mach_task_self( ), MACH_PORT_RIGHT_RECEIVE, &clientPort );

        if ( status == KERN_SUCCESS )
        {
            mach_port_limits_t limits = { 0 };

            limits.mpl_qlimit = 1;

            /*
             * Set up the session's client port.  It requires no more than one queue element.
             */

            status = mach_port_set_attributes( mach_task_self( ),
                                               clientPort,
                                               MACH_PORT_LIMITS_INFO,
                                               ( mach_port_info_t ) &limits,
                                               MACH_PORT_LIMITS_INFO_COUNT );

            if ( status == KERN_SUCCESS )
            {
                CFMachPortRef     client;
                CFMachPortContext clientContext;

                clientContext.version         = 0;
                clientContext.info            = session;
                clientContext.retain          = CFRetain;
                clientContext.release         = CFRelease;
                clientContext.copyDescription = NULL;

                /*
                 * Create the session's client port run loop source.
                 */

                client = CFMachPortCreateWithPort( CFGetAllocator( session ), clientPort, _DASessionCallback, &clientContext, NULL );

                if ( client )
                {
                    CFRunLoopSourceRef source;

                    source = CFMachPortCreateRunLoopSource( CFGetAllocator( session ), client, 0 );

                    if ( source )
                    {
                        session->_client = client;
                        session->_source = source;

                        _DAServerSessionSetClientPort( session->_server, CFMachPortGetPort( client ) );

                        return;
                    }

                    CFMachPortInvalidate( client );

                    CFRelease( client );
                }
            }

            mach_port_mod_refs( mach_task_self( ), clientPort, MACH_PORT_RIGHT_RECEIVE, -1 );
        }
    }
}

__private_extern__ void _DASessionUnscheduleFromRunLoop( DASessionRef session )
{
    if ( session->_sourceCount == 1 )
    {
        if ( session->_source )
        {
            CFRunLoopSourceInvalidate( session->_source );

            CFRelease( session->_source );

            session->_source = NULL;
        }

        if ( session->_client )
        {
            mach_port_t clientPort;

            clientPort = CFMachPortGetPort( session->_client );

            CFMachPortInvalidate( session->_client );

            CFRelease( session->_client );

            mach_port_mod_refs( mach_task_self( ), clientPort, MACH_PORT_RIGHT_RECEIVE, -1 );

            session->_client = NULL;
        }
    }

    if ( session->_sourceCount )
    {
        session->_sourceCount--;
    }
}

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
DAApprovalSessionRef DAApprovalSessionCreate( CFAllocatorRef allocator )
{
    return DASessionCreate( allocator );
}
#endif

CFTypeID DAApprovalSessionGetTypeID( void )
{
    return DASessionGetTypeID( );
}

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
void DAApprovalSessionScheduleWithRunLoop( DAApprovalSessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    DASessionScheduleWithRunLoop( session, runLoop, kDAApprovalRunLoopMode );

    DASessionScheduleWithRunLoop( session, runLoop, runLoopMode );
}


void DAApprovalSessionUnscheduleFromRunLoop( DAApprovalSessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    DASessionUnscheduleFromRunLoop( session, runLoop, kDAApprovalRunLoopMode );

    DASessionUnscheduleFromRunLoop( session, runLoop, runLoopMode );
}
#endif


static bool DASessionEstablish(DASessionRef session  )
{
    if ( session )
    {
        mach_port_t   bootstrapPort;
        kern_return_t status;

        /*
         * Obtain the bootstrap port.
         */

        status = task_get_bootstrap_port( mach_task_self( ), &bootstrapPort );

        if ( status == KERN_SUCCESS )
        {
            mach_port_t masterPort;

            /*
             * Obtain the Disk Arbitration master port.
             */

            status = bootstrap_look_up2( bootstrapPort, _kDADaemonName, &masterPort, 0, BOOTSTRAP_PRIVILEGED_SERVER );

            mach_port_deallocate( mach_task_self( ), bootstrapPort );

            if ( status == KERN_SUCCESS )
            {
                mach_port_t server;

                /*
                 * Create the session at the server.
                 */

                status = _DAServerSessionCreate( masterPort,
                                                 basename( ( char * ) _dyld_get_image_name( 0 ) ),
                                                 &server );

                mach_port_deallocate( mach_task_self( ), masterPort );

                if ( status == KERN_SUCCESS )
                {
                    session->_name   = strdup( basename( ( char * ) _dyld_get_image_name( 0 ) ) );
                    session->_pid    = getpid( );
                    session->_server = server;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
///w:start
if ( strcmp( session->_name, "SystemUIServer" ) == 0 )
{
    _DASessionGetAuthorization( session );
}
///w:stop
#endif
                    return true;
                }
            }
        }
    }
    return false;
}

DASessionRef DASessionCreate( CFAllocatorRef allocator )
{
    DASessionRef session;

    /*
     * Initialize the Disk Arbitration framework.
     */

    _DAInitialize( );

    /*
     * Create the session.
     */

    session = __DASessionCreate( allocator );

    if ( session )
    {
        mach_port_t   bootstrapPort;
        kern_return_t status;

        /*
         * Obtain the bootstrap port.
         */

        status = task_get_bootstrap_port( mach_task_self( ), &bootstrapPort );

        if ( status == KERN_SUCCESS )
        {
            mach_port_t masterPort;

            /*
             * Obtain the Disk Arbitration master port.
             */

            status = bootstrap_look_up2( bootstrapPort, _kDADaemonName, &masterPort, 0, BOOTSTRAP_PRIVILEGED_SERVER );

            mach_port_deallocate( mach_task_self( ), bootstrapPort );

            if ( status == KERN_SUCCESS )
            {
                mach_port_t server;

                /*
                 * Create the session at the server.
                 */

                status = _DAServerSessionCreate( masterPort,
                                                 basename( ( char * ) _dyld_get_image_name( 0 ) ),
                                                 &server );

                mach_port_deallocate( mach_task_self( ), masterPort );

                if ( status == KERN_SUCCESS )
                {
                    session->_name   = strdup( basename( ( char * ) _dyld_get_image_name( 0 ) ) );
                    session->_pid    = getpid( );
                    session->_server = server;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
///w:start
if ( strcmp( session->_name, "SystemUIServer" ) == 0 )
{
    _DASessionGetAuthorization( session );
}
///w:stop
#endif
                    return session;
                }
            }
        }

        CFRelease( session );
    }

    return NULL;
}

CFTypeID DASessionGetTypeID( void )
{
    _DAInitialize( );

    return __kDASessionTypeID;
}

void DASessionScheduleWithRunLoop( DASessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    if ( session )
    {
        _DASessionScheduleWithRunLoop( session );

        if ( session->_source )
        {
            CFRunLoopAddSource( runLoop, session->_source, runLoopMode );
        }
    }
}

void DASessionSetDispatchQueue( DASessionRef session, dispatch_queue_t queue )
{
    if ( session )
    {
        if ( session->_source2 )
        {
            dispatch_source_cancel( session->_source2 );

            dispatch_release( session->_source2 );

            session->_source2 = NULL;
        }

        if ( queue )
        {
            mach_port_t   client;
            kern_return_t status;

            /*
             * Create the session's client port.
             */

            status = mach_port_allocate( mach_task_self( ), MACH_PORT_RIGHT_RECEIVE, &client );

            if ( status == KERN_SUCCESS )
            {
                mach_port_limits_t limits = { 0 };

                limits.mpl_qlimit = 1;

                /*
                 * Set up the session's client port.  It requires no more than one queue element.
                 */

                status = mach_port_set_attributes( mach_task_self( ),
                                                   client,
                                                   MACH_PORT_LIMITS_INFO,
                                                   ( mach_port_info_t ) &limits,
                                                   MACH_PORT_LIMITS_INFO_COUNT );

                if ( status == KERN_SUCCESS )
                {
                    dispatch_source_t source;

                    /*
                     * Create the session's client port dispatch source.
                     */

                    source = dispatch_source_create( DISPATCH_SOURCE_TYPE_MACH_RECV, client, 0, queue );

                    if ( source )
                    {
                        session->_source2 = source;

                        CFRetain( session );
                        mach_port_t port;
                        
                        status = mach_port_request_notification( mach_task_self( ),
                                                                client,
                                                                 MACH_NOTIFY_NO_SENDERS,
                                                                 1,
                                                                client,
                                                                 MACH_MSG_TYPE_MAKE_SEND_ONCE,
                                                                 &port );
                        if ( status == KERN_SUCCESS )
                        {

                            dispatch_source_set_cancel_handler( session->_source2, ^
                                                               {
                                mach_port_mod_refs( mach_task_self( ), client, MACH_PORT_RIGHT_RECEIVE, -1 );

                                CFRelease( session );
                            } );

                            dispatch_source_set_event_handler( session->_source2, ^
                                                              {
                                mach_msg_empty_rcv_t message;
                                mach_msg( ( void * ) &message, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof( message ), client, 0, MACH_PORT_NULL );
                                if (MACH_NOTIFY_NO_SENDERS == message.header.msgh_id)
                                {
                                    os_log(OS_LOG_DEFAULT, "diskarbitrationd exited.");
                                    if ( session->_name )  free( session->_name );
                                    session->_name = NULL;
                                    if ( session->_server )  mach_port_deallocate( mach_task_self( ), session->_server );
                                    session->_server        = MACH_PORT_NULL;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
                                    if ( session->_authorization )  AuthorizationFree( session->_authorization, kAuthorizationFlagDefaults );
                                    session->_authorization = NULL;
#endif
                                }
                                else
                                {
                                    _DASessionCallback( NULL, NULL, 0, session );
                                }
                                
                            } );

                            dispatch_resume( session->_source2 );

                            _DAServerSessionSetClientPort( session->_server, client );

                            return;
                        }
                    }
                }

                mach_port_mod_refs( mach_task_self( ), client, MACH_PORT_RIGHT_RECEIVE, -1 );
            }
        }
    }
}

void DASessionUnscheduleFromRunLoop( DASessionRef session, CFRunLoopRef runLoop, CFStringRef runLoopMode )
{
    if ( session )
    {
        if ( session->_source )
        {
            CFRunLoopRemoveSource( runLoop, session->_source, runLoopMode );
        }

        _DASessionUnscheduleFromRunLoop( session );
    }
}

__private_extern__ CFMutableDictionaryRef DACallbackCreate( CFAllocatorRef   allocator,
                                mach_vm_offset_t address,
                                mach_vm_offset_t context,
                                _DACallbackKind kind,
                                CFIndex         order,
                                CFDataRef match,
                                CFDataRef      watch,
                                bool block )
{
    CFMutableDictionaryRef callback;

    callback = CFDictionaryCreateMutable( allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    if ( callback )
    {
        ___CFDictionarySetIntegerValue( callback, _kDACallbackAddressKey, address );
        ___CFDictionarySetIntegerValue( callback, _kDACallbackContextKey, context );
        ___CFDictionarySetIntegerValue( callback, _kDACallbackKindKey,    kind    );
        ___CFDictionarySetIntegerValue( callback, _kDACallbackOrderKey,   order   );
        ___CFDictionarySetIntegerValue( callback, _kDACallbackBlockKey,   block   );

        if ( match )  CFDictionarySetValue( callback, _kDACallbackMatchKey, match );
        if ( watch )  CFDictionarySetValue( callback, _kDACallbackWatchKey, watch );
    }

    return  callback;
}

__private_extern__ SInt32 DAAddCallbackToSession(DASessionRef session, CFMutableDictionaryRef callback)
{
    SInt32 currentIndex = 0;

    /*
     * Add the callback dict object to the session's register queue
     */
    
    if ( session )
    { 
        pthread_mutex_lock( &session->_registerLock );
        currentIndex = ++session->_registerIndex;
        CFNumberRef cfnumber = CFNumberCreate( NULL, kCFNumberSInt32Type, &currentIndex );

        while (CFDictionaryContainsKey( session->_register, cfnumber ))
        {
            currentIndex = ++session->_registerIndex;
            
            /*
             * skip 0 as key since it is being used as callback address to server and the server will disregard NULL values.
             */
            
            if ( 0 == currentIndex )
            {
                currentIndex = ++session->_registerIndex;
            }
            CFRelease( cfnumber );
            cfnumber = CFNumberCreate( NULL, kCFNumberSInt32Type, &currentIndex );
        }

        CFDictionarySetValue( session->_register, cfnumber, callback );
        CFRelease( cfnumber );
        pthread_mutex_unlock( &session->_registerLock );
    }
    return currentIndex;
}

__private_extern__ void DARemoveCallbackFromSessionWithKey(DASessionRef session, SInt32 index)
{
    /*
     * Remove the callback dict object from the session's register queue
     */
    SInt32 matchingKey = 0;
    
    if ( session )
    {
        CFNumberRef number = CFNumberCreate( NULL, kCFNumberSInt32Type, &index );
        pthread_mutex_lock( &session->_registerLock );
        CFMutableDictionaryRef callback =  ( CFMutableDictionaryRef ) CFDictionaryGetValue( session->_register , number );
        int block = ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackBlockKey );
        void *currentaddress = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackAddressKey );
        if ( block )
        {
            Block_release ( currentaddress );
        }
        CFDictionaryRemoveValue( session->_register , number );
        CFNumberGetValue( number, kCFNumberSInt32Type, &matchingKey );
        session->_registerIndex = matchingKey - 1;
        pthread_mutex_unlock( &session->_registerLock );
        CFRelease( number );
    }
}

__private_extern__ SInt32 DARemoveCallbackFromSession(DASessionRef session, mach_vm_offset_t address,
                                mach_vm_offset_t context)
{
    SInt32 matchingKey = 0;
    
    /*
     * Remove the callback dict object from the session's register queue
     * by matching the address and context
     */
    
    if ( session )
    {
        pthread_mutex_lock( &session->_registerLock );
        CFIndex count = CFDictionaryGetCount( session->_register );
        const void** keys = ( const void** ) malloc( sizeof(void*)*count );
        const void** values = ( const void** ) malloc( sizeof(void*)*count );
        SInt32 queueIndex;

        CFDictionaryGetKeysAndValues( session->_register, keys, values );
        pthread_mutex_unlock( &session->_registerLock );
        for ( queueIndex = 0; queueIndex < count; queueIndex++ )
        {
            CFMutableDictionaryRef callback = ( CFMutableDictionaryRef )( values[queueIndex] );
            void *currentaddress = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackAddressKey );
            void *currentcontext = ( void * ) ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackContextKey );
            int block = ( uintptr_t ) ___CFDictionaryGetIntegerValue( callback, _kDACallbackBlockKey );

            if ( currentaddress == ( void * ) address && currentcontext ==(void *) context )
            {
                CFNumberRef cfnumber =  ( CFNumberRef )( keys[queueIndex] );
                CFNumberGetValue( cfnumber, kCFNumberSInt32Type, &matchingKey );
                if ( block )
                {
                    Block_release ( currentaddress );
                }
                pthread_mutex_lock( &session->_registerLock );
                CFDictionaryRemoveValue( session->_register , cfnumber );
                session->_registerIndex = matchingKey - 1;
                pthread_mutex_unlock( &session->_registerLock );
                break;
            }
        }

        free( keys );
        free( values );
    }
    return matchingKey;
}

__private_extern__ CFMutableDictionaryRef DAGetCallbackFromSession(DASessionRef session, SInt32 index)
{
    CFMutableDictionaryRef callback = 0;
    
    /*
     * Get the callback object from the session's stored callback list
     */
    
    if ( session )
    {
        CFNumberRef number = CFNumberCreate( NULL, kCFNumberSInt32Type, &index );
        pthread_mutex_lock( &session->_registerLock );
        callback =  ( CFMutableDictionaryRef ) CFDictionaryGetValue( session->_register , number );
        pthread_mutex_unlock( &session->_registerLock );
        CFRelease( number );
    }
    return callback;
}

__private_extern__ DAReturn _DASessionRecreate( DASessionRef session )
{
    DAReturn status = kDAReturnSuccess;
    SInt32 queueIndex = 1;
    SInt32 index = 0;
    
    if ( false == DASessionEstablish(session) )
    {
        os_log(OS_LOG_DEFAULT, "failed to establish session with diskarbitrationd");
        return kDAReturnBadArgument;
    }
    
    DASessionSetDispatchQueue( session, session->_queue);
    
    pthread_mutex_lock( &session->_registerLock );
    CFIndex count = CFDictionaryGetCount( session->_register );
    pthread_mutex_unlock( &session->_registerLock );
    CFNumberRef key;
    
    while( index < count )
    {
        key = CFNumberCreate( NULL, kCFNumberSInt32Type, &queueIndex );
        if ( key )
        {
            pthread_mutex_lock( &session->_registerLock );
            CFMutableDictionaryRef callback = ( CFMutableDictionaryRef ) CFDictionaryGetValue( session->_register, key);
            pthread_mutex_unlock( &session->_registerLock );
            if ( callback )
            {
                int kind = (int)  ___CFDictionaryGetIntegerValue( callback, _kDACallbackKindKey );
                int32_t order =  ___CFDictionaryGetIntegerValue( callback, _kDACallbackOrderKey );
                CFDataRef match =  CFDictionaryGetValue( callback, _kDACallbackMatchKey );
                CFDataRef watch =  CFDictionaryGetValue( callback, _kDACallbackWatchKey );
            
                _DAServerSessionRegisterCallback( _DASessionGetID( session ),
                                             ( uintptr_t              ) queueIndex,
                                             ( uintptr_t              ) queueIndex,
                                             ( uint32_t                ) kind,
                                             ( int32_t                ) order,
                                             ( vm_address_t           ) ( match ? CFDataGetBytePtr( match ) : 0 ),
                                             ( mach_msg_type_number_t ) ( match ? CFDataGetLength(  match ) : 0 ),
                                             ( vm_address_t           ) ( watch ? CFDataGetBytePtr( watch ) : 0 ),
                                             ( mach_msg_type_number_t ) ( watch ? CFDataGetLength(  watch ) : 0 ) );
                index++;
            }
            CFRelease( key );
        }
        queueIndex++;
    }
    status = _DAServerSessionSetKeepAlive(session->_server);
    
    return status;
}

DAReturn DASessionKeepAlive( DASessionRef session , dispatch_queue_t queue)
{
    DAReturn status = kDAReturnBadArgument;
    if ( session )
    {
        session->_keepAlive = true;
        session->_queue = queue;
        session->_token = notify_register_dispatch("com.apple.diskarbitrationd.launched", &session->_token, queue, ^(__unused int token){
            os_log(OS_LOG_DEFAULT, "diskarbitrationd relaunched");
            if ( session->_server == NULL )
            {
                _DASessionRecreate( session );
            }
        });
        status = _DAServerSessionSetKeepAlive(session->_server);
    }
    return status;
}

