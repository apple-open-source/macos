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
#include <IOKit/cdb/IOSCSILib.h>

#include "IOCDBCommandClass.h"
#include "IOSCSIUserClient.h"

__BEGIN_DECLS
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
__END_DECLS

static CFMutableDictionaryRef deviceDict;

static inline CFMutableSetRef 
getCommandSetForDevice(IOCDBDeviceInterface **dev)
{
    CFMutableSetRef set = 0;

    if (deviceDict)
        set = (CFMutableSetRef) CFDictionaryGetValue(deviceDict, dev);

    return set;
}

IOCDBCommandClass::
IOCDBCommandClass(IOCDBCommandInterface *vtable)
: IOSCSIIUnknown(vtable),
  fCDBDevice(0),
  fSGList(0),
  fTarget(0),
  fRefcon(0),
  fAction(0),
  fConnection(0),
  fKernelHandle(-1),
  fSeqNumber(0),
  fState(kIdle)
{
    memset(&fExecuteData, '\0', sizeof(fExecuteData));
    memset(&fResultSense, '\0', sizeof(fResultSense));
}

IOCDBCommandClass::~IOCDBCommandClass()
{
    CFMutableSetRef commandSet;

    commandSet = getCommandSetForDevice(fCDBDevice);
    if (commandSet)
        CFSetRemoveValue(commandSet, this);

    if (-1 != fKernelHandle) {
	mach_msg_type_number_t len = 0;
	int args[6], i = 0;

	args[i++] = fKernelHandle;

	// kIOCDBUserClientCommandFree, kIOUCScalarIScalarO,	 1,  0
	io_connect_method_scalarI_scalarO(fConnection,
	    kIOCDBUserClientCommandFree, args, i, NULL, &len);
    }

    if (fConnection) {
        IOConnectRelease(fConnection);
        fConnection = MACH_PORT_NULL;
    }

    if (fCDBDevice) {
        (*fCDBDevice)->Release(fCDBDevice);
        fCDBDevice = 0;
    }
}

void IOCDBCommandClass::
commandReportClose(const void *value, void * /* context */)
{
    ((IOCDBCommandClass *) value)->reportClose();
}

void IOCDBCommandClass::commandDeviceClosing(IOCDBDeviceInterface **dev)
{
    CFSetRef commandSet;

    commandSet = getCommandSetForDevice(dev);
    if (commandSet) {
        // Shut all of the commands down and abort them if necessary
        CFSetApplyFunction
            (commandSet, &IOCDBCommandClass::commandReportClose, 0);

        //
        // Remove this pool from the dictionary of cdbdevice pools,
        // Note that we don't hold a seperate reference to the set
        // the dictionary will release the final reference.
        //
        CFDictionaryRemoveValue(deviceDict, dev);
    }
}

IOCDBCommandInterface ** IOCDBCommandClass::
alloc(IOCDBDeviceInterface **dev, io_connect_t conn)
{
    IOCDBCommandClass *me;

    me = new IOCDBCommandClass(&cdbCommandInterfaceV1);
    if (me) {
        if (me->init(dev, conn))
            return (IOCDBCommandInterface **) &me->iunknown.pseudoVTable;
        else {
            me->release();
            return 0;
        }
    }

    return 0;
}

bool IOCDBCommandClass::init(IOCDBDeviceInterface **dev, io_connect_t conn)
{
    kern_return_t ret;
    CFMutableSetRef commandSet;
    mach_msg_type_number_t len = 1;
    int args[6], i = 0;
    
    // Add the back references
    fCDBDevice = dev;
    (*fCDBDevice)->AddRef(fCDBDevice);
    
    ret = IOConnectAddRef(conn);
    if (KERN_SUCCESS == ret) 
        fConnection = conn;
    else
        return false;
    
    args[i++] = (int) &fResultSense;			// Sense data storage 
    args[i++] = (int) this;				// target
    args[i++] = (int) &IOCDBCommandClass::completeThis;	// action

    // kIOCDBUserClientCommandAlloc, kIOUCScalarIScalarO, 2, 1
    ret = io_connect_method_scalarI_scalarO(fConnection,
        kIOCDBUserClientCommandAlloc, args, i, &fKernelHandle, &len);
    if (kIOReturnSuccess != ret) {
        fKernelHandle = -1;
        return false;
    }
    fExecuteData.kernelHandle = fKernelHandle;
    
    commandSet = getCommandSetForDevice(fCDBDevice);
    if (!commandSet) {
        if (!deviceDict) {
            
            deviceDict = CFDictionaryCreateMutable(NULL, 0, NULL, 
                            &kCFTypeDictionaryValueCallBacks);
            if (!deviceDict)
                return false;
        }
        
        commandSet = CFSetCreateMutable(NULL, 0, NULL);
        if (!commandSet)
            return false;
        CFDictionaryAddValue(deviceDict, fCDBDevice, commandSet);
        CFRelease(commandSet);
    }
    CFSetAddValue(commandSet, this);

    return true;
}

void IOCDBCommandClass::reportClose()
{
    cmdStates oldState = fState;

    fState = kClosing;
        
    if (kExecute == oldState) {
        abort(fSeqNumber);

        if (fAction)
            (*fAction)(fTarget, kIOReturnAborted, fRefcon, &iunknown);
    }
}

HRESULT IOCDBCommandClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res;

    // Change to CDB Device interface
    if (CFEqual(uuid, kIOCDBDeviceInterfaceID)
    ||	CFEqual(uuid, kIOSCSIDeviceInterfaceID)) // Add others when possible
        res = (*fCDBDevice)->QueryInterface(fCDBDevice, iid, ppv);
    else if (CFEqual(uuid, IUnknownUUID)
         ||  CFEqual(uuid, kIOCDBCommandInterfaceID)) {
        *ppv = &iunknown;
        addRef();
        res = S_OK;
    }
    else {
        *ppv = 0;
        res = E_NOINTERFACE;
    }

    CFRelease(uuid);
    return res;
}

#define idleCheck() do {				\
    if (kClosing == fState)				\
            return kIOReturnOffline;			\
    if (fState != kIdle)				\
            return kIOReturnBusy;			\
} while (0)

IOReturn IOCDBCommandClass::setPointers(IOVirtualRange *sgList,
                                    int sgEntries,
                                    UInt32 transferCount,
                                    Boolean isWrite)
{
    idleCheck();	// Abort command if state not idle

    fSGList = sgList;
    fExecuteData.sgEntries = sgEntries;
    fExecuteData.transferCount = transferCount;
    fExecuteData.isWrite = isWrite;

    return kIOReturnSuccess;
}

void IOCDBCommandClass::getPointers(IOVirtualRange **sgListP,
                                    int *sgEntriesP,
                                    UInt32 *transferCountP,
                                    Boolean *isWriteP)
{
    if (sgListP)
        *sgListP = fSGList;
    if (sgEntriesP)
        *sgEntriesP = fExecuteData.sgEntries;
    if (transferCountP)
        *transferCountP = fExecuteData.transferCount;
    if (isWriteP)
        *isWriteP = fExecuteData.isWrite;
}

IOReturn IOCDBCommandClass::setTimeout(UInt32 timeoutMS)
{
    idleCheck();	// Abort command if state not idle

    fExecuteData.timeoutMS = timeoutMS;

    return kIOReturnSuccess;
}

void IOCDBCommandClass::getTimeout(UInt32 *timeoutMSP)
{
    if (timeoutMSP)
        *timeoutMSP = fExecuteData.timeoutMS;
}

IOReturn IOCDBCommandClass::
setCallback(void *target, IOCDBCallbackFunction callback, void *refcon)
{
    idleCheck();	// Abort command if state not idle

    fTarget = target;
    fAction = callback;
    fRefcon = refcon;

    return kIOReturnSuccess;
}

void IOCDBCommandClass::getCallback(void **targetP,
                                    IOCDBCallbackFunction *callbackP,
                                    void **refconP)
{
    if (targetP)
        *targetP = fTarget;
    if (callbackP)
        *callbackP = fAction;
    if (refconP)
        *refconP = fRefcon;
}

IOReturn IOCDBCommandClass::setCDB(CDBInfo *cdbInfo)
{
    idleCheck();	// Abort command if state not idle

    if (!cdbInfo->cdbLength || cdbInfo->cdbLength > sizeof(cdbInfo->cdb))
        return kIOReturnBadArgument;

    fExecuteData.cdbInfo = *cdbInfo;

    return kIOReturnSuccess;
}

void IOCDBCommandClass::getCDB(CDBInfo *cdbInfo)
{
    if (cdbInfo)
        *cdbInfo = fExecuteData.cdbInfo;
}

IOReturn IOCDBCommandClass::getResults(SCSIResults *outCDBResults)
{
    if (outCDBResults)
        *outCDBResults = fResultSense.fCDBResults;
    return fResultSense.fCDBResults.returnCode;
}

void IOCDBCommandClass::getSenseData(SCSISenseData *outSenseData)
{
    if (outSenseData)
        *outSenseData = fResultSense.fSenseData;
}

#define kMaxEntries (int)						 \
    ((kIOCDBCommandExecuteDataMaxSize - sizeof(IOCDBCommandExecuteData)) \
	/ sizeof(IOVirtualRange))

IOReturn IOCDBCommandClass::execute(UInt32 *sequenceNumberP)
{
    IOReturn res;
    UInt32 seqNumber;
    mach_msg_type_number_t inLen;
    mach_msg_type_number_t outLen;
    char commandBuf[kIOCDBCommandExecuteDataMaxSize];
    IOCDBCommandExecuteData *cmdbuf;
    
    idleCheck();	// Abort command if state not idle

    fState = kExecute;
    if (0 == fAction) {
        fExecuteData.isSynch = true;
        outLen = 0;
    }
    else {
        fExecuteData.isSynch = false;
        outLen = sizeof(UInt32);
    }

    cmdbuf = (IOCDBCommandExecuteData *) commandBuf;
    *cmdbuf = fExecuteData;
    if (fExecuteData.sgEntries < kMaxEntries) {
        memmove(&cmdbuf->sgList[0], fSGList,
                fExecuteData.sgEntries * sizeof(IOVirtualRange));
        inLen = ((char *) &cmdbuf->sgList[fExecuteData.sgEntries])
              - commandBuf;
    }
    else {
        cmdbuf->sgList[0].address = (IOVirtualAddress) fSGList;
        inLen = ((char *) &cmdbuf->sgList[1]) - commandBuf;
    }
    
    // kIOCDBUserClientCommandExecute, kIOUCStructIStructO, -1, sizeof(UInt32)
    res = io_connect_method_structureI_structureO(fConnection,
                kIOCDBUserClientCommandExecute,
                commandBuf, inLen, (char *) &seqNumber, &outLen);
    if (fExecuteData.isSynch) {
        fState = kIdle;
        res = fResultSense.fCDBResults.returnCode;
    } else if (kIOReturnSuccess == res) {
        fSeqNumber = seqNumber;
        if (sequenceNumberP)
            *sequenceNumberP = fSeqNumber;
    }
    
    return res;
}

void IOCDBCommandClass::abort(UInt32 sequenceNumber)
{
    mach_msg_type_number_t len = 0;
    int args[6], i = 0;

    args[i++] = fKernelHandle;
    args[i++] = (int) sequenceNumber;

    // kIOCDBUserClientCommandAbort, kIOUCScalarIScalarO, 2,  0
    (void) io_connect_method_scalarI_scalarO(fConnection,
	kIOCDBUserClientCommandAbort, args, i, NULL, &len);
}

UInt32 IOCDBCommandClass::getSequenceNumber()
{
    return fSeqNumber;
}

IOReturn IOCDBCommandClass::
setAndExecuteCommand(CDBInfo *cdbInfo,
		     UInt32 transferCount,
		     IOVirtualRange *sgList,
		     int sgEntries,
		     Boolean isWrite,
		     UInt32 timeoutMS,
		     void *target,
		     IOCDBCallbackFunction callback,
		     void *refcon,
		     UInt32 *outSequenceNumber)
{
    IOReturn res;

    setTimeout(timeoutMS);
    if (cdbInfo && (res = setCDB(cdbInfo)) != kIOReturnSuccess)
        return res;
    if (sgList && sgEntries
    && (res = setPointers(sgList, sgEntries, transferCount, isWrite)) != kIOReturnSuccess)
        return res;
    if (callback
    && (res = setCallback(target, callback, refcon)) != kIOReturnSuccess)
        return res;

    return execute(outSequenceNumber);
}

void IOCDBCommandClass::completeThis(void *target, IOReturn res)
{
    IOCDBCommandClass *me = (IOCDBCommandClass *) target;

    if (me->fAction)
        (*me->fAction)(me->fTarget, res, me->fRefcon, &me->iunknown);

    me->fState = kIdle;
}

IOCDBCommandInterface IOCDBCommandClass::cdbCommandInterfaceV1 = {
    0,
    &IOSCSIIUnknown::genericQueryInterface,
    &IOSCSIIUnknown::genericAddRef,
    &IOSCSIIUnknown::genericRelease,
    &IOCDBCommandClass::commandSetPointers,
    &IOCDBCommandClass::commandGetPointers,
    &IOCDBCommandClass::commandSetTimeout,
    &IOCDBCommandClass::commandGetTimeout,
    &IOCDBCommandClass::commandSetCallback,
    &IOCDBCommandClass::commandGetCallback,
    &IOCDBCommandClass::commandSetCDB,
    &IOCDBCommandClass::commandGetCDB,
    &IOCDBCommandClass::commandSetAndExecuteCommand,
    &IOCDBCommandClass::commandExecute,
    &IOCDBCommandClass::commandGetSequenceNumber,
    &IOCDBCommandClass::commandAbort,
    &IOCDBCommandClass::commandGetResults,
    &IOCDBCommandClass::commandSenseData
};

IOReturn IOCDBCommandClass::
commandSetPointers(void *cmd,
		   IOVirtualRange *sgList,
		   int sgEntries,
		   UInt32 transferCount,
		   Boolean isWrite)
{
    return getThis(cmd)->
        setPointers(sgList, sgEntries, transferCount, isWrite);
}

void IOCDBCommandClass::
commandGetPointers(void *cmd,
		   IOVirtualRange **sgListP,
		   int *sgEntriesP,
		   UInt32 *transferCountP,
		   Boolean *isWriteP)
{
    getThis(cmd)->
        getPointers(sgListP, sgEntriesP, transferCountP, isWriteP);
}

IOReturn IOCDBCommandClass::commandSetTimeout(void *cmd, UInt32 timeoutMS)
    { return getThis(cmd)->setTimeout(timeoutMS); }

void IOCDBCommandClass::commandGetTimeout(void *cmd, UInt32 *timeoutMSP)
    { getThis(cmd)->getTimeout(timeoutMSP); }

IOReturn IOCDBCommandClass::commandSetCallback(void *cmd,
                                           void *target,
                                           IOCDBCallbackFunction callback,
                                           void *refcon)
    { return getThis(cmd)->setCallback(target, callback, refcon); }

void IOCDBCommandClass::commandGetCallback(void *cmd,
                                           void **targetP,
                                           IOCDBCallbackFunction *callbackP,
                                           void **refconP)
    { getThis(cmd)->getCallback(targetP, callbackP, refconP); }

IOReturn IOCDBCommandClass::commandSetCDB(void *cmd, CDBInfo *cdbInfo)
    { return getThis(cmd)->setCDB(cdbInfo); }

void IOCDBCommandClass::commandGetCDB(void *cmd, CDBInfo *cdbInfo)
    { getThis(cmd)->getCDB(cdbInfo); }

IOReturn IOCDBCommandClass::
commandGetResults(void *cmd, void *cdbResults)
    { return getThis(cmd)->getResults((SCSIResults *) cdbResults); }

void IOCDBCommandClass::
commandSenseData(void *cmd, void *senseData)
    { getThis(cmd)->getSenseData((SCSISenseData *) senseData); }

IOReturn IOCDBCommandClass::
commandExecute(void *cmd, UInt32 *sequenceNumber)
    { return getThis(cmd)->execute(sequenceNumber); }

void IOCDBCommandClass::commandAbort(void *cmd, UInt32 sequenceNumber)
    { getThis(cmd)->abort(sequenceNumber); }

UInt32 IOCDBCommandClass::commandGetSequenceNumber(void *cmd)
    { return getThis(cmd)->getSequenceNumber(); }

IOReturn IOCDBCommandClass::
commandSetAndExecuteCommand(void *cmd,
			    CDBInfo *cdbInfo,
			    UInt32 transferCount,
			    IOVirtualRange *sgList,
			    int sgEntries,
			    Boolean isWrite,
			    UInt32 timeoutMS,
			    void *target,
			    IOCDBCallbackFunction callback,
			    void *refcon,
			    UInt32 *sequenceNumber)
{
    return getThis(cmd)->setAndExecuteCommand(cdbInfo,
                                              transferCount,
                                              sgList,
                                              sgEntries,
                                              isWrite,
                                              timeoutMS,
                                              target,
                                              callback,
                                              refcon,
                                              sequenceNumber);
}

