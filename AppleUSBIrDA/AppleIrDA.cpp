/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#include "AppleIrDA.h"
#include "IrDAUser.h"
#include "IrDAUserClient.h"

#define super IORS232SerialStreamSync

    OSDefineMetaClassAndAbstractStructors(AppleIrDASerial, IORS232SerialStreamSync);
    

#undef super
#define super IOService
    OSDefineMetaClassAndStructors( AppleIrDA, IOService );

/*static*/
AppleIrDA*
AppleIrDA::withNub(AppleIrDASerial *nub)
{
    AppleIrDA *obj;

    obj = new AppleIrDA;
    
    if (obj != NULL) {
	if (obj->init() == false) {
	    obj->release();
	    obj = NULL;
	}
    }
    if (obj != NULL) {
	obj->fNub = nub;
    }
    return (obj);
}

IOReturn
AppleIrDA::newUserClient( task_t owningTask, void*, UInt32 type, IOUserClient **handler )
{

    IOReturn ioReturn = kIOReturnSuccess;
    IrDAUserClient *client = NULL;

    ELG( 0, type, 'irda', "new user client" );
 
   // Check that this is a user client type that we support.
   // type is known only to this driver's user and kernel
   // classes. It could be used, for example, to define
   // read or write privileges. In this case, we look for
   // a private value.
    if (type != kIrDAUserClientCookie) {        // some magic cookie
	ioReturn = -1;
	ELG(0, 0, 'irda', "bad magic cookie");
    }
    else {
       // Construct a new client instance for the requesting task.
       // This is, essentially  client = new IrDAUserClient;
       //                               ... create metaclasses ...
       //                               client->setTask(owningTask)
	client = IrDAUserClient::withTask(owningTask);
	if (client == NULL) {
	    ioReturn = -2;
	    ELG(0, 0, 'irda', "Can not create user client");
	}
    }
    if (ioReturn == kIOReturnSuccess) {
       // Attach ourself to the client so that this client instance
       // can call us.
	if (client->attach(fNub) == false) {
	    ioReturn = -3;
	    ELG(0, 0, 'irda', "Can not attach user client");
	}
    }
    if (ioReturn == kIOReturnSuccess) {
       // Start the client so it can accept requests.
	if (client->start(fNub) == false) {
	    ioReturn = -4;
	    ELG(0, 0, 'irda', "Can not start user client");
	}
    }
    if (ioReturn != kIOReturnSuccess && client != NULL) {
	client->detach(this);
	client->release();
    }
    *handler = client;
    return (ioReturn);
}
