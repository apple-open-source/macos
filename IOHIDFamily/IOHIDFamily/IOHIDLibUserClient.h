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
 * Changes to this API are expected.
 */

#ifndef _IOKIT_IOHIDLibUserClient_H_
#define _IOKIT_IOHIDLibUserClient_H_

#include "IOHIDKeys.h"

// evil hack alert: we are using type in one of two ways:
// if type == 0, then we want to get the element values
// otherwise type is an object pointer (in kernel space).
// this was deemed better for now than duplicating
// the code from IOUserClient.cpp: is_io_connect_map_memory,
// mapClientMemory and is_io_connect_unmap_memory
enum IOHIDLibUserClientMemoryTypes {
    IOHIDLibUserClientElementValuesType = 0
};


enum IOHIDLibUserClientAsyncCommandCodes {
    kIOHIDLibUserClientSetAsyncPort,   		// kIOUCScalarIScalarO, 0, 0
    kIOHIDLibUserClientSetQueueAsyncPort,	// kIOUCScalarIScalarO, 1, 0
    kIOHIDLibUserClientAsyncGetReport,		// kIOUCScalarIScalarO, 5, 0
    kIOHIDLibUserClientAsyncSetReport,		// kIOUCScalarIScalarO, 5, 0

    kIOHIDLibUserClientNumAsyncCommands
};

enum IOHIDLibUserClientCommandCodes {
    kIOHIDLibUserClientOpen,			// kIOUCScalarIScalarO, 0, 0
    kIOHIDLibUserClientClose,			// kIOUCScalarIScalarO, 0, 0
    kIOHIDLibUserClientCreateQueue,		// kIOUCScalarIScalarO, 2, 1
    kIOHIDLibUserClientDisposeQueue,		// kIOUCScalarIScalarO, 1, 0
    kIOHIDLibUserClientAddElementToQueue,	// kIOUCScalarIScalarO, 3, 0
    kIOHIDLibUserClientRemoveElementFromQueue,	// kIOUCScalarIScalarO, 2, 0
    kIOHIDLibUserClientQueueHasElement, 	// kIOUCScalarIScalarO, 2, 1
    kIOHIDLibUserClientStartQueue, 		// kIOUCScalarIScalarO, 1, 0
    kIOHIDLibUserClientStopQueue, 		// kIOUCScalarIScalarO, 1, 0
    kIOHIDLibUserClientUpdateElementValue, 	// kIOUCScalarIScalarO, 1, 0
    kIOHIDLibUserClientPostElementValue,	// kIOUCStructIStructO, 0xffffffff, 0
    kIOHIDLibUserClientGetReport,		// kIOUCScalarIStructO, 2, 0xffffffff
    kIOHIDLibUserClientGetReportOOL,		// kIOUCStructIStructO, 
    kIOHIDLibUserClientSetReport,		// kIOUCScalarIScalarO, 2, 0xffffffff
    kIOHIDLibUserClientSetReportOOL,		// kIOUCStructIStructO,

    kIOHIDLibUserClientNumCommands
};

#if 0
struct IOHIDCommandExecuteData {
    HIDInfo HIDInfo;
    HIDResults *HIDResults;
	int kernelHandle;
	int sgEntries;
	UInt32 timeoutMS;
	IOVirtualRange sgList[0];
};

#define kIOHIDCommandExecuteDataMaxSize 1024

#endif

struct IOHIDElementValue
{
    IOHIDElementCookie cookie;
    UInt32             totalSize;
    AbsoluteTime       timestamp;
    UInt32             generation;
    UInt32             value[1];
};

struct IOHIDReportReq
{
    IOHIDReportType	reportType;
    UInt32		reportID;
    void 		*reportBuffer;
    UInt32		reportBufferSize;
};

#if KERNEL

#include <mach/mach_types.h>
#include <IOKit/IOUserClient.h>

class IOHIDDevice;
class IOSyncer;
#if 0
class IOCommandGate;

struct HIDResults;
#endif

class IOHIDLibUserClient : public IOUserClient 
{
    OSDeclareDefaultStructors(IOHIDLibUserClient)

protected:
    static const IOExternalMethod
		sMethods[kIOHIDLibUserClientNumCommands];
    static const IOExternalAsyncMethod
		sAsyncMethods[kIOHIDLibUserClientNumAsyncCommands];

    IOHIDDevice *fNub;
    IOCommandGate *fGate;

    task_t fClient;
    mach_port_t fWakePort;
    mach_port_t fQueuePort;

    bool fNubIsTerminated;

    // Methods
    virtual bool
	initWithTask(task_t owningTask, void *security_id, UInt32 type);
    virtual IOReturn clientClose(void);

    virtual bool start(IOService *provider);

    virtual IOExternalMethod *
	getTargetAndMethodForIndex(IOService **target, UInt32 index);

    virtual IOExternalAsyncMethod *
	getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index);

    virtual IOReturn setAsyncPort(OSAsyncReference asyncRef,
                                  void *, void *, void *,
                                  void *, void *, void *);
                                  
    virtual IOReturn setQueueAsyncPort(OSAsyncReference asyncRef,
                                  void *vInQueue, void *, void *,
                                  void *, void *, void *);

    // Open the IOHIDDevice
    virtual IOReturn open(void *, void *, void *,
			  void *, void *, void *);
    
    // Close the IOHIDDevice
    virtual IOReturn close(void * = 0, void * = 0, void * = 0,
			   void * = 0, void * = 0, void *gated = 0);
    
    virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer);
    
    virtual bool requestTerminate( IOService * provider, IOOptionBits options );

    virtual void free();

    // return the shared memory for type (called indirectly)
    virtual IOReturn clientMemoryForType(
                           UInt32                type,
                           IOOptionBits *        options,
                           IOMemoryDescriptor ** memory );

    // Create a queue
    virtual IOReturn createQueue(void * vInFlags, void * vInDepth, void * vOutQueue,
			   void *, void *, void * gated);

    // Dispose a queue
    virtual IOReturn disposeQueue(void * vInQueue, void *, void *,
			   void *, void *, void * gated);

    // Add an element to a queue
    virtual IOReturn addElementToQueue(void * vInQueue, void * vInElementCookie, 
                            void * vInFlags, void *, void *, void * gated);
   
    // remove an element from a queue
    virtual IOReturn removeElementFromQueue (void * vInQueue, void * vInElementCookie, 
                            void *, void *, void *, void * gated);
    
    // Check to see if a queue has an element
    virtual IOReturn queueHasElement (void * vInQueue, void * vInElementCookie, 
                            void * vOutHasElement, void *, void *, void * gated);
    
    // start a queue
    virtual IOReturn startQueue (void * vInQueue, void *, void *, 
                            void *, void *, void * gated);
    
    // stop a queue
    virtual IOReturn stopQueue (void * vInQueue, void *, void *, 
                            void *, void *, void * gated);
                            
    // Update Feature element value
    virtual IOReturn updateElementValue (void *cookie, void *, void *,
                                                void *, void *, void *);
                                                
    // Post element value
    virtual IOReturn postElementValue (void *cookie, void *, void *,
                                                void *, void *, void *);
                                                
    virtual IOReturn getReport (IOHIDReportType reportType, 
                                UInt32 reportID, 
                                void *reportBuffer, 
                                UInt32 *reportBufferSize);
                                
    virtual IOReturn getReportOOL(  IOHIDReportReq *reqIn, 
                                    UInt32 *sizeOut, 
                                    IOByteCount inCount, 
                                    IOByteCount *outCount);

    virtual IOReturn setReport (IOHIDReportType reportType, 
                                UInt32 reportID, 
                                void *reportBuffer, 
                                UInt32 reportBufferSize);
                                
    virtual IOReturn setReportOOL (IOHIDReportReq *req, IOByteCount inCount);


    virtual IOReturn asyncGetReport (OSAsyncReference asyncRef, 
                                    IOHIDReportType reportType, 
                                    UInt32 reportID, 
                                    void *reportBuffer,
                                    UInt32 reportBufferSize, 
                                    UInt32 completionTimeOutMS);
                                
    virtual IOReturn asyncSetReport (OSAsyncReference asyncRef, 
                                    IOHIDReportType reportType, 
                                    UInt32 reportID, 
                                    void *reportBuffer,
                                    UInt32 reportBufferSize, 
                                    UInt32 completionTimeOutMS);

protected:
    // used 'cause C++ is a pain in the backside
    static IOReturn closeAction
        (OSObject *self, void *, void *, void *, void *);

    static void ReqComplete(void *obj, void *param, IOReturn status, UInt32 remaining);
};

#endif /* KERNEL */

#endif /* ! _IOKIT_IOHIDLibUserClient_H_ */

