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

#ifndef __APPLE_INTEL_ICH3_AC97_CONTROLLER_H
#define __APPLE_INTEL_ICH3_AC97_CONTROLLER_H

#include <IOKit/pci/IOPCIDevice.h>
#include "AC97Controller.h"

class AppleIntelICH3AC97Controller : public AppleIntelAC97Controller
{
    OSDeclareDefaultStructors( AppleIntelICH3AC97Controller )

protected:
    UInt16           _mixerBase;
    UInt16           _bmBase;

    virtual bool     configureProvider( IOService * provider );

    virtual UInt32   bmRead32( UInt16      offset,
                               DMAChannel  channel = 0 ) const;

    virtual UInt16   bmRead16( UInt16      offset,
                               DMAChannel  channel = 0 ) const;

    virtual UInt8    bmRead8(  UInt16      offset,
                               DMAChannel  channel = 0 ) const;

    virtual void     bmWrite32( UInt16     offset,
                                UInt32     value,
                                DMAChannel channel = 0 );

    virtual void     bmWrite16( UInt16     offset,
                                UInt16     value,
                                DMAChannel channel = 0 );

    virtual void     bmWrite8(  UInt16     offset,
                                UInt8      value,
                                DMAChannel channel = 0 );

    virtual void     free();

    virtual AppleIntelAC97Codec * createCodec( CodecID codecID );

public:
    virtual IOReturn mixerRead16( CodecID  codecID,
                                  UInt8    offset,
                                  UInt16 * value );

    virtual IOReturn mixerWrite16( CodecID codecID,
                                   UInt8   offset,
                                   UInt16  value );
};

#endif /* !__APPLE_INTEL_ICH3_AC97_CONTROLLER_H */
