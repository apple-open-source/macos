/*
* Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef APPLECUDAUSERCLIENT_H
#define APPLECUDAUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>

#include "AppleCuda.h"

class AppleCudaUserClient:public IOUserClient
{
    OSDeclareDefaultStructors(AppleCudaUserClient)

private:
    AppleCuda 	* theInterface;
    task_t		fTask;

public:
    static  AppleCudaUserClient *withTask(task_t owningTask);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
    IOReturn connectClient(IOUserClient *client);
    
    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type);
    virtual bool start(IOService *provider);

    IOReturn setProperties( OSObject * properties );
};

#endif /* APPLECUDAUSERCLIENT_H */
