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
	/*****	For fans of kprintf, IOLog and debugging infrastructure of the	*****/
	/*****	string ilk, please modify the ELG and PAUSE macros or their	*****/
	/*****	associated EvLog and Pause functions to suit your taste. These	*****/
	/*****	macros currently are set up to log events to a wraparound	*****/
	/*****	buffer with minimal performance impact. They take 2 UInt32	*****/
	/*****	parameters so that when the buffer is dumped 16 bytes per line,	*****/
	/*****	time stamps (~1 microsecond) run down the left side while	*****/
	/*****	unique 4-byte ASCII codes can be read down the right side.	*****/
	/*****	Preserving this convention facilitates different maintainers	*****/
	/*****	using different debugging styles with minimal code clutter.	*****/

#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to event log - LDEBUG must also be set
#define kEvLogSize  (4096*16)			// 16 pages = 64K = 4096 events
#define	LOG_DATA	0			// logs data to the IOLog - LDEBUG must also be set

#define Sleep_Time	20

#if LDEBUG
    #if USE_ELG
        #define ELG(A,B,ASCI,STRING)    EvLog( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )		
    #else /* not USE_ELG */
        #define ELG(A,B,ASCI,STRING)	{IOLog( "AppleUSBCDCDriver: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B) );IOSleep(Sleep_Time);}
    #endif /* USE_ELG */
    #if LOG_DATA
        #define LogData(D, C, b)	USBLogData((UInt8)D, (UInt32)C, (char *)b)
    #else /* not LOG_DATA */
        #define LogData(D, C, b)
    #endif /* LOG_DATA */
#else /* not LDEBUG */
    #define ELG(A,B,ASCI,S)
    #define LogData(D, C, b)
    #undef USE_ELG
    #undef LOG_DATA
#endif /* LDEBUG */

#define IOLogIt(A,B,ASCI,STRING)	IOLog( "AppleUSBCDCDriver: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B) )

#define baseName		"usbmodem"
#define defaultName		"USB Modem"
#define numberofPorts		4						// Number of ports we'll support
#define productNameLength	32						// Arbitrary length
#define propertyTag		"Product Name"

        // USB CDC Defintions
		
#define kUSBAbstractControlModel	2
#define kUSBv25				1		
	
#define kUSBbRxCarrier			0x01			// Carrier Detect
#define kUSBDCD				kUSBbRxCarrier
#define kUSBbTxCarrier			0x02			// Data Set Ready
#define kUSBDSR				kUSBbTxCarrier
#define kUSBbBreak			0x04
#define kUSBbRingSignal			0x08
#define kUSBbFraming			0x10
#define kUSBbParity			0x20
#define kUSBbOverRun			0x40

#define kDTROff				0
#define kRTSOff				0
#define kDTROn				1
#define kRTSOn				2

enum
{
    kUSBSEND_ENCAPSULATED_COMMAND 	= 0,			// Requests
    kUSBGET_ENCAPSULATED_RESPONSE 	= 1,
    kUSBSET_COMM_FEATURE 		= 2,
    kUSBGET_COMM_FEATURE 		= 3,
    kUSBCLEAR_COMM_FEATURE 		= 4,
    kUSBSET_LINE_CODING 		= 0x20,
    kUSBGET_LINE_CODING 		= 0x21,
    kUSBSET_CONTROL_LINE_STATE 		= 0x22,
    kUSBSEND_BREAK 			= 0x23
};
	
enum
{
    kUSBNETWORK_CONNECTION 		= 0,			// Notifications
    kUSBRESPONSE_AVAILABLE 		= 1,
    kUSBSERIAL_STATE 			= 0x20
};
	
typedef struct
{	
    UInt32	dwDTERate;
    UInt8	bCharFormat;
    UInt8	bParityType;
    UInt8	bDataBits;
} LineCoding;
	
#define dwDTERateOffset	0

#define wValueOffset	2
#define wIndexOffset	4
#define wLengthOffset	6

enum
{
    CS_INTERFACE		= 0x24,
		
    Header_FunctionalDescriptor	= 0x00,
    CM_FunctionalDescriptor	= 0x01,
    ACM_FunctionalDescriptor	= 0x02,
    Union_FunctionalDescriptor	= 0x06,
    CS_FunctionalDescriptor	= 0x07,
		
    CM_ManagementData		= 0x01,
    CM_ManagementOnData		= 0x02,
		
    ACM_DeviceSuppCommFeature	= 0x01,
    ACM_DeviceSuppControl	= 0x02,
    ACM_DeviceSuppBreak		= 0x04,
    ACM_DeviceSuppNetConnect	= 0x08
};
	
typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
} FunctionalDescriptorHeader;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bmCapabilities;
    UInt8 	bDataInterface;
} CMFunctionalDescriptor;
	
typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bmCapabilities;
} ACMFunctionalDescriptor;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bMasterInterface;
    UInt8	bSlaveInterface[];
} UnionFunctionalDescriptor;

    // Globals

typedef struct globals      // Globals for this module (not per instance)
{
    UInt32      evLogFlag; // debugging only
    UInt8       *evLogBuf;
    UInt8       *evLogBufe;
    UInt8       *evLogBufp;
    UInt8       intLevel;
    class AppleUSBCDCDriver	*AppleUSBCDCDriverInstance;
} globals;
		
enum ParityType
{
    NoParity 		= 0,
    OddParity,
    EvenParity
};
	
#define kDefaultBaudRate	9600
#define kMaxBaudRate		230400
#define kMaxCirBufferSize	4096

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

    // PPCSerialPort.h

#define BIGGEST_EVENT		3

#define SPECIAL_SHIFT		(5)
#define SPECIAL_MASK		((1<<SPECIAL_SHIFT) - 1)
#define STATE_ALL		( PD_RS232_S_MASK | PD_S_MASK )
#define FLOW_RX_AUTO   	 	( PD_RS232_A_RFR | PD_RS232_A_DTR | PD_RS232_A_RXO )
#define FLOW_TX_AUTO    	( PD_RS232_A_CTS | PD_RS232_A_DSR | PD_RS232_A_TXO | PD_RS232_A_DCD )
#define CAN_BE_AUTO		( FLOW_RX_AUTO | FLOW_TX_AUTO )
#define CAN_NOTIFY		( PD_RS232_N_MASK )
#define EXTERNAL_MASK   	( PD_S_MASK | (PD_RS232_S_MASK & ~PD_RS232_S_LOOP) )
#define INTERNAL_DELAY  	( PD_RS232_S_LOOP )
#define DEFAULT_AUTO		( PD_RS232_A_RFR | PD_RS232_A_CTS | PD_RS232_A_DSR )
#define DEFAULT_NOTIFY		0x00
#define DEFAULT_STATE		( PD_S_TX_ENABLE | PD_S_RX_ENABLE | PD_RS232_A_TXO | PD_RS232_A_RXO )

#define IDLE_XO	   		 0
#define NEEDS_XOFF 		 1
#define SENT_XOFF 		-1
#define NEEDS_XON  		 2
#define SENT_XON  		-2

#define MAX_BLOCK_SIZE	PAGE_SIZE
#define COMM_BUFF_SIZE	16

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
    UInt32		State;
    UInt32		WatchStateMask;
    IOLock		*serialRequestLock;

        // queue control structures:
			
    CirQueue		RX;
    CirQueue		TX;
    IOLock		*RXqueueRequestLock;
    IOLock		*TXqueueRequestLock;

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
	
    bool		AreTransmitting;
	
        // extensions for USB Driver
    
    IOUSBInterface	*CommInterface;
    IOUSBInterface	*DataInterface;
    
    IOUSBPipe		*InPipe;
    IOUSBPipe		*OutPipe;
    IOUSBPipe		*CommPipe;
    
    IOBufferMemoryDescriptor	*CommPipeMDP;
    IOBufferMemoryDescriptor	*PipeInMDP;
    IOBufferMemoryDescriptor	*PipeOutMDP;

    UInt8		*CommPipeBuffer;
    UInt8		*PipeInBuffer;
    UInt8		*PipeOutBuffer;
    
    UInt8		CommInterfaceNumber;
    UInt8		DataInterfaceNumber;
    UInt32		Count;
    UInt32		OutPacketSize;
    
    UInt8		CMCapabilities;			// Call Management Capabilities
    UInt8		ACMCapabilities;		// Abstract Control Management
		
    UInt32		LastCharLength;
    UInt32		LastStopBits;
    UInt32		LastTX_Parity;
    UInt32		LastBaudRate;
		
} PortInfo_t;
	
    // Inline time conversions
	
static inline unsigned long tval2long( mach_timespec val )
{
   return (val.tv_sec * NSEC_PER_SEC) + val.tv_nsec;   
}

static inline mach_timespec long2tval( unsigned long val )
{
    mach_timespec	tval;

    tval.tv_sec  = val / NSEC_PER_SEC;
    tval.tv_nsec = val % NSEC_PER_SEC;
    return tval;	
}

	/* AppleUSBCDCDriver.h - This file contains the class definition for the	*/
	/* USB Communication Device Class (CDC) driver - ACM only at the moment. 	*/

class AppleUSBCDCDriver : public IOSerialDriverSync
{
    OSDeclareDefaultStructors( AppleUSBCDCDriver );	// Constructor & Destructor stuff

private:
    UInt8			fSessions;				// Active sessions
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    bool			fSuspendOK;                             // Ok to suspend the device
    UInt8			fbmAttributes;				// Device attributes
    UInt8			fProductName[productNameLength];	// Product String from the Device
    PortInfo_t 			*fPorts[numberofPorts];			// Port array

    IOUSBCompletion		fCommCompletionInfo;
    IOUSBCompletion		fReadCompletionInfo;
    IOUSBCompletion		fWriteCompletionInfo;
    IOUSBCompletion		fMERCompletionInfo;

    static void			commReadComplete(  void *obj, void *param, IOReturn ior, UInt32 remaining );
    static void			dataReadComplete(  void *obj, void *param, IOReturn ior, UInt32 remaining );
    static void			dataWriteComplete( void *obj, void *param, IOReturn ior, UInt32 remaining );
    static void			merWriteComplete( void *obj, void *param, IOReturn ior, UInt32 remaining );

public:

    IOUSBDevice			*fpDevice;

        // IOKit methods:
		
    virtual void		free( void );
    virtual bool		start( IOService *provider );
    virtual void		stop( IOService *provider );
    virtual IOReturn 		message( UInt32 type, IOService *provider,  void *argument = 0 );

        // IOSerialDriverSync Abstract Method Implementation

    virtual IOReturn		acquirePort( bool sleep, void *refCon );
    virtual IOReturn		releasePort( void *refCon );
    virtual IOReturn		setState( UInt32 state, UInt32 mask, void *refCon );
    virtual UInt32		getState( void *refCon );
    virtual IOReturn		watchState( UInt32 *state, UInt32 mask, void *refCon );
    virtual UInt32		nextEvent( void *refCon );
    virtual IOReturn		executeEvent( UInt32 event, UInt32 data, void *refCon );
    virtual IOReturn		requestEvent( UInt32 event, UInt32 *data, void *refCon );
    virtual IOReturn		enqueueEvent( UInt32 event, UInt32 data, bool sleep, void *refCon );
    virtual IOReturn		dequeueEvent( UInt32 *event, UInt32 *data, bool sleep, void *refCon );
    virtual IOReturn		enqueueData( UInt8 *buffer, UInt32 size, UInt32 * count, bool sleep, void *refCon );
    virtual IOReturn		dequeueData( UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon );
												
        // CDC Driver Methods
	
    bool 			allocateResources( PortInfo_t *port );
    void			releaseResources( PortInfo_t *port );
    bool 			configureDevice( UInt8 numConfigs );
    bool			initDevice( UInt8 numConfigs );
    bool			getFunctionalDescriptors( PortInfo_t *port );
    bool 			createSuffix( unsigned char *sufKey );
    bool			createSerialStream( PortInfo_t *port );
    bool 			SetUpTransmit( PortInfo_t *port );
    void 			StartTransmission( PortInfo_t *port );
    void 			USBSetLineCoding( PortInfo_t *port );
    void 			USBSetControlLineState( PortInfo_t *port, bool RTS, bool DTR);
    void 			USBSendBreak( PortInfo_t *port, bool sBreak);
    void 			SetStructureDefaults( PortInfo_t *port, bool Init );
    void 			freeRingBuffer( CirQueue *Queue );
    bool 			allocateRingBuffer( CirQueue *Queue, size_t BufferSize );
    bool			WakeonRing();

private:

	// QueuePrimatives
        
    QueueStatus			AddBytetoQueue( CirQueue *Queue, char Value, IOLock *queueRequestLock );
    QueueStatus			GetBytetoQueue( CirQueue *Queue, UInt8 *Value, IOLock *queueRequestLock );
    QueueStatus			InitQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size );
    QueueStatus			CloseQueue( CirQueue *Queue );
    size_t 			AddtoQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size, IOLock *queueRequestLock );
    size_t 			RemovefromQueue( CirQueue *Queue, UInt8 *Buffer, size_t MaxSize, IOLock *queueRequestLock );
    size_t 			FreeSpaceinQueue( CirQueue *Queue, IOLock *queueRequestLock );
    size_t 			UsedSpaceinQueue( CirQueue *Queue );
    size_t 			GetQueueSize( CirQueue *Queue );
    QueueStatus 		GetQueueStatus( CirQueue *Queue );
    void 			CheckQueues( PortInfo_t *port );
    
        // Miscellaneous

    static IOReturn		privateWatchState( PortInfo_t *port, UInt32 *state, UInt32 mask );
    static UInt32		readPortState( PortInfo_t *port );
    static void			changeState( PortInfo_t *port, UInt32 state, UInt32 mask );
    
}; /* end class AppleUSBCDCDriver */

