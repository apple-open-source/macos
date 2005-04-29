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

#ifndef _APPLEGENERICPCATADRIVER_H
#define _APPLEGENERICPCATADRIVER_H

#include <IOKit/IOTypes.h>
#include <IOKit/ata/IOATAController.h>
#include <IOKit/IOInterruptEventSource.h>

class AppleGenericPCATADriver : public IOATAController
{
    OSDeclareDefaultStructors( AppleGenericPCATADriver )

private:
    UInt16                      _irq;
    IOInterruptEventSource *    _intSrc;
    AppleGenericPCATAChannel *  _provider;

    /* Interrupt event source action */
    
    static void interruptOccurred( OSObject *               owner,
                                   IOInterruptEventSource * evtSrc,
                                   int                      count );

public:
    /* IOService overrides */

    virtual bool     start( IOService * provider );

    virtual void     free( void );

    virtual IOWorkLoop * getWorkLoop( void ) const;

    virtual IOReturn message( UInt32      type,
                              IOService * provider,
                              void *      argument );

    /* Mandatory IOATAController overrides */

	virtual bool     configureTFPointers( void );

	virtual IOReturn provideBusInfo( IOATABusInfo * infoOut );

    virtual IOReturn getConfig( IOATADevConfig * configOut,
                                UInt32           unitNumber );

    virtual IOReturn selectConfig( IOATADevConfig * configRequest,
                                   UInt32           unitNumber );

    /* Optional IOATAController overrides */

    virtual UInt32   scanForDrives( void );

    virtual IOReturn handleQueueFlush( void );

    virtual IOReturn synchronousIO( void );
};

#endif /* !_APPLEGENERICPCATADRIVER_H */
