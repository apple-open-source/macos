/*
 *
 *  Created by jguyton on Sun Apr 01 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */

#include "IrDAUser.h"
#include "AppleIrDA.h"
#include "IrDAComm.h"
#include "IrDALog.h"
#include "IrDADebugging.h"

#define super   IOUserClient
#undef ELG
#define ELG(x,y,z,msg) ((void)0)

OSDefineMetaClassAndStructors(IrDAUserClient, IOUserClient)

/*static*/
IrDAUserClient*
IrDAUserClient::withTask(task_t owningTask)
{
    IrDAUserClient *client;
    ELG(0, owningTask, 'irda', "IrDAUser: withTask");

    client = new IrDAUserClient;
    
    if (client != NULL) {
	if (client->init() == false) {
	    client->release();
	    client = NULL;
	}
    }
    if (client != NULL) {
	client->fTask = owningTask;
    }
    return (client);
}

bool
IrDAUserClient::start(IOService *provider)
{
    bool result = false;
    ELG(0, 0, 'irda', "IrDAUser: start");

    fDriver = OSDynamicCast(AppleIrDASerial, provider);

    if (fDriver != NULL)
	result = super::start(provider);
    else
	result = false;

    if (result == false) {
	IOLog("IrDAUserClient: provider start failed\n");
    }
    else {
	// Initialize the call structure. The method with index
	// kSerialDoOneTrial calls the doOneTrial method
	// with two parameters, a scalar and a buffer pointer
	// that doOneTrial will write to. A pointer to this
	// method structure is returned to the kernel when the
	// user executes io_connect_method_scalarI_structureO.
	// Thie kernel uses it to dispatch the command to the
	// driver (running in kernel space)

	fMethods[0].object = this;
	fMethods[0].func   = (IOMethod) &IrDAUserClient::userPostCommand;
	fMethods[0].count0 = 0xFFFFFFFF;                /* One input as big as I need */
	fMethods[0].count1 = 0xFFFFFFFF;                /* One output as big as I need */
	fMethods[0].flags  = kIOUCStructIStructO;
    }
    return (result);
}
IOReturn
IrDAUserClient::clientClose(void)
{
    ELG(0, 0, 'irda', "IrDAUser: client close");
    detach(fDriver);
    return (kIOReturnSuccess);
}

IOReturn
IrDAUserClient::clientDied(void)
{
    ELG(0, 0, 'irda', "IrDAUser: client died");
   return (clientClose());
}

IOReturn
IrDAUserClient::connectClient(IOUserClient *client)
{
    ELG(0, 0, 'irda', "IrDAUser: connect client");
    return (kIOReturnSuccess);
}

IOReturn
IrDAUserClient::registerNotificationPort(mach_port_t port, UInt32 type)
{
    ELG(0, 0, 'irda', "IrDAUser: register notification ignored");
    return (kIOReturnUnsupported);
}

IOExternalMethod *
IrDAUserClient::getExternalMethodForIndex(UInt32 index)
{
    IOExternalMethod *result    = NULL;
    ELG(0, index, 'irda', "IrDAUser: get external method");

    if (index == 0) {
	result = &fMethods[0];
    }
    return (result);
}

IOReturn
IrDAUserClient::userPostCommand(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize)
{
    // check first byte of input data for a command code
    if (pIn && pOut && inputSize > 0) {
	unsigned char *input = (unsigned char *)pIn;
	switch(*input) {
	    case kIrDAUserCmd_GetLog:
		    return getIrDALog(pIn, pOut, inputSize, outPutSize);
	    
	    case kIrDAUserCmd_GetStatus:
		    return getIrDAStatus(pIn, pOut, inputSize, outPutSize);
	    
	    case kIrDAUserCmd_Enable:
		    return setIrDAState(true);
	    
	    case kIrDAUserCmd_Disable:
		    return setIrDAState(false);
	    
	    default:
		    IOLog("IrDA: Bad command to userPostCommand, %d\n", *input);
	}
    }
    else IOLog("IrDA: pin/pout,size error\n");
    
    return kIOReturnBadArgument;
}

// get irda log
//
// input is 9 bytes:
//      command code (kIrDAUserCmd_GetLog)
//      four bytes of buffer address
//      four bytes of buffer size
//
// output set to IrDALogInfo record
//      and buffer filled with log data

IOReturn
IrDAUserClient::getIrDALog(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize)
{
#if (hasTracing > 0)

    IOMemoryDescriptor *md;         // make a memory descriptor for the client's big buffer
    unsigned char *input = (unsigned char *)pIn;
    mach_vm_address_t bigaddr;
    IOByteCount   biglen;
    IrDALogInfo *info;

    require(inputSize == 9, Fail);
    require(outPutSize, Fail);
    require(*outPutSize == sizeof(IrDALogInfo), Fail);
	
    //bigaddr = input[1] << 24 | input[2] << 16 | input[3] << 8 | input[4];
    //biglen  = input[5] << 24 | input[6] << 16 | input[7] << 8 | input[8];
    bcopy(&input[1], &bigaddr, sizeof(bigaddr));
    bcopy(&input[5], &biglen, sizeof(biglen));
    
    //IOLog("biglen is %d\n", biglen);
    
    // create and init the memory descriptor
    //md = IOMemoryDescriptor::withAddress(bigaddr, biglen, kIODirectionOutIn, fTask);        // REVIEW direction
    //use withAddressRange() and prepare() instead
    md = IOMemoryDescriptor::withAddressRange(bigaddr, biglen, kIODirectionOutIn, fTask);        // REVIEW direction
    md->prepare(kIODirectionOutIn);

    require(md, Fail);
    
    info = IrDALogGetInfo();        // get the info block
		    
    //ELG(info->hdr,       info->hdrSize,       'irda', "info hdr");
    //ELG(info->eventLog,  info->eventLogSize,  'irda', "info events");
    //ELG(info->msgBuffer, info->msgBufferSize, 'irda', "info msg buf");
		    
    bcopy(info, pOut, sizeof(*info));       // copy the info record back to the client
    *outPutSize = sizeof(*info);            // set the output size (nop, it already is)
    
    // copy the buffer over now if there is room
    if (biglen >= info->hdrSize + info->eventLogSize + info->msgBufferSize) {
	IOByteCount ct;
	IOReturn rc;
	
	rc = md->prepare(kIODirectionNone);
	if (rc)  {ELG(-1, rc, 'irda', "prepare failed"); }
	
	ct = md->writeBytes(0,                              info->hdr,       info->hdrSize);
	if (ct != info->hdrSize) ELG(-1, rc, 'irda', "write of hdr failed");
	
	ct = md->writeBytes(info->hdrSize,                   info->eventLog,  info->eventLogSize);
	if (ct != info->eventLogSize) ELG(-1, rc, 'irda', "write of events failed");
	
	ct = md->writeBytes(info->hdrSize+info->eventLogSize, info->msgBuffer, info->msgBufferSize);
	if (ct != info->msgBufferSize) ELG(-1, rc, 'irda', "write of msgs failed");
	
	ELG(0, info->hdrSize+info->eventLogSize, 'irda', "wrote msgs at offset");
	
	rc = md->complete(kIODirectionNone);
	if (!rc) { ELG(0, 0, 'irda', "complete worked"); }
	else    { ELG(-1, rc, 'irda', "complete failed"); }

	// todo check return code of above before resetting the buffer
	IrDALogReset();     // reset the buffer now
    }
    md->release();  // free it

    return kIOReturnSuccess;


Fail:

#endif          // hasTracing > 0

    return kIOReturnBadArgument;
}

// get irda status
//
// input: just the command byte
// output: status buffer returned directly to pOut

IOReturn
IrDAUserClient::getIrDAStatus(void *pIn, void *pOut, IOByteCount inputSize, IOByteCount *outPutSize)
{
    IrDAComm *irda;
    
    require(*outPutSize == sizeof(IrDAStatus), Fail);
    require(fDriver, Fail);
    
	bzero(pOut, sizeof(IrDAStatus));
	
    irda = fDriver->GetIrDAComm();
	if (irda)                               // sometimes IrDA may not be there
	    irda->GetIrDAStatus((IrDAStatus *)pOut);
	    
    fDriver->GetIrDAStatus((IrDAStatus *)pOut);
    return kIOReturnSuccess;

Fail:
    IOLog("IrDA: Failing to get status\n");
    return kIOReturnBadArgument;
}

// set irda state
//
// input: just the state (true = on, false = off)
// output: none

IOReturn
IrDAUserClient::setIrDAState(bool state)
{
    IOReturn    rtn = kIOReturnSuccess;
    
    require(fDriver, Fail);

    rtn = fDriver->SetIrDAUserClientState(state);
    return rtn;

Fail:
    IOLog("IrDA: Failing to set IrDA state\n");
    return kIOReturnBadArgument;
}
