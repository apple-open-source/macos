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

#include <IOKit/hidsystem/ev_keymap.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDUsageTables.h>

#include "IOHIDElement.h"
#include "IOHIDConsumer.h"


//====================================================================================================
// Defines
//====================================================================================================

#define super 			IOHIKeyboard

#define DEBUGGING_LEVEL 	0

#define InitButtons(ptrs) 			\
{						\
    ptrs = 0;					\
    ptrs ## Count = 0;				\	
}


#define AppendButtons(ptrs, ptr)		\
{						\
    UInt32 size = sizeof(UInt32 *) * ( ptrs ## Count); \
    UInt32 ** tempPtrs = IOMalloc(size + sizeof(UInt32 *)); \
                                                \
    if (ptrs) {					\
        bcopy( ptrs, tempPtrs, size );		\
        IOFree(ptrs, size);			\
    }						\
    ptrs = tempPtrs;				\
    ptrs[ ptrs ## Count ++] = ptr;		\
}


#define ReleaseButtons(ptrs)			\
{						\
    UInt32 size = sizeof(UInt32 *) * ( ptrs ## Count); \
    IOFree(ptrs, size);				\
    ptrs = 0;					\
    ptrs ## Count = 0;				\
}			

#define GetButtonValues(ptrs, value)		\
{						\
    for (int i=0; i< ptrs ## Count ; i++)	\
    {						\
        value |= (ptrs[i][0] & 0x1);		\
    }						\
}						

OSDefineMetaClassAndStructors( IOHIDConsumer, IOHIKeyboard )

//====================================================================================================
// Consumer - constructor
//====================================================================================================
IOHIDConsumer * 
IOHIDConsumer::Consumer(OSArray *elements) 
{
    IOHIDConsumer *consumer = new IOHIDConsumer;
    
    if ((consumer == 0) || !consumer->init() || 
            !consumer->findDesiredElements(elements))
    {
        if (consumer) consumer->release();
        return 0;
    }

    return consumer;
}

//====================================================================================================
// Dispatcher - constructor
//====================================================================================================
IOHIDConsumer * 
IOHIDConsumer::Dispatcher(IOService * owner) 
{
    IOHIDConsumer *consumer = new IOHIDConsumer;
    
    if ((consumer == 0) || !consumer->init())
    {
        if (consumer) consumer->release();
        return 0;
    }
    
    consumer->_isDispatcher 	= true;
    
    if ( OSDynamicCast(IOHIDKeyboard, owner) )
    {
        consumer->_keyboard = owner;
        consumer->_keyboard->retain();
    }
    
    return consumer;
}


//====================================================================================================
// init
//====================================================================================================
bool
IOHIDConsumer::init(OSDictionary *properties)
{
  if (!super::init(properties))  return false;
  
    bzero(_states, sizeof(bool) * NX_NUMSPECIALKEYS);

    _isDispatcher	= false;
    _vendorID		= 0;
    _productID		= 0;
    _locationID		= 0;
    _transport		= 0;
    _keyboard		= 0;

    _otherEventFlags 	= 0;
    _otherCapsLockOn 	= FALSE;
    
    _publishNotify 	= 0;
    _keyboardLock 	= IOLockAlloc(); 
        
    InitButtons(_systemPowerValuePtrs);
    InitButtons(_systemSleepValuePtrs);
    InitButtons(_systemWakeUpValuePtrs);
    
    InitButtons(_volumeIncValuePtrs);
    InitButtons(_volumeDecValuePtrs);
    InitButtons(_volumeMuteValuePtrs);

    InitButtons(_powerValuePtrs);
    InitButtons(_resetValuePtrs);
    InitButtons(_sleepValuePtrs);
    
    InitButtons(_playValuePtrs);
    InitButtons(_playOrPauseValuePtrs);
    InitButtons(_playOrSkipPtrs);
    InitButtons(_nextTrackValuePtrs);
    InitButtons(_prevTrackValuePtrs);
    InitButtons(_fastFowardValuePtrs);
    InitButtons(_rewindValuePtrs);
    InitButtons(_stopOrEjectPtrs);
    InitButtons(_ejectValuePtrs);

    return true;
}

void IOHIDConsumer::free()
{
    ReleaseButtons(_systemPowerValuePtrs);
    ReleaseButtons(_systemSleepValuePtrs);
    ReleaseButtons(_systemWakeUpValuePtrs);
    
    ReleaseButtons(_volumeIncValuePtrs);
    ReleaseButtons(_volumeDecValuePtrs);
    ReleaseButtons(_volumeMuteValuePtrs);

    ReleaseButtons(_powerValuePtrs);
    ReleaseButtons(_resetValuePtrs);
    ReleaseButtons(_sleepValuePtrs);
    
    ReleaseButtons(_playValuePtrs);
    ReleaseButtons(_playOrPauseValuePtrs);
    ReleaseButtons(_playOrSkipPtrs);
    ReleaseButtons(_nextTrackValuePtrs);
    ReleaseButtons(_prevTrackValuePtrs);
    ReleaseButtons(_fastFowardValuePtrs);
    ReleaseButtons(_rewindValuePtrs);
    ReleaseButtons(_stopOrEjectPtrs);
    ReleaseButtons(_ejectValuePtrs);

    if (_publishNotify) 
    {
        _publishNotify->remove();
    	_publishNotify = 0;
    }

    if (_keyboardLock)
    {
        IOLockLock(_keyboardLock);
        IOLock*	 tempLock = _keyboardLock;
	_keyboardLock = NULL;
        IOLockUnlock(tempLock);
	IOLockFree(tempLock);
    }


    super::free();
}

//====================================================================================================
// findDesiredElements
//====================================================================================================
bool
IOHIDConsumer::findDesiredElements(OSArray *elements)
{
    IOHIDElement 	*element;
    bool		found = false;
    
    if (!elements)
        return false;
    
    for (UInt32 i=0; i<elements->getCount(); i++)
    {
        element = (IOHIDElement *)elements->getObject(i);

        if (element->getUsagePage() == kHIDPage_Consumer)
        {
            switch(element->getUsage())
            {
                case kHIDUsage_Csmr_Power:
                    AppendButtons(_powerValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Reset:
                    AppendButtons(_resetValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Sleep:
                    AppendButtons(_sleepValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Play:
                    AppendButtons(_playValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_PlayOrPause:
                    AppendButtons(_playOrPauseValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_PlayOrSkip:
                    AppendButtons(_playOrSkipPtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_ScanNextTrack:
                    AppendButtons(_nextTrackValuePtrs, element->getElementValue()->value);
                     found = true;
                    break;
                case kHIDUsage_Csmr_ScanPreviousTrack:
                    AppendButtons(_prevTrackValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_FastForward:
                    AppendButtons(_fastFowardValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Rewind:
                    AppendButtons(_rewindValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_StopOrEject:
                    AppendButtons(_stopOrEjectPtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Eject:
                    AppendButtons(_ejectValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_VolumeIncrement:
                    AppendButtons(_volumeIncValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_VolumeDecrement:
                    AppendButtons(_volumeDecValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Mute:
                    AppendButtons(_volumeMuteValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                default:
                    break;
            }
        }
        else if (element->getUsagePage() == kHIDPage_GenericDesktop)
        {
            switch (element->getUsage())
            {
                case kHIDUsage_GD_SystemPowerDown:
                    AppendButtons(_systemPowerValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_GD_SystemSleep:
                    AppendButtons(_systemSleepValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_GD_SystemWakeUp:
                    AppendButtons(_systemWakeUpValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
            }
        }
    }
        
    return found;
}

//====================================================================================================
// start
//====================================================================================================
bool IOHIDConsumer::start(IOService * provider)
{
    _transport	= OSDynamicCast(OSString,provider->getProperty(kIOHIDTransportKey));
    _vendorID	= OSDynamicCast(OSNumber,provider->getProperty(kIOHIDVendorIDKey));
    _productID	= OSDynamicCast(OSNumber,provider->getProperty(kIOHIDProductIDKey));
    _locationID	= OSDynamicCast(OSNumber,provider->getProperty(kIOHIDLocationIDKey));
    
    setProperty(kIOHIDTransportKey, _transport);
    setProperty(kIOHIDVendorIDKey, _vendorID);
    setProperty(kIOHIDProductIDKey, _productID);
    setProperty(kIOHIDLocationIDKey, _locationID);

    // If this service is a dispatcher, the the _keyboard should have
    // been set and thus no need for the notification.
    if ( !_keyboard )
    {
        OSDictionary * matchingDictionary = IOService::serviceMatching( "IOHIDKeyboard" );
        
        if( matchingDictionary )
        {
            matchingDictionary->setObject(kIOHIDTransportKey, _transport);
            matchingDictionary->setObject(kIOHIDVendorIDKey, _vendorID);
            matchingDictionary->setObject(kIOHIDProductIDKey, _productID);
            matchingDictionary->setObject(kIOHIDLocationIDKey, _locationID);
    
            _publishNotify = addNotification( gIOPublishNotification, 
                                matchingDictionary,
                                &IOHIDConsumer::_publishNotificationHandler,
                                this, 0 );                                
        }
    }

    if (!super::start(provider))
        return false;
    
    return true;
}

//====================================================================================================
// stop
//====================================================================================================
void IOHIDConsumer::stop(IOService * provider)
{    
    IOLockLock(_keyboardLock);
    if( _keyboard) {
        _keyboard->release();
        _keyboard = 0;
    }
    IOLockUnlock(_keyboardLock);
    
    super::stop(provider);
}

//====================================================================================================
// handleReport
//====================================================================================================
void IOHIDConsumer::handleReport()
{
    AbsoluteTime		now;
    
    bool			newStates[NX_NUMSPECIALKEYS];
    
    bzero(newStates, sizeof(bool) * NX_NUMSPECIALKEYS);
    
    // Record current time for the keypress posting.
    clock_get_uptime( &now );

    // If we aren't associated with a keyboard, 
    // grab the modifiers from all keyboards.
    IOLockLock(_keyboardLock);
    
    if (!_keyboard) findKeyboardsAndGetModifiers();
    
    IOLockUnlock(_keyboardLock);
    
    // Check and see if the states have changed since last report, if so, we'll notify whoever
    // cares by posting the appropriate key and keystates.
    GetButtonValues(_powerValuePtrs, newStates[NX_POWER_KEY]);
    GetButtonValues(_resetValuePtrs, newStates[NX_POWER_KEY]);
    GetButtonValues(_sleepValuePtrs, newStates[NX_POWER_KEY]);
    GetButtonValues(_systemPowerValuePtrs, newStates[NX_POWER_KEY]);
    GetButtonValues(_systemSleepValuePtrs, newStates[NX_POWER_KEY]);
    GetButtonValues(_systemWakeUpValuePtrs, newStates[NX_POWER_KEY]);

    GetButtonValues(_playValuePtrs, newStates[NX_KEYTYPE_PLAY]);
    GetButtonValues(_playOrPauseValuePtrs, newStates[NX_KEYTYPE_PLAY]);
    GetButtonValues(_playOrSkipPtrs, newStates[NX_KEYTYPE_PLAY]);
    
    GetButtonValues(_ejectValuePtrs, newStates[NX_KEYTYPE_EJECT]);

    GetButtonValues(_volumeMuteValuePtrs, newStates[NX_KEYTYPE_MUTE]);

    GetButtonValues(_volumeIncValuePtrs, newStates[NX_KEYTYPE_SOUND_UP]);

    GetButtonValues(_volumeDecValuePtrs, newStates[NX_KEYTYPE_SOUND_DOWN]);

    GetButtonValues(_fastFowardValuePtrs, newStates[NX_KEYTYPE_FAST]);

    GetButtonValues(_rewindValuePtrs, newStates[NX_KEYTYPE_REWIND]);

    GetButtonValues(_nextTrackValuePtrs, newStates[NX_KEYTYPE_NEXT]);

    GetButtonValues(_prevTrackValuePtrs, newStates[NX_KEYTYPE_PREVIOUS]);

    for (int i=0; i<NX_NUMSPECIALKEYS; i++)
    {
        switch (i)
        {
            case NX_KEYTYPE_MUTE:
            case NX_KEYTYPE_SOUND_UP:
            case NX_KEYTYPE_SOUND_DOWN:
                if (newStates[NX_KEYTYPE_MUTE] && newStates[NX_KEYTYPE_SOUND_UP] && 
                        newStates[NX_KEYTYPE_SOUND_DOWN] )
                {
                    break;
                }
            default:
                if ( newStates[i] != _states[i] )
                {
                    dispatchKeyboardEvent( i, newStates[i], now );
                }
                break;
        }
    }
            
    // Save newStates for our next report.
    bcopy(newStates, _states, sizeof(newStates));
}

//====================================================================================================
// dispatchSpecialKeyEvent - Allows attached keyboard to dispatch special key events.  Will be used
// by usb keyboard in portables.
//====================================================================================================
void IOHIDConsumer::dispatchSpecialKeyEvent(int key, bool down, AbsoluteTime ts)
{
    dispatchKeyboardEvent( key, down, ts );
}

//====================================================================================================
// eventFlags - IOHIKeyboard override. This is necessary because we will need to return the state
// of the modifier keys on attached keyboards. If we don't, then we the HIDSystem gets
// the event, it will look like all modifiers are off.
//====================================================================================================
unsigned IOHIDConsumer::eventFlags()
{
    unsigned flags = 0;
    
    IOLockLock(_keyboardLock);
    
    flags = (_keyboard) ? _keyboard->eventFlags() : _otherEventFlags;
    
    IOLockUnlock(_keyboardLock);

    return( flags );
}

//====================================================================================================
// alphaLock  - IOHIKeyboard override. This is necessary because we will need to return the state
// of the caps lock keys on attached keyboards. If we don't, then we the HIDSystem gets
// the event, it will look like caps lock keys are off.
//====================================================================================================
bool IOHIDConsumer::alphaLock()
{
    bool state = false;
    
    IOLockLock(_keyboardLock);
    
    state = (_keyboard) ? _keyboard->alphaLock() : _otherCapsLockOn;
    
    IOLockUnlock(_keyboardLock);

    return( state );
}

//====================================================================================================
// setNumLock  - IOHIKeyboard override. This is necessary because we will need to toggle the num lock
// led on the keyboard interface
//====================================================================================================
void IOHIDConsumer::setNumLock(bool val)
{    
    IOLockLock(_keyboardLock);
    
    if (_keyboard) _keyboard->setNumLock(val);
    
    IOLockUnlock(_keyboardLock);
}

//====================================================================================================
// numLock  - IOHIKeyboard override. This is necessary because we will need to check the num lock
// status on the keyboard interface
//====================================================================================================
bool IOHIDConsumer::numLock()
{
    bool state = false;
    
    IOLockLock(_keyboardLock);
    
    state = (_keyboard) ? _keyboard->numLock() : super::numLock();
    
    IOLockUnlock(_keyboardLock);

    return( state );
}

//====================================================================================================
// doesKeyLock  - IOHIKeyboard override. This is necessary because the system will only toggle the led
// and set appropriate event flags if the num lock key physically locks.
//====================================================================================================
bool IOHIDConsumer:: doesKeyLock ( unsigned key)
{
    if ( key == NX_KEYTYPE_NUM_LOCK )
        return true;
    
    return false;
}

UInt32 IOHIDConsumer::deviceType()
{
    UInt32 type = 0;
    
    IOLockLock(_keyboardLock);
    
    type = (_keyboard) ? _keyboard->deviceType() : super::deviceType();
    
    IOLockUnlock(_keyboardLock);

    return( type );
}



//====================================================================================================
// defaultKeymapOfLength - IOHIKeyboard override
// This allows us to associate the scancodes we choose with the special
// keys we are interested in posting later. This gives us auto-repeats for free. Kewl.
//====================================================================================================
const unsigned char * IOHIDConsumer::defaultKeymapOfLength( UInt32 * length )
{
    static const unsigned char ConsumerKeyMap[] =
    {
		// The first 16 bits are always read first, to determine if the rest of
        // the keymap is in shorts (16 bits) or bytes (8 bits). If the first 16 bits
        // equals 0, data is in bytes; if first 16 bits equal 1, data is in shorts.
        
        0x00,0x00,		// data is in bytes

        // The next value is the number of modifier keys. We have none in our driver.

        0x00,
        
        // The next value is number of key definitions. We have none in our driver.
        
        0x00,
        
        // The next value is number of of sequence definitions there are. We have none.
        
        0x00,
        
        // The next value is the number of special keys. We use these.
        
        NX_NUMSPECIALKEYS,
        
        // Special Key	  	SCANCODE
        //-----------------------------------------------------------        
                
        NX_KEYTYPE_SOUND_UP,		NX_KEYTYPE_SOUND_UP,
        NX_KEYTYPE_SOUND_DOWN,		NX_KEYTYPE_SOUND_DOWN,
        NX_KEYTYPE_BRIGHTNESS_UP,	NX_KEYTYPE_BRIGHTNESS_UP,
        NX_KEYTYPE_BRIGHTNESS_DOWN,	NX_KEYTYPE_BRIGHTNESS_DOWN,
        NX_KEYTYPE_CAPS_LOCK,		NX_KEYTYPE_CAPS_LOCK,
        NX_KEYTYPE_HELP,		NX_KEYTYPE_HELP,
        NX_POWER_KEY,			NX_POWER_KEY,
        NX_KEYTYPE_MUTE,		NX_KEYTYPE_MUTE,
        NX_UP_ARROW_KEY,		NX_UP_ARROW_KEY,
        NX_DOWN_ARROW_KEY,		NX_DOWN_ARROW_KEY,
        NX_KEYTYPE_NUM_LOCK,		NX_KEYTYPE_NUM_LOCK,
        NX_KEYTYPE_CONTRAST_UP,		NX_KEYTYPE_CONTRAST_UP,
        NX_KEYTYPE_CONTRAST_DOWN,	NX_KEYTYPE_CONTRAST_DOWN,
        NX_KEYTYPE_LAUNCH_PANEL,	NX_KEYTYPE_LAUNCH_PANEL,
        NX_KEYTYPE_EJECT,		NX_KEYTYPE_EJECT,
        NX_KEYTYPE_VIDMIRROR,		NX_KEYTYPE_VIDMIRROR,
        NX_KEYTYPE_PLAY,		NX_KEYTYPE_PLAY,
        NX_KEYTYPE_NEXT,		NX_KEYTYPE_NEXT,
        NX_KEYTYPE_PREVIOUS,		NX_KEYTYPE_PREVIOUS,
        NX_KEYTYPE_FAST,		NX_KEYTYPE_FAST,
        NX_KEYTYPE_REWIND,		NX_KEYTYPE_REWIND,
        NX_KEYTYPE_ILLUMINATION_UP,	NX_KEYTYPE_ILLUMINATION_UP,
        NX_KEYTYPE_ILLUMINATION_DOWN,	NX_KEYTYPE_ILLUMINATION_DOWN,
        NX_KEYTYPE_ILLUMINATION_TOGGLE,	NX_KEYTYPE_ILLUMINATION_TOGGLE

    };
    
 
    if( length ) *length = sizeof( ConsumerKeyMap );
    
    return( ConsumerKeyMap );
}

//====================================================================================================
// findKeyboardsAndGetModifiers
//====================================================================================================
UInt32 IOHIDConsumer::findKeyboardsAndGetModifiers()
{
	OSIterator	*iterator = NULL;
	OSDictionary	*matchingDictionary = NULL;
	IOHIKeyboard	*device = NULL;	
	
	_otherEventFlags = 0;
	_otherCapsLockOn = FALSE;
	
	// Get matching dictionary.
	
	matchingDictionary = IOService::serviceMatching( "IOHIKeyboard" );
	if( !matchingDictionary )
	{
		goto exit;
	}
	
	// Get an iterator for the IOHIKeyboard devices.
	
	iterator = IOService::getMatchingServices( matchingDictionary );
	if( !iterator )
	{
		goto exit;
	}
	
	// User iterator to find devices and eject.
	//
	while( (device = (IOHIKeyboard*) iterator->getNextObject()) )
	{		
		
		//Ignore the eventFlags keyboards that don't have modifiers defined
		if (!device->getProperty(kIOHIDKeyboardSupportedModifiersKey))
			continue;
		
		// Save the caps lock state. If more than one keyboard has it down, that's fine -- we
		// just want to know if ANY keyboards have the key down.
		//
		if( device->alphaLock() )
		{
			_otherCapsLockOn = TRUE;
		}

		// OR in the flags, so we get a combined IOHIKeyboard device flags state. That
		// way, if the user is pressing command on one keyboard, shift on another, and
		// then hits an eject key, we'll get both modifiers.
		//
		_otherEventFlags |= device->eventFlags();
	}
	
exit:
	
	if( matchingDictionary ) matchingDictionary->release();
	if( iterator ) iterator->release();
	
	return( _otherEventFlags );
}

//====================================================================================================
// _publishNotificationHandler
//====================================================================================================
bool IOHIDConsumer::_publishNotificationHandler(
			void * target,
			void * /* ref */,
			IOService * newService )
{
    IOHIDConsumer * self = (IOHIDConsumer *) target;

    IOLockLock(self->_keyboardLock);
    
    if( OSDynamicCast(IOHIDKeyboard,newService) && !self->_keyboard ) 
    {
        
        self->_keyboard = newService;
        self->_keyboard->retain();
       
	if ( self->_publishNotify )
	{ 
            self->_publishNotify->remove();
            self->_publishNotify = 0;
	}
    }
    
    IOLockUnlock(self->_keyboardLock);

    return true;
}

//---------------------------------------------------------------------------
// Compare the properties in the supplied table to this object's properties.

static bool CompareProperty( IOService * owner, OSDictionary * matching, const char * key )
{
    // We return success if we match the key in the dictionary with the key in
    // the property table, or if the prop isn't present
    //
    OSObject 	* value;
    bool	matches;
    
    value = matching->getObject( key );

    if( value)
        matches = value->isEqualTo( owner->getProperty( key ));
    else
        matches = true;

    return matches;
}

//====================================================================================================
// matchPropertyTable
//====================================================================================================
bool IOHIDConsumer::matchPropertyTable(OSDictionary * table, SInt32 * score)
{
    bool match = true;

    // Ask our superclass' opinion.
    if (super::matchPropertyTable(table, score) == false)  return false;

    // Compare properties.        
    if (!CompareProperty(this, table, kIOHIDLocationIDKey) 	||
        !CompareProperty(this, table, kIOHIDTransportKey) 	||
        !CompareProperty(this, table, kIOHIDVendorIDKey) 	||
        !CompareProperty(this, table, kIOHIDProductIDKey))
        match = false;

    return match;
}

