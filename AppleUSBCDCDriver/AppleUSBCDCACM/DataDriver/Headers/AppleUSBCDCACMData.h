/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef __APPLEUSBCDCACMData__
#define __APPLEUSBCDCACMData__

#include "AppleUSBCDCCommon.h"
#include "AppleUSBCDC.h"
#include "AppleUSBCDCACMControl.h"

#define baseName		"usbmodem"
#define defaultName		"USB Modem"
#define productNameLength	32						// Arbitrary length
#define propertyTag		"Product Name"

#define kDefaultBaudRate	9600
#define kMaxBaudRate		230400
#define kMaxCirBufferSize	4096

    // Default and Maximum buffer pool values

#define kInBufPool		4
#define kOutBufPool		2

#define kMaxInBufPool		kInBufPool*16
#define kMaxOutBufPool		kOutBufPool*8

#define	inputTag		"InputBuffers"
#define	outputTag		"OutputBuffers"

    // SccQueuePrimatives.h

typedef struct CirQueue
{
    UInt8	*Start;
    UInt8	*End;
    UInt8	*NextChar;
    UInt8	*LastChar;
    size_t	Size;
    size_t	InQueue;
} CirQueue;

typedef enum QueueStatus
{
    queueNoError = 0,
    queueFull,
    queueEmpty,
    queueMaxStatus
} QueueStatus;

    // Miscellaneous
        
#define BIGGEST_EVENT		3

#define SPECIAL_SHIFT		(5)
#define SPECIAL_MASK		((1<<SPECIAL_SHIFT) - 1)
#define STATE_ALL		(PD_RS232_S_MASK | PD_S_MASK)
#define FLOW_RX_AUTO   	 	(PD_RS232_A_RFR | PD_RS232_A_DTR | PD_RS232_A_RXO)
#define FLOW_TX_AUTO    	(PD_RS232_A_CTS | PD_RS232_A_DSR | PD_RS232_A_TXO | PD_RS232_A_DCD)
#define CAN_BE_AUTO		(FLOW_RX_AUTO | FLOW_TX_AUTO)
#define CAN_NOTIFY		(PD_RS232_N_MASK)
#define EXTERNAL_MASK   	(PD_S_MASK | (PD_RS232_S_MASK & ~PD_RS232_S_LOOP))
#define INTERNAL_DELAY  	(PD_RS232_S_LOOP)
#define DEFAULT_AUTO		(PD_RS232_A_RFR | PD_RS232_A_CTS | PD_RS232_A_DSR)
#define DEFAULT_NOTIFY		0x00
#define DEFAULT_STATE		(PD_S_TX_ENABLE | PD_S_RX_ENABLE | PD_RS232_A_TXO | PD_RS232_A_RXO)

#define IDLE_XO	   		 0
#define NEEDS_XOFF 		 1
#define SENT_XOFF 		-1
#define NEEDS_XON  		 2
#define SENT_XON  		-2

#define MAX_BLOCK_SIZE	PAGE_SIZE
#define COMM_BUFF_SIZE	16
#define DATA_BUFF_SIZE	1024

typedef struct
{
    UInt32	ints;
    UInt32	txInts;
    UInt32	rxInts;
    UInt32	mdmInts;
    UInt32	txChars;
    UInt32	rxChars;
} Stats_t;

typedef struct BufferMarks
{
    unsigned long	BufferSize;
    unsigned long	HighWater;
    unsigned long	LowWater;
    bool		OverRun;
} BufferMarks;

typedef struct 
{
    IOBufferMemoryDescriptor	*pipeMDP;
    UInt8			*pipeBuffer;
//    SInt32			count;
    bool			dead;
    IOUSBCompletion		completionInfo;
} inPipeBuffers;

typedef struct 
{
    IOBufferMemoryDescriptor	*pipeMDP;
    UInt8			*pipeBuffer;
    SInt32			count;
    bool			avail;
    IOUSBCompletion		completionInfo;
} outPipeBuffers;

typedef struct
{

        // State and serialization variables

    UInt32		State;
    UInt32		WatchStateMask;

        // queue control structures:
			
    CirQueue		RX;
    CirQueue		TX;

    BufferMarks		RXStats;
    BufferMarks		TXStats;
	
        // UART configuration info:
			
    UInt32		CharLength;
    UInt32		StopBits;
    UInt32		TX_Parity;
    UInt32		RX_Parity;
    UInt32		BaudRate;
    UInt8		FCRimage;
    UInt8		IERmask;
    bool            	MinLatency;
	
        // flow control state & configuration:
			
    UInt8		XONchar;
    UInt8		XOFFchar;
    UInt32		SWspecial[ 0x100 >> SPECIAL_SHIFT ];
    UInt32		FlowControl;			// notify-on-delta & auto_control
		
    int			RXOstate;    			// Indicates our receive state.
    int			TXOstate;			// Indicates our transmit state, if we have received any Flow Control.
	
    IOThread		FrameTOEntry;
	
    mach_timespec	DataLatInterval;
    mach_timespec	CharLatInterval;
	
        // extensions for USB Driver
    
    IOUSBPipe		*InPipe;
    IOUSBPipe		*OutPipe;
    
    inPipeBuffers       inPool[kMaxInBufPool];
    outPipeBuffers      outPool[kMaxOutBufPool];
    UInt16		outPoolIndex;
    
    UInt8		CommInterfaceNumber;
    UInt8		DataInterfaceNumber;

    UInt32		OutPacketSize;
    UInt32		InPacketSize;
		
    UInt32		LastCharLength;
    UInt32		LastStopBits;
    UInt32		LastTX_Parity;
    UInt32		LastBaudRate;

} PortInfo_t;

class AppleUSBCDC;
class AppleUSBCDCACMControl;

	/* AppleUSBCDCACMData.h - This file contains the class definition for the		*/
	/* USB Communication Device Class (CDC) Data Interface driver - ACM only at the moment.	*/

class AppleUSBCDCACMData : public IOSerialDriverSync
{
    OSDeclareDefaultStructors(AppleUSBCDCACMData);			// Constructor & Destructor stuff

private:
	AppleUSBCDC		*fCDCDriver;			// The CDC driver
	AppleUSBCDCACMControl   *fControlDriver;			// Our Control Driver
    UInt16			fSessions;				// Number of active sessions
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    bool			fStopping;				// Are we being "stopped"
	bool			fResetOnClose;			// Do we need to reset the device on closing
    UInt8			fProductName[productNameLength];	// Product String from the Device
    
    static void			dataReadComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			dataWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);

public:

    IOUSBInterface		*fDataInterface;
    IOWorkLoop			*fWorkLoop;
    IOCommandGate		*fCommandGate;
    PortInfo_t 			fPort;					// Port structure
    
    UInt16			fInBufPool;
    UInt16			fOutBufPool;
    
    UInt8			fConfigAttributes;			// Configuration descriptor attributes

        // IOKit methods:
		
	virtual IOService   *probe(IOService *provider, SInt32 *score);
    virtual bool		start(IOService *provider);
    virtual void		stop(IOService *provider);
    virtual IOReturn 		message(UInt32 type, IOService *provider,  void *argument = 0);

        // IOSerialDriverSync Abstract Method Implementation

    virtual IOReturn		acquirePort(bool sleep, void *refCon);
    virtual IOReturn		releasePort(void *refCon);
    virtual UInt32		getState(void *refCon);
    virtual IOReturn		setState(UInt32 state, UInt32 mask, void *refCon);
    virtual IOReturn		watchState(UInt32 *state, UInt32 mask, void *refCon);
    virtual UInt32		nextEvent(void *refCon);
    virtual IOReturn		executeEvent(UInt32 event, UInt32 data, void *refCon);
    virtual IOReturn		requestEvent(UInt32 event, UInt32 *data, void *refCon);
    virtual IOReturn		enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon);
    virtual IOReturn		dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon);
    virtual IOReturn		enqueueData(UInt8 *buffer, UInt32 size, UInt32 * count, bool sleep, void *refCon);
    virtual IOReturn		dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon);
    
        // Static stubs for IOCommandGate::runAction
        
    static	IOReturn	stopAction(OSObject *owner, void *, void *, void *, void *);
    static	IOReturn	acquirePortAction(OSObject *owner, void *arg0, void *, void *, void *);
    static	IOReturn	releasePortAction(OSObject *owner, void *, void *, void *, void *);
    static	IOReturn	getStateAction(OSObject *owner, void *, void *, void *, void *);
    static	IOReturn	setStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	watchStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	executeEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *);
    static	IOReturn	enqueueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
    static	IOReturn	dequeueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
    
        // Gated methods called by the Static stubs

    virtual	void		stopGated(void);
    virtual	IOReturn	acquirePortGated(bool sleep);
    virtual	IOReturn	releasePortGated(void);
    virtual	UInt32		getStateGated(void);
    virtual	IOReturn	setStateGated(UInt32 state, UInt32 mask);
    virtual	IOReturn	watchStateGated(UInt32 *state, UInt32 mask);
    virtual	IOReturn	executeEventGated(UInt32 event, UInt32 data);
    virtual	IOReturn	enqueueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep);
    virtual	IOReturn	dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);
												
        // CDC Data Driver Methods
	
    void			USBLogData(UInt8 Dir, UInt32 Count, char *buf);
    bool 			createSuffix(unsigned char *sufKey);
    bool			createSerialStream(void);
    bool 			setUpTransmit(void);
    void 			startTransmission(void);
    void 			setLineCoding(void);
    void 			setControlLineState(bool RTS, bool DTR);
    void 			sendBreak(bool sBreak);
    IOReturn		checkPipe(IOUSBPipe *thePipe, bool devReq);
    void 			initStructure(void);
    void 			setStructureDefaults(void);
    bool 			allocateResources(void);
    void			releaseResources(void);
    void 			freeRingBuffer(CirQueue *Queue);
    bool 			allocateRingBuffer(CirQueue *Queue, size_t BufferSize);
    bool			WakeonRing(void);
	void			setWakeFeature(void);
	void			resurrectRead(void);

private:

	// QueuePrimatives
        
    QueueStatus			AddBytetoQueue(CirQueue *Queue, char Value);
    QueueStatus			GetBytetoQueue(CirQueue *Queue, UInt8 *Value);
    QueueStatus			InitQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size);
    QueueStatus			CloseQueue(CirQueue *Queue);
    size_t 			AddtoQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size);
    size_t 			RemovefromQueue(CirQueue *Queue, UInt8 *Buffer, size_t MaxSize);
    size_t 			FreeSpaceinQueue(CirQueue *Queue);
    size_t 			UsedSpaceinQueue(CirQueue *Queue);
    size_t 			GetQueueSize(CirQueue *Queue);
    QueueStatus 		GetQueueStatus(CirQueue *Queue);
    void 			CheckQueues(void);
    
}; /* end class AppleUSBCDCACMData */
#endif