/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <libkern/OSByteOrder.h>
extern "C" {
#include <pexpert/pexpert.h>
}

#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBKeyboard.h"

#define super IOHIKeyboard
#define self this

OSDefineMetaClassAndStructors(AppleUSBKeyboard, IOHIKeyboard)

extern unsigned char def_usb_2_adb_keymap[];  //In Cosmo_USB2ADB.cpp


bool
AppleUSBKeyboard::init(OSDictionary *properties)
{
  if (!super::init(properties))  return false;
  
    _prevent_LED_set = false;
    _ledState = 0;
    _deviceIsDead = false;
    _deviceHasBeenDisconnected = false;
    _retryCount = kKeyboardRetryCount;
    prev_bytes_read=8;
    oldmodifier = 0;
    _outstandingIO = 0;
    _needToClose = false;

    bzero( old_array, kUSB_LOWSPEED_MAXPACKET);
    
    //This makes separate copy of ADB translation table.  Needed to allow ISO
    //  keyboards to swap two keys without affecting non-ISO keys that are
    //  also connected, or that will be plugged in later through USB ports
    bcopy(def_usb_2_adb_keymap, usb_2_adb_keymap, ADB_CONVERTER_LEN);

      
  return true;
}



bool 
AppleUSBKeyboard::start(IOService * provider)
{
    IOReturn			err = 0;
    UInt16			productID;
    OSNumber 			*xml_swap_CTRL_CAPSLOCK;
    IOWorkLoop			*wl;
    
    USBLog(3, "%s[%p]::start - beginning - retain count = %d", getName(), this, getRetainCount());
    IncrementOutstandingIO();			// make sure we don't close this before we are done starting

    _interface = OSDynamicCast(IOUSBInterface, provider);
    if (!_interface)
    {
	USBError(1, "%s[%p]::start - Provider is not an IOUSBInterface! - something must be wrong", getName(), this);
        goto ErrorExit;
    }

    _device = _interface->GetDevice();
    if (!_device)
    {
	USBError(1, "%s[%p]::start - unable to get handle to my device - something must be wrong", getName(), this);
        goto ErrorExit;
    }
    
    if( !_interface->open(this))
    {
        USBError(1, "%s[%p]::start - unable to open provider. returning false", getName(), this);
        goto ErrorExit;
    }

    // Fix hardware bug in iMac USB keyboard mapping for ISO keyboards
    // This should really be done in personalities.
    if ( _device->GetVendorID() == kIOUSBVendorIDAppleComputer)
    {
        productID = _device->GetProductID();
        if ((productID == kprodUSBAndyISOKbd) || (productID == kprodUSBCosmoISOKbd) || ( productID == kprodQ6ISOKbd) || (productID == kprodUSBProF16ISOKbd) ) 
        {
            usb_2_adb_keymap[0x35] = 0x0a;  //Cosmo key18 swaps with key74, 0a is ADB keycode
            usb_2_adb_keymap[0x64] = 0x32;
        }
        
        Set_Idle_Millisecs(0);
    }
    else
    {
        Set_Idle_Millisecs(24);
    }

    xml_swap_CTRL_CAPSLOCK = OSDynamicCast( OSNumber, getProperty("Swap control and capslock"));
    if (xml_swap_CTRL_CAPSLOCK)
    {
	if ( xml_swap_CTRL_CAPSLOCK->unsigned32BitValue())
	{
	    char temp;
	    
	    temp = usb_2_adb_keymap[0x39];  //Caps lock
            usb_2_adb_keymap[0x39] = usb_2_adb_keymap[0xe0];  //Left CONTROL modifier
            usb_2_adb_keymap[0xe0] = temp;
	}
    }

    
    do {

        _gate = IOCommandGate::commandGate(this);

        if(!_gate)
        {
            USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
            break;
        }

	wl = getWorkLoop();
	if (!wl)
	{
            USBError(1, "%s[%p]::start - unable to find my workloop", getName(), this);
            break;
	}
	
        if (wl->addEventSource(_gate) != kIOReturnSuccess)
        {
            USBError(1, "%s[%p]::start - unable to add gate to work loop", getName(), this);
            break;
        }

        IOUSBFindEndpointRequest request;
        request.type = kUSBInterrupt;
        request.direction = kUSBIn;
        _interruptPipe = _interface->FindNextPipe(NULL, &request);

        if(!_interruptPipe)
        {
            USBError(1, "%s[%p]::start - unable to get interrupt pipe", getName(), this);
            break;
        }

        _maxPacketSize = request.maxPacketSize;
        _buffer = IOBufferMemoryDescriptor::withCapacity(_maxPacketSize, kIODirectionIn);
        if ( !_buffer )
        {
            USBError(1, "%s[%p]::start - unable to create buffer", getName(), this);
            break;
        }

        _completion.target = (void *)self;
        _completion.action = (IOUSBCompletionAction) &AppleUSBKeyboard::InterruptReadHandlerEntry;
        _completion.parameter = (void *)0;  // not used
        IncrementOutstandingIO();
	USBLog(3, "%s[%p]::start - issuing initial read", getName(), this);
	
        _buffer->setLength(_maxPacketSize); // shouldn't this be _maxPacketSize?

        if ((err = _interruptPipe->Read(_buffer, &_completion)))
        {
            DecrementOutstandingIO();
            USBError(1, "%s[%p]::start - err (%x) in interrupt read, retain count %d after release", getName(), this, err, getRetainCount());
            break;
        }
    
         // allocate a thread_call structure
        _deviceDeadCheckThread = thread_call_allocate((thread_call_func_t)CheckForDeadDeviceEntry, (thread_call_param_t)this);
        _clearFeatureEndpointHaltThread = thread_call_allocate((thread_call_func_t)ClearFeatureEndpointHaltEntry, (thread_call_param_t)this);
        _asyncLEDThread = thread_call_allocate((thread_call_func_t)AsyncLED, (thread_call_param_t)this);
	
        if ( !_deviceDeadCheckThread || !_clearFeatureEndpointHaltThread || !_asyncLEDThread)
        {
            USBLog(3, "[%s]%p: could not allocate all thread functions", getName(), this);
            break;
        }

        USBError(1, "%s[%p]::start USB Generic Keyboard @ %d (0x%x)", getName(), this, _interface->GetDevice()->GetAddress(), strtol(_interface->GetDevice()->getLocation(), (char **)NULL, 16));

	// OK- so this is not totally kosher in the IOKit world. You are supposed to call super::start near the BEGINNING
	// of your own start method. However, the IOHIKeyboard::start method invokes registerService, which we don't want to
	// do if we get any error up to this point. So we wait and call IOHIKeyboard::start here.
	if( !super::start(_interface))
	{
	    USBError(1, "%s[%p]::start - unable to start superclass. returning false", getName(), this);
	    break;	// error
	}
        
        DecrementOutstandingIO();			// release the hold
        return true;

    } while (false);

    USBLog(3, "%s[%p]::start aborting.  err = 0x%x", getName(), this, err);

    // We MAY NOT touch the interrupt pipe once we have closed our provider
    if ( _interruptPipe )
    {
	_interruptPipe->Abort();
	_interruptPipe = NULL;
    }

    provider->close(this);
    stop(provider);

ErrorExit:
        
    DecrementOutstandingIO();
    return false;
}



void
AppleUSBKeyboard::stop(IOService * provider)
{

    if (_buffer) 
    {
	_buffer->release();
        _buffer = NULL;
    }

    if (_deviceDeadCheckThread)
    {
        thread_call_cancel(_deviceDeadCheckThread);
        thread_call_free(_deviceDeadCheckThread);
	_deviceDeadCheckThread = 0;
    }
    
    if (_clearFeatureEndpointHaltThread)
    {
        thread_call_cancel(_clearFeatureEndpointHaltThread);
        thread_call_free(_clearFeatureEndpointHaltThread);
	_clearFeatureEndpointHaltThread = 0;
    }
    
    if (_asyncLEDThread)
    {
	thread_call_cancel(_asyncLEDThread);
	thread_call_free(_asyncLEDThread);
	_asyncLEDThread = 0;
    }
    
    if (_gate)
    {
	IOWorkLoop	*wl = getWorkLoop();
	if (wl)
	    wl->removeEventSource(_gate);
	_gate->release();
	_gate = NULL;
    }
    
    super::stop(provider);
}



void
AppleUSBKeyboard::Set_Idle_Millisecs(UInt16 msecs)
{
    IOReturn    	err = kIOReturnSuccess;
    
    _request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    _request.bRequest = kHIDRqSetIdle;  //See USBSpec.h
    _request.wValue = (msecs/4) << 8;
    _request.wIndex = _interface->GetInterfaceNumber();
    _request.wLength = 0;
    _request.pData = NULL;

    err = _device->DeviceRequest(&_request, 5000, 0);
    if ( err )
    {
        USBLog(3, "%s[%p]: Set_Idle_Millisecs returned error 0x%x",getName(), this, err);
    }
    
    return;

}


//bits = 2 is Caps Lock, I haven't tried anything else.  0 clears all
void
AppleUSBKeyboard::Set_LED_States(UInt8 bits)
{
    IOReturn    err = kIOReturnSuccess;


    if (_prevent_LED_set)
        return; 

    //_request and _hid_report must be static because the deviceRequest() call
    // is asynchronous.

    _request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    _request.bRequest = kHIDRqSetReport;  //See USBSpec.h
    _request.wValue = kHIDRtOutputReport << 8;
    _request.wIndex = _interface->GetInterfaceNumber();
    _request.wLength = 1;

    _hid_report[0] = bits;  // 2 to set LED, 0 to clear all LEDs
    _hid_report[1] = 0;
    _request.pData = (void *)_hid_report;

    err = _device->DeviceRequest(&_request, 5000, 0);
    if ( err )
    {
        USBLog(3, "%s[%p]: Set_LED_States returned error 0x%x",getName(), this, err);
        _prevent_LED_set = TRUE;
    }
    
    return;
}


//Get the LED states.  Bit 1 is numlock, bit 2 is caps lock, bit 3 is scroll lock
void 
AppleUSBKeyboard::Get_LED_States()
{
    IOReturn    err = kIOReturnSuccess;

    if (_prevent_LED_set)
        return; 

    //_request and _hid_report must be static because the deviceRequest() call
    // is asynchronous.

    _request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
    _request.bRequest = kHIDRqGetReport;  //See USBSpec.h
    _request.wValue = kHIDRtInputReport << 8;
    _request.wIndex = _interface->GetInterfaceNumber();
    _request.wLength = 1;

    _request.pData = (void *)_hid_report; //value is returned in _hid_report[]

    err = _device->DeviceRequest(&_request, 5000, 0);
    if ( err )
    {
        USBLog(3, "%s[%p]: Get_LED_States returned error 0x%x",getName(), this, err);
    }
    
    return;

}



extern "C" { 
	void Debugger( const char * ); 
	void boot(int paniced, int howto, char * command);
#define RB_BOOT		1	/* Causes reboot, not halt.  Is in xnu/bsd/sys/reboot.h */

}


//This helper function is only called by StartHandler below.  key_ptr points
// to 8 (kUSB_LOWSPEED_MAXPACKET) valid bytes of data
void 
AppleUSBKeyboard::Simulate_ADB_Event(UInt8 *key_ptr, UInt32 bytes_read)
{
    UInt8		alpha, modifier=0;
    bool		found;
    AbsoluteTime	now;
    UInt8		seq_key, i;//counter for alpha keys pressed.

/**
UInt8		*kbdData;
kbdData = key_ptr;
kprintf("Num = %d x%x  x%x  x%x  x%x  x%x  x%x  x%x  x%x\n",
bytes_read, *kbdData, *(kbdData +1),
*(kbdData+2), *(kbdData +3),
*(kbdData+4), *(kbdData +5),
*(kbdData+6), *(kbdData +7)
);
**/

    // Test for the keyboard bug where all the keys are 0x01. JDC.
    found = true;
    for (seq_key = 2; seq_key < prev_bytes_read; seq_key++) {
      if (*(key_ptr + seq_key) != 1) found = false;
    }
    if (found) return;


    if (bytes_read > kUSB_LOWSPEED_MAXPACKET)  // 8 bytes
    {
	bytes_read = kUSB_LOWSPEED_MAXPACKET;  //Limit myself to low-speed keyboards
    }
    modifier = *key_ptr;
    //alpha = *(key_ptr +2);  // byte +1 seems to be unused
    //adb_code = usb_2_adb_keymap[alpha];



    //Handle new key information.  The first byte is a set of bits describing
    //  which modifier keys are down.  The 2nd byte never seems to be used.
    //  The third byte is the first USB key down, and the fourth byte is the
    //  second key down, and so on.
    //When a key is released, there's no code... just a zero upon USB polling
    //8/2/99 A.W. fixed Blue Box's multiple modifier keys being pressed 
    //   simultaneously.  The trick is if a modifier key DOWN event is reported,
    //   and another DOWN is reported, then Blue Box loses track of it.  I must
    //   report a UP key event first, or else avoid resending the DOWN event.

 
    //SECTION 1. Handle modifier keys here first
    if (modifier == oldmodifier) 
    {
        //Do nothing.  Same keys are still pressed, or if 0 then none pressed
	// so don't overload the HID system with useless events.
    }
    else //Modifiers may or may not be pressed right now
    {
	//kprintf("mod is %x\n", modifier);
        clock_get_uptime(&now);

        //left-hand CONTROL modifier key
        if ((modifier & kUSB_LEFT_CONTROL_BIT) && !(oldmodifier & kUSB_LEFT_CONTROL_BIT))
        {
            dispatchKeyboardEvent(usb_2_adb_keymap[0xe0], true, now);  //ADB left-hand CONTROL
	    _control_key = true;	//determine if we reboot CPU.  Is instance variable.
        }
	else if ((oldmodifier & kUSB_LEFT_CONTROL_BIT) && !(modifier & kUSB_LEFT_CONTROL_BIT)
	    && !(modifier & kUSB_RIGHT_CONTROL_BIT))
	{
	    //Now check for released modifier keys.  Both right and left modifiers must be
	    //   checked otherwise Window Server thinks none are held down
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe0], false, now); 
	    _control_key = false;	
	}

        //right-hand CONTROL modifier
        if ((modifier & kUSB_RIGHT_CONTROL_BIT) && !(oldmodifier & kUSB_RIGHT_CONTROL_BIT))
        {
            dispatchKeyboardEvent(usb_2_adb_keymap[0xe4], true, now);  //right-hand CONTROL
	    _control_key = true;	//determine if we reboot CPU.  Is instance variable.
        }
	else if ((oldmodifier & kUSB_RIGHT_CONTROL_BIT) && !(modifier & kUSB_RIGHT_CONTROL_BIT)
	    && !(modifier & kUSB_LEFT_CONTROL_BIT))
	{
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe4], false, now); 
	    _control_key = false;	
	}

        //left-hand SHIFT
        if ((modifier & kUSB_LEFT_SHIFT_BIT) && !(oldmodifier & kUSB_LEFT_SHIFT_BIT))
        {
            dispatchKeyboardEvent(usb_2_adb_keymap[0xe1], true, now);
        }
	else if ((oldmodifier & kUSB_LEFT_SHIFT_BIT) && !(modifier & kUSB_LEFT_SHIFT_BIT)
	    && !(modifier & kUSB_RIGHT_SHIFT_BIT))
	{
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe1], false, now); 
	}

        //right-hand SHIFT
        if ((modifier & kUSB_RIGHT_SHIFT_BIT) && !(oldmodifier & kUSB_RIGHT_SHIFT_BIT))
        {
            dispatchKeyboardEvent(usb_2_adb_keymap[0xe5], true, now);
        }
	else if ((oldmodifier & kUSB_RIGHT_SHIFT_BIT) && !(modifier & kUSB_RIGHT_SHIFT_BIT)
	    && !(modifier & kUSB_LEFT_SHIFT_BIT))
	{
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe5], false, now); 
	}

        if ((modifier & kUSB_LEFT_ALT_BIT) && !(oldmodifier & kUSB_LEFT_ALT_BIT))
        {
            dispatchKeyboardEvent(usb_2_adb_keymap[0xe2], true, now);
        }
	else if ((oldmodifier & kUSB_LEFT_ALT_BIT) && !(modifier & kUSB_LEFT_ALT_BIT)
	    && !(modifier & kUSB_RIGHT_ALT_BIT))
	{
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe2], false, now); 
	}

        if ((modifier & kUSB_RIGHT_ALT_BIT) && !(oldmodifier & kUSB_RIGHT_ALT_BIT))
        {
            dispatchKeyboardEvent(usb_2_adb_keymap[0xe6], true, now);
        }
	else if ((oldmodifier & kUSB_RIGHT_ALT_BIT) && !(modifier & kUSB_RIGHT_ALT_BIT)
	    && !(modifier & kUSB_LEFT_ALT_BIT))
	{
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe6], false, now); 
	}

        if ((modifier & kUSB_LEFT_FLOWER_BIT) && !(oldmodifier & kUSB_LEFT_FLOWER_BIT))
        {
            dispatchKeyboardEvent(usb_2_adb_keymap[0xe3], true, now);
	    _flower_key = true;	//determine if we go into kernel debugger, or reboot CPU
        }
	else if ((oldmodifier & kUSB_LEFT_FLOWER_BIT) && !(modifier & kUSB_LEFT_FLOWER_BIT)
	    && !(modifier & kUSB_RIGHT_FLOWER_BIT))
	{
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe3], false, now); 
	    _flower_key = false;
	}

        if ((modifier & kUSB_RIGHT_FLOWER_BIT) && !(oldmodifier & kUSB_RIGHT_FLOWER_BIT))
        {
            //dispatchKeyboardEvent(0x7e, true, now);
	    //WARNING... NeXT only recognizes left-hand flower key, so
	    //  emulate that for now
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe7], true, now);
	    _flower_key = true;	
        }
	else if ((oldmodifier & kUSB_RIGHT_FLOWER_BIT) && !(modifier & kUSB_RIGHT_FLOWER_BIT)
	    && !(modifier & kUSB_LEFT_FLOWER_BIT))
	{
	    dispatchKeyboardEvent(usb_2_adb_keymap[0xe7], false, now); 
	    _flower_key = false;
	}

    }

    //SECTION 2. Handle regular alphanumeric keys now.  Look first at previous keystrokes.
    //  Alphanumeric portion of HID report starts at byte +2.

    for (seq_key = 2; seq_key < prev_bytes_read; seq_key++)
    {
        alpha = old_array[seq_key];
	if (alpha == 0) //No keys pressed
	{
	    continue;
	}
	found = false;
	for (i = 2; i < bytes_read; i++)  //Look through current keypresses
	{
            if (alpha == *(key_ptr + i))
	    {
		found = true;	//This key has been held down for a while, so do nothing.
		break;		//   Autorepeat is taken care of by IOKit.
	    }
	}
	if (!found)
	{
	    clock_get_uptime(&now);

	    dispatchKeyboardEvent(usb_2_adb_keymap[alpha], false, now);  //KEY UP
	}
    }

    //Now take care of KEY DOWN.  
    for (seq_key = 2; seq_key < bytes_read; seq_key++)
    {
        alpha = *(key_ptr + seq_key);
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
	for (i = 2; i < prev_bytes_read; i++)
	{
            if (alpha == old_array[i])
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
	    dispatchKeyboardEvent(usb_2_adb_keymap[alpha], true, now);
	}
    }

    //Save the history for next time
    oldmodifier = modifier;
    prev_bytes_read = bytes_read;
    for (i=0; i< bytes_read; i++)  //guaranteed not to exceed 8
    {
	old_array[i] = *(key_ptr + i);
    }

}



// ***************************************************************************
// usb_asyncLED
//
// Called asynchronously to turn on/off the keyboard LED
//
// **************************************************************************
void 
AppleUSBKeyboard::AsyncLED(OSObject *target)
{
    AppleUSBKeyboard *me = OSDynamicCast(AppleUSBKeyboard, target);

    // Don't do anything if we are already terminating
    //
    USBLog(5, "+%s[%p]::usb_asyncLED, isInactive() = (%d)", me->getName(), me, me->isInactive());
    if ( !me->isInactive() )
    {
        me->Set_LED_States( me->_ledState ); 
    }
    USBLog(5, "-%s[%p]::usb_asyncLED, isInactive() = (%d)", me->getName(), me, me->isInactive());
    me->DecrementOutstandingIO();
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
AppleUSBKeyboard::setAlphaLockFeedback ( bool LED_state)
{
    UInt8	newState = _ledState;

    if (LED_state) //set alpha lock
	newState |= kUSB_CAPSLOCKLED_SET;   //2nd bit is caps lock on USB
    else
	newState &= ~kUSB_CAPSLOCKLED_SET;

    if (newState != _ledState)
    {
        _ledState = newState;
        IncrementOutstandingIO();	// since we are sending a method to a new thread
        thread_call_enter(_asyncLEDThread);
    }
}



void
AppleUSBKeyboard::setNumLockFeedback ( bool LED_state)
{
    UInt8	newState = _ledState;

    if (LED_state) 
	newState |= kUSB_NUMLOCKLED_SET;   //1st bit is num lock on USB
    else
	newState &= ~kUSB_NUMLOCKLED_SET;

    if (newState != _ledState)
    {
        _ledState = newState;
        IncrementOutstandingIO();	// since we are sending a method to a new thread
        thread_call_enter(_asyncLEDThread);
    }
}


//Called from parent classes
unsigned
AppleUSBKeyboard::getLEDStatus (void )
{    
    Get_LED_States(); 
    return _hid_report[0];
    //This also works, but it's not the point:   return _ledState;
}


// *****************************************************************************
// maxKeyCodes
// A.W. copied 3/25/99 from ADB keyboard driver, I don't know what this does
// ***************************************************************************
UInt32 AppleUSBKeyboard::maxKeyCodes (void )
{
    return 0x80;
}


// *************************************************************************
// deviceType
//
// **************************************************************************
UInt32 AppleUSBKeyboard::deviceType ( void )
{
    UInt32	id;	
    OSNumber 	*xml_handlerID;

    //Info.plist key is <integer>, not <string>
    xml_handlerID = OSDynamicCast( OSNumber, getProperty("alt_handler_id"));
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
UInt32 AppleUSBKeyboard::interfaceID ( void )
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
AppleUSBKeyboard::handlerID ( void )
{
    UInt32 ret_id = 2;  //Default for all unknown USB keyboards is 2

    //Return value must match "handler_id" line in .keyboard file
    // For some reason the ADB and PS/2 keyboard drivers are missing this method
    // Also, Beaker Keyboard Prefs doesn't run properly
    //Fix hardware bug in iMac USB keyboard mapping for ISO keyboards
    //kprintf("\nUSB product = %x, vendor = %x\n", _deviceDescriptor->product, _deviceDescriptor->vendor);
    // this should also be done in personalities
    if (_device->GetVendorID() == kIOUSBVendorIDAppleComputer)
    {
        ret_id = _device->GetProductID();
        if ((ret_id == kprodUSBAndyISOKbd) || (ret_id == kprodUSBCosmoISOKbd))
        {
            usb_2_adb_keymap[0x35] = 0x0a;  //Cosmo key18 swaps with key74, 0a is ADB keycode
            usb_2_adb_keymap[0x64] = 0x32;
            USBLog(5,"USB ISO keys swapped");
        }
    }

    if (_device->GetVendorID() == 0x045e)  //Microsoft ID
    {
        if (_device->GetProductID() == 0x000b)   //Natural USB+PS/2 keyboard
            ret_id = 2;  //18 was OSX Server, now 2 is OSX Extended ADB keyboard, unknown manufacturer
    }

    //New feature for hardware identification using Gestalt.h values
    if (_device->GetVendorID() == kIOUSBVendorIDAppleComputer)
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
            case kprodQ6ANSIKbd:  //Q6 ANSI
                ret_id = kgestQ6ANSIKbd;
                break;
            case kprodQ6ISOKbd:  //Q6 ISO
                ret_id = kgestQ6ISOKbd;
                break;
            case kprodQ6JISKbd:  //Q6 JIS
                ret_id = kgestQ6JISKbd;
                break;
            case kprodUSBProF16ANSIKbd:
                ret_id = kgestUSBProF16ANSIKbd;
                break;
            case kprodUSBProF16ISOKbd:
                ret_id = kgestUSBProF16ISOKbd;
                break;
            case kprodUSBProF16JISKbd:
                ret_id = kgestUSBProF16JISKbd;
                break;
            default:  // No Gestalt.h values, but still is Apple keyboard,
                      //   so return a generic Cosmo ANSI
                ret_id = kgestUSBCosmoANSIKbd;
                break;
        }
            
    return ret_id;  //non-Apple USB keyboards should all return "2"
}


//Get key values from ev_keymap.h
bool AppleUSBKeyboard:: doesKeyLock ( unsigned key)
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
const unsigned char * AppleUSBKeyboard::defaultKeymapOfLength (UInt32 * length )
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
        0x00,0xfe,0x32,0x00,0xfe,0x35,0x00,0xfe,0x33,0xff,0x00,0xfe,0x29,0xff,0x00,0xfe,0x2b,0xff,
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

IOReturn 
AppleUSBKeyboard::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn	err = kIOReturnSuccess;
    
    switch ( type )
    {
      case kIOUSBMessagePortHasBeenReset:
	    USBLog(3, "%s[%p]: received kIOUSBMessagePortHasBeenReset", getName(), this);
            _retryCount = kKeyboardRetryCount;
            _deviceIsDead = FALSE;
            _deviceHasBeenDisconnected = FALSE;
            
            IncrementOutstandingIO();
            if ((err = _interruptPipe->Read(_buffer, &_completion)))
            {
                DecrementOutstandingIO();
                USBLog(3, "%s[%p]::message - err (%x) in interrupt read", getName(), this, err);
                // _interface->close(this); didTerminate will do this for us
            }
            break;
  
        default:
            err = super::message (type, provider, argument);
            break;
    }
    
    return err;
}

bool 
AppleUSBKeyboard::finalize(IOOptionBits options)
{
    return(super::finalize(options));
}


//=============================================================================================
//
//  InterruptReadHandlerEntry is called to process any data coming in through our interrupt pipe
//
//=============================================================================================
//
void 
AppleUSBKeyboard::InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining)
{
    AppleUSBKeyboard *	me = OSDynamicCast(AppleUSBKeyboard, target);

    if (!me)
        return;
    
    me->InterruptReadHandler(status, bufferSizeRemaining);
    me->DecrementOutstandingIO();
}



void
AppleUSBKeyboard::InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining)
{
    bool		queueAnother = true;
    bool		timeToGoAway = false;
    IOReturn		err = kIOReturnSuccess;

    switch (status)
    {
        case kIOReturnOverrun:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnOverrun error", getName(), this);
            // This is an interesting error, as we have the data that we wanted and more...  We will use this
            // data but first we need to clear the stall and reset the data toggle on the device.  We will not
            // requeue another read because our _clearFeatureEndpointHaltThread will requeue it.  We then just 
            // fall through to the kIOReturnSuccess case.
            // 01-18-02 JRH If we are inactive, then don't do anything
	    if (!isInactive())
	    {
		//
		// First, clear the halted bit in the controller
		//
		_interruptPipe->ClearStall();
		
		// And call the device to reset the endpoint as well
		//
		IncrementOutstandingIO();
		thread_call_enter(_clearFeatureEndpointHaltThread);
	    }
            queueAnother = false;
            timeToGoAway = false;
            
            // Fall through to process the data.
            
        case kIOReturnSuccess:
            // Reset the retry count, since we had a successful read
            //
            _retryCount = kKeyboardRetryCount;

            // Handle the data
            //
            Simulate_ADB_Event((UInt8 *)_buffer->getBytesNoCopy(), (UInt32)_maxPacketSize - bufferSizeRemaining);
	    if (isInactive())
		queueAnother = false;
            break;

        case kIOReturnNotResponding:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnNotResponding error", getName(), this);
            // If our device has been disconnected or we're already processing a
            // terminate message, just go ahead and close the device (i.e. don't
            // queue another read.  Otherwise, go check to see if the device is
            // around or not. 
            //
            if ( _deviceHasBeenDisconnected || isInactive() )
            {
                  queueAnother = false;
                  timeToGoAway = true;
            }
            else
            {
                USBLog(3, "%s[%p]::InterruptReadHandler Checking to see if keyboard is still connected", getName(), this);
                IncrementOutstandingIO();
                thread_call_enter(_deviceDeadCheckThread);
                
                // Before requeueing, we need to clear the stall
                //
                _interruptPipe->ClearStall();
            }
                
            break;
            
	case kIOReturnAborted:
	    // This generally means that we are done, because we were unplugged, but not always
            //
            if (isInactive() || _deviceIsDead )
	    {
                USBLog(3,"%s[%p]::InterruptReadHandler Read aborted. We are terminating", getName(), this);
		queueAnother = false;
                timeToGoAway = true;
	    }
	    else
            {
                USBLog(3,"%s[%p]::InterruptReadHandler Read aborted. Don't know why. Trying again", getName(), this);
            }
	    break;
            
        case kIOReturnUnderrun:
        case kIOUSBPipeStalled:
        case kIOUSBLinkErr:
        case kIOUSBNotSent2Err:
        case kIOUSBNotSent1Err:
        case kIOUSBBufferUnderrunErr:
        case kIOUSBBufferOverrunErr:
        case kIOUSBWrongPIDErr:
        case kIOUSBPIDCheckErr:
        case kIOUSBDataToggleErr:
        case kIOUSBBitstufErr:
        case kIOUSBCRCErr:
            // These errors will halt the endpoint, so before we requeue the interrupt read, we have
            // to clear the stall at the controller and at the device.  We will not requeue the read
            // until after we clear the ENDPOINT_HALT feature.  We need to do a callout thread because
            // we are executing inside the gate here and we cannot issue a synchronous request.
            USBLog(3, "%s[%p]::InterruptReadHandler OHCI error (0x%x) reading interrupt pipe", getName(), this, status);
	    // 01-18-02 JRH If we are inactive, then ignore the error
	    if (!isInactive())
	    {
		// First, clear the halted bit in the controller
		//
		_interruptPipe->ClearStall();
		
		// And call the device to reset the endpoint as well
		//
		IncrementOutstandingIO();
		thread_call_enter(_clearFeatureEndpointHaltThread);
            }
            // We don't want to requeue the read here, AND we don't want to indicate that we are done
            //
            queueAnother = false;
            break;
            
        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            USBLog(3, "%s[%p]::InterruptReadHandler error (0x%x) reading interrupt pipe", getName(), this, status);
	    if (isInactive())
		queueAnother = false;
            break;
    }

    if ( queueAnother )
    {
        // Queue up another one before we leave.
        //
        IncrementOutstandingIO();
        err = _interruptPipe->Read(_buffer, &_completion);
        if ( err != kIOReturnSuccess)
        {
            // This is bad.  We probably shouldn't continue on from here.
            USBError(1, "%s[%p]::InterruptReadHandler -  immediate error 0x%x queueing read\n", getName(), this, err);
            DecrementOutstandingIO();
            timeToGoAway = true;
        }
    }

    if ( timeToGoAway )
    {
        // It's time to go awaw.  This fix needed by Classic for non-stop repeated keys after unplugging
        //
	if (_codeToRepeat != (unsigned) -1)
	{
	    AbsoluteTime	now;

	    clock_get_uptime(&now);
	    dispatchKeyboardEvent(_codeToRepeat, false, now); 
	}
    }
}

//=============================================================================================
//
//  CheckForDeadDevice is called when we get a kIODeviceNotResponding error in our interrupt pipe.
//  This can mean that (1) the device was unplugged, or (2) we lost contact
//  with our hub.  In case (1), we just need to close the driver and go.  In
//  case (2), we need to ask if we are still attached.  If we are, then we update 
//  our retry count.  Once our retry count (3 from the 9 sources) are exhausted, then we
//  issue a DeviceReset to our provider, with the understanding that we will go
//  away (as an interface).
//
//=============================================================================================
//
void 
AppleUSBKeyboard::CheckForDeadDeviceEntry(OSObject *target)
{
    AppleUSBKeyboard *me = OSDynamicCast(AppleUSBKeyboard, target);
    
    if (!me)
        return;
        
    me->CheckForDeadDevice();
    me->DecrementOutstandingIO();
}

void 
AppleUSBKeyboard::CheckForDeadDevice()
{
    IOReturn			err = kIOReturnSuccess;
    IOUSBDevice *		device;

    // Are we still connected?  Don't check if we are already processing a request
    //
    if ( _interface && !_deviceDeadThreadActive )
    {
        _deviceDeadThreadActive = true;

        device = _interface->GetDevice();
        if ( device )
        {
            err = device->message(kIOUSBMessageHubIsDeviceConnected, NULL, 0);
        
            if ( kIOReturnSuccess == err )
            {
                // Looks like the device is still plugged in.  Have we reached our retry count limit?
                //
                if ( --_retryCount == 0 )
                {
                    _deviceIsDead = true;
                    USBLog(3, "%s[%p]: Detected an kIONotResponding error but still connected.  Resetting port", getName(), this);
                    
                    if (_interruptPipe)
                        _interruptPipe->Abort();  // This will end up closing the interface as well.

                    // OK, let 'er rip.  Let's do the reset thing
                    //
                    device->ResetDevice();
                        
                }
            }
            else
            {
                // Device is not connected -- our device has gone away.  The message kIOServiceIsTerminated
                // will take care of shutting everything down.  
                //
                _deviceHasBeenDisconnected = true;
                USBLog(5, "%s[%p]: CheckForDeadDevice: keyboard has been unplugged", getName(), this);
            }
        }
        _deviceDeadThreadActive = false;
    }
}

//=============================================================================================
//
//  ClearFeatureEndpointHaltEntry is called when we get an OHCI error from our interrupt read
//  (except for kIOReturnNotResponding  which will check for a dead device).  In these cases
//  we need to clear the halted bit in the controller AND we need to reset the data toggle on the
//  device.
//
//=============================================================================================
//
void 
AppleUSBKeyboard::ClearFeatureEndpointHaltEntry(OSObject *target)
{
    AppleUSBKeyboard *	me = OSDynamicCast(AppleUSBKeyboard, target);
    
    if (!me)
        return;
        
    me->ClearFeatureEndpointHalt();
    me->DecrementOutstandingIO();
}

void 
AppleUSBKeyboard::ClearFeatureEndpointHalt( )
{
    IOReturn			status;

    // Clear out the structure for the request
    //
    bzero( &_request, sizeof(IOUSBDevRequest));

    // Build the USB command to clear the ENDPOINT_HALT feature for our interrupt endpoint
    //
    _request.bmRequestType 	= USBmakebmRequestType(kUSBNone, kUSBStandard, kUSBEndpoint);
    _request.bRequest 		= kUSBRqClearFeature;
    _request.wValue		= kUSBFeatureEndpointStall;
    _request.wIndex		= _interruptPipe->GetEndpointNumber() | 0x80 ; // bit 7 sets the direction of the endpoint to IN
    _request.wLength		= 0;
    _request.pData 		= NULL;

    // Send the command over the control endpoint
    //
    status = _device->DeviceRequest(&_request, 5000, 0);

    if ( status )
    {
        USBLog(3, "%s[%p]::ClearFeatureEndpointHalt -  DeviceRequest returned: 0x%x", getName(), this, status);
    }
    
    // Now that we've sent the ENDPOINT_HALT clear feature, we need to requeue the interrupt read.  Note
    // that we are doing this even if we get an error from the DeviceRequest.
    //
    IncrementOutstandingIO();
    status = _interruptPipe->Read(_buffer, &_completion);
    if ( status != kIOReturnSuccess)
    {
        // This is bad.  We probably shouldn't continue on from here.
        USBLog(3, "%s[%p]::ClearFeatureEndpointHalt -  immediate error %d queueing read", getName(), this, status);
        DecrementOutstandingIO();
        // _interface->close(this); didTerminate will do this
    }
}


bool
AppleUSBKeyboard::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(5, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
    if (_interruptPipe)
    {
	_interruptPipe->Abort();
    }
    return super::willTerminate(provider, options);
}


bool
AppleUSBKeyboard::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
    USBLog(5, "%s[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), _outstandingIO);
    if (!_outstandingIO)
	_interface->close(this);
    else
	_needToClose = true;
    return super::didTerminate(provider, options, defer);
}


#if 0
// this was only used for experimental purposes
bool 	
AppleUSBKeyboard::requestTerminate( IOService * provider, IOOptionBits options )
{
    USBLog(3, "%s[%p]::requestTerminate isInactive = %d", getName(), this, isInactive());
    return super::requestTerminate(provider, options);
}


bool
AppleUSBKeyboard::terminate( IOOptionBits options = 0 )
{
    USBLog(3, "%s[%p]::terminate isInactive = %d", getName(), this, isInactive());
    return super::terminate(options);
}


void
AppleUSBKeyboard::free( void )
{
    USBLog(3, "%s[%p]::free isInactive = %d", getName(), this, isInactive());
    super::free();
}


bool
AppleUSBKeyboard::terminateClient( IOService * client, IOOptionBits options )
{
    USBLog(3, "%s[%p]::terminateClient isInactive = %d", getName(), this, isInactive());
    return super::terminateClient(client, options);
}
#endif

void
AppleUSBKeyboard::DecrementOutstandingIO(void)
{
    if (!_gate)
    {
	if (!--_outstandingIO && _needToClose)
	{
	    USBLog(3, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), _outstandingIO);
	    _interface->close(this);
	}
	return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)-1);
}


void
AppleUSBKeyboard::IncrementOutstandingIO(void)
{
    if (!_gate)
    {
	_outstandingIO++;
	return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)1);
}


IOReturn
AppleUSBKeyboard::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    AppleUSBKeyboard *me = OSDynamicCast(AppleUSBKeyboard, target);
    UInt32	direction = (UInt32)param1;
    
    if (!me)
    {
	USBLog(1, "AppleUSBKeyboard::ChangeOutstandingIO - invalid target");
	return kIOReturnSuccess;
    }
    switch (direction)
    {
	case 1:
	    me->_outstandingIO++;
	    break;
	    
	case -1:
	    if (!--me->_outstandingIO && me->_needToClose)
	    {
		USBLog(3, "%s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), me->_outstandingIO);
		me->_interface->close(me);
	    }
	    break;
	    
	default:
	    USBLog(1, "%s[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
    }
    return kIOReturnSuccess;
}

