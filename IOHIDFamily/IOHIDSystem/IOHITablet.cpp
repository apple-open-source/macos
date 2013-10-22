/*
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

#include <AssertMacros.h>

#include "IOHITablet.h"
#include "IOHITabletPointer.h"

OSDefineMetaClassAndStructors(IOHITablet, IOHIPointing);

UInt16 IOHITablet::generateTabletID()
{
    static UInt16 _nextTabletID = 0x8000;
    return _nextTabletID++;
}

bool IOHITablet::init(OSDictionary *propTable)
{
    if (!IOHIPointing::init(propTable)) {
        return false;
    }

    _systemTabletID = 0;

    return true;
}

bool IOHITablet::open(IOService *			client,
                      IOOptionBits 			options,
                      RelativePointerEventAction	rpeAction,
                      AbsolutePointerEventAction	apeAction,
                      ScrollWheelEventAction		sweAction,
                      TabletEventAction			tabletAction,
                      ProximityEventAction		proximityAction)
{
    
    if (client == this) return true;
     
    return open(client, 
                options, 
                0,
                (RelativePointerEventCallback)rpeAction, 
                (AbsolutePointerEventCallback)apeAction, 
                (ScrollWheelEventCallback)sweAction, 
                (TabletEventCallback)tabletAction, 
                (ProximityEventCallback)proximityAction);
}

bool IOHITablet::open(IOService *			client,
                      IOOptionBits			options,
                      void *				/*refcon*/,
                      RelativePointerEventCallback	rpeCallback,
                      AbsolutePointerEventCallback	apeCallback,
                      ScrollWheelEventCallback		sweCallback,
                      TabletEventCallback		tabletCallback,
                      ProximityEventCallback		proximityCallback)
{
    if (client == this) return true;

    if (!IOHIPointing::open(client, 
                            options,
                            0,
                            rpeCallback, 
                            apeCallback, 
                            sweCallback)) {
        return false;
    }

    _tabletEventTarget = client;
    _tabletEventAction = (TabletEventAction)tabletCallback;
    _proximityEventTarget = client;
    _proximityEventAction = (ProximityEventAction)proximityCallback;

    return open(this, 
                options, 
                (RelativePointerEventAction)IOHIPointing::_relativePointerEvent, 
                (AbsolutePointerEventAction)IOHIPointing::_absolutePointerEvent, 
                (ScrollWheelEventAction)IOHIPointing::_scrollWheelEvent, 
                (TabletEventAction)_tabletEvent, 
                (ProximityEventAction)_proximityEvent);
}


void IOHITablet::dispatchTabletEvent(NXEventData *tabletEvent,
                                     AbsoluteTime ts)
{
    _tabletEvent(   this,
                    tabletEvent,
                    ts);
}

void IOHITablet::dispatchProximityEvent(NXEventData *proximityEvent,
                                        AbsoluteTime ts)
{
    _proximityEvent(this,
                    proximityEvent,
                    ts);
}

bool IOHITablet::startTabletPointer(IOHITabletPointer *pointer, OSDictionary *properties)
{
    require(pointer, no_attach);
    require(pointer->init(properties), no_attach);
    require(pointer->attach(this), no_attach);
    require(pointer->start(this), no_start);
    
no_start:
    pointer->detach(this);
no_attach:
    return false;
}

void IOHITablet::_tabletEvent(IOHITablet *self,
                           NXEventData *tabletData,
                           AbsoluteTime ts)
{
    TabletEventCallback teCallback;
    
    if (!(teCallback = (TabletEventCallback)self->_tabletEventAction) ||
        !tabletData)
        return;
        
    (*teCallback)(
                    self->_tabletEventTarget,
                    tabletData,
                    ts,
                    self,
                    0);
}

void IOHITablet::_proximityEvent(IOHITablet *self,
                              NXEventData *proximityData,
                              AbsoluteTime ts)
{
    ProximityEventCallback peCallback;
    
    if (!(peCallback = (ProximityEventCallback)self->_proximityEventAction) ||
        !proximityData)
        return;
            
    if (self->_systemTabletID == 0)
    {
        self->_systemTabletID = IOHITablet::generateTabletID();
        self->setProperty(kIOHISystemTabletID, (unsigned long long)self->_systemTabletID, 16);
    }

    proximityData->proximity.systemTabletID = self->_systemTabletID;

    (*peCallback)(  
                    self->_proximityEventTarget,
                    proximityData,
                    ts,
                    self,
                    0);
}



