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
#ifndef _IOKIT_AppleUSBOHCI_H
#define _IOKIT_AppleUSBOHCI_H

#include <libkern/c++/OSData.h>

#include <IOKit/IOService.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBHub.h>

#include "USBOHCI.h"
#include "USBOHCIRootHub.h"

#define USB_CONSTANT16(x)	((((x) >> 8) & 0x0ff) | ((x & 0xff) << 8))
#define MICROSECOND		(1)
#define MILLISECOND		(1000)

#ifdef __ppc__
#define IOSync eieio
#else
#define IOSync()
#endif

extern "C" {
    extern int	kvtophys(vm_offset_t);
    extern void delay(int);
};

struct InterruptTransaction {
    IOMemoryDescriptor *	buf;
    UInt32 			bufLen;
    IOUSBCompletion		completion;
};
#define kMaxOutstandingTrans 4

class IONaturalMemoryCursor;

class AppleUSBOHCI : public IOUSBController
{
    OSDeclareDefaultStructors(AppleUSBOHCI)

private:
    virtual IOReturn 		setPowerState( unsigned long, IOService* );
    virtual void 		initForPM (IOService *provider);
    unsigned long 		maxCapabilityForDomainState ( IOPMPowerFlags domainState );
    unsigned long 		initialPowerStateForDomainState ( IOPMPowerFlags domainState );
    void			ResumeUSBBus();
    void			SuspendUSBBus();

    void print_td(OHCIGeneralTransferDescriptorPtr x);
    void print_itd(OHCIIsochTransferDescriptorPtr x);
    void print_ed(OHCIEndpointDescriptorPtr x);
    void print_isoc_ed(OHCIEndpointDescriptorPtr x);
    void print_list(OHCIEndpointDescriptorPtr pListHead,
                    OHCIEndpointDescriptorPtr pListTail);
    void print_control_list(void);
    void print_bulk_list(void);
    void print_int_list(void);

protected:

    IOPCIDevice *      				_device;
    IOMemoryMap *				_deviceBase;
    IONaturalMemoryCursor * 			_genCursor;
    IONaturalMemoryCursor * 			_isoCursor;
    OHCIGeneralTransferDescriptorPtr 		_pendingHead;
    OHCIGeneralTransferDescriptorPtr		_pendingTail;
    UInt16					_vendorID;
    UInt16					_deviceID;
    UInt16					_revisionID;
    UInt32					_errataBits;		// various bits for chip erratas
    OHCIRegistersPtr				_pOHCIRegisters;	// Pointer to base address of OHCI registers.
    Ptr						_pHCCA;			// Pointer to HCCA.
    OHCIIntHead					_pInterruptHead[63];	// ptr to private list of all interrupts heads 			
    volatile OHCIEndpointDescriptorPtr		_pIsochHead;		// ptr to Isochtonous list
    volatile OHCIEndpointDescriptorPtr		_pIsochTail;		// ptr to Isochtonous list
    volatile OHCIEndpointDescriptorPtr		_pBulkHead;		// ptr to Bulk list
    volatile OHCIEndpointDescriptorPtr		_pControlHead;		// ptr to Control list
    volatile OHCIEndpointDescriptorPtr		_pBulkTail;		// ptr to Bulk list
    volatile OHCIEndpointDescriptorPtr		_pControlTail;		// ptr to Control list
    volatile OHCIPhysicalLogicalPtr		_pPhysicalLogical;	// ptr to list of memory maps
    volatile OHCIGeneralTransferDescriptorPtr	_pFreeTD;		// list of availabble Trasfer Descriptors
    volatile OHCIIsochTransferDescriptorPtr	_pFreeITD;		// list of availabble Trasfer Descriptors
    volatile OHCIEndpointDescriptorPtr		_pFreeED;		// list of available Endpoint Descriptors
    volatile OHCIGeneralTransferDescriptorPtr	_pLastFreeTD;		// last of availabble Trasfer Descriptors
    volatile OHCIIsochTransferDescriptorPtr	_pLastFreeITD;		// last of availabble Trasfer Descriptors
    volatile OHCIEndpointDescriptorPtr		_pLastFreeED;		// last of available Endpoint Descriptors
    volatile OHCIGeneralTransferDescriptorPtr	_pPendingTD;		// list of non processed Trasfer Descriptors
    Ptr						_pDataAllocation;	// ptr to block used for TD, ED, ITD
    Ptr						_pPCIAssignedAddresses;	// ptr to PCI assigned addresses
    UInt32					_pageSize;		// OS Logical page size
    struct  {
        volatile UInt32	scheduleOverrun;				// updated by the interrupt handler
        volatile UInt32	unrecoverableError;				// updated by the interrupt handler
        volatile UInt32	frameNumberOverflow;				// updated by the interrupt handler
        volatile UInt32	ownershipChange;				// updated by the interrupt handler
        } _errors;
    volatile UInt64				_frameNumber;
    UInt16					_rootHubFuncAddress;	// Function Address for the root hub
    int						_OptiOn;
    UInt32					_isochBandwidthAvail;	// amount of available bandwidth for Isochronous transfers
    UInt32					_disablePortsBitmap;	// Bitmaps of ports that support port suspend even if they have an errata
    UInt32					_dataAllocationSize;	// # of bytes allocated in for TD's
    IOInterruptEventSource *			_interruptSource;
    IOLock *					_intLock;
    struct InterruptTransaction			_outstandingTrans[kMaxOutstandingTrans];
    bool					_uimInitialized;
    bool					_unloadUIMAcrossSleep;
    bool					_onCardBus;
    bool					_idleSuspend;
    IOPhysicalAddress 				_hccaPhysAddr;
    UInt8					_ohciBusState;
    AbsoluteTime				_lastCheckedTime;	// Last time we checked the Root Hub for inactivity
    AbsoluteTime				_lastRootHubStatusChanged;	// Last time we had activity on the root hub
    

    static void 				InterruptHandler(OSObject *owner,  IOInterruptEventSource * source, int count);
    void					SetVendorInfo(void);
    void					finishPending();
    IOReturn 					ControlInitialize(void);
    IOReturn 					BulkInitialize (void);
    IOReturn 					IsochronousInitialize(void);
    IOReturn 					InterruptInitialize (void);

    // callPlatformFunction symbols
    //
    const OSSymbol *				usb_remote_wakeup;
    bool  					_remote_wakeup_occurred;
    
    // Memory routines
    UInt32 GetLogicalAddress(UInt32 pPhysicalAddress);
    UInt32 GetPhysicalAddress(UInt32 LogicalAddress,
                                     UInt32 count);
    UInt32 CreatePhysicalAddress(UInt32 pLogicalAddress,
                                        UInt32 count);
    IOReturn AllocateMemory(int num_of_TDs,
                                   int num_of_EDs,
                                   int num_of_ITDs);
    void doCallback(OHCIGeneralTransferDescriptorPtr	nextTD,
                    UInt32				transferStatus,
                    UInt32  				bufferSizeRemaining);
    UInt32 findBufferRemaining (OHCIGeneralTransferDescriptorPtr pCurrentTD);
    OHCIIsochTransferDescriptorPtr AllocateITD(void);
    OHCIGeneralTransferDescriptorPtr AllocateTD(void);
    OHCIEndpointDescriptorPtr AllocateED(void);
    IOReturn DeallocateITD (OHCIIsochTransferDescriptorPtr pTD);
    IOReturn DeallocateTD (OHCIGeneralTransferDescriptorPtr pTD);
    IOReturn DeallocateED (OHCIEndpointDescriptorPtr pED);
    IOReturn RemoveAllTDs(OHCIEndpointDescriptorPtr pED);
    IOReturn RemoveTDs(OHCIEndpointDescriptorPtr pED);
    IOReturn DoDoneQueueProcessing(OHCIGeneralTransferDescriptorPtr pHCDoneTD,
                                   IOUSBCompletionAction safeAction);
    void UIMProcessDoneQueue(IOUSBCompletionAction safeAction=0);
    void UIMRootHubStatusChange( void );
    void UIMRootHubStatusChange(bool abort);
    void SimulateRootHubInt(
            UInt8					endpoint,
            IOMemoryDescriptor * 			buf,
            UInt32 					bufLen,
            IOUSBCompletion				completion);
    IOReturn SimulateInterruptEDCreate(UInt16 maxPacketSize, short pollingRate);
    IOReturn SimulateControlEDCreate (UInt16 maxPacketSize);
    IOReturn SimulateEDAbort (short endpointNumber, short direction);
    IOReturn SimulateEDDelete (short endpointNumber, short direction);
    IOReturn SimulateEDClearStall (short endpointNumber, short direction);
    
    OHCIEndpointDescriptorPtr AddEmptyEndPoint(
            UInt8 					functionAddress,
            UInt8					endpointNumber,
            UInt16					maxPacketSize,
            UInt8					speed,
            UInt8					direction,
            OHCIEndpointDescriptorPtr			pED,
            OHCIEDFormat				format);

    OHCIEndpointDescriptorPtr FindEndpoint (
            short 					functionNumber,
            short 					endpointNumber,
            short 					direction,
            OHCIEndpointDescriptorPtr 			*pEDQueueBack,
            UInt32 					*controlMask);

    OHCIEndpointDescriptorPtr FindControlEndpoint (
            short 					functionNumber,
            short					endpointNumber,
            OHCIEndpointDescriptorPtr   		*pEDBack);

    OHCIEndpointDescriptorPtr FindBulkEndpoint (
            short 					functionNumber,
            short					endpointNumber,
            short					direction,
            OHCIEndpointDescriptorPtr			*pEDBack);

    OHCIEndpointDescriptorPtr FindIsochronousEndpoint(
            short 					functionNumber,
            short					endpointNumber,
            short 					direction,
            OHCIEndpointDescriptorPtr			*pEDBack);
            
    OHCIEndpointDescriptorPtr FindInterruptEndpoint(
            short 					functionNumber,
            short					endpointNumber,
            short					direction,
            OHCIEndpointDescriptorPtr			*pEDBack);

    
    void DoOptiFix(OHCIEndpointDescriptorPtr pIsochHead);
    void OptiLSHSFix(void);
    void dumpRegs(void);
    bool DetermineInterruptOffset(UInt32          pollingRate,
                            UInt32          reserveBandwidth,
                            int             *offset);
    void ReturnTransactions(
                OHCIGeneralTransferDescriptor 	*transaction,
                UInt32				tail);

    void ReturnOneTransaction(
                OHCIGeneralTransferDescriptor 	*transaction,
		OHCIEndpointDescriptorPtr	pED,
		IOReturn			err);

    void CheckEDListForTimeouts(
		OHCIEndpointDescriptorPtr 	head, 
		OHCIEndpointDescriptorPtr 	tail);
    
public:
    virtual bool 	init(OSDictionary * propTable);
    virtual bool 	start( IOService * provider );
    virtual void 	stop( IOService * provider );
    virtual bool 	finalize(IOOptionBits options);
    virtual IOReturn 	message( UInt32 type, IOService * provider,  void * argument = 0 );

    /*
     * UIM methods
     */
    IOReturn UIMInitialize(IOService * provider);
    IOReturn UIMFinalize();
    IOReturn UIMInitializeForPowerUp();
    IOReturn UIMFinalizeForPowerDown();
    
    // Control
    virtual IOReturn UIMCreateControlEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt16				maxPacketSize,
            UInt8				speed);

    // method in 1.8 and 1.8.1
   virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            void *				CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);
    //same method in 1.8.2
   virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCommand*			command,
            void *				CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    // method in 1.8 and 1.8.1
    virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    //same method in 1.8.2
    virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCommand*			command,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    // Bulk
    virtual IOReturn UIMCreateBulkEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt8				direction,
            UInt8				speed,
            UInt8				maxPacketSize);

    // method in 1.8 and 1.8.1
    virtual IOReturn UIMCreateBulkTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    // same method in 1.8.2
    virtual IOReturn UIMCreateBulkTransfer(IOUSBCommand* command);

    // general purpose method to handle all of the other transfers - not in the controller class
    virtual IOReturn CreateGeneralTransfer(
            OHCIEndpointDescriptorPtr		queue,
            IOUSBCommand*			command,
            IOMemoryDescriptor*			CBP,
            UInt32				bufferSize,
            UInt32				flags,
            UInt32				type,
            UInt32				kickBits);

    // Interrupt
    virtual IOReturn UIMCreateInterruptEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt8				direction,
            short				speed,
            UInt16				maxPacketSize,
            short				pollingRate);

    // method in 1.8 and 1.8.1
    virtual IOReturn UIMCreateInterruptTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction);

    // method in 1.8.2
    virtual IOReturn UIMCreateInterruptTransfer(IOUSBCommand* command);

    // Isoch
    virtual IOReturn UIMCreateIsochEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt32				maxPacketSize,
            UInt8				direction);

    virtual IOReturn UIMCreateIsochTransfer(
	short				functionAddress,
	short				endpointNumber,
	IOUSBIsocCompletion		completion,
	UInt8				direction,
	UInt64				frameStart,
	IOMemoryDescriptor *		pBuffer,
	UInt32				frameCount,
	IOUSBIsocFrame			*pFrames);

    virtual IOReturn UIMAbortEndpoint(
            short				functionNumber,
            short				endpointNumber,
            short				direction);
    virtual IOReturn UIMDeleteEndpoint(
            short				functionNumber,
            short				endpointNumber,
            short				direction);
    virtual IOReturn UIMClearEndpointStall(
            short				functionNumber,
            short				endpointNumber,
            short				direction);
    /*
     * Root hub methods
     */
    IOReturn 		GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc);
    IOReturn 		GetRootHubDescriptor(IOUSBHubDescriptor *desc);
    IOReturn 		SetRootHubDescriptor(OSData *buffer);
    IOReturn 		GetRootHubConfDescriptor(OSData *desc);
    IOReturn 		GetRootHubStatus(IOUSBHubStatus *status);
    IOReturn		GetRootHubStringDescriptor(UInt8 index, OSData *desc);
    IOReturn 		SetRootHubFeature(UInt16 wValue);
    IOReturn 		ClearRootHubFeature(UInt16 wValue);
    IOReturn 		GetRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port);
    IOReturn 		SetRootHubPortFeature(UInt16 wValue, UInt16 port);
    IOReturn 		ClearRootHubPortFeature(UInt16 wValue, UInt16 port);
    IOReturn 		GetRootHubPortState(UInt8 *state, UInt16 port);
    IOReturn 		SetHubAddress(UInt16 wValue);
    AbsoluteTime	LastRootHubPortStatusChanged( bool reset );
    bool		RootHubAreAllPortsDisconnected( void );
    void		OHCIGetNumberOfPorts(UInt8 *numPorts);
    
    void OHCIRootHubPower(bool on);
    void OHCIRootHubResetChangeConnection(UInt16 port);
    void OHCIRootHubResetResetChange(UInt16 port);
    void OHCIRootHubResetSuspendChange(UInt16 port);
    void OHCIRootHubResetEnableChange(UInt16 port);
    void OHCIRootHubResetOverCurrentChange(UInt16 port);
    void OHCIRootHubResetPort (UInt16 port);
    void OHCIRootHubPortEnable(UInt16 port, bool on);
    void OHCIRootHubPortSuspend(UInt16 port, bool on);
    void OHCIRootHubPortPower(UInt16 port, bool on);

    virtual UInt32 GetBandwidthAvailable();
    virtual UInt64 GetFrameNumber();
    virtual UInt32 GetFrameNumber32();

    virtual void PollInterrupts(IOUSBCompletionAction safeAction=0);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);
    virtual void UIMCheckForTimeouts(void);

};


// constants for the uimFlags field of the GTD structure
// may also be used in ITD one day if needed. plenty of bits to go around
enum
{
    kUIMFlagsLastTD	=	0x0001
};

enum
{
    kOHCIBusStateOff		= 0,
    kOHCIBusStateSuspended	= 1,
    kOHCIBusStateRunning	= 2
};

// Constants that define the different power states in the setPowerState call
enum
{
    kOHCISetPowerLevelSuspend		= 0,
    kOHCISetPowerLevelRunning		= 1,
    kOHCISetPowerLevelIdleSuspend	= 2
};

// constants to use to decide whether to idle suspend a bus
//
enum
{
    kOHCICheckForRootHubConnectionsPeriod = 30, 	// Check every x secs to see if root hub has connections
    kOHCICheckForRootHubInactivityPeriod = 30		// Wait for x secs after the last time the root hub was active
};

#endif /* _IOKIT_AppleUSBOHCI_H */
