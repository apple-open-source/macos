/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright © 1998-2010 Apple Inc.  All rights reserved.
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

#ifndef _IOKIT_APPLEUSBHUB_H
#define _IOKIT_APPLEUSBHUB_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBHub.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>

#include <kern/thread_call.h>

/* Convert USBLog to use kprintf debugging */
#ifndef APPLEUSBHUB_USE_KPRINTF
	#define APPLEUSBHUB_USE_KPRINTF 0
#endif

#if APPLEUSBHUB_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= APPLEUSBHUB_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif


enum 
{
	kHubPortPowerOff = 0,
	kHubPortPowerOn = 1
};

enum
{
      kErrataCaptiveOKBit = 1,
      kStartupDelayBit = 2,
	  kDisplayOverCurrentTimeout = 30			// # of seconds that need to pass before we will show an overcurrent dialog again
};


class IOUSBController;
class IOUSBDevice;
class IOUSBInterface;
class IOUSBPipe;
class AppleUSBHubPort;


class AppleUSBHub : public IOUSBHubPolicyMaker
{
    OSDeclareDefaultStructors(AppleUSBHub)

    friend class AppleUSBHubPort;
    friend class AppleUSBHSHubUserClient;

    IOUSBInterface *					_hubInterface;
    IOUSBConfigurationDescriptor		*_configDescriptor;
    IOUSBHubDescriptor					_hubDescriptor;
    USBDeviceAddress					_address;
    IOUSBHubPortStatus					_hubStatus;
    IOUSBPipe *							_interruptPipe;
    IOBufferMemoryDescriptor *			_buffer;
    IOCommandGate *						_gate;
    IOWorkLoop *						_workLoop;
    UInt32								_locationID;
    UInt32								_inStartMethod;
	UInt32								_devZeroLockedTimeoutCounter;					// We use this to count down to see when we need to check for a possible stuck dev zero lock
    bool								_portSuspended;
    bool								_hubHasBeenDisconnected;
    bool								_hubIsDead;
	bool								_abortExpected;
	UInt32								_retryCount;
    IOUSBHubDevice *					_hubParent;										// the hub to which our provider hub is attached (NULL for root hubs)
    
    // Power stuff
    bool								_busPowered;
    bool								_selfPowered;
    bool								_busPowerGood;
    bool								_selfPowerGood;
		
	// bookkeeping
    bool								_needToClose;
	bool								_okToCloseWhileOff;						// T if we can close from the OFF power state
    
	bool								_needInterruptRead;						// T if we need a new interrupt read on either a power change or on the last I/O
	bool								_needToCallResetDevice;
	UInt32								_interruptReadPending;					// 1 if there is ab outstanding read on the interrupt pipe, 0 if there is not
    
    UInt32								_powerForCaptive;
    thread_call_t						_workThread;
    thread_call_t						_resetPortZeroThread;
    thread_call_t						_hubDeadCheckThread;
    thread_call_t						_clearFeatureEndpointHaltThread;
	thread_call_t						_checkForActivePortsThread;
	thread_call_t						_waitForPortResumesThread;
	thread_call_t						_ensureUsabilityThread;
	thread_call_t						_initialDelayThread;
	thread_call_t						_hubResetPortThread;

    // Port stuff
    UInt8								_readBytes;
    UInt8								_numCaptive;
    AppleUSBHubPort **					_ports;									// Allocated at runtime
    bool								_multiTTs;								// Hub is multiTT capable, and configured.
    bool								_hsHub;									// our provider is a HS bus
	bool								_needToAckSetPowerState;
	bool								_checkPortsThreadActive;
	bool								_abandonCheckPorts;						// T if we should abandon the check ports thread
	bool								_doPortActionLock;						// Lock to synchronize accesses to any "PortAction" (supend/reenumerate)
	bool								_waitingForPowerOn;						// T if we are in a commandSleep waiting for a power change to ON
    IOTimerEventSource *				_timerSource;
    UInt32								_timeoutFlag;
    UInt32								_portTimeStamp[32];
    UInt32								_portWithDevZeroLock;
    UInt32								_outstandingIO;
	UInt32								_raisedPowerStateCount;					// to keep track of when ports want our power state raised
	UInt32								_outstandingResumes;
	UInt32								_hubGetConfigResetRetries;				

    // Errata stuff
    UInt32								_errataBits;
    UInt32								_startupDelay;
	AbsoluteTime						_wakeupTime;
	bool								_ignoreDisconnectOnWakeup;
	bool								_retryBogusPortStatus;
	bool								_overCurrentNoticeDisplayed;
	AbsoluteTime						_overCurrentNoticeTimeStamp;
	bool								_hubWithExpressCardPort;				// T if this hub has a port that connects to an expresscard slot
	int									_expressCardPort;						// Port # of the hub that connects to the express card slot
	bool								_hasExtraPowerRequest;
	
	bool								_hubDeadCheckLock;
	
	
    static void 	InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining);
    void			InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining);
    
    static void 	ProcessStatusChangedEntry(OSObject *target);
    void			ProcessStatusChanged(void);

    static void			ResetPortZeroEntry(OSObject *target);
    void				ResetPortZero();
    
    static void			CheckForDeadHubEntry(OSObject *target);
    void				CheckForDeadHub();

    static void			ClearFeatureEndpointHaltEntry(OSObject *target);
    void				ClearFeatureEndpointHalt(void);

    static void			CheckForActivePortsEntry(OSObject *target);
    void				CheckForActivePorts(void);
	
    static void			WaitForPortResumesEntry(OSObject *target);
    void				WaitForPortResumes(void);
	
    static void			TimeoutOccurred(OSObject *owner, IOTimerEventSource *sender);

    IOReturn			DoDeviceRequest(IOUSBDevRequest *request);
    IOReturn			DoDeviceRequestWithRetries(IOUSBDevRequest *request, bool retrySTALLs = false);
    UInt32				GetHubErrataBits(void);

    void				DecrementOutstandingIO(void);
    void				IncrementOutstandingIO(void);
    static IOReturn		ChangeOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
    void				LowerPowerState(void);
    void				RaisePowerState(void);
    static IOReturn		ChangeRaisedPowerState(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
    void				IncrementOutstandingResumes(void);
    void				DecrementOutstandingResumes(void);
    static IOReturn		ChangeOutstandingResumes(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
    IOReturn			TakeDoPortActionLock(void);
    IOReturn			ReleaseDoPortActionLock(void);
    static IOReturn		ChangeDoPortActionLock(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
	
    // Hub functions
    void			UnpackPortFlags(void);
    void			CountCaptivePorts(void);
    IOReturn		CheckPortPowerRequirements(void);
    IOReturn		AllocatePortMemory(void);
    IOReturn		StartPorts(void);
    IOReturn		SuspendPorts(void);
    IOReturn 		StopPorts(void);
    IOReturn		ConfigureHub(void);

    bool			HubStatusChanged(void);

    IOReturn		GetHubDescriptor(IOUSBHubDescriptor *desc);
    IOReturn		GetHubStatus(IOUSBHubStatus *status);
    IOReturn		ClearHubFeature(UInt16 feature);

    IOReturn		GetPortStatus(IOUSBHubPortStatus *status, UInt16 port);
    IOReturn		GetPortState(UInt8 *state, UInt16 port);
    IOReturn		SetPortFeature(UInt16 feature, UInt16 port);
    IOReturn		ClearPortFeature(UInt16 feature, UInt16 port);
    IOReturn		GetDeviceStatus(USBStatus *status);

    void			PrintHubDescriptor(IOUSBHubDescriptor *desc);

    void			FatalError(IOReturn err, const char *str);
    IOReturn		DoPortAction(UInt32 type, UInt32 portNumber, UInt32 options );
    void			StartWatchdogTimer();
    void			StopWatchdogTimer();
    IOReturn		RearmInterruptRead();
    void			ResetMyPort();
    void			CallCheckForDeadHub(void);

    IOUSBHubDescriptor 	GetCachedHubDescriptor() { return _hubDescriptor; }
	bool				HubAreAllPortsDisconnectedOrSuspended();
    bool				IsPortInitThreadActiveForAnyPort();
    bool				IsStatusChangedThreadActiveForAnyPort();
    bool				IsHSRootHub();
	bool				HasExpressCardPort();
	
	
	// local function for an initial delay
	void				InitialDelay(void);
	
	// local function for an resettting our port
	void				HubResetPortAfterPowerChangeDone(void);
	
	// waiting for power change
	IOReturn			WaitForPowerOn( uint64_t timeout );
	void				WakeOnPowerOn( );

	
	static const char *	HubMessageToString(UInt32 message);

	bool				ValidateHubDevice();
	bool				HasInternalDevice(UInt32 portnum);
	
	
public:

    //  IOKit methods
    virtual bool			init(OSDictionary * propTable );
    virtual bool			start(IOService * provider);
    virtual void			stop(IOService *  provider);
    virtual bool			finalize(IOOptionBits options);
    virtual IOReturn		message( UInt32 type, IOService * provider,  void * argument = 0 );
	virtual IOReturn		powerStateWillChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice);
	virtual IOReturn		setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice );
	virtual IOReturn		powerStateDidChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice);
	virtual void			powerChangeDone ( unsigned long fromState);

    virtual bool			willTerminate( IOService * provider, IOOptionBits options );
    virtual bool			didTerminate( IOService * provider, IOOptionBits options, bool * defer );
    virtual bool			requestTerminate( IOService * provider, IOOptionBits options );
    virtual bool			terminate( IOOptionBits options = 0 );
    virtual void			free( void );
    virtual bool			terminateClient( IOService * client, IOOptionBits options );
	
	// IOUSBHubPolicyMaker methods
	virtual bool			ConfigureHubDriver(void);
	virtual IOReturn		HubPowerChange(unsigned long powerStateOrdinal);
	virtual IOReturn		EnsureUsability(void);
	virtual	IOReturn		GetPortInformation(UInt32 portNum, UInt32 *info);
	virtual	IOReturn		ResetPort(UInt32 portNum);
	virtual	IOReturn		SuspendPort(UInt32 portNum, bool suspend);
	virtual	IOReturn		ReEnumeratePort(UInt32 portNum, UInt32 options);
	
	// static entry for EnsureUsability to make sure we are outside of the gate when we do it
    static void				EnsureUsabilityEntry(OSObject *target);
	
	// static entry for InitialDelay to not Lower the Hub Power State until some time has passed (5 seconds)
    static void				InitialDelayEntry(OSObject *target);
	
	// static entry for HubResetPortAfterPowerChangeDoneEntry to issue a ResetDevice() on another thread other than powerChangeDone
    static void				HubResetPortAfterPowerChangeDoneEntry(OSObject *target);
	
	// inline method
    IOUSBDevice * GetDevice(void) { return _device; }

};


#endif _IOKIT_APPLEUSBHUB_H
