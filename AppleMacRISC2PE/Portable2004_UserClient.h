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
/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 */

#ifndef _PORTABLE2004CLIENT_H
#define _PORTABLE2004CLIENT_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include "UserClient.h"

class Portable2004Client : public IOUserClient
{
    OSDeclareDefaultStructors(Portable2004Client)
    
protected:

    IOService *		fProvider;
    task_t			fTask;
    bool			fDead;
      
public:
       
    // IOUserClient methods
    virtual IOReturn  open(void);
    virtual IOReturn  close(void);
    
    virtual void stop(IOService * provider);
    virtual bool start(IOService * provider);
    
    virtual bool initWithTask(task_t owningTask, void * security_id, UInt32 type);

    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService ** target, UInt32 index);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
};

#endif