/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/usb/IOUSBControllerV2.h>

#include "UHCI.h"

/* Convert USBLog to use kprintf debugging */
#define UHCI_USE_KPRINTF 0

#if UHCI_USE_KPRINTF
#undef USBLog
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= 2) { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#ifdef __ppc__
#define IOSync eieio
#else
#define IOSync()
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

/* Software queue head structure.
 */
typedef struct AppleUHCIQH {
    struct UHCIQH       hw;         /* Hardware structure; must come first. */
    IOPhysicalAddress   paddr;      /* Physical address of this structure. */
    int                 vframe;     /* Virtual frame location of this QH, if any. */
    struct AppleUHCIQH *hlink;
    struct AppleUHCITD *elink;
} QH;

/* Software transaction descriptor structure.
 */
typedef struct AppleUHCITD {
    struct UHCITD       hw;         /* Hardware structure; must come first. */
    IOPhysicalAddress   paddr;      /* Physical address of this structure. */
    struct UHCIAlignmentBuffer  *buffer;     /* Buffer for unaligned transactions. */
    IOUSBLowLatencyIsocFrame *fllp; /* Pointer to low-latency isoc frame for this TD. */
    struct AppleUHCITD *link;
} TD;

/* It is possible to use a shorter list of "virtual" frames to reduce the memory requirement
 * of 1024 physical frames.
 * This value should be a power of two that is less than or equal to kUHCI_FRAME_COUNT.
 * Having more virtual frames will allow queueing more isochronous frames.
 */

#define kUHCI_NVFRAMES 1024
/* 2 ^ (kUHCI_NINTR_QHS - 1) == kUHCI_NVFRAMES */
#define kUHCI_NINTR_QHS 11

#define kUHCI_NVFRAMES_MASK (kUHCI_NVFRAMES-1)

/* Minimum frame offset for scheduling an isochronous transaction. */
enum {
    kUHCI_MIN_FRAME_OFFSET = 1
};

/* Key for identifying isoc frames that span two TDs. */
enum {
    kUHCI_ISOC_SPAN_TD = (err_local|err_sub(0x99)|0x42)
};


struct VirtualFrame {
    TD *td;
    QH *first_qh;
    QH *last_qh;
    int numIntr;
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

typedef struct UHCITransaction {
    IOMemoryDescriptor *    buf;
    UInt32                  bufLen;
    int                     nCompletions;
    IOUSBCompletion         completion;
    IOUSBCommand *          command;
    QH *                    qh;
    TD *                    first_td;
    TD *                    last_td;
    int                     state;
    int                     type;
    AbsoluteTime            timestamp;
    UInt32                  timeout;    // in ms
    
    /* Isochronous support */
    IOUSBIsocCompletion     isoc_completion;
    bool                    isoc_low_latency;
    bool                    isoc_unaligned;
    void *                  isoc_frames;
    UInt32                  isoc_num_frames;
    UInt32                  isoc_start_frame;
    IOMemoryMap *           isoc_map;
    
    UInt64                  isoc_full_start_frame;
    UInt64                  isoc_request_received;
    
    struct UHCIEndpoint *   endpoint;
    
    queue_chain_t           active_chain;     // Chain of all active transactions.
    queue_chain_t           endpoint_chain;   // Chain of active transactions for this endpoint.
} UHCITransaction;

// Leave room for block descriptor at end of chunk
// to round out to a nice IOMalloc allocation size.
#define kNTransactionChunk 30

typedef struct UHCIEndpoint {
    UInt16				functionNumber;
    UInt16				endpointNumber;
    UInt16				direction;
    UInt16                              speed;
    UInt16				maxPacketSize;
    UInt16                              pollingRate;  // For interrupt endpoints.
    UInt32                              type;         // Control, interrupt, etc.
    bool                                stalled;
    
    bool                                lastDBit;
    AbsoluteTime                        timestamp;
    
    /* Location after which to queue transactions. */
    QH *                                head_qh;

    /* Support for isochronous endpoints. */
    TD **                               isoc_tds;
    
    TD *                                firstTD;      // Request queue.
    TD *                                lastTD;
    
    queue_head_t                        pendingTransactions;
    queue_head_t                        activeTransactions; // Chain of active transactions
                                                      // for this endpoint.

    queue_head_t                        freeBuffers;
    int                                 buffersInUse;
    UInt16                              maxBufferSize; // Used for isoc endpoints
    queue_head_t                        allocatedBuffers; // Data blocks for buffering
                                                      // unaligned transactions.
    
    queue_chain_t                       chain;        // Chain of all endpoints.
} UHCIEndpoint;

/*
 * Buffers for unaligned transaction.
 * The size of the buffer is the maxPacketSize
 * of the associated endpoint.
 */
typedef struct UHCIAlignmentBuffer {
    IOPhysicalAddress       paddr;
    IOVirtualAddress        vaddr;
    /* For standard transfers */
    IOMemoryDescriptor *    userBuffer;
    IOByteCount             userOffset;
    /* For isochronous transfers */
    IOVirtualAddress        userAddr;
    IOByteCount             userLength;
    /* Fields used by filter interrupt routine */
    UHCIAlignmentBuffer     *next;
    UInt32                  frameNumber;
    UInt32                  copyAtInterruptTime;
    /* Queue fields */
    queue_chain_t           chain;
} UHCIAlignmentBuffer;

enum {
    kUHCI_BUFFER_ALIGN = 4
};

/* Checking for idleness.
 */
enum
{
    kUHCICheckForRootHubConnectionsPeriod = 30000,  // Check every 30 seconds for connections
    kUHCICheckForRootHubInactivityPeriod = 30000    // Wait 30 seconds for root hub to be idle
};    

/*
 * Power management.
 */

enum
{
    kUHCIPowerLevelSuspend		= 0,
    kUHCIPowerLevelRunning		= 1,
    kUHCIPowerLevelIdleSuspend          = 2
};

/*
 * Errata bits.  Eventually this should move into the USB family
 * errata bits when it is documented properly.
 */
enum
{
    kUHCIResetAfterBabble   = 1
};


class AppleUSBUHCI : public IOUSBControllerV2
{
    OSDeclareDefaultStructors(AppleUSBUHCI)


private:
    void ioWrite8(UInt16 offset, UInt8 value);
    void ioWrite16(UInt16 offset, UInt16 value);
    void ioWrite32(UInt16 offset, UInt32 value);
    UInt8 ioRead8(UInt16 offset);
    UInt16 ioRead16(UInt16 offset);
    UInt32 ioRead32(UInt16 offset);

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
    IOPCIDevice *       _device;
    IOMemoryMap *       _ioMap;
    IOPhysicalAddress   _ioPhysAddress;
    IOVirtualAddress    _ioVirtAddress;
    UInt16              _ioBase;
    UInt16              _vendorID;
    UInt16              _deviceID;
    UInt16              _revisionID;
    UInt32              _errataBits;
    int                 _deviceNameLen;
    const char *        _deviceName;
    IOFilterInterruptEventSource *_interruptSource;
    bool		_uimInitialized;
    
    /* Allocation of QHs and TDs */
    QH *                _freeQHs;
    TD *                _freeTDs;
//    AllocatedBlock *    _allocatedBlocks;
    queue_head_t        _allocatedBuffers;
    
    /* Timeouts. */
    AbsoluteTime        _lastTime;
    
    /* 64-bit frame number support. */
    IOLock *            _frameLock;
    UInt32              _lastFrameNumberLow;
    UInt32              _lastFrameNumberHigh;
    
    AbsoluteTime        _lastFrameNumberTime;
    UInt64              _lastTimeoutFrameNumber;
    
    /* Copying buffers at interrupt time. */
    UHCIAlignmentBuffer * _interruptAlignmentBuffers;

    /* Isochronous bandwidth management. */
    UInt32              _isocBandwidth;

    /* Frame management. */
    IOPhysicalAddress   _framesPaddr;  // Physical frames.
    struct VirtualFrame _vframes[kUHCI_NVFRAMES]; // Virtual frame list.
    
    /* Queue management. */
    QH * _lastQH;
    QH * _bulkQHStart;
    QH * _bulkQHEnd;
    QH * _hsControlQHStart;
    QH * _hsControlQHEnd;
    QH * _lsControlQHStart;
    QH * _lsControlQHEnd;
    
    /* Interrupt queues. */
    QH * _intrQH[kUHCI_NINTR_QHS];

    QH * FindPrevQH(QH *head, QH *qh);
    
    void QueueInterrupt(UHCITransaction *);
    void RemoveInterrupt(UHCITransaction *);
    
    void QueueControl(UHCITransaction *);
    void RemoveControl(UHCITransaction *);

    void QueueBulk(UHCITransaction *);
    void RemoveBulk(UHCITransaction *);
    
    void AddInterruptQH(QH *);
    void RemoveInterruptQH(QH *);
    
    /* Endpoint management. */
    queue_head_t _endpoints;
    
    /* In-process transaction management. */
    queue_head_t _freeTransactions;
    queue_head_t _activeTransactions;
    
    void StartTransaction(UHCITransaction *tp);
    void HWCompleteTransaction(UHCITransaction *tp);
    void CompleteTransaction(UHCITransaction *tp, IOReturn returnCode);

    IOReturn TDToUSBError(UInt32 error);
    void CompleteIsoc(IOUSBIsocCompletion   completion,
                      IOReturn              status,
                      void *                pFrames);
    
    /* Root hub support. */
    IOTimerEventSource * _rhTimer;
    UInt16 _rootFunctionNumber;
    UInt16 _lastPortStatus[kUHCI_NUM_PORTS];
    bool   _portWasReset[kUHCI_NUM_PORTS];
    bool   _portSuspendChange[kUHCI_NUM_PORTS];
    queue_head_t _rhIntrTransactions;
    UHCIEndpoint *_rhEndpoint;
    AbsoluteTime _rhChangeTime;

    IOReturn RHAbortEndpoint (short endpointNumber, short direction);
    IOReturn RHDeleteEndpoint (short endpointNumber, short direction);
    IOReturn RHCreateInterruptTransfer(IOUSBCommand * command);
    IOReturn RHCreateInterruptEndpoint(
                                       short				endpointNumber,
                                       UInt8				direction,
                                       short				speed,
                                       UInt16				maxPacketSize,
                                       short				pollingRate);
    static void RHTimerFired(OSObject *owner, IOTimerEventSource *sender);
    AbsoluteTime	RHLastPortStatusChanged(void);
    bool		RHAreAllPortsDisconnected(void);
    void                RHCheckStatus(void);

    /* Port numbers are 1-based. */
    void     RHEnablePort(int port, bool enable);
    IOReturn RHSuspendPort(int port, bool suspend);
    IOReturn RHResetPort(int port);
    
    void     RHDumpPortStatus(int port); // Debugging.
    void     RHDumpHubPortStatus(IOUSBHubPortStatus *status);

    
    virtual IOReturn GetRootHubStringDescriptor(UInt8	index, OSData *desc);

    void    ReturnEndpointTransactions(UHCIEndpoint *ep, IOReturn status);
        
    /* UIM support. */
    void UIMProcessDoneQueue(IOUSBCompletionAction safeAction=0);
    void UIMRootHubStatusChange( void );
    void UIMRootHubStatusChange(bool abort);
    
    UHCIEndpoint * FindEndpoint(IOUSBCommand *command);
    UHCIEndpoint * FindEndpoint(short functionNumber, short endpointNumber, UInt8 direction);
    IOReturn DeleteEndpoint(short functionNumber, short endpointNumber, UInt8 direction);

    /* Interrupt handling. */
    static void InterruptHandler(OSObject *owner,
                                 IOInterruptEventSource * /*source*/,
                                 int /*count*/);
    static bool PrimaryInterruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);

    bool FilterInterrupt(void);
    void HandleInterrupt(void);
    bool HandleShortPacket(UHCITransaction *tp, TD *td);
    bool IsTransactionComplete(UHCITransaction *tp);
    int ProcessCompletedTransactions(void);
    
    /* Resetting. */
    void GlobalReset(void);
    IOReturn Reset(bool enableInterrupts = false);
    
    /* Initializing hardware and setup. */
    IOReturn HardwareInit(void);
    IOReturn Run(bool);
    
    /* Memory management. */
    struct AppleUHCITD * AllocTD(void);
    void FreeTD(struct AppleUHCITD *);
    struct AppleUHCIQH * AllocQH(void);
    void FreeQH(struct AppleUHCIQH *);
    UHCITransaction * AllocTransaction(UHCIEndpoint *);
    void FreeTransaction(UHCITransaction *);
    UHCIEndpoint * AllocEndpoint(void);
    UHCIEndpoint * AllocEndpoint(UInt16 functionNumber,
                                 UInt16 endpointNumber,
                                 UInt16 direction,
                                 UInt16 speed,
                                 UInt16 maxPacketSize,
                                 UInt32 type);
    
    void FreeEndpoint(UHCIEndpoint *);
    UHCIAlignmentBuffer * EndpointAllocBuffer(UHCIEndpoint *ep);
    void EndpointFreeBuffer(UHCIEndpoint *ep, UHCIAlignmentBuffer *bp);
    IOReturn EndpointFreeAllBuffers(UHCIEndpoint *ep);


    IOReturn AllocTDChain(UHCIEndpoint *ep,
                          IOMemoryDescriptor *mp,
                          IOByteCount len,
                          bool shortOK,
                          short direction,
                          TD **start_p, TD **end_p,
                          bool setVFlag = false,
                          bool isControlTranser = false);
    
    void FreeTDChain(TD *td);


    /* Debugging. */
    void DumpTransaction(UHCITransaction *tp, int level = 7);
    void DumpTD(TD *td, int level = 7);
    void DumpTDChain(TD *td, bool qhOK = false, int level = 7);
    void DumpQH(QH *, int level = 7);
    void DumpQHChain(QH *, int level = 7);
    void DumpFrame(UInt16 frame = 0, int level = 7);
    void SingleStep(int count, bool runAfter);


    /*
     * Isochronous support.
     */
    IOReturn CreateIsochTransfer(
                                 short				functionAddress,
                                 short				endpointNumber,
                                 IOUSBIsocCompletion		completion,
                                 UInt8				direction,
                                 UInt64				frameStart,
                                 IOMemoryDescriptor *		pBuffer,
                                 UInt32				frameCount,
                                 void * 			pFrames,
                                 UInt32                         updateFrequency,
                                 bool                           isLowLatency);
    
    IOReturn StartIsochTransfer(
                                         UHCITransaction *tp
                                         );
        
    /*
     * Power management.
     */
    bool _unloadUIMAcrossSleep;
    UInt32 _saveFrameAddress;
    UInt16 _saveFrameNumber;
    bool _remoteWakeupOccurred;
    int _powerLevel;
    virtual void initForPM (IOPCIDevice *provider);
    unsigned long maxCapabilityForDomainState ( IOPMPowerFlags domainState );
    unsigned long initialPowerStateForDomainState ( IOPMPowerFlags domainState );
    
    virtual IOReturn setPowerState( unsigned long powerStateOrdinal, IOService* whatDevice );
    
    void ResumeController(void);
    void SuspendController(void);
    void StopController(void);
    void RestartController(void);
    
    
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
            short                               speed,
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

    
    virtual IOReturn 		UIMCreateIsochEndpoint(
            short		functionAddress,
            short		endpointNumber,
            UInt32		maxPacketSize,
            UInt8		direction,
            USBDeviceAddress highSpeedHub,
            int      highSpeedPort);

    virtual IOReturn UIMCreateIsochTransfer(
	short				functionAddress,
	short				endpointNumber,
	IOUSBIsocCompletion		completion,
	UInt8				direction,
	UInt64				frameStart,
	IOMemoryDescriptor *		pBuffer,
	UInt32				frameCount,
	IOUSBIsocFrame			*pFrames);

    virtual IOReturn 
        UIMCreateIsochTransfer(
                               short				functionAddress,
                               short				endpointNumber,
                               IOUSBIsocCompletion			completion,
                               UInt8				direction,
                               UInt64				frameNumberStart,
                               IOMemoryDescriptor *		pBuffer,
                               UInt32				frameCount,
                               IOUSBLowLatencyIsocFrame		*pFrames,
                               UInt32				updateFrequency);
        
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
    
    
    

};

#endif /* ! _IOKIT_AppleUSBUHCI_H */
