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
/*
 *  IOI2CInterface.h
 */

#ifndef _IOKIT_IOI2CINTERFACE_H
#define _IOKIT_IOI2CINTERFACE_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef struct IOI2CRequest IOI2CRequest;
typedef struct IOI2CBuffer IOI2CBuffer;

typedef void (*IOI2CRequestCompletion) (IOI2CRequest * request);

// IOI2CRequest.sendTransactionType, IOI2CRequest.replyTransactionType
enum {
    kIOI2CNoTransactionType         = 0,		/* No transaction */
    kIOI2CSimpleTransactionType     = 1,		/* Simple I2C message */
    kIOI2CDDCciReplyTransactionType = 2,		/* DDC/ci message (with embedded length) */
    kIOI2CCombinedTransactionType   = 3			/* Combined format I2C R/~W transaction */
};

// IOI2CRequest.commFlags
enum {
    kIOI2CUseSubAddressCommFlag	    = 0x00000002	/* Transaction includes subaddress */
};

struct IOI2CRequest
{
    UInt64			i2cBusID;

    IOReturn			result;
    IOI2CRequestCompletion	completion;

    IOOptionBits		commFlags;
    /* Minimum delay as absolute time between send and reply transactions */
    uint64_t			minReplyDelay;

    /* I2C address to write */
    UInt8			sendAddress;
    UInt8			sendSubAddress;
    UInt8			__reservedA[2];

    /* See kIOI2CSimpleTransactionType etc.*/
    IOOptionBits		sendTransactionType;
    /* Pointer to the send buffer*/
    vm_address_t		sendBuffer;
    /* Number of bytes to send*/
    IOByteCount         	sendBytes;

    /* I2C Address from which to read */
    UInt8			replyAddress;
    UInt8			replySubAddress;
    UInt8			__reservedB[2];

    /* See kIOI2CDDCciReplyType etc.*/
    IOOptionBits		replyTransactionType;
    /* Pointer to the reply buffer*/
    vm_address_t		replyBuffer;
    /* Max bytes to reply (size of replyBuffer)*/
    IOByteCount			replyBytes;

    UInt32			__reservedC[16];
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define kIOI2CInterfaceClassName	"IOI2CInterface"

#define kIOI2CInterfaceIDKey		"IOI2CInterfaceID"
#define kIOI2CBusTypeKey		"IOI2CBusType"
#define kIOI2CTransactionTypesKey	"IOI2CTransactionTypes"
#define kIOI2CSupportedCommFlagsKey	"IOI2CSupportedCommFlags"

#define kIOFBI2CInterfaceIDsKey		"IOFBI2CInterfaceIDs"

// kIOI2CBusTypeKey values
enum {
    kIOI2CBusTypeI2C		= 1
};

#ifndef KERNEL

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// options for IOFBCopyI2CInterfaceForBus()
enum {
    kIOI2CBusNumberMask 		= 0x000000ff
};

IOReturn IOFBGetI2CInterfaceCount( io_service_t framebuffer, IOItemCount * count );
IOReturn IOFBCopyI2CInterfaceForBus( io_service_t framebuffer, IOOptionBits bus, io_service_t * interface );

typedef struct IOI2CConnect * IOI2CConnectRef;

IOReturn IOI2CCopyInterfaceForID( CFTypeRef identifier, io_service_t * interface );
IOReturn IOI2CInterfaceOpen( io_service_t interface, IOOptionBits options,
                             IOI2CConnectRef * connect );
IOReturn IOI2CInterfaceClose( IOI2CConnectRef connect, IOOptionBits options );
IOReturn IOI2CSendRequest( IOI2CConnectRef connect, IOOptionBits options, 
                           IOI2CRequest * request );

#else

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOI2CInterface : public IOService
{
    OSDeclareDefaultStructors(IOI2CInterface)
    
protected:
    UInt64	fID;

public:
    IOReturn newUserClient( task_t		owningTask,
                            void * 		security_id,
                            UInt32  		type,
                            IOUserClient **	handler );

    bool registerI2C( UInt64 id );

    virtual IOReturn startIO( IOI2CRequest * request ) = 0;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#endif /* KERNEL */

#endif /* ! _IOKIT_IOI2CINTERFACE_H */

