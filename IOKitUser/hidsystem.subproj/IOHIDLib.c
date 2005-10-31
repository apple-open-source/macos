/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */

#include <System/libkern/OSCrossEndian.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/iokitmig.h>
#include <sys/types.h>
#include <unistd.h>

kern_return_t
IOFramebufferServerStart( void );

kern_return_t
IOHIDCreateSharedMemory( io_connect_t connect,
	unsigned int version )
{
    kern_return_t	err;
    unsigned int	len;

    IOFramebufferServerStart();

    len = 0;
    err = io_connect_method_scalarI_scalarO( connect, 0, /*index*/
                    &version, 1, NULL, &len);

    return( err);
}

kern_return_t
IOHIDSetEventsEnable( io_connect_t connect,
	boolean_t enable )
{
    kern_return_t	err;
    unsigned int	len;

    len = 0;
    err = io_connect_method_scalarI_scalarO( connect, 1, /*index*/
                    &enable, 1, NULL, &len);

    return( err);
}

kern_return_t
IOHIDSetCursorEnable( io_connect_t connect,
	boolean_t enable )
{
    kern_return_t	err;
    unsigned int	len;

    len = 0;
    err = io_connect_method_scalarI_scalarO( connect, 2, /*index*/
                    &enable, 1, NULL, &len);

    return( err);
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
    kern_return_t       err;
    unsigned int        len;
    int *               eventPid = 0;
    int                 dataSize = sizeof(struct evioLLEvent) + sizeof(int);
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

    ROSETTA_ONLY(
        int i;
        
        event->type         = OSSwapInt32(eventType);
        event->location.x   = OSSwapInt16(location.x);
        event->location.y   = OSSwapInt16(location.y);
        event->flags        = OSSwapInt32(eventFlags);
        event->setFlags     = OSSwapInt32(options);
        event->setCursor    = OSSwapInt32(event->setCursor);
        *eventPid           = OSSwapInt32(*eventPid);

        /* Swap individual event data fields */
        switch ( eventType )
        {
            case NX_LMOUSEDOWN:
            case NX_LMOUSEUP:
            case NX_RMOUSEDOWN:
            case NX_RMOUSEUP:
            case NX_OMOUSEDOWN:
            case NX_OMOUSEUP:
                event->data.mouse.eventNum  = OSSwapInt16(eventData->mouse.eventNum);
                event->data.mouse.click     = OSSwapInt32(eventData->mouse.click);
                event->data.mouse.reserved3 = OSSwapInt32(eventData->mouse.reserved3);
                
                switch ( eventData->mouse.subType )
                {
                    case NX_SUBTYPE_TABLET_PROXIMITY:
                        event->data.mouse.tablet.proximity.vendorID             = OSSwapInt16(eventData->mouse.tablet.proximity.vendorID);
                        event->data.mouse.tablet.proximity.tabletID             = OSSwapInt16(eventData->mouse.tablet.proximity.tabletID);
                        event->data.mouse.tablet.proximity.pointerID            = OSSwapInt16(eventData->mouse.tablet.proximity.pointerID);
                        event->data.mouse.tablet.proximity.deviceID             = OSSwapInt16(eventData->mouse.tablet.proximity.deviceID);                        
                        event->data.mouse.tablet.proximity.systemTabletID       = OSSwapInt16(eventData->mouse.tablet.proximity.systemTabletID);
                        event->data.mouse.tablet.proximity.vendorPointerType    = OSSwapInt16(eventData->mouse.tablet.proximity.vendorPointerType);
                        event->data.mouse.tablet.proximity.pointerSerialNumber  = OSSwapInt32(eventData->mouse.tablet.proximity.pointerSerialNumber);
                        event->data.mouse.tablet.proximity.uniqueID             = OSSwapInt64(eventData->mouse.tablet.proximity.uniqueID);
                        event->data.mouse.tablet.proximity.capabilityMask       = OSSwapInt32(eventData->mouse.tablet.proximity.capabilityMask);
                        event->data.mouse.tablet.proximity.reserved1            = OSSwapInt16(eventData->mouse.tablet.proximity.reserved1);                        
                        break;
                    case NX_SUBTYPE_TABLET_POINT:
                        event->data.mouse.tablet.point.x                        = OSSwapInt32(eventData->mouse.tablet.point.x);
                        event->data.mouse.tablet.point.y                        = OSSwapInt32(eventData->mouse.tablet.point.y);
                        event->data.mouse.tablet.point.z                        = OSSwapInt32(eventData->mouse.tablet.point.z);
                        event->data.mouse.tablet.point.buttons                  = OSSwapInt16(eventData->mouse.tablet.point.buttons);
                        event->data.mouse.tablet.point.pressure                 = OSSwapInt16(eventData->mouse.tablet.point.pressure);
                        event->data.mouse.tablet.point.tilt.x                   = OSSwapInt16(eventData->mouse.tablet.point.tilt.x);
                        event->data.mouse.tablet.point.tilt.y                   = OSSwapInt16(eventData->mouse.tablet.point.tilt.y);
                        event->data.mouse.tablet.point.rotation                 = OSSwapInt16(eventData->mouse.tablet.point.rotation);
                        event->data.mouse.tablet.point.tangentialPressure       = OSSwapInt16(eventData->mouse.tablet.point.tangentialPressure);
                        event->data.mouse.tablet.point.deviceID                 = OSSwapInt16(eventData->mouse.tablet.point.deviceID);                      
                        event->data.mouse.tablet.point.vendor1                  = OSSwapInt16(eventData->mouse.tablet.point.vendor1);                      
                        event->data.mouse.tablet.point.vendor2                  = OSSwapInt16(eventData->mouse.tablet.point.vendor2);                      
                        event->data.mouse.tablet.point.vendor3                  = OSSwapInt16(eventData->mouse.tablet.point.vendor3);                      
                        break;
                }
                
                break;
            case NX_MOUSEMOVED:
            case NX_LMOUSEDRAGGED:
            case NX_RMOUSEDRAGGED:
            case NX_OMOUSEDRAGGED:
                event->data.mouseMove.dx        = OSSwapInt32(eventData->mouseMove.dx);
                event->data.mouseMove.dy        = OSSwapInt32(eventData->mouseMove.dy);
                event->data.mouseMove.reserved2 = OSSwapInt32(eventData->mouseMove.reserved2);

                switch ( eventData->mouseMove.subType )
                {
                    case NX_SUBTYPE_TABLET_PROXIMITY:
                        event->data.mouseMove.tablet.proximity.vendorID             = OSSwapInt16(eventData->mouseMove.tablet.proximity.vendorID);
                        event->data.mouseMove.tablet.proximity.tabletID             = OSSwapInt16(eventData->mouseMove.tablet.proximity.tabletID);
                        event->data.mouseMove.tablet.proximity.pointerID            = OSSwapInt16(eventData->mouseMove.tablet.proximity.pointerID);
                        event->data.mouseMove.tablet.proximity.deviceID             = OSSwapInt16(eventData->mouseMove.tablet.proximity.deviceID);                        
                        event->data.mouseMove.tablet.proximity.systemTabletID       = OSSwapInt16(eventData->mouseMove.tablet.proximity.systemTabletID);
                        event->data.mouseMove.tablet.proximity.vendorPointerType    = OSSwapInt16(eventData->mouseMove.tablet.proximity.vendorPointerType);
                        event->data.mouseMove.tablet.proximity.pointerSerialNumber  = OSSwapInt32(eventData->mouseMove.tablet.proximity.pointerSerialNumber);
                        event->data.mouseMove.tablet.proximity.uniqueID             = OSSwapInt64(eventData->mouseMove.tablet.proximity.uniqueID);
                        event->data.mouseMove.tablet.proximity.capabilityMask       = OSSwapInt32(eventData->mouseMove.tablet.proximity.capabilityMask);
                        event->data.mouseMove.tablet.proximity.reserved1            = OSSwapInt16(eventData->mouseMove.tablet.proximity.reserved1);                        
                        break;
                    case NX_SUBTYPE_TABLET_POINT:
                        event->data.mouseMove.tablet.point.x                    = OSSwapInt32(eventData->mouseMove.tablet.point.x);
                        event->data.mouseMove.tablet.point.y                    = OSSwapInt32(eventData->mouseMove.tablet.point.y);
                        event->data.mouseMove.tablet.point.z                    = OSSwapInt32(eventData->mouseMove.tablet.point.z);
                        event->data.mouseMove.tablet.point.buttons              = OSSwapInt16(eventData->mouseMove.tablet.point.buttons);
                        event->data.mouseMove.tablet.point.pressure             = OSSwapInt16(eventData->mouseMove.tablet.point.pressure);
                        event->data.mouseMove.tablet.point.tilt.x               = OSSwapInt16(eventData->mouseMove.tablet.point.tilt.x);
                        event->data.mouseMove.tablet.point.tilt.y               = OSSwapInt16(eventData->mouseMove.tablet.point.tilt.y);
                        event->data.mouseMove.tablet.point.rotation             = OSSwapInt16(eventData->mouseMove.tablet.point.rotation);
                        event->data.mouseMove.tablet.point.tangentialPressure   = OSSwapInt16(eventData->mouseMove.tablet.point.tangentialPressure);
                        event->data.mouseMove.tablet.point.deviceID             = OSSwapInt16(eventData->mouseMove.tablet.point.deviceID);                      
                        event->data.mouseMove.tablet.point.vendor1              = OSSwapInt16(eventData->mouseMove.tablet.point.vendor1);                      
                        event->data.mouseMove.tablet.point.vendor2              = OSSwapInt16(eventData->mouseMove.tablet.point.vendor2);                      
                        event->data.mouseMove.tablet.point.vendor3              = OSSwapInt16(eventData->mouseMove.tablet.point.vendor3);                      
                        break;
                }
                break;
            case NX_MOUSEENTERED:
            case NX_MOUSEEXITED:
                event->data.tracking.reserved   = OSSwapInt16(eventData->tracking.reserved);
                event->data.tracking.eventNum   = OSSwapInt16(eventData->tracking.eventNum);
                event->data.tracking.trackingNum= OSSwapInt32(eventData->tracking.trackingNum);
                event->data.tracking.userData   = OSSwapInt32(eventData->tracking.userData);
                
                // reserved
                for (i=0; i<9; i++)
                {
                    ((UInt32 *)&(event->data.tracking.reserved1))[i] = OSSwapInt32(((UInt32 *)&(eventData->tracking.reserved1))[i]);
                }
                break;
                
            case NX_FLAGSCHANGED:
                break;

            case NX_KEYDOWN:
            case NX_KEYUP:
                event->data.key.origCharSet     = OSSwapInt16(eventData->key.origCharSet);
                event->data.key.repeat          = OSSwapInt16(eventData->key.repeat);
                event->data.key.charSet         = OSSwapInt16(eventData->key.charSet);
                event->data.key.charCode        = OSSwapInt16(eventData->key.charCode);
                event->data.key.keyCode         = OSSwapInt16(eventData->key.keyCode);
                event->data.key.origCharCode    = OSSwapInt16(eventData->key.origCharCode);
                event->data.key.reserved1       = OSSwapInt16(eventData->key.reserved1);
                event->data.key.keyboardType    = OSSwapInt16(eventData->key.keyboardType);

                // reserved
                for (i=0; i<7; i++)
                {
                    ((UInt32 *)&(event->data.key.reserved2))[i] = OSSwapInt32(((UInt32 *)&(eventData->key.reserved2))[i]);
                }
                break;
            
            case NX_SCROLLWHEELMOVED:
                event->data.scrollWheel.deltaAxis1          = OSSwapInt16(eventData->scrollWheel.deltaAxis1);
                event->data.scrollWheel.deltaAxis2          = OSSwapInt16(eventData->scrollWheel.deltaAxis2);
                event->data.scrollWheel.deltaAxis3          = OSSwapInt16(eventData->scrollWheel.deltaAxis3);
                event->data.scrollWheel.reserved1           = OSSwapInt16(eventData->scrollWheel.reserved1);
                event->data.scrollWheel.fixedDeltaAxis1     = OSSwapInt32(eventData->scrollWheel.fixedDeltaAxis1);
                event->data.scrollWheel.fixedDeltaAxis2     = OSSwapInt32(eventData->scrollWheel.fixedDeltaAxis2);
                event->data.scrollWheel.fixedDeltaAxis3     = OSSwapInt32(eventData->scrollWheel.fixedDeltaAxis3);
                event->data.scrollWheel.pointDeltaAxis1     = OSSwapInt32(eventData->scrollWheel.pointDeltaAxis1);
                event->data.scrollWheel.pointDeltaAxis2     = OSSwapInt32(eventData->scrollWheel.pointDeltaAxis2);
                event->data.scrollWheel.pointDeltaAxis3     = OSSwapInt32(eventData->scrollWheel.pointDeltaAxis3);

                // reserved
                for (i=0; i<4; i++)
                {
                    ((UInt32 *)&(event->data.scrollWheel.reserved8))[i] = OSSwapInt32(((UInt32 *)&(eventData->scrollWheel.reserved8))[i]);
                }
                break;
                
            case NX_TABLETPOINTER:
                event->data.tablet.x                    = OSSwapInt32(eventData->tablet.x);
                event->data.tablet.y                    = OSSwapInt32(eventData->tablet.y);
                event->data.tablet.z                    = OSSwapInt32(eventData->tablet.z);
                event->data.tablet.buttons              = OSSwapInt16(eventData->tablet.buttons);
                event->data.tablet.pressure             = OSSwapInt16(eventData->tablet.pressure);
                event->data.tablet.tilt.x               = OSSwapInt16(eventData->tablet.tilt.x);
                event->data.tablet.tilt.y               = OSSwapInt16(eventData->tablet.tilt.y);
                event->data.tablet.rotation             = OSSwapInt16(eventData->tablet.rotation);
                event->data.tablet.tangentialPressure   = OSSwapInt16(eventData->tablet.tangentialPressure);
                event->data.tablet.deviceID             = OSSwapInt16(eventData->tablet.deviceID);                      
                event->data.tablet.vendor1              = OSSwapInt16(eventData->tablet.vendor1);                      
                event->data.tablet.vendor2              = OSSwapInt16(eventData->tablet.vendor2);                      
                event->data.tablet.vendor3              = OSSwapInt16(eventData->tablet.vendor3);                      

                // reserved
                for (i=0; i<4; i++)
                {
                    ((UInt32 *)&(event->data.tablet.reserved))[i] = OSSwapInt32(((UInt32 *)&(eventData->tablet.reserved))[i]);
                }
                break;
            case NX_TABLETPROXIMITY:
                event->data.proximity.vendorID              = OSSwapInt16(eventData->proximity.vendorID);
                event->data.proximity.tabletID              = OSSwapInt16(eventData->proximity.tabletID);
                event->data.proximity.pointerID             = OSSwapInt16(eventData->proximity.pointerID);
                event->data.proximity.deviceID              = OSSwapInt16(eventData->proximity.deviceID);
                event->data.proximity.systemTabletID        = OSSwapInt16(eventData->proximity.systemTabletID);
                event->data.proximity.vendorPointerType     = OSSwapInt16(eventData->proximity.vendorPointerType);
                event->data.proximity.pointerSerialNumber   = OSSwapInt32(eventData->proximity.pointerSerialNumber);
                event->data.proximity.uniqueID              = OSSwapInt64(eventData->proximity.uniqueID);
                event->data.proximity.capabilityMask        = OSSwapInt32(eventData->proximity.capabilityMask);
                event->data.proximity.reserved1             = OSSwapInt16(eventData->proximity.reserved1);

                // reserved
                for (i=0; i<4; i++)
                {
                    ((UInt32 *)&(event->data.proximity.reserved2))[i] = OSSwapInt32(((UInt32 *)&(eventData->proximity.reserved2))[i]);
                }
                break;
                
            case NX_SYSDEFINED:
                event->data.compound.reserved   = OSSwapInt16(eventData->compound.reserved);
                event->data.compound.subType    = OSSwapInt16(eventData->compound.subType);
                
                switch ( eventData->compound.subType )
                {
                    case NX_SUBTYPE_AUX_MOUSE_BUTTONS:
                    case NX_SUBTYPE_AUX_CONTROL_BUTTONS:
                        // swap compound longs
                        for (i=0; i<11; i++)
                        {
                            ((UInt32 *)&(event->data.compound.misc.L))[i] = OSSwapInt32(((UInt32 *)&(eventData->compound.misc.L))[i]);
                        }
                        break;
                }
                break;
        }
    ); /* END ROSETTA_ONLY */

    len = 0;
    err = io_connect_method_structureI_structureO(
             connect,
             3,                /* index       */
             data,             /* input       */
             dataSize,         /* inputCount  */
             NULL,             /* output      */
             &len);            /* outputCount */
             
    return (err);
}

extern kern_return_t
IOHIDSetCursorBounds( io_connect_t connect, const IOGBounds * bounds )
{
    IOByteCount	len = 0;

	if ( !bounds )
		return kIOReturnBadArgument;

    ROSETTA_ONLY(
        IOGBounds   newBounds;

        bcopy(bounds, &newBounds, sizeof(newBounds));

        newBounds.minx = OSSwapInt16(newBounds.minx);
        newBounds.maxx = OSSwapInt16(newBounds.maxx);
        newBounds.miny = OSSwapInt16(newBounds.miny);
        newBounds.maxy = OSSwapInt16(newBounds.maxy);

        return (IOConnectMethodStructureIStructureO( connect, 6, /*index*/
                    sizeof( newBounds), &len,
                    &newBounds, NULL ));

    );

    return (IOConnectMethodStructureIStructureO( connect, 6, /*index*/
                    sizeof( *bounds), &len,
                    bounds, NULL ));
}

kern_return_t
IOHIDSetMouseLocation( io_connect_t connect,
	int x, int y)
{
    kern_return_t	err;
    unsigned int	len;
    IOGPoint *		loc;
    int             dataSize = sizeof(IOGPoint) + sizeof(int);
    char            data[dataSize];
    int *           eventPid = 0;
        
    bzero(data, dataSize);
    
    loc = (IOGPoint *)data;
    
    loc->x = x;
    loc->y = y;

    eventPid = (int *)(loc + 1);
    *eventPid = getpid();

    ROSETTA_ONLY(
        loc->x      = OSSwapInt16(x);
        loc->y      = OSSwapInt16(y);
        *eventPid   = OSSwapInt32(*eventPid);
    );

    len = 0;
    err = io_connect_method_structureI_structureO( connect, 4, /*index*/
                    data, dataSize, NULL, &len);

    return( err);
}

kern_return_t
IOHIDGetButtonEventNum( io_connect_t connect,
	NXMouseButton button, int * eventNum )
{
    kern_return_t	err;
    unsigned int	len;

    len = 1;
    err = io_connect_method_scalarI_scalarO( connect, 5, /*index*/
                    (int *)&button, 1, (void *)eventNum, &len);

    return( err);
}
