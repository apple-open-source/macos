/*
 *Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 *@APPLE_LICENSE_HEADER_START@
 *
 *The contents of this file constitute Original Code as defined in and
 *are subject to the Apple Public Source License Version 1.1 (the
 *"License").  You may not use this file except in compliance with the
 *License.  Please obtain a copy of the License at
 *http://www.apple.com/publicsource and read it before using this file.
 *
 *This Original Code and all software distributed under the License are
 *distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *License for the specific language governing rights and limitations
 *under the License.
 *
 *@APPLE_LICENSE_HEADER_END@
 */
/*
 *PPCSerialPort.h - This file contains the class definition for the
 *		     AppleSCCSerial driver, which supports IBM Asynchronous
 *		     Communications Adapters, and compatible hardware.
 *
 *Writers:
 *Humphrey Looney	MacOSX IOKit ppc
 *Elias Keshishoglou 	MacOSX Server ppc
 *Dean Reece		Original Next Intel version
 *
 *History
 *23-June-94	Dean Reece at NeXT	Created
 *05-Jan-01		Paul Sun	Fixed bug # 2550140 & 2553750 by adding mutex locks
 *							IOdbdmaLock in the SccChannel structure.
 *22-Jan-01		Paul Sun	Fixed bug # 2560437 -- took out the enableScc() function
 *							which is now supported by PlatformExpert.
 *
 *Copyright ©: 	1999 Apple Computer, Inc.  all rights reserved.
 */

#ifndef PPC_Serial_Port_h
#define PPC_Serial_Port_h

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOConditionLock.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/platform/AppleMacIODevice.h>

#include <IOKit/serial/IOSerialDriverSync.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include <IOKit/IOBufferMemoryDescriptor.h>

#include "SccTypes.h"
#include "HardwareDefs.h"
#include "SccQueuePrimatives.h"

// for DCP -- begin
#define	DCPFunction_ParseData		1
#define	DCPFunction_SetDCD			2
#define	DCPFunction_QuitDCP			3

#define	kOff	0
#define	kOn		1
// for DCP -- end

//#define SHOW_DEBUG_STRINGS	ON
//#define TRACE ON


#define USE_WORK_LOOPS	1
#define USE_FILTER_EVENT_SOURCES	0



#define BIGGEST_EVENT		3
#define BUFFER_SIZE_MINIMUM	(BIGGEST_EVENT*6)
#define BUFFER_SIZE_DEFAULT	(16*1024)
#define BUFFER_SIZE_MAXIMUM	(256<<10)
#define TX_BUFFER_SIZE_KEY	"TX Buffer Size"
#define RX_BUFFER_SIZE_KEY	"RX Buffer Size"
#define CHIP_TYPE_KEY		"Chip Type"
#define INSTANCE_KEY		"Instance"
#define CHIP_CLOCK_KEY		"Chip Clock"
#define CHIP_CLOCK_DEFAULT	(1843200l)
#define HEART_BEAT_KEY		"Heart Beat Interval"
#define HEART_BEAT_DEFAULT	(11000l)
#define MSR_INTERRUPT_KEY	"Enable MSR Interrupts"
#define BUS_TYPE_KEY		"Bus Type"
#define BUS_EISA_VAL		"EISA"
#define BUS_PCMCIA_VAL		"PCMCIA"

#define SPECIAL_SHIFT		(5)
#define SPECIAL_MASK		((1<<SPECIAL_SHIFT) - 1)
#define STATE_ALL			(PD_RS232_S_MASK | PD_S_MASK )
#define FLOW_RX_AUTO   	 	(PD_RS232_A_RFR | PD_RS232_A_DTR \
                                | PD_RS232_A_RXO )
#define FLOW_TX_AUTO    	(PD_RS232_A_CTS | PD_RS232_A_DSR \
                                | PD_RS232_A_TXO | PD_RS232_A_DCD )
#define CAN_BE_AUTO			(FLOW_RX_AUTO | FLOW_TX_AUTO )
#define CAN_NOTIFY			(PD_RS232_N_MASK )
#define EXTERNAL_MASK   	(PD_S_MASK | (PD_RS232_S_MASK & ~PD_RS232_S_LOOP) )
#define INTERNAL_DELAY  	(PD_RS232_S_LOOP )
#define DEFAULT_AUTO		(PD_RS232_A_DTR | PD_RS232_A_RFR \
                                | PD_RS232_A_CTS | PD_RS232_A_DSR )
#define DEFAULT_NOTIFY		(0x00 )
#define DEFAULT_STATE		(PD_S_TX_ENABLE | PD_S_RX_ENABLE \
                                | PD_RS232_A_TXO | PD_RS232_A_RXO )
#define IDLE_XO	   		0
#define NEEDS_XOFF 		1
#define SENT_XOFF 		-1
#define NEEDS_XON  		2
#define SENT_XON  		-2

#define	CONTINUE_SEND	1
#define	PAUSE_SEND		2

//typedef void				*CalloutEntry;
//typedef UInt32 long		CalloutTime;
typedef UInt32			CalloutTime;


#define calloutEntryAllocate 		thread_call_allocate
#define calloutEntryRemove		thread_call_cancel
#define calloutEntryFree		thread_call_free
#define calloutEntryDispatch		thread_call_enter
#define calloutEntryDispatchDelayed 	thread_call_enter_delayed
//#define calloutDeadlineFromInterval 	deadline_from_interval *** threads

#define    kX1UserClockMode		0x00	// for MIDI devices
#define    kX16UserClockMode	0x0b
#define    kX32UserClockMode	0x19
#define    kX64UserClockMode	0x32

#include <IOKit/ppc/IODBDMA.h>

/*
 *It is possible to improve the total behavior of the driver changing the
 *dimension of the MAX_BLOCK_SIZE, that can not be however larger than
 *PAGESIZE
 */
#define MAX_BLOCK_SIZE		PAGE_SIZE

// for DCP -- begin

#define	kDCPModem					0x0a

class DCPModemSupport : public IOService
{
    OSDeclareDefaultStructors( DCPModemSupport );
	
private:
	UInt32	gDCPBufferSize;
	void *	userClientPtr;	

public:

    /**** IOKit methods	****/
	virtual	bool		start   ( IOService *provider );
	virtual	void		stop    ( IOService *provider );
    virtual	IOService*  probe   ( IOService *provider, SInt32 *pScore );
	virtual	IOReturn	callPlatformFunction ( const OSSymbol * functionName,
												bool waitForFunction,
												void *param1, void *param2,
												void *param3, void *param4 );
	virtual	void		setUserClientPtr (void * ptr);
	virtual	void		setDCPBufferSize (UInt32 size);
	virtual	bool		callDCPModemSupportFunctions (UInt32 functionIndex, UInt8 data, bool * checkByte, bool * addByte);

};

// for DCP -- end

typedef struct SerialDBDMAStatusInfo
{
    IODBDMAChannelRegisters	*dmaBase;                  /* -> DBDMA channel area (phis) */
    IODBDMAChannelRegisters	*dmaChannelAddress;        /* -> DBDMA channel area (log) */
    IODBDMADescriptor 		*dmaChannelCommandArea;    /* -> DBDMA channel area (log)  */
    UInt32			dmaNumberOfDescriptors;    /* Number of DATA descriptors   */
    IOBufferMemoryDescriptor	*dmaChannelCommandAreaMDP; /* Memory descriptor for Command area */
    UInt8			*dmaTransferBuffer;        /* address of the buffer to move */
    UInt32			dmaTransferSize;           /* dimension of the block to move */
    IOBufferMemoryDescriptor	*dmaTransferBufferMDP; 	   /* Memory descriptor for Transfer buffer */
    UInt32			lastPosition;              /* last position of the dma */
} SerialDBDMAStatusInfo;


typedef struct {
    UInt32 ints;
    UInt32 txInts;
    UInt32 rxInts;
    UInt32 mdmInts;
    UInt32 txChars;
    UInt32 rxChars;
} Stats_t;

typedef struct BufferMarks {
    UInt32	BufferSize;
    UInt32	HighWater;
    UInt32	LowWater;
    bool	OverRun;
} BufferMarks;

typedef struct {
    //id			Self;		****
    UInt32			Instance;
	void *			fAppleSCCSerialInstance;
    unsigned const char		*PortName;
    UInt32			State;
    UInt32			WatchStateMask;
   // IOSimpleLock *		WatchLock;
   // IOSimpleLock *       	serialRequestLock;

    IOLock *		WatchLock;
    //IOLock 			*serialRequestLock;
    IORecursiveLock 		*serialRequestLock;
    // queue control structures:
    CirQueue			RX;
    CirQueue			TX;
    BufferMarks			RXStats;
    BufferMarks			TXStats;

	// dbdma memory control
    IOLock			*IODBDMARxLock;
    IOLock			*IODBDMATrLock;

	IOLock					*SCCAccessLock;
	
    // UART configuration info:
    UInt32			Base;
    //UInt32			IRQ;
    /* No Longer used ejk 	*/
    //UInt32			Type;
    UInt32			CharLength;
    UInt32			StopBits;
    UInt32			TX_Parity;
    UInt32			RX_Parity;
    UInt32			BreakLength;
    UInt32			BaudRate;
    unsigned short		DLRimage;
    UInt8			LCRimage;
    UInt8			FCRimage;
    UInt8			IERmask;
    UInt8			RBRmask;
    UInt32			MasterClock;
    bool                	MinLatency;
    bool			WaitingForTXIdle;
    //bool              	JustDoneInterrupt;
    //bool			PCMCIA;
    //bool			PCMCIA_yanked;

    // flow control state & configuration:
    UInt8					XONchar;
    UInt8					XOFFchar;
    UInt32					SWspecial[(0x100)>>SPECIAL_SHIFT];
    UInt32					FlowControl;	// notify-on-delta & auto_control
    UInt32					FlowControlState;
    bool					DCDState;
    bool					CTSState;
    /* Indicates our receive state.
    */
    int						RXOstate;
    /* Indicates our transmit state, if we have received any Flow Control.
    */
    int						TXOstate;
    UInt8					xOffSent;		/* ejk 	*/

    /* Globals in the Copeland Version.
    */
    UInt32 					GlobalRecvChars;
    UInt32  				OverRunChars;
    //UInt32 TrChars;

    // callout entries:
    IOThread				FrameTOEntry;
    IOThread				DataLatTOEntry;
    IOThread				DelayTOEntry;
    IOThread				HeartBeatTOEntry;
    mach_timespec			FrameInterval;
    mach_timespec			DataLatInterval;
    mach_timespec			CharLatInterval;
    mach_timespec			HeartBeatInterval;
    // Statistics
    Stats_t					Stats;
    bool					AreTransmitting;
    bool					GotTXInterrupt;

    // chip dependent
    bool					baudRateGeneratorEnable;
    UInt8					baudRateGeneratorLo;
    UInt8					baudRateGeneratorHi;
    UInt32					rtxcFrequency;
    UInt8					lastWR[ kNumSCCWR ];

    UInt8					*ControlRegister;
    UInt8					*DataRegister;
    IOPhysicalAddress		ChipBaseAddress;
    UInt32					ConfigWriteRegister;
    SerialPortSelector		whichPort;
    IOPhysicalAddress		TxDMABase;
    UInt32					TxDMALength;
    IOPhysicalAddress		RxDMABase;
    UInt32					RxDMALength;

    UInt8					InterruptNumbers[MaxInterrupts];

    // enable bits
    UInt8					ourReceiveBits;
    UInt8					ourTransmitBits;
    UInt8					ourClockSource;
    bool					haveQueuedRxSIH;
    bool					haveQueuedTxSIH;

    /*
     *gDBDMAChannelAddress, gChannelCommandArea reference the DBDMA channel
     *command area. This is a kernel memory area.
     */
    SerialDBDMAStatusInfo	TxDBDMAChannel;
    SerialDBDMAStatusInfo	RxDBDMAChannel;
    
	bool					DTRAsserted;
	bool					aboveRxHighWater;
    
    bool					lastCTSState;
    UInt32					lastCTSTime;
    UInt32					ctsTransitionCount;
    
    
    IOTimerEventSource*		rxTimer;
    // for DCP -- begin
	
    bool					gDCPUserClientSet;
	bool					gDCPModemFound;
	DCPModemSupport			*DCPModemSupportPtr;
	
	// for DCP -- end

} PortInfo_t, SccChannel;

extern vm_map_t kernel_map;	// _t (-*) need to verify if correct ***

void SCC_TRACEMSG(char *msg);
UInt32 SCC_GetSystemTime(void);


static inline UInt32 intToBool(long inIntVal )
{
    return inIntVal ? true : false;
}

static inline UInt32 tval2long(mach_timespec val)
{
   return ((val.tv_sec *NSEC_PER_SEC) + val.tv_nsec);
}

static inline mach_timespec long2tval(UInt32 val)
{
    mach_timespec tval;

    tval.tv_sec  = val / NSEC_PER_SEC;
    tval.tv_nsec = val % NSEC_PER_SEC;
    return tval;
}

typedef void (*SCCCarrierHack)(OSObject *target, bool carrier);

class AppleSCCSerial : public IOSerialDriverSync
{
    OSDeclareDefaultStructors(AppleSCCSerial)

public:

	// member variables
    IOWorkLoop*			myWorkLoop;		// holds the workloop for this driver
    IOCommandGate		*fCommandGate;		// and the command gate

	IOTimerEventSource*	myTimer;		// holds the timer we create
	UInt32				counter;		// counter incremented each time the timeout handler is called

    PortInfo_t			Port;
    AppleMacIODevice	*fProvider;
    SCCCarrierHack		modemDCDStateChangeCallback;
    thread_call_t		fPollingThread;
	
    thread_call_t		fdmaStartTransmissionThread;
    thread_call_t		dmaRxHandleCurrentPositionThread;
	
	
    OSObject 			*fModemObject;
    SInt32				fCarrierHackCount;
    SInt32				fTransmissionCount;
    SInt32				fCurrentPositionCount;

#if USE_WORK_LOOPS
//	IOWorkLoop*						myWorkLoop;
	IOInterruptEventSource*			sccInterruptSource;
#if USE_FILTER_EVENT_SOURCES
	IOFilterInterruptEventSource*	txDMAInterruptSource;
	IOFilterInterruptEventSource*	rxDMAInterruptSource;
#else
	IOInterruptEventSource*			txDMAInterruptSource;
	IOInterruptEventSource*			rxDMAInterruptSource;
#endif
#endif
  
    SccChannel *portPtr() {return &Port;}

    /* IOSerialDriverSync Implementation */
    virtual IOReturn acquirePort(bool sleep, void *refCon);
    virtual IOReturn releasePort(void *refCon);
    virtual IOReturn setState(UInt32 state, UInt32 mask, void *refCon);
    UInt32 getState(void *refCon);
    virtual IOReturn watchState(UInt32 *state, UInt32 mask, void *refCon);
    UInt32 nextEvent(void *refCon);
    virtual IOReturn executeEvent(UInt32 event, UInt32 data, void *refCon);
    virtual IOReturn requestEvent(UInt32 event, UInt32 *data, void *refCon);
    virtual IOReturn enqueueEvent(UInt32 event, UInt32 data,
                                  bool sleep, void *refCon);
    virtual IOReturn dequeueEvent(UInt32 *event, UInt32 *data,
                                  bool sleep, void *refCon);
    virtual IOReturn enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count,
                                 bool sleep, void *refCon);
    virtual IOReturn dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count,
                                 UInt32 min, void *refCon);
    /* End IOSerialDriverSync Implementation */

    /* IOKit methods */
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);

    /* Registry & Modem handling methods */
    virtual void setPortName(IOService *provider);

    static void interruptOccurred(OSObject *obj,
                                  IOInterruptEventSource *src, int count);
    bool createSerialStream();

    static void deactivatePort(PortInfo_t *port);
    static IOReturn activatePort(PortInfo_t *port);

    static bool	allocateRingBuffer(CirQueue *Queue, size_t BufferSize);
    static void	freeRingBuffer(CirQueue *Queue);

    static void	SetStructureDefaults(PortInfo_t *port, bool Init);
    static IOReturn watchState(PortInfo_t *port, UInt32 *state, UInt32 mask);
    static void changeState(PortInfo_t *port, UInt32 state, UInt32 mask);
    static IOReturn executeEvent(PortInfo_t *port, UInt32 event,UInt32 data, UInt32 *state, UInt32 *delta);
    static UInt32 readPortState(PortInfo_t *port);

    static void CheckQueues(PortInfo_t *port);

    static void frameTOHandler(PortInfo_t *port);
    static void dataLatTOHandler(PortInfo_t *port);
    static void	delayTOHandler(PortInfo_t *port);
    static void heartBeatTOHandler(PortInfo_t *port);

    // Protos for the interrupt handler
    static void handleInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
    static void handleDBDMATxInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
    static void handleDBDMARxInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
  
      
	static void timeoutHandler(OSObject *owner, IOTimerEventSource *sender);

    static bool interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
    static void interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
  
    void setCarrierHack(OSObject *target, SCCCarrierHack action);
    static void callCarrierhack(thread_call_param_t whichDevice, thread_call_param_t carrier);
	static bool	NonSCCModem();
	// for DCP -- begin
    static bool LookForInternalModem(UInt8 modemID);
	// for DCP -- end

	// Static stubs for IOCommandGate::runAction
    static	IOReturn	releasePortAction(OSObject *owner, void *, void *, void *, void *);


	// Gated methods called by the Static stubs
    virtual	IOReturn	releasePortGated(void);
};

#endif
