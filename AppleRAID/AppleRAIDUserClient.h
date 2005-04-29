/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLERAIDUSERCLIENT_H
#define _APPLERAIDUSERCLIENT_H

// User Client method names    
enum
{
    kAppleRAIDClientOpen,
    kAppleRAIDClientClose,
    kAppleRAIDGetListOfSets,
    kAppleRAIDGetSetProperties,
    kAppleRAIDGetMemberProperties,
    kAppleRAIDUpdateSet,
    kAppleRAIDUserClientMethodMaxCount
};

#define kAppleRAIDMessageSetChanged  ('raid')

enum {
    kAppleRAIDMaxUUIDStringSize = 64				// 128bit UUID (in ascii hex plus some dashes) "uuidgen | wc" == 37
};

// update set sub commands
enum {
    kAppleRAIDUpdateResetSet = 1,
    kAppleRAIDUpdateDestroySet = 2
};

#ifdef KERNEL

#include <IOKit/IOUserClient.h>

class AppleRAIDUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(AppleRAIDUserClient)
    
 protected:
    IOService *				fProvider;
    task_t				fTask;
    bool				fDead;
      
public:
       
    // IOUserClient methods
    virtual IOReturn  open(void);
    virtual IOReturn  close(void);
    
    virtual void stop(IOService * provider);
    virtual bool start(IOService * provider);
    
    virtual IOReturn message(UInt32 type, IOService * provider,  void * argument = 0);
    
    virtual bool initWithTask(task_t owningTask, void * security_id, UInt32 type);
    virtual bool finalize(IOOptionBits options);

    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService ** target, UInt32 index);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
    virtual bool terminate(IOOptionBits options = 0);    
};

#endif KERNEL

#endif _APPLERAIDUSERCLIENT_H
