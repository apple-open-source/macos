/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#ifndef _IOKIT_IOCDBCommandClass_H
#define _IOKIT_IOCDBCommandClass_H

#include "IOSCSIIUnknown.h"
#include "IOCDBUserClient.h"
#include <IOKit/cdb/IOSCSILib.h>

class IOCDBCommandClass : public IOSCSIIUnknown {
private:
    IOCDBCommandClass(IOCDBCommandClass &src);	// Disable copy constructor
    void operator =(IOCDBCommandClass &src);
    IOCDBCommandClass();

protected:
    enum cmdStates { 
        kIdle,
        kExecute,
        kClosing
    };

    IOCDBCommandExecuteData fExecuteData;
    IOCDBResultSense fResultSense;

    IOCDBDeviceInterface **fCDBDevice;
    IOVirtualRange *fSGList;
    void *fTarget, *fRefcon;
    IOCDBCallbackFunction fAction;

    io_connect_t fConnection;
    int fKernelHandle;
    UInt32 fSeqNumber;
    cmdStates fState;
    
    // Constructor 
    IOCDBCommandClass(IOCDBCommandInterface *vtable);
    virtual ~IOCDBCommandClass();
    
    // Internal use API's
    static void commandReportClose(const void *value, void *context);
    static void completeThis(void *target, IOReturn res);
    virtual void reportClose();
    virtual bool init(IOCDBDeviceInterface **cdbDevice,
                      io_connect_t connection);

public:
    // API's for the CDBDevice's use
    static IOCDBCommandInterface **alloc(IOCDBDeviceInterface **cdbDevice,
                                      io_connect_t connection);
    static void commandDeviceClosing(IOCDBDeviceInterface **dev);

    // Implementation of Plug In interfaces
    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn setPointers(IOVirtualRange *sgList,
			     int sgEntries,
			     UInt32 transferCount,
			     Boolean isWrite);
    virtual IOReturn setTimeout(UInt32 timeoutMS);	    
    virtual IOReturn setCallback(void *target,
                             IOCDBCallbackFunction callback,
                             void *refcon);
    virtual IOReturn setCDB(CDBInfo *cdbInfo);

    virtual void getPointers(IOVirtualRange **outSGList,
			     int *outSGEntries,
			     UInt32 *outTransferCount,
			     Boolean *outIsWrite);
    virtual void getTimeout(UInt32 *outTimeoutMS);	
    virtual void getCallback(void **outTarget,
                             IOCDBCallbackFunction *outCallback,
                             void **outRefcon);
    virtual void getCDB(CDBInfo *cdbInfo);

    virtual IOReturn setAndExecuteCommand(CDBInfo *cdbInfo,
					 UInt32 transferCount,
					 IOVirtualRange *sgList,
					 int sgEntries,
					 Boolean isWrite,
					 UInt32 timeoutMS,
					 void *target,
					 IOCDBCallbackFunction callback,
					 void *refcon,
					 UInt32 *sequenceNumber);
    virtual IOReturn execute(UInt32 *outSequenceNumber);
    virtual UInt32 getSequenceNumber();
    virtual void abort(UInt32 sequenceNumber);

    virtual IOReturn getResults(SCSIResults *outCDBResults);
    virtual void getSenseData(SCSISenseData *outSenseData);

protected:
    static IOCDBCommandInterface cdbCommandInterfaceV1;

    // Methods for routing command interface v1
    static inline IOCDBCommandClass *getThis(void *cmd)
        { return (IOCDBCommandClass *) ((InterfaceMap *) cmd)->obj; };

    static IOReturn commandSetPointers(void *cmd,
                                       IOVirtualRange *sgList,
                                       int sgEntries,
                                       UInt32 transferCount,
                                       Boolean isWrite);
    static IOReturn commandSetTimeout(void *cmd, UInt32 timeoutMS);
    static IOReturn commandSetCallback(void *cmd,
                                       void *target,
                                       IOCDBCallbackFunction callback,
                                       void *refcon);
    static IOReturn commandSetCDB(void *cmd, CDBInfo *cdbInfo);

    static void commandGetPointers(void *cmd,
				   IOVirtualRange **outSGList,
				   int *outSGEntries,
				   UInt32 *outTransferCount,
				   Boolean *outIsWrite);
    static void commandGetTimeout(void *cmd, UInt32 *outTimeoutMS);
    static void commandGetCallback(void *cmd,
                                   void **outTarget,
                                   IOCDBCallbackFunction *outCallback,
                                   void **outRefcon);
    static void commandGetCDB(void *cmd, CDBInfo *cdbInfo);

    static IOReturn commandGetResults(void *cmd, void *cdbResults);
    static void commandSenseData(void *cmd, void *senseData);
    static IOReturn commandExecute(void *cmd, UInt32 *sequenceNumber);
    static void commandAbort(void *cmd, UInt32 sequenceNumber);
    static UInt32 commandGetSequenceNumber(void *cmd);
    static IOReturn commandSetAndExecuteCommand(void *cmd,
				 CDBInfo *cdbInfo,
				 UInt32 transferCount,
				 IOVirtualRange *sgList,
				 int sgEntries,
				 Boolean isWrite,
				 UInt32 timeoutMS,
				 void *target,
				 IOCDBCallbackFunction callback,
				 void *refcon,
				 UInt32 *sequenceNumber);

};

#endif /* !_IOKIT_IOCDBCommandClass_H */
