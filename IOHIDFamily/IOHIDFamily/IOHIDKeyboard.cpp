/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * 13 Aug 2002 		ryepez
 *			This class is based off IOHIKeyboard and handles
 *			USB HID report based keyboard devices
 */

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>
#include <IOKit/hidsystem/IOHIDUsageTables.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/usb/USB.h>

#include "IOHIDKeyboard.h"
#include "IOHIDKeys.h"
#include "IOHIDElement.h"

#define super IOHIKeyboard

OSDefineMetaClassAndStructors(IOHIDKeyboard, IOHIKeyboard)

extern unsigned char hid_usb_2_adb_keymap[];  //In Cosmo_USB2ADB.cpp

IOHIDKeyboard * 
IOHIDKeyboard::Keyboard(OSArray *elements) 
{
    IOHIDKeyboard *keyboard = new IOHIDKeyboard;
    
    if ((keyboard == 0) || !keyboard->init() || 
            !keyboard->findDesiredElements(elements))
    {
        if (keyboard) keyboard->release();
        return 0;
    }

    return keyboard;
}


bool
IOHIDKeyboard::init(OSDictionary *properties)
{
  if (!super::init(properties))  return false;
    
    _oldmodifier = 0;  
    _asyncLEDThread = 0;
    _ledState = 0;
    _numLeds = 0;
    
    _keyCodeArrayElementCount = 0;
    _keyCodeArrayElementBitSize = 0;
    _keyCodeArrayValuePtr = 0;
    
    _ledCookies[0] = -1;
    _ledCookies[1] = -1;
    
    bzero(_modifierValuePtrs, sizeof(UInt32*)*8);
    bzero(_ledValuePtrs, sizeof(UInt32*)*8);
    
    //This makes separate copy of ADB translation table.  Needed to allow ISO
    //  keyboards to swap two keys without affecting non-ISO keys that are
    //  also connected, or that will be plugged in later through USB ports
    bcopy(hid_usb_2_adb_keymap, _usb_2_adb_keymap, ADB_CONVERTER_LEN);

      
  return true;
}


bool
IOHIDKeyboard::start(IOService *provider)
{
    OSNumber *xml_swap_CTRL_CAPSLOCK;
    OSNumber *productIDNumber;
    OSNumber *vendorIDNumber;

    if (!super::start(provider))
        return false;

    _provider = provider;
    
    productIDNumber = OSDynamicCast(OSNumber, 
                        _provider->getProperty(kIOHIDProductIDKey));
    vendorIDNumber = OSDynamicCast(OSNumber, 
                        _provider->getProperty(kIOHIDVendorIDKey));
                                
    _productID = productIDNumber ? productIDNumber->unsigned16BitValue() : 0;
    _vendorID = vendorIDNumber ? vendorIDNumber->unsigned16BitValue() : 0;

    // Fix hardware bug in iMac USB keyboard mapping for ISO keyboards
    // This should really be done in personalities.
    if ( ((_productID == kprodUSBAndyISOKbd) || (_productID == kprodUSBCosmoISOKbd)) 
            && (_vendorID == kIOUSBVendorIDAppleComputer))
    {
            _usb_2_adb_keymap[0x35] = 0x0a;  //Cosmo key18 swaps with key74, 0a is ADB keycode
            _usb_2_adb_keymap[0x64] = 0x32;
    }
        

    xml_swap_CTRL_CAPSLOCK = OSDynamicCast( OSNumber, provider->getProperty("Swap control and capslock"));
    if (xml_swap_CTRL_CAPSLOCK)
    {
	if ( xml_swap_CTRL_CAPSLOCK->unsigned32BitValue())
	{
	    char temp;
	    
	    temp = _usb_2_adb_keymap[0x39];  //Caps lock
            _usb_2_adb_keymap[0x39] = _usb_2_adb_keymap[0xe0];  //Left CONTROL modifier
            _usb_2_adb_keymap[0xe0] = temp;
	}
    }

    // Need separate thread to handle LED
    _asyncLEDThread = thread_call_allocate((thread_call_func_t)AsyncLED, (thread_call_param_t)this);
    
    return true;

}

void
IOHIDKeyboard::stop(IOService * provider)
{    
    if (_asyncLEDThread)
    {
	thread_call_cancel(_asyncLEDThread);
	thread_call_free(_asyncLEDThread);
	_asyncLEDThread = 0;
    }
    
    super::stop(provider);
}

void
IOHIDKeyboard::free()
{
    if (_oldArraySelectors)
        IOFree(_oldArraySelectors, sizeof(UInt32) * _keyCodeArrayElementCount);
    
    super::free();
}


bool
IOHIDKeyboard::findDesiredElements(OSArray *elements)
{
    IOHIDElement 	*element;
    UInt32		usage;
    
    if (!elements)
        return false;
    
    for (int i=0; i<elements->getCount(); i++)
    {
        element = elements->getObject(i);
        usage = element->getUsage();
        if (element->getUsagePage() == kHIDPage_KeyboardOrKeypad)
        {
            // Modifier Elements
            if ((usage >= kHIDUsage_KeyboardLeftControl) &&
                (usage <= kHIDUsage_KeyboardRightGUI) && 
                (_modifierValuePtrs[usage - kHIDUsage_KeyboardLeftControl] == 0))
            {
                _modifierValuePtrs[usage - kHIDUsage_KeyboardLeftControl] = 
                                                element->getElementValue()->value;
            }
            // Key Array Element
            else if (element->getUsage() == 0xffffffff) 
            {
                _keyCodeArrayValuePtr = element->getElementValue()->value;
                _keyCodeArrayElementBitSize = element->getReportBits();
                _keyCodeArrayElementCount = element->getReportCount();
                
                _oldArraySelectors = (UInt32 *)IOMalloc
                                (sizeof(UInt32) * _keyCodeArrayElementCount);
                                
                bzero(_oldArraySelectors, sizeof(UInt32) * _keyCodeArrayElementCount);
            }
        }
        else if (element->getUsagePage() == kHIDPage_LEDs)
        {
            if (((usage == kHIDUsage_LED_NumLock) || 
                (usage == kHIDUsage_LED_CapsLock)) &&
                (_ledValuePtrs[usage - kHIDUsage_LED_NumLock] == 0))
            {
                _ledValuePtrs[usage - kHIDUsage_LED_NumLock] = element->getElementValue()->value;
                _ledCookies[usage - kHIDUsage_LED_NumLock] = element->getElementCookie();
                _numLeds++;
            }
        }
    }
    
    return _keyCodeArrayValuePtr;
}

extern "C" { 
	void Debugger( const char * ); 
	void boot(int paniced, int howto, char * command);
#define RB_BOOT		1	/* Causes reboot, not halt.  Is in xnu/bsd/sys/reboot.h */

}

#define BIT_MASK(bits)  ((1 << (bits)) - 1)

#define UpdateWordOffsetAndShift(bits, offset, shift)  \
    do { offset = bits >> 5; shift = bits & 0x1f; } while (0)

static void getSelectors( const UInt32 * src,
                           UInt32 *       dst,
                           UInt32	  reportCount,
                           UInt32         bitSize)
{
    UInt32 dstOffset;
    UInt32 srcShift    = 0;
    UInt32 srcStartBit = 0;
    UInt32 srcOffset   = 0;
    UInt8  bitsProcessed;
    UInt32 tmp;
    UInt32 totalBits = bitSize * reportCount;

    while ( totalBits )
    {
        bitsProcessed = min( totalBits, bitSize );

        dst[dstOffset++] = (src[srcOffset] >> srcShift) & BIT_MASK(bitsProcessed);

        srcStartBit += bitsProcessed;
        totalBits  -= bitsProcessed;

        UpdateWordOffsetAndShift( srcStartBit, srcOffset, srcShift );
    }
}

void 
IOHIDKeyboard::handleReport()
{
    UInt8		modifier=0;
    UInt32		alpha = 0;
    UInt32		newArray[_keyCodeArrayElementCount];
    bool		found;
    AbsoluteTime	now;
    UInt8		seq_key, i;//counter for alpha keys pressed.


    // RY: We parse the report here to pick off the individual array
    // selector.  Since a arraySel can be as big as UInt32 we should
    // pick them off one at a time with getSelectors.
    bzero(newArray, (_keyCodeArrayElementCount * sizeof(UInt32)));

    getSelectors(_keyCodeArrayValuePtr, newArray, _keyCodeArrayElementCount, _keyCodeArrayElementBitSize);
    
    // Test for the keyboard bug where all the keys are 0x01. JDC.
    found = true;
    for (seq_key = 0; seq_key < _keyCodeArrayElementCount; seq_key++) {
      if (newArray[seq_key] != 1) found = false;
    }
    if (found) return;


    //Handle new key information.  The first byte is a set of bits describing
    //  which modifier keys are down.  The 2nd byte never seems to be used.
    //  The third byte is the first USB key down, and the fourth byte is the
    //  second key down, and so on.
    //When a key is released, there's no code... just a zero upon USB polling
    //8/2/99 A.W. fixed Blue Box's multiple modifier keys being pressed 
    //   simultaneously.  The trick is if a modifier key DOWN event is reported,
    //   and another DOWN is reported, then Blue Box loses track of it.  I must
    //   report a UP key event first, or else avoid resending the DOWN event.
    
    // Create modifier byte
    for (i = 0; i < 8; i++)
    {
        modifier |= _modifierValuePtrs[i][0] << i;
    }
     
    //SECTION 1. Handle modifier keys here first
    if (modifier == _oldmodifier) 
    {
        //Do nothing.  Same keys are still pressed, or if 0 then none pressed
	// so don't overload the HID system with useless events.
    }
    else //Modifiers may or may not be pressed right now
    {
	//kprintf("mod is %x\n", modifier);
        clock_get_uptime(&now);

        //left-hand CONTROL modifier key
        if ((modifier & kUSB_LEFT_CONTROL_BIT) && !(_oldmodifier & kUSB_LEFT_CONTROL_BIT))
        {
            dispatchKeyboardEvent(_usb_2_adb_keymap[0xe0], true, now);  //ADB left-hand CONTROL
	    _control_key = true;	//determine if we reboot CPU.  Is instance variable.
        }
	else if ((_oldmodifier & kUSB_LEFT_CONTROL_BIT) && !(modifier & kUSB_LEFT_CONTROL_BIT)
	    && !(modifier & kUSB_RIGHT_CONTROL_BIT))
	{
	    //Now check for released modifier keys.  Both right and left modifiers must be
	    //   checked otherwise Window Server thinks none are held down
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe0], false, now); 
	    _control_key = false;	
	}

        //right-hand CONTROL modifier
        if ((modifier & kUSB_RIGHT_CONTROL_BIT) && !(_oldmodifier & kUSB_RIGHT_CONTROL_BIT))
        {
            dispatchKeyboardEvent(_usb_2_adb_keymap[0xe4], true, now);  //right-hand CONTROL
	    _control_key = true;	//determine if we reboot CPU.  Is instance variable.
        }
	else if ((_oldmodifier & kUSB_RIGHT_CONTROL_BIT) && !(modifier & kUSB_RIGHT_CONTROL_BIT)
	    && !(modifier & kUSB_LEFT_CONTROL_BIT))
	{
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe4], false, now); 
	    _control_key = false;	
	}

        //left-hand SHIFT
        if ((modifier & kUSB_LEFT_SHIFT_BIT) && !(_oldmodifier & kUSB_LEFT_SHIFT_BIT))
        {
            dispatchKeyboardEvent(_usb_2_adb_keymap[0xe1], true, now);
        }
	else if ((_oldmodifier & kUSB_LEFT_SHIFT_BIT) && !(modifier & kUSB_LEFT_SHIFT_BIT)
	    && !(modifier & kUSB_RIGHT_SHIFT_BIT))
	{
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe1], false, now); 
	}

        //right-hand SHIFT
        if ((modifier & kUSB_RIGHT_SHIFT_BIT) && !(_oldmodifier & kUSB_RIGHT_SHIFT_BIT))
        {
            dispatchKeyboardEvent(_usb_2_adb_keymap[0xe5], true, now);
        }
	else if ((_oldmodifier & kUSB_RIGHT_SHIFT_BIT) && !(modifier & kUSB_RIGHT_SHIFT_BIT)
	    && !(modifier & kUSB_LEFT_SHIFT_BIT))
	{
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe5], false, now); 
	}

        if ((modifier & kUSB_LEFT_ALT_BIT) && !(_oldmodifier & kUSB_LEFT_ALT_BIT))
        {
            dispatchKeyboardEvent(_usb_2_adb_keymap[0xe2], true, now);
        }
	else if ((_oldmodifier & kUSB_LEFT_ALT_BIT) && !(modifier & kUSB_LEFT_ALT_BIT)
	    && !(modifier & kUSB_RIGHT_ALT_BIT))
	{
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe2], false, now); 
	}

        if ((modifier & kUSB_RIGHT_ALT_BIT) && !(_oldmodifier & kUSB_RIGHT_ALT_BIT))
        {
            dispatchKeyboardEvent(_usb_2_adb_keymap[0xe6], true, now);
        }
	else if ((_oldmodifier & kUSB_RIGHT_ALT_BIT) && !(modifier & kUSB_RIGHT_ALT_BIT)
	    && !(modifier & kUSB_LEFT_ALT_BIT))
	{
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe6], false, now); 
	}

        if ((modifier & kUSB_LEFT_FLOWER_BIT) && !(_oldmodifier & kUSB_LEFT_FLOWER_BIT))
        {
            dispatchKeyboardEvent(_usb_2_adb_keymap[0xe3], true, now);
	    _flower_key = true;	//determine if we go into kernel debugger, or reboot CPU
        }
	else if ((_oldmodifier & kUSB_LEFT_FLOWER_BIT) && !(modifier & kUSB_LEFT_FLOWER_BIT)
	    && !(modifier & kUSB_RIGHT_FLOWER_BIT))
	{
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe3], false, now); 
	    _flower_key = false;
	}

        if ((modifier & kUSB_RIGHT_FLOWER_BIT) && !(_oldmodifier & kUSB_RIGHT_FLOWER_BIT))
        {
            //dispatchKeyboardEvent(0x7e, true, now);
	    //WARNING... NeXT only recognizes left-hand flower key, so
	    //  emulate that for now
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe7], true, now);
	    _flower_key = true;	
        }
	else if ((_oldmodifier & kUSB_RIGHT_FLOWER_BIT) && !(modifier & kUSB_RIGHT_FLOWER_BIT)
	    && !(modifier & kUSB_LEFT_FLOWER_BIT))
	{
	    dispatchKeyboardEvent(_usb_2_adb_keymap[0xe7], false, now); 
	    _flower_key = false;
	}

    }

    //SECTION 2. Handle regular alphanumeric keys now.  Look first at previous keystrokes.
    //  Alphanumeric portion of HID report starts at byte +2.

    for (seq_key = 0; seq_key < _keyCodeArrayElementCount; seq_key++)
    {
        alpha = _oldArraySelectors[seq_key];
	if (alpha == 0) //No keys pressed
	{
	    continue;
	}
	found = false;
	for (i = 0; i < _keyCodeArrayElementCount; i++)  //Look through current keypresses
	{
            if (alpha == newArray[i])
	    {
		found = true;	//This key has been held down for a while, so do nothing.
		break;		//   Autorepeat is taken care of by IOKit.
	    }
	}
	if (!found)
	{
          //еее  if ( (alpha > 0x58) && ( alpha < 0x63 ) )
          //еее      USBLog(3,"Keypad %d pressed",(alpha-0x58));
                
	    clock_get_uptime(&now);

	    dispatchKeyboardEvent(_usb_2_adb_keymap[alpha], false, now);  //KEY UP
	}
    }

    //Now take care of KEY DOWN.  
    for (seq_key = 0; seq_key < _keyCodeArrayElementCount; seq_key++)
    {
        alpha = newArray[seq_key];
	if (alpha == 0) //No keys pressed
	{
	    continue;
	}
	else if (alpha == 0x66)	   //POWER ON key 
	{
	    if ((_control_key) && (_flower_key))  //User wants to reboot
	    {
		boot( RB_BOOT, 0, 0 );  //call to xnu/bsd/kern/kern_shutdown.c
	    }
	    if (_flower_key)  // Apple CMD modifier must be simultaneously held down
	    {
		PE_enter_debugger("USB Programmer Key");
		//In xnu/pexpert/ppc/pe_interrupt.c  and defined in pexpert.h above
	    }
	    //The reason this kernel debugger call is made here instead of KEY UP
	    //  is that the HID system will see the POWER key and bring up a
	    //  dialog box to shut down the computer, which is not what we want.
	}

	//Don't dispatch the same key again which was held down previously
	found = false;
	for (i = 0; i < _keyCodeArrayElementCount; i++)
	{
            if (alpha == _oldArraySelectors[i])
	    {
		found = true;
		break;
	    }
	}
	if (!found)
	{
       	    clock_get_uptime(&now);
	    //If Debugger() is triggered then I shouldn't show the restart dialog
	    //  box, but I think developers doing kernel debugging can live with
	    //  this minor incovenience.  Otherwise I need to do more checking here.
	    dispatchKeyboardEvent(_usb_2_adb_keymap[alpha], true, now);
	}
    }

    //Save the history for next time
    _oldmodifier = modifier;
    bcopy(newArray, _oldArraySelectors, sizeof(UInt32) * _keyCodeArrayElementCount);
}


// ***************************************************************************
// AsyncLED
//
// Called asynchronously to turn on/off the keyboard LED
//
// **************************************************************************
void 
IOHIDKeyboard::AsyncLED(OSObject *target)
{
    IOHIDKeyboard *me = OSDynamicCast(IOHIDKeyboard, target);

    me->Set_LED_States( me->_ledState ); 
}

void
IOHIDKeyboard::Set_LED_States(UInt8 ledState)
{
    IOHIDElementCookie	cookies[_numLeds];
    int			cookieCount = 0;
    
    for (int i=0; i<2; i++)
    {
        if (_ledValuePtrs[i])
        {
            _ledValuePtrs[i][0] = (ledState >> i) & 1;
            cookies[cookieCount++] = _ledCookies[i];
        }
    }
    
    _provider->postElementValues(cookies, cookieCount);
}

//******************************************************************************
// COPIED from ADB keyboard driver 3/25/99
// This is usually called on a call-out thread after the caps-lock key is pressed.
// ADB operations to PMU are synchronous, and this is must not be done
// on the call-out thread since that is the PMU driver workloop thread, and
// it will block itself.
//
// Therefore, we schedule the ADB write to disconnect the call-out thread
// and the one that initiates the ADB write.
//
// *******************************************************************************
void
IOHIDKeyboard::setAlphaLockFeedback ( bool LED_state)
{
    //*** TODO *** REVISIT ***
    UInt8	newState = _ledState;

    if (LED_state) //set alpha lock
	newState |= kUSB_CAPSLOCKLED_SET;   //2nd bit is caps lock on USB
    else
	newState &= ~kUSB_CAPSLOCKLED_SET;

    if (newState != _ledState)
    {
        _ledState = newState;
        thread_call_enter(_asyncLEDThread);
    }
}



void
IOHIDKeyboard::setNumLockFeedback ( bool LED_state)
{

    //*** TODO *** REVISIT ***
    UInt8	newState = _ledState;

    if (LED_state) 
	newState |= kUSB_NUMLOCKLED_SET;   //1st bit is num lock on USB
    else
	newState &= ~kUSB_NUMLOCKLED_SET;

    if (newState != _ledState)
    {
        _ledState = newState;
        thread_call_enter(_asyncLEDThread);
    }
}


//Called from parent classes
unsigned
IOHIDKeyboard::getLEDStatus (void )
{
    unsigned	ledState = 0;
    
    for (int i=0; i<2; i++)
    {
        if (_ledValuePtrs[i])
        {
            ledState |= _ledValuePtrs[i][0] << i;
        }
    }

}

// *****************************************************************************
// maxKeyCodes
// A.W. copied 3/25/99 from ADB keyboard driver, I don't know what this does
// ***************************************************************************
UInt32 
IOHIDKeyboard::maxKeyCodes (void )
{
    return 0x80;
}


// *************************************************************************
// deviceType
//
// **************************************************************************
UInt32 
IOHIDKeyboard::deviceType ( void )
{
    UInt32	id;	
    OSNumber 	*xml_handlerID;

    //Info.plist key is <integer>, not <string>
    xml_handlerID = OSDynamicCast( OSNumber, _provider->getProperty("alt_handler_id"));
    if (xml_handlerID)
    {
	id = xml_handlerID->unsigned32BitValue();
    }
    else
    {
	id = handlerID();
    }

    return id;

}

// ************************************************************************
// interfaceID.  Fake ADB for now since USB defaultKeymapOfLength is too complex
//
// **************************************************************************
UInt32 
IOHIDKeyboard::interfaceID ( void )
{
    //Return value must match "interface" line in .keyboard file
    return NX_EVS_DEVICE_INTERFACE_ADB;  // 2 This matches contents of AppleExt.keyboard
}


/***********************************************/
//Get handler ID 
//
//  I assume that this method is only called if a valid USB keyboard
//  is found. This method should return a 0 or something if there's
//  no keyboard, but then the USB keyboard driver should never have
//  been probed if there's no keyboard, so for now it won't return 0.
UInt32
IOHIDKeyboard::handlerID ( void )
{
    UInt32 ret_id = 2;  //Default for all unknown USB keyboards is 2

    // Return value must match "handler_id" line in .keyboard file
    // For some reason the ADB and PS/2 keyboard drivers are missing this method
    // Also, Beaker Keyboard Prefs doesn't run properly
    // Fix hardware bug in iMac USB keyboard mapping for ISO keyboards
    // kprintf("\nUSB product = %x, vendor = %x\n", _deviceDescriptor->product, _deviceDescriptor->vendor);
    // this should also be done in personalities
    if (_vendorID == kIOUSBVendorIDAppleComputer)
    {
        ret_id = _productID;
        if ((ret_id == kprodUSBAndyISOKbd) || (ret_id == kprodUSBCosmoISOKbd))
        {
            _usb_2_adb_keymap[0x35] = 0x0a;  //Cosmo key18 swaps with key74, 0a is ADB keycode
            _usb_2_adb_keymap[0x64] = 0x32;
            IOLog("IOHIDKeyboard::handlerID: ISO keys swapped.\n");
        }
    }

    if (_vendorID == 0x045e)  //Microsoft ID
    {
        if (_productID == 0x000b)   //Natural USB+PS/2 keyboard
            ret_id = 2;  //18 was OSX Server, now 2 is OSX Extended ADB keyboard, unknown manufacturer
    }

    //New feature for hardware identification using Gestalt.h values
    if (_vendorID == kIOUSBVendorIDAppleComputer)
    switch (ret_id)
    {
	case kprodUSBCosmoANSIKbd:  //Cosmo ANSI is 0x201
		ret_id = kgestUSBCosmoANSIKbd; //0xc6
		break;
	case kprodUSBCosmoISOKbd:  //Cosmo ISO
		ret_id = kgestUSBCosmoISOKbd; //0xc7
		break;
	case kprodUSBCosmoJISKbd:  //Cosmo JIS
		ret_id = kgestUSBCosmoJISKbd;  //0xc8
		break;
	case kprodUSBAndyANSIKbd:  //Andy ANSI is 0x204
		ret_id = kgestUSBAndyANSIKbd; //0xcc
		break;
	case kprodUSBAndyISOKbd:  //Andy ISO
		ret_id = kgestUSBAndyISOKbd; //0xcd
		break;
	case kprodUSBAndyJISKbd:  //Andy JIS is 0x206
		ret_id = kgestUSBAndyJISKbd; //0xce
		break;
	default:  // No Gestalt.h values, but still is Apple keyboard,
		  //   so return a generic Cosmo ANSI
		ret_id = kgestUSBCosmoANSIKbd;  
		break;
    }

    return ret_id;  //non-Apple USB keyboards should all return "2"
}


//Get key values from ev_keymap.h
bool 
IOHIDKeyboard:: doesKeyLock ( unsigned key)
{
    switch (key) {
	case NX_KEYTYPE_CAPS_LOCK:
		return false;
	case NX_KEYTYPE_NUM_LOCK:
		return false;
	default:
		return false;
    }
}


// *****************************************************************************
// defaultKeymapOfLength
// A.W. copied from ADB keyboard, I don't have time to make custom USB version
// *****************************************************************************
const unsigned char * 
IOHIDKeyboard::defaultKeymapOfLength (UInt32 * length )
{
    static const unsigned char appleUSAKeyMap[] = {
        0x00,0x00,
	0x06,   //Number of modifier keys.  Was 7
        //0x00,0x01,0x39,  //CAPSLOCK, uses one byte.
        0x01,0x01,0x38,
        0x02,0x01,0x3b,0x03,0x01,0x3a,0x04,
        0x01,0x37,0x05,0x15,0x52,0x41,0x4c,0x53,0x54,0x55,0x45,0x58,0x57,0x56,0x5b,0x5c,
        0x43,0x4b,0x51,0x7b,0x7d,0x7e,0x7c,0x4e,0x59,0x06,0x01,0x72,0x7f,0x0d,0x00,0x61,
        0x00,0x41,0x00,0x01,0x00,0x01,0x00,0xca,0x00,0xc7,0x00,0x01,0x00,0x01,0x0d,0x00,
        0x73,0x00,0x53,0x00,0x13,0x00,0x13,0x00,0xfb,0x00,0xa7,0x00,0x13,0x00,0x13,0x0d,
        0x00,0x64,0x00,0x44,0x00,0x04,0x00,0x04,0x01,0x44,0x01,0xb6,0x00,0x04,0x00,0x04,
        0x0d,0x00,0x66,0x00,0x46,0x00,0x06,0x00,0x06,0x00,0xa6,0x01,0xac,0x00,0x06,0x00,
            0x06,0x0d,0x00,0x68,0x00,0x48,0x00,0x08,0x00,0x08,0x00,0xe3,0x00,0xeb,0x00,0x00,
            0x18,0x00,0x0d,0x00,0x67,0x00,0x47,0x00,0x07,0x00,0x07,0x00,0xf1,0x00,0xe1,0x00,
            0x07,0x00,0x07,0x0d,0x00,0x7a,0x00,0x5a,0x00,0x1a,0x00,0x1a,0x00,0xcf,0x01,0x57,
            0x00,0x1a,0x00,0x1a,0x0d,0x00,0x78,0x00,0x58,0x00,0x18,0x00,0x18,0x01,0xb4,0x01,
            0xce,0x00,0x18,0x00,0x18,0x0d,0x00,0x63,0x00,0x43,0x00,0x03,0x00,0x03,0x01,0xe3,
            0x01,0xd3,0x00,0x03,0x00,0x03,0x0d,0x00,0x76,0x00,0x56,0x00,0x16,0x00,0x16,0x01,
            0xd6,0x01,0xe0,0x00,0x16,0x00,0x16,0x02,0x00,0x3c,0x00,0x3e,0x0d,0x00,0x62,0x00,
            0x42,0x00,0x02,0x00,0x02,0x01,0xe5,0x01,0xf2,0x00,0x02,0x00,0x02,0x0d,0x00,0x71,
            0x00,0x51,0x00,0x11,0x00,0x11,0x00,0xfa,0x00,0xea,0x00,0x11,0x00,0x11,0x0d,0x00,
            0x77,0x00,0x57,0x00,0x17,0x00,0x17,0x01,0xc8,0x01,0xc7,0x00,0x17,0x00,0x17,0x0d,
            0x00,0x65,0x00,0x45,0x00,0x05,0x00,0x05,0x00,0xc2,0x00,0xc5,0x00,0x05,0x00,0x05,
            0x0d,0x00,0x72,0x00,0x52,0x00,0x12,0x00,0x12,0x01,0xe2,0x01,0xd2,0x00,0x12,0x00,
            0x12,0x0d,0x00,0x79,0x00,0x59,0x00,0x19,0x00,0x19,0x00,0xa5,0x01,0xdb,0x00,0x19,
            0x00,0x19,0x0d,0x00,0x74,0x00,0x54,0x00,0x14,0x00,0x14,0x01,0xe4,0x01,0xd4,0x00,
            0x14,0x00,0x14,0x0a,0x00,0x31,0x00,0x21,0x01,0xad,0x00,0xa1,0x0e,0x00,0x32,0x00,
            0x40,0x00,0x32,0x00,0x00,0x00,0xb2,0x00,0xb3,0x00,0x00,0x00,0x00,0x0a,0x00,0x33,
            0x00,0x23,0x00,0xa3,0x01,0xba,0x0a,0x00,0x34,0x00,0x24,0x00,0xa2,0x00,0xa8,0x0e,
            0x00,0x36,0x00,0x5e,0x00,0x36,0x00,0x1e,0x00,0xb6,0x00,0xc3,0x00,0x1e,0x00,0x1e,
            0x0a,0x00,0x35,0x00,0x25,0x01,0xa5,0x00,0xbd,0x0a,0x00,0x3d,0x00,0x2b,0x01,0xb9,
            0x01,0xb1,0x0a,0x00,0x39,0x00,0x28,0x00,0xac,0x00,0xab,0x0a,0x00,0x37,0x00,0x26,
            0x01,0xb0,0x01,0xab,0x0e,0x00,0x2d,0x00,0x5f,0x00,0x1f,0x00,0x1f,0x00,0xb1,0x00,
            0xd0,0x00,0x1f,0x00,0x1f,0x0a,0x00,0x38,0x00,0x2a,0x00,0xb7,0x00,0xb4,0x0a,0x00,
            0x30,0x00,0x29,0x00,0xad,0x00,0xbb,0x0e,0x00,0x5d,0x00,0x7d,0x00,0x1d,0x00,0x1d,
            0x00,0x27,0x00,0xba,0x00,0x1d,0x00,0x1d,0x0d,0x00,0x6f,0x00,0x4f,0x00,0x0f,0x00,
            0x0f,0x00,0xf9,0x00,0xe9,0x00,0x0f,0x00,0x0f,0x0d,0x00,0x75,0x00,0x55,0x00,0x15,
            0x00,0x15,0x00,0xc8,0x00,0xcd,0x00,0x15,0x00,0x15,0x0e,0x00,0x5b,0x00,0x7b,0x00,
            0x1b,0x00,0x1b,0x00,0x60,0x00,0xaa,0x00,0x1b,0x00,0x1b,0x0d,0x00,0x69,0x00,0x49,
            0x00,0x09,0x00,0x09,0x00,0xc1,0x00,0xf5,0x00,0x09,0x00,0x09,0x0d,0x00,0x70,0x00,
            0x50,0x00,0x10,0x00,0x10,0x01,0x70,0x01,0x50,0x00,0x10,0x00,0x10,0x10,0x00,0x0d,
            0x00,0x03,0x0d,0x00,0x6c,0x00,0x4c,0x00,0x0c,0x00,0x0c,0x00,0xf8,0x00,0xe8,0x00,
            0x0c,0x00,0x0c,0x0d,0x00,0x6a,0x00,0x4a,0x00,0x0a,0x00,0x0a,0x00,0xc6,0x00,0xae,
            0x00,0x0a,0x00,0x0a,0x0a,0x00,0x27,0x00,0x22,0x00,0xa9,0x01,0xae,0x0d,0x00,0x6b,
            0x00,0x4b,0x00,0x0b,0x00,0x0b,0x00,0xce,0x00,0xaf,0x00,0x0b,0x00,0x0b,0x0a,0x00,
            0x3b,0x00,0x3a,0x01,0xb2,0x01,0xa2,0x0e,0x00,0x5c,0x00,0x7c,0x00,0x1c,0x00,0x1c,
            0x00,0xe3,0x00,0xeb,0x00,0x1c,0x00,0x1c,0x0a,0x00,0x2c,0x00,0x3c,0x00,0xcb,0x01,
            0xa3,0x0a,0x00,0x2f,0x00,0x3f,0x01,0xb8,0x00,0xbf,0x0d,0x00,0x6e,0x00,0x4e,0x00,
            0x0e,0x00,0x0e,0x00,0xc4,0x01,0xaf,0x00,0x0e,0x00,0x0e,0x0d,0x00,0x6d,0x00,0x4d,
            0x00,0x0d,0x00,0x0d,0x01,0x6d,0x01,0xd8,0x00,0x0d,0x00,0x0d,0x0a,0x00,0x2e,0x00,
            0x3e,0x00,0xbc,0x01,0xb3,0x02,0x00,0x09,0x00,0x19,0x0c,0x00,0x20,0x00,0x00,0x00,
            0x80,0x00,0x00,0x0a,0x00,0x60,0x00,0x7e,0x00,0x60,0x01,0xbb,0x02,0x00,0x7f,0x00,
            0x08,0xff,0x02,0x00,0x1b,0x00,0x7e,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	    0xff,0xff,0x00,0x00,0x2e,0xff,0x00,0x00,
            0x2a,0xff,0x00,0x00,0x2b,0xff,0x00,0x00,0x1b,0xff,0xff,0xff,0x0e,0x00,0x2f,0x00,
            0x5c,0x00,0x2f,0x00,0x1c,0x00,0x2f,0x00,0x5c,0x00,0x00,0x0a,0x00,0x00,0x00,0x0d, //XX03
            0xff,0x00,0x00,0x2d,0xff,0xff,0x0e,0x00,0x3d,0x00,0x7c,0x00,0x3d,0x00,0x1c,0x00,
            0x3d,0x00,0x7c,0x00,0x00,0x18,0x46,0x00,0x00,0x30,0x00,0x00,0x31,0x00,0x00,0x32,
            0x00,0x00,0x33,0x00,0x00,0x34,0x00,0x00,0x35,0x00,0x00,0x36,0x00,0x00,0x37,0xff,
            0x00,0x00,0x38,0x00,0x00,0x39,0xff,0xff,0xff,0x00,0xfe,0x24,0x00,0xfe,0x25,0x00,
            0xfe,0x26,0x00,0xfe,0x22,0x00,0xfe,0x27,0x00,0xfe,0x28,0xff,0x00,0xfe,0x2a,0xff,
            0x00,0xfe,0x32,0xff,0x00,0xfe,0x33,0xff,0x00,0xfe,0x29,0xff,0x00,0xfe,0x2b,0xff,
            0x00,0xfe,0x34,0xff,0x00,0xfe,0x2e,0x00,0xfe,0x30,0x00,0xfe,0x2d,0x00,0xfe,0x23,
            0x00,0xfe,0x2f,0x00,0xfe,0x21,0x00,0xfe,0x31,0x00,0xfe,0x20,
	    0x00,0x01,0xac, //ADB=0x7b is left arrow
	    0x00,0x01,0xae, //ADB = 0x7c is right arrow
	    0x00,0x01,0xaf, //ADB=0x7d is down arrow.  
	    0x00,0x01,0xad, //ADB=0x7e is up arrow	 
	    0x0f,0x02,0xff,0x04,            
            0x00,0x31,0x02,0xff,0x04,0x00,0x32,0x02,0xff,0x04,0x00,0x33,0x02,0xff,0x04,0x00,
            0x34,0x02,0xff,0x04,0x00,0x35,0x02,0xff,0x04,0x00,0x36,0x02,0xff,0x04,0x00,0x37,
            0x02,0xff,0x04,0x00,0x38,0x02,0xff,0x04,0x00,0x39,0x02,0xff,0x04,0x00,0x30,0x02,
            0xff,0x04,0x00,0x2d,0x02,0xff,0x04,0x00,0x3d,0x02,0xff,0x04,0x00,0x70,0x02,0xff,
            0x04,0x00,0x5d,0x02,0xff,0x04,0x00,0x5b,
0x05, // following are 7 special keys
0x04,0x39,  //caps lock
0x05,0x72,  //NX_KEYTYPE_HELP is 5, ADB code is 0x72
0x06,0x7f,  //NX_POWER_KEY is 6, ADB code is 0x7f
0x07,0x4a,  //NX_KEYTYPE_MUTE is 7, ADB code is 0x4a
// remove arrow keys as special keys. They are generating double up/down scroll events
// in both carbon and coco apps.
//0x08,0x7e,  //NX_UP_ARROW_KEY is 8, ADB is 3e raw, 7e virtual (KMAP)
//0x09,0x7d,  //NX_DOWN_ARROW_KEY is 9, ADB is 0x3d raw, 7d virtual
0x0a,0x47   //NX_KEYTYPE_NUM_LOCK is 10, ADB combines with CLEAR key for numlock
    };
 
*length = sizeof(appleUSAKeyMap);
return appleUSAKeyMap;
}
