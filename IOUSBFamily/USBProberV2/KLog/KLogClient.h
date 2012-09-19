/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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


#ifndef KLOGCLIENT_H
#define KLOGCLIENT_H

#include "KLog.h"
#include <IOKit/IODataQueue.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>

//================================================================================================
//   Formware Declarations
//================================================================================================

class com_apple_iokit_KLog;

//================================================================================================
//   com_apple_iokit_KLogClient
//================================================================================================

class com_apple_iokit_KLogClient : public IOUserClient
{

    OSDeclareDefaultStructors(com_apple_iokit_KLogClient)

    com_apple_iokit_KLog *	myProvider;
    task_t					fTask;
    char *					ptr;
    int						State;
    bool					ActiveFlag;
    IOLock *				ClientLock;
    IODataQueue *			myLogQueue;
    mach_port_t				myPort;
    int						Q_Err;
    
	
public:

    static const IOExternalMethod sMethods[];
    static const IOItemCount sMethodCount;

    virtual bool init();
    virtual void free();

    void AddEntry(void *entry, UInt32 sizeOfentry);

    static  com_apple_iokit_KLogClient *withTask(task_t owningTask);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
  
    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService **target, UInt32 index);
    void * QueueMSG(void * inPtr, 
												void * outPtr, 
												IOByteCount inSize, 
												IOByteCount *outSize, 
												void * inUnused1, 
												void * inUnused2 );
     
    // perform a registration
    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon);
    
    virtual IOReturn clientMemoryForType(	UInt32 type,
											IOOptionBits * options,
											IOMemoryDescriptor ** memory );

    virtual bool start(IOService *provider); 
    
    bool set_Q_Size(UInt32 capacity);
};

#endif


