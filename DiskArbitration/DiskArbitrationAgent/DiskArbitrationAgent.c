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

#include "../diskarbitrationd/DAInternal.h"

#include <xpc/private.h>
#include <ApplicationServices/ApplicationServices.h>

static void __DAAgentMessageCallback( xpc_object_t object );

static void __DAAgentConnectionCallback( xpc_object_t object )
{
    xpc_type_t type;

    type = xpc_get_type( object );

    if ( type == XPC_TYPE_CONNECTION )
    {
        xpc_connection_set_event_handler( object, ^( xpc_object_t object ) { __DAAgentMessageCallback( object ); } );

        xpc_connection_resume( object );
    }
}

static void __DAAgentMessageCallback( xpc_object_t object )
{
    xpc_type_t type;

    type = xpc_get_type( object );

    if ( type == XPC_TYPE_DICTIONARY )
    {
        CFURLRef path;

        path = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, CFSTR( "/Applications/Utilities/Disk Utility.app" ), kCFURLPOSIXPathStyle, FALSE );

        if ( path )
        {
            LSOpenCFURLRef( path, NULL );

            CFRelease( path );
        }
    }

    exit( 0 );
}

int main( )
{
    xpc_connection_t connection;

    connection = xpc_connection_create_listener( _kDAAgentName, NULL );

    if ( connection )
    {
        xpc_connection_set_event_handler( connection, ^( xpc_object_t object ) { __DAAgentConnectionCallback( object ); } );

        xpc_connection_set_legacy( connection );

        xpc_connection_resume( connection );

        dispatch_main( );

        xpc_release( connection );
    }

    return 0;
}
