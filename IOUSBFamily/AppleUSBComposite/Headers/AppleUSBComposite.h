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
#ifndef _IOKIT_APPLEUSBCOMPOSITE_H
#define _IOKIT_APPLEUSBCOMPOSITE_H

#include <IOKit/IOLib.h>
#include <IOKit/IONotifier.h>
#include <IOKit/IOService.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USB.h>

class AppleUSBComposite : public IOService
{
    OSDeclareDefaultStructors(AppleUSBComposite)
    
    IOUSBDevice	* 	_device;
    IONotifier * 	_notifier;
    bool		_expectingClose;
    
    bool		ConfigureDevice();
    IOReturn		ReConfigureDevice();

    static IOReturn	CompositeDriverInterestHandler(  void * target, void * refCon, UInt32 messageType, IOService * provider,  void * messageArgument, vm_size_t argSize );

public:

    virtual bool	start(IOService * provider);
#if 0
    virtual void 	stop( IOService * provider );
    virtual bool 	finalize(IOOptionBits options);
#endif
    virtual IOReturn 	message( UInt32 type, IOService * provider,  void * argument = 0 );

    // "new" IOKit methods. Some of these may go away before we ship 1.8.5
    virtual bool 	willTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	didTerminate( IOService * provider, IOOptionBits options, bool * defer );
#if 0
    virtual bool 	requestTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	terminate( IOOptionBits options = 0 );
    virtual void 	free( void );
    virtual bool 	terminateClient( IOService * client, IOOptionBits options );
#endif
};

#endif _IOKIT_APPLEUSBCOMPOSITE_H
