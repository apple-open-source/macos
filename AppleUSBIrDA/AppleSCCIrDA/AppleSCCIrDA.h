
    /* Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _APPLESCCIRDA_
#define _APPLESCCIRDA_

#include <kern/thread_call.h>
#include "AppleIrDA.h"

#define SCCsuffix           "ch-b"                  // Revisit this

    /* PPCSerialPort.h  */

#define SPECIAL_SHIFT       (5)
#define SPECIAL_MASK        ((1<<SPECIAL_SHIFT) - 1)
#define STATE_ALL           ( PD_RS232_S_MASK | PD_S_MASK )
#define FLOW_RX_AUTO        ( PD_RS232_A_RFR | PD_RS232_A_DTR | PD_RS232_A_RXO )
#define FLOW_TX_AUTO        ( PD_RS232_A_CTS | PD_RS232_A_DSR | PD_RS232_A_TXO | PD_RS232_A_DCD )
#define CAN_BE_AUTO         ( FLOW_RX_AUTO | FLOW_TX_AUTO )
#define CAN_NOTIFY          ( PD_RS232_N_MASK )
#define EXTERNAL_MASK       ( PD_S_MASK | (PD_RS232_S_MASK & ~PD_RS232_S_LOOP) )
#define INTERNAL_DELAY      ( PD_RS232_S_LOOP )
#define DEFAULT_AUTO        ( PD_RS232_A_DTR | PD_RS232_A_RFR | PD_RS232_A_CTS | PD_RS232_A_DSR )
#define DEFAULT_NOTIFY      0x00
#define DEFAULT_STATE       ( PD_S_TX_ENABLE | PD_S_RX_ENABLE | PD_RS232_A_TXO | PD_RS232_A_RXO )

#define IDLE_XO          0
#define NEEDS_XOFF       1
#define SENT_XOFF       -1
#define NEEDS_XON        2
#define SENT_XON        -2

typedef struct BufferMarks
{
    unsigned long   BufferSize;
    unsigned long   HighWater;
    unsigned long   LowWater;
//    bool          OverRun;
} BufferMarks;

typedef struct
{
    UInt32          State;
    UInt32          WatchStateMask;
    IOLock          *serialRequestLock;

	// queue control structures:
	    
    CirQueue        RX;
    CirQueue        TX;

    BufferMarks     RXStats;
    BufferMarks     TXStats;
    
	// UART configuration info:
	    
    UInt32          CharLength;
    UInt32          StopBits;
    UInt32          TX_Parity;
    UInt32          RX_Parity;
    UInt32          BaudRate;
    UInt8           FCRimage;
    UInt8           IERmask;
    bool            MinLatency;
    
	// flow control state & configuration:
	    
    UInt8           XONchar;
    UInt8           XOFFchar;
    UInt32          SWspecial[ 0x100 >> SPECIAL_SHIFT ];
    UInt32          FlowControl;    // notify-on-delta & auto_control
	
    int             RXOstate;    /* Indicates our receive state.    */
    int             TXOstate;    /* Indicates our transmit state, if we have received any Flow Control. */
    
//    IOThread      FrameTOEntry;
    
    mach_timespec   DataLatInterval;
    mach_timespec   CharLatInterval;
    
    bool            AreTransmitting;
    
	/* extensions to handle the Driver */
	    
//    bool          isDriver;
//    void          *DriverPowerRegister;
//    UInt32            DriverPowerMask;
	
//    bool          readActive;
	
} PortInfo_t;


#define SCCLapPayLoad       (2*2048)+40         // 2k byte packes, plus byte doubling plus some overhead
    
#define kDefaultRawBaudRate 19200
#define kDefaultDataSize    8
#define kDefaultStopBits    1

enum
{
    kMode_SIR,                  // slow mode (up to 115k)
    kMode_MIR,                  // medium (not supported)
    kMode_FIR                   // fast mode (1mbit, 4mbit) 
};

enum                    // parse states extracting the packet from read data
{
    kParseStateIdle,                // looking for BOF
    kParseStateBOF,             // recv'd at least one BOF (0xC0, not 0xFF)
    kParseStateData,                // recv'd a non-pad byte (after a BOF)
    kParseStatePad              // recv'd a pad byte (after a BOF)
};

enum                                // command mode speed settings
{
    kSetModeHP115200_1_6    = 0x4d,
    kSetModeHP57600_1_6     = 0x4e,
    kSetModeHP38400_1_6     = 0x4f,
    kSetModeHP19200_1_6     = 0x50,
    kSetModeHP9600_1_6      = 0x51,
    kSetModeHP4800_1_6      = 0x52,
    kSetModeHP2400_1_6      = 0x53
};


class IrDAComm;

    // AppleSCCIrDA - contains definitions for IrDA

class AppleSCCIrDA : public AppleIrDASerial
{
    OSDeclareDefaultStructors( AppleSCCIrDA );      // Constructor & Destructor stuff

private:

    IOService           *fProvider;
    
    UInt8           fSessions;      // Active sessions
    bool            fTerminate;     // Are we being terminated (ie the device was unplugged)
    bool            fKillRead;      // Kill any application read thread
    
    IrDAComm            *fIrDA;         // IrDA object
    AppleIrDA           *fUserClientNub;    // nub to publish newUserClient for us
    PortInfo_t          *fPort;         // our pseudo tty port structure
    USBIrDAQoS          fQoS;           // Quality of Service
    bool            fIrDAOn;        // IrDA state (on or off)
    bool            fUserClientStarted; // User/client has started (stopped) us
    bool            fSCCStarted;        // iokit has started (stopped) us
    
    UInt32          fBaudCode;      // Current baud rate
    UInt8           fBofsCode;      // Extra bof count (encoded but unshifted)
    UInt8           fModeFlag;      // SIR or FIR mode (kMode_FIR or kMode_SIR)
    UInt8           fParseState;        // Keep track of read state
    UInt32          fICRCError;     // Input CRC error counter    
    
    UInt8           *fInBuffer;     // Raw input buffer
    UInt8           *fOutBuffer;        // Formatted output buffer
    UInt8           *fInUseBuffer;      // Parse input buffer
    UInt32          fDataLength;        // The current length of the data in the in use buffer
	
    bool            fSendingCommand;    // flag for rx thread to let SendCommand take incoming bytes
    bool            fSpeedChanging;     // flag for tx thread to defer until speed change finished
    thread_call_t   rx_thread_call;     // non-gated rx loop to let scc driver run w/out us blocking it
    thread_call_t   tx_thread_call;     // non-gated tx thread to let scc xmit run w/out us blocking it
    thread_call_t   speed_change_thread_call;   // non-gated speed change thread to do the hardware change dance
    UInt8           fPowerState;        // powered on or off as requested by policy maker

public:

    IORS232SerialStreamSync *fpDevice;  // up and over to the AppleSCCSerial driver
    
	// IOKit methods
	
    virtual bool        init( OSDictionary *dict );
    virtual IOService*      probe( IOService *provider, SInt32 *score );
    virtual bool        attach(IOService *provider);
    virtual void        detach(IOService *provider);
    virtual bool        start( IOService *provider );
    virtual void        free( void );
    virtual void        stop( IOService *provider );
    virtual IOReturn    message( UInt32 type, IOService *provider,  void *argument);
    virtual unsigned long initialPowerStateForDomainState ( IOPMPowerFlags );
    virtual IOReturn    setPowerState(unsigned long powerStateOrdinal, IOService * whatDevice);

	// IORS232SerialStreamSync Abstract Method Implementation

    virtual IOReturn        acquirePort( bool sleep );
    virtual IOReturn        releasePort( void );
    virtual IOReturn        setState( UInt32 state, UInt32 mask );
    virtual UInt32          getState( void );
    virtual IOReturn        watchState( UInt32 *state, UInt32 mask );
    virtual UInt32          nextEvent( void );
    virtual IOReturn        executeEvent( UInt32 event, UInt32 data );
    virtual IOReturn        requestEvent( UInt32 event, UInt32 *data );
    virtual IOReturn        enqueueEvent( UInt32 event, UInt32 data, bool sleep );
    virtual IOReturn        dequeueEvent( UInt32 *event, UInt32 *data, bool sleep );
    virtual IOReturn        enqueueData( UInt8 *buffer, UInt32 size, UInt32 * count, bool sleep );
    virtual IOReturn        dequeueData( UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min );
    
    void    SetStructureDefaults(PortInfo_t *port, bool Init);
    void    changeState(PortInfo_t *port, UInt32 state, UInt32 mask);
    UInt32  readPortState(PortInfo_t *port);
    void    CheckQueues( PortInfo_t *port );
    IOReturn    privateWatchState( PortInfo_t *port, UInt32 *state, UInt32 mask );


						
	// AppleSCCIrDA Methods
	// IrDA Specific (Public)
	
    void            Add_RXBytes( UInt8 *Buffer, size_t Size );
    SInt16          SetBofCount( SInt16 bof_count );
    UInt16          SetSpeed( UInt32 brate );
    IrDAComm*       GetIrDAComm( void );
    USBIrDAQoS*     GetIrDAQoS( void );
    bool            SetUpTransmit( void );
    IOReturn        StartTransmit( UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer );
    void            GetIrDAStatus( IrDAStatus *status );
    IOReturn        SetIrDAUserClientState( bool IrDAOn );
    IOReturn        CheckIrDAState( void );
    IOReturn        SetIrDAState( bool IrDAOn );

    static void rx_thread(thread_call_param_t parm0, thread_call_param_t parm1);
    static void tx_thread(thread_call_param_t parm0, thread_call_param_t parm1);
    static void speed_change_thread(thread_call_param_t parm0, thread_call_param_t parm1);
    
private:

	// Queue primatives
    
    QueueStatus         AddBytetoQueue( CirQueue *Queue, char Value );
    QueueStatus         GetBytetoQueue( CirQueue *Queue, UInt8 *Value );
    QueueStatus         InitQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size );
    QueueStatus         CloseQueue( CirQueue *Queue );
    size_t          AddtoQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size );
    size_t          RemovefromQueue( CirQueue *Queue, UInt8 *Buffer, size_t MaxSize );
    size_t          FreeSpaceinQueue( CirQueue *Queue );
    size_t          UsedSpaceinQueue( CirQueue *Queue );
    size_t          GetQueueSize( CirQueue *Queue );
    //QueueStatus           GetQueueStatus( CirQueue *Queue );
    
	// Class Specific
	
    bool            enableSCC( void );              // clocks on -- move to platform expert
    bool            disableSCC(void);               // clocks off -- move to platform expert
    IOReturn            sendBaud( UInt32 baud );
    IOReturn            sendCommand( UInt8 command, UInt8* result );
    IOReturn            getFIReWorksVersion( void );
    void            resetDTR( void );
    IOReturn        setupDevice( void );
    bool            configureDevice( void );
    bool            createSerialStream( void );
    void            freeRingBuffer( CirQueue *Queue );
    bool            allocateRingBuffer( CirQueue *Queue, size_t BufferSize );
    void            parseInputBuffer( UInt32 length, UInt8 *data );
    void            parseInputSIR( UInt32 length, UInt8 *data );
    void            parseInputFIR( UInt32 length, UInt8 *data );
    void            parseInputReset(void);
    void            SIRStuff(UInt8 byte, UInt8 **destPtr, UInt32 *lengthPtr);
    UInt32          Prepare_Buffer_For_Transmit(UInt8 *fOutBuffer, UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer );
    bool            initForPM(IOService *provider);
    
}; /* end class AppleSCCIrDA */

#endif
