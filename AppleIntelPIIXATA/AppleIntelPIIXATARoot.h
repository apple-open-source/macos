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

#ifndef _APPLEINTELPIIXATAROOT_H
#define _APPLEINTELPIIXATAROOT_H

#include <IOKit/IOLocks.h>
#include <IOKit/pci/IOPCIDevice.h>

class AppleIntelPIIXATARoot : public IOService
{
    OSDeclareDefaultStructors( AppleIntelPIIXATARoot )

protected:
    OSSet *       _nubs;
    OSSet *       _openNubs;
    IOPCIDevice * _provider;
    IOLock *      _pciConfigLock;

    virtual OSSet * createATAChannelNubs( void );

    virtual OSDictionary * createNativeModeChannelInfo( UInt32 ataChannel,
                                                        UInt8  mapValue );

    virtual OSDictionary * createLegacyModeChannelInfo( UInt32 ataChannel,
                                                        UInt8  mapValue );

    virtual OSDictionary * createChannelInfo( UInt32 ataChannel,
                                              UInt8  mapValue,
                                              UInt16 commandPort,
                                              UInt16 controlPort,
                                              UInt8  interruptVector );

    virtual IORegistryEntry * getDTChannelEntry( int channelID );

public:
    virtual IOService * probe( IOService * provider,
                               SInt32 *    score );

    virtual bool start( IOService * provider );

    virtual void free( void );

    virtual bool handleOpen( IOService *  client,
                             IOOptionBits options,
                             void *       arg );
    
    virtual void handleClose( IOService *  client,
                              IOOptionBits options );

    virtual bool handleIsOpen( const IOService * client ) const;

    virtual void pciConfigWrite8( UInt8 offset,
                                  UInt8 data,
                                  UInt8 mask = 0xff );
    
    virtual void pciConfigWrite16( UInt8  offset,
                                   UInt16 data,
                                   UInt16 mask = 0xffff );

    virtual bool serializeProperties( OSSerialize * s ) const;
};

#endif /* !_APPLEINTELPIIXATAROOT_H */
