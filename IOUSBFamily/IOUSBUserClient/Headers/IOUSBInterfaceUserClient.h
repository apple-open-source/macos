/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <IOKit/IOUserClient.h>
#include <IOKit/usb/IOUSBUserClient.h>
#include <IOKit/IOCommandPool.h>

enum
{
    kSizeToIncrementLowLatencyCommandPool = 10
};


struct AsyncPB {
    OSAsyncReference 		fAsyncRef;
    UInt32 			fMax;
    IOMemoryDescriptor 		*fMem;
    IOUSBDevRequestDesc		req;
};

struct IsoAsyncPB {
    OSAsyncReference 	fAsyncRef;
    int			frameLen;	// In bytes
    void *		frameBase;	// In user task
    IOMemoryDescriptor *dataMem;
    IOMemoryDescriptor *countMem;
    IOUSBIsocFrame	frames[0];
};

typedef struct LowLatencyUserClientBufferInfo  LowLatencyUserClientBufferInfo;

struct LowLatencyUserClientBufferInfo
{
    UInt32				cookie;
    UInt32				bufferType;
    void *				bufferAddress;
    UInt32				bufferSize;
    IOMemoryDescriptor *		bufferDescriptor;
    IOMemoryDescriptor *		frameListDescriptor;
    IOMemoryMap *			frameListMap;
    IOVirtualAddress			frameListKernelAddress;
    LowLatencyUserClientBufferInfo * 	nextBuffer;
};

class IOUSBLowLatencyCommand : public IOCommand
{
    OSDeclareAbstractStructors(IOUSBLowLatencyCommand)

private:

    OSAsyncReference		fAsyncRef;
    IOByteCount			fFrameLength;	// In bytes
    void *			fFrameBase;	// In user task
    IOMemoryDescriptor *	fDataBufferDescriptor;

public:

    // static constructor
    static IOUSBLowLatencyCommand *	NewCommand(void);

    // accessor methods
    //
    void  			SetAsyncReference(OSAsyncReference  ref);
    void  			SetFrameLength(IOByteCount frameLength) { fFrameLength = frameLength; }
    void  			SetFrameBase(void * frameBase) { fFrameBase = frameBase; }
    void  			SetDataBuffer(IOMemoryDescriptor * dataMem) {fDataBufferDescriptor = dataMem; }

    void	 	 	GetAsyncReference(OSAsyncReference *ref) {  *ref = fAsyncRef; }
    IOByteCount 		GetFrameLength(void) { return fFrameLength; }
    void * 			GetFrameBase(void){ return fFrameBase; }
    IOMemoryDescriptor * 	GetDataBuffer(void){ return fDataBufferDescriptor; }
};


class IOUSBInterfaceUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOUSBInterfaceUserClient)

private:
    IOUSBInterface *			fOwner;
    task_t				fTask;
    const IOExternalMethod		*fMethods;
    const IOExternalAsyncMethod 	*fAsyncMethods;
    IOCommandGate 			*fGate;
    UInt32				fNumMethods;
    UInt32				fNumAsyncMethods;
    UInt32				fOutstandingIO;
    mach_port_t 			fWakePort;
    bool				fDead;
    bool				fNeedToClose;
    IOCommandPool *			fFreeUSBLowLatencyCommandPool;
    UInt32				fCurrentSizeOfCommandPool;
    IOWorkLoop	*			fWorkLoop;
    LowLatencyUserClientBufferInfo *	fUserClientBufferInfoListHead;

    static const IOExternalMethod	sMethods[kNumUSBInterfaceMethods];
    static const IOExternalAsyncMethod	sAsyncMethods[kNumUSBInterfaceAsyncMethods];

   // my protected methods
    virtual void 			SetExternalMethodVectors(void);
    void 			IncreaseCommandPool();

    // IOKit methods
    virtual void 	stop(IOService * provider);
    virtual bool 	start( IOService * provider );
    virtual bool 	finalize(IOOptionBits options);
    virtual void 	free();
    // "new" IOService methods
    virtual bool 	willTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	didTerminate( IOService * provider, IOOptionBits options, bool * defer );
  
    // pseudo IOKit methods - these methods are NOT the IOService:: methods, since both IOService::open
    // and IOService::close require an IOService* as the first parameter
    virtual IOReturn  	open(bool seize);
    virtual IOReturn  	close(void);

    // IOUserClient methods
    virtual bool			initWithTask(task_t owningTask, void *security_id, UInt32 type);
    virtual IOExternalMethod* 		getTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOExternalAsyncMethod* 	getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOReturn 			clientClose( void );
    virtual IOReturn 			clientDied( void );

    // misc methods
    virtual IOUSBPipe*			GetPipeObj(UInt8 pipeNo);

    // worker method for Isoch stuff
    virtual IOReturn DoIsochPipeAsync(OSAsyncReference asyncRef, IOUSBIsocStruct *stuff, IODirection direction);
    virtual IOReturn DoLowLatencyIsochPipeAsync(OSAsyncReference asyncRef, IOUSBLowLatencyIsocStruct *stuff, IODirection direction);

    // IOUSBInterface methods
    virtual IOReturn GetDevice(io_service_t *device);
    virtual IOReturn SetAlternateInterface(UInt8 altInterface);
    virtual IOReturn GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size);
    virtual IOReturn GetBandwidthAvailable(UInt32 *bandwidth);
    virtual IOReturn GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt32 *transferType, UInt32 *maxPacketSize, UInt32 *interval);
    
    // working with pipes
    virtual IOReturn GetPipeProperties(UInt8 pipeRef, UInt32 *direction, UInt32 *number, UInt32 *transferType, UInt32 *maxPacketSize, UInt32 *interval);
    virtual IOReturn ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size);
    virtual IOReturn ReadPipeOOL(IOUSBBulkPipeReq *reqIn, UInt32 *sizeOut, IOByteCount inCount, IOByteCount *outCount);
    virtual IOReturn WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 size);
    virtual IOReturn WritePipeOOL(IOUSBBulkPipeReq *req, IOByteCount inCount);
    virtual IOReturn GetPipeStatus(UInt8 pipeRef);
    virtual IOReturn AbortPipe(UInt8 pipeRef);
    virtual IOReturn ResetPipe(UInt8 pipeRef);
    virtual IOReturn SetPipeIdle(UInt8 pipeRef);
    virtual IOReturn SetPipeActive(UInt8 pipeRef);
    virtual IOReturn ClearPipeStall(UInt8 pipeRef, bool bothEnds);
    virtual IOReturn SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval);

    virtual IOReturn ControlRequestIn(UInt32 param1, UInt32 param2, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size);
    virtual IOReturn ControlRequestOut(UInt32 param1, UInt32 param2, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 size);

    virtual IOReturn ControlRequestInOOL(IOUSBDevReqOOLTO *req, UInt32 *sizeOut, IOByteCount inCount, IOByteCount *outCount);
    virtual IOReturn ControlRequestOutOOL(IOUSBDevReqOOLTO *req, IOByteCount inCount);

    // async methods
    virtual IOReturn SetAsyncPort(OSAsyncReference asyncRef);

    virtual IOReturn ControlAsyncRequestOut(OSAsyncReference asyncRef, IOUSBDevReqOOLTO *reqIn, IOByteCount inCount);
    virtual IOReturn ControlAsyncRequestIn(OSAsyncReference asyncRef, IOUSBDevReqOOLTO *reqIn, IOByteCount inCount);

    virtual IOReturn AsyncReadPipe(OSAsyncReference asyncRef, UInt32 pipe, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout);
    virtual IOReturn AsyncWritePipe(OSAsyncReference asyncRef, UInt32 pipe, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout);

    virtual IOReturn ReadIsochPipe(OSAsyncReference asyncRef, IOUSBIsocStruct *stuff, UInt32 sizeIn);
    virtual IOReturn WriteIsochPipe(OSAsyncReference asyncRef, IOUSBIsocStruct *stuff, UInt32 sizeIn);
    virtual IOReturn LowLatencyReadIsochPipe(OSAsyncReference asyncRef, IOUSBLowLatencyIsocStruct *stuff, UInt32 sizeIn);
    virtual IOReturn LowLatencyWriteIsochPipe(OSAsyncReference asyncRef, IOUSBLowLatencyIsocStruct *stuff, UInt32 sizeIn);

    // bookkeeping methods
    void		DecrementOutstandingIO(void);
    void		IncrementOutstandingIO(void);
    UInt32		GetOutstandingIO(void);
    
    // Low Latency Buffer methods
    //
    virtual IOReturn			LowLatencyPrepareBuffer(LowLatencyUserBufferInfo *dataBuffer);
    virtual IOReturn			LowLatencyReleaseBuffer(LowLatencyUserBufferInfo *dataBuffer);
    virtual void			AddDataBufferToList( LowLatencyUserClientBufferInfo * insertBuffer );
    virtual LowLatencyUserClientBufferInfo *	FindBufferCookieInList( UInt32 cookie);
    virtual bool			RemoveDataBufferFromList( LowLatencyUserClientBufferInfo *removeBuffer);
    
    
    // static methods
    static void 	ReqComplete(void *obj, void *param, IOReturn status, UInt32 remaining);
    static void		IsoReqComplete(void *obj, void *param, IOReturn res, IOUSBIsocFrame *pFrames);
    static void		LowLatencyIsoReqComplete(void *obj, void *param, IOReturn res, IOUSBLowLatencyIsocFrame *pFrames);
    static IOReturn	ChangeOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
    static IOReturn	GetGatedOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);

};

