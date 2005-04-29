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

#ifndef __IOAC97CODECDEVICE_H
#define __IOAC97CODECDEVICE_H

#include <IOKit/IOService.h>
#include "IOAC97Types.h"

// Property Table Keys
#define kIOAC97CodecPNPVendorIDKey  "PNPVendorID"

class IOAC97Controller;

class IOAC97CodecDevice : public IOService
{
    OSDeclareDefaultStructors( IOAC97CodecDevice )

protected:
    IOAC97Controller * fController;
    IOAC97CodecID      fCodecID;
    void *             fCodecParam;

public:
    static IOAC97CodecDevice * codec( IOAC97Controller * controller,
                                      IOAC97CodecID      codecID,
                                      void *             codecParam = 0);

    virtual bool               init( IOAC97Controller * controller,
                                     IOAC97CodecID      codecID,
                                     void *             codecParam );

    virtual bool               handleOpen( IOService *  client,
                                           IOOptionBits options,
                                           void *       arg );

    virtual void               handleClose( IOService *  client,
                                            IOOptionBits options );

    virtual IOReturn           codecRead(  IOAC97CodecOffset offset,
                                           IOAC97CodecWord * word );

    virtual IOReturn           codecWrite( IOAC97CodecOffset offset,
                                           IOAC97CodecWord   word );

    virtual IOAC97CodecID      getCodecID( void ) const;

    virtual void *             getCodecParameter( void ) const;

    virtual IOAC97Controller * getController( void ) const;

    virtual IOReturn           message( UInt32 type,
                                        IOService * provider,
                                        void * argument = 0 );

    virtual bool               matchPropertyTable(
                                    OSDictionary * table,
                                    SInt32 *   score );
};

#endif /* !__IOAC97CODECDEVICE_H */
