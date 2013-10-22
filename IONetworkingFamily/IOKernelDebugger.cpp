/*
 * Copyright (c) 1998-2008 Apple Inc. All rights reserved.
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
/*
 * IOKernelDebugger.cpp
 *
 * HISTORY
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOLocks.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IOKernelDebugger.h>
#include <IOKit/IOBSD.h> //for kIOBSDNameKey
#include <libkern/OSAtomic.h>
#include "IONetworkControllerPrivate.h"

#define kIOPrimaryDebugPortKey      "IOPrimaryDebugPort"

#define kMatchNameArg				"kdp_match_name"
#define kMatchMacArg				"kdp_match_mac"

//---------------------------------------------------------------------------
// IOKDP

class IOKDP : public IOService
{
    OSDeclareDefaultStructors( IOKDP )

public:
	/*! @function probe
	@abstract verify controller meets criteria for debugger
	@discussion This checks that the controller that is attaching the debugger
	meets the criteria set by the user on the kernel command line. Current boot-args
	are kdp_match_mac=001122334455 to match against a specific MAC address and
	kdp_match_name=bsdname to match against interfaces with the specified BSD interface
	name (for example, en1)
	*/
	
	virtual IOService *probe(IOService *provider, SInt32 *score);
	
    virtual bool start( IOService * provider );

    virtual void stop( IOService * provider );

    virtual IOReturn message( UInt32       type,
                              IOService *  provider,
                              void *       argument = 0 );
};

//---------------------------------------------------------------------------
// IOKDP defined globals.

static IOLock * gIOKDPLock    = 0;
static IOKDP *  gIOKDP        = 0;
static UInt32   gDebugBootArg = 0;

class IOKDPGlobals
{
public:
    IOKDPGlobals();
    ~IOKDPGlobals();
    
    inline bool isValid() const;
};

static IOKDPGlobals gIOKDPGlobals;

IOKDPGlobals::IOKDPGlobals()
{
    gIOKDPLock = IOLockAlloc();

    PE_parse_boot_argn( "debug", &gDebugBootArg, sizeof (gDebugBootArg) );
}

IOKDPGlobals::~IOKDPGlobals()
{
    if ( gIOKDPLock )
    {
        IOLockFree( gIOKDPLock );
        gIOKDPLock = 0;
    }
}

bool IOKDPGlobals::isValid() const
{
    return ( gIOKDPLock );
}

#define super IOService
OSDefineMetaClassAndStructors( IOKDP, IOService )

//---------------------------------------------------------------------------
// start/stop/message.
IOService *IOKDP::probe(IOService *provider, SInt32 *score)
{
	char textBuffer[32];
	
	// we expect our provider is an IOKernelDebugger and that its provider is an IONetworkController.
	// If the controller has an IONetworkInterface client (it should) we can use info in it to
	// determine if this is the best match for a debugger.
	IONetworkInterface *interface = 0;
	OSObject *client;
	IOService *controller = provider->getProvider();
	
//	IOLog("_kdp_ probing...\n");
	if (controller)
	{
		OSIterator *clients = controller->getClientIterator();
		//try to find a network interface on the provider
		while ((client = clients->getNextObject()))
		{
			if ((interface = OSDynamicCast(IONetworkInterface, client)))
				break;
		}
		clients->release();
		
	}
		
	do
	{
		if(PE_parse_boot_argn( kMatchNameArg, textBuffer, sizeof(textBuffer)))
		{
			if(!interface) //user wants name match but we're not on a controller with an interface
			{
				//IOLog("_kdp_ no interface\n");
				return 0;
			}
			OSString *bsdname = OSDynamicCast(OSString, interface->getProperty(kIOBSDNameKey));
			if(!bsdname)
			{
				//IOLog("_kdp_ no bsd property\n");
				return 0;
			}
			if(bsdname->isEqualTo(textBuffer) == false)
			{
				//IOLog("_kdp_ name doesn't match %s\n", textBuffer);
				return 0;
			}
			break; 
		}
		
		if(PE_parse_boot_argn( kMatchMacArg, textBuffer, sizeof(textBuffer)))
		{
			char ctrMac[13];
			if(!controller) //looking for mac match, but the debugger isn't on a controller (!?)
			{
				//IOLog("_kdp_ no controller\n");
				return 0;
			}
			OSData * macAddr = OSDynamicCast(OSData, controller->getProperty(kIOMACAddress));
			if ( (macAddr == 0) || (macAddr->getLength() != 6) )
			{
				//IOLog("_kdp_ bad mac\n");
				return 0;
			}
			
			//make sure command line mac is in upper case
			for(int i=0; i<12; i++)
				textBuffer[i] = textBuffer[i] >= 'a' && textBuffer[i] <= 'z' ? textBuffer[i] - 32 : textBuffer[i];

			// now convert the controller mac property to a string
			unsigned char *macData = (unsigned char *)macAddr->getBytesNoCopy();
			snprintf(ctrMac, sizeof(ctrMac), "%02X%02X%02X%02X%02X%02X", macData[0], macData[1], macData[2], macData[3], macData[4], macData[5]);
			
			//now see if they match...
			if(strncmp(ctrMac, textBuffer, 12))
			{
				//IOLog("_kdp_ mac doesn't match %s\n", textBuffer);
				return 0;
			}
			break;
		}
		
		//else default to old plist metric: the IOKernelDebugger has IOPrimaryDebugPort property
		OSBoolean *pdp = OSDynamicCast(OSBoolean, provider->getProperty(kIOPrimaryDebugPortKey));
		if(!pdp || pdp->isFalse())
		{
			//IOLog("_kdp_ not primary debug port\n");
			return 0;
		}
		break;
	}while(false);
	
	//IOLog("_kdp_ MATCHED!\n");
	
	//make sure the super is ok with this.
	IOService *ret = super::probe(provider, score);
	return ret;
}

bool IOKDP::start( IOService * provider )
{
    bool ret = false;

    if ( super::start(provider) == false )
        return false;

    if ( gIOKDPGlobals.isValid() == false )
        return false;

    IOLockLock( gIOKDPLock );

    do {
        if ( gIOKDP )
            break;

        if ( provider->open(this) == false )
            break;

        gIOKDP = this;
        ret    = true;
    }
    while ( false );

    IOLockUnlock( gIOKDPLock );
    
    if ( ret ) registerService();

    return ret;
}

void IOKDP::stop( IOService * provider )
{
    provider->close(this);

    IOLockLock( gIOKDPLock );

    if ( gIOKDP == this ) gIOKDP = 0;

    IOLockUnlock( gIOKDPLock );

    super::stop(provider);
}

IOReturn IOKDP::message( UInt32       type,
                         IOService *  provider,
                         void *       argument )
{
    if ( type == kIOMessageServiceIsTerminated )
    {
        provider->close(this);
    }
    return kIOReturnSuccess;
}


//---------------------------------------------------------------------------
// IOKernelDebugger

extern "C" {
//
// Defined in osfmk/kdp/kdp_en_debugger.h, but the header file is not
// exported, thus the definition is replicated here.
//
typedef void (*kdp_send_t)( void * pkt, UInt pkt_len );
typedef void (*kdp_receive_t)( void * pkt, UInt * pkt_len, UInt timeout );
typedef UInt32  (*kdp_link_t)(void);
typedef boolean_t (*kdp_mode_t)(boolean_t active);
void kdp_register_send_receive( kdp_send_t send, kdp_receive_t receive );
void kdp_unregister_send_receive( kdp_send_t send, kdp_receive_t receive );
void kdp_register_link( kdp_link_t link, kdp_mode_t mode );
void kdp_unregister_link( kdp_link_t link, kdp_mode_t mode );
void kdp_set_interface(void *, const void *);
}

#undef  super
#define super IOService
OSDefineMetaClassAndStructors( IOKernelDebugger, IOService )
OSMetaClassDefineReservedUnused( IOKernelDebugger, 0 );
OSMetaClassDefineReservedUnused( IOKernelDebugger, 1 );
OSMetaClassDefineReservedUnused( IOKernelDebugger, 2 );
OSMetaClassDefineReservedUnused( IOKernelDebugger, 3 );

// IOKernelDebugger global variables.
//
IOService *             gIODebuggerDevice                = 0;
IODebuggerTxHandler     gIODebuggerTxHandler             = 0;
IODebuggerRxHandler     gIODebuggerRxHandler             = 0;
IODebuggerLinkStatusHandler gIODebuggerLinkStatusHandler = 0;
IODebuggerSetModeHandler gIODebuggerSetModeHandler       = 0;
UInt32                  gIODebuggerTxBytes               = 0;
UInt32                  gIODebuggerRxBytes               = 0;
SInt32                  gIODebuggerSemaphore             = 0;
UInt32                  gIODebuggerFlag                  = 0;
UInt8                   gIODebuggerSignalled             = 0;

// Global debugger flags.
//
enum {
    kIODebuggerFlagRegistered       = 0x01,
    kIODebuggerFlagWarnNullHandler  = 0x02
};

// Expansion variables.
//
#define _state                      _reserved->stateVars[0]
#define _activationChangeThreadCall _reserved->activationChangeThreadCall
#define _interfaceNotifier	    _reserved->interfaceNotifier
#define _linkStatusHandler          _reserved->linkStatusHandler 
#define _setModeHandler             _reserved->setModeHandler 
#define EARLY_DEBUG_SUPPORT         (gDebugBootArg != 0)

static void handleActivationChange( IOKernelDebugger * debugger,
                                    void *             change );

bool IOKernelDebugger::interfacePublished(
    void * target, void *param, IOService * service, IONotifier * notifier )
{
	IOService *debugger = (IOService *)target;
    IONetworkInterface *interface = OSDynamicCast(IONetworkInterface, service);

	//IOLog("new (or changes on) interface detected\n");
	//only reregister ourselves if the interface has the same controller (provider) as we do.
	if(debugger && service && debugger->getProvider() == service->getProvider())
	{
		//IOLog("it's on our controller- reregister\n");
		debugger->registerService();
        if (interface)
            interface->debuggerRegistered();
	}
	return true;
}

//---------------------------------------------------------------------------
// The KDP receive dispatch function. Dispatches KDP receive requests to the
// registered receive handler. This function is registered with KDP via 
// kdp_register_send_receive().

void IOKernelDebugger::kdpReceiveDispatcher( void *   buffer,
                                             UInt32 * length, 
                                             UInt32   timeout )
{
    *length = 0;    // return a zero length field by default.

    if ( gIODebuggerSemaphore ) return;  // FIXME - Driver is busy!

    (*gIODebuggerRxHandler)( gIODebuggerDevice, buffer, length, timeout );

    gIODebuggerRxBytes += *length;
}

//---------------------------------------------------------------------------
// The KDP transmit dispatch function. Dispatches KDP transmit requests to the
// registered transmit handler. This function is registered with KDP via 
// kdp_register_send_receive().

void IOKernelDebugger::kdpTransmitDispatcher( void * buffer, UInt32 length )
{
    if ( gIODebuggerSemaphore ) return;  // FIXME - Driver is busy!

    (*gIODebuggerTxHandler)( gIODebuggerDevice, buffer, length );

    gIODebuggerTxBytes += length;
}

//---------------------------------------------------------------------------
// The KDP link up dispatch function. Dispatches KDP link up queries to the
// registered link up handler. This function is registered with KDP via 
// kdp_register_link().

UInt32 IOKernelDebugger::kdpLinkStatusDispatcher( void )
{
    if ( gIODebuggerSemaphore ) 
        return 0;  // FIXME - Driver is busy!

    return (*gIODebuggerLinkStatusHandler)( gIODebuggerDevice);
}


//---------------------------------------------------------------------------
// The KDP set mode dispatch function. Dispatches KDP set mode commands to the
// registered setMode handler. This function is registered with KDP via 
// kdp_register_link().

boolean_t IOKernelDebugger::kdpSetModeDispatcher(boolean_t active)
{
   if ( gIODebuggerSemaphore ) 
        return FALSE;  // FIXME - Driver is busy!

   if ((*gIODebuggerSetModeHandler)( gIODebuggerDevice, active == TRUE ? true : false) == true)
        return TRUE;

   return FALSE;
}


//---------------------------------------------------------------------------
// Null debugger handlers.

void IOKernelDebugger::nullTxHandler( IOService * target,
                                      void *      buffer,
                                      UInt32      length )
{
}

void IOKernelDebugger::nullRxHandler( IOService * target,
                                      void *      buffer,
                                      UInt32 *    length,
                                      UInt32      timeout )
{
    if ( gIODebuggerFlag & kIODebuggerFlagWarnNullHandler )
    {
        IOLog("IOKernelDebugger::%s no debugger device\n", __FUNCTION__);
        gIODebuggerFlag &= ~kIODebuggerFlagWarnNullHandler;
    }
}

UInt32 IOKernelDebugger::nullLinkStatusHandler(__unused IOService * target)
{
    return kIONetworkLinkValid | kIONetworkLinkActive; /* default is link up. */
}

bool IOKernelDebugger::nullSetModeHandler(__unused IOService * target,
                                          __unused bool active)
{
    return true; 
}



//---------------------------------------------------------------------------
// Take the debugger lock conditionally.

IODebuggerLockState IOKernelDebugger::lock( IOService * object )
{
    if ( gIODebuggerDevice == object )
    {
        OSIncrementAtomic( &gIODebuggerSemaphore );
        return kIODebuggerLockTaken;
    }
    return (IODebuggerLockState) 0;
}

inline static void invokeDebugger(void)
{
    if ( gIODebuggerSemaphore ) 
            return;

    if (OSTestAndClear(0, &gIODebuggerSignalled) == false) {
        Debugger("remote NMI");
    }
}

//---------------------------------------------------------------------------
// Release the debugger lock if the kIODebuggerLockTaken flag is set.

void IOKernelDebugger::unlock( IODebuggerLockState state )
{
    if ( state & kIODebuggerLockTaken ) {
        OSDecrementAtomic( &gIODebuggerSemaphore );

        invokeDebugger();
    }
}

//---------------------------------------------------------------------------
// drop into the Debugger when safe
void IOKernelDebugger::signalDebugger(void)
{
    OSTestAndSet(0, &gIODebuggerSignalled);
    invokeDebugger();
}

//---------------------------------------------------------------------------
// Initialize an IOKernelDebugger instance.

bool IOKernelDebugger::init( IOService *                 target,
                             IODebuggerTxHandler         txHandler,
                             IODebuggerRxHandler         rxHandler,
                             IODebuggerLinkStatusHandler linkStatusHandler,
                             IODebuggerSetModeHandler    setModeHandler)
{
    if ( ( super::init() == false )                ||
         ( OSDynamicCast(IOService, target) == 0 ) ||
         ( txHandler == 0 )                        ||
         ( rxHandler == 0 ) )
    {
        return false;
    }

    // Allocate memory for the ExpansionData structure.

    _reserved = IONew( ExpansionData, 1 );
    if ( _reserved == 0 )
    {
        return false;
    }

    _activationChangeThreadCall = thread_call_allocate( 
                                (thread_call_func_t)  handleActivationChange,
                                (thread_call_param_t) this );

	// if user wants bsd name matching, odds are good that the interface won't
	// have been named at the time IOKDP is matching on us...fortunately IONetworkStack
	// reregisters the interface service when it gets named, so we can add a notifier
	// and reregister ourselves at that time in order to try matching IOKDP again.
	char textBuffer[64];
	if (PE_parse_boot_argn( kMatchNameArg, textBuffer, sizeof(textBuffer)))
	{
        OSDictionary * matching = serviceMatching(kIONetworkInterfaceClass);
        if (matching)
        {
            _interfaceNotifier = addMatchingNotification(
									   /* type   */    gIOPublishNotification,
									   /* match  */    matching,
									   /* action */    interfacePublished,
									   /* param  */    this );
            matching->release();
        }
    }
	else _interfaceNotifier = 0;

    if ( !_activationChangeThreadCall )
    {
        return false;
    }

    // Cache the target and handlers provided.

    _target            = target;
    _txHandler         = txHandler;
    _rxHandler         = rxHandler;
    _linkStatusHandler = linkStatusHandler;
    _setModeHandler    = setModeHandler;
    _state      = 0;

    return true;
}

//---------------------------------------------------------------------------
// Factory method which performs allocation and initialization of an 
// IOKernelDebugger instance.

IOKernelDebugger * IOKernelDebugger::debugger( IOService *                 target,
                                               IODebuggerTxHandler         txHandler,
                                               IODebuggerRxHandler         rxHandler,
                                               IODebuggerLinkStatusHandler linkStatusHandler,
                                               IODebuggerSetModeHandler    setModeHandler)
{
    IOKernelDebugger * debugger = new IOKernelDebugger;
	
    if (debugger && (debugger->init( target, txHandler, rxHandler, linkStatusHandler, setModeHandler ) == false))
    {
        debugger->release();
        return 0;
    }

	// determine if this debugger is the "primary" debugger- the one KDP will attach to unless told otherwise
    IOService * device = target->getProvider(); //get info from our provider's provider
    //the debugger is primary if...
	if ( device && device->getProperty( "built-in" )) // ...it is a built in controller...
	{
		OSObject *locationProperty = device->copyProperty("location");
		OSData *locationAsData = OSDynamicCast(OSData, locationProperty);
		if(!locationAsData || // ...AND we don't know anything else about its location.....
		   (strcmp( (char *)locationAsData->getBytesNoCopy(), "1")== 0) ) // ...OR we do know its location and it is 'slot' 1
			debugger->setProperty( kIOPrimaryDebugPortKey, true );
		else
			debugger->setProperty( kIOPrimaryDebugPortKey, false );
		if(locationProperty)
			locationProperty->release();
	}
    return debugger;
}

//---------------------------------------------------------------------------
// Register the debugger handlers.

void IOKernelDebugger::registerHandler( IOService *                 target,
                                        IODebuggerTxHandler         txHandler,
                                        IODebuggerRxHandler         rxHandler,
                                        IODebuggerLinkStatusHandler linkStatusHandler,
                                        IODebuggerSetModeHandler    setModeHandler)
{
    bool doRegister;

    assert( ( target == gIODebuggerDevice ) ||
            ( target == 0 )                 ||
            ( gIODebuggerDevice == 0 ) );

    doRegister = ( target && ( txHandler != 0 ) && ( rxHandler != 0 ) );

    if ( !doRegister && ( gIODebuggerFlag & kIODebuggerFlagRegistered ) )
    {
        // Unregister the polling functions from KDP.
        kdp_unregister_send_receive( (kdp_send_t) kdpTransmitDispatcher,
                                     (kdp_receive_t) kdpReceiveDispatcher);
        kdp_unregister_link( (kdp_link_t) kdpLinkStatusDispatcher, (kdp_mode_t) kdpSetModeDispatcher);

        gIODebuggerFlag &= ~kIODebuggerFlagRegistered;
    }

    if ( txHandler == 0 ) txHandler = &IOKernelDebugger::nullTxHandler;
    if ( rxHandler == 0 ) rxHandler = &IOKernelDebugger::nullRxHandler;    
    if ( linkStatusHandler == 0 ) linkStatusHandler = &IOKernelDebugger::nullLinkStatusHandler;    
    if ( setModeHandler == 0 ) setModeHandler = &IOKernelDebugger::nullSetModeHandler;    

    OSIncrementAtomic( &gIODebuggerSemaphore );

    gIODebuggerDevice            = target;
    gIODebuggerTxHandler         = txHandler;
    gIODebuggerRxHandler         = rxHandler;
    gIODebuggerLinkStatusHandler = linkStatusHandler;
    gIODebuggerSetModeHandler    = setModeHandler;
    gIODebuggerFlag     |= kIODebuggerFlagWarnNullHandler;

    OSDecrementAtomic( &gIODebuggerSemaphore );

    if ( doRegister && (( gIODebuggerFlag & kIODebuggerFlagRegistered ) == 0) )
    {
        // if we're on a controller that has an interfacet(we should be), we can register
		// the IONetworkInterface * as an identifier so that the bsd portion
		// knows when to assign an ip to kdp
		IONetworkInterface *interface = 0;
		OSIterator *clients = target->getClientIterator();
		//try to find a network interface on the target
		while (OSObject *client = clients->getNextObject())
		{
			if ((interface = OSDynamicCast(IONetworkInterface, client)))
				break;
		}
		clients->release();
		
                if (interface) {
                    OSData *mac = OSDynamicCast(OSData,
                                                interface->getController()->getProperty(kIOMACAddress));
                    
                    kdp_set_interface(interface, mac ? mac->getBytesNoCopy() : NULL); 
                    interface->debuggerRegistered();
                }

        // Register dispatch function, these in turn will call the
        // handlers when the debugger is active.
        // 
        // Note: The following call may trigger an immediate break
        //       to the debugger.
        kdp_register_link((kdp_link_t) kdpLinkStatusDispatcher, (kdp_mode_t) kdpSetModeDispatcher);
        kdp_register_send_receive( (kdp_send_t) kdpTransmitDispatcher,
                                   (kdp_receive_t) kdpReceiveDispatcher);

        // Limit ourself to a single real KDP registration.

        gIODebuggerFlag |= kIODebuggerFlagRegistered;
        
        publishResource("kdp");
    }
}

//---------------------------------------------------------------------------
// enableTarget / disableTarget

enum {
    kEventClientOpen     = 0x0001,
    kEventTargetUsable   = 0x0002,
    kEventDebuggerActive = 0x0004,
    kTargetIsEnabled     = 0x0100,
    kTargetWasEnabled    = 0x0200,
};

static void enableTarget( IOKernelDebugger * self,
                          IOService *        target,
                          UInt32 *           state,
                          UInt32             event )
{
    IONetworkController * ctr = OSDynamicCast( IONetworkController, target );

    #define kReadyMask ( kEventClientOpen     | \
                         kEventTargetUsable   | \
                         kEventDebuggerActive )

    *state |= event;
    *state &= ~kTargetWasEnabled;

    if ( ( *state & kReadyMask ) == kReadyMask )
    {
        if ( !ctr || ( *state & kTargetIsEnabled ) ||
             ctr->doEnable( self ) == kIOReturnSuccess )
        {
            if ( ( *state & kTargetIsEnabled ) == 0 )
                *state |= kTargetWasEnabled;
            *state |= kTargetIsEnabled;
        }
    }
}

static void disableTarget( IOKernelDebugger * self,
                           IOService *        target,
                           UInt32 *           state,
                           UInt32             event )
{
    IONetworkController * ctr = OSDynamicCast( IONetworkController, target );
    UInt32                on  = *state & kTargetIsEnabled;

    *state &= ~( event | kTargetIsEnabled | kTargetWasEnabled );

    if ( ctr && on ) ctr->doDisable( self );
}

//---------------------------------------------------------------------------
// Called by open() with the arbitration lock held.

bool IOKernelDebugger::handleOpen( IOService *    forClient,
                                   IOOptionBits   options,
                                   void *         arg )
{
    bool ret = false;

    do {
        // Only a single client at a time.

        if ( _client || !_target ) break;

        // Register the target to prime the lock()/unlock() functionality
        // before opening the target.

        registerHandler( _target );

        // While the target is opened/enabled, it must block any thread
        // which may acquire the debugger lock in its execution path.

        if ( _target->open( this ) == false )
            break;

        // For early debugging, the debugger must become active and enable
        // the controller as early as possible. Otherwise, the debugger is
        // not activated until the controller is enabled by BSD.

        if ( EARLY_DEBUG_SUPPORT )
            _state |= kEventDebuggerActive;

        // Register interest in receiving notifications about controller
        // power state changes. The controller is usable if it is power
        // managed, and is in an usable state, or if the controller is
        // not power managed.

        if ( _target->registerInterestedDriver(this) &
             ( kIOPMDeviceUsable | kIOPMNotPowerManaged ) )
            _state |= kEventTargetUsable;

        // Enable the target if possible.

        enableTarget( this, _target, &_state, kEventClientOpen );
        if ( _state & kTargetWasEnabled )
        {
            // If the target was enabled, complete the registration.
            IOLog("%s: registering debugger\n", getName());
            registerHandler( _target, _txHandler, _rxHandler, _linkStatusHandler, _setModeHandler );
        }

        // Remember the client.

        _client = forClient;

        ret = true;
    }
    while (0);

    if ( ret == false )
    {
        registerHandler( 0 );
        _target->close( this );
    }

    return ret;
}

//---------------------------------------------------------------------------
// Called by IOService::close() with the arbitration lock held.

void IOKernelDebugger::handleClose( IOService *   forClient,
                                    IOOptionBits  options )
{
    if ( _target && _client && ( _client == forClient ) )
    {
        // There is no KDP un-registration. The best we can do is to
        // register dummy handlers.

        registerHandler( 0 );

        disableTarget( this, _target, &_state, kEventClientOpen );

        // Before closing the controller, remove interest in receiving
        // notifications about controller power state changes.

        _target->deRegisterInterestedDriver( this );

        _client = 0;

        _target->close( this );
    }
}

//---------------------------------------------------------------------------
// Called by IOService::isOpen() with the arbitration lock held.

bool IOKernelDebugger::handleIsOpen( const IOService * forClient ) const
{
    if ( forClient == 0 )
        return ( forClient != _client );
    else
        return ( forClient == _client );
}

//---------------------------------------------------------------------------
// Free the IOKernelDebugger object.

void IOKernelDebugger::free()
{
    if ( _reserved )
    {
        if ( _activationChangeThreadCall )
            thread_call_free( _activationChangeThreadCall );

		if ( _interfaceNotifier )
			_interfaceNotifier->remove();

        IODelete( _reserved, ExpansionData, 1 );
        _reserved = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
// Handle controller's power state change notitifications.

IOReturn
IOKernelDebugger::powerStateWillChangeTo( IOPMPowerFlags  flags,
                                          unsigned long   stateNumber,
                                          IOService *     policyMaker )
{
    if ((flags & IOPMDeviceUsable) == 0)
    {
        // Controller is about to transition to an un-usable state.
        // The debugger nub should be disabled.

        lockForArbitration();

        // Keep an open on the controller, but inhibit access to the
        // controller's debugger handlers, and disable controller's
        // hardware support for the debugger.

        if ( _client ) registerHandler( 0 );
        disableTarget( this, _target, &_state, kEventTargetUsable );

        unlockForArbitration();
    }

    return IOPMAckImplied;
}

IOReturn
IOKernelDebugger::powerStateDidChangeTo( IOPMPowerFlags  flags,
                                         unsigned long   stateNumber,
                                         IOService *     policyMaker )
{
    if ( flags & IOPMDeviceUsable )
    {
        // Controller has transitioned to an usable state.
        // The debugger nub should be enabled if necessary.
        
        lockForArbitration();

        enableTarget( this, _target, &_state, kEventTargetUsable );
        if ( _state & kTargetWasEnabled )
            registerHandler( _target, _txHandler, _rxHandler, _linkStatusHandler, _setModeHandler );

        unlockForArbitration();
    }

    return IOPMAckImplied;
}

//---------------------------------------------------------------------------
// handleActivationChange

static void
handleActivationChange( IOKernelDebugger * debugger, void * change )
{
    debugger->message( kMessageDebuggerActivationChange, 0, change );
    debugger->release();
}

//---------------------------------------------------------------------------
// message()

IOReturn IOKernelDebugger::message( UInt32 type, IOService * provider,
                                    void * argument )
{
    IOReturn ret = kIOReturnSuccess;

	switch ( type )
    {
        case kMessageControllerWasEnabledForBSD:
        case kMessageControllerWasDisabledForBSD:
            // For early debugging support, these messages are ignored.
            // The debugger will enable the controller independently
            // from BSD.

            if ( EARLY_DEBUG_SUPPORT ) break;

            // Process this change in another thread to avoid deadlocks
            // between the controller's work loop, and our arbitration
            // lock.

            retain();
            if ( thread_call_enter1( _activationChangeThreadCall,
                                     (void *)(uintptr_t) type ) == TRUE )
                release();

            break;

        case kMessageControllerWillShutdown:
            // Disable controller before system shutdown and restart.
            // Intentional fall-through to next case.

        case kMessageDebuggerActivationChange:
            lockForArbitration();

            // If controller was enabled, activate the debugger.
            // Otherwise make the debugger inactive.

            if ( (void *) kMessageControllerWasEnabledForBSD == argument )
            {
                enableTarget( this, _target, &_state, kEventDebuggerActive );
                if ( _state & kTargetWasEnabled )
                    registerHandler( _target, _txHandler, _rxHandler, _linkStatusHandler, _setModeHandler );
            }
            else
            {
                if ( _client ) registerHandler( 0 );
                disableTarget( this, _target, &_state, kEventDebuggerActive );
            }

            unlockForArbitration();
            break;

        default:
            ret = super::message( type, provider, argument );
    }

    return ret;
}
