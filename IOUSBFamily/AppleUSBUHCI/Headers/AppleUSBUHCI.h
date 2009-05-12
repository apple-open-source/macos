/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
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
 

#ifndef _IOKIT_AppleUSBUHCI_H
#define _IOKIT_AppleUSBUHCI_H

#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
#define IOASSERT 1
#include <IOKit/assert.h>
#endif

#include <libkern/OSByteOrder.h>

extern "C" {
#include <kern/queue.h>
}

#include <IOKit/IOService.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOReturn.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBHub.h>
#include <IOKit/usb/IOUSBControllerV3.h>

#include "UHCI.h"
#include "AppleUSBEHCI.h"

// forward declarations
class AppleUHCItdMemoryBlock;
class AppleUHCIqhMemoryBlock;
class AppleUHCIQueueHead;
class AppleUHCITransferDescriptor;
class AppleUHCIIsochTransferDescriptor;
class AppleUHCIIsochEndpoint;
class AppleUSBUHCI;
class AppleUSBUHCIDMACommand;

// Convert USBLog to use kprintf debugging
#ifndef UHCI_USE_KPRINTF
	#define UHCI_USE_KPRINTF 0
#endif

#if UHCI_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= UHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#ifdef __ppc__
#define IOSync eieio
#else
#define IOSync() __asm__ __volatile__ ( "mfence" : : : "memory" )
#endif

/* Whether to use vertical TD queues, or fair queues. */
#define UHCI_USE_VERTICAL_QUEUES 0

#if UHCI_USE_VERTICAL_QUEUES
#define kUHCI_VERTICAL_FLAG  (kUHCI_TD_VF)
#else
#define kUHCI_VERTICAL_FLAG  (0)
#endif

#define USB_CONSTANT16(x)	(OSSwapHostToLittleConstInt16(x))
#define MICROSECOND		(1)
#define MILLISECOND		(1000)
#define NANOSECOND_TO_MILLISECOND (1000000)

#define kUHCI_RESET_DELAY 100   /* reset takes 100ms */

/* The audio hack tries to copy unaligned data into isoc output buffers
 * just before they are needed.  Currently unimplemented.
 */
#define AUDIO_HACK 0


/* It is possible to use a shorter list of "virtual" frames to reduce the memory requirement
 * of 1024 physical frames.
 * This value should be a power of two that is less than or equal to kUHCI_FRAME_COUNT.
 * Having more virtual frames will allow queueing more isochronous frames.
 */

#define kUHCI_NVFRAMES 1024
#define kUHCI_NVFRAMES_MASK (kUHCI_NVFRAMES-1)

// we will allocate 6 interrupt queue heads, representing polling intervals of up to 32 ms
// intrQH[0] will appear in every frame list
// intrQH[1] will appear in every 2nd frame list and point to intrQH[0]
// intrQH[2] will appear in every 4th frame list and point to intrQH[1]
// etc
#define kUHCI_NINTR_QHS 6


/* Minimum frame offset for scheduling an isochronous transaction. */
enum {
    kUHCI_MIN_FRAME_OFFSET = 1
};

/* Key for identifying isoc frames that span two TDs. */
enum {
    kUHCI_ISOC_SPAN_TD = (err_local|err_sub(0x99)|0x42)
};

/* Make a structure for transactions so we can queue them.
*/

// Transaction state
enum {
    kUHCI_TP_STATE_NULL,
    kUHCI_TP_STATE_FREE,
    kUHCI_TP_STATE_ACTIVE,
    kUHCI_TP_STATE_COMPLETE,
    kUHCI_TP_STATE_ABORTED
};

// Leave room for block descriptor at end of chunk
// to round out to a nice IOMalloc allocation size.
#define kNTransactionChunk 30

/*
 * Buffers for unaligned transaction.
 * The size of the buffer is the maxPacketSize
 * of the associated endpoint.
 */
class UHCIAlignmentBuffer : public OSObject
{
	OSDeclareDefaultStructors(UHCIAlignmentBuffer)

public:
	
	enum bufferType
	{
		kTypeCBI,
		kTypeIsoch
	};
	
    IOPhysicalAddress					paddr;
    IOVirtualAddress					vaddr;
	
    IOMemoryDescriptor					*userBuffer;
    IOByteCount							userOffset;
	IOByteCount							actCount;
	AppleUSBUHCI						*controller;
	AppleUSBUHCIDMACommand				*dmaCommand;
	bufferType							type;
	
    // Queue fields
    queue_chain_t						chain;
};


class AppleUSBUHCIDMACommand : public IODMACommand
{
    OSDeclareDefaultStructors(AppleUSBUHCIDMACommand)
	
public:
	
	queue_head_t			_alignment_buffers;

	// constructor
    static AppleUSBUHCIDMACommand *		withSpecification(SegmentFunction  outSegFunc,
														  UInt8            numAddressBits,
														  UInt64           maxSegmentSize,
														  MappingOptions   mappingOptions = kMapped,
														  UInt64           maxTransferSize = 0,
														  UInt32           alignment = 1,
														  IOMapper        *mapper = 0,
														  void            *refCon = 0);
	
	// overriden methods
    virtual IOReturn		clearMemoryDescriptor(bool autoComplete = true);

};


enum {
    kUHCI_BUFFER_CBI_ALIGN_SIZE		= 64,
	kUHCI_BUFFER_ISOCH_ALIGN_SIZE	= 1024,
	kUHCI_BUFFER_ISOCH_ALIGN_QTY	= 24
};

/* Checking for idleness.
 */
enum
{
    kUHCICheckForRootHubConnectionsPeriod = 5,			// Check every 5 seconds for connections
    kUHCICheckForRootHubInactivityPeriod = 2,			// Wait 2 seconds for root hub to be idle
	kUHCITimeoutForPortRecovery = 2						// After 2 seconds, forget we applied the port recovery code
};    


/*
 * Errata bits.  Eventually this should move into the USB family
 * errata bits when it is documented properly.
 */
enum
{
    kUHCIResetAfterBabble   = 1
};


class AppleUSBUHCI : public IOUSBControllerV3
{
    OSDeclareDefaultStructors(AppleUSBUHCI)


private:
    void				ioWrite8(UInt16 offset, UInt8 value);
    void				ioWrite16(UInt16 offset, UInt16 value);
    void				ioWrite32(UInt16 offset, UInt32 value);
    UInt8				ioRead8(UInt16 offset);
    UInt16				ioRead16(UInt16 offset);
    UInt32				ioRead32(UInt16 offset);
    void				showRegisters(UInt32 level, const char *s);

    inline void Command(UInt16 cmd) {
      ioWrite16(kUHCI_CMD, cmd);
    }
    /* Port numbers for these functions are 0-based. */
    inline UInt16 ReadPortStatus(int port) {
        return ioRead16(kUHCI_PORTSC1 + (port * 2));
    }
    inline void WritePortStatus(int port, UInt16 value) {
        ioWrite16(kUHCI_PORTSC1 + (port * 2), value);
    }
    inline UInt16 ReadFrameNumberRegister(void) {
        return (ioRead16(kUHCI_FRNUM) & kUHCI_FRNUM_MASK);
    }
    inline UInt16 ReadFrameNumber(void) {
        return (ioRead16(kUHCI_FRNUM) & kUHCI_FRNUM_FRAME_MASK);
    }
    
    void SetVendorInfo(void);
    void SetDeviceName(void);



protected:
    IOMemoryMap						*_ioMap;
    IOPhysicalAddress				_ioPhysAddress;
    IOVirtualAddress				_ioVirtAddress;
	IOBufferMemoryDescriptor		*_frameListBuffer;
	IOBufferMemoryDescriptor		*_cbiAlignBuffer;
	IOBufferMemoryDescriptor		*_isochAlignBuffer;
    UInt16							_ioBase;
    UInt16							_vendorID;
    UInt16							_deviceID;
    UInt16							_revisionID;
    UInt32							_errataBits;
    int								_deviceNameLen;
    const char *					_deviceName;
    IOFilterInterruptEventSource	*_interruptSource;
    bool							_uimInitialized;
    
	queue_head_t					_cbiAlignmentBuffers;			// alignment buffers for control/bulk/interrupt (64 byte buffers)
	queue_head_t					_isochAlignmentBuffers;			// alignment buffers for isoch (1024 byte buffers)
	SInt32							_uhciAlignmentHighWaterMark;
	SInt32							_uhciAlignmentBuffersInUse;
	
    // Timeouts
    AbsoluteTime					_lastTime;
    
    /* 64-bit frame number support. */
    IOLock *						_frameLock;
    UInt32							_lastFrameNumberLow;
    UInt32							_lastFrameNumberHigh;
    
    AbsoluteTime					_lastFrameNumberTime;
    UInt64							_lastTimeoutFrameNumber;
    
    // Interrupt status bits
    UInt32							_intrStatus;
	UInt16							_hostControllerProcessInterrupt;
	UInt16							_hostSystemErrorInterrupt;
	UInt16							_resumeDetectInterrupt;
	UInt16							_usbErrorInterrupt;
	UInt16							_usbCompletionInterrupt;
    
    // Isochronous bandwidth management
    UInt32							_isocBandwidth;

    // Frame management
    IOPhysicalAddress				_framesPaddr;							// Physical frames.
    IOUSBControllerListElement		*_logicalFrameList[kUHCI_NVFRAMES];		// Virtual frame list - each of which points to a list element
	USBPhysicalAddress32				*_frameList;							// list of pointers to the shared frame list
    
	//Rollover interrupt QH+TD, this is a static 'dummy' QH that will cause the UHCI 
	//controller to generate a hardware interrupt.  It is statically inserted at 
	//the begining of the frame list (index 0). 
	AppleUHCIQueueHead				*_rolloverQH;
	AppleUHCITransferDescriptor		*_rolloverTD;
	
    // Queue management
    AppleUHCIQueueHead				*_lastQH;
    AppleUHCIQueueHead				*_bulkQHStart;
    AppleUHCIQueueHead				*_bulkQHEnd;
    AppleUHCIQueueHead				*_fsControlQHStart;
    AppleUHCIQueueHead				*_fsControlQHEnd;
    AppleUHCIQueueHead				*_lsControlQHStart;
    AppleUHCIQueueHead				*_lsControlQHEnd;
	
	// disabled Queue Head list
    AppleUHCIQueueHead				*_disabledQHList;
    
    // Interrupt queues
    AppleUHCIQueueHead					*_intrQH[kUHCI_NINTR_QHS];

	// Transfer Descriptors
	AppleUHCITransferDescriptor			*_pFreeTD;
	AppleUHCITransferDescriptor			*_pLastFreeTD;
	AppleUHCIIsochTransferDescriptor	*_pFreeITD;
	AppleUHCIIsochTransferDescriptor	*_pLastFreeITD;
    AppleUHCIqhMemoryBlock				*_qhMBHead;
    AppleUHCItdMemoryBlock				*_tdMBHead;
    
	// Queue Heads
	AppleUHCIQueueHead					*_pFreeQH;
	AppleUHCIQueueHead					*_pLastFreeQH;
	
    IOSimpleLock *						_isochScheduleLock;
    IOSimpleLock *						_wdhLock;
    UInt16								_outSlot;
	UInt32								_controlBulkTransactionsOut;

    IOReturn TDToUSBError(UInt32 error);
    void CompleteIsoc(IOUSBIsocCompletion completion, IOReturn status, void *pFrames);
    
    // Root hub support.
    UInt16								_rootFunctionNumber;
    UInt16								_lastPortStatus[kUHCI_NUM_PORTS];
    bool								_portWasReset[kUHCI_NUM_PORTS];
    bool								_portSuspendChange[kUHCI_NUM_PORTS];
    AbsoluteTime						_rhChangeTime;
	bool								_previousPortRecoveryAttempted[kUHCI_NUM_PORTS];
    AbsoluteTime						_portRecoveryTime[kUHCI_NUM_PORTS];
	bool								_rhPortBeingResumed[kUHCI_NUM_PORTS];			// while we are outside the WL resuming a root hub port
	thread_call_t						_rhResumePortTimerThread[kUHCI_NUM_PORTS];		// thread off the WL gate to resume a RH port

	// Isoch support
	volatile AppleUHCIIsochTransferDescriptor	*_savedDoneQueueHead;				// saved by the Filter Interrupt routine
    volatile UInt32								_producerCount;						// Counter used to synchronize reading of the done queue between filter (producer) and action (consumer)
    volatile UInt32								_consumerCount;						// Counter used to synchronize reading of the done queue between filter (producer) and action (consumer)
    volatile bool								_filterInterruptActive;				// in the filter interrupt routine
	bool										_inAbortIsochEP;
	
	// variables to get the anchor frame
	// used to implement GetFrameNumberWithTime
	AbsoluteTime							_tempAnchorTime;
	AbsoluteTime							_anchorTime;
	UInt64									_tempAnchorFrame;
	UInt64									_lastIrqFrame;
	UInt32									_lastIrqFrameLow;
	UInt64									_anchorFrame;

	void                            UpdateFrameNumberWithTime(void);
	UInt64                          GetFrameNumberInternal(void);

    IOReturn						RHAbortEndpoint (short endpointNumber, short direction);
    IOReturn						RHDeleteEndpoint (short endpointNumber, short direction);
    AbsoluteTime					RHLastPortStatusChanged(void);
    bool							RHAreAllPortsDisconnectedOrSuspended(void);
    void							RHCheckStatus(void);

    // Port numbers are 1-based
    void							RHEnablePort(int port, bool enable);
    IOReturn						RHSuspendPort(int port, bool suspend);
    IOReturn						RHResetPort(int port);
	IOReturn						RHHoldPortReset(int port);
    
    void							RHDumpPortStatus(int port); // Debugging.
    void							RHDumpHubPortStatus(IOUSBHubPortStatus *status);

    
    virtual IOReturn				GetRootHubStringDescriptor(UInt8	index, OSData *desc);

    void							StopEndpoint(AppleUHCIQueueHead *qh);
    void							ReturnEndpointTransactions(AppleUHCIQueueHead *qh, IOReturn status);
        
    // UIM support
    void							UIMProcessDoneQueue(IOUSBCompletionAction safeAction=0);
    virtual void					UIMRootHubStatusChange( void );
	virtual void					UIMRootHubStatusChange( bool abort );
    
    AppleUHCIQueueHead				*FindQueueHead(short functionNumber, short endpointNumber, UInt8 direction, UInt8 type, AppleUHCIQueueHead **ppQHPrev = NULL);
	IOReturn						UnlinkQueueHead(AppleUHCIQueueHead *pQH, AppleUHCIQueueHead *pQHPrev);
	
    IOReturn						DeleteEndpoint(short functionNumber, short endpointNumber, UInt8 direction);

    // Interrupt handling
    static void						InterruptHandler(OSObject *owner, IOInterruptEventSource *, int);
    static bool						PrimaryInterruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);

    bool							FilterInterrupt(void);
    void							HandleInterrupt(void);
    void							ProcessCompletedTransactions(void);
	IOReturn						scavengeIsochTransactions(void);
	IOReturn						scavengeAnIsochTD(AppleUHCIIsochTransferDescriptor *pTD);
	IOReturn						scavengeQueueHeads(IOUSBControllerListElement *);
	IOReturn						UHCIUIMDoDoneQueueProcessing(AppleUHCITransferDescriptor *pHCDoneTD, OSStatus forceErr, AppleUHCITransferDescriptor *stopAt);
    
    // Resetting
    void							GlobalReset(void);
    IOReturn						Reset(bool enableInterrupts = false);
    
    // Initializing hardware and setup
    IOReturn						HardwareInit(void);
    IOReturn						Run(bool);
	IOReturn						InitializeAlignmentBuffers(void);
    
    // Memory management
	AppleUHCITransferDescriptor			*AllocateTD(AppleUHCIQueueHead *);
    IOReturn							DeallocateTD(AppleUHCITransferDescriptor *);
	AppleUHCIIsochTransferDescriptor	*AllocateITD(void);

	AppleUHCIQueueHead					*AllocateQH(UInt16 functionNumber,
														UInt16 endpointNumber,
														UInt8 direction,
														UInt16 speed,
														UInt16 maxPacketSize,
														UInt8 type);
													   
    void									DeallocateQH(AppleUHCIQueueHead *);

    IOReturn								AllocTDChain(AppleUHCIQueueHead* pQH, IOUSBCommand *command, IOMemoryDescriptor* CBP, UInt32 bufferSize, UInt16 direction, Boolean controlTransaction);
    
    void									FreeTDChain(AppleUHCITransferDescriptor *td);


    // Debugging
    // void									DumpTransaction(UHCITransaction *tp, int level = 7);
    void									DumpTD(AppleUHCITransferDescriptor *td, int level = 7);
    void									DumpTDChain(AppleUHCITransferDescriptor *td, bool qhOK = false, int level = 7);
    void									DumpQH(AppleUHCIQueueHead *, int level = 7);
    void									DumpQHChain(AppleUHCIQueueHead *, int level = 7);
    void									DumpEndpoint(AppleUHCIQueueHead *qh, int level = 7);
    void									SingleStep(int count, bool runAfter);

	void									PrintFrameList(UInt32 slot, int level);

    /*
     * Isochronous support.
     */
    IOReturn								CreateIsochTransfer(IOUSBControllerIsochEndpoint* pEP, IOUSBIsocCommand *command);    
        
	void									AddIsochFramesToSchedule(IOUSBControllerIsochEndpoint*);
    IOReturn								AbortIsochEP(IOUSBControllerIsochEndpoint*);
    IOReturn								DeleteIsochEP(IOUSBControllerIsochEndpoint*);

	/*
     * Power management.
     */
    UInt32									_saveFrameAddress;
    UInt16									_saveFrameNumber;
	UInt16									_saveInterrupts;
    IONotifier								*_powerDownNotifier;
	UInt32									_ExpressCardPort;					// Port number of ExpressCard (0 if no ExpressCard on this controller)
	bool									_badExpressCardAttached;			// True if a driver has identified a bad ExpressCard
	
	UInt32									ExpressCardPort( IOService * provider );
    
    void									ResumeController(void);
    void									SuspendController(void);
    void									EnableUSBInterrupt(bool enableInterrupt);    
    
public:
    virtual bool							init(OSDictionary * propTable);
    virtual bool							start( IOService * provider );
    virtual bool							finalize(IOOptionBits options);
    virtual IOReturn						message( UInt32 type, IOService * provider,  void * argument = 0 );
	virtual IOReturn						powerStateWillChangeTo ( IOPMPowerFlags, unsigned long, IOService* );
	virtual IOReturn						powerStateDidChangeTo ( IOPMPowerFlags, unsigned long, IOService* );
	virtual void							powerChangeDone ( unsigned long fromState);

    /*
     * UIM methods
     */
    IOReturn								UIMInitialize(IOService * provider);
    IOReturn								UIMFinalize();

	IOReturn								DeallocateITD(AppleUHCIIsochTransferDescriptor *);

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
    virtual IOReturn					UIMCreateBulkEndpoint(	UInt8				functionNumber,
																UInt8				endpointNumber,
																UInt8				direction,
																UInt8				speed,
																UInt8				maxPacketSize);
    
    virtual IOReturn					UIMCreateBulkEndpoint(	UInt8				functionNumber,
																UInt8				endpointNumber,
																UInt8				direction,
																UInt8				speed,
																UInt16				maxPacketSize,
																USBDeviceAddress    highSpeedHub,
																int			        highSpeedPort);
    
    // method in 1.8 and 1.8.1
    virtual IOReturn					UIMCreateBulkTransfer(	short				functionNumber,
																short				endpointNumber,
																IOUSBCompletion		completion,
																IOMemoryDescriptor	*CBP,
																bool				bufferRounding,
																UInt32				bufferSize,
																short				direction);

    // same method in 1.8.2
    virtual IOReturn					UIMCreateBulkTransfer(IOUSBCommand* command);

    // Interrupt
    virtual IOReturn					UIMCreateInterruptEndpoint( short				functionAddress,
																	short				endpointNumber,
																	UInt8				direction,
																	short				speed,
																	UInt16				maxPacketSize,
																	short				pollingRate);

    virtual IOReturn					UIMCreateInterruptEndpoint( short				functionAddress,
																	short				endpointNumber,
																	UInt8				direction,
																	short               speed,
																	UInt16				maxPacketSize,
																	short				pollingRate,
																	USBDeviceAddress	highSpeedHub,
																	int                 highSpeedPort);
    
    // method in 1.8 and 1.8.1
    virtual IOReturn					UIMCreateInterruptTransfer( short				functionNumber,
																	short				endpointNumber,
																	IOUSBCompletion		completion,
																	IOMemoryDescriptor	*CBP,
																	bool				bufferRounding,
																	UInt32				bufferSize,
																	short				direction);

    // method in 1.8.2
    virtual IOReturn					UIMCreateInterruptTransfer(IOUSBCommand* command);

    // Isoch
    virtual IOReturn					UIMCreateIsochEndpoint( short				functionAddress,
																short				endpointNumber,
																UInt32				maxPacketSize,
																UInt8				direction);

    
    virtual IOReturn					UIMCreateIsochEndpoint(	short		functionAddress,
																short		endpointNumber,
																UInt32		maxPacketSize,
																UInt8		direction,
																USBDeviceAddress highSpeedHub,
																int      highSpeedPort);

	// obsolete method
    virtual IOReturn					UIMCreateIsochTransfer( short						functionAddress,
																short						endpointNumber,
																IOUSBIsocCompletion			completion,
																UInt8						direction,
																UInt64						frameStart,
																IOMemoryDescriptor			*pBuffer,
																UInt32						frameCount,
																IOUSBIsocFrame				*pFrames);

	// obsolete method
    virtual IOReturn					UIMCreateIsochTransfer(short						functionAddress,
															   short						endpointNumber,
															   IOUSBIsocCompletion			completion,
															   UInt8						direction,
															   UInt64						frameNumberStart,
															   IOMemoryDescriptor			*pBuffer,
															   UInt32						frameCount,
															   IOUSBLowLatencyIsocFrame		*pFrames,
															   UInt32						updateFrequency);
        
	// new method
	virtual IOReturn					UIMCreateIsochTransfer(IOUSBIsocCommand *command);

    virtual IOReturn UIMAbortEndpoint(short functionNumber, short endpointNumber, short direction);
    virtual IOReturn UIMDeleteEndpoint(short functionNumber, short endpointNumber, short direction);
    virtual IOReturn UIMClearEndpointStall(short functionNumber, short endpointNumber, short direction);
	
	// implementation of Abort and ClearStall
	IOReturn		HandleEndpointAbort(short functionAddress, short endpointNumber, short direction, bool clearToggle);
	
    /*
     * Root hub methods
     */
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

    virtual void PollInterrupts(IOUSBCompletionAction safeAction=0);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);
    virtual void UIMCheckForTimeouts(void);
	void  ReturnOneTransaction(AppleUHCITransferDescriptor		*pTD,
							   AppleUHCIQueueHead				*pQH,
							   AppleUHCIQueueHead				*pQHBack,
							   IOReturn							err);
    
    UInt32 findBufferRemaining(AppleUHCIQueueHead *pQH);
	
	// alignment buffers
	UHCIAlignmentBuffer *						GetCBIAlignmentBuffer();
	void										ReleaseCBIAlignmentBuffer(UHCIAlignmentBuffer*);
	UHCIAlignmentBuffer *						GetIsochAlignmentBuffer();
	void										ReleaseIsochAlignmentBuffer(UHCIAlignmentBuffer*);

	IOReturn									InitializeBufferMemory();
	void										FreeBufferMemory();

    
	virtual IOUSBControllerIsochEndpoint*			AllocateIsochEP();

    virtual IOReturn								GetLowLatencyOptionsAndPhysicalMask(IOOptionBits *optionBits, mach_vm_address_t *physicalMask);
	virtual IODMACommand							*GetNewDMACommand();
    virtual void									PutTDonDoneQueue(IOUSBControllerIsochEndpoint* pED, IOUSBControllerIsochListElement *pTD, bool checkDeferred);
	
	// this call is not gated, so we need to gate it ourselves
	virtual IOReturn								GetFrameNumberWithTime(UInt64* frameNumber, AbsoluteTime *theTime);
	// here is the gated version
	static IOReturn									GatedGetFrameNumberWithTime(OSObject *owner, void* arg0, void* arg1, void* arg2, void* arg3);

	// separated this from initForPM
	void											CheckSleepCapability(void);

	// these are for handling a root hub resume without hanging out in the WL for 20 ms
	static void			RHResumePortTimerEntry(OSObject *target, thread_call_param_t port);
	void				RHResumePortTimer(UInt32 port);
	static IOReturn		RHResumePortCompletionEntry(OSObject *target, void *param1, void *param2, void *param3, void *param4);
	IOReturn			RHResumePortCompletion(UInt32 port);

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
};

#endif /* ! _IOKIT_AppleUSBUHCI_H */
