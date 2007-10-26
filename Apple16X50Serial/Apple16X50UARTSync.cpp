/*
Copyright (c) 1997-2006 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Apple16X50UARTSync.cpp
 * This file contains the implementation of a generic 16550 (and later) serial
 * driver.  It is intended to support a variety of chips in this family,
 * connected via a variety of busses.  One instance of this class controls
 * a single UART.  One or more of these objects may be attached to a provider
 * of class com_apple_driver_16X50BusInterface, which provides register access
 * and interrupt dispatch.
 * 
 * 1995-06-23	dreece	Created
 * 2002-02-15	dreece	I/O Kit port, based on NeXT drvISASerialPort DriverKit driver.
 */

#include "Apple16X50UARTSync.h"
#include <IOKit/IOLib.h>
#include <IOKit/serial/IOSerialKeys.h>

#define	bits		<<1
#define kMinBaud	((UInt32)(50 bits))

#define	kOneMS	(1000)		// 1 ms expressed in microseconds
#define	kTwoMS	(2*kOneMS)	// 2 ms expressed in microseconds
#define	kTenMS	(10*kOneMS)	// 10 ms expressed in microseconds

#define kXOnChar  '\x11'
#define kXOffChar '\x13'

#define k1xMasterClock (1843200)

#define kRxAutoFlow	((UInt32)( PD_RS232_A_RFR | PD_RS232_A_DTR | PD_RS232_A_RXO ))
#define kTxAutoFlow	((UInt32)( PD_RS232_A_CTS | PD_RS232_A_DSR | PD_RS232_A_TXO | PD_RS232_A_DCD ))
#define kMSR_StateMask	((UInt32)( PD_RS232_S_CTS | PD_RS232_S_DSR | PD_RS232_S_CAR | PD_RS232_S_RI  ))

#define super IORS232SerialStreamSync
OSDefineMetaClassAndStructors(com_apple_driver_16X50UARTSync, IORS232SerialStreamSync);

inline UInt32 Apple16X50UARTSync::msrState()
{
    register UInt32 stat=0;
    register UInt8 msr=(OffLine)?0:inb(kREG_ModemStatus);
    if (msr & kMSR_CTS) stat |= PD_RS232_S_CTS;
    if (msr & kMSR_DSR) stat |= PD_RS232_S_DSR;
    if (msr & kMSR_RI)  stat |= PD_RS232_S_RI;
    if (msr & kMSR_DCD) stat |= PD_RS232_S_CAR;
    return stat;
}

/* acquirePort tests and sets the state of the port object.  If the port is
 * available, then the state is set to acquired, and kIOReturnSuccess is returned.
 * If the port was already busy and sleep is true, then the calling thread will block
 * until the port is freed, then re-attempts the acquire.  If the port was
 * already busy and sleep is false, then kIOReturnExclusiveAccess is returned.
 */
IOReturn Apple16X50UARTSync::acquirePort(bool sleep)
{
    IOReturn ret;
    retain();
    ret=CommandGate->runAction(acquirePortAction, (void*)sleep);
    release();
    return ret;
}

IOReturn Apple16X50UARTSync::
acquirePortAction(OSObject *owner, void*arg0, void*, void*, void*)
{ return ((Apple16X50UARTSync *)owner)->acquirePortGated((bool)arg0); }

IOReturn Apple16X50UARTSync::acquirePortGated(bool sleep)
{
    IOReturn rtn = kIOReturnExclusiveAccess;
    
    DEBUG_IOLog("%s::acquirePortGated(%s)\n", Name, BOOLSTR(sleep));
    assert((Stage==kReleased) || (Stage==kStarted));
    if (OffLine || (Stage<kStarted) || (Stage>=kFinalized)) return kIOReturnOffline;
    
    retain(); // hold reference till releasePort(), unless we fail to acquire
    while (Acquired) {
        if (sleep) {
            retain();
            CommandGate->retain();
            rtn = CommandGate->commandSleep((void*)&Acquired);
            CommandGate->release();
            release();
            if (OffLine || (Stage<kStarted)||(Stage>=kTerminated))
                rtn = kIOReturnOffline;
        }
        if (rtn != THREAD_AWAKENED) goto fail;
    }
    if (!(Provider->open(this))) {
        rtn = kIOReturnExclusiveAccess;
        goto fail;
    }
    
    Stage = kAcquired;
    Acquired = true;

    // default everything
    setStateGated(PD_S_ACQUIRED | PD_S_TX_ENABLE | PD_S_RX_ENABLE | PD_RS232_A_TXO | PD_RS232_A_RXO , PD_RS232_S_MASK | PD_S_MASK);
    if (!OffLine) {
        resetUART();
        outb(kREG_IRQ_Enable, IER_Mask & kIRQEN_ModemStatus);
        flowMachine();
        setStateGated(msrState(), kMSR_StateMask);
#ifdef HEARTBEAT
        if (HeartBeatInterval > 0) {
            HeartBeatNeeded = true;
            HeartBeatTimer->setTimeoutUS(HeartBeatInterval);
        }
#endif
        startTxEngine();
    }

	portOpened = true;
    return kIOReturnSuccess;

fail:
    CommandGate->commandWakeup((void*)&Acquired, !OffLine);
    release();
    return rtn;
}

/* release sets the state of the port object to available and wakes up and
 * threads sleeping for access to this port.  It will return kIOReturnSuccess
 * if the port was in a busy state, and kIOReturnNotOpen if it was available.
 */
IOReturn Apple16X50UARTSync::releasePort()
{
    switch (Stage) {
        case kReleased :	return kIOReturnSuccess;
        case kTerminated :	assert(CommandGate);
        case kAcquired :	IOReturn ret;
                                retain();
                                ret=CommandGate->runAction(releasePortAction);
                                release();
                                return ret;
        default :		break;
    }
    return kIOReturnOffline;
}

IOReturn Apple16X50UARTSync::
releasePortAction(OSObject *owner, void*, void*, void*, void*)
{ return ((Apple16X50UARTSync *)owner)->releasePortGated(); }

IOReturn Apple16X50UARTSync::releasePortGated()
{
    DEBUG_IOLog("%s::releasePortGated()\n", Name);
    if (!Acquired) return kIOReturnSuccess;
    
    deactivatePort();
    resetUART();
    setStateGated(0, PD_RS232_S_MASK | PD_S_MASK);	// Clear the entire state word
    if (!OffLine) outb(kREG_ModemControl, 0x00);
    CommandGate->commandWakeup((void*)&Acquired, !OffLine);
    Provider->close(this);
    if (Stage<kReleased) Stage = kReleased;
    Acquired=false;
    release(); // dispose of the self-reference we took in acquirePort()
    return kIOReturnSuccess;
}

// setState() accepts a new state value as input, and takes all steps necessary
// to make the current state equal to this new state.  This includes waking up any
// threads asleep on WatchStateMask, as well as dealing with Flow Control notification.
IOReturn Apple16X50UARTSync::setState(UInt32 state, UInt32 mask)
{
//  DEBUG_IOLog("%s::setState(0x%08x,0x%08x)\n", Name, (int)state, (int)mask);
    if (Stage != kAcquired) return kIOReturnOffline;
    assert(CommandGate);

    if (mask & (PD_S_ACQUIRED | PD_S_ACTIVE)) // may not acquire or activate via setState
        return kIOReturnBadArgument;
    // ignore any bits that are read-only
    mask &= ~(FlowControl & PD_RS232_A_MASK);
    if (mask) {
        retain();
        CommandGate->runAction(setStateAction, (void*)state, (void*)mask);
        release();
    }
    return kIOReturnSuccess;
}

IOReturn Apple16X50UARTSync::
setStateAction(OSObject *owner, void*arg0, void*arg1, void*, void*)
{
    ((Apple16X50UARTSync *)owner)->setStateGated((UInt32)arg0, (UInt32)arg1);
    return kIOReturnSuccess;
}

void Apple16X50UARTSync::setStateGated(UInt32 state, UInt32 mask)
{
    UInt32	delta;
    //    DEBUG_IOLog("%s::setStateGated(0x%08x,0x%08x)\n", Name, (int)state, (int)mask);
    state = maskMux(State, state, mask);	// compute the new state
    delta = (state ^ State);			// keep a copy of the diffs
    State = state;
    // Wake up all threads asleep on WatchStateMask
    if (delta & WatchStateMask) {
        CommandGate->commandWakeup((void*)&State);
    }
    // if any modem control signals changed, we need to do an outb()
    if (delta & ( PD_RS232_S_DTR | PD_RS232_S_RFR ))
        programMCR(state);
    // if an Xon or Xoff is needed, make sure the TX engine is running
    startTxEngine();
    // do we need to enqueue a flow control notification event?
    delta <<= PD_RS232_N_SHIFT;
    if (delta & FlowControl)
        RxQ->enqueueEvent(PD_E_FLOW_CONTROL, delta | (state & PD_RS232_S_MASK));
}

// Get the state for the port device.
UInt32 Apple16X50UARTSync::getState()
{
//  DEBUG_IOLog("%s::getState()=0x%08x\n", Name, (int)State);
    return State;
}

/* watchState()
 * Wait for the at least one of the state bits defined in mask to be equal
 * to the value defined in state. Check upon entry, then sleep until necessary.
 * A return value of kIOReturnSuccess means that at least one of the port state
 * bits specified by mask is equal to the value passed in by state.  A return
 * value of kIOReturnIOError indicates that the port went inactive.  A return
 * value of kIOReturnAborted indicates sleep was interrupted by a signal.
 */
IOReturn Apple16X50UARTSync::watchState(UInt32 *state, UInt32 mask)
{
    IOReturn ret;
    //DEBUG_IOLog("%s::watchState(*0x%08x,0x%08x)\n", Name, (int)(*state), (int)mask);
    
    if (!state) return kIOReturnBadArgument;
    if (!mask) return kIOReturnSuccess;
    if ((Stage<kStarted)||(Stage>=kTerminated)) return kIOReturnOffline;
    assert(CommandGate);

    retain();
    ret=CommandGate->runAction(watchStateAction, (void*)state, (void*)mask);
    release();
    return ret;
}

IOReturn Apple16X50UARTSync::
watchStateAction(OSObject *owner, void*arg0, void*arg1, void*, void*)
{ return ((Apple16X50UARTSync *)owner)->watchStateGated((UInt32*)arg0, (UInt32)arg1); }

IOReturn Apple16X50UARTSync::watchStateGated(UInt32 *state, UInt32 mask)
{
    bool watchForInactive = false;
    IOReturn rtn;
    DEBUG_IOLog("%s::watchStateGated(*0x%08x,0x%08x)\n", Name, (int)(*state), (int)mask);
    if ((Stage<kStarted)||(Stage>=kTerminated)) return kIOReturnOffline;
    if ( !(mask & (PD_S_ACQUIRED | PD_S_ACTIVE)) ) {
        (*state) &= ~PD_S_ACTIVE;	// Check for low PD_S_ACTIVE
        mask     |=  PD_S_ACTIVE;	// Register interest in PD_S_ACTIVE bit
        watchForInactive = true;
    }
    while (true) {
        // compare *state with State, too see if any interesting bits match
        if (mask & ~((*state) ^ State)) {
            *state = State;
            if (watchForInactive && !(State & PD_S_ACTIVE))
                rtn = kIOReturnOffline;
            else
                rtn = kIOReturnSuccess;
            break;
        }
        // Everytime we go around the loop we have to reset the watch mask.
        // This means any event that could affect the WatchStateMask must
        // wakeup all watch state threads.  The two event's are an interrupt
        // or one of the bits in the WatchStateMask changing.
        WatchStateMask |= mask;
        retain();	// refuse to be freed until all threads are awake
        CommandGate->retain();
        rtn = CommandGate->commandSleep((void*)&State);
        CommandGate->release();
        if (OffLine || (Stage<kStarted)||(Stage>=kTerminated)) {
            DEBUG_IOLog("%s::watchStateGated() awakend to find port going away!\n", Name);
            release();
            return kIOReturnOffline;
        }
        //DEBUG_IOLog("%s::watchStateGated() awakend=%p with State=%p\n", Name, (void*)rtn, (void*)State);
        if (rtn == THREAD_TIMED_OUT) {
            rtn=kIOReturnTimeout;
            break;
        } else if (rtn == THREAD_INTERRUPTED) {
            rtn=kIOReturnAborted;
            break;
        }
        release();
    }
    // As it is impossible to undo the masking used by this thread,
    // we clear down the watch state mask and wakeup every
    // sleeping thread to reinitialize the mask before exiting.
    WatchStateMask = 0;
    CommandGate->commandWakeup((void*)&State);
    DEBUG_IOLog("%s::watchStateGated() returning=%p\n", Name, (void*)rtn);
    return rtn;
}

// nextEvent returns the type of the next event on the RX queue.
// If no events are present on the RX queue, then PD_E_EOQ is returned.
UInt32 Apple16X50UARTSync::nextEvent()
{
    UInt32 ret=PD_E_EOQ;
    if (Stage==kAcquired) {
        retain();
        ret=(UInt32)(CommandGate->runAction(nextEventAction));
        release();
    }
    return ret;
}

IOReturn Apple16X50UARTSync::
nextEventAction(OSObject *owner, void*, void*, void*, void*)
{ return (IOReturn)(((Apple16X50UARTSync *)owner)->nextEventGated()); }

UInt32 Apple16X50UARTSync::nextEventGated()
{
    UInt32 event=PD_E_EOQ;
    if ((Stage==kAcquired) && RxQ)
        event = RxQ->peekEvent();
//  DEBUG_IOLog("%s::nextEventGated()=0x%02x\n", Name, (int)event);
    return event;
}

// executeEvent causes the specified event to be processed immediately.
IOReturn Apple16X50UARTSync::executeEvent(UInt32 event, UInt32 data)
{
    IOReturn ret=kIOReturnOffline;
    if (Stage==kAcquired) {
        retain();
        ret=CommandGate->runAction(executeEventAction, (void*)event, (void*)data);
        release();
    }
    return ret;
}

IOReturn Apple16X50UARTSync::
executeEventAction(OSObject *owner, void*arg0, void*arg1, void*, void*)
{ return ((Apple16X50UARTSync *)owner)->executeEventGated((UInt32)arg0, (UInt32)arg1); }

IOReturn Apple16X50UARTSync::executeEventGated(UInt32 event, UInt32 data)
{
    IOReturn ret = kIOReturnSuccess;
    UInt32 tmp, state, delta;
    DEBUG_IOLog("%s::executeEventGated(0x%02x,0x%02x)\n", Name, (int)event, (int)data);
    if (Stage!=kAcquired) return kIOReturnOffline;
    switch (event) {
        case PD_E_FLOW_CONTROL:
            tmp = kRxAutoFlow & (data ^ FlowControl);
            FlowControl = data & ( kRxAutoFlow | kTxAutoFlow | PD_RS232_N_MASK );
            if (tmp) {
                if (tmp & PD_RS232_A_RXO)
                    RXO_State = kXO_Idle;
                flowMachine();
            }
                break;
        case PD_E_DELAY:
            DelayInterval = data;
            break;
        case PD_E_RXQ_SIZE:
            if (State & PD_S_ACTIVE)
                ret = kIOReturnBusy;
            else
                RxQ->setSize(data);
            break;
        case PD_E_TXQ_SIZE:
            if (State & PD_S_ACTIVE)
                ret = kIOReturnBusy;
            else
                TxQ->setSize(data);
            break;
        default:
            delta = 0;
            state = State;
            ret = executeEventGated(event, data, &state, &delta);
            setStateGated(state, delta);
            break;
    }
    return ret;
}

// executeEvent causes the specified event to be processed immediately.
IOReturn Apple16X50UARTSync::
executeEventGated(UInt32 event, UInt32 data, UInt32 *state, UInt32 *delta)
{
    IOReturn ret = kIOReturnSuccess;
    UInt32 tmp;
    DEBUG_IOLog("%s::executeEventGated(0x%02x,0x%02x,*0x%08x,*0x%08x)\n", Name, (int)event, (int)data, (int)*state, (int)*delta);
    if (Stage!=kAcquired) return kIOReturnOffline;
    switch (event) {
        case PD_RS232_E_XON_BYTE:
            if (data>0xff)
                ret = kIOReturnBadArgument;
            else
                XOnChar = data;
            break;
        case PD_RS232_E_XOFF_BYTE:
            if (data>0xff)
                ret = kIOReturnBadArgument;
            else
                XOffChar = data;
            break;
        case PD_E_SPECIAL_BYTE:
            if (data&(~0xff))
                ret = kIOReturnBadArgument;
            else
                SW_Special[data>>5] |= (1<<(data&0x1f));
            break;
        case PD_E_VALID_DATA_BYTE:
            if (data&(~0xff))
                ret = kIOReturnBadArgument;
            else
                SW_Special[data>>5] &= ~(1<<(data&0x1f));
            break;
            // Enqueued flow control event allows the user to change the state of any flow control
            // signals set on Manual (its Auto bit in exec/req event is cleared)
        case PD_E_FLOW_CONTROL :
            tmp = (data>>PD_RS232_D_SHIFT) & kRxAutoFlow & (~FlowControl);
            *state = maskMux(*state, data, tmp);
            *delta |= tmp;
            if (tmp & PD_RS232_S_RXO)
                RXO_State = (data & PD_RS232_S_RXO) ? kXOnNeeded : kXOffNeeded;
            programMCR(*state);
            break;
        case PD_E_ACTIVE:
            if (BOOLVAL(data))
                ret = activatePort();
            else
                deactivatePort();
            break;
        case PD_E_DATA_LATENCY:
            DataLatInterval = data;
            break;
        case PD_RS232_E_MIN_LATENCY:
            MinLatency = BOOLVAL(data);
            Divisor = 0x0000;  // force recompute of FIFO levels
            programUART();
            break;
        case PD_E_DATA_INTEGRITY:
            if ((data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE))
                ret = kIOReturnBadArgument;
            else {
                Parity = data;
                IgnoreParityErrors = false;
                programUART();
            }
            break;
        case PD_E_DATA_RATE:
            if ((data < kMinBaud) || (data > MaxBaud))
                ret = kIOReturnBadArgument;
            else {
                BaudRate = data;
                programUART();
            }
            break;
        case PD_E_DATA_SIZE:
            if ((data < (5 bits)) || (data > (8 bits)) || (data&1))
                ret = kIOReturnBadArgument;
            else {
                DataWidth = data;
                programUART();
            }
            break;
        case PD_RS232_E_STOP_BITS:
            if ((data < (1 bits)) || (data > (2 bits)))
                ret = kIOReturnBadArgument;
            else {
                StopBits = data;
                programUART();
            }
            break;
        case PD_E_RXQ_FLUSH:
            if (RxQ) RxQ->flush();
            if (!OffLine) outb(kREG_FIFOControl, (FCR_Image | kFIFO_ResetRx));
            *state = maskMux(*state, generateRxQState(), (PD_S_RXQ_MASK | kRxAutoFlow));
            *delta |= PD_S_RXQ_MASK | kRxAutoFlow;
            break;
        case PD_E_RX_DATA_INTEGRITY:
            if (data==Parity) break;
            if (data==PD_RS232_PARITY_DEFAULT) break;
            if ((data==PD_RS232_PARITY_ANY) && (Parity!=PD_RS232_PARITY_NONE)) break;
            ret = kIOReturnUnsupported;
            break;
        case PD_E_RX_DATA_RATE:
            if ((data != 0) && (data != BaudRate))
                ret = kIOReturnUnsupported;
            break;
        case PD_E_RX_DATA_SIZE:
            if ((data != 0) && (data != DataWidth))
                ret = kIOReturnUnsupported;
            break;
        case PD_RS232_E_RX_STOP_BITS:
            if ((data != 0) && (data != StopBits))
                ret = kIOReturnUnsupported;
            break;
        case PD_E_TXQ_FLUSH:
            if (TxQ) TxQ->flush();
            if (!OffLine) outb(kREG_FIFOControl, (FCR_Image | kFIFO_ResetTx));
            *state = maskMux(*state, TxQ->getState(), PD_S_TXQ_MASK);
            *delta |= PD_S_TXQ_MASK;
            break;
        case PD_RS232_E_LINE_BREAK :
            if (!OffLine) {
                LCR_Image = boolBit(LCR_Image, BOOLVAL(data), kLCR_SendBreak);
                outb(kREG_LineControl, LCR_Image);
                *state = boolBit(*state, BOOLVAL(data), PD_RS232_S_BRK);
                *delta |= PD_RS232_S_BRK;
            }
            break;
        case PD_E_DELAY:
            if (data > 0) {
                DelayTimerRunning=true;
                DelayTimer->setTimeoutUS(data);
            }
            break;
        case PD_E_RXQ_HIGH_WATER:
            RxQ->setHighWater(data);
            *state = maskMux(*state, generateRxQState(), (PD_S_RXQ_MASK | kRxAutoFlow));
            *delta |= PD_S_RXQ_MASK | kRxAutoFlow;
            break;
        case PD_E_RXQ_LOW_WATER:
            RxQ->setLowWater(data);
            *state = maskMux(*state, generateRxQState(), (PD_S_RXQ_MASK | kRxAutoFlow));
            *delta |= PD_S_RXQ_MASK | kRxAutoFlow;
            break;
        case PD_E_TXQ_HIGH_WATER:
            TxQ->setHighWater(data);
            *state = maskMux(*state, TxQ->getState(), PD_S_TXQ_MASK);
            *delta |= PD_S_TXQ_MASK;
            break;
        case PD_E_TXQ_LOW_WATER:
            TxQ->setLowWater(data);
            *state = maskMux(*state, TxQ->getState(), PD_S_TXQ_MASK);
            *delta |= PD_S_TXQ_MASK;
            break;
        default:
            DEBUG_IOLog("%s::executeEventGated() unknown event=0x%02x, data=%p\n", Name, (int)event, (void*)data);
            ret = kIOReturnBadArgument;
            break;
    }
    return ret;
}

/* requestEvent processes the specified event as an immediate request and
 * returns the results in data.  This is primarily used for getting link
 * status information and verifying baud rate and such.
 */
IOReturn Apple16X50UARTSync::requestEvent(UInt32 event, UInt32 *data)
{
    DEBUG_IOLog("%s::requestEvent(0x%02x,*0x%02x)\n", Name, (int)event, (int)*data);
    if (Stage!=kAcquired) return kIOReturnOffline;
    if (data == NULL) return kIOReturnBadArgument;
    switch (event) {
        case PD_E_ACTIVE :             *data = BOOLVAL(State&PD_S_ACTIVE);	break;
        case PD_E_FLOW_CONTROL:        *data = FlowControl;			break;
        case PD_E_DELAY :              *data = DelayInterval;			break;
        case PD_E_DATA_LATENCY :       *data = DataLatInterval;			break;
        case PD_E_TXQ_SIZE :           *data = TxQ->Size;			break;
        case PD_E_RXQ_SIZE :           *data = RxQ->Size;			break;
        case PD_E_TXQ_LOW_WATER :      *data = TxQ->LowWater;			break;
        case PD_E_RXQ_LOW_WATER :      *data = RxQ->LowWater;			break;
        case PD_E_TXQ_HIGH_WATER :     *data = TxQ->HighWater;			break;
        case PD_E_RXQ_HIGH_WATER :     *data = RxQ->HighWater;			break;
        case PD_E_TXQ_AVAILABLE :      *data = TxQ->Size - TxQ->Count;		break;
        case PD_E_RXQ_AVAILABLE :      *data = RxQ->Count;			break;
        case PD_E_DATA_RATE :          *data = BaudRate;			break;
        case PD_E_RX_DATA_RATE :       *data = 0x00;                            break;
        case PD_E_DATA_SIZE :          *data = DataWidth;			break;
        case PD_E_RX_DATA_SIZE :       *data = 0x00;                            break;
        case PD_E_DATA_INTEGRITY :     *data = Parity;				break;
        case PD_E_RX_DATA_INTEGRITY :  *data = IgnoreParityErrors;		break;
        case PD_RS232_E_STOP_BITS :    *data = StopBits;			break;
        case PD_RS232_E_RX_STOP_BITS : *data = 0x00;                            break;
        case PD_RS232_E_XON_BYTE :     *data = XOnChar;				break;
        case PD_RS232_E_XOFF_BYTE :    *data = XOffChar;			break;
        case PD_RS232_E_LINE_BREAK :   *data = BOOLVAL(State&PD_RS232_S_BRK);	break;
        case PD_RS232_E_MIN_LATENCY:   *data = BOOLVAL(MinLatency);		break;
            
        default :	DEBUG_IOLog("%s::requestEvent() unknown event=0x%02x\n", Name, (int)event);
                        return kIOReturnBadArgument;
    }
    return kIOReturnSuccess;
}

/* enqueueEvent will place the specified event into the TX queue.  The
 * sleep argument allows the caller to specify the enqueueEvent's
 * behaviour when the TX queue is full.  If sleep is true, then this
 * method will sleep until the event is enqueued.  If sleep is false,
 * then enqueueEvent will immediatly return kIOReturnNoResources.
 */
IOReturn Apple16X50UARTSync::enqueueEvent(UInt32 event, UInt32 data, bool sleep)
{
    IOReturn ret=kIOReturnOffline;
    retain();
    if ((Stage==kAcquired)&&(State&PD_S_ACTIVE)) {
        assert(CommandGate);
        ret=CommandGate->runAction(enqueueEventAction, (void*)event, (void*)data, (void*)sleep);
    }
    release();
    return ret;
}

IOReturn Apple16X50UARTSync::
enqueueEventAction(OSObject *owner, void*arg0, void*arg1, void*arg2, void*)
{ return ((Apple16X50UARTSync *)owner)->enqueueEventGated((UInt32)arg0, (UInt32)arg1, (bool)arg2); }

IOReturn Apple16X50UARTSync::enqueueEventGated(UInt32 event, UInt32 data, bool sleep)
{
    DEBUG_IOLog("%s::enqueueEventGated(0x%02x,0x%02x,%s)\n", Name, (int)event, (int)data, BOOLSTR(sleep));
    if (Stage!=kAcquired) return kIOReturnOffline;
    if (!(State&PD_S_ACTIVE)) return kIOReturnOffline;
    
    while (!(TxQ->enqueueEventTry(event, data))) {
        UInt32 state=0;
        IOReturn rtn;
        if (!sleep) return kIOReturnNoResources;
        rtn = watchStateGated(&state, (UInt32)PD_S_TXQ_FULL);
        if (rtn != kIOReturnSuccess) return rtn;
    }
    if (TxQ->enqueueThresholdExceeded())
        setStateGated(TxQ->getState(), PD_S_TXQ_MASK);
    startTxEngine();
    return kIOReturnSuccess;
}

/* dequeueEvent will remove the oldest event from the RX queue and return
 * it in event & data.  The sleep argument defines the behavior if the RX
 * queue is empty.  If sleep is true, then this method will sleep until an
 * event is available.  If sleep is false, then an PD_E_EOQ event will be
 * returned.  In either case kIOReturnSuccess is returned.
 */
IOReturn Apple16X50UARTSync::dequeueEvent(UInt32 *event, UInt32 *data, bool sleep)
{
    IOReturn ret=kIOReturnOffline;
    if ((!event) || (!data)) return kIOReturnBadArgument;
    retain();
    if ((Stage==kAcquired) && (State&PD_S_ACTIVE)) {
        assert(CommandGate);
        ret=CommandGate->runAction(dequeueEventAction, (void*)event, (void*)data, (void*)sleep);
    }
    release();
    return ret;
}

IOReturn Apple16X50UARTSync::
dequeueEventAction(OSObject *owner, void*arg0, void*arg1, void*arg2, void*)
{ return ((Apple16X50UARTSync *)owner)->dequeueEventGated((UInt32*)arg0, (UInt32*)arg1, (bool)arg2); }

IOReturn Apple16X50UARTSync::dequeueEventGated(UInt32 *event, UInt32 *data, bool sleep)
{
    DEBUG_IOLog("%s::dequeueEventGated(*0x%02x,*0x%02x,%s)\n", Name, (int)*event, (int)*data, BOOLSTR(sleep));
    if (Stage!=kAcquired) return kIOReturnOffline;
    if (!(State&PD_S_ACTIVE)) return kIOReturnOffline;
    do {
        UInt8 e = RxQ->dequeueEvent(data);
        if (e != PD_E_EOQ) {
            *event = (UInt32)e;
            break; // exit do-while loop
        } else {
            if (sleep) {
                UInt32 state=0;
                IOReturn rtn = watchStateGated(&state, (UInt32)PD_S_TXQ_EMPTY);
                if (rtn != kIOReturnSuccess) return rtn;
            }
        }
    } while (sleep);
    if (RxQ->dequeueThresholdExceeded())
        setStateGated(generateRxQState(), PD_S_RXQ_MASK | kRxAutoFlow);
    return kIOReturnSuccess;
}

/* enqueueData will attempt to copy data from the specified buffer to the
 * TX queue as a sequence of VALID_DATA events.  The argument bufferSize
 * specifies the number of bytes to be sent.  The actual number of bytes
 * transferred is returned in transferCount.  If sleep is true, then this
 * method will sleep until all bytes can be transferred.  If sleep is
 * false, then as many bytes as possible will be copied to the TX queue.
 *
 * Note that the caller should ALWAYS check the transferCount unless the
 * return value was kIOReturnBadArgument, indicating one or more arguments
 * were not valid.  Other possible return values are kIOReturnSuccess if all
 * requirements were met; kIOReturnIPCError if sleep was interrupted by
 * a signal; kIOReturnIOError if the port was deactivated.
 */
IOReturn Apple16X50UARTSync::
enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
{
    UInt8 *p = buffer;
    DEBUG_IOLog("%s::enqueueData(%p,%d,%p,%s)\n", Name, buffer, (int)size, count, BOOLSTR(sleep));
    if (!(buffer && count)) return kIOReturnBadArgument;
    *count = 0;
    if (Stage!=kAcquired) return kIOReturnOffline;
    if (!(State&PD_S_ACTIVE)) return kIOReturnOffline;
    retain();
    while (*count < size) {
        UInt32 chunkSize, chunkCount=0;
        IOReturn rtn;
        chunkSize = min((size-(*count)), 2048);
        rtn = CommandGate->runAction(enqueueDataAction, (void*)p, (void*)chunkSize,
                                 (void*)&chunkCount, (void*)sleep);
        if (rtn != kIOReturnBadArgument) {
            *count += chunkCount;
            p += chunkCount;
        }
        if (rtn != kIOReturnSuccess) {
            release();
            return rtn;
        }
    }
    release();
    return kIOReturnSuccess;
}

IOReturn Apple16X50UARTSync::
enqueueDataAction(OSObject *owner, void*arg0, void*arg1, void*arg2, void*arg3)
{ return ((Apple16X50UARTSync *)owner)->enqueueDataGated((UInt8*)arg0, (UInt32)arg1, (UInt32*)arg2, (bool)arg3); }

IOReturn Apple16X50UARTSync::
enqueueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
{
    UInt8 *p = buffer;
    DEBUG_IOLog("%s::enqueueDataGated(%p,%d,%p,%s)\n", Name, buffer, (int)size, count, BOOLSTR(sleep));
    if (!(buffer && count)) return kIOReturnBadArgument;
    *count = 0;
    if (Stage!=kAcquired) return kIOReturnOffline;
    if (!(State&PD_S_ACTIVE)) return kIOReturnOffline;
    for (*count=0; (*count) < size; (*count)++) {
        IOReturn rtn = enqueueEventGated(PD_E_VALID_DATA_BYTE, (UInt32)(*p), sleep);
        if (rtn != kIOReturnSuccess) return rtn;
        p++;
    }
    if (TxQ->enqueueThresholdExceeded())
        setStateGated(TxQ->getState(), PD_S_TXQ_MASK);
    return kIOReturnSuccess;
}

/* dequeueData() will attempt to copy data from the RX queue to the specified
 * buffer.  No more than bufferSize VALID_DATA events will be transferred.
 * In other words, copying will continue until either a non-data event is
 * encountered or the transfer buffer is full.  The actual number of bytes
 * transferred is returned in transferCount.
 *
 * The sleep semantics of this method are slightly more complicated than
 * other methods in this API:  Basically, this method will continue to
 * sleep until either minCount characters have been received or a non
 * data event is next in the RX queue.  If minCount is zero, then this
 * method never sleeps and will return immediately if the queue is empty.
 *
 * Note that the caller should ALWAYS check the transferCount unless the
 * return value was kIOReturnBadArgument, indicating one or more arguments
 * were not valid.  Other possible return values are kIOReturnSuccess if all
 * requirements were met; kIOReturnIPCError if sleep was interrupted by
 * a signal; kIOReturnIOError if the port was deactivated.
 */
IOReturn Apple16X50UARTSync::
dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
{
    UInt8 *p = buffer;
    IOReturn rtn = kIOReturnSuccess;
    enum { kTimerDisarmed, kTimerArmed, kTimerStarted } timerControl;

    DEBUG_IOLog("%s::dequeueData(%p,%d,%p,%d)\n", Name, buffer, (int)size, count, (int)min);

    if (!(count && buffer && (size >= min))) return kIOReturnBadArgument;
    *count = 0;
    if (Stage!=kAcquired) {
        return kIOReturnOffline;
    }
    if (!(State&PD_S_ACTIVE)) {
        return kIOReturnOffline;
    }

    // 1st, get as much data as we can without sleeping:
    // if we can avoid setting the latency timeout, we should

    while (*count < size) {
        UInt32 chunkSize, chunkCount;
        chunkSize = min((size-(*count)), 2048);
        if (!CommandGate) return kIOReturnOffline;
        retain();
        rtn = CommandGate->runAction(dequeueDataAction, (void*)p, (void*)chunkSize, (void*)&chunkCount, (void*)(false));
        release();
        if (rtn != kIOReturnSuccess) return rtn;
        if (chunkCount==0) { // nothing left on queue
            if ((*count) >= min) // but we've already met the minimum
                return rtn;
            else	// we are going to have to sleep to meet the minimum
                break;
        }
        *count += chunkCount;	// adjust count & p to account for new data
        p += chunkCount;
    }

    retain();
    // We now need to sleep for data, but we must set the latency timer if *some* data
    // has already been read.  Otherwise, we set the latency timer after the first
    // byte comes in.

    timerControl = ((DataLatInterval > 0) && ((min-(*count)) > 1)) ? kTimerArmed : kTimerDisarmed;

    while (*count < min) {
        UInt32 chunkSize, chunkCount;
	if ((timerControl == kTimerArmed) && (*count)) {
	    WorkLoop->runAction(timerControlAction, DataLatTimer, (void*)DataLatInterval); // set the latency timeout
	    timerControl = kTimerStarted;
	}
        chunkSize = min((min-(*count)), (timerControl == kTimerArmed) ? (UInt32)1 : (UInt32)2048);
        rtn = CommandGate->runAction(dequeueDataAction, (void*)p, (void*)chunkSize, (void*)&chunkCount, (void*)(true));
        if (rtn == kIOReturnBadArgument) break;
        *count += chunkCount;
        p += chunkCount;
        if ( (rtn != kIOReturnSuccess) || (chunkCount == 0) )
            break; // non-data next in queue
    }

    if (timerControl == kTimerStarted)
	WorkLoop->runAction(timerControlAction, DataLatTimer); // clear the latency timeout
    release();
    return rtn;
}

IOReturn Apple16X50UARTSync::
dequeueDataAction(OSObject *owner, void*arg0, void*arg1, void*arg2, void*arg3)
{ return ((Apple16X50UARTSync *)owner)->dequeueDataGated((UInt8*)arg0, (UInt32)arg1, (UInt32*)arg2, (UInt32)arg3); }

IOReturn Apple16X50UARTSync::
dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
{
    UInt8 *p = buffer, event;
    UInt32 data='?', bytes;
    IOReturn rtn = kIOReturnSuccess;
    
    DEBUG_IOLog("%s::dequeueDataGated(%p,%d,%p,%s)\n", Name, buffer, (int)size, count, BOOLSTR(sleep));
    if (!(buffer && count)) return kIOReturnBadArgument;
    *count = bytes = 0;
    if (Stage!=kAcquired) return kIOReturnOffline;
    if (!(State&PD_S_ACTIVE)) return kIOReturnOffline;
    while (bytes < size) {
        event = RxQ->peekEvent();
        if (event==PD_E_VALID_DATA_BYTE) {
            assert(RxQ->Count);
            event=RxQ->dequeueEvent(&data);
            assert(event==PD_E_VALID_DATA_BYTE);
            *p++ = (UInt8)data;
            bytes++;
        } else if ((event==PD_E_EOQ) && sleep) {
            UInt32 state=0;
            rtn = watchStateGated(&state, (UInt32)PD_S_RXQ_EMPTY);
            if (rtn != kIOReturnSuccess) break;
        } else break;
    }
    *count = bytes;
    if (RxQ->dequeueThresholdExceeded())
        setStateGated(generateRxQState(), PD_S_RXQ_MASK | kRxAutoFlow);
    return rtn;
}

bool Apple16X50UARTSync::attach(IOService *provider)
{
    DEBUG_IOLog("%s::attach(%p)\n", Name, provider);
    assert(Stage == kAllocated);
    
    Provider = OSDynamicCast(Apple16X50BusInterface, provider);
    if (!Provider) return false;

    if ((!Provider->metaCast("com_apple_driver_16X50PCCard")) && (!Provider->metaCast("com_apple_driver_16X50PCI")) &&
        (!Provider->metaCast("com_apple_driver_16X50ACPI")))
        return false;	// Only allow us to attach to classes in this project - this is not a publicly usable class
    
    if (!IOService::attach(provider)) return false;

    Stage = kAttached;
    return true;
}

bool Apple16X50UARTSync::attachToChild(IORegistryEntry *child, const IORegistryPlane *plane)
{
    if (!super::attachToChild(child, plane))
        return false;
    
    if ((child->metaCast("IOSerialBSDClient")) && (plane == gIOServicePlane))
        child->setProperty(kNPProductNameKey, getProperty(kNPProductNameKey));
    
    return true;
}

bool Apple16X50UARTSync::start(IOService *provider)
{
    char buf[80];
    OSString *nameString = OSDynamicCast(OSString, getProperty(kIOTTYSuffixKey));
    if (nameString) {
        sprintf(buf, "%s%s", Name, (char *)(nameString->getCStringNoCopy()));
        setName(buf);
        Name=getName();
    }
    DEBUG_IOLog("%s::start(%p)\n", Name, provider);
    assert(Stage == kAttached);

    if (!super::start(provider)) return false;
    
    WorkLoop = OSDynamicCast(IOWorkLoop, Provider->getWorkLoop(fRefCon));
    if (!WorkLoop) return false;
    WorkLoop->retain();
    
    CommandGate = IOCommandGate::commandGate(this);
    if (!CommandGate) goto fail;
    if (WorkLoop->addEventSource(CommandGate) != kIOReturnSuccess) goto fail;
    CommandGate->enable();

    FrameTimer = IOTimerEventSource::timerEventSource(this, frameTimeoutAction);
    if (!FrameTimer) goto fail;
    if (WorkLoop->addEventSource(FrameTimer) != kIOReturnSuccess) goto fail;
    FrameTimer->enable();

    DataLatTimer = IOTimerEventSource::timerEventSource(this, dataLatencyTimeoutAction);
    if (!DataLatTimer) goto fail;
    if (WorkLoop->addEventSource(DataLatTimer) != kIOReturnSuccess) goto fail;
    DataLatTimer->enable();

    DelayTimer = IOTimerEventSource::timerEventSource(this, delayTimeoutAction);
    if (!DelayTimer) goto fail;
    if (WorkLoop->addEventSource(DelayTimer) != kIOReturnSuccess) goto fail;
    DelayTimer->enable();

#ifdef HEARTBEAT
    HeartBeatTimer = IOTimerEventSource::timerEventSource(this, heartbeatTimeoutAction);
    if (!HeartBeatTimer) goto fail;
    if (WorkLoop->addEventSource(HeartBeatTimer) != kIOReturnSuccess) goto fail;
    HeartBeatTimer->enable();
#endif
    
    TxQ = new Apple16X50Queue(kTxQ);
    RxQ = new Apple16X50Queue(kRxQ);

    resetUART();
    sprintf (buf, "%s FIFO=%d MaxBaud=%d", IOFindNameForValue(UART_Type, gUARTnames), (int)FIFO_Size, (int)(MaxBaud>>1));
    IOLog("%s: Detected %s\n", Name, buf);
    setProperty("UART Type", buf);

    Stage = kStarted;
    registerService();
	
    // register for power changes
    fCurrentPowerState = 1;		// we default to power on
    if (!initForPM(provider)) 
	{
        IOLog("Apple16X50UARTSync::start - failed to init for power management");
        //shutDown();
		return false;
    }	
	
    return true;
    
fail:
    DEBUG_IOLog("%s::start() Unable to setup EventSources\n", Name);
    RELEASE(WorkLoop);
    return false;
}

bool Apple16X50UARTSync::terminate(IOOptionBits options)
{
    Stage = kTerminated;
    OffLine = true;
    CommandGate->commandWakeup((void*)&Acquired); // wake all waiting to acquire the port
    CommandGate->commandWakeup((void*)&State); // wake all waiting for state
    return super::terminate(options);
}

bool Apple16X50UARTSync::finalize(IOOptionBits options)
{
    DEBUG_IOLog("%s::finalize()\n", Name);
    assert((Stage==kReleased)||(Stage==kStarted)||(Stage==kTerminated));
    assert(CommandGate);
    Stage = kFinalized;
    OffLine = true;
    // There should be no clients at this point, but lets wake everybody up to be sure
    CommandGate->commandWakeup((void*)&Acquired); // wake all waiting to acquire the port
    CommandGate->commandWakeup((void*)&State); // wake all waiting for state
    if (DelayTimer) {
        DelayTimer->cancelTimeout();
        DelayTimer->disable();
    }
    if (DataLatTimer) {
        DataLatTimer->cancelTimeout();
        DataLatTimer->disable();
    }
    if (FrameTimer) {
        FrameTimer->cancelTimeout();
        FrameTimer->disable();
    }
#ifdef HEARTBEAT
    if (HeartBeatTimer) {
        HeartBeatTimer->cancelTimeout();
        HeartBeatTimer->disable();
    }
#endif
    RxQ->setSize(0);
    TxQ->setSize(0);
    
    return super::finalize(options);
}

void Apple16X50UARTSync::stop(IOService *provider)
{
    retain();
    DEBUG_IOLog("%s::stop(%p)\n", Name, provider);
    assert(Stage==kFinalized);
    Stage = kStopped;
    OffLine = true;
    CommandGate->runAction(stopAction, (void*)provider);
    super::stop(provider);
    release();
}

IOReturn Apple16X50UARTSync::
stopAction(OSObject *owner, void*arg0, void*, void*, void*)
{ ((Apple16X50UARTSync *)owner)->stopGated((IOService *)arg0); return kIOReturnSuccess; }

void Apple16X50UARTSync::stopGated(IOService *provider)
{
    DEBUG_IOLog("%s::stopGated(%p)\n", Name, provider);
    OffLine = true;
    CommandGate->commandWakeup((void*)&Acquired);
    CommandGate->commandWakeup((void*)&State);
}

void Apple16X50UARTSync::free()
{
    DEBUG_IOLog("%s::free()\n", Name);
    assert(Stage == kStopped);
    assert(WorkLoop);
    if (DelayTimer) {
        WorkLoop->removeEventSource(DelayTimer);
        RELEASE(DelayTimer);
    }
    if (DataLatTimer) {
        WorkLoop->removeEventSource(DataLatTimer);
        RELEASE(DataLatTimer);
    }
    if (FrameTimer) {
        WorkLoop->removeEventSource(FrameTimer);
        RELEASE(FrameTimer);
    }
#ifdef HEARTBEAT
    if (HeartBeatTimer) {
        WorkLoop->removeEventSource(HeartBeatTimer);
        RELEASE(HeartBeatTimer);
    }
#endif
    WorkLoop->removeEventSource(CommandGate);
//    XXX workaround for 2999641 - power off/sleep panic
//    RELEASE(CommandGate);
    RELEASE(WorkLoop);
    
    if (RxQ) delete RxQ;
    if (TxQ) delete TxQ;

    Stage = kFreed;
    super::free();
}

/* flowMachine() - This function is called anytime the auto-bits of FlowControl
 * change.  It will review the current state of the port and its queues, changing
 * the state of the modem control bits.
 */
void Apple16X50UARTSync::flowMachine()
{
    register UInt32 state = State;
    if ((FlowControl & PD_RS232_A_RFR) || (FlowControl & PD_RS232_A_RXO)) {
        // RTS/RFR or XON/XOFF is enabled, so DTR is used as session control
        if (FlowControl & PD_RS232_A_DTR)
            state = boolBit(state, (State & PD_S_ACTIVE), PD_RS232_A_DTR);
        if (FlowControl & PD_RS232_A_RFR)
            state = boolBit(state, ((RxQ->Count <= RxQ->HighWater) && (State & PD_S_ACTIVE)), PD_RS232_A_RFR);
        else if (FlowControl & PD_RS232_A_RXO) {
            state = boolBit(state, (RxQ->Count <= RxQ->HighWater), PD_RS232_A_RXO);
            if (RxQ->Count > RxQ->HighWater)
                RXO_State = kXOffNeeded;
            else if ((RXO_State != kXO_Idle) && (State & PD_S_ACTIVE))
                RXO_State = kXOnNeeded;
        }
    } else if (FlowControl & PD_RS232_A_DTR) {
        // RTS/RFR and XON/XOFF are disabled, so DTR is used as flow control
        state = boolBit(state, ((RxQ->Count <= RxQ->HighWater)  && (State & PD_S_ACTIVE)), PD_RS232_A_DTR);
    }
    setStateGated(state, kRxAutoFlow);
}

// generateRxQState() : Called to generate the status bits for queue control.
// This routine should be called any time an enqueue/dequeue boundary is crossed
// or any of the queue level variables are changed by the user.
// WARNING: {BIGGEST_EVENT ≤ LowWater ≤ (HighWater-BIGGEST_EVENT)} and
//	{(LowWater-BIGGEST_EVENT) ≤ HighWater ≤ (size-BIGGEST_EVENT)} must be enforced.
UInt32 Apple16X50UARTSync::generateRxQState()
{
    UInt32 state = State & (kRxAutoFlow | kTxAutoFlow);
    tQueueState fifostate = RxQ->getState();
    state = maskMux(state, (UInt32)fifostate >> PD_S_RX_OFFSET, PD_S_RXQ_MASK);
    switch (fifostate) {
        case kQueueEmpty :
        case kQueueLow :
            if (FlowControl & PD_RS232_A_RFR) {
                state |= PD_RS232_S_RFR;
            } else if (FlowControl & PD_RS232_A_RXO) {
                state |= PD_RS232_S_RXO;
                switch (RXO_State) {
                    case kXOffSent :
                    case kXO_Idle :	RXO_State=kXOnNeeded;	break;
                    case kXOffNeeded :	RXO_State=kXOnSent;	break;
                    default :		break;
                }                    
            } else if (FlowControl & PD_RS232_A_DTR) {
                state |= PD_RS232_S_DTR;
            }
            break;
        case kQueueHigh :
        case kQueueFull :
            if (FlowControl & PD_RS232_A_RFR) {
                state &= ~PD_RS232_S_RFR;
            } else if (FlowControl & PD_RS232_A_RXO) {
                state &= ~PD_RS232_S_RXO;
                switch (RXO_State) {
                    case kXOnSent :
                    case kXO_Idle :	RXO_State=kXOffNeeded;	break;
                    case kXOnNeeded :	RXO_State=kXOffSent;	break;
                    default :		break;
                }
            } else if (FlowControl & PD_RS232_A_DTR) {
                state &= ~PD_RS232_S_DTR;
            }
            break;
        case kQueueMedium : break;
    }
    return state;
}

IOReturn Apple16X50UARTSync::activatePort()
{
    DEBUG_IOLog("%s::activatePort()\n", Name);
    if (State & PD_S_ACTIVE) return kIOReturnSuccess;
    if (OffLine) return kIOReturnOffline;
    if (RxQ->Size == 0) RxQ->setSize(); // default Queue size
    if (TxQ->Size == 0) TxQ->setSize(); // default Queue size
    programUART();
    outb(kREG_FIFOControl, (FCR_Image | kFIFO_ResetTx | kFIFO_ResetRx));
    setStateGated(PD_S_ACTIVE, PD_S_ACTIVE); // activate port
    flowMachine();  // set up HW Flow Control
    setStateGated(TxQ->getState() | generateRxQState(),
                PD_S_TXQ_MASK | PD_S_RXQ_MASK | kRxAutoFlow);
    if ( !(State & PD_S_TX_BUSY) ) // restart TX engine if idle
        FrameTimer->setTimeoutUS(1);
    outb(kREG_IRQ_Enable, IER_Mask &
         ( kIRQEN_RxDataAvail | kIRQEN_TxDataEmpty | kIRQEN_LineStatus  | kIRQEN_ModemStatus ) );
    return kIOReturnSuccess;
}

void Apple16X50UARTSync::deactivatePort()
{
    DEBUG_IOLog("%s::deactivatePort()\n", Name);
    if (State & PD_S_ACTIVE) {
        outb(kREG_IRQ_Enable, OffLine?0x00:IER_Mask & kIRQEN_ModemStatus);

#ifdef HEARTBEAT
        HeartBeatTimer->cancelTimeout();
#endif
        DelayTimer->cancelTimeout();
        DataLatTimer->cancelTimeout();
        FrameTimer->cancelTimeout();

        // Clear active and wakeup all sleepers
        setStateGated(0, PD_S_ACTIVE);
        TxQ->setSize(0); // frees associated storage
        RxQ->setSize(0); // frees associated storage
        flowMachine();
    }
}

IOReturn Apple16X50UARTSync::
timerControlAction(OSObject *owner, void*arg0, void*, void*, void*)
{
    UInt32 usec = (UInt32)arg0;
//    DEBUG_IOLog("Apple16X50UART: timerControl(%p,%d)\n", owner, (int)usec);
    if (usec /*&& (!(((IOTimerEventSource *)owner)->OffLine))*/)
        return ((IOTimerEventSource *)owner)->setTimeoutUS(usec);
    else
        ((IOTimerEventSource *)owner)->cancelTimeout();
    return kIOReturnSuccess;
}

inline void Apple16X50UARTSync::startTxEngine()
{
    if (OffLine) return;
    if (!(State & PD_S_TX_BUSY))
        FrameTimer->setTimeoutUS(1);
    State |= PD_S_TX_BUSY;
    DEBUG_IOLog("%s::startTxEngine()\n", Name);
    //  Provider->causeInterrupt(0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This is where the bulk of the chip driver logic lives.  It isn't pretty.
 * This section includes the interrupt handler and a couple callouts
 * that are used to simulate interrupts:
 *	dataLatencyTimeoutAction()
 *	frameTimeoutAction()
 *	delayTimeoutAction()
 */
void Apple16X50UARTSync::
dataLatencyTimeoutAction(OSObject *owner, IOTimerEventSource *)
{
    Apple16X50UARTSync *me = (Apple16X50UARTSync *)owner;
    DEBUG_IOLog("%s::dataLatencyTimeoutAction(%p,*)\n", ((Apple16X50UARTSync *)owner)->Name, owner);
    me->RxQ->enqueueEvent(PD_E_DATA_LATENCY, 0);
    if (me->RxQ->enqueueThresholdExceeded())
        me->setStateGated(me->generateRxQState(), PD_S_RXQ_MASK | kRxAutoFlow);
}

/* Frame TimeOut:
 * This timeout is used to simulate an interrupt after the transmitter
 * shift-register goes idle.  Hardware interrupts are only available when
 * the holding register is empty.  When setting the baud rate, or a line
 * break, it is important to wait for the xmit shift register to finish
 * its job first.
 */
void Apple16X50UARTSync::
frameTimeoutAction(OSObject *owner, IOTimerEventSource *)
{
    DEBUG_IOLog("%s::frameTimeoutAction(%p,*) F+", ((Apple16X50UARTSync *)owner)->Name, owner);
    ((Apple16X50UARTSync *)owner)->FrameTimerRunning = false;
    ((Apple16X50UARTSync *)owner)->interrupt();
    DEBUG_IOLog("-F\n");
}

void Apple16X50UARTSync::
delayTimeoutAction(OSObject *owner, IOTimerEventSource *)
{
    DEBUG_IOLog("%s::delayTimeoutAction(%p,*) D+", ((Apple16X50UARTSync *)owner)->Name, owner);
    ((Apple16X50UARTSync *)owner)->DelayTimerRunning = false;
    ((Apple16X50UARTSync *)owner)->interrupt();
    DEBUG_IOLog("-D\n");
}

#ifdef HEARTBEAT
void Apple16X50UARTSync::
heartbeatTimeoutAction(OSObject *owner, IOTimerEventSource *)
{
    Apple16X50UARTSync *me = (Apple16X50UARTSync *)owner;
    DEBUG_IOLog("%s::heartbeatTimeoutAction(%p,*)\n", ((Apple16X50UARTSync *)owner)->Name, owner);
    if (!OffLine && (me->State & PD_S_ACQUIRED)) {
	if (me->HeartBeatNeeded) {
            DEBUG_IOLog("H+");
	    me->interrupt();
            DEBUG_IOLog("-H\n");
	}
	me->HeartBeatNeeded = true;
	me->HeartBeatTimer->setTimeoutUS(me->HeartBeatInterval);
    }
}
#endif

/* The basic strategy is to go through the loop at least once, regardless
 * of what the Interrupt Identification Register (iir) says.  The real info
 * is in the Line Status Register (lsr) anyway.  Each pass through the loop
 * will process one more of each type of event.  This will continue until
 * the chip clears its interrupt AND there are no deferred operations to
 * complete.  Specifically, the chip will clear its interrupt when the
 * FIFO's drop below their low-water mark;  We want to continue reading
 * RX till empty and writing TX until full, even though the IIR says there
 * is no interrupt condition.
 */
void Apple16X50UARTSync::interrupt()
{
    UInt32	delta=0, state=State;
    UInt32	tmp=0, data=0;
    UInt8	lsr, event=0;
    UInt32	fifoOverRun=0, fifoFree=0;
    bool	waitForTxIdle = FrameTimerRunning;
    bool	again;
    bool	breakme=false;

#ifdef HEARTBEAT
    HeartBeatNeeded = false;
#endif

    if (OffLine || (Stage != kAcquired)) {
        DEBUG_IOLog("interrupt() while offline or not acquired!\n");
        return; // don't want to touch any registers
    }
    DEBUG_putc('/');
    
    do {
        DEBUG_putc('A');
        again = false;

        // this while-loop handles all rx data in the fifo
        while ((lsr = inb(kREG_LineStatus)) & kLSR_DataReady) {
            data = inb(kREG_Data);
            DEBUG_putc('B');
            if ((lsr == 0xff) && (data == 0xff)) {
                DEBUG_IOLog("%s::interrupt() UART has been removed or powered down!\\\n", Name);
                return;
            }
            if (!(state & PD_S_RX_ENABLE))
                continue;	// Discard inbound data if RX is disabled
            if ((lsr & kLSR_Overrun) && !fifoOverRun)
                fifoOverRun = FIFO_Size;	// enqueue an HW overrun event after reading FIFO_Size bytes
            if (lsr & kLSR_GotBreak) {
                RxQ->enqueueEvent(PD_RS232_E_RX_LINE_BREAK);
            } else if (lsr & kLSR_FramingError ) {
                RxQ->enqueueEvent(PD_E_FRAMING_BYTE, data);
            } else if ((lsr & kLSR_ParityError) && (!IgnoreParityErrors)) {
                RxQ->enqueueEvent(PD_E_INTEGRITY_BYTE, data);
            } else { // valid data, or we are ignoring parity errors
                data &= DataMask;
                if ((FlowControl & (PD_RS232_A_TXO|PD_RS232_N_TXO))&&(data == XOnChar)) {
                    state |= PD_RS232_S_TXO;
                    delta |= PD_RS232_S_TXO;
                } else if ((FlowControl & (PD_RS232_A_TXO|PD_RS232_N_TXO))&&(data == XOffChar) ) {
                    state &= ~PD_RS232_S_TXO;
                    delta |= PD_RS232_S_TXO;
                } else { // real data
                    if (FlowControl & PD_RS232_A_XANY) {
                        state |= PD_RS232_S_TXO;
                        delta |= PD_RS232_S_TXO;
                    }
                    event = ((SW_Special[data>>5] & (1<<(data&0x1f))) ? PD_E_SPECIAL_BYTE : PD_E_VALID_DATA_BYTE);
                    RxQ->enqueueEvent(event, data);
                    DEBUG_putc('\'');
                    DEBUG_putc((char)data);
                    breakme=true;
               }
            }
            if (fifoOverRun && (--fifoOverRun == 0))
                RxQ->enqueueEvent(PD_E_HW_OVERRUN_ERROR);
        }
        if (fifoOverRun) { // Odd - we may think the FIFO is bigger than it really is...
            RxQ->enqueueEvent(PD_E_HW_OVERRUN_ERROR);
            fifoOverRun = 0;
        }
        if ((state&PD_S_ACTIVE) && (RxQ->enqueueThresholdExceeded())) {
	    // Regenerate the RX queue state
            if (breakme) {
                breakme=false;
            }
            state = maskMux(state, generateRxQState(), (PD_S_RXQ_MASK | kRxAutoFlow));
            delta |= ((PD_S_RXQ_MASK | kRxAutoFlow) & (State ^ state));
            programMCR(state);
        }
        state = maskMux(state, msrState(), kMSR_StateMask);
        delta |= (kMSR_StateMask & (State ^ state));

        // Now we deal with TX data.  The algorithm is complicated by
        // several things:  The chip will only generate an interrupt when
        // the TX holding register is empty.  Unfortunately, this means the
        // shift register may still be busy for up to one frame time (1ms at
        // 9600 bps).  If any of the following are true, then we must wait
        // till the TX shift register is idle before proceeding:
        // (1) The next event would change the characteristics of the frame
        //     (baud,  word size, parity...).
        // (2) The next event is a break event.
        // (3) The FIFO is enabled and there is exactly one byte in the TX
        //     queue.  This is due to a 16550 bug that might leave one byte
        //     in the TX FIFO  unsent until a second byte is written into
        //     the FIFO.
        //
        // If any of these are true, then we set the bool waitForTxIdle.
        // At the end of the isr, we check this bit and schedule an
        // interrupt event in exactly one frame time.  This may result in
        // some transmitter-idle time but it is the only way to guarantee
        // reliable communications.
        //
        // If none of these three conditions are true, or the the TX shift
        // register is already idle, we can safely process the next event.

        if ((fifoFree == 0) && (!(lsr & kLSR_TxDataEmpty)))
            continue; // can't TX anything new right now
        if (lsr & kLSR_TxIdle) {
            waitForTxIdle = false;
            fifoFree = FIFO_Size;
            if (FrameTimerRunning) {
                FrameTimer->cancelTimeout();
                FrameTimerRunning = false;
            }
        }
        if ((FlowControl & PD_RS232_A_RXO) && (RXO_State > kXO_Idle)) {
            outb(kREG_Data, (RXO_State == kXOnNeeded) ? XOnChar : XOffChar);
            (int)RXO_State = -(int)(RXO_State);
            if (fifoFree) fifoFree--;
        } else if (DelayTimerRunning) {
            fifoFree = 0;
        } else switch (TxQ->peekEvent()) {
            case PD_E_EOQ :  /* no TX event to process */
                fifoFree = 0;
                break;
            case PD_E_VALID_DATA_BYTE : // next TX event is Data
                DEBUG_putc('D');
                if ( (~state) & ((kTxAutoFlow & FlowControl) | PD_S_TX_ENABLE) ) {
                    fifoFree = 0;	// TX flow control is preventing transmission
                    break;
                }
                if (((lsr&(kLSR_TxDataEmpty|kLSR_TxIdle))==kLSR_TxDataEmpty) && (TxQ->peekEvent(1)!=PD_E_VALID_DATA_BYTE))
                    waitForTxIdle = true;
                while (fifoFree) {
                    event = TxQ->peekEvent();
                    if (event == PD_E_VALID_DATA_BYTE) {
                        event = TxQ->dequeueEvent(&data);
                        assert(event == PD_E_VALID_DATA_BYTE);
                        outb(kREG_Data, (UInt8)data);
                        fifoFree--;
                    } else break;
                }
                    break;
            default: // next TX event is Control
                DEBUG_putc('C');
                fifoFree = 0;
                if (lsr & kLSR_TxIdle) {
                    event = TxQ->dequeueEvent(&data);
                    executeEventGated(event, data, &state, &delta);
                    again = true;
                } else	// TX engine is not Idle
                    waitForTxIdle = true;
                break;
        }
    } while (again || fifoFree || ((inb(kREG_IRQ_Ident)&kIRQID_None)==0));

        if ((lsr&kLSR_TxDataEmpty) && (!(lsr&kLSR_TxIdle)))
        waitForTxIdle = true;
    if (waitForTxIdle && (!FrameTimerRunning)) {
        FrameTimerRunning = true;
        FrameTimer->setTimeoutUS(FrameInterval);
    }
        
    // Did we dequeue enough to cross a boundary?
    if (TxQ->dequeueThresholdExceeded())
        state = maskMux(state, TxQ->getState(), PD_S_TXQ_MASK);
    state = boolBit(state, !(lsr & kLSR_TxIdle), PD_S_TX_BUSY);
    delta |= (State ^ state); // pick up any changes we might have missed
    // If any of the 'delta' bits match the 'notify' bits in FlowControl,
    // then we need to enqueue a FLOW_CONTROL event with 'delta' in the MSW,
    // and State in the LSW.
    if ( tmp = ((delta << PD_RS232_D_SHIFT) & FlowControl) )
        RxQ->enqueueEvent(PD_E_FLOW_CONTROL, (state & PD_RS232_S_MASK) | tmp);
        
    // Wake up all threads asleep on WatchStateMask
    State = state;
    if (delta & WatchStateMask) {
        DEBUG_putc('W');
        assert(CommandGate);
        CommandGate->commandWakeup((void*)&State);
    }
    DEBUG_putc('\\');
    
    return;
}

// Factory method.  This static factory method will attempt to identify
// the UART referenced by provider&refCon.  If no UART is identified,
// NULL is returned.  If a UART is identified an instance is allocated,
// initialized, and returned.
Apple16X50UARTSync *Apple16X50UARTSync::
probeUART(Apple16X50BusInterface *provider, void *refCon, tUART_Type type)
{
    UInt32	fifoSize, tries=0;
    
    DEBUG_IOLog("Apple16X50UARTSync::probeUART(%p,%p,%s)\n",
                provider, refCon, IOFindNameForValue(type, gUARTnames));

    if ((!provider->metaCast("com_apple_driver_16X50PCCard")) && (!provider->metaCast("com_apple_driver_16X50PCI")) &&
        (!provider->metaCast("com_apple_driver_16X50ACPI")))
        return NULL;	// Only allow us to be instantiated by classes in this project - this is not a publicly usable class

    while ((kUART_Unknown == type) && (tries++ < 16)) {
	DEBUG_IOLog("Apple16X50UARTSync::probeUART() - Try # %d\n", tries);
        type = identifyUART(provider, refCon);
        if ((kUART_Unknown == type) && (tries < 16)) IOSleep(250); // wait 1/4 sec before trying again.
    }
    
    switch (type) {
        case kUART_16550C :
        case kUART_16C1550 :	fifoSize = 16;	break;

        case kUART_16C650 :	fifoSize = 32;	break;

        case kUART_16C750 :
        case kUART_16C950 :	fifoSize = 128;	break;
            
        default : return NULL;
    }
    
    Apple16X50UARTSync *uart = new Apple16X50UARTSync;
    if (!uart) return NULL;
    
    // Initialize ALL the iVars

    // Global resources
    uart->Provider = NULL;
    uart->RxQ = NULL;
    uart->TxQ = NULL;
    uart->WorkLoop = NULL;
    uart->CommandGate = NULL;
    uart->Name = "Apple16X50UARTSync";
    uart->Stage = kAllocated;
    uart->State = 0x00000000;
    uart->WatchStateMask = 0x00000000;
    uart->Acquired = false;
    uart->OffLine = false;

    // Timers
    uart->FrameTimer = NULL;
    uart->FrameInterval=0;
    uart->FrameTimerRunning=false;
    uart->DataLatTimer = NULL;
    uart->DataLatInterval=0;
    uart->DelayTimer = NULL;
    uart->DelayInterval=0;
    uart->DelayTimerRunning=false;
#ifdef HEARTBEAT
    uart->HeartBeatTimer = NULL;
    uart->HeartBeatInterval=0;
    uart->HeartBeatNeeded=false;
#endif

    // UART characteristics and register shadows:
    uart->UART_Type=type;
    uart->MaxBaud = 115200 bits;
    uart->FIFO_Size = fifoSize;
    uart->LCR_Image=0x00;
    uart->FCR_Image=0x00;
    uart->IER_Mask=0x00;

    // Framing and data rate
    bzero(uart->SW_Special, 256>>3);
    uart->DataWidth=0;
    uart->DataMask=0x00;
    uart->StopBits=0;
    uart->Parity=0;
    uart->BaudRate=0;
    uart->MasterClock=k1xMasterClock;	// default, the original AT Serial clock
    
    uart->Divisor=0x0000;
    uart->MinLatency=false;
    uart->IgnoreParityErrors=false;

    // Flow Control
    uart->FlowControl = 0x00000000;	// notify-on-delta & auto_control
    uart->RXO_State = kXO_Idle;
    uart->XOnChar=kXOnChar;
    uart->XOffChar=kXOffChar;
    return uart;
}

#define INB(reg)	( provider->getReg((reg), refCon) )
#define OUTB(reg,val)	( provider->setReg((reg), val, refCon) )

/* identifyUART() - This function attempts to identify the type of UART
 * present.  This routine is only intended to be run from the init method,
 * before the chip is active.  XXX - this routine needs to be revisited
 * with an eye towards more current chips (650, 750, 850, and 950 variants)
 */
tUART_Type Apple16X50UARTSync::
identifyUART(Apple16X50BusInterface *provider, void *refCon)
{
    register UInt32 tmp;
 
    DEBUG_IOLog("Apple16X50UARTSync::identifyUART()\n");
    
    /* Verify that the BRG Divisor Register is accessable */
    OUTB(kREG_LineControl, kLCR_DivisorAccess);	/* Set DLAB=1 */
    OUTB(kREG_DivisorLSB, 0x5a);
    if (INB(kREG_DivisorLSB)!=0x5a) return kUART_Unknown;
    OUTB(kREG_DivisorLSB, 0xa5);
    if (INB(kREG_DivisorLSB)!=0xa5) return kUART_Unknown;
    OUTB(kREG_LineControl, 0x00);	/* Set DLAB=0 */
    DEBUG_IOLog("Apple16X50UARTSync::identifyUART() BRG Divisor is accessable\n");
    
    /* Verify that the Scratch Pad Register is accessable */
    OUTB(kREG_Scratch, 0x5a);
    if (INB(kREG_Scratch)!=0x5a) return kUART_8250;
    OUTB(kREG_Scratch, 0xa5);
    if (INB(kREG_Scratch)!=0xa5) return kUART_8250;
    DEBUG_IOLog("Apple16X50UARTSync::identifyUART() Scratchpad is accessable\n");
    
    /* Look for FIFO Bits in FIFO Control Register */
    OUTB(kREG_FIFOControl, kFIFO_Enable);
    tmp = INB(kREG_IRQ_Ident) & kIRQID_FIFOEnabled;
    OUTB(kREG_FIFOControl, 0x00);
    switch (tmp) {
        case 0x00 : return kUART_16450;
        case 0x40 : return kUART_16C650;
        case 0x80 : return kUART_16550;
        case 0xC0 : break;
    }
    DEBUG_IOLog("Apple16X50UARTSync::identifyUART() FIFO Enable status returned 0xC0\n");

    // Check for an alternate version of the Scratchpad...
    OUTB(kREG_LineControl, 0x00);
    OUTB(kREG_Scratch, 0xde);
    OUTB(kREG_LineControl, kLCR_DivisorAccess);
    OUTB(kREG_Scratch, 0xa9);
    tmp = INB(kREG_Scratch);
    OUTB(kREG_Scratch, 0x00);
    OUTB(kREG_LineControl, 0x00);
    if ((INB(kREG_Scratch) == 0xde) && (tmp == 0xa9)) {
        DEBUG_IOLog("Apple16X50UARTSync::identifyUART() Alternate Scratchpad is accessable\n");
        return kUART_16C650;
    }

    // Check for power-down bit in the 16X1550
    tmp = INB(kREG_ModemControl) & 0x80;
    OUTB(kREG_ModemControl, 0x00);
    if (tmp == 0x80) return kUART_16C1550;

    // Look for the FCR, which is present only in the 16750 and later
    OUTB(kREG_FIFOControl, kFIFO_Enable);
    OUTB(kREG_LineControl, 0x00);	/* Set DLAB=0 */
    OUTB(kREG_LineControl, 0xbf);	/* Set DLAB=1 */
    tmp = INB(kREG_LineControl);
    OUTB(kREG_LineControl, 0x00);	/* Set DLAB=0 */
    OUTB(kREG_FIFOControl, 0x00);
    if (tmp == 0x80) {
        DEBUG_IOLog("Apple16X50UARTSync::identifyUART() 0xBF Magic number accepted!\n");
        return kUART_16C750;
    }
    
    return kUART_16550C;
}

#undef INB
#undef OUTB

// XXX - This function is a crude first attempt to determing the crystal
// frequency by timing a byte through a loopback.  It seems to fail for
// some chips (not get any bytes looped) though I'm not sure why yet.
// In those cases, it just returns a reasonable default.  When it works,
// it seems to work well.  This needs to be made more robust and should
// probably be made static and moved to Apple16X50UARTTypes.cpp.
UInt32 Apple16X50UARTSync::determineMasterClock()
{
    UInt32	count;
    UInt32	sample[5];
    UInt32	samples=0;
    UInt32	tries=10;
    IOSimpleLock *prot;

    // 1.8432 Mhz (1x, the original PC ACE clock)
    // 3.072 MHz (5/3 x)
    // 7.372 Mhz (4x)
    // 14.7455 Mhz (8x)
    // 18.432 Mhz (10x)

    prot = IOSimpleLockAlloc();
    if (!prot) return 0;
    IOSimpleLockInit(prot);

    resetUART();
    programMCR(0, false, true);
    setDivisor(1);
    outb(kREG_LineControl, kLCR_8bitData | kLCR_1bitStop | kLCR_NoParity); // 10-bit frame

    while (tries && (samples < 5)) {
        AbsoluteTime before, after;
        uint64_t duration;
        // make sure there is no data in the receive buffer
        IOSleep(1);  // let the UART "rest" before we pummel it
        count = 0;
        while (inb(kREG_LineStatus) & kLSR_DataReady) {
            inb(kREG_Data);
            if (count++ >= 256) goto nogood;
        }
        //showRegs();
        IOSimpleLockLock(prot);	// disable preemption
        outb(kREG_Data, samples*tries);	// send a byte
        clock_get_uptime(&before);	// Get the initial abstime

        count = 0;    // loop till byte is in
        while (!(inb(kREG_LineStatus) & kLSR_DataReady))
            if (count++ >= 256) break;

        clock_get_uptime(&after);	// Get the final abstime
        IOSimpleLockUnlock(prot);	// enable preemption
        if (count >= 256) goto nogood;

        if (inb(kREG_Data)!=samples*tries) goto nogood;

        // compute diff in nano seconds
        SUB_ABSOLUTETIME(&after, &before);	// "after" now holds diff
        absolutetime_to_nanoseconds(after, &duration);
        sample[samples]=(UInt32)duration;

        if (sample[samples] > 300000) goto nogood;
        DEBUG_IOLog("%s::determineMasterClock(): sample[%d/%d]=%d ns took %d loops\n",
                    Name, (int)samples, (int)tries, (int)duration, (int)count);
        samples++;
    nogood:
        tries--;
    }
    // Clean up
    IOSimpleLockFree(prot);
    DEBUG_IOLog("%s::determineMasterClock(): got %d samples out of %d tries: ", Name, (int)samples, 10-(int)tries);

    register unsigned int i;
    for (i=0; i<samples; i++)
        DEBUG_IOLog("%d ", (int)sample[i]);
    DEBUG_IOLog("\n");
    if (samples<3) goto fail;
    for (i=samples-1; i; i--) {	// bubble sort
        register unsigned int j;
        for (j=0; j<i; j++)
            if (sample[j+1] < sample[j]) {
                register UInt32 tmp;
                tmp=sample[j];
                sample[j] = sample[j+1];
                sample[j+1] = tmp;
            }
    }
    samples=sample[samples>>1];

    if (samples < 10000) MasterClock = 10 * k1xMasterClock;
    else if (samples < 15000) MasterClock = 8 * k1xMasterClock;
    else if (samples < 30000) MasterClock = 4 * k1xMasterClock;
    else if (samples < 70000) MasterClock = (5 * k1xMasterClock) / 3;
    else					
        
fail:
    MasterClock = k1xMasterClock;
    DEBUG_IOLog("%s::determineMasterClock(): median=%d ns, MasterClock=%d : ", Name, (int)samples, (int)MasterClock);
    return MasterClock;
}

void Apple16X50UARTSync::setMasterClock(UInt32 clock)
{
    MasterClock = clock;
}

void Apple16X50UARTSync::setDivisor(UInt32 div)
{
    register UInt8 lcr;
    DEBUG_IOLog("%s::setDivisor(%d)\n", Name, (int)div);
    div = CONSTRAIN(0x0001,div,0xffff);
    lcr=inb(kREG_LineControl);
    outb(kREG_LineControl, lcr | kLCR_DivisorAccess);
    outb(kREG_DivisorLSB, (UInt8)(div&0xff) );
    outb(kREG_DivisorMSB, (UInt8)((div%0xff00)>>8) );
    outb(kREG_LineControl, lcr);
}

UInt32 Apple16X50UARTSync::getDivisor()
{
    register UInt8 lcr;
    register UInt32 div;
    
    lcr=inb(kREG_LineControl);
    outb(kREG_LineControl, lcr | kLCR_DivisorAccess);
    div = inb(kREG_DivisorLSB);
    div |= (inb(kREG_DivisorMSB)<<8);
    outb(kREG_LineControl, lcr);

//    DEBUG_IOLog("%s::getDivisor()=%d\n", Name, (int)div);
    return div;
}

#ifdef DEBUG
void Apple16X50UARTSync::showRegs()
{
//    DEBUG_IOLog("%s::showRegs() RX=%02x IER=%02x IIR=%02x LCR=%02x MCR=%02x LSR=%02x MSR=%02x SCR=%02x DIV=%04x\n",
	Name, inb(kREG_Data), inb(kREG_IRQ_Enable), inb(kREG_IRQ_Ident), LCR_Image=inb(kREG_LineControl),
        inb(kREG_ModemControl), inb(kREG_LineStatus), inb(kREG_ModemStatus), inb(kREG_Scratch), (int)getDivisor()
    );
	
	IOLog("Apple16X50UARTSync::showRegs Register Shadows: LCR_Image:(%x) FCR_Image(%x), IER_Mask(%x) \n",LCR_Image,FCR_Image,IER_Mask);
	IOLog("Apple16X50UARTSync::showRegs Register Shadows: Divisor:(%x) MasterClock(%x) \n",Divisor,MasterClock);

}
#endif

void Apple16X50UARTSync::restoreUART()
{
	OffLine = FALSE;
	outb(kREG_IRQ_Enable, IER_Mask);
	outb(kREG_LineControl, LCR_Image);
	outb(kREG_ModemControl, MCR_Image);
	outb(kREG_FIFOControl, FCR_Image);
	setDivisor(Divisor);
}

void Apple16X50UARTSync::saveUART()
{	
	IER_Mask = inb(kREG_IRQ_Enable);
	LCR_Image= inb(kREG_LineControl);
	MCR_Image =inb(kREG_ModemControl);
	FCR_Image =inb(kREG_FIFOControl);
	OffLine = TRUE;
}


// resetUART() - This function sets up various default values for UART
// parameters, then calls programUART().
void Apple16X50UARTSync::resetUART()
{
    if (OffLine) return; // nothing we can do
    DEBUG_IOLog("%s::resetUART()\n", Name);
    DataWidth = 8 bits;
    DataMask = 0xff;
    StopBits = 1 bits;
    Parity = PD_RS232_PARITY_NONE;
    IgnoreParityErrors = false;
    BaudRate = 9600 bits;
    Divisor = 0x0000;
    bzero(SW_Special, 256>>3);
    XOnChar = kXOnChar;
    XOffChar = kXOffChar;
    RXO_State = kXO_Idle;
    FCR_Image = 0x00;
    LCR_Image = 0x00;
    IER_Mask = 0x0f;
    outb(kREG_IRQ_Enable, 0x00);
    outb(kREG_Scratch, 0x00);
    outb(kREG_LineControl, 0x00);
    outb(kREG_IRQ_Enable, 0x00);
    outb(kREG_FIFOControl, kFIFO_ResetRx | kFIFO_ResetTx);
    outb(kREG_ModemControl, 0x00);
    FlowControl = PD_RS232_A_DTR | PD_RS232_A_RFR | PD_RS232_A_CTS | PD_RS232_A_DSR;
    programUART();
    if (RxQ) RxQ->setSize(0);
    if (RxQ) TxQ->setSize(0);
}

/* programUART() - This function programs the UART according to the
 * various instance variables.  This function should be called
 * with the CommandGate closed.
 */
void Apple16X50UARTSync::programUART()
{
    UInt8 lcr=0;
    UInt16 dlr=0;
    UInt32 frameSize=0;
    UInt32 frame;
    int level=0;

    if (OffLine) return; // nothing we can do
    DEBUG_IOLog("%s::programUART()\n", Name);

    DataWidth = (DataWidth+1)&(~1); // round half-bits up
    DataWidth = CONSTRAIN((5 bits), DataWidth, (8 bits));
    switch (DataWidth) {
        case (5 bits) : lcr = kLCR_5bitData;   DataMask = 0x1f;   break;
        case (6 bits) : lcr = kLCR_6bitData;   DataMask = 0x3f;   break;
        case (7 bits) : lcr = kLCR_7bitData;   DataMask = 0x7f;   break;
        case (8 bits) : lcr = kLCR_8bitData;   DataMask = 0xff;   break;
    }
    switch (StopBits) {
        case 3 :	// 1.5 stop bits
        case (2 bits) :	lcr |= kLCR_2bitStop;
                        if (DataWidth == (5 bits))
                            StopBits = 3; // 1.5 stop bits
                        else
                            StopBits = (2 bits);
                        break;

        default :	lcr |= kLCR_1bitStop;
                        StopBits = (1 bits);
                        break;
    }
    switch (Parity) {
        case PD_RS232_PARITY_ODD   : lcr |= kLCR_OddParity;	break;
        case PD_RS232_PARITY_EVEN  : lcr |= kLCR_EvenParity;	break;
        case PD_RS232_PARITY_MARK  : lcr |= kLCR_MarkParity;	break;
        case PD_RS232_PARITY_SPACE : lcr |= kLCR_SpaceParity;	break;
        default			   : lcr |= kLCR_NoParity;	break;
    }
   if (State & PD_RS232_S_BRK)
       lcr |= kLCR_SendBreak;
    BaudRate = CONSTRAIN(kMinBaud, BaudRate, MaxBaud);

        

    // The formula for the clock divisor is: divisor = MasterClock/(16*bps)
    // DataWidth & StopBits are in half-bits, and BaudRate is half-bits per second.
    dlr = (MasterClock) / (BaudRate << 3);
    // Make sure we didn't loose any significant fractional value.
    int err_lo = ((MasterClock) / (dlr << 3)) - BaudRate;
    int err_hi = BaudRate - ((MasterClock) / ((dlr+1) << 3));
    if (err_lo > err_hi) dlr++;

    if (dlr != Divisor) {
        setDivisor(Divisor = dlr);
        frameSize = ( (DataWidth + StopBits) +
                      ((Parity==PD_RS232_PARITY_NONE) ? (1 bits) : (2 bits) ) ); // frame size in half-bits
	frame = ( (1000000 * frameSize) / BaudRate); // frame duration in microseconds
        FrameInterval = frame;
        level = min( ((kTenMS/frame)-3), (FIFO_Size-(kTwoMS/frame)) );
        DEBUG_IOLog("%s::programUART() MasterClock=%dHz frame=%dbits BaudRate=%dbps FrameInterval=%dµs\n",
                    Name, (int)MasterClock, (int)(frameSize>>1), (int)(BaudRate>>1), (int)FrameInterval);
        switch (UART_Type) {
            case kUART_16550C :
            case kUART_16C1550 :
                if (MinLatency)		FCR_Image = kFIFO_Enable | kFIFO_01of16;
                else if (level<4)	FCR_Image = kFIFO_Enable | kFIFO_01of16;
                else if (level<8)	FCR_Image = kFIFO_Enable | kFIFO_04of16;
                else			FCR_Image = kFIFO_Enable | kFIFO_08of16;
                break;
                
            case kUART_16C650 :
                if (MinLatency)		FCR_Image = 0x00;
                else if (level<16)	FCR_Image = kFIFO_Enable | kFIFO_08of32;
                else if (level<24)	FCR_Image = kFIFO_Enable | kFIFO_16of32;
                else			FCR_Image = kFIFO_Enable | kFIFO_24of32;
                break;
                
            case kUART_16C750 :
            case kUART_16C950 :
                if (MinLatency)		FCR_Image = kFIFO_Enable | kFIFO_001of128;
                else if (level<32)	FCR_Image = kFIFO_Enable | kFIFO_001of128;
                else if (level<64)	FCR_Image = kFIFO_Enable | kFIFO_032of128;
                else			FCR_Image = kFIFO_Enable | kFIFO_064of128;
                outb(kREG_FIFOControl, FCR_Image);
                outb(kREG_LineControl, lcr);	// Set DLAB=0
                outb(kREG_LineControl, 0xbf);	// Set DLAB=1
                outb(kREG_FuncControl, 0x10);	// Enhanced mode
                DEBUG_IOLog("%s::programUART() set enhanced mode=0x%02x!\n", Name, inb(kREG_FuncControl));
                outb(kREG_LineControl, lcr);	// Set DLAB=0
                break;
                
            default :
                FCR_Image = 0x00;
                break;
        }
        outb(kREG_FIFOControl, FCR_Image);
        DEBUG_IOLog("%s::programUART() FIFO Target=%d/%d FCR=0x%02x\n", Name, level, (int)FIFO_Size, FCR_Image);
    }
    outb(kREG_LineControl, lcr);
    LCR_Image = lcr;
}

void Apple16X50UARTSync::programMCR(UInt32 state, bool irqen, bool loop)
{
    UInt8 mcr=0;

    if (OffLine) return; // nothing we can do
    if (irqen) mcr |= kMCR_Out2; // interrupt-enable bit
    if (loop)  mcr |= kMCR_Loop; // loop-back bit
    if (state&PD_RS232_S_DTR)  mcr |= kMCR_DTR;
    if (state&PD_RS232_S_RFR)  mcr |= kMCR_RTS;
    
    outb(kREG_ModemControl, mcr);
}

#ifdef REFBUG
void Apple16X50UARTSync::retain() const
{
    super::retain();
    DEBUG_IOLog("%s::retain()==%d\n", Name, getRetainCount());
}

void Apple16X50UARTSync::release() const
{
    DEBUG_IOLog("%s::release()==%d\n",Name, getRetainCount());
    super::release();
}
#endif
