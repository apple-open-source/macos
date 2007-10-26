/*
 * Copyright (c) 1998-2007 Apple Inc. All rights reserved.
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

//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <libkern/OSByteOrder.h>

#include <IOKit/IOUserClient.h>
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBUserClient.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/IOCommandPool.h>
/*
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/IOUserClient.h>
#include <IOKit/usb/IOUSBUserClient.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/usb/IOUSBControllerV2.h>
*/

enum
{
    kSizeToIncrementLowLatencyCommandPool = 10
};


//================================================================================================
//
//   Structure declarations
//
//================================================================================================
//
typedef struct IOUSBLowLatencyUserClientBufferInfo  IOUSBLowLatencyUserClientBufferInfo;

struct IOUSBLowLatencyUserClientBufferInfo
{
    UInt32									cookie;
    UInt32									bufferType;
    void *									bufferAddress;
    UInt32									bufferSize;
    IOMemoryDescriptor *					bufferDescriptor;
    IOMemoryDescriptor *					frameListDescriptor;
    IOMemoryMap *							frameListMap;
    IOVirtualAddress						frameListKernelAddress;
	IOBufferMemoryDescriptor *				writeDescritporForUHCI;
	IOMemoryMap *							writeMapForUHCI;
    IOUSBLowLatencyUserClientBufferInfo * 	nextBuffer;
};


//================================================================================================
//
//   Class Definition for IOUSBLowLatencyCommand
//
//================================================================================================
//
class IOUSBLowLatencyCommand : public IOCommand
{
    OSDeclareAbstractStructors(IOUSBLowLatencyCommand)

private:

    OSAsyncReference64		fAsyncRef;
    IOByteCount				fFrameLength;				// In bytes
    void *					fFrameBase;					// In user task
    IOMemoryDescriptor *	fDataBufferDescriptor;
	uint32_t				fAsyncReferenceCount;
	
    struct IOUSBLowLatencyExpansionData 
    {
    };
    
    IOUSBLowLatencyExpansionData * fIOUSBLowLatencyExpansionData;
public:

    // static constructor
    //
    static IOUSBLowLatencyCommand *	NewCommand(void);

    // accessor methods
    //
    virtual void						SetAsyncReference(OSAsyncReference64  ref);
	virtual void						SetAsyncCount(uint32_t  count);
    void                                SetFrameLength(IOByteCount frameLength)     { fFrameLength = frameLength; }
    void                                SetFrameBase(void * frameBase)              { fFrameBase = frameBase; }
    void                                SetDataBuffer(IOMemoryDescriptor * dataMem) { fDataBufferDescriptor = dataMem; }

	void                                GetAsyncReference(OSAsyncReference64 *ref)    { bcopy (&fAsyncRef, ref, kOSAsyncRef64Size); }
    IOByteCount                         GetFrameLength(void)                        { return fFrameLength; }
    void *                              GetFrameBase(void)                          { return fFrameBase; }
    IOMemoryDescriptor *                GetDataBuffer(void)                         { return fDataBufferDescriptor; }
    IOUSBLowLatencyExpansionData  *     GetExpansionData(void)                      { return fIOUSBLowLatencyExpansionData; }
    uint32_t							GetAsyncReferenceCount(void)				{ return fAsyncReferenceCount; }

    // padding methods
    //
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  0);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  1);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  2);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  3);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  4);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  5);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  6);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  7);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  8);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand,  9);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 10);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 11);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 12);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 13);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 14);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 15);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 16);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 17);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 18);
    OSMetaClassDeclareReservedUnused(IOUSBLowLatencyCommand, 19);
};


//================================================================================================
//
//   Class Definition for IOUSBInterfaceUserClientV2
//
//================================================================================================
//
class IOUSBInterfaceUserClientV2 : public IOUserClient
{
    OSDeclareDefaultStructors(IOUSBInterfaceUserClientV2)

private:
    IOUSBInterface *							fOwner;
    task_t										fTask;
    mach_port_t									fWakePort;
    IOCommandGate *                             fGate;
    IOWorkLoop	*                               fWorkLoop;
    IOUSBLowLatencyUserClientBufferInfo *		fUserClientBufferInfoListHead;
    IOCommandPool *                             fFreeUSBLowLatencyCommandPool;
    uint32_t									fOutstandingIO;
    bool                                        fDead;
    bool                                        fNeedToClose;
	bool										fClientRunningUnderRosetta;					// True if our user space client is running PPC code under Rosetta
	bool										fNeedContiguousMemoryForLowLatencyIsoch;
    uint32_t                                    fCurrentSizeOfCommandPool;

    struct IOUSBInterfaceUserClientExpansionData 
    {
    };
    
    IOUSBInterfaceUserClientExpansionData *		fIOUSBInterfaceUserClientExpansionData;
    
protected:

	static const IOExternalMethodDispatch		sMethods[kIOUSBLibInterfaceUserClientNumCommands];

public:
        
    // IOKit methods
    //
    virtual void                                stop(IOService * provider);
    virtual bool                                start( IOService * provider );
    virtual bool                                finalize(IOOptionBits options);
    virtual void                                free();
    virtual bool                                willTerminate( IOService * provider, IOOptionBits options );
    virtual bool                                didTerminate( IOService * provider, IOOptionBits options, bool * defer );
    virtual IOReturn							message( UInt32 type, IOService * provider,  void * argument = 0 );
  
	// IOUserClient methods
    //
    virtual bool								initWithTask(task_t owningTask, void *security_id, UInt32 type, OSDictionary *properties);
	virtual IOReturn							externalMethod(	uint32_t selector, IOExternalMethodArguments * arguments, IOExternalMethodDispatch * dispatch, OSObject * target, void * reference);
    virtual IOReturn                            clientClose( void );
    virtual IOReturn                            clientDied( void );
	
	// Open the IOUSBInterface
    static	IOReturn							_open(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            open(bool seize);
	
	// Close the IOUSBInterface
	static	IOReturn							_close(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            close(void);

    // IOUSBInterface methods
    //
	static	IOReturn							_GetDevice(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            GetDevice(uint64_t *device);

    static	IOReturn							_SetAlternateInterface(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            SetAlternateInterface(UInt8 altInterface);
    
	static	IOReturn							_GetFrameNumber(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size);
    
	static	IOReturn							_GetMicroFrameNumber(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            GetMicroFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size);
    
	static	IOReturn							_GetFrameNumberWithTime(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            GetFrameNumberWithTime(IOUSBGetFrameStruct *data, UInt32 *size);

	static	IOReturn							_GetBandwidthAvailable(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            GetBandwidthAvailable(uint64_t *bandwidth);
    
	static	IOReturn							_GetFrameListTime(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            GetFrameListTime(uint64_t *microsecondsInFrame);
    
	static	IOReturn							_GetEndpointProperties(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, uint64_t *transferType, uint64_t *maxPacketSize, uint64_t *interval);
    
 	static	IOReturn							_GetConfigDescriptor(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn							GetConfigDescriptor(UInt8 configIndex, IOUSBConfigurationDescriptorPtr desc, UInt32 *size);
	virtual IOReturn							GetConfigDescriptor(UInt8 configIndex, IOMemoryDescriptor * mem, uint32_t *size);
    // working with pipes
    //
	static	IOReturn							_GetPipeProperties(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            GetPipeProperties(UInt8 pipeRef, uint64_t *direction, uint64_t *number, uint64_t *transferType, uint64_t *maxPacketSize, uint64_t *interval);
    
	static	IOReturn							_ReadPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn							ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion);
	virtual IOReturn                            ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size);
	virtual IOReturn                            ReadPipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, IOByteCount *bytesRead);
        
	static	IOReturn							_WritePipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn							WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, mach_vm_address_t buffer, mach_vm_size_t size, IOUSBCompletion * completion);
	virtual IOReturn                            WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, const void *buf, UInt32 size);
	virtual IOReturn                            WritePipe(UInt8 pipeRef, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem);
	
	static	IOReturn							_GetPipeStatus(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            GetPipeStatus(UInt8 pipeRef);
    
	static	IOReturn							_AbortPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            AbortPipe(UInt8 pipeRef);
    
	static	IOReturn							_ResetPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            ResetPipe(UInt8 pipeRef);
    
	static	IOReturn							_ClearPipeStall(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            ClearPipeStall(UInt8 pipeRef, bool bothEnds);
    
	static	IOReturn							_SetPipePolicy(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval);
    
	static	IOReturn							_ControlRequestIn(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion);
	virtual IOReturn                            ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, void *requestBuffer, uint32_t *size);
	virtual IOReturn                            ControlRequestIn(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, uint32_t *pOutSize);
    
	static	IOReturn							_ControlRequestOut(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion);
	virtual IOReturn                            ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, const void *requestBuffer, uint32_t size);
	virtual IOReturn                            ControlRequestOut(UInt8 pipeRef, UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem);
    
	static	IOReturn							_SetAsyncPort(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            SetAsyncPort(mach_port_t port);

	static	IOReturn							_ReadIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	static	IOReturn							_WriteIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            DoIsochPipeAsync(IOUSBIsocStruct *stuff, io_user_reference_t * asyncReference, uint32_t asyncCount, IODirection direction);
	
	static	IOReturn							_LowLatencyReadIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	static	IOReturn							_LowLatencyWriteIsochPipe(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            DoLowLatencyIsochPipeAsync(IOUSBLowLatencyIsocStruct *isocInfo, IOUSBLowLatencyIsocCompletion *completion, IODirection direction);

    // Low Latency Buffer methods
    //
 	static	IOReturn							_LowLatencyPrepareBuffer(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                            LowLatencyPrepareBuffer(LowLatencyUserBufferInfoV2 *bufferData, uint64_t * addrOut);
	
	static	IOReturn							_LowLatencyReleaseBuffer(IOUSBInterfaceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                            LowLatencyReleaseBuffer(LowLatencyUserBufferInfoV2 *dataBuffer);
	
    // misc methods
    //
    virtual IOUSBPipe *                         GetPipeObj(UInt8 pipeNo);
    virtual void                                AddDataBufferToList( IOUSBLowLatencyUserClientBufferInfo * insertBuffer );
    virtual IOUSBLowLatencyUserClientBufferInfo *	FindBufferCookieInList( UInt32 cookie);
    virtual bool                                RemoveDataBufferFromList( IOUSBLowLatencyUserClientBufferInfo *removeBuffer);
    virtual void                                ReleasePreparedDescriptors();
    
	
	// bookkeeping methods
    //
    virtual void                                DecrementOutstandingIO(void);
    virtual void                                IncrementOutstandingIO(void);
    virtual UInt32                              GetOutstandingIO(void);
    virtual void                                IncreaseCommandPool();
	
	void										PrintExternalMethodArgs( IOExternalMethodArguments * arguments, UInt32 level );
	
	
    // static methods
    //
    static void                                 ReqComplete(void *obj, void *param, IOReturn status, UInt32 remaining);
    static void                                 IsoReqComplete(void *obj, void *param, IOReturn res, IOUSBIsocFrame *pFrames);
    static void                                 LowLatencyIsoReqComplete(void *obj, void *param, IOReturn res, IOUSBLowLatencyIsocFrame *pFrames);
    static IOReturn                             ChangeOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
    static IOReturn                             GetGatedOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);

    // padding methods
    //
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  0);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  1);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  2);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  3);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  4);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  5);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  6);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  7);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  8);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2,  9);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 10);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 11);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 12);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 13);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 14);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 15);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 16);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 17);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 18);
    OSMetaClassDeclareReservedUnused(IOUSBInterfaceUserClientV2, 19);
};


