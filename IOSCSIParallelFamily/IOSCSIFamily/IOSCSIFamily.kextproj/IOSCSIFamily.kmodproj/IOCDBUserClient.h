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

/*
 * Changes to this API are expected.
 */

#ifndef _IOKIT_IOCDBUSERCLIENT_H_
#define _IOKIT_IOCDBUSERCLIENT_H_

#include <IOKit/scsi/scsi-device/SCSIPublic.h>
#include <IOKit/scsi/scsi-device/SCSICommand.h>

enum IOCDBUserClientAsyncCommandCodes {
    kIOCDBUserClientSetAsyncPort,   // kIOUCScalarIScalarO,  0,	 0
    kIOCDBUserClientNumAsyncCommands
};

enum IOCDBUserClientCommandCodes {
    kIOCDBUserClientGetInquiryData, // kIOUCScalarIStructO,  1, -1
    kIOCDBUserClientOpen,	    // kIOUCScalarIScalarO,  0,	 0
    kIOCDBUserClientClose,	    // kIOUCScalarIScalarO,  0,	 0
    kIOCDBUserClientAbort,	    // kIOUCScalarIScalarO,  0,	 0
    kIOCDBUserClientReset,	    // kIOUCScalarIScalarO,  0,	 0
    kIOCDBUserClientCommandAlloc,   // kIOUCScalarIScalarO,  3,	 1
    kIOCDBUserClientCommandExecute, // kIOUCStructIStructO, -1,	 sizeof(CDBResults)
    kIOCDBUserClientCommandAbort,   // kIOUCScalarIScalarO,  2,	 0
    kIOCDBUserClientCommandFree,    // kIOUCScalarIScalarO,  1,	 0
    kIOCDBUserClientNumCommands
};

struct IOCDBResultSense {
    SCSISenseData fSenseData;
    SCSIResults fCDBResults;	// Should be CDB but I'm going to wait for SAM
};

struct IOCDBCommandExecuteData {
	CDBInfo cdbInfo;
	int kernelHandle;
	int sgEntries;
	UInt32 timeoutMS;
	UInt32 transferCount;
	bool isWrite;
	bool isSynch;
	IOVirtualRange sgList[0];
};

#define kIOCDBCommandExecuteDataMaxSize 1024

#if KERNEL

#include <mach/mach_types.h>
#include <IOKit/IOUserClient.h>

class IOSCSIDevice;
class IOSCSICommand;
class IOSyncer;
class IOCommandGate;
struct CDBResults;

class IOCDBUserClient : public IOUserClient 
{
    OSDeclareDefaultStructors(IOCDBUserClient)

public:
    enum constants { kMaxCommands = 32 };
    struct commandData {
	IOSyncer *fSyncher;
	IOCDBUserClient *fCDBUserClient;
        IOMemoryDescriptor *fResultSenseMem;
	SCSIResults fResults;
	OSAsyncReference fAsyncRef;
        UInt32 fActiveSequenceNumber;
        UInt32 fIsActive;
    };

protected:
    static const IOExternalMethod
		sMethods[kIOCDBUserClientNumCommands];
    static const IOExternalAsyncMethod
		sAsyncMethods[kIOCDBUserClientNumAsyncCommands];

    IOSCSICommand *fCommandPool[kMaxCommands];
    IOSCSIDevice *fNub;
    IOCommandGate *fGate;

    const IOExternalMethod *fMethods;
    const IOExternalAsyncMethod *fAsyncMethods;

    task_t fClient;
    mach_port_t fWakePort;
    int fNumMethods, fNumAsyncMethods;

    // Methods
    virtual bool
	initWithTask(task_t owningTask, void *security_id, UInt32 type);
    virtual IOReturn clientClose(void);

    virtual bool start(IOService *provider);

    virtual void setExternalMethodVectors();

    virtual IOExternalMethod *
	getTargetAndMethodForIndex(IOService **target, UInt32 index);

    virtual IOExternalAsyncMethod *
	getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index);

    virtual IOReturn setAsyncPort(OSAsyncReference asyncRef,
                                  void *, void *, void *,
                                  void *, void *, void *);

    // Obtain information about this device
    virtual IOReturn getInquiryData(void *vinSize, void *vBuf, void *voutSize,
				    void *, void *, void *);

    // Open the IOCDBDevice
    virtual IOReturn open(void *, void *, void *,
			  void *, void *, void *);
    
    // Close the IOCDBDevice
    virtual IOReturn close(void * = 0, void * = 0, void * = 0,
			   void * = 0, void * = 0, void *gated = 0);
    
    // Abort all outstanding commands on this device
    virtual IOReturn abort(void *, void *, void *,
			   void *, void *, void *);
    
    // Reset device (also aborts all outstanding commands)
    virtual IOReturn reset(void *, void *, void *,
			   void *, void *, void *);

    // Command verbs
    virtual IOReturn
        userCommandAlloc(void *vTarget, void *vCallback, void *voutCmd,
                         void *, void *, void *gated);
    virtual IOReturn userCommandExecute(void *vIn, void *vOut, void *vInSize,
				    void *vOutSize, void *, void *gated);
    virtual IOReturn userCommandAbort(void *vCmd, void *, void *,
				  void *, void *, void *gated);
    virtual IOReturn userCommandFree(void *vCmd, void *, void *,
				 void *, void *, void *gated);
    static void commandComplete(void *vCmd, void *vCmdData);

protected:
    // Internal use APIs to maintain the command pool
    virtual IOSCSICommand *newCommand();
    virtual IOSCSICommand *allocCommand(
                        void *vResSenseData, void *vTarget, void *vCallback);
    virtual bool initCommand(IOSCSICommand *cmd,
                        void *vResSenseData, void *vTarget, void *vCallback);
    virtual void freeCommand(IOSCSICommand *cmd);
    virtual IOSCSICommand *getCommand(void *vCmd);

    // used 'cause C++ is a pain in the backside
    static IOReturn closeAction
        (OSObject *self, void *, void *, void *, void *);
    static IOReturn userCommandAllocAction
        (OSObject *self, void *voutCmd, void *, void *, void *);
    static IOReturn userCommandExecuteAction
        (OSObject *self, void *vIn, void *vOut, void *vInSize, void *vOutSize);
    static IOReturn userCommandAbortAction
        (OSObject *self, void *vCmd, void *, void *, void *);
    static IOReturn userCommandFreeAction
        (OSObject *self, void *vCmd, void *, void *, void *);
};

#endif /* KERNEL */

#endif /* ! _IOKIT_IOCDBUSERCLIENT_H_ */

