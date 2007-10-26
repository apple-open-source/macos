/*
Copyright (c) 1997-2006 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Apple16X50UARTSync.h
 * This file contains the declarations for a generic 16550 (and later) serial
 * driver.  It is intended to support a variety of chips in this family,
 * connected via a variety of busses.  One instance of this class controls
 * a single UART.  One or more of these objects may be attached to a provider
 * of class com_apple_driver_16X50BusInterface, which provides register access
 * and interrupt dispatch.
 * 
 * 1995-06-23	dreece	Created
 * 2002-02-15	dreece	I/O Kit port, based on NeXT drvISASerialPort DriverKit driver.
 */

#ifndef _APPLE16X50UARTSYNC_H
#define _APPLE16X50UARTSYNC_H

#include "Apple16X50Serial.h"
#include <IOKit/serial/IORS232SerialStreamSync.h> // superclass
#include "Apple16X50BusInterface.h" // Provider class
#include "Apple16X50Queue.h" // RX & TX command queue class
#include "Apple16X50UARTTypes.h" // Hardware specific details
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>

// This are the valid values for the software flow Ccontrol
// state machine (XON/XOFF/XANY)
enum tXO_State {
    kXOnSent = -2,
    kXOffSent = -1,
    kXO_Idle = 0,
    kXOffNeeded = 1,
    kXOnNeeded = 2
} ;

#define Apple16X50BusInterface com_apple_driver_16X50BusInterface
class Apple16X50BusInterface;

#define Apple16X50UARTSync com_apple_driver_16X50UARTSync
class Apple16X50UARTSync : public IORS232SerialStreamSync
{
    OSDeclareDefaultStructors(com_apple_driver_16X50UARTSync);

public:
#ifdef REFBUG
    virtual void retain() const;
    virtual void release() const;
#endif
    // For Clients to call (inherited from IORS232SerialStreamSync)
    virtual IOReturn acquirePort(bool sleep);
    virtual IOReturn releasePort();
    virtual IOReturn setState(UInt32 state, UInt32 mask);
    virtual UInt32 getState();
    virtual IOReturn watchState(UInt32 *state, UInt32 mask);
    virtual UInt32 nextEvent();
    virtual IOReturn executeEvent(UInt32 event, UInt32 data);
    virtual IOReturn requestEvent(UInt32 event, UInt32 *data);
    virtual IOReturn enqueueEvent(UInt32 event, UInt32 data, bool sleep);
    virtual IOReturn dequeueEvent(UInt32 *event, UInt32 *data, bool sleep);
    virtual IOReturn enqueueData(UInt8 *buffer,  UInt32 size, UInt32 *count, bool sleep );
    virtual IOReturn dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);
    virtual bool attachToChild(IORegistryEntry *child, const IORegistryPlane *plane);

    // For Providers to call (Apple16X50BusInterface subclasses) */
    virtual bool start(IOService *provider);
    virtual bool attach(IOService *provider);
    virtual bool terminate(IOOptionBits options);
    virtual bool finalize(IOOptionBits options);
    virtual void stop(IOService *provider);
    virtual void free();
    virtual void interrupt();
    static Apple16X50UARTSync* probeUART(Apple16X50BusInterface *provider,
                                         void *refCon, tUART_Type type=kUART_Unknown);
    virtual UInt32 determineMasterClock();
    virtual void setMasterClock(UInt32 clock);
    
protected:
    static tUART_Type identifyUART(Apple16X50BusInterface *Provider, void *refCon);
    virtual void setDivisor(UInt32 div);
    virtual UInt32 getDivisor();
    virtual void resetUART();
    virtual void programUART();
    virtual void startTxEngine();
    static void dataLatencyTimeoutAction(OSObject *owner, IOTimerEventSource *);
    static void frameTimeoutAction(OSObject *owner, IOTimerEventSource *);
    static void delayTimeoutAction(OSObject *owner, IOTimerEventSource *);
    static void heartbeatTimeoutAction(OSObject *owner, IOTimerEventSource *);
    virtual IOReturn executeEventGated(UInt32 event, UInt32 data, UInt32 *state, UInt32 *delta);
    virtual UInt32 generateRxQState();
    virtual void flowMachine();
    virtual IOReturn activatePort();
    virtual void deactivatePort();
    virtual void programMCR(UInt32 state, bool irqen=true, bool loop=false);
    inline UInt8 inb(tUARTregisters reg) { return Provider->getReg(reg, fRefCon); };
    inline void outb(tUARTregisters reg, UInt8 val) { Provider->setReg(reg, val, fRefCon); };
    inline UInt32 msrState();
#ifdef DEBUG
    virtual void showRegs();
#endif
	virtual void restoreUART();
	virtual void saveUART();

    // Static stubs for IOCommandGate::runAction()
    static IOReturn acquirePortAction(OSObject *owner, void*arg0, void*, void*, void*);
    static IOReturn releasePortAction(OSObject *owner, void*, void*, void*, void*);
    static IOReturn setStateAction(OSObject *owner, void*arg0, void*arg1, void*, void*);
    static IOReturn watchStateAction(OSObject *owner, void*arg0, void*arg1, void*, void*);
    static IOReturn nextEventAction(OSObject *owner, void*, void*, void*, void*);
    static IOReturn executeEventAction(OSObject *owner, void*arg0, void*arg1, void*, void*);
    static IOReturn enqueueEventAction(OSObject *owner, void*arg0, void*arg1, void*arg2, void*);
    static IOReturn dequeueEventAction(OSObject *owner, void*arg0, void*arg1, void*arg2, void*);
    static IOReturn enqueueDataAction(OSObject *owner, void*arg0, void*arg1, void*arg2, void*arg3);
    static IOReturn dequeueDataAction(OSObject *owner, void*arg0, void*arg1, void*arg2, void*arg3);
    static IOReturn timerControlAction(OSObject *owner, void*arg0, void*, void*, void*);
    static IOReturn stopAction(OSObject *owner, void*arg0, void*, void*, void*);

    // Gated methods called by the Static stubs above
    virtual IOReturn acquirePortGated(bool sleep);
    virtual IOReturn releasePortGated();
    virtual void setStateGated(UInt32 state, UInt32 mask);
    virtual IOReturn watchStateGated(UInt32 *state, UInt32 mask);
    virtual UInt32 nextEventGated();
    virtual IOReturn executeEventGated(UInt32 event, UInt32 data);
    virtual IOReturn enqueueEventGated(UInt32 event, UInt32 data, bool sleep);
    virtual IOReturn dequeueEventGated(UInt32 *event, UInt32 *data, bool sleep);
    virtual IOReturn enqueueDataGated(UInt8 *buffer,  UInt32 size, UInt32 *count, bool sleep);
    virtual IOReturn dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep);
    virtual void stopGated(IOService *provider);

    // ***Instance Variables***
    // Primary resources:
    Apple16X50BusInterface	*Provider;
    Apple16X50Queue		*RxQ, *TxQ;
    IOWorkLoop			*WorkLoop;
    IOCommandGate		*CommandGate;
    const char			*Name;
    UInt32			State;
    bool			OffLine; // true when card cannot be touched
    bool			Acquired; // true while acquired
    UInt32			WatchStateMask;
    enum { kFreed=0, kAllocated, kAttached, kProbed, kStarted, kAcquired, kReleased, kTerminated, kFinalized, kStopped, kDetached } Stage;

    // Timers:
    IOTimerEventSource		*FrameTimer;
    UInt32			FrameInterval;		// Frame time in microseconds, used to wait for TxIdle
    bool			FrameTimerRunning;
    IOTimerEventSource		*DataLatTimer;
    UInt32			DataLatInterval;	// Maximum time a char can sit undelivered in the queue
    IOTimerEventSource		*DelayTimer;
    UInt32			DelayInterval;
    bool			DelayTimerRunning;
#ifdef HEARTBEAT
    IOTimerEventSource		*HeartBeatTimer;
    UInt32			HeartBeatInterval;	// Periodically fake an interrupt to prevent data loss
    bool			HeartBeatNeeded;
#endif

    // UART characteristics and register shadows:
    tUART_Type			UART_Type;
    UInt32			MaxBaud;
    UInt32			FIFO_Size;
    UInt8			LCR_Image;
    UInt8			FCR_Image;
    UInt8			IER_Mask;
    UInt8			MCR_Image;

    // Framing and data rate
    UInt32			SW_Special[8];
    UInt32			DataWidth;
    UInt8			DataMask;
    UInt32			StopBits;
    UInt32			Parity;
    UInt32			BaudRate;
    UInt32			MasterClock;
    UInt16			Divisor;
    bool			MinLatency;
    bool			IgnoreParityErrors;

    // Flow Control
    UInt32			FlowControl;	// notify-on-delta & auto_control
    tXO_State			RXO_State;
    UInt8			XOnChar;
    UInt8			XOffChar;
	
    // power management
	bool		portOpened;

	
    bool				initForPM(IOService *policyMaker);
    IOReturn			setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice);
    unsigned long		powerStateForDomainState(IOPMPowerFlags domainState );
    unsigned long		maxCapabilityForDomainState(IOPMPowerFlags domainState);
    unsigned long		initialPowerStateForDomainState(IOPMPowerFlags domainState);

    static void			handleSetPowerState(thread_call_param_t param0, thread_call_param_t param1);
    static IOReturn		setPowerStateGated(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);

    // private power state stuff
    thread_call_t		fPowerThreadCall;
    bool				fWaitForGatedCmd;
    unsigned int		fCurrentPowerState;     // current power state (0 or 1)
};

#endif /* !_APPLE16X50UARTSYNC_H */
