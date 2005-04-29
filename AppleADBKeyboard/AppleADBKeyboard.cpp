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
/*
 * 18 June 1998
 * Start IOKit version.
 */

#include "AppleADBKeyboard.h"
#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#define super IOHIKeyboard
#ifndef kIOHIDFKeyModeKey
#define kIOHIDFKeyModeKey    "HIDFKeyMode"
#endif

enum {
    kCapsLockState_CapsLockEngaged      = 0x01,
    kCapsLockState_CapsLockGoingDown    = 0x02,
    kCapsLockState_PowerEngaged         = 0x04,
    kCapsLockState_PowerCapsMapped      = 0x08
};

OSDefineMetaClassAndStructors(AppleADBKeyboard,IOHIKeyboard)


#define RB_HALT		0x08	/* don't reboot, just halt */

extern "C" {
    void Debugger( const char * );
    void boot(int paniced, int howto, char * command);
}

static void AppleADBKeyboardReboot( thread_call_param_t arg, thread_call_param_t );
static void new_kbd_data ( IOService * us, UInt8 adbCommand, IOByteCount length, UInt8 * data );
static void asyncSetLEDFunc ( thread_call_param_t, thread_call_param_t );

#if 0  //The following table is in Info.plist now
//Convert raw ADB codes to MacOS 9 KMAP virtual key codes in dispatchKeyboardEvent()
static unsigned char	kmapConvert[] = 
	{
	//00,00,00,00,  These 4 are in System resource, but are unused
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
	0x30,0x31,0x32,0x33,0x34,0x35,0x3B,0x37,0x38,0x39,0x3A,0x7B,0x7C,0x7D,0x7E,0x3F,
	0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
	0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x3C,0x3D,0x3E,0x36,0x7F,
	00,00
	}; 	
#endif

void AppleADBKeyboard::stop( IOService * provider )
{
    if (adbDevice)
    {
	IORegistryEntry * us;
	
	us = this;
	adbDevice->releaseFromClient(us); //us is just a placeholder, not used

	adbDevice = 0;
    }

    super::stop(provider);
}

void AppleADBKeyboard::free( void )
{
    if (_keybrdLock)
    {
	IOLockFree(_keybrdLock);
	_keybrdLock = NULL;
    }

	if ( _packetLock )
	{
		IOLockFree( _packetLock );
		_packetLock = NULL;
	}

    if (adbDevice)
    {
	IORegistryEntry * us;
	
	us = this;
	adbDevice->releaseFromClient(us); //us is just a placeholder, not used

	adbDevice = 0;
    }
    
    if ( ledThreadCall )
    {
        thread_call_free ( ledThreadCall );
        ledThreadCall = 0;
    }

    if ( rebootThreadCall )
    {
        thread_call_free ( rebootThreadCall );
        rebootThreadCall = 0;
    }

    super::free();
}

// **********************************************************************************
// start
//
// **********************************************************************************
bool AppleADBKeyboard::start ( IOService * theNub )
{
    OSString *		data;
    const char *	pTable;
    OSNumber 		*enable_fwd_delete;
    
    _keybrdLock = IOLockAlloc(); 
	_packetLock = IOLockAlloc();

    _fn_key_invoked_power = false;   //Used by iBook and PowerBooks
    enable_fwd_delete = OSDynamicCast( OSNumber, getProperty("PowerBook fn Foward Delete"));
    if (enable_fwd_delete)
    {
	_enable_fwd_delete = (bool) enable_fwd_delete->unsigned32BitValue();
    }
   
	if ( _packetLock )
		IOLockLock( _packetLock );
        
    adbDevice = (IOADBDevice *)theNub;
    if( !adbDevice->seizeForClient(this, new_kbd_data) ) {
	    IOLog("%s: Seize failed\n", getName());
        if ( _packetLock )
            IOLockUnlock( _packetLock );
	    return false;
    }
    
    _oneshotCAPSLOCK = ((getLEDStatus() & ADBKS_LED_CAPSLOCK) != 0);
    
	if ( _packetLock )
		IOLockUnlock( _packetLock );
    
    //Get virtual key map table
    data = OSDynamicCast( OSString, getProperty( "ADBVirtualKeys" ));
    if (data)
    {
	pTable = data->getCStringNoCopy();
	if (data->getLength() < (5 * 0x7f)) //7f codes, 5 bytes per code in XML string
	{
	    IOLog("AppleADBKeyboard: too few virtual keys found in Info.plist");
	    for (int i=0; i<128; i++)
	    {
		_virtualmap[i] = i;	//Somewhat usable keymap
	    }
	} 
	else //pTable could get access violation if not all bytes are accounted for
	{
	    for (int i=0; i< 128; i++) //No codes greater than 0x7f allowed in packet()
	    {
		_virtualmap[i] = strtol(pTable, NULL, 16); //Must be base 16
		pTable += 5;  //Format is "0x00,0x01,0x02"
	    }
	}	
    }
    else
    {
	return false; 
    }
    
    _sticky_fn_ON = _stickymodeON = false;
    
    data = OSDynamicCast( OSString, getProperty( "fnVirtualKeys" ));  // 
    if (data)
    {
	pTable = data->getCStringNoCopy();
	if (data->getLength() < (5 * 0x7f)) //7f codes, 5 bytes per code in XML string
	{
	    IOLog("AppleADBKeyboard: too few sticky fn keys found in Info.plist");
	    for (int i=0; i<128; i++)
	    {
		_fnvirtualmap[i] = i;	//Somewhat usable keymap
	    }
	} 
	else 
	{
	    for (int i=0; i< 128; i++) 
	    {
		_fnvirtualmap[i] = strtol(pTable, NULL, 16); //Must be base 16
		pTable += 5;  //Format is "0x00,0x01,0x02"
	    }
	}	
    }
    else
    {
	IOLog("AppleADBKeyboard: no fn keymap found in Info.plist"); 
	//No error return if sticky fn data does not exist, it's not as critical since
	//   numlock on PB would accomplish the same thing.  However, _fnvirtualmap
	//   must be set to something valid
	memcpy(_fnvirtualmap, _virtualmap, 128);
    } 
    
    _get_last_keydown = OSSymbol::withCString("get_last_keydown");
    _get_handler_id = OSSymbol::withCString("get_handler_id");
    _get_device_flags = OSSymbol::withCString("get_device_flags");
    
    turnLEDon = ADBKS_LED_CAPSLOCK | ADBKS_LED_NUMLOCK | ADBKS_LED_SCROLLLOCK; //negative logic
    setAlphaLockFeedback(false);
    setNumLockFeedback(false);
    
    clock_interval_to_absolutetime_interval( 4, kSecondScale, &rebootTime);
    clock_interval_to_absolutetime_interval( 1, kSecondScale, &debuggerTime);
    
    _hasDualModeFunctionKeys = false;
    updateFKeyMap();  //UPDATE PMU DATA IF APPROPRIATE 
    
    getFKeyMode();  // this will update _hasDualModeFunctionKeys
    if (_hasDualModeFunctionKeys)
    {
        setFKeyMode( 0 );  //set PowerBook Function keys to default (0) mode.
    }
    
    // RY: allocate thread calls for led and reboot
    ledThreadCall = thread_call_allocate ( asyncSetLEDFunc, (thread_call_param_t) this);
    rebootThreadCall = thread_call_allocate ( AppleADBKeyboardReboot, (thread_call_param_t) RB_HALT);

    
    return super::start(theNub);
}

bool AppleADBKeyboard::open(IOService *			client,
                        IOOptionBits		  	options,
                        KeyboardEventAction		keAction,
                        KeyboardSpecialEventAction	kseAction,
                        UpdateEventFlagsAction		uefAction)
{
    if (!super::open(client, options, keAction, kseAction, uefAction))
        return false;

    // RY: Check to see if LED was set prior to start.  If so dispatch
    // event so that software state matches hardware state.
    if (_oneshotCAPSLOCK)
    {
        if ( _packetLock )
            IOLockLock( _packetLock );
            
        _oneshotCAPSLOCK = false;  // only set to true by start()
        
        //check handler ID since keyboards of type 1 fail register 2 check

        if (adbDevice && adbDevice->handlerID() > 0xc0) //Anything beyond 0xc0 is WallStreet or higher, and USB and BT
        {
            AbsoluteTime	locktime;
            clock_get_uptime(&locktime);
        
            // setting caps lock
            super::dispatchKeyboardEvent(ADBK_CAPSLOCK, true, locktime);
            super::dispatchKeyboardEvent(ADBK_CAPSLOCK, false, locktime);

            _capsLockState |= ( kCapsLockState_CapsLockEngaged | kCapsLockState_PowerCapsMapped );
            _capsLockState &= ~kCapsLockState_CapsLockGoingDown;
        }
        
        if ( _packetLock )
            IOLockUnlock( _packetLock );
    }
    
    return true;
}

// --------------------------------------------------------------------------
//
// Method: setFKeyMode
//
// Purpose:  set PowerBook function keys mode.  An argument of zero means
// default mode, while an argument of 1 means F keys primary.  The default
// mode will trigger functions as they appear on the keyboard.  Older
// portables like WallStreet will not be affected since F keys don't
// have dual purposes.  And of course external ADB keyboards are not affected.
void
AppleADBKeyboard::setFKeyMode(UInt8 mode)
{
    UInt16              value;
    IOByteCount         length = sizeof( UInt16);

    if (_hasDualModeFunctionKeys && adbDevice)
    {
        value = mode;
        adbDevice->writeRegister(1, (UInt8 *)&value, &length);
        setProperty(kIOHIDFKeyModeKey, mode, sizeof(UInt32));
    }
}



// --------------------------------------------------------------------------
//
// Method: getFKeyMode
//
// Purpose:  get PowerBook function keys mode.  Also set up instance variable
// to keep track of whether this device has dual mode Function Keys or not.
SInt8
AppleADBKeyboard::getFKeyMode(void)
{
    UInt8       	adbdata[8], isFnPrimary = 0;
    IOByteCount		length = 8;
    OSDictionary 	* FKeyModeParamDict;
    SInt32		dictData = -1;
    OSNumber 		* datan;

    if (!adbDevice)
	return -1;	// -1 means feature unsupported
    
    FKeyModeParamDict = OSDictionary::withCapacity(4);
    bzero(adbdata, 8);
    adbDevice->readRegister(1, adbdata, &length);
    // [1] contains F key mode and [0] contains world region (ANSI, JIS, ISO)
    _hasDualModeFunctionKeys = false;
    if (adbdata[0])	//is 0 for external keyboards.  
    {
	_hasDualModeFunctionKeys = true;
	if (adbdata[1] & 0x0001)
	    isFnPrimary = 1;
	setProperty(kIOHIDFKeyModeKey, isFnPrimary, sizeof(UInt32));
	dictData = isFnPrimary;
    }
    datan = OSNumber::withNumber((unsigned long long) dictData, 32 ); // 0, 1, or -1
    if( datan) {
	FKeyModeParamDict->setObject( kIOHIDFKeyModeKey, datan);
	datan->release();
    }
    if (_keyboardEventTarget)
    {
	((IOHIDSystem *)_keyboardEventTarget)->setParamProperties(FKeyModeParamDict); 
	//Merge new property with HIDSystem properties
    }
    FKeyModeParamDict->release();
    
    if (_hasDualModeFunctionKeys)
	return isFnPrimary;
    else
	return -1;	// -1 means feature unsupported
    
}


// --------------------------------------------------------------------------
//
// Method: updateFKeyMap
//
// Purpose:  Update the Fkey map in PMU with the values from the name
//   	     registry, if needed.  This came from the UpdateFKeyMap routine
//	     in OS9 Excelsior:OS:Wallyworld sources.       
/* static */ void
AppleADBKeyboard::updateFKeyMap()
{
    IORegistryEntry		*devicetreeRegEntry;
    typedef char  	keystrtype[4];
    unsigned long 	buttonkeyvalue; 	// button translation value from name registry
    unsigned char	fkeyindex;		// f key number being checked
    keystrtype mapentry[] = 
					{
					{"F0"},  {"F1"},  {"F2"},  {"F3"},  {"F4"},  {"F5"},  {"F6"},
							{"F7"},  {"F8"},  {"F9"},  {"F10"},  {"F11"},  {"F12"}, 
					};
    devicetreeRegEntry = fromPath("mac-io/via-pmu/adb/keyboard", gIODTPlane);
    if(devicetreeRegEntry != NULL)  {
	if (OSDynamicCast(OSData, devicetreeRegEntry->getProperty("AAPL,has-embedded-fn-keys"))) {
            for (fkeyindex = 1; fkeyindex <= 12;  fkeyindex++)
                {
		OSData *tmpData = OSDynamicCast(OSData, devicetreeRegEntry->getProperty(mapentry[fkeyindex]));
		if(tmpData != NULL) {
			memcpy(&buttonkeyvalue, (UInt8 *)tmpData->getBytesNoCopy(), sizeof(buttonkeyvalue));
			setButtonTransTableEntry(fkeyindex, buttonkeyvalue);
			}
		}
            }	    
	    devicetreeRegEntry->release();  //3172112
	}
}

// ******************************************************************************************************
// Method:  setButtonTransTableEntry
//
// Purpose:	Used for local write to PMU adb interface (to set up PMU registers).
//		This is essentially a writeToDevice routine (as in ADB device).
//		The command string typically looks like (on the ApplePMU driver
//		sendMiscCommand side) as an example: 
//		ADBcmd ADBcnt IntKbdCmd AutoPollbit ADBcount PMUcount selector  F12key keyvalue
//		  20      07      28        02         04      03       01        0c      8b
// ******************************************************************************************************
/* static */ void
AppleADBKeyboard::setButtonTransTableEntry(unsigned char fkeynum, unsigned char transvalue)
{
    if (!adbDevice)
	return;
						// CMD: int KBD 2 reg 0 listen filled in by IOPMUADBController
    UInt8 oBuffer[3];				// embed. data count =3 to PMU filled in by IOPMUADBController
    oBuffer[0] = 1;				// selector for PMU: set fkey command
    oBuffer[1] = fkeynum;			// fkey to set
    oBuffer[2] = transvalue;			// button code to attach
    IOByteCount oLength = sizeof(oBuffer);
    adbDevice->writeRegister(0, oBuffer, &oLength);   // adbRegister = 0, IOADBDevice adds addr of 0x02 => 0x28
}	

// **********************************************************************************
// interfaceID
//
// **********************************************************************************
UInt32 AppleADBKeyboard::interfaceID ( void )
{
return NX_EVS_DEVICE_INTERFACE_ADB;
}


// **********************************************************************************
// deviceType
//
// **********************************************************************************
UInt32 AppleADBKeyboard::deviceType ( void )
{
    UInt32	id;	//We need handler ID to remap adjustable JIS keyboard
    IORegistryEntry 	*regEntry;
    OSData 		*data = 0;
    UInt32 		*dataptr;
    OSNumber 		*xml_handlerID;

    if (!adbDevice)
	return 2;

    xml_handlerID = OSDynamicCast( OSNumber, getProperty("alt_handler_id"));
    if (xml_handlerID)
    {
	id = xml_handlerID->unsigned32BitValue();
    }
    else
    {
	id = adbDevice->handlerID();
    }

    if (id == 18)  //Adjustable JIS
    {
	_virtualmap[0x32] = 0x35; //tilde to ESC
	_fnvirtualmap[0x32] = 0x35;
    }

    if ((id == 2) || (id == 3))  //External ADB keyboards may retain ID = 3 between boots
    {
	adbDevice->setHandlerID(3);
	id = 2;  // Gestalt.h has no idea what type 3 is since historically it was never used
    }
    else
    if (id == 5)  //ISO extended keyboard
    {
	adbDevice->setHandlerID(3);
	id = 5;  
    }
    
    if ((id == kgestaltPwrBkEKDomKbd) || (id == kgestaltPwrBkEKISOKbd) || 
	(id == kgestaltPwrBkEKJISKbd) || (id == kgestaltPwrBk99JISKbd))
    {	
	if( (regEntry = IORegistryEntry::fromPath( "/pci@f2000000/mac-io/via-pmu/adb/keyboard", gIODTPlane ))) 
	{
	    data = OSDynamicCast(OSData, regEntry->getProperty( "keyboard-id", gIODTPlane, kIORegistryIterateRecursively ));
	    if (data)
	    {
		dataptr = (UInt32 *)data->getBytesNoCopy();
		id = *dataptr; //make sure no byte swapping
	    }
	    regEntry->release();
	}
    }    

    return id;
}


// **********************************************************************************
// setAlphaLockFeedback
// This is usually called on a call-out thread after the caps-lock key is pressed.
// ADB operations to PMU are synchronous, and this is must not be done
// on the call-out thread since that is the PMU driver workloop thread, and
// it will block itself.
//
// Therefore, we schedule the ADB write to disconnect the call-out thread
// and the one that initiates the ADB write.
//
// **********************************************************************************
void AppleADBKeyboard::setAlphaLockFeedback ( bool to )
{    
    if (to)
	turnLEDon &= ~ADBKS_LED_CAPSLOCK; //Inverse logic applies here
    else
	turnLEDon |= ADBKS_LED_CAPSLOCK;

    if ( !isInactive() && ledThreadCall) {
        thread_call_enter(ledThreadCall);
    }
}

void AppleADBKeyboard::setNumLockFeedback ( bool to )
{
     if (to) //LED on means clear that bit
	turnLEDon &= ~ ADBKS_LED_NUMLOCK; 
    else
	turnLEDon |= ADBKS_LED_NUMLOCK;

    if ( !isInactive() && ledThreadCall) {
        thread_call_enter(ledThreadCall);
    }

}



// **********************************************************************************
// asyncSetLEDFunc
//
// Called asynchronously to turn on/off the capslock and numlock LED
//
// **********************************************************************************
static void asyncSetLEDFunc ( thread_call_param_t self, thread_call_param_t )
{
    
UInt16		value;
IOByteCount	length = sizeof( UInt16);

    if (!((AppleADBKeyboard*)self)->adbDevice)
	return;

    value = ((AppleADBKeyboard*)self)->turnLEDon;
    ((AppleADBKeyboard*)self)->adbDevice->writeRegister(2, (UInt8 *)&value, &length);
}

/**********************************************************************
Get LED status by reading hardware.  Register 2 has 16 bits.
Note that early ADB keyboards don't support this much data in 
register 2, so it will come back all zeros and that is interpreted
as every LED being turned on.
**********************************************************************/
unsigned AppleADBKeyboard::getLEDStatus (void )
{  
    UInt8       data[8];  //8 bytes max for ADB read (talk) operation
    IOByteCount length = 8;

    if (!adbDevice)
	return 0;

    bzero(data, 8);
    LEDStatus = 0;
    adbDevice->readRegister(2, data, &length);

    if ((data[1] & ADBKS_LED_NUMLOCK) == 0)
	LEDStatus |= ADBKS_LED_NUMLOCK;
    if ((data[1] & ADBKS_LED_CAPSLOCK) == 0)
	LEDStatus |= ADBKS_LED_CAPSLOCK;
    if ((data[1] & ADBKS_LED_SCROLLLOCK) == 0)
	LEDStatus |= ADBKS_LED_SCROLLLOCK;

    return LEDStatus;
}

// **********************************************************************************
// new_kbd_data
//
// **********************************************************************************
static void new_kbd_data ( IOService * us, UInt8 adbCommand, IOByteCount length, UInt8 * data )
{
((AppleADBKeyboard *)us)->packet(data,length,adbCommand);
}

// **********************************************************************************
// dispatchKeyboardEvent
//
// **********************************************************************************
static void AppleADBKeyboardReboot( thread_call_param_t arg, thread_call_param_t )
{
    boot( 0, (int) arg, 0 );
}

void AppleADBKeyboard::dispatchKeyboardEvent(unsigned int	keyCode,
                            /* direction */ bool         	goingDown,
                            /* timeStamp */ AbsoluteTime	time)
{
    char * pvirtualmap;
 
 
    // ----------------------------------------------------------------
    // RY: To better support the BSD (hacker) community, the behavior
    // of the caps lock key needs to be altered.  On portables, the
    // keyboard behaves a bit differently than external ADB keyboards.
    // The keyboard still dispatches ADBK_CAPSLOCK in line with the 
    // behavior of a locking capslock, but we have also noticed
    // that power keyup events are issued that correspond to the
    // events not dispatched by the capslock key.  
    // 
    // Here is the sequence of events turning on capslock and the LED:
    //
    // Physicall Event              Dispatched Event
    // ---------------              ----------------
    // CapsLock down                capslock keydown event dispatched
    // CapsLock up                  power keyup event dispatched
    //
    // Here is the sequence of events turning off capslock and the LED:
    //
    // Physicall Event              Dispatched Event
    // ---------------              ----------------    
    // CapsLock down                power keyup event dispatched
    // CapsLock up                  capslock keyup event dispatched
    //
    // We can use this information to maintain the state of caps lock 
    // in SW and remove the designation of the CAPSLOCK as being locked
    // by AppleADBKeyboard::doesKeyLock.  This change in behavior will 
    // allow us to remap the capslock to control and appease the will
    // of the hacker community.
    // 
    // FYI:  The LED will still continue to be controlled by the PMU 
    // and change regardless of the state in SW.  While this may not
    // be desired, it has been the position of the BSD management and
    // WWDR that the developement community is not concerned.
    if ( adbDevice && (adbDevice->handlerID() > 0xc0) && (keyCode == ADBK_CAPSLOCK) )
    {

        // The LED state check in AppleADBKeyboard::setParamProperties
        // has still not taken place.  Consumer to key to avoid an
        // inconsistant capslock state.
        if (_oneshotCAPSLOCK)
        {
            return;
        }
            
        if (goingDown)
        {
            if ((_capsLockState & kCapsLockState_CapsLockEngaged) != 0)
            {
                return;
            }
            
            _capsLockState |= (kCapsLockState_CapsLockEngaged | kCapsLockState_CapsLockGoingDown);
        }
        else if (!goingDown)
        {   

            if ((_capsLockState & kCapsLockState_CapsLockEngaged) == 0)
            {
                return;
            }
            else if ((_capsLockState & kCapsLockState_PowerCapsMapped) == 0)
            {
                super::dispatchKeyboardEvent(ADBK_CAPSLOCK, false, time);
                super::dispatchKeyboardEvent(ADBK_CAPSLOCK, true, time);
            }
            else if ((_capsLockState & kCapsLockState_CapsLockGoingDown) == 0)
            {
                super::dispatchKeyboardEvent(ADBK_CAPSLOCK, true, time);                
            }
            
            _capsLockState &= ~(kCapsLockState_CapsLockEngaged | kCapsLockState_CapsLockGoingDown);        
        }        
        _capsLockState &= ~kCapsLockState_PowerCapsMapped;
    }
    // RY: Apply special handling for case of POWER keyup during CAPSLOCK key sequence.
    else if ( (_capsLockState & kCapsLockState_CapsLockEngaged) && 
                !(_capsLockState & kCapsLockState_PowerEngaged) && 
                (keyCode == ADBK_POWER) && (goingDown == false) )

    {
        if (((_capsLockState & kCapsLockState_CapsLockGoingDown) != 0) && 
            ((_capsLockState & kCapsLockState_PowerCapsMapped) == 0))
        {
            _capsLockState |= kCapsLockState_PowerCapsMapped;
            _capsLockState &= ~kCapsLockState_CapsLockGoingDown;
            
            keyCode         = ADBK_CAPSLOCK;
        }
        else if (((_capsLockState & kCapsLockState_CapsLockGoingDown) == 0) &&
                 ((_capsLockState & kCapsLockState_PowerCapsMapped) != 0))
        {
            _capsLockState |= kCapsLockState_CapsLockGoingDown;
            
            goingDown       = true;
            keyCode         = ADBK_CAPSLOCK;
        }
    }
    // ----------------------------------------------------------------

    pvirtualmap = _virtualmap;
    if ((_stickymodeON) && (_sticky_fn_ON))
    {
	pvirtualmap = _fnvirtualmap;
    }

    if( !goingDown && programmerKey) {
        programmerKey = false;
        EVK_KEYUP( ADBK_CONTROL, _keyState);
        SUB_ABSOLUTETIME( &time, &programmerKeyTime );
        if( CMP_ABSOLUTETIME( &time, &rebootTime) >= 0) {
            if ( rebootThreadCall ) {
                thread_call_enter( rebootThreadCall );
            }
            
        } else if( CMP_ABSOLUTETIME( &time, &debuggerTime) >= 0) {
            Debugger("Programmer Key");
	}

    } 
    else if ( keyCode == ADBK_POWER )
    {
        if ( goingDown )
            _capsLockState |= kCapsLockState_PowerEngaged;
        else
            _capsLockState &= ~kCapsLockState_PowerEngaged;

	if (EVK_IS_KEYDOWN( ADBK_CONTROL, _keyState)) {

	    if( !programmerKey) {
		programmerKey = true;
		programmerKeyTime = time;
	    }
	    return;
	}
	else if (deviceFlags() & NX_SECONDARYFNMASK)
	{ 
	    //On PowerBooks fn + CMD creates a POWER keycode, so change it now to CMD code
	    keyCode = ADBK_FLOWER;
	    //If fn is released before CMD key, we need to fake another CMD key later,
	    //  but if CMD key is released first then we don't need to do anything else
	    //  later since the CMD key will still generate a POWER key-up when fn is down.
	    _fn_key_invoked_power = goingDown;
	}
	else if (_fn_key_invoked_power)
	{
	    //If fn key is released before CMD key, then when CMD key is released
	    //  the keycode is still 7f (POWER).  But that makes CMD key look like
	    //  it is stuck in the down position from line above, so fake CMD up now
	    keyCode = ADBK_FLOWER;
	    _fn_key_invoked_power = false;
	}
    }
    
    if ((_enable_fwd_delete) && (deviceFlags() & NX_SECONDARYFNMASK))
    {
	if (keyCode == ADBK_DELETE)
	{
	    keyCode = ADBK_FORWARD_DELETE;
	    _fwd_delete_down = goingDown;
	}
	else if (keyCode == ADBK_PBFNKEY)
	{
	    if ((!goingDown) && (_fwd_delete_down))
	    {
		super::dispatchKeyboardEvent( pvirtualmap[ADBK_FORWARD_DELETE], 
		    false, time );
		_fwd_delete_down = false;
	    }
	}
    
    }

    // RY: For portable keyboards, we need to convert right hand modifiers
    // keycodes to left hand keycodes.  This is necessary because said
    // keycodes can only be generated with the use of the fn modifier.  To
    // elminate confusion, fn + modifier should be used to differentiate
    // the keystrokes.
    if ( adbDevice && (adbDevice->handlerID() > 0xc0))
    {
        switch ( keyCode )
        {
            case ADBK_CONTROL_R:
                keyCode = ADBK_CONTROL;
                break;
            case ADBK_SHIFT_R:
                keyCode = ADBK_SHIFT;
                break;
            case ADBK_OPTION_R:
                keyCode = ADBK_OPTION;
                break;
        }
    }

    super::dispatchKeyboardEvent( pvirtualmap[keyCode], goingDown, time );
}

// **********************************************************************************
// packet
//
// **********************************************************************************
IOReturn AppleADBKeyboard::packet (UInt8 * data, IOByteCount, UInt8 adbCommand )
{
unsigned int	keycode1, keycode2;
bool		down;
AbsoluteTime	now;

	//---Since packet is re-entrant, lock around it - [3418665] ---
	if ( _packetLock )
		IOLockLock( _packetLock );

keycode1 = *data;
down = ((keycode1 & 0x80) == 0);
keycode1 &= 0x7f;
if(keycode1 == 0x7e) keycode1 = ADBK_POWER;
clock_get_uptime(&now);
setTimeLastNonmodKeydown(now, keycode1);
dispatchKeyboardEvent(keycode1,down,now);

keycode2 = *(data + 1);
if( keycode2 != 0xff ) {
        down = ((keycode2 & 0x80) == 0);
        keycode2 &= 0x7f;
        if( keycode2 == 0x7e) keycode2 = ADBK_POWER;
	if( (keycode1 != ADBK_POWER) || (keycode2 != ADBK_POWER))
		dispatchKeyboardEvent(keycode2,down,now);
}

if ( _packetLock )
	IOLockUnlock( _packetLock );

return kIOReturnSuccess;
}

//This is needed by trackpads to correct jitter when gestures
//  are enabled.
void AppleADBKeyboard::setTimeLastNonmodKeydown (AbsoluteTime now, unsigned int keycode)
{
    switch (_virtualmap[keycode]) {
	case  0x3b:	//left control, not raw
	case  ADBK_FLOWER:	
	case  ADBK_SHIFT:	
	case  ADBK_SHIFT_R:    
	case  ADBK_CAPSLOCK:	
	case  ADBK_OPTION:	
	case  ADBK_OPTION_R:
	case  ADBK_PBFNKEY:	//PowerBook secondary fn key
	    break;
	default:
	    _lastkeydown = now;
	    break;
    }
}

AbsoluteTime AppleADBKeyboard::getTimeLastNonmodKeydown (void)
{
    if (CMP_ABSOLUTETIME( &_lastkeyCGEvent, &_lastkeydown) > 0)
     	return _lastkeyCGEvent;  //time of autorepeat
    else
	return _lastkeydown;     //time of physical keypress
}

//The only reason this method is subclassed is to access autorepeat
//  events without changing IOHIKeyboard's public APIs.  All normal
//  as well as autorepeated keys must come through here
void AppleADBKeyboard::keyboardEvent(unsigned eventType,
	/* flags */              unsigned flags,
	/* keyCode */            unsigned keyCode,
	/* charCode */           unsigned charCode,
	/* charSet */            unsigned charSet,
	/* originalCharCode */   unsigned origCharCode,
	/* originalCharSet */    unsigned origCharSet)
{

    //only save time if autorepeated key
    //save my own time, ignore super's time.  
    switch ( _codeToRepeat)
    {
	case  ADBK_F9:
	case  ADBK_F10:
	case  ADBK_F11:
	case  ADBK_F12:
	case  (unsigned) -1:
	    break;
	default:
	    clock_get_uptime(&_lastkeyCGEvent);  
	    break;
    }
	
    super::keyboardEvent( eventType, flags, keyCode, charCode, charSet,
	origCharCode, origCharSet);
}

IOReturn AppleADBKeyboard::callPlatformFunction(const OSSymbol *functionName,
						    bool waitForFunction,
						    void *param1, void *param2,
						    void *param3, void *param4)
{  
    if (functionName == _get_last_keydown)
    {
	AbsoluteTime *timeptr;
	
	timeptr = (AbsoluteTime *)param1;
	*timeptr = getTimeLastNonmodKeydown();
	return kIOReturnSuccess;
    }
    
    if (functionName == _get_handler_id)
    {
	UInt32	*id;
	
	id = (UInt32 *)param1;
	*id = deviceType();
	return kIOReturnSuccess;
    }

    if (functionName == _get_device_flags)
    {
	UInt32	*id;
	
	id = (UInt32 *)param1;
	//TBD:  check if keyboard is external or embedded?
	*id = deviceFlags();
	return kIOReturnSuccess;
    }
    
    
    return kIOReturnBadArgument;
}


// **********************************************************************************
// maxKeyCodes
//
// **********************************************************************************
UInt32 AppleADBKeyboard::maxKeyCodes ( void )
{
return 0x80;
}

//Get key values from ev_keymap.h
bool AppleADBKeyboard::doesKeyLock ( unsigned key)
{
    switch (key) {
    case NX_KEYTYPE_CAPS_LOCK:
        if ( adbDevice && (adbDevice->handlerID() > 0xc0))
            return false;
        else 
            return true;
	case NX_KEYTYPE_NUM_LOCK:
		return false;
	default:
		return false;
    }
}

// **********************************************************************************
//  setParamProperties
//  
// **********************************************************************************
IOReturn AppleADBKeyboard::setParamProperties( OSDictionary * dict )
{
    OSNumber 	*datan;

    if (_keybrdLock)
	IOLockLock (_keybrdLock);
    
    if (datan = OSDynamicCast(OSNumber, dict->getObject(kIOHIDFKeyModeKey)))
    {
	bool	theFkeyMode;
	
	theFkeyMode = (bool) datan->unsigned32BitValue();
	setFKeyMode(theFkeyMode); //This calls setProperty too
    }
    
    if (datan = OSDynamicCast(OSNumber, dict->getObject(kIOHIDStickyKeysOnKey)))
    {
	UInt32	propertydata;
	
        propertydata = (bool) datan->unsigned32BitValue();;
		
	if (propertydata) 
	{
	    _stickymodeON = true;
	}
	else
	{
	    _stickymodeON = false;
	}
    }

    if (_keybrdLock)
	IOLockUnlock (_keybrdLock);

    return super::setParamProperties(dict);    
}

// **********************************************************************************
//  keyboardSpecialEvent
//  Used only to capture sticky PowerBook fn key from IOHIDSystem (in IOHIDFamily)
// **********************************************************************************
void AppleADBKeyboard::keyboardSpecialEvent( unsigned eventType, unsigned flags, 
	unsigned keyCode, unsigned flavor)
{
    if ((_stickymodeON) && (eventType == NX_SYSDEFINED) && (keyCode == NX_NOSPECIALKEY))
    {
	if (flavor == NX_SUBTYPE_STICKYKEYS_FN_UP)
	{
	    _sticky_fn_ON = false;
	}
	if ((flavor == NX_SUBTYPE_STICKYKEYS_FN_LOCK) || 
	    (flavor == NX_SUBTYPE_STICKYKEYS_FN_DOWN))
	{
	    _sticky_fn_ON = true;
	}
    }
    return super::keyboardSpecialEvent(eventType, flags, keyCode, flavor);
}

// **********************************************************************************
// defaultKeymapOfLength
//
// **********************************************************************************
const unsigned char * AppleADBKeyboard::defaultKeymapOfLength (UInt32 * length )
{
static const unsigned char appleUSAKeyMap[] = {
            0x00,0x00,
	    0x0b,	//11 modifier keys
	    0x00,0x01,0x39,  //NX_MODIFIERKEY_ALPHALOCK
	    0x01,0x01,0x38,  //NX_MODIFIERKEY_SHIFT virtual from KMAP
	    0x02,0x01,0x3b,  //NX_MODIFIERKEY_CONTROL
	    0x03,0x01,0x3a,  //NX_MODIFIERKEY_ALTERNATE
	    0x04,0x01,0x37,	  //NX_MODIFIERKEY_COMMAND
	    0x05,0x11,0x52,0x41,0x4c,0x53,0x54,0x55,0x45,0x58,0x57,0x56,0x5b,0x5c,
            0x43,0x4b,0x51,0x4e,0x59,  //NX_MODIFIERKEY_NUMERICPAD no longer includes arrows
	    0x06,0x01,0x72, //NX_MODIFIERKEY_HELP  7th modifier here
	    0x07,0x01,0x3f, //NX_MODIFIERKEY_SECONDARYFN 8th modifier
	    0x09,0x01,0x3c, // NX_MODIFIERKEY_RSHIFT
	    0x0a,0x01,0x3e, // NX_MODIFIERKEY_RCONTROL
	    0x0b,0x01,0x3d, // NX_MODIFIERKEY_RALTERNATE
	    //0x0c,0x01,0x36, // 36 (for NX_MODIFIERKEY_RCOMMAND) not possible with Apple ADB Extended Keyboard 
	    0x7f,0x0d,0x00,0x61,
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
            0x08,0xff,0x02,0x00,0x1b,0x00,0x7e,0xff,0xff,0xff,0xff,0xff,
	    /*
	    0x00,0x01,0xac,0x00,
            0x01,0xae,0x00,0x01,0xaf,0x00,0x01,0xad,
	    */
	    0xff, 0xff, 0xff, 0xff,
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
	    //A.W.  Added following 4 lines to fix wakeup on PowerBooks. 
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
0x04, // following are 4 special keys
0x05,0x72,  //NX_KEYTYPE_HELP is 5, ADB code is 0x72
0x06,0x7f,  //NX_POWER_KEY is 6, ADB code is 0x7f
0x07,0x4a,  //NX_KEYTYPE_MUTE is 7, ADB code is 0x4a
//Removed so Right-Ctrl, Right-Option not seen as character-generating keys - [3543906]
//0x08,0x3e,  //NX_UP_ARROW_KEY is 8, ADB is 3e raw, 7e virtual (KMAP)
//0x09,0x3d,   //NX_DOWN_ARROW_KEY is 9, ADB is 0x3d raw, 7d virtual
0x0a,0x47   //NX_KEYTYPE_NUM_LOCK is 10, ADB combines with CLEAR key for numlock
    };
    
static const unsigned char appleUSAPortableKeyMap[] = {
            0x00,0x00,
	    0x08,	//11 modifier keys
	    0x00,0x01,0x39,  //NX_MODIFIERKEY_ALPHALOCK
	    0x01,0x01,0x38,  //NX_MODIFIERKEY_SHIFT virtual from KMAP
	    0x02,0x01,0x3b,  //NX_MODIFIERKEY_CONTROL
	    0x03,0x01,0x3a,  //NX_MODIFIERKEY_ALTERNATE
	    0x04,0x01,0x37,	  //NX_MODIFIERKEY_COMMAND
	    0x05,0x11,0x52,0x41,0x4c,0x53,0x54,0x55,0x45,0x58,0x57,0x56,0x5b,0x5c,
            0x43,0x4b,0x51,0x4e,0x59,  //NX_MODIFIERKEY_NUMERICPAD no longer includes arrows
	    0x06,0x01,0x72, //NX_MODIFIERKEY_HELP  7th modifier here
	    0x07,0x01,0x3f, //NX_MODIFIERKEY_SECONDARYFN 8th modifier
	    //0x0c,0x01,0x36, // 36 (for NX_MODIFIERKEY_RCOMMAND) not possible with Apple ADB Extended Keyboard 
	    0x7f,0x0d,0x00,0x61,
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
            0x08,0xff,0x02,0x00,0x1b,0x00,0x7e,0xff,0xff,0xff,0xff,0xff,
	    /*
	    0x00,0x01,0xac,0x00,
            0x01,0xae,0x00,0x01,0xaf,0x00,0x01,0xad,
	    */
	    0xff, 0xff, 0xff, 0xff,
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
	    //A.W.  Added following 4 lines to fix wakeup on PowerBooks. 
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
0x04, // following are 4 special keys
0x05,0x72,  //NX_KEYTYPE_HELP is 5, ADB code is 0x72
0x06,0x7f,  //NX_POWER_KEY is 6, ADB code is 0x7f
0x07,0x4a,  //NX_KEYTYPE_MUTE is 7, ADB code is 0x4a
//Removed so Right-Ctrl, Right-Option not seen as character-generating keys - [3543906]
//0x08,0x3e,  //NX_UP_ARROW_KEY is 8, ADB is 3e raw, 7e virtual (KMAP)
//0x09,0x3d,   //NX_DOWN_ARROW_KEY is 9, ADB is 0x3d raw, 7d virtual
0x0a,0x47   //NX_KEYTYPE_NUM_LOCK is 10, ADB combines with CLEAR key for numlock
    };


    if ( adbDevice && (adbDevice->handlerID() > 0xc0))
    {
        *length = sizeof(appleUSAPortableKeyMap);
        return appleUSAPortableKeyMap;
    }

    *length = sizeof(appleUSAKeyMap);
    return appleUSAKeyMap;
}



