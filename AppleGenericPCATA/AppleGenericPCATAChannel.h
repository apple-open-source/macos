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

#ifndef _APPLEGENERICPCATACHANNEL_H
#define _APPLEGENERICPCATACHANNEL_H

#include <IOKit/IOService.h>

class AppleGenericPCATAChannel : public IOService
{
    OSDeclareDefaultStructors( AppleGenericPCATAChannel )

protected:
    IOService *    fProvider;

    virtual bool   mergeProperties( OSDictionary * properties );

    virtual bool   getNumberValue( const char * propKey,
                                   void       * outValue,
                                   UInt32       outBits ) const;

public:
    virtual bool   init( IOService *       provider,
                         OSDictionary *    properties,
                         IORegistryEntry * dtEntry );

    virtual UInt16 getCommandBlockAddress( void ) const;

    virtual UInt16 getControlBlockAddress( void ) const;

    virtual UInt32 getInterruptVector( void ) const;

    virtual UInt32 getChannelNumber( void ) const;

    virtual UInt32 getPIOMode( void ) const;

    virtual bool   handleOpen( IOService *  client,
                               IOOptionBits options,
                               void *       arg );

    virtual void   handleClose( IOService *  client,
                                IOOptionBits options );
};

#endif /* !_APPLEGENERICPCATACHANNEL_H */
