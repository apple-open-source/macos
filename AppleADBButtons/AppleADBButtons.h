#ifndef _APPLEADBBUTTONS_H
#define _APPLEADBBUTTONS_H

/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/adb/IOADBDevice.h>

#define kVolume_up			0x06
#define kVolume_down			0x07
#define kMute				0x08
#define kVolume_up_AV			0x03  //Apple ADB AV monitors have different button codes
#define kVolume_down_AV			0x02
#define kMute_AV			0x01
#define kBrightness_up			0x09
#define kBrightness_down		0x0a
#define kEject				0x0b
#define kVideoMirror			0x0c
#define kIllumination_toggle		0x0d
#define kIllumination_down		0x0e
#define kIllumination_up		0x0f
#define kNum_lock_on_laptops		0x7f

#define kMax_registrations 		10
#define	kMax_keycode			0x0a
#define kNullKey			0xFF

typedef void (*button_handler)(void * );

class AppleADBButtons :  public IOHIKeyboard
{
    OSDeclareDefaultStructors(AppleADBButtons)

private:

    unsigned int	keycodes[kMax_registrations];
    void *		registrants[kMax_registrations];
    button_handler	downHandlers[kMax_registrations];

    void dispatchButtonEvent (unsigned int, bool );
    UInt32		_initial_handler_id;
    const OSSymbol 	*register_for_button;
    UInt32		_eject_delay;
    thread_call_t	_peject_timer;
    bool		_eject_released;
    IOService 		*_pADBKeyboard;
    const OSSymbol 	*_get_handler_id, *_get_device_flags;

public:

    const unsigned char * defaultKeymapOfLength (UInt32 * length );
    UInt32 interfaceID();
    UInt32 deviceType();
    UInt64 getGUID();
    void _check_eject_held( void ) ;

public:

    IOService * displayManager;			// points to display manager
    IOADBDevice *	adbDevice;

    bool start ( IOService * theNub );
    IOReturn packet (UInt8 * data, IOByteCount length, UInt8 adbCommand );
    IOReturn registerForButton ( unsigned int, IOService *, button_handler, bool );

    IOReturn setParamProperties(OSDictionary *dict);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);

};

#endif /* _APPLEADBBUTTONS_H */
