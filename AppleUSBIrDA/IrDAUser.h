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
#ifndef __IrDAUser__
#define __IrDAUser__

#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>

class AppleIrDASerial;

class IrDAUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IrDAUserClient)

public:
    static IrDAUserClient *withTask(task_t owningTask);     // factory create
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);

    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type);   // not impl
    virtual IOReturn  connectClient(IOUserClient *client);
    virtual IOExternalMethod *getExternalMethodForIndex(UInt32 index);
    virtual bool start(IOService *provider);

    IOReturn userPostCommand(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);
    IOReturn getIrDALog(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);
    IOReturn getIrDAStatus(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize);
    IOReturn setIrDAState(bool state);
    
private:
    AppleIrDASerial     *fDriver;
    task_t               fTask;

    IOExternalMethod   fMethods[1];     // just one method

};

#endif  // __IrDAUser__