/* IrDAComm.h - This file contains glue to IrDA and the IrComm client */
    
#ifndef _IRDACOMM_
#define _IRDACOMM_

#include <kern/thread_call.h>

#include "AppleIrDA.h"
#include "IrDAUserClient.h"         // for status 

class TIrGlue;
class IrComm;
class IOCommandGate;
class CTimer;
class IrDATimerEventSource;

enum {                          // private irdacomm states
    kIrDACommStateStart,                    // starting up and doing initial connect attempts
    kIrDACommStateIdle,                     // not doing much
    kIrDACommStateConnecting,               // trying to connect
    kIrDACommStateListening,                // waiting for peer to connect to us
    kIrDACommStateStoppingListen,           // waiting for listen abort to finish
    kIrDACommStateDisconnecting,            // waiting for a disconnect request to finish
    kIrDACommStateConnected,                // ircomm channel open, data can flow
    kIrDACommStateStopping2,                // stopping, need two callbacks
    kIrDACommStateStopping,                 // stopping, need one callback
    kIrDACommStateStopped                   // all stopped
};

enum {                          // private irdacomm events
    kIrDACommEventTimer,            // main irdacomm timer fired
    kIrDACommEventConnected,        // ircomm callback with state of connected
    kIrDACommEventDisconnected,     // ircomm callback with state of disconnected
    kIrDACommEventStop              // Want to disconnect and stop
};

class IrDAComm : public OSObject
{
    OSDeclareDefaultStructors( IrDAComm )   ;   /* Constructor & Destructor stuff   */

public:
    /**** IrDAComm Methods ****/
	
    static IrDAComm * irDAComm(AppleIrDASerial *driver, AppleIrDA *appleirda);  // IOKit style creation/init pair
    bool            init(AppleIrDASerial *driver, AppleIrDA *appleirda);        // Set up to start
    //IOReturn      Start();                                // Start trying to connect
    void            free(); 

    //*****
    // The following are safe to call from outside our workloop -- they get the 
    // command gate before getting to IrDA.
    //*****
    
    IOReturn        Stop();                                 // Disconnect and stop

    // Pseudo tty wants to send data to peer over IrCOMM
    size_t          TXBufferAvailable();                    // Return max tx size available to write to ircomm
    size_t          Write( UInt8 *Buf, size_t Length );     // Send data over the ircomm channel
    
    // Hardware driver has a packet to send to IrLAP
    IOReturn        ReadComplete( UInt8 *Buf, size_t Length );      // Call with the incoming data
    
    // Sending back flow-control to the peer (pseudo tty consumed the data)
    void            ReturnCredit(size_t byte_count);        // serial client has consumed count bytes of data

    // Hardware driver calls this when the transmit to the pod has finished
    void            Transmit_Complete(Boolean worked);
    
    // Hardware driver calls this when SetSpeed completes
    void            SetSpeedComplete(Boolean worked);
    
    //*****
    // The following routines are for IrComm layer back to us and
    // assume we're already running in our workloop
    //*****
    
    void            ConnectionStatus(Boolean connected);            // ircomm calls this to tell us of connection states
    void            IrCommDataRead(UInt8 *buf, UInt32 length);      // ircomm data to pass back to the tty
    void            BackEnable();                                   // our tinytp peer can handle some more data sent to it now
    
    static void     TimerRoutine(OSObject *owner, IrDATimerEventSource *iotimer);   // our state engine CTimer callback
    
    // For user-client status interface, maybe should get the gate, but ok if race condition - it's just status
    void            GetIrDAStatus(IrDAStatus *status);      // connnected status and statistics for user-client

    // bsd pseudo tty open calls this while stalling for initial connect attempt
    bool            Starting();                 // returns true for initial few connection attempts
    
private:
    UInt8                       fState;         // our state
    AppleIrDASerial             *fDriver;       // back to scc/usb driver
    //IOTimerEventSource            *fTimerSrc;     // our listen/discover timer
    CTimer                      *fTimer;        // our state-engine timer
    USBIrDAQoS                  *fQoS;          // driver supplied qos
    TIrGlue                     *fIrDA;         // the stack, we make and free
    IrComm                      *fIrComm;       // ircomm protocol layer
    Boolean                     fWriteBusy;     // last txbufferavailable returned zero
    IOCommandGate               *fGate;         // my command gate to serialize to irda
    UInt8                       fStartCounter;  // counter for initial connection attempts
    thread_call_t               fStop_thread;   // non-gated stop

    void    StateChange(int event);             // handle a change in status or timer firing
    static IOReturn     DoSomething(OSObject *owner, void *a, void *b, void *c, void *d);   // run command in command gate
    static void stop_thread(thread_call_param_t param0, thread_call_param_t param1);

}; /* end class IrDAComm */

#endif
