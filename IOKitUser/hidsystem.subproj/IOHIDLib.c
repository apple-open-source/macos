/*
 * Copyright (c) 1998-2009 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */

#include <unistd.h>
#include <sys/types.h>

#include <CoreFoundation/CoreFoundation.h>
#include <libkern/OSByteOrder.h>


#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>

#if !TARGET_OS_EMBEDDED
kern_return_t IOFramebufferServerStart( void );
#endif

kern_return_t
IOHIDCreateSharedMemory( io_connect_t connect,
	unsigned int version )
{
#if !TARGET_OS_EMBEDDED
    IOFramebufferServerStart();
#endif
    uint64_t inData = version;
    return IOConnectCallMethod( connect, 0,		// Index
			   &inData, 1, NULL, 0,		// Input
			   NULL, NULL, NULL, NULL);	// Output
}

kern_return_t
IOHIDSetEventsEnable( io_connect_t connect,
	boolean_t enable )
{
    uint64_t inData = enable;
    return IOConnectCallMethod( connect, 1,		// Index
			   &inData, 1, NULL, 0,		// Input
			   NULL, NULL, NULL, NULL);	// Output
}

kern_return_t
IOHIDSetCursorEnable( io_connect_t connect,
	boolean_t enable )
{
    uint64_t inData = enable;
    return IOConnectCallMethod( connect, 2,		// Index
			   &inData, 1, NULL, 0,		// Input
			   NULL, NULL, NULL, NULL);	// Output
}

/* DEPRECATED form of IOHIDPostEvent().
kern_return_t
IOHIDPostEvent( mach_port_t connect,
                int type, IOGPoint location, NXEventData *data,
                boolean_t setCursor, int flags, boolean_t setFlags)
*/

kern_return_t
IOHIDPostEvent( io_connect_t        connect,
                UInt32              eventType,
                IOGPoint            location,
                const NXEventData * eventData,
                UInt32              eventDataVersion,
                IOOptionBits        eventFlags,
                IOOptionBits        options )
{
    int *               eventPid = 0;
    size_t              dataSize = sizeof(struct evioLLEvent) + sizeof(int);
    char                data[dataSize];
    struct evioLLEvent* event;
    UInt32              eventDataSize = sizeof(NXEventData);

    bzero(data, dataSize);
    
    event = (struct evioLLEvent*)data;
    
    event->type      = eventType;
    event->location  = location;
    event->flags     = eventFlags;
    event->setFlags  = options;
    event->setCursor = options & (kIOHIDSetCursorPosition | kIOHIDSetRelativeCursorPosition);
    
    eventPid = (int *)(event + 1);
    *eventPid = getpid();

    if ( eventDataVersion < 2 )
    {
        // Support calls from legacy IOHIDPostEvent clients.
        // 1. NXEventData was 32 bytes long.
        // 2. eventDataVersion was (boolean_t) setCursor
        eventDataSize   = 32;
        event->setCursor = eventDataVersion; // 0 or 1
    }

    if ( eventDataSize < sizeof(event->data) )
    {
        bcopy( eventData, &(event->data), eventDataSize );
        bzero( ((UInt8 *)(&(event->data))) + eventDataSize,
               sizeof(event->data) - eventDataSize );
    }
    else
        bcopy( eventData, &event->data, sizeof(event->data) );


    return IOConnectCallMethod(connect, 3,		// Index
			   NULL, 0,    data, dataSize,	// Input
			   NULL, NULL, NULL, NULL);	// Output
}

extern kern_return_t
IOHIDSetCursorBounds( io_connect_t connect, const IOGBounds * bounds )
{
	if ( !bounds )
		return kIOReturnBadArgument;


	return IOConnectCallMethod(connect, 6,			// Index
			NULL, 0,    bounds, sizeof(*bounds),	// Input,
			NULL, NULL, NULL,   NULL);				// Output
}

kern_return_t
IOHIDSetMouseLocation( io_connect_t connect,
	int x, int y)
{
    const size_t    dataSize = sizeof(IOGPoint) + sizeof(int);
    char            data[dataSize];
        
    bzero(data, dataSize);
    
    IOGPoint *loc = (IOGPoint *)data;
    
	int pid = getpid();
    int *eventPid = (int *) &loc[1];

    {
		loc->x = x;
		loc->y = y;
		*eventPid = pid;
	}

	return IOConnectCallMethod(connect, 4,		// Index
			NULL, 0,    data, dataSize, 	// Input
			NULL, NULL, NULL, NULL);	// Output
}

kern_return_t
IOHIDGetButtonEventNum( io_connect_t connect,
	NXMouseButton button, int * eventNum )
{
    kern_return_t	err;

	uint64_t inData = button;
	uint64_t outData;
	uint32_t outSize = 1;
	err = IOConnectCallMethod(connect, 5,						// Index
						  &inData, 1, NULL, 0,				// Input
						  &outData, &outSize, NULL, NULL);	// Output
	*eventNum = (int) outData;
    return( err);
}

kern_return_t
IOHIDGetModifierLockState( io_connect_t handle, int selector, bool *state )
{
    kern_return_t err;
    uint64_t        inData[1] = {selector};
    uint64_t        outData[1] = {0};
    uint32_t        outCount = 1;
    // IOHIDSystem::extGetModifierLockState
    err = IOConnectCallMethod(handle, 5,      // Index
                              inData, 1, NULL, 0,    // Input
                              outData, &outCount, NULL, NULL); // Output
    
    *state = outData[0];
    return err;
}

kern_return_t
IOHIDSetModifierLockState( io_connect_t handle, int selector, bool state )
{
    kern_return_t err;
    uint64_t        inData[2] = {selector, state};
    uint32_t        outCount = 0;
    
    err = IOConnectCallMethod(handle, 6,      // Index
                              inData, 2, NULL, 0,    // Input
                              NULL, &outCount, NULL, NULL); // Output
    
    return err;
}
