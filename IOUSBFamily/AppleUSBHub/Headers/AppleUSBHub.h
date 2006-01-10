/*
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <kern/thread_call.h>

enum{
      kErrataCaptiveOKBit = 1,
      kStartupDelayBit = 2,
};

class IOUSBController;
class IOUSBDevice;
class IOUSBInterface;
class IOUSBPipe;
class AppleUSBHubPort;


class AppleUSBHub : public IOService
{
    OSDeclareDefaultStructors(AppleUSBHub)

    friend class AppleUSBHubPort;
    friend class AppleUSBHSHubUserClient;

    IOUSBController *		_bus;
    IOUSBDevice *		_device;
    IOUSBInterface *		_hubInterface;
    IOUSBConfigurationDescriptor *_configDescriptor;
    IOUSBHubDescriptor		_hubDescriptor;
    USBDeviceAddress   		_address;
    IOUSBHubPortStatus       	_hubStatus;
    IOUSBPipe * 		_interruptPipe;
    IOBufferMemoryDescriptor *	_buffer;
    IOCommandGate *		_gate;
    IOWorkLoop *		_workLoop;
    UInt32			_locationID;
    UInt32			_inStartMethod;
    bool			_portSuspended;
    bool			_hubHasBeenDisconnected;
    bool			_hubIsDead;
    
    // Power stuff
    bool			_busPowered;
    bool			_selfPowered;
    bool			_busPowerGood;
    bool       			_selfPowerGood;
    bool			_needToClose;
    
    UInt32			_powerForCaptive;
    thread_call_t		_workThread;
    thread_call_t		_resetPortZeroThread;
    thread_call_t		_hubDeadCheckThread;
    thread_call_t		_clearFeatureEndpointHaltThread;

    // Port stuff
    UInt8			_readBytes;
    UInt8			_numCaptive;
    AppleUSBHubPort **	   	_ports;		// Allocated at runtime
    bool			_multiTTs;	// Hub is multiTT capable, and configured.
    bool			_hsHub;		// our provider is a HS bus
    bool			_isRootHub;	// we are driving a root hub (needed for test mode)
    bool			_inTestMode;	// T while we are in test mode
    IOTimerEventSource *      	_timerSource;
    UInt32			_timeoutFlag;
    UInt32			_portTimeStamp[32];
    UInt32			_portWithDevZeroLock;
    UInt32			_outstandingIO;

    // Errata stuff
    UInt32			_errataBits;
    UInt32			_startupDelay;
    
    static void 	InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining);
    void 		InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining);
    
    static void 	ProcessStatusChangedEntry(OSObject *target);
    void 		ProcessStatusChanged(void);

    static void		ResetPortZeroEntry(OSObject *target);
    void		ResetPortZero();
    
    static void 	CheckForDeadHubEntry(OSObject *target);
    void		CheckForDeadHub();

    static void		ClearFeatureEndpointHaltEntry(OSObject *target);
    void		ClearFeatureEndpointHalt(void);

    static void 	TimeoutOccurred(OSObject *owner, IOTimerEventSource *sender);

    IOReturn 		DoDeviceRequest(IOUSBDevRequest *request);
    UInt32		GetHubErrataBits(void);

    void		DecrementOutstandingIO(void);
    void		IncrementOutstandingIO(void);
    static IOReturn	ChangeOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);

    // Hub functions
    void		UnpackPortFlags(void);
    void 		CountCaptivePorts(void);
    IOReturn		CheckPortPowerRequirements(void);
    IOReturn		AllocatePortMemory(void);
    IOReturn		StartPorts(void);
    IOReturn 		StopPorts(void);
    IOReturn		ConfigureHub(void);

    bool		HubStatusChanged(void);

    IOReturn		GetHubDescriptor(IOUSBHubDescriptor *desc);
    IOReturn		GetHubStatus(IOUSBHubStatus *status);
    IOReturn		ClearHubFeature(UInt16 feature);

    IOReturn		GetPortStatus(IOUSBHubPortStatus *status, UInt16 port);
    IOReturn		GetPortState(UInt8 *state, UInt16 port);
    IOReturn		SetPortFeature(UInt16 feature, UInt16 port);
    IOReturn		ClearPortFeature(UInt16 feature, UInt16 port);

    void 		PrintHubDescriptor(IOUSBHubDescriptor *desc);

    void 		FatalError(IOReturn err, char *str);
    IOReturn		DoPortAction(UInt32 type, UInt32 portNumber, UInt32 options );
    void		StartWatchdogTimer();
    void		StopWatchdogTimer();
    IOReturn		RearmInterruptRead();
    void		ResetMyPort();
    void		CallCheckForDeadHub(void);

    IOUSBHubDescriptor 	GetCachedHubDescriptor() { return _hubDescriptor; }
    bool		MergeDictionaryIntoProvider(IOService *  provider, OSDictionary *  mergeDict);
    bool		MergeDictionaryIntoDictionary(OSDictionary *  sourceDictionary,  OSDictionary *  targetDictionary);
    
    // test mode functions, called by the AppleUSBHSHubUserClient
    IOReturn		EnterTestMode();
    IOReturn		LeaveTestMode();
    bool		IsHSRootHub();
    IOReturn 		PutPortIntoTestMode(UInt32 port, UInt32 mode);
    
public:

    virtual bool	init(OSDictionary * propTable );
    virtual bool	start(IOService * provider);
    virtual void 	stop(IOService *  provider);
    virtual bool 	finalize(IOOptionBits options);
    virtual IOReturn 	message( UInt32 type, IOService * provider,  void * argument = 0 );

    // "new" IOKit methods. Some of these may go away before we ship 1.8.5
    virtual bool 	willTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	didTerminate( IOService * provider, IOOptionBits options, bool * defer );
    virtual bool 	requestTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	terminate( IOOptionBits options = 0 );
    virtual void 	free( void );
    virtual bool 	terminateClient( IOService * client, IOOptionBits options );

    virtual IOUSBDevice * GetDevice(void) { return _device; }

};


#endif _IOKIT_APPLEUSBHUB_H
