/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>

#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/IOUSBControllerListElement.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBHub.h>

#include "USBEHCI.h"
#include "USBEHCIRootHub.h"

#define MICROSECOND		(1)
#define MILLISECOND		(1000)

// Convert USBLog to use kprintf debugging
// The switch is here, but the work is done in the individual source files because this header is included by the companion controllers
#ifndef EHCI_USE_KPRINTF
	#define EHCI_USE_KPRINTF 0
#endif

#ifdef __ppc__
#define IOSync eieio
#else
#define IOSync() __asm__ __volatile__ ( "mfence" : : : "memory" )
#endif

#ifndef MIN
	#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif 


class IONaturalMemoryCursor;
class AppleEHCIedMemoryBlock;
class AppleEHCItdMemoryBlock;
class AppleEHCIitdMemoryBlock;
class AppleEHCIsitdMemoryBlock;
class AppleEHCIQueueHead;
class AppleEHCIIsochTransferDescriptor;
class AppleEHCISplitIsochTransferDescriptor;

typedef struct EHCIGeneralTransferDescriptor
EHCIGeneralTransferDescriptor,
*EHCIGeneralTransferDescriptorPtr;

// these need to be included after the definitions above
#include "AppleEHCIListElement.h"
#include "AppleUSBEHCIHubInfo.h"

// this is the extra state needed to manage TDs
struct EHCIGeneralTransferDescriptor
{
    EHCIGeneralTransferDescriptorSharedPtr	pShared;				// points to the shared memory area that the HC sees
    IOUSBCommand							*command;				// only used if last TD, other wise its  nil
    AppleEHCIQueueHead						*pQH;					// pointer to TD's Queue Head
    bool									traceFlag;
	bool									callbackOnTD;			// this TD kicks off a completion callback
	bool									multiXferTransaction;	// this is a multi transfer (i.e. control) Xaction
	bool									finalXferInTransaction;	// this is the final transfer (i.e. the status phase) Xaction
    USBPhysicalAddress32					pPhysical;
    EHCIGeneralTransferDescriptorPtr		pLogicalNext;
    void*									logicalBuffer;			// used for UnlockMemory
	UInt32									lastFrame;				// the lower 32 bits the last time we checked this TD
    UInt32									lastRemaining;			//the "remaining" count the last time we checked
    UInt32									tdSize;					//the total bytes to be transferred by this TD. For statistics only
    UInt32									flagsAtError;			// the flags word the last time this stopped with an error
    UInt32									errCount;				// software error count for restarting transactions.
	
};

struct EHCIDoneQueueParams 
{ 
	EHCIGeneralTransferDescriptorPtr		pHCDoneTD; 
	OSStatus								forceErr;  
	IOUSBCompletionAction					safeAction; 
	EHCIGeneralTransferDescriptorPtr		stopAt; 
};



enum{
	kEHCIElementTypeBad = 0,
	kEHCIElementTypeED,
	kEHCIElementTypeiTD,
	kEHCIElementTypeSiTD
};

enum
{
    kEHCICheckForRootHubConnectionsPeriod = 5,		// Check every 5 secs to see if root hub has connections
    kEHCICheckForRootHubInactivityPeriod = 2		// Wait for 2 secs after the last time the root hub was active
};


enum{
	kMaxPorts = 15
};


// stuff to manage "extra" USB power on some machines
typedef struct AppleEHCIExtraPower
{
	UInt32			version;				// version of this structure
	UInt32			perPort;				// the amount availeable per port above and beyond the 500ma in the spec (in milliamps) [AAPL,current-extra]
	UInt32			aggregate;				// the total amount available on the controller or machine, to be allocated amoung all ports (in milliamps) [AAPL,current-available]
	UInt32			inSleep;				// total amount of current available on the port when the machine is in sleep (in milliamps) [AAPL,current-extra]
} AppleEHCIExtraPower;

enum {
	kAppleEHCIExtraPowerVersion = 0x100
};

//================================================================================================
//
//   AppleUSBEHCI_IOLockClass
//
//	 Used for locking access to shared resources between all EHCI controllers
//
//================================================================================================
//
class AppleUSBEHCI_IOLockClass
{
public:
	AppleUSBEHCI_IOLockClass(void);								// Constructor
	virtual ~AppleUSBEHCI_IOLockClass(void);					// Destructor
	
	IOLock *lock;
};

class AppleUSBEHCI : public IOUSBControllerV3
{
    friend class AppleUSBEHCIDiagnostics;
	
    OSDeclareDefaultStructors(AppleUSBEHCI)

	// Structure used for statistics for UIMs.
	typedef struct  
	{
		UInt64			lastNanosec;
		UInt64			totalBytes;
		UInt64			prevBytes;
		UInt32			acessCount;
		UInt32			totalErrors;
		UInt32			prevErrors;
		UInt32			timeouts;
		UInt32			prevTimeouts;
		UInt32			resets;
		UInt32			prevResets;
		UInt32			recoveredErrors;
		UInt32			prevRecoveredErrors;
		UInt32			errors2Strikes;
		UInt32			prevErrors2Strikes;
		UInt32			errors3Strikes;
		UInt32			prevErrors3Strikes;
	} UIMDiagnostics;
	
	
private:
 	UInt32							ExpressCardPort( IOService * provider );
	IOACPIPlatformDevice *			CopyACPIDevice( IORegistryEntry * device );
	bool							HasExpressCardUSB( IORegistryEntry * acpiDevice, UInt32 * portnum );
	
    void							showRegisters(UInt32 level, const char *s);
    void							printTD(EHCIGeneralTransferDescriptorPtr pTD, int level);
    void							printAsyncQueue(int level, const char *str);
    
    void							AddIsocFramesToSchedule(AppleEHCIIsochEndpoint*);
    IOReturn						AbortIsochEP(AppleEHCIIsochEndpoint*);
    IOReturn						DeleteIsochEP(AppleEHCIIsochEndpoint*);
	
	static AppleEHCIExtraPower		_extraPower;						// this is static as currently it is share by all machines
    
protected:
    IOMemoryMap *							_deviceBase;
    UInt16									_vendorID;
    UInt16									_deviceID;
    UInt16									_revisionID;
    UInt32									_errataBits;						// various bits for chip erratas
    EHCICapRegistersPtr						_pEHCICapRegisters;					// Capabilities registers
    EHCIRegistersPtr						_pEHCIRegisters;					// Pointer to base address of EHCI registers.
    UInt32									_dataAllocationSize;				// # of bytes allocated in for TD's
    SInt32									_greatestPeriod;					// Longest interrupt period allocated.
    EHCIGeneralTransferDescriptorPtr		_pFreeTD;							// list of availabble Trasfer Descriptors
    AppleEHCIIsochTransferDescriptor		*_pFreeITD;							// list of availabble Trasfer Descriptors
    AppleEHCISplitIsochTransferDescriptor 	*_pFreeSITD;						// list of availabble Trasfer Descriptors
    AppleEHCIQueueHead						*_pFreeQH;							// list of available Endpoint Descriptors
    EHCIGeneralTransferDescriptorPtr		_pLastFreeTD;						// last of availabble Trasfer Descriptors
    AppleEHCIIsochTransferDescriptor		*_pLastFreeITD;						// last of availabble Trasfer Descriptors
    AppleEHCISplitIsochTransferDescriptor	*_pLastFreeSITD;					// last of availabble Trasfer Descriptors
    AppleEHCIQueueHead						*_pLastFreeQH;						// last of available Endpoint Descriptors
	IOBufferMemoryDescriptor				*_periodicListBuffer;				// IOBMD for the periodic list
    USBPhysicalAddress32					*_periodicList;						// Physical interrrupt heads
    IOUSBControllerListElement				**_logicalPeriodicList;				// logical interrupt heads
	AppleEHCIQueueHead						*_dummyIntQH[kEHCIMaxPollingInterval];	// dummy interrupt queue heads
	
	// bandwidth allocation - we keep track of bytes of periodic bandwidth used in each
	// microframe of a 32 ms scheduling window (which repeats 32 times in the overall schedule)
    UInt16									_periodicBandwidthUsed[kEHCIMaxPollingInterval][kEHCIuFramesPerFrame];	// bandwidth remaining per frame
	UInt16									_controllerThinkTime;				// amount of "think time" needed as the controller goes from one QH to the next
    AppleUSBEHCIHubInfo						*_hsHubs;							// high speed hubs
	
    IOFilterInterruptEventSource *			_filterInterruptSource;
    UInt32									_filterInterruptCount;
	UInt8									_istKeepAwayFrames;					// the isochronous schedule threshold keepaway
    volatile IOUSBControllerIsochListElement *_savedDoneQueueHead;				// saved by the Filter Interrupt routine
    volatile UInt32							_producerCount;						// Counter used to synchronize reading of the done queue between filter (producer) and action (consumer)
    volatile UInt32							_consumerCount;						// Counter used to synchronize reading of the done queue between filter (producer) and action (consumer)
    volatile bool							_filterInterruptActive;				// in the filter interrupt routine
    IOSimpleLock *							_wdhLock;
    IOSimpleLock *							_isochScheduleLock;
    UInt32									_asyncAdvanceInterrupt;
    UInt32									_hostErrorInterrupt;
    UInt32									_frameRolloverInterrupt;
    UInt32									_portChangeInterrupt;
    UInt32									_errorInterrupt;
    UInt32									_completeInterrupt;
	UInt32									_controlBulkTransactionsOut;
    bool									_uimInitialized;
    bool									_testModeEnabled;
    bool									_sleepRegistersSaved;
    bool									_hasPCIPwrMgmt;
	bool									_is64bit;
	bool									_inAbortIsochEP;
	UInt8									_asynchScheduleUnsynchCount;
	UInt8									_periodicScheduleUnsynchCount;
    UInt32									_periodicEDsInSchedule;					// interrupt endpoints
    volatile UInt64							_frameNumber;							// the current frame number (high bits only)
    UInt16									_rootHubFuncAddress;					// Function Address for the root hub
    struct  {
        volatile UInt32	hostSystemError;				// updated by the interrupt handler
        volatile UInt32	unrecoverableError;				// updated by the interrupt handler
        volatile UInt32	ownershipChange;				// updated by the interrupt handler
		UInt32			displayed;
	} _errors;
    UInt32									_frameListSize;
    AppleEHCIQueueHead						*_AsyncHead;							// ptr to Control list
	
    AppleEHCIedMemoryBlock					*_edMBHead;
    AppleEHCItdMemoryBlock					*_tdMBHead;
    AppleEHCIitdMemoryBlock					*_itdMBHead;
    AppleEHCIsitdMemoryBlock				*_sitdMBHead;
    AbsoluteTime							_lastRootHubStatusChanged;				// Last time we had activity on the root hub
    UInt32									_savedUSBIntr;							// to save during sleep
    UInt32									_savedUSBCMD;							// to save during suspend/resume
    UInt32									_physPeriodicListBase;					// to save during sleep/wake on PC[I] Cards
    UInt32									_savedAsyncListAddr;					// to save during sleep/wake on PC[I] Cards
	UInt32									_savedSuspendedPortBitmap;				// to save the ports that were suspended prior to sleep
    UInt16									_outSlot;
	
    AbsoluteTime							_lastCheckedTime;						// Last time we checked the Root Hub for inactivity
	UInt32									_rhPrevStatus[kMaxPorts];				// Previous status of the root hub SC registers
	UInt32									_rhChangeBits[kMaxPorts];				// Change bits of the root hub
	bool									_rhPortBeingResumed[kMaxPorts];			// while we are outside the WL resuming a root hub port
	thread_call_t							_rhResumePortTimerThread[kMaxPorts];	// thread off the WL gate to resume a RH port
	UInt32									_ExpressCardPort;						// port number (1-based) where we have an ExpressCard connector
    IONotifier *                            _powerDownNotifier;
	bool									_gangedOvercurrent;						// True if our root hubs reports overcurrent on all ports
	bool									_badExpressCardAttached;				// True if a driver has identified a bad ExpressCard
	
	// variables to get the anchor frame
	AbsoluteTime							_tempAnchorTime;
	AbsoluteTime							_anchorTime;
	UInt64									_tempAnchorFrame;
	UInt64									_anchorFrame;

	// dealing with doneQueue reentrancy
	UInt32									_nextDoneQueue;
	struct EHCIDoneQueueParams				_doneQueueParams[20];
	
	// disabled queue heads from hubs which have gone to sleep
	AppleEHCIQueueHead *					_disabledQHList;
	
	// UIM diagnostics stuff
	OSObject *								_diagnostics;

	UIMDiagnostics							_UIMDiagnostics;
	
	
	
	
	
	// methods
    
    static void 				InterruptHandler(OSObject *owner, IOInterruptEventSource * source, int count);
    static bool 				PrimaryInterruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
	
    bool						FilterInterrupt(int index);
	
    virtual void			UIMRootHubStatusChange(void);
	virtual void			UIMRootHubStatusChange( bool abort );
	
    void	SetVendorInfo(void);
    AppleEHCIQueueHead *MakeEmptyEndPoint(UInt8 				functionAddress,
										  UInt8					endpointNumber,
										  UInt16				maxPacketSize,
										  UInt8					speed,
										  USBDeviceAddress		highSpeedHub,
										  int					highSpeedPort,
										  UInt8					direction);
	
    AppleEHCIQueueHead *MakeEmptyIntEndPoint(UInt8 					functionAddress,
											 UInt8					endpointNumber,
											 UInt16					maxPacketSize,
											 UInt8					speed,
											 USBDeviceAddress				highSpeedHub,
											 int						highSpeedPort,
											 UInt8					direction);
	
	void linkInterruptEndpoint(AppleEHCIQueueHead *pEHCIEndpointDescriptor);
	void linkAsyncEndpoint(AppleEHCIQueueHead *CBED);
	void returnTransactions(AppleEHCIQueueHead *pED, EHCIGeneralTransferDescriptor *untilThisOne, IOReturn error, bool clearToggle);
	
	IOUSBControllerListElement *GetPeriodicListLogicalEntry(int offset);
	USBPhysicalAddress32 GetPeriodicListPhysicalEntry(int offset);
	void SetPeriodicListEntry(int offset, IOUSBControllerListElement *pListElem);
	
    AppleEHCIQueueHead *AddEmptyCBEndPoint(UInt8 					functionAddress,
										   UInt8					endpointNumber,
										   UInt16					maxPacketSize,
										   UInt8					speed,
										   USBDeviceAddress			highSpeedHub,
										   int						highSpeedPort,
										   UInt8					direction);
	
    AppleEHCIQueueHead *FindInterruptEndpoint(short							functionNumber,
											  short					endpointNumber,
											  short					direction,
											  IOUSBControllerListElement			**pLEBack);
    AppleEHCIQueueHead *AllocateQH(void);
    EHCIGeneralTransferDescriptorPtr AllocateTD(void);
    AppleEHCIIsochTransferDescriptor *AllocateITD(void);
    AppleEHCISplitIsochTransferDescriptor *AllocateSITD(void);
	
    IOReturn  allocateTDs(AppleEHCIQueueHead		*pEDQueue,
						  IOUSBCommand*			command,
						  IOMemoryDescriptor *		CBP,
						  UInt32				bufferSize,
						  UInt16				direction,
						  Boolean				controlTransaction);
	
    AppleEHCIQueueHead *FindControlBulkEndpoint (short 						functionNumber, 
												 short						endpointNumber, 
												 AppleEHCIQueueHead   				**pEDBack,
												 short direction);
	
    void scavengeCompletedTransactions(IOUSBCompletionAction safeAction);
	IOReturn scavengeIsocTransactions(IOUSBCompletionAction safeAction, bool reQueueTransactions);
	IOReturn scavengeAnIsocTD(IOUSBControllerIsochListElement *pTD, IOUSBCompletionAction safeAction);
	
    IOReturn scavengeAnEndpointQueue(IOUSBControllerListElement *pEDQueue, IOUSBCompletionAction safeAction);
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
	
    IOReturn			EnterTestMode(void);
    IOReturn			PlacePortInMode(UInt32 port, UInt32 mode);
    IOReturn			LeaveTestMode(void);
	
    void				ResumeUSBBus();
    void				SuspendUSBBus();
    void				StopUSBBus();
    void				RestartUSBBus();
	
    IOReturn			AcquireOSOwnership(void);

    IOReturn			CreateHSIsochTransfer(AppleEHCIIsochEndpoint *pEP, IOUSBIsocCommand *command);

    IOReturn			CreateSplitIsochTransfer(AppleEHCIIsochEndpoint *pEP, IOUSBIsocCommand *command);
	
	void				ReturnOneTransaction(EHCIGeneralTransferDescriptor 	*transaction,
							   AppleEHCIQueueHead				*pED,
							   AppleEHCIQueueHead				*pEDBack,
							   IOReturn							err);
	
    UInt32				findBufferRemaining(AppleEHCIQueueHead *pED);
	void				UIMCheckForTimeouts(void);
	
	IOReturn			AllocateInterruptBandwidth(AppleEHCIQueueHead	*pED, AppleUSBEHCITTInfo *pTT);
	IOReturn			ReturnInterruptBandwidth(AppleEHCIQueueHead	*pED);
	
	IOReturn			AllocateIsochBandwidth(AppleEHCIIsochEndpoint	*pEP, AppleUSBEHCITTInfo *pTT);
	IOReturn			ReturnIsochBandwidth(AppleEHCIIsochEndpoint	*pEP);
	
	IOReturn			AllocateHSPeriodicSplitBandwidth(AppleUSBEHCISplitPeriodicEndpoint *pSPE);
	IOReturn			ReturnHSPeriodicSplitBandwidth(AppleUSBEHCISplitPeriodicEndpoint *pSPE);
	
	IOReturn			AdjustSPEs(AppleUSBEHCISplitPeriodicEndpoint *pSPEChanged, bool added);

	IOReturn			ReservePeriodicBandwidth(int frame, int uFrame, UInt16 bandwidth);
	IOReturn			ReleasePeriodicBandwidth(int frame, int uFrame, UInt16 bandwidth);
	IOReturn			ShowPeriodicBandwidthUsed(int level, const char *fromStr);
	
public:
	virtual bool		init(OSDictionary * propTable);
    virtual bool		start( IOService * provider );
    virtual void		free();
    virtual IOReturn 	message( UInt32 type, IOService * provider,  void * argument = 0 );
	virtual IOReturn	powerStateDidChangeTo ( IOPMPowerFlags, unsigned long, IOService* );
	virtual void		powerChangeDone ( unsigned long fromState );
	
	
    IOReturn UIMInitialize(IOService * provider);
    IOReturn UIMFinalize();
	
    IOReturn DeallocateITD (AppleEHCIIsochTransferDescriptor *pTD);
    IOReturn DeallocateSITD (AppleEHCISplitIsochTransferDescriptor *pTD);
	
	// Control
    virtual IOReturn UIMCreateControlEndpoint(UInt8				functionNumber,
											  UInt8				endpointNumber,
											  UInt16			maxPacketSize,
											  UInt8				speed);
	
    virtual IOReturn UIMCreateControlEndpoint(UInt8				functionNumber,
											  UInt8				endpointNumber,
											  UInt16			maxPacketSize,
											  UInt8				speed,
											  USBDeviceAddress  highSpeedHub,
											  int			    highSpeedPort);
	
	// method in 1.8 and 1.8.1
	virtual IOReturn UIMCreateControlTransfer(short				functionNumber,
											  short				endpointNumber,
											  IOUSBCompletion	completion,
											  void				*CBP,
											  bool				bufferRounding,
											  UInt32			bufferSize,
											  short				direction);
    //same method in 1.8.2
    virtual IOReturn UIMCreateControlTransfer(short					functionNumber,
											  short					endpointNumber,
											  IOUSBCommand*			command,
											  IOMemoryDescriptor	*CBP,
											  bool					bufferRounding,
											  UInt32				bufferSize,
											  short					direction);
    // method in 1.8 and 1.8.1
    virtual IOReturn UIMCreateControlTransfer(short					functionNumber,
											  short					endpointNumber,
											  IOUSBCompletion		completion,
											  IOMemoryDescriptor	*CBP,
											  bool					bufferRounding,
											  UInt32				bufferSize,
											  short					direction);
	
	virtual IOReturn UIMCreateControlTransfer(short				functionNumber,
											  short				endpointNumber,
											  IOUSBCommand		*command,
											  void				*CBP,
											  bool				bufferRounding,
											  UInt32			bufferSize,
											  short				direction);
	
	
    // Bulk
    virtual IOReturn UIMCreateBulkEndpoint(UInt8				functionNumber,
										   UInt8				endpointNumber,
										   UInt8				direction,
										   UInt8				speed,
										   UInt8				maxPacketSize);
	
    virtual IOReturn UIMCreateBulkEndpoint(UInt8				functionNumber,
										   UInt8				endpointNumber,
										   UInt8				direction,
										   UInt8				speed,
										   UInt16				maxPacketSize,
										   USBDeviceAddress    	highSpeedHub,
										   int					highSpeedPort);
	
	
	
    // method in 1.8 and 1.8.1
    virtual IOReturn UIMCreateBulkTransfer(short				functionNumber,
										   short				endpointNumber,
										   IOUSBCompletion		completion,
										   IOMemoryDescriptor   *CBP,
										   bool					bufferRounding,
										   UInt32				bufferSize,
										   short				direction);
	
    // same method in 1.8.2
    virtual IOReturn UIMCreateBulkTransfer(IOUSBCommand* command);
	
    // Interrupt
    virtual IOReturn UIMCreateInterruptEndpoint(short				functionAddress,
												short				endpointNumber,
												UInt8				direction,
												short				speed,
												UInt16				maxPacketSize,
												short				pollingRate);
	
    virtual IOReturn UIMCreateInterruptEndpoint(short				functionAddress,
												short				endpointNumber,
												UInt8				direction,
												short				speed,
												UInt16				maxPacketSize,
												short				pollingRate,
												USBDeviceAddress    highSpeedHub,
												int                 highSpeedPort);
	
    // method in 1.8 and 1.8.1
    virtual IOReturn UIMCreateInterruptTransfer(short				functionNumber,
												short				endpointNumber,
												IOUSBCompletion		completion,
												IOMemoryDescriptor  *CBP,
												bool				bufferRounding,
												UInt32				bufferSize,
												short				direction);
	
	
    // method in 1.8.2
    virtual IOReturn UIMCreateInterruptTransfer(IOUSBCommand* command);
	
    // Isoch
    virtual IOReturn UIMCreateIsochEndpoint(short				functionAddress,
											short				endpointNumber,
											UInt32				maxPacketSize,
											UInt8				direction);
	
    virtual IOReturn UIMCreateIsochEndpoint(short				functionAddress,
											short				endpointNumber,
											UInt32				maxPacketSize,
											UInt8				direction,
											USBDeviceAddress    highSpeedHub,
											int                 highSpeedPort);
	
    virtual IOReturn UIMCreateIsochEndpoint(short				functionAddress,
											short				endpointNumber,
											UInt32				maxPacketSize,
											UInt8				direction,
											USBDeviceAddress    highSpeedHub,
											int					highSpeedPort,
											UInt8				interval);
	
	// obsolete method
    virtual IOReturn UIMCreateIsochTransfer(short					functionAddress,
											short					endpointNumber,
											IOUSBIsocCompletion		completion,
											UInt8					direction,
											UInt64					frameStart,
											IOMemoryDescriptor		*pBuffer,
											UInt32					frameCount,
											IOUSBIsocFrame			*pFrames);
	
    virtual IOReturn UIMAbortEndpoint(short				functionNumber,
									  short				endpointNumber,
									  short				direction);
	
    virtual IOReturn UIMDeleteEndpoint(short				functionNumber,
									   short				endpointNumber,
									   short				direction);
	
    virtual IOReturn UIMClearEndpointStall(short				functionNumber,
										   short				endpointNumber,
										   short				direction);
	
    IOReturn HandleEndpointAbort(short functionNumber, short endpointNumber, short direction, bool clearToggle);
    
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
	IOReturn RHCompleteResumeOnAllPorts();
    void		RHCheckForPortResumes(void);

    IOReturn GetRootHubStringDescriptor(UInt8	index, OSData *desc);
    AbsoluteTime	LastRootHubPortStatusChanged( bool reset );
	
	// obsolete method
    virtual IOReturn 		UIMCreateIsochTransfer(short					functionAddress,
												   short					endpointNumber,
												   IOUSBIsocCompletion		completion,
												   UInt8					direction,
												   UInt64					frameStart,
												   IOMemoryDescriptor *		pBuffer,
												   UInt32					frameCount,
												   IOUSBLowLatencyIsocFrame *pFrames,
												   UInt32					updateFrequency);
	
	// new method
	virtual IOReturn		UIMCreateIsochTransfer(IOUSBIsocCommand *command);
	
    virtual IOReturn 		UIMHubMaintenance(USBDeviceAddress highSpeedHub, UInt32 highSpeedPort, UInt32 command, UInt32 flags);
    virtual IOReturn		UIMSetTestMode(UInt32 mode, UInt32 port);

    IOReturn		EnableAsyncSchedule(bool waitForON);
    IOReturn		DisableAsyncSchedule(bool waitForOFF);
    IOReturn		EnablePeriodicSchedule(bool waitForON);
    IOReturn		DisablePeriodicSchedule(bool waitForOFF);
	
    void 			CheckEDListForTimeouts(AppleEHCIQueueHead *head);
    void			GetNumberOfPorts(UInt8 *numPorts);

	virtual IOUSBControllerIsochEndpoint*			AllocateIsochEP();
    virtual void									ReturnIsochDoneQueue(IOUSBControllerIsochEndpoint*);

    virtual IOReturn								GetLowLatencyOptionsAndPhysicalMask(IOOptionBits *optionBits, mach_vm_address_t *physicalMask);
	virtual IODMACommand							*GetNewDMACommand();
	
	// this call is not gated, so we need to gate it ourselves
	virtual IOReturn								GetFrameNumberWithTime(UInt64* frameNumber, AbsoluteTime *theTime);
	// here is the gated version
	static IOReturn									GatedGetFrameNumberWithTime(OSObject *owner, void* arg0, void* arg1, void* arg2, void* arg3);
	

	// these are for handling a root hub resume without hanging out in the WL for 20 ms
	static void			RHResumePortTimerEntry(OSObject *target, thread_call_param_t port);
	void				RHResumePortTimer(UInt32 port);
	static IOReturn		RHResumePortCompletionEntry(OSObject *target, void *param1, void *param2, void *param3, void *param4);
	IOReturn			RHResumePortCompletion(UInt32 port);
	
	
	// separated this from initForPM
	void											CheckSleepCapability(void);
	
	// in the new IOUSBControllerV3 class
	virtual	IOReturn				ResetControllerState(void);
	virtual IOReturn				RestartControllerFromReset(void);
	virtual	IOReturn				SaveControllerStateForSleep(void);
	virtual	IOReturn				RestoreControllerStateFromSleep(void);
	virtual IOReturn				DozeController(void);
	virtual IOReturn				WakeControllerFromDoze(void);
	virtual IOReturn				UIMEnableAddressEndpoints(USBDeviceAddress address, bool enable);
	virtual IOReturn				UIMEnableAllEndpoints(bool enable);
	virtual IOReturn				EnableInterruptsFromController(bool enable);
	virtual	UInt32					AllocateExtraRootHubPortPower(UInt32 extraPowerRequested);
	virtual	void					ReturnExtraRootHubPortPower(UInt32 extraPowerReturned);
	
};


#endif /* _IOKIT_AppleUSBEHCI_H */
