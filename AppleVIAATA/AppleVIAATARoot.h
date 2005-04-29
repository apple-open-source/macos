/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLEVIAATAROOT_H
#define _APPLEVIAATAROOT_H

#include <IOKit/IOLocks.h>
#include <IOKit/pci/IOPCIDevice.h>

class AppleVIAATARoot : public IOService
{
    OSDeclareDefaultStructors( AppleVIAATARoot )

protected:
    IOPCIDevice *  fProvider;
    IOLock *       fPCILock;
    OSSet *        fChannels;
    OSSet *        fOpenChannels;
    IONotifier *   fISABridgeNotifier;
    UInt8          fHardwareType;
    UInt8          fHardwareFlags;
    bool           fIsSATA;

    static bool    isaBridgePublished( void *      target,
                                       void *      param,
                                       IOService * service );

    virtual OSSet * createATAChannels( void );

    virtual OSDictionary * createNativeModeChannelInfo( UInt32 ataChannel );

    virtual OSDictionary * createLegacyModeChannelInfo( UInt32 ataChannel );

    virtual OSDictionary * createChannelInfo( UInt32 ataChannel,
                                              UInt16 commandPort,
                                              UInt16 controlPort,
                                              UInt8  interruptVector );

    virtual IORegistryEntry * getDTChannelEntry( int channelID );

public:
    virtual IOService * probe( IOService * provider,
                               SInt32 *    score );

    virtual bool   start( IOService * provider );

    virtual void   free( void );

    virtual bool   handleOpen( IOService *  client,
                               IOOptionBits options,
                               void *       arg );
    
    virtual void   handleClose( IOService *  client,
                                IOOptionBits options );

    virtual bool   handleIsOpen( const IOService * client ) const;

    virtual UInt32 getHardwareType( void ) const;

    virtual UInt32 getHardwareFlags( void ) const;

    virtual UInt32 getUltraDMAModeMask( void ) const;

    virtual void   pciConfigWrite8( UInt8 offset,
                                    UInt8 data,
                                    UInt8 mask = 0xff );

    virtual void   pciConfigWrite32( UInt8  offset,
                                     UInt32 data,
                                     UInt32 mask = 0xffffffff );

    virtual UInt8  pciConfigRead8( UInt8 offset );

    virtual UInt16 pciConfigRead16( UInt8 offset );

    virtual UInt32 pciConfigRead32( UInt8 offset );
};

#endif /* !_APPLEVIAATAROOT_H */
