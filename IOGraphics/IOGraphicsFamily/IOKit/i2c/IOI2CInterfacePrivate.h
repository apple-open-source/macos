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

#ifndef _IOKIT_IOI2CINTERFACEPRIVATE_H
#define _IOKIT_IOI2CINTERFACEPRIVATE_H

#ifdef KERNEL
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/ndrvsupport/IONDRVFramebuffer.h>
#endif /* KERNEL */

#include <IOKit/i2c/IOI2CInterface.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum { kIOI2CInlineBufferBytes = 1024 };

struct IOI2CBuffer
{
    IOI2CRequest        request;
    UInt8               inlineBuffer[ kIOI2CInlineBufferBytes ];
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifdef KERNEL

#pragma pack(push, 4)

struct IOI2CRequest_10_5_0
{
    UInt64              __reservedA;
    IOReturn            result;
    uint32_t            completion;
    IOOptionBits        commFlags;
    uint64_t            minReplyDelay;
    uint8_t             sendAddress;
    uint8_t             sendSubAddress;
    uint8_t             __reservedB[2];
    IOOptionBits        sendTransactionType;
    uint32_t            sendBuffer;
    uint32_t            sendBytes;
    uint8_t             replyAddress;
    uint8_t             replySubAddress;
    uint8_t             __reservedC[2];
    IOOptionBits        replyTransactionType;
    uint32_t            replyBuffer;
    uint32_t            replyBytes;
    uint32_t            __reservedD[16];
#ifdef __ppc__
    uint32_t            __reservedE;
#endif
};

#pragma pack(pop)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOI2CInterfaceUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOI2CInterfaceUserClient)

protected:
    task_t      fTask;

public:
    // IOUserClient methods
    virtual IOReturn clientClose( void );

    virtual IOService * getService( void );

    virtual IOExternalMethod * getTargetAndMethodForIndex(
                                        IOService ** targetP, UInt32 index );
    static IOI2CInterfaceUserClient * withTask( task_t owningTask );
    virtual bool start( IOService * provider );
    virtual IOReturn setProperties( OSObject * properties );

    // others

    virtual IOReturn extAcquireBus( void );
    virtual IOReturn extReleaseBus( void );
    virtual IOReturn extIO( void * inStruct, void * outStruct,
                            IOByteCount inSize, IOByteCount * outSize );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#endif /* KERNEL */

#endif /* ! _IOKIT_IOI2CINTERFACEPRIVATE_H */

