/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLEINTELICHXSATA_H
#define _APPLEINTELICHXSATA_H

#include "AppleIntelPIIXPATA.h"

class AppleIntelICHxSATA : public AppleIntelPIIXPATA
{
    OSDeclareDefaultStructors( AppleIntelICHxSATA )

protected:
    bool             _initPortEnable;

    virtual void     setSATAPortEnable( UInt32 driveUnit, bool enable );
    
    virtual bool     getSATAPortPresentStatus( UInt32 driveUnit );

    virtual IOReturn selectDevice( ataUnitID unit );

public:
    virtual bool     start( IOService * provider );

    virtual IOReturn provideBusInfo( IOATABusInfo * infoOut );
    
    virtual UInt32   scanForDrives( void );

    virtual IOReturn setPowerState( unsigned long stateIndex,
                                    IOService *   whatDevice );
};

#endif /* !_APPLEINTELICHXSATA_H */
