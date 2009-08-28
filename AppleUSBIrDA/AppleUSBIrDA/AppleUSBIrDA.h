
    /* Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
     *
     * @APPLE_LICENSE_HEADER_START@
     * 
     * The contents of this file constitute Original Code as defined in and
     * are subject to the Apple Public Source License Version 1.1 (the
     * "License").  You may not use this file except in compliance with the
     * License.  Please obtain a copy of the License at
     * http://www.apple.com/publicsource and read it before using this file.
     * 
     * This Original Code and all software distributed under the License are
     * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
     * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
     * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
     * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
     * License for the specific language governing rights and limitations
     * under the License.
     * 
     * @APPLE_LICENSE_HEADER_END@
     */

#ifndef _APPLEUSBIRDA_
#define _APPLEUSBIRDA_

#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/serial/IOSerialDriverSync.h>

#include "AppleIrDA.h"

#define defaultName     "IrDA Device"
#define productNameLength   32                  // Arbitrary length
#define propertyTag     "Product Name"
    
    
    /* IrDA USB stuff */
    
#define USBIrDAClassDescriptor  0x21

enum                            // usb-irda bridge baud codes, in both rcv and xmit headers
{                           
    kLinkSpeedIgnored   = 0,
    kLinkSpeed2400  = 1,
    kLinkSpeed9600  = 2,
    kLinkSpeed19200 = 3,
    kLinkSpeed38400 = 4,
    kLinkSpeed57600 = 5,
    kLinkSpeed115200    = 6,
    kLinkSpeed576000    = 7,
    kLinkSpeed1152000   = 8,
    kLinkSpeed4000000   = 9,
    
    kLinkSpeedMask  = 0x0f  // low four bits in the inbound/outbound headers
};

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

#define INTERRUPT_BUFF_SIZE 1
#define USBLapPayLoad       2048

typedef struct
{
    UInt32  ints;
    UInt32  txInts;
    UInt32  rxInts;
    UInt32  mdmInts;
    UInt32  txChars;
    UInt32  rxChars;
} Stats_t;

typedef struct BufferMarks
{
    unsigned long   BufferSize;
    unsigned long   HighWater;
    unsigned long   LowWater;
    bool            OverRun;
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
    
    IOThread        FrameTOEntry;
    
    mach_timespec   DataLatInterval;
    mach_timespec   CharLatInterval;
    
    bool            AreTransmitting;
    
	/* extensions to handle the Driver */
	    
    bool            isDriver;
    void            *DriverPowerRegister;
    UInt32          DriverPowerMask;
	
	
} PortInfo_t;
    
class IrDAComm;
class AppleUSBIrDADriver;

    /* AppleUSBIrDA.h - This file contains definitions for IrDA */
class AppleUSBIrDA : public AppleIrDASerial         // glue for IrDA to call the USB IrDA Driver
{
    OSDeclareDefaultStructors( AppleUSBIrDA )   ;   /* Constructor & Destructor stuff   */
public:
    bool        attach(AppleUSBIrDADriver *provider);   // IOSerialStream attach requires a IOSerialDriverSync
	
    void        Add_RXBytes( UInt8 *Buffer, size_t Size );
    SInt16      SetBofCount( SInt16 bof_count );
    UInt16      SetSpeed( UInt32 brate );
    bool        SetUpTransmit( void );

    IOReturn    StartTransmit( UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer );
    USBIrDAQoS* GetIrDAQoS( void );
    IrDAComm*   GetIrDAComm( void );
    void        GetIrDAStatus( IrDAStatus *status );
    IOReturn    SetIrDAUserClientState( bool IrDAOn );
};

class AppleUSBIrDADriver : public IOSerialDriverSync
{
    OSDeclareDefaultStructors( AppleUSBIrDADriver ) ;   /* Constructor & Destructor stuff   */

private:
    UInt32          fCount;         // usb write length
    UInt8           fSessions;      // Active sessions (count of opens on /dev/tty entries)
    bool            fUserClientStarted; // user/client has started (stopped) us
    bool            fUSBStarted;        // usb family has started (stopped) us
    bool            fTerminate;     // Are we being terminated (ie the device was unplugged)
    UInt8           fProductName[productNameLength];    // Actually the product String from the Device
    PortInfo_t      *fPort;         // The Port
    bool            fReadActive;    // usb read is active
    bool            fWriteActive;   // usb write is active
    UInt8           fPowerState;    // off,on ordinal for power management

    
	// Some of these globals would normally be by port but as we only have one port it doesn't really matter
    
    IrDAComm        *fIrDA;             // IrDA (IrCOMM) object
    AppleUSBIrDA    *fNub;              // glue back to IOSerialStream side
    AppleIrDA       *fUserClientNub;    // nub to publish newUserClient for us
    USBIrDAQoS      fQoS;               // Quality of Service
    bool            fIrDAOn;            // IrDA state (on or off)
    bool            fSuspendFail;       // Suspend not supported or failed
    
    UInt8           fBaudCode;          //  encoded baud code for change speed byte
    UInt8           fBofsCode;          //  extra bof count (encoded but unshifted)
    UInt8           fMediabusy;         //  media busy flag (0 or 1)
    UInt8           fLastChangeByte;    //  saved BOF / baud code byte (send only if changed)
    UInt32          fCurrentBaud;       //  current speed in bps
    
    IOBufferMemoryDescriptor    *fpinterruptPipeMDP;
    IOBufferMemoryDescriptor    *fpPipeInMDP;
    IOBufferMemoryDescriptor    *fpPipeOutMDP;

    UInt8               *fpinterruptPipeBuffer;
    UInt8               *fPipeInBuffer;
    UInt8               *fPipeOutBuffer;
    
    UInt8               fpInterfaceNumber;
    
    IOUSBCompletion     finterruptCompletionInfo;
    IOUSBCompletion     fReadCompletionInfo;
    IOUSBCompletion     fWriteCompletionInfo;
    IOUSBCompletion     fRequestCompletionInfo;
    
    thread_call_t	fPowerThreadCall;
    IOCommandGate	*fGate;
    bool		fWaitForGatedCmd;
    
    static void         interruptReadComplete(  void *obj, void *param, IOReturn ior, UInt32 remaining );
    static void         dataReadComplete(  void *obj, void *param, IOReturn ior, UInt32 remaining );
    static void         dataWriteComplete( void *obj, void *param, IOReturn ior, UInt32 remaining );
    static void         workAroundComplete( void *obj, void *param, IOReturn ior, UInt32 remaining );
    
    bool                initForPM(IOService *provider);
    static void		handleSetPowerState(thread_call_param_t param0, thread_call_param_t param1 );
    static IOReturn	setPowerStateGated(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
    IOReturn		setPowerStateGatedPrivate(uintptr_t powerStateOrdinal);

public:

    IOUSBDevice         *fpDevice;
    IOUSBInterface      *fpInterface;
    IOUSBPipe           *fpInPipe;
    IOUSBPipe           *fpOutPipe;
    IOUSBPipe           *fpInterruptPipe;
    
	/* IOKit methods:   */
	
    virtual bool        init( OSDictionary *dict );
    virtual IOService*  probe( IOService *provider, SInt32 *score );
    virtual bool        start( IOService *provider );
    virtual void        free( void );
    virtual void        stop( IOService *provider );
    virtual IOReturn    message( UInt32 type, IOService *provider,  void *argument = 0 );

	/**** IOSerialDriverSync Abstract Method Implementation ****/

    virtual IOReturn    acquirePort( bool sleep, void *refCon );
    virtual IOReturn    releasePort( void *refCon );
    virtual IOReturn    setState( UInt32 state, UInt32 mask, void *refCon );
    virtual UInt32      getState( void *refCon );
    virtual IOReturn    watchState( UInt32 *state, UInt32 mask, void *refCon );
    virtual UInt32      nextEvent( void *refCon );
    virtual IOReturn    executeEvent( UInt32 event, UInt32 data, void *refCon );
    virtual IOReturn    requestEvent( UInt32 event, UInt32 *data, void *refCon );
    virtual IOReturn    enqueueEvent( UInt32 event, UInt32 data, bool sleep, void *refCon );
    virtual IOReturn    dequeueEvent( UInt32 *event, UInt32 *data, bool sleep, void *refCon );
    virtual IOReturn    enqueueData( UInt8 *buffer, UInt32 size, UInt32 * count, bool sleep, void *refCon );
    virtual IOReturn    dequeueData( UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon );
    
    // Power management routines
    virtual unsigned long initialPowerStateForDomainState ( IOPMPowerFlags );
    virtual IOReturn    setPowerState(unsigned long powerStateOrdinal, IOService * whatDevice);
						
	/**** AppleUSBIrDA Methods ****/
	/**** IrDA Specific (Public) ****/
	
    void            Add_RXBytes( UInt8 *Buffer, size_t Size );
    SInt16          SetBofCount( SInt16 bof_count );
    UInt16          SetSpeed( UInt32 brate );
    bool            SetUpTransmit( void );
    IOReturn        StartTransmit( UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer );
    
    IrDAComm*       GetIrDAComm( void );
    void            GetIrDAStatus( IrDAStatus *status );
    IOReturn        SetIrDAUserClientState( bool IrDAOn );
    USBIrDAQoS*     GetIrDAQoS( void );
    
private:

	/**** Queue primatives ****/
    
    QueueStatus     AddBytetoQueue( CirQueue *Queue, char Value );
    QueueStatus     GetBytetoQueue( CirQueue *Queue, UInt8 *Value );
    QueueStatus     InitQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size );
    QueueStatus     CloseQueue( CirQueue *Queue );
    size_t          AddtoQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size );
    size_t          RemovefromQueue( CirQueue *Queue, UInt8 *Buffer, size_t MaxSize );
    size_t          FreeSpaceinQueue( CirQueue *Queue );
    size_t          UsedSpaceinQueue( CirQueue *Queue );
    size_t          GetQueueSize( CirQueue *Queue );
    //QueueStatus       GetQueueStatus( CirQueue *Queue );
    void            CheckQueues( PortInfo_t *port );

	/**** State manipulations ****/
    
    IOReturn        privateWatchState( PortInfo_t *port, UInt32 *state, UInt32 mask );
    UInt32          readPortState( PortInfo_t *port );
    void            changeState( PortInfo_t *port, UInt32 state, UInt32 mask );
    IOReturn        CheckIrDAState();       // combines fSessions, fStartStopUserClient, fStartStopUSB to new state
    
	/**** USB Specific ****/
    
    bool            configureDevice( UInt8 numConfigs );
    void            Workaround();                               // reset confused silicon
    
    bool            allocateResources( void );                  // allocate pipes
    void            releaseResources( void );                   // free pipes
    bool            startPipes();                               // start the usb reads going
    void            stopPipes();
    bool            createSerialStream();                       // create bsd stream
    void            destroySerialStream();                      // delete bsd stream
    bool            createSuffix( unsigned char *sufKey, int sufMaxLen );
    bool            startIrDA();                                // start irda up
    void            stopIrDA();                                 // shut down irda
    bool            createNub();                                // create nub (and port)
    void            destroyNub();
    void            SetStructureDefaults( PortInfo_t *port, bool Init );
    bool            allocateRingBuffer( CirQueue *Queue, size_t BufferSize );
    void            freeRingBuffer( CirQueue *Queue );
    
}; /* end class AppleUSBIrDADriver */

#endif
