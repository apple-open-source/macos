/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLEINTELPIIXATACONTROLLER_H
#define _APPLEINTELPIIXATACONTROLLER_H

#include <IOKit/IOService.h>

class AppleIntelPIIXATAController : public IOService
{
    OSDeclareDefaultStructors( AppleIntelPIIXATAController )

protected:
    UInt16       _ioPorts;
    UInt16       _irq;
    UInt16       _channel;
    IOService *  _provider;
    UInt8        _pioModes;
    UInt8        _dmaModes;
    UInt8        _udmaModes;
    const char * _deviceName;
    bool         _perChannelTimings;

	virtual bool setupInterrupt( UInt32 line );

public:
    virtual bool init( IOService *    provider,
                       OSDictionary * dictionary );

    virtual UInt16 getIOBaseAddress() const;

    virtual UInt16 getInterruptLine() const;

    virtual UInt16 getChannelNumber() const;

    virtual UInt8  getPIOModes() const;

    virtual UInt8  getDMAModes() const;
    
    virtual UInt8  getUltraDMAModes() const;

    virtual bool   hasPerChannelTimingSupport() const;

    virtual const char * getDeviceName() const;

    virtual bool handleOpen( IOService *  client,
                             IOOptionBits options,
                             void *       arg );

    virtual void handleClose( IOService *  client,
                              IOOptionBits options );

    virtual void pciConfigWrite8( UInt8 offset,
                                  UInt8 data,
                                  UInt8 mask = 0xff );

    virtual void pciConfigWrite16( UInt8  offset,
                                   UInt16 data,
                                   UInt16 mask = 0xffff );
};

#endif /* !_APPLEINTELPIIXATACONTROLLER_H */
