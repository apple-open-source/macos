/*
 *  LogClient.h
 *  LoggerKext
 *
 *  Created by prophet on Wed May 30 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
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
    void * com_apple_iokit_KLogClient::QueueMSG(void * inPtr, 
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


