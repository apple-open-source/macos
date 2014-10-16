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
    OSArray *   maps = NULL;
    int         index;
    
    if ( !super::handleStart(provider) )
        return false;
    
    for ( index=0; index<32; index++ ) {
        _buttonMap[index].eventType         = kIOHIDEventTypePointer;
        _buttonMap[index].u.pointer.mask    = (1<<index);
    }
    
    maps = OSDynamicCast(OSArray, getProperty("ButtonMaps"));
    if ( maps ) {
        
        for ( index=0; index<maps->getCount(); index++ ) {
            OSDictionary *  map;
            OSNumber *      number;
            uint32_t        button;
            
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
            
            _buttonMap[button-1].eventType = number->unsigned32BitValue();
            
            switch ( _buttonMap[button-1].eventType ) {
                case kIOHIDEventTypeKeyboard:
                    {
                        uint32_t usagePage, usage;
                        
                        number = OSDynamicCast(OSNumber, map->getObject("UsagePage"));
                        if ( !number )
                            break;
                        
                        usagePage = number->unsigned32BitValue();
                        
                        number = OSDynamicCast(OSNumber, map->getObject("Usage"));
                        if ( !number )
                            break;
                        
                        usage = number->unsigned32BitValue();
                        
                        _buttonMap[button-1].u.keyboard.usagePage  = usagePage;
                        _buttonMap[button-1].u.keyboard.usage      = usage;
                        
                    }
                    break;
                case kIOHIDEventTypePointer:
                    {
                        uint32_t mask;
                        
                        number = OSDynamicCast(OSNumber, map->getObject("Mask"));
                        if ( !number )
                            break;
                        
                        mask = number->unsigned32BitValue();

                        
                        _buttonMap[button-1].u.pointer.mask = mask;
                    }
                    break;
                default:
                    break;
            }
             
        }
    }
    
    return true;
}

//====================================================================================================
// IOHIDEventOverrideDriver::dispatchEvent
//====================================================================================================
void IOHIDEventOverrideDriver::dispatchEvent(IOHIDEvent * event, IOOptionBits options)
{
    IOHIDEvent * targetEvent = NULL;
    
    if ( (targetEvent = event->getEvent(kIOHIDEventTypePointer)) ) {
        
        IOHIDEvent *    newEvent                        = NULL;
        AbsoluteTime    timestamp                       = targetEvent->getTimeStamp();
        uint32_t        rawPointerButtonMask            = targetEvent->getIntegerValue(kIOHIDEventFieldPointerButtonMask);
        uint32_t        rawPointerButtonMaskDelta       = _rawPointerButtonMask ^ rawPointerButtonMask;
        uint32_t        resultantPointerButtonMask      = _resultantPointerButtonMask;
        int             index;
        
        for ( index=0; index<32; index++ ) {
            
            bool value = rawPointerButtonMask & (1<<index);
            
            if ( (rawPointerButtonMaskDelta & (1<<index)) == 0 )
                continue;
            
            switch (_buttonMap[index].eventType ) {
                case kIOHIDEventTypeKeyboard:
                    dispatchKeyboardEvent(timestamp, _buttonMap[index].u.keyboard.usagePage, _buttonMap[index].u.keyboard.usage, value);
                    break;
                case kIOHIDEventTypePointer:
                    if ( value ) 
                        resultantPointerButtonMask |= _buttonMap[index].u.pointer.mask;
                    else
                        resultantPointerButtonMask &= ~_buttonMap[index].u.pointer.mask;
                    break;
                default:
                    break;
            }
        }
        
        _rawPointerButtonMask = rawPointerButtonMask;
        
        newEvent = IOHIDEvent::relativePointerEvent(timestamp, event->getIntegerValue(kIOHIDEventFieldPointerX), event->getIntegerValue(kIOHIDEventFieldPointerY), event->getIntegerValue(kIOHIDEventFieldPointerZ), resultantPointerButtonMask, _resultantPointerButtonMask);
        if ( newEvent ) {
            super::dispatchEvent(newEvent);
            newEvent->release();
        }

        _resultantPointerButtonMask = resultantPointerButtonMask;

    } else {
        super::dispatchEvent(event, options);
    }
}

