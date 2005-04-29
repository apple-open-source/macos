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
#define USE_ELG		1			// to event log - LDEBUG must also be set
#define	LOG_DATA	0			// logs data to the IOLog - LDEBUG must also be set
#define DUMPALL		0			// Dumps all the data to the log - LOG_DATA must also be set

#define Sleep_Time	300			// 20 ms per iolog

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
        #define XTRACE2(x, y, msg) XTRACE_HELPER(gXTrace, gTraceID, x, y, "AppleRS232Serial: "  msg, true)
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


enum
{
	kSerialOut = 0,		    // debug only, used for LogData() direction
	kSerialIn = 1,
	kSerialOther = 2
};

enum
{
    R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15
};

#define USE_FILTER_EVENT_SOURCES	0

#define BUFFER_SIZE_DEFAULT	(16*1024)	    // tx and rx queues are always this big

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

    // Interrupts

typedef enum InterruptTypes
{
    kTxInterrupts		= 4,  		//SCC chip level
    kRxInterrupts		= 5,
    kSccInterrupts		= 6,		// sets/clears chip interrupts
} InterruptTypes;


//#define channelDataOffsetRISC		0x010
#define channelControlOffsetRISC	0x000
//#define channelDataOffset		4
//#define channelControlOffset		0

enum InterruptAssignments
{
    kIntChipSet			= 0,
    kIntTxDMA,
    kIntRxDMA
};
	
//#define DMABufferSize		4096

enum ParityType
{
    NoParity 			= 0,
    OddParity,
    EvenParity
};

enum SerialPortSelector
{
    serialPortA			= 0,
    serialPortB			= 1
};

#define MIN_BAUD (50 << 1)
#define kDefaultBaudRate	9600
#define kMaxBaudRate		230400

typedef struct SerialDBDMAStatusInfo
{
    IODBDMAChannelRegisters	*dmaBase;                 	// address of DBDMA channel regs
    IODBDMADescriptor 		*dmaChannelCommandArea;    	// room for DBDMA channel program commands (shared pool)
    IODBDMADescriptor		*dmaChannelCommandAreaPhysical;	// DBDMA channel program, physical address of above
    UInt32			dmaNumberOfDescriptors;    	// number of dbdma command descriptors allocated in above
    UInt8			*dmaTransferBuffer;        	// alloc'd buffer for read/writes (always PAGE_SIZE for now)
    UInt32			dmaTransferSize;           	// byte count to move (tx) or amount that has been read in (rx)
    IOBufferMemoryDescriptor	*dmaTransferBufferMDP; 	   	// memory descriptor for alloc & memory prepare of dmaTransferBuffer
} SerialDBDMAStatusInfo;


typedef struct BufferMarks {		    // high/low/size for tx/rx queues
    UInt32	BufferSize;		    // currenty fixed at 16k per queue (BUFFER_SIZE_DEFAULT)
    UInt32	HighWater;		    // currently fixed at 2/3 of buffersize
    UInt32	LowWater;		    // currently fixed at 1/3 of buffersize
} BufferMarks;

enum {					    // indexes/counts for manipulating/allocating the rx/tx dbdma command chains
	kRxDBDMACmd_First_Read = 0,		// a single byte read to generate an interrupt (when desired)
	kRxDBDMACmd_Main_Read,			// then a big read for the rest of the buffer
	kRxDBDMACmd_Stop,			// finally a stop
	kNumberOfRxDBDMACommands,		// count of commands we need per rx dbdma command set

	kTxDBDMACmd_Write = 0,			// tx has a write and a stop
	kTxDBDMACmd_Stop,
	kNumberOfTxDBDMACommands
};

enum {						// sizes of the dbdma transfer buffers
	kRxDBDMABufferSize = 4096,		// each of the two rx buffers is this big
	kTxDBDMABufferSize = 4096		// one tx buffer
};

class AppleRS232Serial;

typedef struct
{
    UInt32			State;
    UInt32			WatchStateMask;
    
    AppleRS232Serial		*RS232;
    
        // queue control structures
    
    CirQueue			RX;
    CirQueue			TX;
    BufferMarks			RXStats;
    BufferMarks			TXStats;
        
	
        // UART configuration info
    
    UInt32			CharLength;	    // 5 thru 8 bits per byte
    UInt32			StopBits;	    // 1, 1.5 or 2 stop bits (x<<1)
    UInt32			TX_Parity;
    UInt32			RX_Parity;
    UInt32			BaudRate;	    // [MIN_BAUD .. kMaxBaudRate]
    bool                	MinLatency;	    // true iff low latency req (not impl yet)

        // flow control state & configuration
        
    UInt8			XONchar;				// defaults to ^Q
    UInt8			XOFFchar;				// defaults to ^S
    UInt32			SWspecial[(0x100)>>SPECIAL_SHIFT];	// a bit to flag each of [0..255] byte codes as special.  not impl
    UInt32			FlowControl;				// notify-on-delta & auto_control
    UInt32			FlowControlState;			// tx flow control state, one of PAUSE_SEND if paused or CONTINUE_SEND if not blocked
    bool			DCDState;
    bool			CTSState;
    int				RXOstate;				// Indicates our receive state
    int				TXOstate;				// Indicates our transmit state, if we have received any Flow Control
    bool			xOffSent;				// init false, set true if sw flow control and we've sent an xoff
    bool			DTRAsserted;				// init true, set false if DTR flow control and DTR is cleared to hold back rx
    bool			RTSAsserted;				// init true, set false if RTS flow control and RTS is cleared to hold back rx
    bool			aboveRxHighWater;
    bool			BreakState;
        
    mach_timespec		DataLatInterval;
    mach_timespec		CharLatInterval;			// saved & returned but not impl'd
    
    bool			AreTransmitting;

        // chip dependent
        
    UInt8			baudRateGeneratorLo;
    UInt8			baudRateGeneratorHi;
    UInt32			rtxcFrequency;
    UInt8			lastWR[kNumSCCWR];

    IOVirtualAddress		ChipBaseAddress;		// virtual address of the chip
    IOPhysicalAddress		ChipBaseAddressPhysical;	// physical addr of the chip
    IOVirtualAddress		ControlRegister;		// virtual address of the control reg
    //UInt8			*DataRegister;
    //UInt32			ConfigWriteRegister;
    SerialPortSelector		whichPort;			// serialPortA or serialPortB


    IOBufferMemoryDescriptor	*dmaChannelCommandAreaMDP; 	// descriptor for alloc & memory prepare of rx/tx channel command pool
    SerialDBDMAStatusInfo	TxDBDMAChannel;			// tx dma info
    SerialDBDMAStatusInfo	rxDBDMAChannels[2];		// rx is double buffered now 
    int				activeRxChannelIndex;		// index to either rxDBDMAChannels[0] or rxDBDMAChannels[1]
    
    
    bool			lastCTSState;			// for midi cts insanity check
    UInt32			lastCTSTime;
    UInt32			ctsTransitionCount;
    
    IOTimerEventSource		*rxTimer;
} PortInfo_t, SccChannel;


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


#if LOG_DATA
    UInt8 Asciify(UInt8 i);
    void SerialLogData(UInt8 Dir, UInt32 Count, char *buf);
#endif

class AppleRS232Serial : public IOSerialDriverSync
{
    OSDeclareDefaultStructors(AppleRS232Serial)
    
protected:

    bool		portOpened;
    AbsoluteTime	startingTime;

public:

	// Instance variables
        
    IOWorkLoop			*fWorkLoop;		// holds the workloop for this driver
    IOCommandGate		*fCommandGate;		// and the command gate

    PortInfo_t			fPort;
    IOService			*fProvider;

    thread_call_t		fdmaStartTransmissionThread;
    thread_call_t		dmaRxHandleCurrentPositionThread;


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
    virtual	IOWorkLoop	*getWorkLoop() const;

        // Static stubs for IOCommandGate::runAction
        
    static	IOReturn	acquirePortAction(OSObject *owner, void *arg0, void *, void *, void *);
    static	IOReturn	releasePortAction(OSObject *owner, void *, void *, void *, void *);
    static	IOReturn	getStateAction(OSObject *owner, void *, void *, void *, void *);
    static	IOReturn	setStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	watchStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	executeEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	requestEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
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
    virtual	IOReturn	requestEventGated(UInt32 event, UInt32 *data);
    virtual	IOReturn	enqueueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep);
    virtual	IOReturn	dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);

        // IOKit methods
    
    virtual	bool		start(IOService *provider);
    virtual	void		stop(IOService *provider);

        // Registry & RS232 handling methods
        
                void 		shutDown();

    static	void		interruptOccurred(OSObject *obj, IOInterruptEventSource *src, int count);
                bool		createSerialStream(IOService *provider);

    static	bool		allocateRingBuffer(CirQueue *Queue, size_t BufferSize, IOWorkLoop *workloop);
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
