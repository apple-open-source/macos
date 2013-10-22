/*
 * Copyright (c) 1998-2013 Apple Inc. All rights reserved.
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

#include "DAAgent.h"
#include "DADialog.h"

#include <xpc/xpc.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>

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
        const void * _disk;
        size_t       _diskSize;

        _disk = xpc_dictionary_get_data( object, _kDAAgentDiskKey, &_diskSize );

        if ( _disk )
        {
            CFDataRef serialization;

            serialization = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault, _disk, _diskSize, kCFAllocatorNull );

            if ( serialization )
            {
                DASessionRef session;

                session = DASessionCreate( kCFAllocatorDefault );

                if ( session )
                {
                    DADiskRef disk;

                    disk = _DADiskCreateFromSerialization( kCFAllocatorDefault, session, serialization );

                    if ( disk )
                    {
                        _DAAgentAction _action;

                        _action = xpc_dictionary_get_uint64( object, _kDAAgentActionKey );

                        switch ( _action )
                        {
                            case _kDAAgentActionShowDeviceRemoval:
                            {
                                DADialogShowDeviceRemoval( disk );

                                break;
                            }
                            case _kDAAgentActionShowDeviceUnreadable:
                            {
                                DADialogShowDeviceUnreadable( disk );

                                break;
                            }
                            case _kDAAgentActionShowDeviceUnrepairable:
                            {
                                DADialogShowDeviceUnrepairable( disk );

                                break;
                            }
                        }

                        CFRelease( disk );
                    }

                    CFRelease( session );
                }

                CFRelease( serialization );
            }
        }
    }
}

int main( )
{
    xpc_connection_t connection;

    connection = xpc_connection_create_mach_service( _kDAAgentName, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER );

    if ( connection )
    {
        xpc_connection_set_event_handler( connection, ^( xpc_object_t object ) { __DAAgentConnectionCallback( object ); } );

        xpc_connection_resume( connection );

        dispatch_main( );
    }

    return 0;
}
