/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
#ifndef _IOKIT_APPLEUSBKEYBOARD_H
#define _IOKIT_APPLEUSBKEYBOARD_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USB.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

enum {
    kUSB_LEFT_CONTROL_BIT = 0x01,
    kUSB_LEFT_SHIFT_BIT = 0x02,
    kUSB_LEFT_ALT_BIT = 0x04,
    kUSB_LEFT_FLOWER_BIT = 0x08,

    kUSB_RIGHT_CONTROL_BIT = 0x10,
    kUSB_RIGHT_SHIFT_BIT = 0x20,
    kUSB_RIGHT_ALT_BIT = 0x040,
    kUSB_RIGHT_FLOWER_BIT = 0x80
};

enum {
    kUSB_LOWSPEED_MAXPACKET = 8,
    kUSB_CAPSLOCKLED_SET = 2,
    kUSB_NUMLOCKLED_SET = 1
};

/* Following table is used to convert Apple USB keyboard IDs into a numbering
   scheme that can be combined with ADB handler IDs for both Cocoa and Carbon */
enum {
    kgestUSBCosmoANSIKbd      = 198,     /* (0xC6) Gestalt Cosmo USB Domestic (ANSI) Keyboard */
    kprodUSBCosmoANSIKbd      = 0x201,   // The actual USB product ID in hardware
    kgestUSBCosmoISOKbd       = 199,     /* (0xC7) Cosmo USB International (ISO) Keyboard */
    kprodUSBCosmoISOKbd       = 0x202,
    kgestUSBCosmoJISKbd       = 200,     /* (0xC8) Cosmo USB Japanese (JIS) Keyboard */
    kprodUSBCosmoJISKbd       = 0x203,
    kgestUSBAndyANSIKbd       = 204,      /* (0xCC) Andy USB Keyboard Domestic (ANSI) Keyboard */
    kprodUSBAndyANSIKbd       = 0x204,
    kgestUSBAndyISOKbd        = 205,      /* (0xCD) Andy USB Keyboard International (ISO) Keyboard */
    kprodUSBAndyISOKbd	      = 0x205,
    kgestUSBAndyJISKbd        = 206,       /* (0xCE) Andy USB Keyboard Japanese (JIS) Keyboard */
    kprodUSBAndyJISKbd	      = 0x206,
    kgestQ6ANSIKbd	      = 31,      /* (0x31) Apple Q6 Keyboard Domestic (ANSI) Keyboard */
    kprodQ6ANSIKbd	      = 0x208,
    kgestQ6ISOKbd	      = 32,      /* (0x32) Apple Q6 Keyboard International (ISO) Keyboard */
    kprodQ6ISOKbd	      = 0x209,
    kgestQ6JISKbd	      = 33,       /* (0x33) Apple Q6 Keyboard Japanese (JIS) Keyboard */
    kprodQ6JISKbd	      = 0x20a,
    kgestUSBProF16ANSIKbd	= 34,		/* USB Pro Keyboard w/ F16 key Domestic (ANSI) Keyboard */
    kprodUSBProF16ANSIKbd	= 0x20B,
    kgestUSBProF16ISOKbd	= 35,		/* USB Pro Keyboard w/ F16 key International (ISO) Keyboard */
    kprodUSBProF16ISOKbd	= 0x20C,
    kgestUSBProF16JISKbd	= 36,	/* USB Pro Keyboard w/ F16 key Japanese (JIS) Keyboard */
    kprodUSBProF16JISKbd	= 0x20D
};

#define ADB_CONVERTER_LEN       0xff + 1   //length of array def_usb_2_adb_keymap[]
#define kKeyboardRetryCount	3

class AppleUSBKeyboard : public IOHIKeyboard
{
    OSDeclareDefaultStructors(AppleUSBKeyboard)

   
    IOUSBInterface *		_interface;
    IOUSBDevice *		_device;
    IOUSBPipe * 		_interruptPipe;
    IOBufferMemoryDescriptor *	_buffer;
    IOCommandGate *		_gate;

    IOUSBCompletion		_completion;
    IOUSBDevRequest  		_request;

    UInt32			_retryCount;
    UInt32			_outstandingIO;

    thread_call_t		_deviceDeadCheckThread;
    thread_call_t		_clearFeatureEndpointHaltThread;
    thread_call_t		_asyncLEDThread;

    UInt16			_maxPacketSize;

    UInt8			old_array[kUSB_LOWSPEED_MAXPACKET];
    UInt8                       usb_2_adb_keymap[ADB_CONVERTER_LEN + 1];
    UInt8       		_hid_report[8];
    UInt8			prev_bytes_read;
    UInt8			oldmodifier;
    UInt8			_ledState;

    bool			_prevent_LED_set;
    bool			_flower_key;  //Mac Command key
    bool			_control_key; //Control needed for 3-finger reboot
    bool			_deviceDeadThreadActive;
    bool			_deviceIsDead;
    bool			_deviceHasBeenDisconnected;
    bool			_needToClose;
    
    // IOService methods
    virtual bool	init(OSDictionary *properties);
    virtual bool	start(IOService * provider);
    virtual void 	stop(IOService *  provider);
    virtual bool 	finalize(IOOptionBits options);
    virtual IOReturn 	message( UInt32 type, IOService * provider,  void * argument = 0 );

    // "new" IOService methods. Some of these may go away before we ship 1.8.5
    virtual bool 	willTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	didTerminate( IOService * provider, IOOptionBits options, bool * defer );
#if 0
    virtual bool 	requestTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	terminate( IOOptionBits options = 0 );
    virtual void 	free( void );
    virtual bool 	terminateClient( IOService * client, IOOptionBits options );
#endif

    // IOHIDevice methods
    UInt32 			interfaceID ( void );
    UInt32 			deviceType ( void );

    // IOHIKeyboard methods
    UInt32 			maxKeyCodes ( void );
    const unsigned char * 	defaultKeymapOfLength (UInt32 * length );
    void 			setAlphaLockFeedback ( bool LED_state);
    void 			setNumLockFeedback ( bool LED_state);
    bool 			doesKeyLock ( unsigned key);
    unsigned 			getLEDStatus (void );
    
    // misc methods
    void 		Simulate_ADB_Event(UInt8 *key_ptr, UInt32 bytes_read);
    void   		Set_LED_States( UInt8);
    void		Set_Idle_Millisecs(UInt16 msecs);
    void   		Get_LED_States( void);    
    UInt32 		handlerID ( void );
    void 		InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining);
    void		CheckForDeadDevice();
    void		ClearFeatureEndpointHalt(void);
    void		DecrementOutstandingIO(void);
    void		IncrementOutstandingIO(void);
    
    // static methods for callbacks, the command gate, new threads, etc.
    static void 	AsyncLED (OSObject *target);
    static void 	InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining);
    static void 	CheckForDeadDeviceEntry(OSObject *target);
    static void		ClearFeatureEndpointHaltEntry(OSObject *target);
    static IOReturn	ChangeOutstandingIO(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);

};

#endif _IOKIT_APPLEUSBKEYBOARD_H
