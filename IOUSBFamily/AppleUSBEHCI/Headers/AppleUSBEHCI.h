/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#ifndef _IOKIT_AppleUSBEHCI_H
#define _IOKIT_AppleUSBEHCI_H

#include <libkern/c++/OSData.h>
#include <IOKit/IOService.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>

#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBHub.h>

#include "AppleUSBEHCIHubInfo.h"
#include "USBEHCI.h"
#include "USBEHCIRootHub.h"

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
class AppleEHCIedMemoryBlock;
class AppleEHCItdMemoryBlock;
class AppleEHCIitdMemoryBlock;
class AppleEHCIsitdMemoryBlock;
class AppleEHCIListElement;
class AppleEHCIQueueHead;
class AppleEHCIIsochListElement;
class AppleEHCIIsochTransferDescriptor;
class AppleEHCISplitIsochTransferDescriptor;

typedef struct EHCIGeneralTransferDescriptor
		    EHCIGeneralTransferDescriptor,
		    *EHCIGeneralTransferDescriptorPtr;

typedef struct AppleEHCIIsochEndpointStruct
		AppleEHCIIsochEndpoint,
		*AppleEHCIIsochEndpointPtr;

// this needs to be included after the definitions above
#include "AppleEHCIListElement.h"

// this is the extra state needed to manage TDs
struct EHCIGeneralTransferDescriptor
{
    EHCIGeneralTransferDescriptorSharedPtr	pShared;
    IOUSBCommand 				*command;			// only used if last TD, other wise its  nil
    UInt32					unused;
    AppleEHCIQueueHead				*pQH;				// pointer to TD's Queue Head
    UInt8					traceFlag;
    UInt8					pType;
	UInt8					lastTDofTransaction;
    IOPhysicalAddress				pPhysical;
    EHCIGeneralTransferDescriptorPtr		pLogicalNext;
    void*					logicalBuffer;			// used for UnlockMemory
	UInt32				lastFrame;		// the lower 32 bits the last time we checked this TD
    UInt32				lastRemaining;		//the "remaining" count the last time we checked

};


enum{
	kEHCIElementTypeBad = 0,
	kEHCIElementTypeED,
	kEHCIElementTypeiTD,
	kEHCIElementTypeSiTD
	};
	
enum
{
    kEHCICheckForRootHubConnectionsPeriod = 30, 	// Check every x secs to see if root hub has connections
    kEHCICheckForRootHubInactivityPeriod = 30		// Wait for x secs after the last time the root hub was active
};


// this is an internal "endpoint" data sructure used to track bandwidth and other
// issues in Isoch connections. It has no "shared" counterpart in the hardware
struct AppleEHCIIsochEndpointStruct {
    AppleEHCIIsochEndpointPtr			nextEP;
    AppleEHCIIsochListElement  			*toDoList;		// ITD or SITD
    AppleEHCIIsochListElement  			*toDoEnd;		// ITD or SITD
    AppleEHCIIsochListElement  			*doneQueue;		// ITD or SITD
    AppleEHCIIsochListElement  			*doneEnd;		// ITD or SITD
    UInt64					firstAvailableFrame;	// next frame available for a transfer on this EP
    UInt32					maxPacketSize;
    UInt32					activeTDs;		// + when added to todo list, - when taken from done queue
    IOReturn					accumulatedStatus;
    UInt16 					inSlot;			// where Isoc TDs are being put in the periodic list 
    short					oneMPS;			// For high bandwidth
    short					mult;			// how many oneMPS sized transactions to do
    short					functionAddress;
    short					endpointNumber;
    USBDeviceAddress				highSpeedHub;
    int						highSpeedPort;
    UInt8					direction;
};


class AppleUSBEHCI : public IOUSBControllerV2
{
    OSDeclareDefaultStructors(AppleUSBEHCI)
private:
    virtual void initForPM (IOPCIDevice *provider);
    unsigned long maxCapabilityForDomainState ( IOPMPowerFlags domainState );
    unsigned long initialPowerStateForDomainState ( IOPMPowerFlags domainState );
    virtual IOReturn setPowerState( unsigned long, IOService* );

    void showRegisters(char *s);
    void printTD(EHCIGeneralTransferDescriptorPtr		pTD);
    void printAsyncQueue(void);
    
    void AddIsocFramesToSchedule(AppleEHCIIsochEndpointPtr);
    void ReturnIsocDoneQueue(AppleEHCIIsochEndpointPtr);
    void PutTDonToDoList(AppleEHCIIsochEndpointPtr pED, AppleEHCIIsochListElement *pTD);
    void PutTDonDoneQueue(AppleEHCIIsochEndpointPtr pED, AppleEHCIIsochListElement *pTD);
    AppleEHCIIsochListElement *GetTDfromToDoList(AppleEHCIIsochEndpointPtr pED);
    AppleEHCIIsochListElement *GetTDfromDoneQueue(AppleEHCIIsochEndpointPtr pED);
    IOReturn AbortIsochEP(AppleEHCIIsochEndpointPtr);
    IOReturn DeleteIsochEP(AppleEHCIIsochEndpointPtr);
    
protected:
    IOPCIDevice *      				_device;
    IOMemoryMap *				_deviceBase;
    UInt16					_vendorID;
    UInt16					_deviceID;
    UInt16					_revisionID;
    UInt32					_errataBits;		// various bits for chip erratas
    UInt32					_pageSize;		// OS Logical page size
    EHCICapRegistersPtr				_pEHCICapRegisters;	// Capabilities registers
    EHCIRegistersPtr				_pEHCIRegisters;	// Pointer to base address of EHCI registers.
    UInt32					_dataAllocationSize;	// # of bytes allocated in for TD's
    SInt32					_greatestPeriod;		// Longest interrupt period allocated.
    EHCIGeneralTransferDescriptorPtr		_pFreeTD;		// list of availabble Trasfer Descriptors
    AppleEHCIIsochTransferDescriptor		*_pFreeITD;		// list of availabble Trasfer Descriptors
    AppleEHCISplitIsochTransferDescriptor 	*_pFreeSITD;		// list of availabble Trasfer Descriptors
    AppleEHCIQueueHead				*_pFreeQH;		// list of available Endpoint Descriptors
    EHCIGeneralTransferDescriptorPtr		_pLastFreeTD;		// last of availabble Trasfer Descriptors
    AppleEHCIIsochTransferDescriptor		*_pLastFreeITD;		// last of availabble Trasfer Descriptors
    AppleEHCISplitIsochTransferDescriptor	*_pLastFreeSITD;		// last of availabble Trasfer Descriptors
    AppleEHCIQueueHead				*_pLastFreeQH;		// last of available Endpoint Descriptors
    EHCIGeneralTransferDescriptorPtr		_pPendingTD;		// list of non processed Trasfer Descriptors
    AppleEHCIIsochEndpointPtr			_isochEPList;		// linked list of active Isoch "endpoints"
    AppleEHCIIsochEndpointPtr			_freeIsochEPList;	// linked list of freed Isoch EP data structures
    IOPhysicalAddress 				*_periodicList;		// Physical interrrupt heads
    AppleEHCIListElement 			**_logicalPeriodicList; 	// logical interrupt heads
    UInt16					_periodicBandwidth[kEHCIMaxPoll];	// bandwidth remaining per frame
    AppleUSBEHCIHubInfoPtr			_hsHubs;		// high speed hubs
    IOFilterInterruptEventSource *		_filterInterruptSource;
    UInt32					_filterInterruptCount;
    UInt8					_ehciBusState;
    IOLock *					_intLock;
    volatile AppleEHCIIsochListElement *	_savedDoneQueueHead;		// saved by the Filter Interrupt routine
    volatile UInt32				_producerCount;			// Counter used to synchronize reading of the done queue between filter (producer) and action (consumer)
    volatile UInt32				_consumerCount;			// Counter used to synchronize reading of the done queue between filter (producer) and action (consumer)
    volatile bool				_filterInterruptActive;		// in the filter interrupt routine
    IOSimpleLock *				_wdhLock;
    UInt32					_asyncAdvanceInterrupt;
    UInt32					_hostErrorInterrupt;
    UInt32					_portChangeInterrupt;
    UInt32					_errorInterrupt;
    UInt32					_completeInterrupt;
    const OSSymbol 				*usb_remote_wakeup;
    bool					_unloadUIMAcrossSleep;
    bool					_onCardBus;
    bool  					_remote_wakeup_occurred;
    bool					_idleSuspend;
    bool					_uimInitialized;
    bool					_testModeEnabled;
    bool					_sleepRegistersSaved;
    bool					_hasPCIPwrMgmt;
    bool					_ehciAvailable;
    UInt32					_isochBandwidthAvail;		// amount of available bandwidth for Isochronous transfers
    UInt32					_periodicEDsInSchedule;
    UInt64					_frameNumber;			// the current frame number
    UInt16					_rootHubFuncAddress;		// Function Address for the root hub
    struct InterruptTransaction			_outstandingTrans[kMaxOutstandingTrans];
    struct  {
        volatile UInt32	hostSystemError;				// updated by the interrupt handler
        volatile UInt32	unrecoverableError;				// updated by the interrupt handler
        volatile UInt32	ownershipChange;				// updated by the interrupt handler
        } _errors;
    UInt32 					_frameListSize;
    AppleEHCIQueueHead				*_AsyncHead;		// ptr to Control list
    EHCIGeneralTransferDescriptorPtr 		_pendingHead, _pendingTail;
   
    AppleEHCIedMemoryBlock			*_edMBHead;
    AppleEHCItdMemoryBlock			*_tdMBHead;
    AppleEHCIitdMemoryBlock			*_itdMBHead;
    AppleEHCIsitdMemoryBlock			*_sitdMBHead;
    AbsoluteTime				_lastRootHubStatusChanged;		// Last time we had activity on the root hub
    UInt32					_savedUSBIntr;				// to save during sleep
    UInt32					_savedUSBCMD;				// to save during suspend/resume
    UInt32					_savedPeriodicListBase;			// to save during sleep/wake on PC[I] Cards
    UInt32					_savedAsyncListAddr;			// to save during sleep/wake on PC[I] Cards
    UInt16 					_outSlot;

    AbsoluteTime				_lastCheckedTime;		// Last time we checked the Root Hub for inactivity


    // methods
    
    static void InterruptHandler(OSObject *owner, IOInterruptEventSource * source, int count);
    static bool PrimaryInterruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
    bool 	FilterInterrupt(int index);

    void UIMRootHubStatusChange(void);
    void UIMRootHubStatusChange( bool abort );

    void SimulateRootHubInt(
            UInt8					endpoint,
            IOMemoryDescriptor * 			buf,
            UInt32 					bufLen,
            IOUSBCompletion				completion);

    void	SetVendorInfo(void);
    AppleEHCIQueueHead *MakeEmptyEndPoint(
            UInt8 					functionAddress,
            UInt8					endpointNumber,
            UInt16					maxPacketSize,
            UInt8					speed,
            USBDeviceAddress				highSpeedHub,
            int						highSpeedPort,
            UInt8					direction);
    AppleEHCIQueueHead *MakeEmptyIntEndPoint(
            UInt8 					functionAddress,
            UInt8					endpointNumber,
            UInt16					maxPacketSize,
            UInt8					speed,
            USBDeviceAddress				highSpeedHub,
            int						highSpeedPort,
            UInt8					direction);

	void linkInterruptEndpoint(AppleEHCIQueueHead *pEHCIEndpointDescriptor);
	void linkAsyncEndpoint(AppleEHCIQueueHead *CBED, AppleEHCIQueueHead *pEDHead);
	void returnTransactions(AppleEHCIQueueHead *pED, 
							EHCIGeneralTransferDescriptor *untilThisOne);

    AppleEHCIQueueHead *AddEmptyCBEndPoint(
            UInt8 					functionAddress,
            UInt8					endpointNumber,
            UInt16					maxPacketSize,
            UInt8					speed,
            USBDeviceAddress				highSpeedHub,
            int						highSpeedPort,
            UInt8					direction,
            AppleEHCIQueueHead				*pED);
    AppleEHCIQueueHead *FindInterruptEndpoint(
            short 					functionNumber,
            short					endpointNumber,
            short					direction,
            AppleEHCIQueueHead				**pEDBack);
    AppleEHCIQueueHead *AllocateQH(void);
    EHCIGeneralTransferDescriptorPtr AllocateTD(void);
    AppleEHCIIsochTransferDescriptor *AllocateITD(void);
    AppleEHCISplitIsochTransferDescriptor *AllocateSITD(void);
    AppleEHCIIsochEndpointPtr AllocateIsochEP(void);
    IOReturn DeallocateIsochEP(AppleEHCIIsochEndpointPtr pEP);
    IOReturn  allocateTDs(
            AppleEHCIQueueHead			*pEDQueue,
            IOUSBCommand*			command,
            IOMemoryDescriptor *		CBP,
	    UInt32				bufferSize,
	    UInt16				direction,
            Boolean				controlTransaction);
    AppleEHCIQueueHead *FindControlBulkEndpoint (
        short 						functionNumber, 
        short						endpointNumber, 
	AppleEHCIQueueHead   				**pEDBack,
    short direction);
    void scavengeCompletedTransactions(IOUSBCompletionAction safeAction);
	IOReturn scavengeIsocTransactions(IOUSBCompletionAction safeAction);
	IOReturn scavengeAnIsocTD(AppleEHCIIsochListElement *pTD, IOUSBCompletionAction safeAction);

    IOReturn scavengeAnEndpointQueue(AppleEHCIListElement *pEDQueue, IOUSBCompletionAction safeAction);
    IOReturn EHCIUIMDoDoneQueueProcessing(EHCIGeneralTransferDescriptorPtr pHCDoneTD, OSStatus forceErr, IOUSBCompletionAction safeAction, EHCIGeneralTransferDescriptorPtr stopAt);
    IOReturn DeallocateTD (EHCIGeneralTransferDescriptorPtr pTD);
    IOReturn DeallocateED (AppleEHCIQueueHead *pED);
    void doCallback(EHCIGeneralTransferDescriptorPtr	nextTD,
                    UInt32				transferStatus,
                    UInt32  				bufferSizeRemaining);
	IOReturn AsyncInitialize (void);
    IOReturn InterruptInitialize (void);
    void unlinkIntEndpoint(AppleEHCIQueueHead *pED);
    void unlinkAsyncEndpoint(AppleEHCIQueueHead *pED, AppleEHCIQueueHead *pEDQueueBack);
    void HaltAsyncEndpoint(AppleEHCIQueueHead *pED, AppleEHCIQueueHead *pEDBack);
    void HaltInterruptEndpoint(AppleEHCIQueueHead *pED);
    void waitForSOF(EHCIRegistersPtr pEHCIRegisters);
    UInt16 validatePollingRate(short rawPollingRate,  short speed, int *offset, UInt16 *bytesAvailable);

    // managing hub information for multi TT hubs
    AppleUSBEHCIHubInfoPtr	GetHubInfo(UInt8 hubAddr, UInt8 hubPort);
    AppleUSBEHCIHubInfoPtr	NewHubInfo(UInt8 hubAddr, UInt8 hubPort);
    IOReturn			DeleteHubInfo(UInt8 hubAddr, UInt8 hubPort);
    
    IOReturn			EnterTestMode(void);
    IOReturn			PlacePortInMode(UInt32 port, UInt32 mode);
    IOReturn			LeaveTestMode(void);

    void			ResumeUSBBus();
    void			SuspendUSBBus();
    void			StopUSBBus();
    void			RestartUSBBus();

    AppleEHCIIsochEndpointPtr	FindIsochronousEndpoint(short functionNumber, short endpointNumber, short direction, AppleEHCIIsochEndpointPtr *pEDBack);
    AppleEHCIIsochEndpointPtr	CreateIsochronousEndpoint(short functionNumber, short endpointNumber, short direction, USBDeviceAddress highSpeedHub, int highSpeedPort);

    IOReturn 		CreateHSIsochTransfer(	AppleEHCIIsochEndpointPtr	pEP,
							IOUSBIsocCompletion		completion,
							UInt64 				frameNumberStart,
							IOMemoryDescriptor 		*pBuffer,
							UInt32 				frameCount,
							IOUSBLowLatencyIsocFrame	*pFrames,
							UInt32 				updateFrequency,
							Boolean lowLatency);
    IOReturn 		CreateHSIsochTransfer(	AppleEHCIIsochEndpointPtr	pEP,
						    IOUSBIsocCompletion		completion,
						    UInt64 			frameNumberStart,
						    IOMemoryDescriptor 		*pBuffer,
						    UInt32 			frameCount,
						    IOUSBLowLatencyIsocFrame	*pFrames,
						    UInt32 			updateFrequency);
    IOReturn 		CreateHSIsochTransfer(	AppleEHCIIsochEndpointPtr	pEP,
						    IOUSBIsocCompletion		completion,
						    UInt64 			frameNumberStart,
						    IOMemoryDescriptor 		*pBuffer,
						    UInt32 			frameCount,
						    IOUSBIsocFrame	*pFrames);
    IOReturn 		CreateSplitIsochTransfer(AppleEHCIIsochEndpointPtr	pEP,
						    IOUSBIsocCompletion		completion,
						    UInt64 			frameNumberStart,
						    IOMemoryDescriptor 		*pBuffer,
						    UInt32 			frameCount,
						    IOUSBIsocFrame		*pFrames, // could be IOUSBLowLatencyIsocFrame*
						    UInt32 			updateFrequency = 0,
						    bool 			lowLatency = false);

	void  ReturnOneTransaction(
            EHCIGeneralTransferDescriptor 	*transaction,
            AppleEHCIQueueHead   	*pED,
            AppleEHCIQueueHead   	*pEDBack,
			IOReturn				err);
    UInt32 findBufferRemaining(AppleEHCIQueueHead *pED);
	void UIMCheckForTimeouts(void);
#if 0
	void printED(AppleEHCIQueueHead * pED);
#endif

public:
    virtual bool 	init(OSDictionary * propTable);
    virtual bool 	start( IOService * provider );
    virtual void 	free();
    IOReturn UIMInitialize(IOService * provider);
    IOReturn UIMFinalize();
    IOReturn UIMFinalizeForPowerDown();
    IOReturn UIMInitializeForPowerUp();

    IOReturn DeallocateITD (AppleEHCIIsochTransferDescriptor *pTD);
    IOReturn DeallocateSITD (AppleEHCISplitIsochTransferDescriptor *pTD);

   // Control
    virtual IOReturn UIMCreateControlEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt16				maxPacketSize,
            UInt8				speed);
    virtual IOReturn UIMCreateControlEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt16				maxPacketSize,
            UInt8				speed,
            USBDeviceAddress    		highSpeedHub,
            int			                highSpeedPort);
            
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
            IOMemoryDescriptor *		CBP,
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
   virtual IOReturn UIMCreateControlTransfer(
            short				functionNumber,
            short				endpointNumber,
            IOUSBCommand*			command,
            void *				CBP,
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
    virtual IOReturn UIMCreateBulkEndpoint(
            UInt8				functionNumber,
            UInt8				endpointNumber,
            UInt8				direction,
            UInt8				speed,
            UInt16				maxPacketSize,
            USBDeviceAddress    		highSpeedHub,
            int			                highSpeedPort);



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

    // Interrupt
    virtual IOReturn UIMCreateInterruptEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt8				direction,
            short				speed,
            UInt16				maxPacketSize,
            short				pollingRate);
    virtual IOReturn UIMCreateInterruptEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt8				direction,
            short				speed,
            UInt16				maxPacketSize,
            short				pollingRate,
            USBDeviceAddress    		highSpeedHub,
            int                 		highSpeedPort);

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
    virtual IOReturn UIMCreateIsochEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt32				maxPacketSize,
            UInt8				direction,
            USBDeviceAddress    		highSpeedHub,
            int                 		highSpeedPort);

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

    IOReturn HandleEndpointAbort(short functionNumber, short endpointNumber, short direction);
    
    IOReturn GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc);
    IOReturn GetRootHubDescriptor(IOUSBHubDescriptor *desc);
    IOReturn SetRootHubDescriptor(OSData *buffer);
    IOReturn GetRootHubConfDescriptor(OSData *desc);
    IOReturn GetRootHubStatus(IOUSBHubStatus *status);
    IOReturn SetRootHubFeature(UInt16 wValue);
    IOReturn ClearRootHubFeature(UInt16 wValue);
    IOReturn GetRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port);
    IOReturn SetRootHubPortFeature(UInt16 wValue, UInt16 port);
    IOReturn ClearRootHubPortFeature(UInt16 wValue, UInt16 port);
    IOReturn GetRootHubPortState(UInt8 *state, UInt16 port);
    IOReturn SetHubAddress(UInt16 wValue);

    virtual UInt32 GetBandwidthAvailable();
    virtual UInt64 GetFrameNumber();
    virtual UInt32 GetFrameNumber32();
    virtual UInt64 GetMicroFrameNumber();

    virtual void PollInterrupts(IOUSBCompletionAction safeAction=0);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);


    IOReturn EHCIRootHubPower(bool on);
    IOReturn EHCIRootHubResetChangeConnection(UInt16 port);
    IOReturn EHCIRootHubResetResetChange(UInt16 port);
    IOReturn EHCIRootHubResetSuspendChange(UInt16 port);
    IOReturn EHCIRootHubResetEnableChange(UInt16 port);
    IOReturn EHCIRootHubResetOverCurrentChange(UInt16 port);
    IOReturn EHCIRootHubResetPort (UInt16 port);
    IOReturn EHCIRootHubPortEnable(UInt16 port, bool on);
    IOReturn EHCIRootHubPortSuspend(UInt16 port, bool on);
    IOReturn EHCIRootHubPortPower(UInt16 port, bool on);
    IOReturn SimulateEDDelete (short endpointNumber, short direction);

    IOReturn SimulateEDAbort (short endpointNumber, short direction);

    IOReturn GetRootHubStringDescriptor(UInt8	index, OSData *desc);
    AbsoluteTime	LastRootHubPortStatusChanged( bool reset );
 
    virtual IOReturn 		UIMCreateIsochTransfer(
                                                        short			functionAddress,
                                                        short			endpointNumber,
                                                        IOUSBIsocCompletion	completion,
                                                        UInt8			direction,
                                                        UInt64			frameStart,
                                                        IOMemoryDescriptor *	pBuffer,
                                                        UInt32			frameCount,
                                                        IOUSBLowLatencyIsocFrame *pFrames,
                                                        UInt32			updateFrequency);


    virtual IOReturn 		UIMHubMaintenance(USBDeviceAddress highSpeedHub, UInt32 highSpeedPort, UInt32 command, UInt32 flags);
    virtual IOReturn		UIMSetTestMode(UInt32 mode, UInt32 port);
    virtual IOReturn		EnableAsyncSchedule(void);
    virtual IOReturn		DisableAsyncSchedule(void);
    virtual IOReturn		EnablePeriodicSchedule(void);
    virtual IOReturn		DisablePeriodicSchedule(void);

    void 			CheckEDListForTimeouts(AppleEHCIQueueHead *head);
    bool			RootHubAreAllPortsDisconnected( void );
    void			GetNumberOfPorts(UInt8 *numPorts);


};
// Constants that define the different power states in the setPowerState call
enum
{
    kEHCISetPowerLevelSuspend		= 0,
    kEHCISetPowerLevelRunning		= 1,
    kEHCISetPowerLevelIdleSuspend	= 2
};

enum
{
    kEHCIBusStateOff		= 0,
    kEHCIBusStateSuspended	= 1,
    kEHCIBusStateRunning	= 2
};


#endif /* _IOKIT_AppleUSBEHCI_H */
