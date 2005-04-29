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

#ifndef _APPLEGENERICPCATAROOT_H
#define _APPLEGENERICPCATAROOT_H

#include <IOKit/IOService.h>

/*
 * I/O port addresses for primary and secondary channels.
 */
#define PRI_CMD_ADDR    0x1f0
#define PRI_CTR_ADDR    0x3f4
#define SEC_CMD_ADDR    0x170
#define SEC_CTR_ADDR    0x374

/*
 * IRQ assigned to primary and secondary channels.
 */
#define PRI_ISA_IRQ     14
#define SEC_ISA_IRQ     15

/*
 * Two ATA channels max.
 */
#define PRI_CHANNEL_ID  0
#define SEC_CHANNEL_ID  1

/*
 * AppleGenericPCATARoot
 */

class AppleGenericPCATARoot : public IOService
{
    OSDeclareDefaultStructors( AppleGenericPCATARoot )

protected:
    OSSet *     fChannels;
    OSSet *     fOpenChannels;
    IOService * fProvider;

    virtual OSSet * createATAChannels( void );

    virtual IORegistryEntry * getDTChannelEntry( int channelID );

    virtual OSDictionary * createNativeModeChannelInfo( UInt32 ataChannel );

    virtual OSDictionary * createLegacyModeChannelInfo( UInt32 ataChannel );

    virtual OSDictionary * createChannelInfo( UInt32 ataChannel,
                                              UInt16 commandPort,
                                              UInt16 controlPort,
                                              UInt8  interruptVector );

public:
    virtual bool start( IOService * provider );

    virtual void free();

    virtual bool handleOpen( IOService *  client,
                             IOOptionBits options,
                             void *       arg );
    
    virtual void handleClose( IOService *  client,
                              IOOptionBits options );

    virtual bool handleIsOpen( const IOService * client ) const;
};

/*
 * AppleGenericPCATAPCIRoot
 */

class AppleGenericPCATAPCIRoot : public AppleGenericPCATARoot
{
    OSDeclareDefaultStructors( AppleGenericPCATAPCIRoot )

public:
    virtual IOService * probe( IOService * provider,
                               SInt32 *    score );
};

#endif /* !_APPLEGENERICPCATAROOT_H */
