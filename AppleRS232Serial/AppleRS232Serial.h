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
 
#ifndef __AppleRS232Serial__
#define __AppleRS232Serial__
 
#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to event log - LDEBUG must also be set
#define	LOG_DATA	0			// logs data to the IOLog - LDEBUG must also be set
#define DUMPALL		0			// Dumps all the data to the log - LOG_DATA must also be set

#define Sleep_Time	20			// 20 ms per iolog

#if LDEBUG
    #if USE_ELG
        #include "XTrace.h"
        #define XTRACE(x, y, msg)                    						\
        do											\
        {											\
            if (gXTrace)									\
            {											\
                static char *__xtrace = 0;              					\
                if (__xtrace)									\
                    gXTrace->LogAdd(gTraceID, (UInt32)(x), (UInt32)(y), __xtrace);    		\
                else										\
                    __xtrace = gXTrace->LogAdd(gTraceID, (UInt32)(x), (UInt32)(y), msg, false);	\
            }											\
        } while(0)
        #define ELG(x, y, msg) XTRACE(x, y, msg)
        #define XTRACE2(x, y, msg) XTRACE_HELPER(gXTrace, gTraceID, x, y, "AppleRS232Serial: "  msg)
    #else /* not USE_ELG */
        #define ELG(A,B,STRING)	{IOLog( "AppleRS232Serial: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B) );IOSleep(Sleep_Time);}
    #endif /* USE_ELG */
    #if LOG_DATA
        #define LogData(D, C, b)	SerialLogData((UInt8)D, (UInt32)C, (char *)b)
    #else /* not LOG_DATA */
        #define LogData(D, C, b)
    #endif /* LOG_DATA */
#else /* not LDEBUG */
    #define ELG(A,B,S)
    #define LogData(D, C, b)
    #undef USE_ELG
    #undef LOG_DATA
#endif /* LDEBUG */

#if LDEBUG && USE_ELG
#define ALERT(A,B,STRING) do { \
	    IOLog( "AppleRS232Serial: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B) );\
	    ELG(A,B,STRING);\
	} while(0)
#else
#define ALERT(A,B,STRING) IOLog( "AppleRS232Serial: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B) )
#endif

#define getDebugFlags()			(getDebugFlagsTable(getPropertyTable()))

#define NEWPHYS		1		// New is getPhysicalSegment (at the moment) as opposed to old pmap-extract

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOConditionLock.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/platform/AppleMacIODevice.h>

#include <IOKit/serial/IOSerialDriverSync.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/ppc/IODBDMA.h>

#include "SccQueue.h"
#include "Z85C30.h"

//#define SHOW_DEBUG_STRINGS	ON
//#define TRACE ON

#define MIN_BAUD (50 << 1)

enum
{
	kSerialOut = 0,
	kSerialIn = 1,
	kSerialOther = 2
};

enum
{
    R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15
};

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
#define STATE_ALL		(PD_RS232_S_MASK | PD_S_MASK )
#define FLOW_RX_AUTO   	 	(PD_RS232_A_RFR | PD_RS232_A_DTR | PD_RS232_A_RXO )
#define FLOW_TX_AUTO    	(PD_RS232_A_CTS | PD_RS232_A_DSR | PD_RS232_A_TXO | PD_RS232_A_DCD )
#define CAN_BE_AUTO		(FLOW_RX_AUTO | FLOW_TX_AUTO )
#define CAN_NOTIFY		(PD_RS232_N_MASK )
#define EXTERNAL_MASK   	(PD_S_MASK | (PD_RS232_S_MASK & ~PD_RS232_S_LOOP) )
#define INTERNAL_DELAY  	(PD_RS232_S_LOOP )
#define DEFAULT_AUTO		(PD_RS232_A_DTR | PD_RS232_A_RFR | PD_RS232_A_CTS | PD_RS232_A_DSR )
#define DEFAULT_NOTIFY		(0x00 )
#define DEFAULT_STATE		(PD_S_TX_ENABLE | PD_S_RX_ENABLE | PD_RS232_A_TXO | PD_RS232_A_RXO )
#define IDLE_XO	   		0
#define NEEDS_XOFF 		1
#define SENT_XOFF 		-1
#define NEEDS_XON  		2
#define SENT_XON  		-2

#define	CONTINUE_SEND		1
#define	PAUSE_SEND		2

typedef UInt32			CalloutTime;

#define calloutEntryAllocate 		thread_call_allocate
#define calloutEntryRemove		thread_call_cancel
#define calloutEntryFree		thread_call_free
#define calloutEntryDispatch		thread_call_enter
#define calloutEntryDispatchDelayed 	thread_call_enter_delayed

#define    kX1UserClockMode	0x00	// for MIDI devices
#define    kX16UserClockMode	0x0b
#define    kX32UserClockMode	0x19
#define    kX64UserClockMode	0x32

static inline void SynchronizeIO(void)
{
    eieio();
}

    // Interrupts

typedef enum InterruptTypes
{
  //  kSerialInterrupts   	= 0,          	// IST Member from motherboard
  //  kTxDMAInterrupts		= 1,     	// IST Member from motherboard
  //  kRxDMAInterrupts		= 2,      	// IST Member from motherboard
  //  kNoInterrupts		= 3,
    kTxInterrupts		= 4,  		//SCC chip level
    kRxInterrupts		= 5,
    kSccInterrupts		= 6,		// sets/clears chip interrupts
 //   kAllInterrupts		= 7		// invokes OS enabler/disbaler
} InterruptTypes;

    // SCC interrupt sources
    
typedef enum SCCInterruptSource
{
    kSccTransmitInterrupt	= 0,		// transmit buffer empty interrupt
    kSccExtStatusInterrupt	= 1,		// external/status interrupt
    kSccReceiveInterrupt	= 2,		// receiver interrupt
    kSccReceiveErrorInterrupt	= 3,		// receiver error interrupt
    kSccInterruptSources	= 4		// total number of SCC interrupt sources
} SCCInterruptSource;

    // Machine Types

typedef enum Machine_Type
{
    kUnknownMachine 		= 0,
    k5300Machine,				// PowerBook Class
    k6100Machine,				// PDM Class
    k7100Machine,
    k8100Machine,
    k7500Machine,				// PowerSurge Class
    k8500Machine,	
    k9500Machine,
    ke407Machine                		// Alchemy
} Machine_Type;

    // These are temporary until the Motherboard expert provides them
    
enum SerialOffsets
{
    channelADataOffset		= 6,		// channel A data in or out
    channelAControlOffset	= 2,		// channel A control
    channelBDataOffset		= 4,		// channel B data in or out
    channelBControlOffset	= 0,		// channel B control

    channelADataOffsetRISC	= 0x30,		// channel A data in or out
    channelAControlOffsetRISC	= 0x20,		// channel A control
    channelBDataOffsetRISC	= 0x10,		// channel B data in or out
    channelBControlOffsetRISC	= 0		// channel B control
};
	
#define channelDataOffsetRISC		0x010
#define channelControlOffsetRISC	0x000
#define channelDataOffset		4
#define channelControlOffset		0

enum InterruptAssignments
{
    kIntChipSet			= 0,
    kIntTxDMA,
    kIntRxDMA,
    MaxInterrupts
};
	
#define DMABufferSize		4096

enum ParityType
{
    NoParity 			= 0,
    OddParity,
    EvenParity,
    MaxParity
};

enum SerialPortSelector
{
    serialPortA			= 0,
    serialPortB			= 1,
    MaxPortsPerChip		= 2
};

#define ChannelAName		"ch-a"
#define ChannelBName		"ch-b"

#define kDefaultBaudRate	9600
#define kMaxBaudRate		230400
#define kMaxCirBufferSize	4096	

    // It is possible to improve the total behavior of the driver changing the
    // dimension of the MAX_BLOCK_SIZE, however it cannot be larger than PAGESIZE
 
#define MAX_BLOCK_SIZE		PAGE_SIZE

typedef struct SerialDBDMAStatusInfo
{
    IODBDMAChannelRegisters	*dmaBase;                 	/* -> DBDMA channel area (phis)		*/
    IODBDMAChannelRegisters	*dmaChannelAddress;        	/* -> DBDMA channel area (log)		*/
    IODBDMADescriptor 		*dmaChannelCommandArea;    	/* -> DBDMA channel area (log) 		*/
    UInt32			dmaNumberOfDescriptors;    	/* Number of DATA descriptors		*/
#if NEWPHYS
    IOBufferMemoryDescriptor	*dmaChannelCommandAreaMDP; 	/* Memory descriptor for Command area 	*/
#endif
    UInt8			*dmaTransferBuffer;        	/* address of the buffer to move	*/
    UInt32			dmaTransferSize;           	/* dimension of the block to move	*/
#if NEWPHYS
    IOBufferMemoryDescriptor	*dmaTransferBufferMDP; 	   	/* Memory descriptor for Transfer buffer */
#endif
    UInt32			lastPosition;              	/* last position of the dma		*/
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

typedef struct
{
    UInt32			Instance;
    unsigned const char		*PortName;
    UInt32			State;
    UInt32			WatchStateMask;
    
    OSObject			*RS232;

    //IOLock 			*WatchLock;
    //IOLock 			*serialRequestLock;
    //IORecursiveLock 		*serialRequestLock;
    
        // queue control structures
    
    CirQueue			RX;
    CirQueue			TX;
    BufferMarks			RXStats;
    BufferMarks			TXStats;

	// dbdma memory control
        
    IOLock			*IODBDMARxLock;
    IOLock			*IODBDMATrLock;

    IOLock			*SCCAccessLock;
	
        // UART configuration info
    
    UInt32			Base;
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

        // flow control state & configuration
        
    UInt8			XONchar;
    UInt8			XOFFchar;
    UInt32			SWspecial[(0x100)>>SPECIAL_SHIFT];
    UInt32			FlowControl;				// notify-on-delta & auto_control
    UInt32			FlowControlState;
    bool			DCDState;
    bool			CTSState;
    int				RXOstate;				// Indicates our receive state
    int				TXOstate;				// Indicates our transmit state, if we have received any Flow Control
    UInt8			xOffSent;
    UInt32 			GlobalRecvChars;
    UInt32  			OverRunChars;
    bool			BreakState;
        
    mach_timespec		DataLatInterval;
    mach_timespec		CharLatInterval;
    
        // Statistics
        
    Stats_t			Stats;
    bool			AreTransmitting;
    bool			GotTXInterrupt;

        // chip dependent
        
    bool			baudRateGeneratorEnable;
    UInt8			baudRateGeneratorLo;
    UInt8			baudRateGeneratorHi;
    UInt32			rtxcFrequency;
    UInt8			lastWR[kNumSCCWR];

    UInt8			*ControlRegister;
    UInt8			*DataRegister;
    IOPhysicalAddress		ChipBaseAddress;
    UInt32			ConfigWriteRegister;
    SerialPortSelector		whichPort;
    IOPhysicalAddress		TxDMABase;
    UInt32			TxDMALength;
    IOPhysicalAddress		RxDMABase;
    UInt32			RxDMALength;

    UInt8			InterruptNumbers[MaxInterrupts];

        // enable bits
        
    UInt8			ourReceiveBits;
    UInt8			ourTransmitBits;
    UInt8			ourClockSource;
    bool			haveQueuedRxSIH;
    bool			haveQueuedTxSIH;

        // gDBDMAChannelAddress, gChannelCommandArea reference the DBDMA channel
        // command area. This is a kernel memory area.

    SerialDBDMAStatusInfo	TxDBDMAChannel;
    SerialDBDMAStatusInfo	RxDBDMAChannel;
    
    bool			DTRAsserted;
    bool			aboveRxHighWater;
    
    bool			lastCTSState;
    UInt32			lastCTSTime;
    UInt32			ctsTransitionCount;
    
    IOTimerEventSource		*rxTimer;
} PortInfo_t, SccChannel;

extern vm_map_t kernel_map;	// _t (-*) need to verify if correct ***

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

#include "SccChip.h"

//typedef void (*SCCCarrierHack)(OSObject *target, bool carrier);

#if LOG_DATA
    UInt8 Asciify(UInt8 i);
    void SerialLogData(UInt8 Dir, UInt32 Count, char *buf);
#endif

//class AppleRS232Serial : public IORS232SerialStreamSync
class AppleRS232Serial : public IOSerialDriverSync
{
    OSDeclareDefaultStructors(AppleRS232Serial)
    
protected:

    bool		portOpened;
    AbsoluteTime	startingTime;

//    virtual	bool		initForPM(IOService *provider);
//    virtual	unsigned long	initialPowerStateForDomainState ( IOPMPowerFlags );
//    virtual	IOReturn	setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);

public:

	// Instance variables
        
    IOWorkLoop			*fWorkLoop;		// holds the workloop for this driver
    IOCommandGate		*fCommandGate;		// and the command gate
    IOTimerEventSource		*fTimer;		// holds the timer we create
    UInt32			fCounter;		// counter incremented each time the timeout handler is called

    PortInfo_t			fPort;
    IOService			*fProvider;

    IOInterruptEventSource		*sccInterruptSource;
#if USE_FILTER_EVENT_SOURCES
    IOFilterInterruptEventSource	*txDMAInterruptSource;
    IOFilterInterruptEventSource	*rxDMAInterruptSource;
#else
    IOInterruptEventSource		*txDMAInterruptSource;
    IOInterruptEventSource		*rxDMAInterruptSource;
#endif

        // IOSerialStreamSync methods
    
    virtual	IOReturn	acquirePort(bool sleep, void *refCon);
    virtual	IOReturn	releasePort(void *refCon);
    virtual	UInt32		getState(void *refCon);
    virtual	IOReturn	setState(UInt32 state, UInt32 mask, void *refCon);
    virtual	IOReturn	watchState(UInt32 *state, UInt32 mask, void *refCon);
    virtual     UInt32		nextEvent(void *refCon);
    virtual	IOReturn	executeEvent(UInt32 event, UInt32 data, void *refCon);
    virtual	IOReturn	requestEvent(UInt32 event, UInt32 *data, void *refCon);
    virtual	IOReturn	enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon);
    virtual	IOReturn	dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon);
    virtual	IOReturn	enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon);
    virtual	IOReturn	dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon);

        // Static stubs for IOCommandGate::runAction
        
    static	IOReturn	acquirePortAction(OSObject *owner, void *arg0, void *, void *, void *);
    static	IOReturn	releasePortAction(OSObject *owner, void *, void *, void *, void *);
    static	IOReturn	getStateAction(OSObject *owner, void *, void *, void *, void *);
    static	IOReturn	setStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	watchStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	executeEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	enqueueEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	enqueueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
    static	IOReturn	dequeueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
    
        // Gated methods called by the Static stubs

    virtual	IOReturn	acquirePortGated(bool sleep);
    virtual	IOReturn	releasePortGated(void);
    virtual	UInt32		getStateGated(void);
    virtual	IOReturn	setStateGated(UInt32 state, UInt32 mask);
    virtual	IOReturn	watchStateGated(UInt32 *state, UInt32 mask);
    virtual	IOReturn	executeEventGated(UInt32 event, UInt32 data);
    virtual	IOReturn	enqueueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep);
    virtual	IOReturn	dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);

        // IOKit methods
    
    virtual	bool		start(IOService *provider);
    virtual	void		stop(IOService *provider);

        // Registry & RS232 handling methods
        
                void 		shutDown();
                IOReturn	sleepThread(void *event);

    static	void		interruptOccurred(OSObject *obj, IOInterruptEventSource *src, int count);
                bool		createSerialStream(IOService *provider);

    static	bool		allocateRingBuffer(CirQueue *Queue, size_t BufferSize);
    static	void		freeRingBuffer(CirQueue *Queue);

                bool		initializePort(PortInfo_t *port);
    static	void		setStructureDefaults(PortInfo_t *port);

                void		CheckQueues(PortInfo_t *port);

        // Interrupt handler
        
    static	void		handleInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
    static	void		handleDBDMATxInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
    static	void		handleDBDMARxInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
  
    static	void		timeoutHandler(OSObject *owner, IOTimerEventSource *sender);

    static	bool		interruptFilter(OSObject *owner, IOFilterInterruptEventSource *source);
    static	void		interruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
    
    // power management
    bool			initForPM(IOService *policyMaker);
    IOReturn			setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice);
    unsigned long		powerStateForDomainState(IOPMPowerFlags domainState );
    unsigned long		maxCapabilityForDomainState(IOPMPowerFlags domainState);
    unsigned long		initialPowerStateForDomainState(IOPMPowerFlags domainState);

    static void			handleSetPowerState(thread_call_param_t param0, thread_call_param_t param1);
    static IOReturn		setPowerStateGated(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);

    // private power state stuff
    thread_call_t		fPowerThreadCall;
    bool			fWaitForGatedCmd;
    unsigned int		fCurrentPowerState;     // current power state (0 or 1)

};
#endif
