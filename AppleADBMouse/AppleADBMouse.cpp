/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * 18 June 1998 	  Start IOKit version.
 * 18 Nov  1998 suurballe port to C++
 *  4 Oct  1999 decesare  Revised for Type 4 support and sub-classed drivers.
 *  1 Feb  2000 tsherman  Added extended mouse functionality (implemented in setParamProperties)
 */

#include "AppleADBMouse.h"
#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>


//Need the following to compile with GCC3 
static inline int
my_abs(int x)
{
   return x < 0 ? -x : x;
}


static bool check_usb_mouse(OSObject *, void *, IOService * );

// ****************************************************************************
// NewMouseData
//
// ****************************************************************************
static void NewMouseData(IOService * target, UInt8 adbCommand, IOByteCount length, UInt8 * data)
{
  ((AppleADBMouse *)target)->packet(adbCommand, length, data);
}


// ****************************************************************************

#undef super
#define super IOHIPointing

OSDefineMetaClassAndStructors(AppleADBMouse, IOHIPointing);


// ****************************************************************************
// probe
//
// ****************************************************************************
IOService * AppleADBMouse::probe(IOService * provider, SInt32 * score)
{
//kprintf("ADB generic probe called\n");
  adbDevice = (IOADBDevice *)provider;
  return this;
}


// ****************************************************************************
// start
//
// ****************************************************************************
bool AppleADBMouse::start(IOService * provider)
{
//kprintf("ADB Mouse super is starting\n");
  if(!super::start(provider)) return false;
  
  if(!adbDevice->seizeForClient(this, NewMouseData)) {
    IOLog("%s: Seize failed\n", getName());
    return false;
  }
  return true;
}


// ****************************************************************************
// interfaceID
//
// ****************************************************************************
UInt32 AppleADBMouse::interfaceID(void)
{
  return NX_EVS_DEVICE_INTERFACE_ADB;
}


// ****************************************************************************
// deviceType
//
// ****************************************************************************
UInt32 AppleADBMouse::deviceType ( void )
{
  return adbDevice->handlerID();
}


// ****************************************************************************
// resolution
//
// ****************************************************************************
IOFixed AppleADBMouse::resolution(void)
{
  return _resolution;
}


// ****************************************************************************
// buttonCount
//
// ****************************************************************************
IOItemCount AppleADBMouse::buttonCount(void)
{
  return _buttonCount;
}


// ****************************************************************************
// packet
//
// ****************************************************************************
void AppleADBMouse::packet(UInt8 /*adbCommand*/,
			   IOByteCount /*length*/, UInt8 * data)
{
  int		  dx, dy;
  UInt32          buttonState = 0;
  AbsoluteTime    now;

  dy = data[0] & 0x7f;
  dx = data[1] & 0x7f;
  
  if (dy & 0x40) dy |= 0xffffffc0;
  if (dx & 0x40) dx |= 0xffffffc0;
  
  if ((data[0] & 0x80) == 0) buttonState |= 1;
  
  clock_get_uptime(&now);
  dispatchRelativePointerEvent(dx, dy, buttonState, now);
}


// ****************************************************************************

#undef super
#define super AppleADBMouse

OSDefineMetaClassAndStructors(AppleADBMouseType1, AppleADBMouse);

IOService * AppleADBMouseType1::probe(IOService * provider, SInt32 * score)
{
  if (!super::probe(provider, score)) return 0;
  
  return this;
}

bool AppleADBMouseType1::start(IOService * provider)
{
  OSNumber 	*dpi;

  if (adbDevice->setHandlerID(1) != kIOReturnSuccess) return false;
  
  dpi = OSDynamicCast( OSNumber, getProperty("dpi"));
  if (dpi)
  {
    _resolution = dpi->unsigned16BitValue() << 16;
  }
  else
  {
    _resolution = 100 << 16;
  }
  _buttonCount = 1;

  return super::start(provider);
}


// ****************************************************************************

#undef super
#define super AppleADBMouse

OSDefineMetaClassAndStructors(AppleADBMouseType2, AppleADBMouse);

IOService * AppleADBMouseType2::probe(IOService * provider, SInt32 * score)
{
  if (!super::probe(provider, score)) return 0;

  if (adbDevice->setHandlerID(2) != kIOReturnSuccess) return 0;
  return this;
}

bool AppleADBMouseType2::start(IOService * provider)
{
  OSNumber 	*dpi;
  
  if (adbDevice->setHandlerID(2) != kIOReturnSuccess) return false;
  
  dpi = OSDynamicCast( OSNumber, getProperty("dpi"));
  if (dpi)
  {
    _resolution = dpi->unsigned16BitValue() << 16;
  }
  else
  {
    _resolution = 200 << 16;
  }
  _buttonCount = 1;
  
  return super::start(provider);
}


// ****************************************************************************

#undef super
#define super AppleADBMouse

OSDefineMetaClassAndStructors(AppleADBMouseType4, AppleADBMouse);

IOService * AppleADBMouseType4::probe(IOService * provider, SInt32 * score)
{
  UInt8       data[8];
  IOByteCount length = 8;
  
  if (!super::probe(provider, score)) return 0;
  
  if (adbDevice->setHandlerID(4) != kIOReturnSuccess) {
    adbDevice->setHandlerID(adbDevice->defaultHandlerID());
    return 0;
  }
  
  // To be a Type 4 Extended Mouse, register 1 must return 8 bytes.
  if (adbDevice->readRegister(1, data, &length) != kIOReturnSuccess) return 0;
  if (length != 8) return 0;
  
  // Save the device's Extended Mouse Info.
  deviceSignature  = ((UInt32 *)data)[0];
  deviceResolution = ((UInt16 *)data)[2];
  deviceClass      = data[6];
  deviceNumButtons = data[7];
  
  return this;
}

bool AppleADBMouseType4::start(IOService * provider)
{
  UInt8       adbdata[8];
  IOByteCount adblength = 8;
  OSNumber 	*dpi;

  typeTrackpad = FALSE;

  if (adbDevice->setHandlerID(4) != kIOReturnSuccess) return false;

  dpi = OSDynamicCast( OSNumber, getProperty("dpi"));
  if (dpi)
  {
    _resolution = dpi->unsigned16BitValue() << 16;
  }
  else
  {
    _resolution = deviceResolution << 16;
  }

  _buttonCount = deviceNumButtons;
  _notifierA = _notifierT = NULL;  //Only used by trackpad, but inspected by all type 4 mice

  adbDevice->readRegister(1, adbdata, &adblength);
  if( (adbdata[0] == 't') && (adbdata[1] = 'p') && (adbdata[2] == 'a') && (adbdata[3] == 'd') )
  {
    mach_timespec_t     t;
    OSNumber 		*jitter_num;

    t.tv_sec = 1; //Wait for keyboard driver for up to 1 second
    t.tv_nsec = 0;
    typeTrackpad = TRUE;
    enableEnhancedMode();
    _pADBKeyboard = waitForService(serviceMatching("AppleADBKeyboard"), &t);
    jitter_num = OSDynamicCast( OSNumber, getProperty("Trackpad Jitter Milliseconds"));
    if (jitter_num)
    {
	_jitterclicktime64 = jitter_num->unsigned16BitValue() * 1000 * 1000;  // in nanoseconds;
    }
    else
    {
	_jitterclicktime64 = 750 * 1000 * 1000;  // in nanoseconds;
    }
    jitter_num = OSDynamicCast( OSNumber, getProperty("Trackpad Jitter Max delta"));
    if (jitter_num)
    {
	_jitterdelta = jitter_num->unsigned16BitValue();
	if (_jitterdelta == 0)
	    _jittermove = false;
    }
    else
    {
	_jitterdelta = 16;  // pixels;
    }

    setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDTrackpadAccelerationType);        
  } //end of trackpad processing
  
  return super::start(provider);
}


void AppleADBMouseType4::free( void )
{
    if (_notifierA)
    {
	_notifierA->remove();
	_notifierA = NULL;
    }
    if (_notifierT)
    {
	_notifierT->remove();
	_notifierT = NULL;
    }
    _ignoreTrackpad = false;
    super::free();
}

bool check_usb_mouse(OSObject * us, void *, IOService * yourDevice)
{
    if (us)
    {
	((AppleADBMouseType4 *)us)->_check_usb_mouse();
    }
    return true;
}

/*
 *  If a USB mouse HID driver is found, then disable the trackpad.
 */
void AppleADBMouseType4::_check_usb_mouse( void ) 
{
    IOService		*pHIDDevice;
    bool		foundUSBHIDMouse = false;
	
    OSIterator	*iterator = NULL;
    OSDictionary	*dict = NULL;
    OSNumber  	*usbClass, *usbPage, *usbUsage;

    dict = IOService::serviceMatching( "IOUSBHIDDriver" );
    if( dict )
    {
	iterator = IOService::getMatchingServices( dict );
	if( iterator )
	{
	    while( (pHIDDevice = (IOHIDDevice *) iterator->getNextObject()) )
	    {
		usbClass = OSDynamicCast( OSNumber, pHIDDevice->getProperty("bInterfaceClass"));
		usbPage = OSDynamicCast( OSNumber, pHIDDevice->getProperty("PrimaryUsagePage"));
		usbUsage = OSDynamicCast( OSNumber, pHIDDevice->getProperty("PrimaryUsage"));

		if ((usbClass == NULL) || (usbPage == NULL) || (usbUsage == NULL) )
		{
		    IOLog("Null found for properties that should exist in IOUSBHIDDriver\n");
		    continue;
		}

		//Keithen said the only way to find a USB mouse in either boot or report
		//  protocol is to make sure the class is 3 (HID) and the page is 1 (desktop)
		//  and the usage is 2 (mouse).  Subclass is 1 for boot protocol and 0 for
		//  report protocol.  bInterfaceProtocol does not exist as a property for
		//  IOUSBHIDDriver objects.
		if ((usbClass->unsigned16BitValue() == 3) && (usbUsage->unsigned16BitValue() == 2) 
		    && (usbPage->unsigned16BitValue() == 1))
		{
		    _ignoreTrackpad = true;
		    foundUSBHIDMouse = true;
		    break;
		}		
	    }
	}

	if( dict ) dict->release();
	if( iterator ) iterator->release();
    }

    if (!foundUSBHIDMouse) 
    {
	//If USB mouse is unplugged, then restore trackpad operation in ::packet()
	_ignoreTrackpad = false;
    }
    
}


void AppleADBMouseType4::packet(UInt8 /*adbCommand*/, IOByteCount length, UInt8 * data)
{
  int		  dx, dy, cnt, numExtraBytes;
  UInt32          buttonState = 0;
  AbsoluteTime	  now;

  if (_notifierA && _notifierT)
  {
    if (typeTrackpad && _ignoreTrackpad) 
      return;
  }
  
  numExtraBytes = length - 2;
  dy = data[0] & 0x7f;
  dx = data[1] & 0x7f;
  
  if ((data[0] & 0x80) == 0) 
  {
    buttonState |= 1;
  }
  
  if ((deviceNumButtons > 1) && ((data[1] & 0x80) == 0))
  {
    if(typeTrackpad)
    {
	if ((_jitterclick) && (_pADBKeyboard))
	{
	    const OSSymbol 	*gettime;
	    AbsoluteTime	keyboardtime; 
	    UInt64		nowtime64, keytime64;

	    gettime = OSSymbol::withCString("get_last_keydown");
	    keyboardtime.hi = 0;
	    keyboardtime.lo = 0;
	    _pADBKeyboard->callPlatformFunction(gettime, false, 
		(void *)&keyboardtime, 0, 0, 0);
	    clock_get_uptime(&now);
	    absolutetime_to_nanoseconds(now, &nowtime64);
	    absolutetime_to_nanoseconds(keyboardtime, &keytime64);
	    if (nowtime64 - keytime64 > _jitterclicktime64)
	    {
		buttonState |= 1;
	    }
	}
	else
	    buttonState |= 1;
    }
    else
    {
        buttonState |= 2;
    }
  }

  for (cnt = 0; cnt < numExtraBytes; cnt++) {
    dy |= ((data[2 + cnt] >> 4) & 7) << (7 + (cnt * 3));
    dx |= ((data[2 + cnt])      & 7) << (7 + (cnt * 3));
    
    if ((deviceNumButtons > (cnt + 2)) && ((data[2 + cnt] & 0x80) == 0))
      buttonState |= 4 << (cnt * 2);
    if ((deviceNumButtons > (cnt + 2 + 1)) && ((data[2 + cnt] & 0x08) == 0))
      buttonState |= 4 << (cnt * 2 + 1);
  }
  
  if (dy & (0x40 << (numExtraBytes * 3)))
    dy |= (0xffffffc0 << (numExtraBytes * 3));
  if (dx & (0x40 << (numExtraBytes * 3)))
    dx |= (0xffffffc0 << (numExtraBytes * 3));
  
  clock_get_uptime(&now);
  if(typeTrackpad)
  {
	if (_jittermove)
	{
	    if (_pADBKeyboard)
	    {
		const OSSymbol 	*gettime;
		AbsoluteTime	keyboardtime; 
		UInt64		nowtime64, keytime64;

		gettime = OSSymbol::withCString("get_last_keydown");
		keyboardtime.hi = 0;
		keyboardtime.lo = 0;
		_pADBKeyboard->callPlatformFunction(gettime, false, (void *)&keyboardtime, 0, 0, 0);
		absolutetime_to_nanoseconds(now, &nowtime64);
		absolutetime_to_nanoseconds(keyboardtime, &keytime64);
		if (nowtime64 - keytime64 < _jitterclicktime64)
		{
		    if ((my_abs(dx) < _jitterdelta) && (my_abs(dy) < _jitterdelta))
		    {
			if (!buttonState)
			{
			    //simulate no mouse move.  Keeps cursor invisible.
			    return;
			}
			else
			{
			    dx = 0;
			    dy = 0;
			}
		    }
		}
	    }
	} //jittermove
  }
  dispatchRelativePointerEvent(dx, dy, buttonState, now);
}

OSData * AppleADBMouseType4::copyAccelerationTable()
{
    char keyName[10];

    strcpy( keyName, "accl" );
    keyName[4] = (deviceSignature >> 24);
    keyName[5] = (deviceSignature >> 16);
    keyName[6] = (deviceSignature >> 8);
    keyName[7] = (deviceSignature >> 0);
    keyName[8] = 0;

    OSData * data = OSDynamicCast( OSData,
                getProperty( keyName ));
    if( data)
    {
        data->retain();
    }
    else
    {
        data = super::copyAccelerationTable();
    }
        
    return( data );
}

// ****************************************************************************
// enableEnhancedMode
//
// ****************************************************************************

bool AppleADBMouseType4::enableEnhancedMode()
{
    UInt8       adbdata[8];
    IOByteCount adblength = 8;
    OSNumber 	*plistnum;
    
    //IOLog("enableEnhancedMode called.\n");
    adbDevice->readRegister(1, adbdata, &adblength);

    if((adbdata[6] != 0x0D))
    {
        adbdata[6] = 0xD;
        if (adbDevice->writeRegister(1, adbdata, &adblength) != 0)
            return FALSE;
        if (adbDevice->readRegister(1, adbdata, &adblength) != 0)
            return FALSE;
        if (adbdata[6] != 0x0D)
        {
            IOLog("AppleADBMouseType4 deviceClass = %d (non-Extended Mode)\n", adbdata[6]);
            return FALSE;
        }
        //IOLog("AppleADBMouseType4 deviceClass = %d (Extended Mode)\n", adbdata[6]);

        // Set ADB Extended Features to default values.
        adbdata[0] = 0x19;
        adbdata[1] = 0x14;
        adbdata[2] = 0x19;
        adbdata[3] = 0xB2;
        adbdata[4] = 0xB2;
        adbdata[5] = 0x8A;
        adbdata[6] = 0x1B;
        adbdata[7] = 0x50;
        adblength = 8;

        adbDevice->writeRegister(2, adbdata, &adblength);

        /* Add IORegistry entries for Enhanced mode */
        Clicking = FALSE;
        Dragging = FALSE;
        DragLock = FALSE;
        
        setProperty("Clicking", (unsigned long long)Clicking, sizeof(Clicking)*8);
        setProperty("Dragging", (unsigned long long)Dragging, sizeof(Dragging)*8);
        setProperty("DragLock", (unsigned long long)DragLock, sizeof(DragLock)*8);
	
	/* check for jitter correction initially before Mouse Preferences
	   calls setParamProperties*/
	plistnum = OSDynamicCast( OSNumber, getProperty("JitterNoClick"));
	if (plistnum)
	{
	    _jitterclick = (bool) plistnum->unsigned16BitValue();
	}
	else
	{
	    _jitterclick = false;
	}
	
	plistnum = OSDynamicCast( OSNumber, getProperty("JitterNoMove"));
	if (plistnum)
	{
	    _jittermove = (bool) plistnum->unsigned16BitValue();
	}
	else
	{
	    _jittermove = false;
	}	
        return TRUE;
    }

    return FALSE;
}

// ****************************************************************************
// setParamProperties
//
// ****************************************************************************
IOReturn AppleADBMouseType4::setParamProperties( OSDictionary * dict )
{
    OSData *	data;
    OSNumber 	*datan;
    IOReturn	err = kIOReturnSuccess;
    UInt8       adbdata[8];
    IOByteCount adblength;
    
    if( (data = OSDynamicCast(OSData, dict->getObject("Clicking"))) && (typeTrackpad == TRUE) )
    {
        adblength = sizeof(adbdata);
        adbDevice->readRegister(2, adbdata, &adblength);
        adbdata[0] = (adbdata[0] & 0x7F) | (*( (UInt8 *) data->getBytesNoCopy() ))<<7;
        setProperty("Clicking", (unsigned long long)((adbdata[0]&0x80)>>7), sizeof(adbdata[0])*8);
        adbDevice->writeRegister(2, adbdata, &adblength);
    }

    if( (data = OSDynamicCast(OSData, dict->getObject("Dragging"))) && (typeTrackpad == TRUE) )
    {
        adblength = sizeof(adbdata);
        adbDevice->readRegister(2, adbdata, &adblength);
        adbdata[1] = (adbdata[1] & 0x7F) | (*( (UInt8 *) data->getBytesNoCopy() ))<<7;
        setProperty("Dragging", (unsigned long long)((adbdata[1]&0x80)>>7), sizeof(adbdata[1])*8);
        adbDevice->writeRegister(2, adbdata, &adblength);
    }

    if( (data = OSDynamicCast(OSData, dict->getObject("DragLock"))) && (typeTrackpad == TRUE) )
    {
        adblength = sizeof(adbdata);
        adbDevice->readRegister(2, adbdata, &adblength);
        adbdata[3] = *((UInt8 *) data->getBytesNoCopy());

        if(adbdata[3])
        {
            setProperty("DragLock", (unsigned long long)adbdata[3], sizeof(adbdata[3])*8);
            adbdata[3] = 0xFF;
            adblength = sizeof(adbdata);
            adbDevice->writeRegister(2, adbdata, &adblength);
        }
        else
        {
            setProperty("DragLock", (unsigned long long)adbdata[3], sizeof(adbdata[3])*8);
            adbdata[3] = 0xB2;
            adblength = sizeof(adbdata);
            adbDevice->writeRegister(2, adbdata, &adblength);
        }
    }
    
    if( (datan = OSDynamicCast(OSNumber, dict->getObject("JitterNoClick"))) && (typeTrackpad == TRUE) )
    {
	_jitterclick = (bool) datan->unsigned32BitValue();
	setProperty("JitterNoClick", _jitterclick, sizeof(UInt32));

    }
    
    if( (datan = OSDynamicCast(OSNumber, dict->getObject("JitterNoMove"))) && (typeTrackpad == TRUE) )
    {
	_jittermove = (bool) datan->unsigned32BitValue();
	setProperty("JitterNoMove", _jittermove, sizeof(UInt32));
    }

    if( (datan = OSDynamicCast(OSNumber, dict->getObject("USBMouseStopsTrackpad"))) && (typeTrackpad == TRUE) )
    {
	UInt8		mode;

	mode = datan->unsigned32BitValue();	
        setProperty("USBMouseStopsTrackpad", (unsigned long long)(mode), sizeof(mode)*8);
	if (mode)
	{
	    if ( ! _notifierA)
		_notifierA = addNotification( gIOFirstMatchNotification, serviceMatching( "IOUSBHIDDriver" ), 
                     (IOServiceNotificationHandler)check_usb_mouse, this, 0 ); 
	    if (! _notifierT)
		_notifierT = addNotification( gIOTerminatedNotification, serviceMatching( "IOUSBHIDDriver" ), 
                     (IOServiceNotificationHandler)check_usb_mouse, this, 0 ); 
	    //The same C function can handle both firstmatch and termination notifications
	}
	else
	{
	    if (_notifierA)
	    {
		_notifierA->remove();
		_notifierA = NULL;
	    }
	    if (_notifierT)
	    {
		_notifierT->remove();
		_notifierT = NULL;
	    }
	    _ignoreTrackpad = false;
	}

    }

#if 0
    // For debugging purposes
    adblength = 8;
    adbDevice->readRegister(2, adbdata, &adblength);
    IOLog("adbdata[0] = 0x%x\n", adbdata[0]);
    IOLog("adbdata[1] = 0x%x\n", adbdata[1]);
    IOLog("adbdata[2] = 0x%x\n", adbdata[2]);
    IOLog("adbdata[3] = 0x%x\n", adbdata[3]);
    IOLog("adbdata[4] = 0x%x\n", adbdata[4]);
    IOLog("adbdata[5] = 0x%x\n", adbdata[5]);
    IOLog("adbdata[6] = 0x%x\n", adbdata[6]);
    IOLog("adbdata[7] = 0x%x\n", adbdata[7]);
#endif

    if (err == kIOReturnSuccess)
    {
        return super::setParamProperties(dict);
    }
    
    IOLog("AppleADBMouseType4::setParamProperties failing here\n");
    return( err );
}

