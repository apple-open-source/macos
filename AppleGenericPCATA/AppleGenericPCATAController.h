/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLEGENERICPCATACONTROLLER_H
#define _APPLEGENERICPCATACONTROLLER_H

#include <IOKit/IOService.h>

class AppleGenericPCATAController : public IOService
{
    OSDeclareDefaultStructors( AppleGenericPCATAController )

protected:
    UInt32      _ioPorts;
    UInt32      _irq;
    UInt32      _pioMode;
    IOService * _provider;
    bool        _irqSet;

    virtual bool setupInterrupt( IOService *provider, UInt32 line );

public:
    virtual bool init( OSDictionary * dictionary );

    virtual UInt32 getIOPorts() const;

    virtual UInt32 getInterruptLine() const;

    virtual UInt32 getPIOMode() const;

    virtual bool handleOpen( IOService *  client,
                             IOOptionBits options,
                             void *       arg );

    virtual void handleClose( IOService *  client,
                              IOOptionBits options );

    virtual bool attachToParent( IORegistryEntry *       parent,
                                 const IORegistryPlane * plane );

    virtual void detachFromParent( IORegistryEntry *       parent,
                                   const IORegistryPlane * plane ); 
};

#endif /* !_APPLEGENERICPCATACONTROLLER_H */
