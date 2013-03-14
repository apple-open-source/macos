/*
 *  AppleUSBXHCIUIM.h
 *
 *  Copyright © 2011-2012 Apple Inc. All Rights Reserved. 
 *
 */

#ifndef _APPLEUSBXHCIUIM_H_
#define _APPLEUSBXHCIUIM_H_


//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <IOKit/IOFilterInterruptEventSource.h>

#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/USBTracepoints.h>

#include "AppleUSBXHCI_IsocQueues.h"
#include "AppleUSBXHCI_RootHub.h"
#include "XHCI.h"


#define kMaxImmediateTRBTransferSize        8        
#define kInvalidImmediateTRBTransferSize    0xFF        

#include "AppleUSBXHCI_AsyncQueues.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#ifdef __ppc__
#define IOSync eieio
#else
#define IOSync() __asm__ __volatile__ ( "mfence" : : : "memory" )
#endif

// Define this to check read buffer for usage
#define DEBUG_BUFFER (0)
// Define this to print out isoc completions, not compatible with DEBUG_BUFFER
#define DEBUG_ISOC (0)
// Define this to check which register returns invalid value
#define DEBUG_REGISTER_READS (0)

// Define this to intercept completions
#define DEBUG_COMPLETIONS (0)

#define XHCIInvalidRegisterValue        0xFFFFFFFF				// if a register returns this value, the controller is likely gone

#define kNoStreamID                     0
#define kXHCIIsochMaxBusStall			25000

#define INITIAL_TRANSFER_RING_PAGES (1)


typedef IOUSBCommand *IOUSBCommandPtr;
typedef IOUSBIsocCommand *IOUSBIsocCommandPtr;
typedef void *voidPtr;


struct ringStruct
{
	IOBufferMemoryDescriptor   *TRBBuffer;					  // Also use as test for ring existance
	TRB                        *transferRing;
	USBPhysicalAddress64		transferRingPhys;
	UInt16						transferRingSize;
    UInt16						transferRingPages;
	UInt32						transferRingPCS;
	volatile UInt16				transferRingEnqueueIdx;
	volatile UInt16				transferRingDequeueIdx;
	UInt16						lastSeenDequeIdx;
	UInt32						lastSeenFrame;
	TRB							stopTRB;
	UInt64						nextIsocFrame;			  // For Isoc endpoints only
    UInt8                       endpointType;             // cntrl, bulk, isoc, int
	void                       *pEndpoint;                // Type AppleXHCIIsochEndpoint or AppleXHCIAsyncEndpoint
    UInt8						slotID;
    UInt8						endpointID;
	bool                        beingReturned;
	bool                        beingDeleted;
    bool						needsDoorbell;
};
typedef struct ringStruct
XHCIRing,
*ringPtr;


struct slotStruct
{
	IOBufferMemoryDescriptor *	buffer;
	Context	*					deviceContext;
	Context64 *					deviceContext64;
	USBPhysicalAddress64		deviceContextPhys;
	UInt32						potentialStreams[kXHCI_Num_Contexts];     // How many streams the endpoint could support
	UInt32						maxStream[kXHCI_Num_Contexts];            // How many streams the endpoint is configured for
	XHCIRing *					rings[kXHCI_Num_Contexts];
    bool 						deviceNeedsReset;
};
typedef struct slotStruct
slot,
*slotPtr;

#define MAKEXHCIERR(CC) (-1000-CC)



enum 
{
	// If this errata bit is set don't rely on Event Data TRBs.
	kXHCIErrata_NoEDs = kXHCIBit0,
	// If this errate bit is set, don't use MSI interrupts.
	kXHCIErrata_NoMSI = kXHCIBit1,
	// If this bit is set there is a Panther point style mux to switch between EHCI/XHCI
	kXHCIErrataPPT = kXHCIBit2,

    // NEC Controller, check the firmware
	kXHCIErrata_NEC = kXHCIBit3,

    kXHCIErrata_EnableAutoCompliance = kXHCIBit4,

    kXHCIErrataPPTMux  = kXHCIBit5,
	
	kXHCIErrata_Rensas = kXHCIBit6,
	
	kXHCIErrata_FrescoLogic = kXHCIBit7,
    
    kXHCIErrata_ParkRing = kXHCIBit8,
	
	kXHCIErrata_FL1100_Ax = kXHCIBit9,
	    
	kXHCIErrata_ASMedia = kXHCIBit10,
    
    kXHCI_MIN_NEC = 0x3028,
	
#if DEBUG_BUFFER
	// These for debugging buffer
	kXHCI_ScratchMark           = 0,
	kXHCI_ScratchMMap           = 1,
	kXHCI_ScratchPat            = 2,
#else
	kXHCI_ScratchdbFrIndex      = 0,
#endif
    
	// These for timeouts
	kXHCI_ScratchStopDeq        = 3,
	kXHCI_ScratchBytes          = 4,
	
	kXHCI_ScratchFirstSeen      = 5,
	kXHCI_ScratchTRTime         = 6,

	// These for Isoc completions
	kXHCI_ScratchFrIndex        = 7,
	kXHCI_ScratchLastTRB        = 8,
	
	// For kXHCIErrata_NoEDs
	kXHCI_ScratchShortfall      = 9,
};

// XHCI config registers to use xMux
enum {
    kXHCI_XUSB2PR			= 0xD0, // set in UIMInitialize
    kXHCI_XUSB2PRM			= 0xD4,	// set in EFI to 0xf
	kXHCI_XUSB3_PSSEN		= 0xD8, // set in UIMInitialize
	kXHCI_XUSB3PRM			= 0xDC  // set in EFI to 0xf
};

enum{
	kMaxSavePortStatus = 16,
	kMaxPorts = 15,
	kMaxSlots = 256,
	kMaxDevices = 128,
	kMaxStreamsPerEndpoint = 256,
    kMaxInterrupters = 16,
    kPrimaryInterrupter = 0,
    kTransferInterrupter = 1,
    
    // Tuning parameter, try to close a fragment  
    // if it uses more than this many TRBs.
	kMaxEndpointsPerDevice  = 31,
    
    kEntriesInEventRingSegmentTable = 1,
    kInterruptModerationInterval = 160,
};


#define kMaxXHCIControllerEndpoints		(kMaxEndpointsPerDevice * kMaxSlots)

#define kMaxHCPortMethods       15
#define kHCPortMethodNameLen    5

// These methods come from Xhci.asl file from EFI
const char xhciMuxedPorts[kMaxHCPortMethods][kHCPortMethodNameLen] = {"XHCA","XHCB","XHCC","XHCD"};

enum 
{
	kControllerEHCI	= 0x00,
	kControllerXHCI	= 0x01
};

enum 
{
	kACPIParamaterCount = 0x01
};

class AppleUSBXHCI;
class AppleXHCIAsyncEndpoint;

typedef void (*CMDComplete)(AppleUSBXHCI*, TRB *, SInt32 *);

typedef struct XHCICommandCompletion
{
	CMDComplete		completionAction;
	SInt32			parameter;
} XHCICommandCompletion;

typedef struct XHCIInterrupter
{
    // Integers

	// Indices, etc used by filter keep these together
    UInt16									EventRingDequeueIdx;    // Dequeue pointer for hardware ring
 	volatile UInt16							EventRing2DequeueIdx;   // Dequeue pointer for software ring
	volatile UInt16							EventRing2EnqueueIdx;   // Enqueue pointer for software ring
	UInt8									EventRingCCS;           // consumer cycle state for hardware ring, only LSB used.
    bool                                    EventRingDPNeedsUpdate; // Need to update deque pointer for hardware ring

	UInt16									numEvents;              // Number of Event TRBS in hardware event ring
	UInt16									numEvents2;             // Number of Event TRBS in software event ring
	
	volatile SInt32							EventRing2Overflows;    // Count of overflows copying to software ring

    // Pointers. (32/64 bits each)
    
	TRB										*EventRing;             // logical pointer for hardware ring
	TRB                                     *EventRing2;            // logical pointer to software ring
	USBPhysicalAddress64					EventRingPhys;          // physical pointer to event ring for hardware ring
	USBPhysicalAddress64					EventRingSegTablePhys;  // physical pointer seg table for hardware ring
    IOBufferMemoryDescriptor                *EventRingBuffer;       // IOMem buffer for hardware ring 
	

#if __LP64__
    UInt64                                  Pad1;
#else    
    UInt32                                  Pad1;
    UInt32                                  Pad2;
    UInt32                                  Pad3;
    UInt32                                  Pad4;
    UInt32                                  Pad5;
#endif
}
XHCIInterrupter __attribute__((aligned(64)));

OSCompileAssert ( sizeof ( XHCIInterrupter ) == 64 );

class AppleUSBXHCI : public IOUSBControllerV3
{
    
    OSDeclareDefaultStructors(AppleUSBXHCI)
	
	friend	class AppleXHCIIsochTransferDescriptor;
    friend  class AppleXHCIAsyncEndpoint;
    
protected:
    IOMemoryMap								*_deviceBase;
    UInt16									_vendorID;
    UInt16									_deviceID;
    UInt16									_revisionID;
    UInt32									_errataBits;						// various bits for chip erratas
	XHCICapRegistersPtr						_pXHCICapRegisters;					// Capabilities registers
    XHCIRegistersPtr						_pXHCIRegisters;					// Pointer to base address of XHCI registers.
	XHCIRunTimeRegPtr						_pXHCIRuntimeReg;					// Pointer to base of runtime regs
	XHCIXECPRegistersPtr					_pXHCIXECPRegistersBase;
	UInt32									*_pXHCIDoorbells;
    UInt32                                  _maxPrimaryStreams;
    UInt16									_rootHubFuncAddressSS;				// Function Address for the XHCI Root hub SS
    UInt16									_rootHubFuncAddressHS;				// Function Address for the XHCI Root hub HS
    IOFilterInterruptEventSource			*_filterInterruptSource;
    static void								InterruptHandler(OSObject *owner, IOInterruptEventSource * source, int count);
    static bool								PrimaryInterruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
    bool									FilterInterrupt(int index);
	
	UInt16									_numDeviceSlots;					// Number of device slots this XHCI supports
	IOBufferMemoryDescriptor				*_DCBAABuffer;						// IOMem buffer for the device context array
	USBPhysicalAddress64					*_DCBAA;							// logical pointer to DCBAA
	USBPhysicalAddress64					_DCBAAPhys;							// physical pointer to DCBAA
    
	// For the command ring
	UInt16									_numCMDs;							// Number of CMD TRBS in CMD ring
	IOBufferMemoryDescriptor				*_CMDRingBuffer;					// IOMem buffer 
	TRB										*_CMDRing;							// logical pointer 
	USBPhysicalAddress64					_CMDRingPhys;						// physical pointer
	UInt16									_CMDRingEnqueueIdx;					// Enqueue index
	UInt16									_CMDRingDequeueIdx;
	UInt32									_CMDRingPCS;						// producer cycle state
	XHCICommandCompletion *					_CMDCompletions;
	
	// For the Event ring
	UInt16									_ERSTMax;                           // max nuumber of Event TRBS in primary event ring
	UInt16									_MaxInterrupters;                   // max nuumber MSI (or MSI-X) interrupters.
    XHCIInterrupter                         _events[kMaxInterrupters];
    Interrupter								_savedInterrupter[kMaxInterrupters];// Save space for the hardware registers

	// Other bad events the primary filter saw
	volatile SInt32							_DebugFlag;
	volatile SInt32							_CCEPhysZero;
	volatile SInt32							_CCEBadIndex;
	volatile SInt32							_EventChanged;
	volatile SInt32							_IsocProblem;
    
	// For the input context
	UInt32									_inputContextLock;
	IOBufferMemoryDescriptor				*_inputContextBuffer;
	Context									*_inputContext;
    Context64								*_inputContext64;
	USBPhysicalAddress64					_inputContextPhys;
    
	
	// Scratchpad buffers
	UInt16									_numScratchpadBufs;
	IOBufferMemoryDescriptor				*_SBABuffer;						// IOMem buffer for the Scratchpad Buffer Array
	USBPhysicalAddress64					*_SBA;								// logical pointer to SBA
	USBPhysicalAddress64					_SBAPhys;							// physical pointer to SBA
	OSArray									*_ScratchPadBuffs;				
	
	// Other stuff
	
	slot									_slots[kMaxSlots];
	UInt8									_devHub[kMaxDevices];
	UInt8									_devPort[kMaxDevices];
	UInt8									_devMapping[kMaxDevices];
	bool									_devEnabled[kMaxDevices];
	UInt16									_devZeroPort;						// Port dev zero is attached to
	UInt16									_devZeroHub;						// Hub (controller's ID for) dev zero is attached to
	bool									_fakedSetaddress;
	volatile SInt16							_configuredEndpointCount;
	SInt16									_maxControllerEndpoints;			// the max number of endpoints that this controller
    // can handle.
	bool                                    _uimInitialized;
    volatile bool							_filterInterruptActive;				// in the filter interrupt routine
	volatile UInt64							_frameNumber64;
	UInt8									_istKeepAwayFrames;					// the isochronous schedule threshold keepaway
	UInt32									_numInterrupts;
	UInt32									_numPrimaryInterrupts;
	UInt32									_numInactiveInterrupts;
	UInt32									_numUnavailableInterrupts;
	
	XHCIRegisters							_savedRegisters;
	bool									_stateSaved;
	bool									_synthesizeCSC[kMaxPorts];		
	bool									_prevSuspend[kMaxPorts];
	bool									_suspendChangeBits[kMaxPorts];
	bool									_rhPortBeingResumed[kMaxPorts];			// while we are outside the WL resuming a root hub port
    bool                                    _rhPortBeingReset[kMaxPorts];           // while we are outside the WL resetting a root hub port
	thread_call_t							_rhResumePortTimerThread[kMaxPorts];	// thread off the WL gate to resume a RH port
    thread_call_t                           _rhResetPortThread[kMaxPorts];          // thread off the WL gate to reset a RH port
    XHCIRootHubResetParams                  _rhResetParams[kMaxPorts];              // Used to pass information to the callout thread and back
    bool                                    _portIsDebouncing[kMaxPorts];           // Indicates that the port is being debounced
	bool									_debouncingADisconnect[kMaxPorts];		// If true, we are debouncing a disconnect.  If false, we are debouncing a connection
	UInt64                                  _debounceNanoSeconds[kMaxPorts];        // Timestamp for when the port started being debounced
	bool                                    _portIsWaitingForWRC[kMaxPorts];        // Indicates that the port is waiting for a WRC change to complete
   
	bool									_hasPCIPwrMgmt;
	UInt32									_ExpressCardPort;						// port number (1-based) where we have an ExpressCard connector
	bool									_badExpressCardAttached;				// True if a driver has identified a bad ExpressCard
    
	volatile UInt32							_debugCtr;
	volatile UInt32							_debugPattern;
	
	UInt16									_saveStatus[kMaxSavePortStatus];
	UInt16									_saveChange[kMaxSavePortStatus];
    
    SInt16                                   _contextInUse;
    
    UInt16                                  _NECControllerVersion;
    
    bool                                    _AC64;
    bool                                    _Contexts64;
    
	IOACPIPlatformDevice					*_acpiDevice;
	bool									_muxedPorts;
    bool                                    _discoveredMuxedPorts;
    bool									_testModeEnabled;
	UInt32                                  *_pXHCIPPTChickenBits;
    
    IOSimpleLock *							_isochScheduleLock;					// used to disable preemption during isoch scheduling
    
    // variables to get the anchor frame
	AbsoluteTime							_tempAnchorTime;
	AbsoluteTime							_anchorTime;
	UInt64									_tempAnchorFrame;
	UInt64									_anchorFrame;
    bool                                    _waitForCommandRingStoppedEvent;
	bool									_lostRegisterAccess;
    
    bool                                    _HSEReported;
    IOBufferMemoryDescriptor               *_DummyBuffer;
    USBPhysicalAddress64                    _DummyRingPhys;
    UInt32                                  _DummyRingCycleBit;
    
private:
    // These methods come from Xhci.asl file from EFI
    char                                    ehciMuxedPorts[kMaxHCPortMethods][kHCPortMethodNameLen];
    
private:
    
	static int DiffTRBIndex(USBPhysicalAddress64 t1, USBPhysicalAddress64 t2);
	
	SInt32 MakeXHCIErrCode(int CC){return(MAKEXHCIERR(CC));}
	XHCIRing *GetRing(int slotID, int endpointID, UInt32 stream);
	XHCIRing *CreateRing(int slotID, int endpointID, UInt32 maxStream);
	XHCIRing *FindStream(int slotID, int endpointID, USBPhysicalAddress64 phys, int *index, bool quiet);
	void SetVendorInfo(void);
	UInt32 GetErrataBits(UInt16 vendorID, UInt16 deviceID, UInt16 revisionID);
	IOReturn MakeBuffer(IOOptionBits options, 
						mach_vm_size_t size, 
						mach_vm_address_t mask, 
						IOBufferMemoryDescriptor **buffer, 
						void **logical, 
						USBPhysicalAddress64 *physical);
	IOReturn AllocStreamsContextArray(XHCIRing *ringX, UInt32 maxStream);
	IOReturn AllocRing(XHCIRing *ringX, int size_in_pages=INITIAL_TRANSFER_RING_PAGES);
    void DeallocRing(XHCIRing *ring);
    void ParkRing(XHCIRing *ring);
#if 0
    IOReturn ExpandRing(XHCIRing *ringX);
#endif
    IOReturn InitAnEventRing(int IRQ);
    void FinalizeAnEventRing(int IRQ);
	void InitCMDRing(void);
	void InitEventRing(int IRQ, bool reinit=false);
	IOReturn MungeXHCIStatus(int code, bool in, UInt8 speed = kUSBDeviceSpeedSuper, bool silent = false);
	IOReturn MungeCommandCompletion(int code, bool silent = false);
	int GetEndpointID(int endpointNumber, short direction);
    int GetSlotID(int functionNumber);
	int CountRingToED(XHCIRing *ring, int index, UInt32 *shortfall, bool advance);
	int FreeSlotsOnRing(XHCIRing *ring);
    bool CanTDFragmentFit(XHCIRing *ring, UInt32 fragmentTransferSize);
	SInt32 WaitForCMD(TRB *t, int command, CMDComplete callBackF=0);
	void ResetEndpoint(int slotID, int EndpointID);
    int StartEndpoint(int slotID, int EndpointID, UInt16 streamID=0);
    void ClearStopTDs(int slotID, int EndpointID);
	int StopEndpoint(int slotID, int EndpointID);
    int QuiesceEndpoint(int slotID, int endpointID);
	void ClearEndpoint(int slotID, int EndpointID);
	IOReturn ReturnAllTransfersAndReinitRing(int slotID, int EndpointID, UInt32 streamID);
	IOReturn ReinitTransferRing(int slotID, int EndpointID, UInt32 streamID);
	void RestartStreams(int slotID, int EndpointID, UInt32 except);
	int SetTRDQPtr(int slotID, int EndpointID, UInt32 stream, int dQindex);
	bool FilterEventRing(int IRQ, bool *needsSignal);
    void DoStopCompletion(TRB *nextEvent);
    bool DoCMDCompletion(TRB nextEvent, UInt16 eventIndex);
    void PollForCMDCompletions(int IRQ);
	bool PollEventRing2(int IRQ);
	void SetTRBAddr64(TRB * CMD, USBPhysicalAddress64 addr);
	void SetStreamCtxAddr64(StreamContext * strc, USBPhysicalAddress64 addr, int sct, UInt32 pcs);
	void SetTRBDCS(TRB * CMD, bool DCS);
    SInt16 DecConfiguredEpCount();
    SInt16 IncConfiguredEpCount();
    IOReturn TestConfiguredEpCount();
    bool IsStreamsEndpoint(int slotID, int EndpointID);
	
	// Register Accessors
	UInt64 Read64Reg(volatile UInt64 *addr);
	void Write64Reg(volatile UInt64 *addr, UInt64 value, bool quiet=false);
	void Write32Reg(volatile UInt32 *addr, UInt32 value);
	UInt8 Read8Reg(volatile UInt8 *addr);
	UInt16 Read16Reg(volatile UInt16 *addr);
	UInt32 Read32Reg(volatile UInt32 *addr);
	bool CheckControllerAvailable(bool quiet=false);
	
	// Endpoint Context Accessors
	void SetDCBAAAddr64(USBPhysicalAddress64 * el, USBPhysicalAddress64 addr);
	void SetEPCtxDQpAddr64(Context * ctx, USBPhysicalAddress64 addr);
	USBPhysicalAddress64 GetEpCtxDQpAddr64(Context * ctx);
	int GetEpCtxEpState(Context * ctx);
	void SetEPCtxEpType(Context * ctx, int t);
	UInt8 GetEpCtxEpType(Context * ctx);
	UInt8 GetEPCtxInterval(Context * ctx);
	void SetEPCtxInterval(Context * ctx, UInt8 interval);
	void SetEPCtxMPS(Context * ctx, UInt16 mps);
	UInt16 GetEpCtxMPS(Context * ctx);
    UInt16 GetEPCtxMult(Context * ctx);
	void SetEPCtxMult(Context * ctx, UInt16 mult);
    UInt16 GetEPCtxMaxBurst(Context * ctx);
	void SetEPCtxMaxBurst(Context * ctx, UInt16 maxBurst);
	void SetEPCtxCErr(Context * ctx, UInt32 cerr);
	void SetEPCtxMaxPStreams(Context * ctx, int maxPStreams);
	void SetEPCtxLSA(Context * ctx, int state);
	void SetEPCtxDCS(Context * ctx, int state);
	void SetEPCtxAveTRBLen(Context * ctx, UInt32 len);
	void SetEPCtxMaxESITPayload(Context * ctx, UInt32 len);
    
	// Slot Context Accessors
	void SetSlCtxEntries(Context * ctx, UInt32 speed);
	UInt8 GetSlCtxEntries(Context * ctx);
	void SetSlCtxSpeed(Context * ctx, UInt32 speed);
	UInt8 GetSlCtxSpeed(Context * ctx);
	UInt32 GetSlCtxRootHubPort(Context * ctx);
	void SetSlCtxRootHubPort(Context * ctx, UInt32 rootHubPort);
	void SetSlCtxTTPort(Context * ctx, UInt32 port);
	int GetSlCtxTTPort(Context * ctx);
	void SetSlCtxTTSlot(Context * ctx, UInt32 slot);
	int GetSlCtxTTSlot(Context * ctx);
    void SetSlCtxInterrupter(Context * ctx, UInt32 interrupter);
    UInt32 GetSlCtxInterrupter(Context * ctx);
	UInt32 GetSlCtxRouteString(Context * ctx);
	void SetSlCtxRouteString(Context * ctx, UInt32 string);
	void ResetSlCtxNumPorts(Context * ctx, UInt32 num);
	void ResetSlCtxTTT(Context * ctx, UInt32 num);
	void SetSlCtxMTT(Context * ctx, bool multiTT);
	bool GetSlCtxMTT(Context * ctx);
	int GetSlCtxSlotState(Context * ctx);
	int GetSlCtxUSBAddress(Context * ctx);
    
	// TRB accessors
	void SetTRBType(TRB * CMD, int t);
	int GetTRBType(TRB * trb);
	int GetTRBSlotID(TRB * trb);
	void SetTRBSlotID(TRB * trb, UInt32 slotID);
	void SetTRBEpID(TRB * trb, UInt32 slotID);
    void SetTRBStreamID(TRB * trb, UInt32 streamID);
	int GetTRBCC(TRB * trb);
	bool IsIsocEP(int slotID, UInt32 endpointIdx);
	void SetTRBCycleBit(TRB *trb, int state);
	void SetTRBChainBit(TRB *trb, int state);
    bool GetTRBChainBit(TRB *trb);
	void SetTRBBSRBit(TRB *trb, int state);
	IOReturn EnqueCMD(TRB *trb, int type, CMDComplete callBackFn, SInt32 **param);
	void ClearTRB(TRB *trb, bool clearCCS);
	void PrintCapRegs(void);
	void PrintRuntimeRegs(void);
	void PrintInterrupter(int level, int IRQ, const char *s);
	void PrintTRB(int level, TRB *trb, const char *s, UInt32 offsC=0);
	// these methods all send the TRB to usbtracer
    void PrintTransferTRB(TRB *trb, XHCIRing* ringX, int indexInRing, UInt32 offsC=0 );
    void PrintEventTRB(TRB *trb, int irq, bool inFilter, XHCIRing* otherRing = NULL );
    void PrintCommandTRB(TRB *trb);
	
	void PrintContext(Context * ctx);
	void PrintSlotContexts(void);
	void PrintCCETRB(TRB *trb);
    void PrintRing(XHCIRing *ring);
    
    bool IsStillConnectedAndEnabled(int SlotID);
    IOReturn AddDummyCommand(XHCIRing *ringX, IOUSBCommand *command);
	void CompleteSlotCommand(TRB *t, void *p);
	void CompleteNECVendorCommand(TRB *t, void *p);
    
	void GetInputContext(void);
	void ReleaseInputContext(void);
	Context * GetContextFromDeviceContext(int SlotID, int contextIdx);
	Context * GetEndpointContext(int SlotID, int EndpointID);
	Context * GetSlotContext(int SlotID);
	Context * GetInputContextByIndex(int index);

	IOReturn AddressDevice(UInt32 slotID, UInt16 maxPacketSize, bool setAddr, UInt8 speed, int highSpeedHubSlot, int highSpeedPort);
    
	static void                 RHResumePortTimerEntry(OSObject *target, thread_call_param_t port);
    static void                 RHResetPortEntry(OSObject *target, thread_call_param_t port);
    
	void                        RHResumePortTimer(UInt32 port);
    IOReturn                    RHResetPort(UInt8 RHSpeed, UInt16 adjustedPort);
    
	static IOReturn             RHResumePortCompletionEntry(OSObject *target, void *param1, void *param2, void *param3, void *param4);
	IOReturn                    RHResumePortCompletion(UInt32 port);
	IOReturn                    RHCompleteResumeOnAllPorts();
    void RHCheckForPortResumes(void);
    
    void SaveAnInterrupter(int IRQ);
    void RestoreAnInterrupter(int IRQ);
	
	void TestCommands(void);
	bool CheckNECFirmware(void);
	IOReturn TestFn(int param);
	void CheckSleepCapability(void);
	
#if DEBUG_BUFFER
	void CheckBuf(IOUSBCommand* command);
#endif
	
    IOReturn GenerateNextPhysicalSegment(TRB *t, IOByteCount *req, UInt32 bufferOffset, IODMACommand *dmaCommand);
	
    TRB *GetNextTRB(XHCIRing *ring, void *xhciTD, TRB **StartofFragment, bool firstFragment);
    
    void PutBackTRB(XHCIRing *ring, TRB *t);
    
	IOReturn CreateStream(int       slotID,
                          int       endpointIdx,
						  UInt32    stream);
    
    void     DeleteStreams(int      slotID, 
                           int      endpointIdx);
    
	IOReturn CreateEndpoint(int							slotID,
							int							endpointIdx,
							UInt16						maxPacketSize,
							short						pollingRate,
							int							epType,
							UInt8						maxStream,
							UInt8						maxBurst,
							UInt8						mult,
							void                        *pEP);
	
	IOReturn BuildRHPortBandwidthArray(OSArray *rhPortArray);
	
	IOReturn CheckPeriodicBandwidth(int			slotID,
                                    int			endpointIdx,
                                    UInt16		maxPacketSize,
                                    short		pollingRate,
                                    int			epType,
                                    UInt8		maxStream,
                                    UInt8		maxBurst,
									UInt8		mult);
	IOReturn CreateTransfer(IOUSBCommand* command, UInt32 stream);
    void PrintTRBs(XHCIRing *ringX, const char *s, TRB *StartofFragment, TRB *end);
    void CloseFragment(XHCIRing *ringX, TRB *StartofFragment, UInt32 offsC1);
    int FindSlotFromPort(UInt16 port);

    
	IOReturn _createTransfer(void       *xhciTD, 
                            bool        isoc,
                            IOByteCount req, 
                            UInt32      offsC0=0, 
                            UInt64      runningOffset=0, 
                            bool        interruptNeeded = false, 
                            bool        fragmentedTDs = false,                             
                            UInt32      *firstTRBIndex = NULL, 
                            UInt32      *numTRBs=NULL,
							bool		noLogging = false,                            
                            SInt16      *completionIndex = NULL);

#if DEBUG_COMPLETIONS
	// Overriding the controller method
	void Complete(
                  IOUSBCompletion	completion,
                  IOReturn		status,
                  UInt32		actualByteCount = 0 );
#endif
	IOReturn	HCSelect ( UInt8 port, UInt8 controllerType );
    IOReturn	HCSelectWithMethod ( char *muxMethod );
	bool		DiscoverMuxedPorts();
    IOReturn    ResetController();
	void        EnableXHCIPorts();
	bool        HasMuxedPorts();
    IOReturn    QuiesceAllEndpoints ();
    
public:
	
	virtual bool		init(OSDictionary * propTable);
    virtual IOReturn 	message( UInt32 type, IOService * provider,  void * argument = 0 );
    virtual bool        terminate( IOOptionBits options = 0);
	virtual bool		willTerminate(IOService *provider, IOOptionBits options);

	virtual void		powerChangeDone ( unsigned long fromState );
    
    
    
    IOReturn UIMInitialize(IOService * provider);
    IOReturn UIMFinalize();
    
    
    
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
    
    IOReturn CreateBulkEndpoint(UInt8       functionNumber,
                                UInt8     endpointNumber,
                                UInt8     direction,
                                UInt8     speed,
                                UInt16    maxPacketSize,
                                USBDeviceAddress  highSpeedHub,
                                int       highSpeedPort,
                                UInt32    maxStream,
                                UInt32    maxBurst);
	
    virtual IOReturn UIMCreateStreams(UInt8				functionNumber,
                                      UInt8				endpointNumber,
                                      UInt8				direction,
                                      UInt32            maxStream);
    
    virtual IOReturn UIMCreateSSBulkEndpoint(
                                             UInt8		functionNumber,
                                             UInt8		endpointNumber,
                                             UInt8		direction,
                                             UInt8		speed,
                                             UInt16		maxPacketSize,
                                             UInt32       maxStream,
                                             UInt32       maxBurst);
	
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
    
    IOReturn CreateInterruptEndpoint(short     functionAddress,
                                     short     endpointNumber,
                                     UInt8     direction,
                                     short     speed,
                                     UInt16    maxPacketSize,
                                     short     pollingRate,
                                     USBDeviceAddress  highSpeedHub,
                                     int       highSpeedPort,
                                     UInt32    maxBurst);
    
    virtual IOReturn UIMCreateSSInterruptEndpoint(
                                                  short		functionAddress,
                                                  short		endpointNumber,
                                                  UInt8		direction,
                                                  short		speed,
                                                  UInt16		maxPacketSize,
                                                  short		pollingRate,
                                                  UInt32   maxBurst);
    
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
	
    IOReturn CreateIsochEndpoint(short				functionAddress,
                                 short				endpointNumber,
                                 UInt32				maxPacketSize,
                                 UInt8				direction,
                                 UInt8				interval,
                                 UInt8				maxBurst,
								 UInt8				mult);
    
    virtual IOReturn UIMCreateSSIsochEndpoint(
                                              short				functionAddress,
                                              short				endpointNumber,
                                              UInt32			maxPacketSize,
                                              UInt8				direction,
                                              UInt8				interval,
                                              UInt32            maxBurstAndMult);
    
    // obsolete method
    virtual IOReturn UIMCreateIsochTransfer(short					functionAddress,
                                            short					endpointNumber,
                                            IOUSBIsocCompletion		completion,
                                            UInt8					direction,
                                            UInt64					frameStart,
                                            IOMemoryDescriptor		*pBuffer,
                                            UInt32					frameCount,
                                            IOUSBIsocFrame			*pFrames);
	
    virtual IOReturn UIMCreateIsochTransfer(IOUSBIsocCommand *command);
    
    virtual IOReturn UIMAbortStream(UInt32		streamID,
                                    short		functionNumber,
                                    short		endpointNumber,
                                    short		direction);
	
    virtual IOReturn UIMAbortEndpoint(short				functionNumber,
                                      short				endpointNumber,
                                      short				direction);
    
    virtual IOReturn UIMDeleteEndpoint(short				functionNumber,
                                       short				endpointNumber,
                                       short				direction);
    
    virtual IOReturn UIMClearEndpointStall(short				functionNumber,
                                           short				endpointNumber,
                                           short				direction);
    
    virtual UInt32			UIMMaxSupportedStream(void);
    
	
    
    virtual void		UIMRootHubStatusChange(void);
    virtual void 		UIMRootHubStatusChange( bool abort );
	
    IOReturn configureHub(UInt32 address, UInt32 flags);
	
    virtual IOReturn 		UIMHubMaintenance(USBDeviceAddress highSpeedHub, UInt32 highSpeedPort, UInt32 command, UInt32 flags);
    virtual IOReturn		UIMSetTestMode(UInt32 mode, UInt32 port);
    virtual UInt32			GetPortSCForWriting(short port);
    virtual IOReturn 		GetRootHubDeviceDescriptor( IOUSBDeviceDescriptor *desc );
    virtual IOReturn 		GetRootHubDescriptor( IOUSBHubDescriptor *desc );
    virtual IOReturn 		GetRootHub3Descriptor( IOUSB3HubDescriptor *desc );
    virtual IOReturn 		SetRootHubDescriptor( OSData *buffer );
    virtual IOReturn 		GetRootHubBOSDescriptor( OSData *desc );
    virtual IOReturn 		GetRootHubConfDescriptor( OSData *desc );
    virtual IOReturn 		GetRootHubStatus( IOUSBHubStatus *status );
    virtual IOReturn 		SetRootHubFeature( UInt16 wValue );
    virtual IOReturn		ClearRootHubFeature( UInt16 wValue );
    virtual IOReturn 		GetRootHubPortStatus( IOUSBHubPortStatus *status, UInt16 port );
    virtual IOReturn 		SetRootHubPortFeature( UInt16 wValue, UInt16 port );
    virtual IOReturn 		ClearRootHubPortFeature( UInt16 wValue, UInt16 port );
    virtual IOReturn 		GetRootHubPortState( UInt8 *state, UInt16 port );
    virtual IOReturn 		SetHubAddress( UInt16 wValue );
    virtual IOReturn 		GetRootHubPortErrorCount( UInt16 port, UInt16 *count );
    
    
    virtual UInt32 		GetBandwidthAvailable( void );
    virtual UInt64 		GetFrameNumber( void );
    virtual UInt64 		GetMicroFrameNumber( void );
    virtual UInt32 		GetFrameNumber32( void );
    
    
    // this call is not gated, so we need to gate it ourselves
    virtual IOReturn								GetFrameNumberWithTime(UInt64* frameNumber, AbsoluteTime *theTime);
    // here is the gated version
    static IOReturn									GatedGetFrameNumberWithTime(OSObject *owner, void* arg0, void* arg1, void* arg2, void* arg3);
    
    // Debugger polled mode
    virtual void 		PollInterrupts( IOUSBCompletionAction safeAction = 0 );
	
	
	
    
    virtual IOReturn GetRootHubStringDescriptor(UInt8	index, OSData *desc);
    
    
    // in the new IOUSBControllerV3 class
    virtual void        ControllerSleep(void);
    virtual	IOReturn	ResetControllerState(void);
    virtual IOReturn	RestartControllerFromReset(void);
    virtual	IOReturn	SaveControllerStateForSleep(void);
    virtual	IOReturn	RestoreControllerStateFromSleep(void);
    virtual IOReturn	DozeController(void);
    virtual IOReturn	WakeControllerFromDoze(void);
    virtual IOReturn	UIMEnableAddressEndpoints(USBDeviceAddress address, bool enable);
    virtual IOReturn	UIMEnableAllEndpoints(bool enable);
    virtual IOReturn	EnableInterruptsFromController(bool enable);
    
    // Overriding the controller V3 method to get hub port info
    virtual  IOReturn	ConfigureDeviceZero(UInt8 maxPacketSize, UInt8 speed, USBDeviceAddress hub, int port);
    
	
	virtual IOReturn    GetBandwidthAvailableForDevice(IOUSBDevice *forDevice, UInt32 *pBandwidthAvailable);

	virtual IOReturn	UIMDeviceToBeReset(short functionAddress);
	bool				checkEPForTimeOuts(int slot, int endp, UInt32 stream, UInt32 curFrame);
	void				CheckSlotForTimeouts(int slot, UInt32 frame);
	virtual void		UIMCheckForTimeouts(void);

	virtual USBDeviceAddress	UIMGetActualDeviceAddress(USBDeviceAddress current);
	
	virtual IOUSBControllerIsochEndpoint*			AllocateIsochEP();
	
	virtual IODMACommand*							GetNewDMACommand();
	
    // Internal routines
    IOReturn  XHCIRootHubPower(bool on);
    IOReturn  XHCIRootHubPowerPort(UInt16 port, bool on);
    IOReturn  XHCIRootHubEnablePort(UInt16 port, bool enable);
    IOReturn  XHCIRootHubSuspendPort(UInt8 RHSpeed, UInt16 adjustedPort, bool suspend);
    IOReturn  XHCIRootHubClearPortConnectionChange(UInt16 port);
    IOReturn  XHCIRootHubClearPortEnableChange(UInt16 port);
    IOReturn  XHCIRootHubClearPortSuspendChange(UInt16 port);
    IOReturn  XHCIRootHubResetOverCurrentChange(UInt16 port);
    IOReturn  XHCIRootHubClearResetChange(UInt8 RHSpeed, UInt16 adjustedPort);
	IOReturn  XHCIRootHubClearWarmResetChange(UInt16 port);
	IOReturn  XHCIRootHubClearPortLinkChange(UInt16 port);
	IOReturn  XHCIRootHubClearConfigErrorChange(UInt16 port);
    
    
	IOReturn  XHCIRootHubResetPort (UInt8 RHSpeed, UInt16 port);
	IOReturn  XHCIRootHubWarmResetPort (UInt16 port);
	IOReturn  XHCIRootHubSetLinkStatePort (UInt16 linkState, UInt16 port);
	IOReturn  XHCIRootHubSetU1TimeoutPort (UInt16 timeout, UInt16 port);
	IOReturn  XHCIRootHubSetU2TimeoutPort (UInt16 timeout, UInt16 port);
	IOReturn  XHCIRootHubRemoteWakeupMaskPort (UInt16 mask, UInt16 port);
    
	void            DecodeExtendedCapability();
	void            DecodeSupportedProtocol(XHCIXECPRegistersPtr protocolBase);
	void            AdjustRootHubPortNumbers(UInt8 speed, UInt16 *port);
    UInt16          GetCompanionRootPort(UInt8 speed, UInt16 adjustedPort);
    bool            IsRootHubPortSuperSpeed(UInt16 *port);
    
	IOReturn PlacePortInMode(UInt32 port, UInt32 mode);
	IOReturn LeaveTestMode();
	IOReturn EnterTestMode();
    void EnableComplianceMode();
    void DisableComplianceMode();
	
	IOReturn StopUSBBus();
    void RestartUSBBus();    
    
    const char *TRBType(int type);
    const char *EndpointState(int state);

	// Isoc management
	IOReturn		ScavengeAnIsocTD(AppleXHCIIsochEndpoint *pEP,  AppleXHCIIsochTransferDescriptor *pTD);
	IOReturn		ScavengeIsocTransactions(AppleXHCIIsochEndpoint *pEP, bool reQueueTransactions);
    void			AddIsocFramesToSchedule(AppleXHCIIsochEndpoint* pEP);
    IOReturn		AbortIsochEP(AppleXHCIIsochEndpoint* pEP);
    IOReturn		DeleteIsochEP(AppleXHCIIsochEndpoint* pEP);

    // Async SW Endpoints Managment
    AppleXHCIAsyncEndpoint  *AllocateAppleXHCIAsyncEndpoint(XHCIRing *ring, UInt32 maxPacketSize, UInt32 maxBurst, UInt32 mult);
	
#if (DEBUG_REGISTER_READS == 1)
private:
	UInt64 fTempReg;
#endif
};


#endif
