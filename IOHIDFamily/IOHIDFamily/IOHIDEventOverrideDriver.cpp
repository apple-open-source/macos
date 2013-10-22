/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2013 Apple Computer, Inc.  All Rights Reserved.
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

#include "IOHIDEventOverrideDriver.h"
#include "IOHIDUsageTables.h"

//===========================================================================
// IOHIDEventOverrideDriver class

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( IOHIDEventOverrideDriver, IOHIDEventDriver )

//====================================================================================================
// IOHIDEventOverrideDriver::dispatchKeyboardEvent
//====================================================================================================
bool IOHIDEventOverrideDriver::handleStart( IOService * provider )
{
    OSArray * maps = NULL;
    
    if ( !super::handleStart(provider) )
        return false;
    
    maps = OSDynamicCast(OSArray, getProperty("ButtonMaps"));
    if ( maps ) {
        int index;
        
        for ( index=0; index<maps->getCount(); index++ ) {
            OSDictionary *  map;
            OSNumber *      number;
            uint32_t        button;
            uint32_t        usagePage;
            uint32_t        usage;
            uint32_t        eventType;
            
            map = OSDynamicCast(OSDictionary, maps->getObject(index));
            if ( !map )
                continue;
            
            number = OSDynamicCast(OSNumber, map->getObject("ButtonNumber"));
            if ( !number )
                continue;
            
            button = number->unsigned32BitValue();
            if ( !button || button>32 )
                continue;
            
            number = OSDynamicCast(OSNumber, map->getObject("EventType"));
            if ( !number )
                continue;
            
            eventType = number->unsigned32BitValue();
            
            number = OSDynamicCast(OSNumber, map->getObject("UsagePage"));
            if ( !number )
                continue;
            
            usagePage = number->unsigned32BitValue();

            number = OSDynamicCast(OSNumber, map->getObject("Usage"));
            if ( !number )
                continue;
            
            usage = number->unsigned32BitValue();
            
            _buttonMap[button-1].eventType  = eventType;
            _buttonMap[button-1].usagePage  = usagePage;
            _buttonMap[button-1].usage      = usage;
        }
    }
    
    return true;
}

//====================================================================================================
// IOHIDEventOverrideDriver::dispatchKeyboardEvent
//====================================================================================================
void IOHIDEventOverrideDriver::dispatchEvent(IOHIDEvent * event, IOOptionBits options)
{
    IOHIDEvent * targetEvent = NULL;
    if ( (targetEvent = event->getEvent(kIOHIDEventTypePointer)) ) {
        AbsoluteTime    timestamp               = targetEvent->getTimeStamp();
        uint32_t        pointerButtonMaskNew    = targetEvent->getIntegerValue(kIOHIDEventFieldPointerButtonMask);
        uint32_t        pointerButtonMaskDelta  = _pointerButtonMask ^ pointerButtonMaskNew;
        int             index;
        
        for ( index=0; index<32; index++ ) {
            bool value;
            if ( (pointerButtonMaskDelta & (1<<index)) == 0 )
                continue;
            
            switch (_buttonMap[index].eventType ) {
                case kIOHIDEventTypeKeyboard:
                    value = pointerButtonMaskNew & (1<<index);
                    dispatchKeyboardEvent(timestamp, _buttonMap[index].usagePage, _buttonMap[index].usage, value);
                    break;
                default:
                    break;
            }
        }
        
        _pointerButtonMask = pointerButtonMaskNew;
    } else {
        super::dispatchEvent(event, options);
    }
}

