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
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>

#include <IOKit/cdb/IOCDBInterface.h>
#include <IOKit/scsi/scsi-device/SCSICommand.h>
#include <IOKit/scsi/scsi-device/SCSIDevice.h>
#include <IOKit/scsi/scsi-device/IOSCSICommand.h>
#include <IOKit/scsi/scsi-device/IOSCSIDevice.h>

#include "IOCDBUserClient.h"

#define super IOUserClient
OSDefineMetaClassAndStructors(IOCDBUserClient, IOUserClient);

const IOExternalMethod IOCDBUserClient::
sMethods[kIOCDBUserClientNumCommands] = {
    { //    kIOCDBUserClientGetInquiryData
	0,
	(IOMethod) &IOCDBUserClient::getInquiryData,
	kIOUCScalarIStructO,
	1,
	0xffffffff
    },
    { //    kIOCDBUserClientOpen
	0,
	(IOMethod) &IOCDBUserClient::open,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kIOCDBUserClientClose
	0,
	(IOMethod) &IOCDBUserClient::close,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kIOCDBUserClientAbort
	0,
	(IOMethod) &IOCDBUserClient::abort,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kIOCDBUserClientReset
	0,
	(IOMethod) &IOCDBUserClient::reset,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kIOCDBUserClientCommandAlloc
	0,
	(IOMethod) &IOCDBUserClient::userCommandAlloc,
	kIOUCScalarIScalarO,
	3,
	1
    },
    { //    kIOCDBUserClientCommandExecute
	0,
	(IOMethod) &IOCDBUserClient::userCommandExecute,
	kIOUCStructIStructO,
	0xffffffff,
	0xffffffff
    },
    { //    kIOCDBUserClientCommandAbort
	0,
	(IOMethod) &IOCDBUserClient::userCommandAbort,
	kIOUCScalarIScalarO,
	2,
	0
    },
    { //    kIOCDBUserClientCommandFree
	0,
	(IOMethod) &IOCDBUserClient::userCommandFree,
	kIOUCScalarIScalarO,
	1,
	0
    }
};

const IOExternalAsyncMethod IOCDBUserClient::
sAsyncMethods[kIOCDBUserClientNumAsyncCommands] = {
    { //    kIOCDBUserClientSetAsyncPort
	0,
	(IOAsyncMethod) &IOCDBUserClient::setAsyncPort,
	kIOUCScalarIScalarO,
	0,
	0
    }
};

void IOCDBUserClient::setExternalMethodVectors()
{
    fMethods = sMethods;
    fNumMethods = kIOCDBUserClientNumCommands;
    fAsyncMethods = sAsyncMethods;
    fNumAsyncMethods = kIOCDBUserClientNumAsyncCommands;
}

bool IOCDBUserClient::
initWithTask(task_t owningTask, void * /* security_id */, UInt32 /* type */)
{
    if (!super::init())
	return false;

    fClient = owningTask;
    task_reference(fClient);
    setExternalMethodVectors();

    return true;
}

IOReturn IOCDBUserClient::clientClose(void)
{
    if (fGate)	// Use as an is open flag
        close();

    if (fNub) {	// Have been started so we better detach
        detach(fNub);
        fNub = 0;
    }

    if (fClient) {
        task_deallocate(fClient);
        fClient = 0;
    }

    return kIOReturnSuccess;
}

bool IOCDBUserClient::start(IOService *provider)
{
    if (!super::start(provider))
	return false;

    fNub = OSDynamicCast(IOSCSIDevice, provider);
    if (!fNub)
	return false;

    return true;
}

IOExternalMethod *IOCDBUserClient::
getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if (index < (UInt32) fNumMethods) {
	*target = this;
	return (IOExternalMethod *) &fMethods[index];
    }
    else
	return 0;
}

IOExternalAsyncMethod * IOCDBUserClient::
getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if (index < (UInt32) fNumAsyncMethods) {
	*target = this;
	return (IOExternalAsyncMethod *) &fAsyncMethods[index];
    }
    else
	return 0;
}


IOReturn IOCDBUserClient::
getInquiryData(void *vinSize, void *vBuf, void *voutSize,
	       void *, void *, void *)
{
    UInt32  bufSize  = (UInt32) vinSize;
    UInt32 *outSizeP = (UInt32 *) voutSize;

    // @@@ gvdl: Ask simon about maximum buffer size;
    if (!bufSize) {
	fNub->getInquiryData(0, 0, (UInt32 *) vBuf);
	*outSizeP = sizeof(UInt32);
    }
    else if (bufSize <= 4096)
	fNub->getInquiryData(vBuf, bufSize, outSizeP);
    else
	return kIOReturnBadArgument;

    return kIOReturnSuccess;
}

IOReturn IOCDBUserClient::
setAsyncPort(OSAsyncReference asyncRef, void *, void *, void *,
                                        void *, void *, void *)
{
    fWakePort = (mach_port_t) asyncRef[0];
    return kIOReturnSuccess;
}

IOReturn IOCDBUserClient::
open(void *, void *, void *, void *, void *, void *)
{
    IOReturn res = kIOReturnSuccess;
    IOWorkLoop *wl;

    if (!fNub->open(this))
	return kIOReturnExclusiveAccess;

    wl = getWorkLoop();
    if (!wl) {
        res = kIOReturnNoResources;
        goto abortOpen;
    }

    fGate = IOCommandGate::commandGate(this);
    if (!fGate) {
        res = kIOReturnNoMemory;
        goto abortOpen;
    }
    wl->retain();
    wl->addEventSource(fGate);

    return kIOReturnSuccess;

abortOpen:
    if (fGate) {
        wl->removeEventSource(fGate);
        wl->release();
        fGate->release();
        fGate = 0;
    }
    fNub->close(this);
        
    return res;
}

IOReturn IOCDBUserClient::
close(void *, void *, void *, void *, void *, void *gated)
{
    if ( ! (bool) gated ) {
        IOReturn res;
        IOWorkLoop *wl;
		
        res = fGate->runAction(closeAction);
		
        wl = fGate->getWorkLoop();
        wl->removeEventSource(fGate);
        wl->release();

        fGate->release();
        fGate = 0;
        return res;
    }
    else /* gated */ {
        for (int i = 0; i < kMaxCommands; i++) {
            IOSCSICommand *cmd = fCommandPool[i];
            if (cmd) {
                cmd->release();
                fCommandPool[i] = 0;
            }
        }
    
        fNub->close(this);
    
        // @@@ gvdl: release fWakePort leak them for the time being
    
        return kIOReturnSuccess;
    }
}

IOReturn IOCDBUserClient::
abort(void *, void *, void *, void *, void *, void *)
{
    fNub->abort();
    return kIOReturnSuccess;
}

IOReturn IOCDBUserClient::
reset(void *, void *, void *, void *, void *, void *)
{
    fNub->reset();
    return kIOReturnSuccess;
}

IOSCSICommand * IOCDBUserClient::
newCommand()
{
    return fNub->allocCommand(fNub, sizeof(commandData));
}

IOSCSICommand * IOCDBUserClient::
allocCommand(void *vResSenseData, void *vTarget, void *vCallback)
{
    IOSCSICommand *cmd = newCommand();

    if (cmd && !initCommand(cmd, vResSenseData, vTarget, vCallback)) {
	freeCommand(cmd);
	cmd = 0;
    }

    return cmd;
}

bool IOCDBUserClient::
initCommand(IOSCSICommand *cmd,
            void *vResSenseData, void *vTarget, void *vCallback)
{
    commandData *cmdData = (commandData *) cmd->getClientData();

    cmdData->fCDBUserClient = 0;
    IOUserClient::setAsyncReference(cmdData->fAsyncRef,
                                    fWakePort, vCallback, vTarget);

    // Create the syncher and clear the default wait condition
    cmdData->fSyncher = IOSyncer::create(/* twoRetains */ false);
    if (!cmdData->fSyncher)
        return false;

    cmdData->fResultSenseMem = IOMemoryDescriptor::withAddress(
                                    (vm_address_t) vResSenseData,
                                    sizeof(IOCDBResultSense),
                                    kIODirectionIn,
                                    fClient);
    if (!cmdData->fResultSenseMem)
        return false;
    task_reference(fClient);	// Hold a reference on our clients VM

    cmd->setCallback(cmd, &IOCDBUserClient::commandComplete, cmdData);
    cmd->setPointers(cmdData->fResultSenseMem, sizeof(SCSISenseData),
                     /* isWrite */ false, /* isSense */ true);
    return true;
}

void IOCDBUserClient::freeCommand(IOSCSICommand *cmd)
{
    commandData *cmdData = (commandData *) cmd->getClientData();

    if (cmdData->fResultSenseMem) {
	cmdData->fResultSenseMem->release();
	cmdData->fResultSenseMem = 0;
        task_deallocate(fClient);	// Release reference on our clients VM
    }

    if (cmdData->fSyncher) {
	cmdData->fSyncher->release();
	cmdData->fSyncher = 0;
    }

    cmd->release();
}

IOSCSICommand *IOCDBUserClient::getCommand(void *vCmd)
{
    int mySlot = (int) vCmd;

    if (mySlot >= 0 && mySlot < kMaxCommands)
	return fCommandPool[mySlot];
    else
	return 0;
}

IOReturn IOCDBUserClient::
userCommandAlloc(void *vResSenseData, void *vTarget, void *vCallback,
                 void *voutCmd, void *, void *)
{
    IOSCSICommand *cmd = 0;
    int mySlot;

    for (mySlot = 0; mySlot < kMaxCommands; mySlot++) {
	if (!fCommandPool[mySlot]) {
	    fCommandPool[mySlot] = (IOSCSICommand *) -1;
	    break;
	}
    }
    if (mySlot >= kMaxCommands)
	return kIOReturnNoResources;

    cmd = allocCommand(vResSenseData, vTarget, vCallback);
    fCommandPool[mySlot] = cmd;
    if (cmd) {
	*((int *) voutCmd) = mySlot;
	return kIOReturnSuccess;
    }
    else
	return kIOReturnNoMemory;
}

#define kCDBResultOffset \
    ((unsigned long ) &((IOCDBResultSense *) 0)->fCDBResults)
void IOCDBUserClient::
commandComplete(void *vCmd, void *vCmdData)
{
    IOSCSICommand *cmd = (IOSCSICommand *) vCmd;
    commandData *cmdData = (commandData *) vCmdData;
    IOCDBUserClient *userClient;
    IOMemoryDescriptor *mem;

    cmd->getPointers(&mem, 0, 0, false);
    if (mem) {
        mem->complete();
        mem->release();
        cmd->setPointers(0, 0, false, false);
    }

    cmd->getResults(&cmdData->fResults);
    cmdData->fResultSenseMem->writeBytes(kCDBResultOffset,
                                         &cmdData->fResults,
                                         sizeof(cmdData->fResults));
    cmdData->fResultSenseMem->complete();

    cmdData->fSyncher->signal(kIOReturnSuccess, /* autoRelease */ false);
    userClient = cmdData->fCDBUserClient;
    cmdData->fCDBUserClient = 0;
    if (userClient) {
        // @@@ gvdl: temporary until I get REAL notifications.

	userClient->sendAsyncResult
            (cmdData->fAsyncRef, cmdData->fResults.returnCode, 0, 0);
    }

    cmdData->fIsActive = false;
    cmd->release();
}

#define kMaxEntries (int) 						 \
    ((kIOCDBCommandExecuteDataMaxSize - sizeof(IOCDBCommandExecuteData)) \
	/ sizeof(IOVirtualRange))

IOReturn IOCDBUserClient::
userCommandExecute(void *vIn, void *vOut, void *vInSize, void *vOutSize,
	      void *, void *)
{
    IOCDBCommandExecuteData *executeData = (IOCDBCommandExecuteData *) vIn;
    IOSCSICommand *cmd = getCommand((void *) executeData->kernelHandle);
    commandData *cmdData;
    vm_size_t *outSize = (vm_size_t *) vOutSize;
    UInt32 seqNumber;
    IODirection direction;
    IOMemoryDescriptor *mem = 0;
    IOReturn res = kIOReturnSuccess;
    SCSICDBInfo scsiCDBInfo;

    if (!cmd
    || ( executeData->isSynch && *outSize != 0)
    || (!executeData->isSynch && *outSize < sizeof(UInt32))
    ||   executeData->cdbInfo.cdbLength == 0
    ||   executeData->cdbInfo.cdbLength > sizeof(executeData->cdbInfo.cdb) )
	return kIOReturnBadArgument;

    cmd->retain();
    cmdData = (commandData *) cmd->getClientData();

    // Is the command in use?
    if ( !OSCompareAndSwap(false, true, &cmdData->fIsActive) ) {
        res = kIOReturnBusy;
        goto abortExecute;
    }

    direction = (executeData->isWrite)? kIODirectionOut : kIODirectionIn;
    if (executeData->sgEntries < kMaxEntries) {
        mem = IOMemoryDescriptor::withRanges(executeData->sgList,
                                             executeData->sgEntries,
                                             direction,
                                             fClient,
                                             /* asReference */ false);
    }
    else {
        // @@@ gvdl: Need to map the scatter/gather list into the kernel
        // then I need to wrap it in a memory descriptor.
        IOLog("IOCDBUserClient: Need to implement Large scatter/gather");
    }
    
    if (mem)
        mem->prepare();
    else {
        res = kIOReturnNoResources;
        goto abortExecute;
    }

    cmd->setPointers(mem, executeData->transferCount, executeData->isWrite);
    bzero( &scsiCDBInfo, sizeof(SCSICDBInfo) );
    scsiCDBInfo.cdbFlags  = executeData->cdbInfo.cdbFlags;
    scsiCDBInfo.cdbLength = executeData->cdbInfo.cdbLength;
    scsiCDBInfo.cdb       = executeData->cdbInfo.cdb;
    cmd->setCDB(&executeData->cdbInfo);
    cmd->setTimeout(executeData->timeoutMS);

    // Prepare the request sense and result memory for I/O
    // completeCommand completes this prepare.
    cmdData->fResultSenseMem->prepare();

    if (!executeData->isSynch)
        cmdData->fCDBUserClient = this;

    if (!cmd->execute(&seqNumber)) {
        res = kIOReturnError;
        goto abortExecute;
    }
    else
        cmdData->fActiveSequenceNumber = seqNumber;

    if (!executeData->isSynch) {
        bcopy(&seqNumber, vOut, sizeof(seqNumber));
        *outSize = sizeof(seqNumber);
    }
    else {
        res = cmdData->fSyncher->wait(/* autoRelease */ false);
        if (kIOReturnSuccess == res) {
            *outSize = 0;
            cmdData->fSyncher->reinit();	// re-arm
        }
        else
            cmd->abort(seqNumber);
    }

    return res;

abortExecute:
    if (mem)
        mem->release();
    if (cmd)
        cmd->release();
    return res;
}

IOReturn IOCDBUserClient::
userCommandAbort(void *vCmd, void *vSeq, void *, void *, void *, void *)
{
    IOSCSICommand *cmd = getCommand(vCmd);
    UInt32 sequenceNumber = (UInt32) vSeq;

    if (!cmd)
        return kIOReturnBadArgument;

    cmd->abort(sequenceNumber);
    return kIOReturnSuccess;
}

IOReturn IOCDBUserClient::
userCommandFree(void *vCmd, void *, void *, void *, void *, void *)
{
    IOSCSICommand *cmd = getCommand(vCmd);
    if (!cmd)
	return kIOReturnBadArgument;

    fCommandPool[(int) vCmd] = 0;
    freeCommand(cmd);

    return kIOReturnSuccess;
}

IOReturn IOCDBUserClient::closeAction
    (OSObject *self, void *, void *, void *, void *)
{
    IOCDBUserClient *me = (IOCDBUserClient *) self;
    return me->close(0, 0, 0, 0, 0, /* gated = */ (void *) true);
}

IOReturn IOCDBUserClient::userCommandAllocAction
    (OSObject *self, void *voutCmd, void *, void *, void *)
{
    IOCDBUserClient *me = (IOCDBUserClient *) self;
    return me->userCommandAlloc
                (voutCmd, 0, 0, 0, 0, /* gated = */ (void *) true);
}

IOReturn IOCDBUserClient::userCommandExecuteAction
    (OSObject *self, void *voutCmd, void *, void *, void *)
{
    IOCDBUserClient *me = (IOCDBUserClient *) self;
    return me->userCommandExecute
                (voutCmd, 0, 0, 0, 0, /* gated = */ (void *) true);
}

IOReturn IOCDBUserClient::userCommandAbortAction
    (OSObject *self, void *vCmd, void *, void *, void *)
{
    IOCDBUserClient *me = (IOCDBUserClient *) self;
    return me->userCommandAbort
                (vCmd, 0, 0, 0, 0, /* gated = */ (void *) true);
}

IOReturn IOCDBUserClient::userCommandFreeAction
    (OSObject *self, void *vCmd, void *, void *, void *)
{
    IOCDBUserClient *me = (IOCDBUserClient *) self;
    return me->userCommandFree
                (vCmd, 0, 0, 0, 0, /* gated = */ (void *) true);
}

