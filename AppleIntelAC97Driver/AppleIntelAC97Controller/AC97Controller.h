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

#ifndef __APPLE_INTEL_AC97_CONTROLLER_H
#define __APPLE_INTEL_AC97_CONTROLLER_H

#include <IOKit/IOService.h>
#include "AC97Defines.h"
#include "AC97Codec.h"

class AppleIntelAC97Controller : public IOService
{
    OSDeclareAbstractStructors( AppleIntelAC97Controller )

protected:
    IOService *      _provider;   // our provider
    UInt32           _openMask;   // mask of codecs holding an open
    IOService *      _codecs[kMaxCodecCount];  // array of codecs

    // AC-Link control.

    virtual IOReturn reserveACLink();
    virtual void     releaseACLink();

    virtual void     coldReset();
    virtual void     warmReset();

    // Subclass start().

    virtual bool     configureProvider( IOService * provider ) = 0;

    // Bus master register access.

    virtual UInt32   bmRead32( UInt16      offset,
                               DMAChannel  channel = 0 ) const = 0;

    virtual UInt16   bmRead16( UInt16      offset,
                               DMAChannel  channel = 0 ) const = 0;

    virtual UInt8    bmRead8(  UInt16      offset,
                               DMAChannel  channel = 0 ) const = 0;

    virtual void     bmWrite32( UInt16     offset,
                                UInt32     value,
                                DMAChannel channel = 0 ) = 0;

    virtual void     bmWrite16( UInt16     offset,
                                UInt16     value,
                                DMAChannel channel = 0 ) = 0;

    virtual void     bmWrite8(  UInt16     offset,
                                UInt8      value,
                                DMAChannel channel = 0 ) = 0;

    inline UInt32    getGlobalStatus() const
    { return bmRead32(kGlobalStatus); }

    // Codec nub management.

    virtual UInt32   attachCodecs();
    
    virtual void     publishCodecs();

    virtual AppleIntelAC97Codec * createCodec( CodecID codecID ) = 0;

    virtual void     free();

public:
    virtual bool     start( IOService * provider );

    virtual const OSSymbol * getControllerFunction() const;

    // Handle codec open/close.

    virtual bool     handleOpen( IOService *  client,
                                 IOOptionBits options,
                                 void *       arg );

    virtual void     handleClose( IOService *  client,
                                  IOOptionBits options );

    virtual bool     handleIsOpen( const IOService * client ) const;

    // Mixer (codec) register access.

    virtual IOReturn mixerRead16( CodecID  codecID,
                                  UInt8    offset,
                                  UInt16 * value ) = 0;

    virtual IOReturn mixerWrite16( CodecID codecID,
                                   UInt8   offset,
                                   UInt16  value ) = 0;

    // Bus master functions used by client audio/modem drivers.

    virtual IOReturn setDescriptorBaseAddress( DMAChannel        channel,
                                               IOPhysicalAddress base );

    virtual void     setLastValidIndex( DMAChannel channel, UInt8 index );

    virtual UInt8    getCurrentIndexValue( DMAChannel channel ) const;

    virtual UInt32   getCurrentBufferPosition( DMAChannel channel,
                                               UInt8 *    index ) const;

    virtual IOReturn startDMAChannel( DMAChannel channel );

    virtual void     stopDMAChannel( DMAChannel channel );

    virtual bool     serviceChannelInterrupt( DMAChannel channel );
};

#endif /* !__APPLE_INTEL_AC97_CONTROLLER_H */
