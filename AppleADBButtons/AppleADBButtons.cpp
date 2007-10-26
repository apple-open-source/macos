/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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
#include "AppleADBButtons.h"
#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(AppleADBButtons,IOHIKeyboard)

void button_data ( IOService * us, UInt8 adbCommand, IOByteCount length, UInt8 * data );
void asyncFunc ( void * );
static void check_eject_held(thread_call_param_t arg);


bool AppleADBButtons::init(OSDictionary * properties)
{
	if ( !super::init(properties) )
		return false;
	
	/*
	 * Initialize minimal state.
	 */
	_pADBKeyboard = 0;
	_cachedKeyboardFlags = 0;
	_eject_released = true;  //initially assume eject button is released
	_publishNotify = 0;
	_terminateNotify = 0;
	/*
		Inability to allocate thread that handles eject should not cause 
		failure.  Use _peject_timer after making sure it's not NULL.
	*/
	_peject_timer = thread_call_allocate((thread_call_func_t)check_eject_held, (thread_call_param_t)this);
	_register_for_button = OSSymbol::withCString("register_for_button");

	return TRUE;
}

// **********************************************************************************
// start
//
// **********************************************************************************
bool AppleADBButtons::start ( IOService * theNub )
{
    int i;
    
    for ( i = 0; i < kMax_registrations; i++ ) {
        keycodes[i] = kNullKey;
        downHandlerThreadCalls[i] = NULL;
    }

    adbDevice = (IOADBDevice *)theNub;

    _initial_handler_id = adbDevice->handlerID();
    if (_initial_handler_id == 0xc0)
    {
        return false;   //Don't allow Apple A/V monitor buttons to generate
                        // ADB button data since monitor is already in LOCAL mode,
                        // courtesy of AppleADBDisplay.kext
    }

    if( !super::start(theNub))
        return false;

    if( !adbDevice->seizeForClient(this, button_data) ) {
        IOLog("%s: Seize failed\n", getName());
        return false;
    }

	_publishNotify = addNotification( 
						gIOPublishNotification, serviceMatching("AppleADBKeyboard"),
						&AppleADBButtons::_publishNotificationHandler,
						this, 0 );

	_terminateNotify = addNotification( 
						gIOTerminatedNotification, serviceMatching("AppleADBKeyboard"),
						&AppleADBButtons::_terminateNotificationHandler,
						this, 0 );

    return true;
}

void AppleADBButtons::free()
{
	if ( _publishNotify )
	{
		_publishNotify->remove();
		_publishNotify = 0;
	}

	if ( _terminateNotify )
	{
		_terminateNotify->remove();
		_terminateNotify = 0;
	}

	if (_register_for_button) 
	{
		_register_for_button->release();
		_register_for_button = 0;
	}

	if ( _peject_timer )
	{
		thread_call_cancel( _peject_timer );
		thread_call_free( _peject_timer );
	}

    for ( int i = 0; i < kMax_registrations; i++ ) {
        if ( downHandlerThreadCalls[i] ) {
            thread_call_free ( downHandlerThreadCalls[i] );
        }
    }

	super::free();
}

bool AppleADBButtons::_publishNotificationHandler( void * target, 
                                void * ref, IOService * newService )
{
	if ( target )
	{
		AppleADBButtons *self = (AppleADBButtons *)target;

		if ( !self->_pADBKeyboard && newService )
			self->_pADBKeyboard = OSDynamicCast( IOHIKeyboard, newService );
	}

	return true;
}
    
bool AppleADBButtons::_terminateNotificationHandler( void * target, 
				void * ref, IOService * service )
{
	if ( target )
		((AppleADBButtons *)target)->_pADBKeyboard = 0;

	return true;
}


IOReturn AppleADBButtons::callPlatformFunction(const OSSymbol *functionName,
                                                            bool waitForFunction,
                                                            void *param1, void *param2,
                                                            void *param3, void *param4)
{
	if ( functionName == _register_for_button )
	{
		registerForButton( (unsigned int)param1, (IOService *)param2,
			(button_handler)param3, (bool)param4 );

		return kIOReturnSuccess;
	}

	return kIOReturnBadArgument;
}

// **********************************************************************************
// doesKeyLock
//
// If the key physically locks (like the num lock key), return true
// **********************************************************************************
bool AppleADBButtons::doesKeyLock(unsigned key)
{
	if ( key == NX_KEYTYPE_NUM_LOCK )
		return TRUE;

	return super::doesKeyLock( key );
}

UInt64 AppleADBButtons::getGUID()
{
  return(kAppleOnboardGUID);
}

UInt32 AppleADBButtons::interfaceID()
{
    return NX_EVS_DEVICE_INTERFACE_ADB;
}

UInt32 AppleADBButtons::deviceType()
{
    OSNumber 	*xml_handlerID;

    //Info.plist settings always have highest priority
    xml_handlerID = OSDynamicCast( OSNumber, getProperty("alt_handler_id"));
    if (xml_handlerID)
    {
	return xml_handlerID->unsigned32BitValue();
    }

    //If initial id is 31 then this is a post-WallStreet PowerBook
    if ( _initial_handler_id == 31 )
    {
		if ( _pADBKeyboard )
		{
			return _pADBKeyboard->deviceType();
		}
		else
		{
			return 195; //Gestalt.h domestic (ANSI) Powerbook keyboard
		}
	}
    
    return adbDevice->handlerID();
}

void AppleADBButtons::setDeviceFlags(unsigned flags)
{
    if ( _pADBKeyboard )
        _pADBKeyboard->setDeviceFlags(flags);
        
    super::setDeviceFlags(flags);
}


// **********************************************************************************
// registerForButton
//
// Clients call here, specifying a button and a routine to call when that
// button is pressed or released.
// **********************************************************************************
IOReturn AppleADBButtons::registerForButton ( unsigned int keycode, IOService * registrant, button_handler handler, bool down )
{
    int i;

    for ( i = 0; i < kMax_registrations; i++ ) {
        if ( keycodes[i] == kNullKey ) {
            if ( down ) {
                downHandlerThreadCalls[i] = thread_call_allocate( (thread_call_func_t) handler, (thread_call_param_t)registrant);
                keycodes[i] = keycode;
                break;
            }
        }
    }
    return kIOReturnSuccess;
}

// **********************************************************************************
// button_data
//
// **********************************************************************************
void button_data ( IOService * us, UInt8 adbCommand, IOByteCount length, UInt8 * data )
{
((AppleADBButtons *)us)->packet(data,length,adbCommand);
}


// **********************************************************************************
// packet
//
// **********************************************************************************
IOReturn AppleADBButtons::packet (UInt8 * data, IOByteCount count, UInt8 adbCommand )
{
    unsigned int	keycode;
    bool		down;

    keycode = *data;
    down = ((keycode & 0x80) == 0);
    keycode &= 0x7f;
    dispatchButtonEvent(keycode,down);
    
    keycode = *(data + 1);
    if( keycode != 0xff ) {
        down = ((keycode & 0x80) == 0);
        keycode &= 0x7f;
        dispatchButtonEvent(keycode,down);
    }
        
    return kIOReturnSuccess;
}


// **********************************************************************************
// dispatchButtonEvent
//
// Look for any registered handlers for this button and notify them.
// **********************************************************************************
void AppleADBButtons::dispatchButtonEvent (unsigned int keycode, bool down )
{
    int i;
    AbsoluteTime now;

    clock_get_uptime(&now);
    
    for ( i = 0; i < kMax_registrations; i++ ) {
        if ( keycodes[i] == keycode ) {
            if ( down ) {
                if (downHandlerThreadCalls[i] != NULL ) {
                    thread_call_enter(downHandlerThreadCalls[i]);
                }
            }
        }
    }
    
    //Copy the device flags (modifier flags) from the ADB keyboard driver
    if ((_initial_handler_id == 31) && _pADBKeyboard)
    {
        UInt32  currentFlags;
        
        currentFlags = deviceFlags() & ~_cachedKeyboardFlags;
        _cachedKeyboardFlags = _pADBKeyboard->deviceFlags();
        currentFlags |= _cachedKeyboardFlags;
                    
        setDeviceFlags(currentFlags);
    }

    //Only dispatch keycodes that this driver understands.  
    //     See appleADBButtonsKeyMap[] for the list.
    switch (keycode)
    {
	case kVolume_up:
	case kVolume_down:
	case kMute:
	case kBrightness_up:
	case kBrightness_down:
	case kNum_lock_on_laptops:
	case kVideoMirror:
	case kIllumination_toggle:
	case kIllumination_down:
	case kIllumination_up:
	    dispatchKeyboardEvent(keycode, down, now);
	    break;
	case kEject:
		if ( _peject_timer )
		{
			if ( down )
			{
                if ( deviceFlags() )
                {
                    dispatchKeyboardEvent( kEject, TRUE, now );
                }
                else
                {
                    AbsoluteTime	deadline;
                    OSNumber		*plist_time;

                    _eject_released = false;
                    _eject_delay = 250;	//.25 second default
                    plist_time = OSDynamicCast( OSNumber, getProperty("Eject Delay Milliseconds"));
                    if ( plist_time )
                    {
                        _eject_delay = plist_time->unsigned32BitValue();
                    }
                    clock_interval_to_deadline(_eject_delay, kMillisecondScale, &deadline);
                    thread_call_enter_delayed(_peject_timer, deadline);
                }
			}
			else
			{
				_eject_released = true;
				thread_call_cancel( _peject_timer );
				dispatchKeyboardEvent( kEject, FALSE, now );
			}
		}

	default:  //Don't dispatch anything else
	    break;
    }
}

static void check_eject_held(thread_call_param_t us)
{
   ((AppleADBButtons *)us)->_check_eject_held();
}

void AppleADBButtons::_check_eject_held( void ) 
{
    AbsoluteTime now;
    
    if (!_eject_released)
    {
	clock_get_uptime(&now);
	dispatchKeyboardEvent(kEject, TRUE, now);
    }
}


const unsigned char *AppleADBButtons::defaultKeymapOfLength(UInt32 *length)
{
    static const unsigned char appleADBButtonsKeyMap[] = {
        0x00, 0x00,	// chars
        0x00,		// no modifier keys
        0x00,		// no defs
        0x00,		// no seqs
        0x0B,		// 11 special keys
        NX_KEYTYPE_SOUND_UP, kVolume_up,
        NX_KEYTYPE_SOUND_DOWN, kVolume_down,
        NX_KEYTYPE_MUTE, kMute,
        NX_KEYTYPE_BRIGHTNESS_UP, kBrightness_up,
        NX_KEYTYPE_BRIGHTNESS_DOWN, kBrightness_down,
        NX_KEYTYPE_NUM_LOCK, kNum_lock_on_laptops,
        NX_KEYTYPE_EJECT, kEject,
        NX_KEYTYPE_VIDMIRROR, kVideoMirror,
        NX_KEYTYPE_ILLUMINATION_TOGGLE, kIllumination_toggle,
        NX_KEYTYPE_ILLUMINATION_DOWN, kIllumination_down,
        NX_KEYTYPE_ILLUMINATION_UP, kIllumination_up,
    };
    
    *length = sizeof(appleADBButtonsKeyMap);
    
    return appleADBButtonsKeyMap;
}

IOReturn AppleADBButtons::setParamProperties(OSDictionary *dict)
{
    dict->removeObject(kIOHIDKeyMappingKey);

    return super::setParamProperties(dict);
}
