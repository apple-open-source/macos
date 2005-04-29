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

#ifndef __IOAC97CONTROLLER_H
#define __IOAC97CONTROLLER_H

#include <IOKit/IOService.h>
#include "IOAC97Types.h"
#include "IOAC97AudioConfig.h"

typedef void (*IOAC97DMAEngineAction)(void * target, void * param);

class IOAC97Controller : public IOService
{
    OSDeclareAbstractStructors( IOAC97Controller )

protected:
    IOService *   fProvider;
    IOOptionBits  fCodecOpenMask;

public:
    virtual bool        start( IOService * provider );

    virtual void        free( void );

    // DMA Engine Control

    virtual IOReturn    startDMAEngine(
                                IOAC97DMAEngineID engine,
                                IOOptionBits      options = 0 ) = 0;

    virtual void        stopDMAEngine(
                                IOAC97DMAEngineID engine ) = 0;

    virtual IOByteCount getDMAEngineHardwarePointer(
                                IOAC97DMAEngineID engine ) = 0;

    // Audio Configuration

    virtual IOReturn    prepareAudioConfiguration(
                                IOAC97AudioConfig * config );

    virtual IOReturn    activateAudioConfiguration(
                                IOAC97AudioConfig *   config,
                                void *                target = 0,
                                IOAC97DMAEngineAction action = 0,
                                void *                param  = 0 );

    virtual void        deactivateAudioConfiguration(
                                IOAC97AudioConfig * config );

    virtual IOReturn    createAudioControls(
                                IOAC97AudioConfig * config,
                                OSArray *           controls );

    // Codec Control

    virtual IOReturn    codecRead(  IOAC97CodecID     codec,
                                    IOAC97CodecOffset offset,
                                    IOAC97CodecWord * word ) = 0;

    virtual IOReturn    codecWrite( IOAC97CodecID     codec,
                                    IOAC97CodecOffset offset,
                                    IOAC97CodecWord   word ) = 0;

    enum {
        kMessageCodecOrderAscending  = 0x0,
        kMessageCodecOrderDescending = 0x1
    };

    virtual IOReturn    messageCodecs(
                                UInt32       type,
                                void *       argument = 0,
                                IOOptionBits options  = 0 );

    // Client Open/Close

    virtual bool        handleOpen( IOService *  client,
                                    IOOptionBits options,
                                    void *       arg );

    virtual void        handleClose( IOService *  client,
                                     IOOptionBits options );

    virtual bool        handleIsOpen( const IOService * client ) const;
};

#endif /* !__IOAC97CONTROLLER_H */
