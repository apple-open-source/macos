#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/platform/AppleMacIO.h>

extern "C" {
#include <pexpert/pexpert.h>
}

#include "AppleMediaBay.h"
#include "AppleMediaBayATANub.h"


// Define the SuperClass
#define super IOService

OSDefineMetaClassAndStructors(AppleMediaBay, IOService)

bool AppleMediaBay::init(OSDictionary *dict)
{
    bool res = super::init(dict);

    return res;
}

void AppleMediaBay::free(void)
{
    PMstop();
    
    if (commandGate)
        commandGate->release();
        
    if(intSource)
        intSource->release();
        
    if(workloop)
        workloop->release();

    if (configAddrMap)
        configAddrMap->release();
    
    super::free();
}

bool AppleMediaBay::start(IOService *provider)
{
    OSData      *compatibleEntry;

    // If the super class failed there is little point in 
    // going on:
    if (!super::start(provider))
        return false;
    
    // Find out the controller for the mediabay:
    mbControllerType = kMBControllerUndefined;
        
    compatibleEntry  = OSDynamicCast( OSData, provider->getProperty( "compatible" ) );
    if ( compatibleEntry == 0 ) {
#ifdef APPLEMB_VERBOSE
        IOLog("No compatible entry found.\n");
#endif //APPLEMB_VERBOSE
        return false;
    }

    if ( compatibleEntry->isEqualTo( "keylargo-media-bay", sizeof("keylargo-media-bay")-1 ) == true ) {
#ifdef APPLEMB_VERBOSE
        IOLog("Found KeyLargo compatible property.\n");
#endif // APPLEMB_VERBOSE

        mbControllerType = kMBControllerKeyLargo;
 
        myMacIO = waitForService(serviceMatching("KeyLargo"));
    }

    if ( compatibleEntry->isEqualTo( "heathrow-media-bay", sizeof("heathrow-media-bay")-1 ) == true )  {
#ifdef APPLEMB_VERBOSE
         IOLog("Found Heathrow compatible property.\n");
#endif // APPLEMB_VERBOSE

        mbControllerType = kMBControllerHeathrow;
        
        // now the parent could be either Hetrow or Gatwick
        // so jump back looking for my MacIO:
        myMacIO = OSDynamicCast(AppleMacIO, provider->getProvider());
    }

    if ( compatibleEntry->isEqualTo( "ohare-media-bay", sizeof("ohare-media-bay")-1 ) == true ) {
#ifdef APPLEMB_VERBOSE
         IOLog("Found OHare compatible property.\n");
#endif // APPLEMB_VERBOSE

        mbControllerType = kMBControllerOHare;
        myMacIO = waitForService(serviceMatching("OHare"));
    }
    
    if( (configAddrMap = provider->mapDeviceMemoryWithIndex( 0 )))
    {
        configAddr = (volatile UInt32 *) configAddrMap->getVirtualAddress();

#ifdef APPLEMB_VERBOSE
         IOLog("configAddr = 0x%08lx.\n",(unsigned int)configAddr);
#endif // APPLEMB_VERBOSE
    }
    else {
#ifdef APPLEMB_VERBOSE
        IOLog("configAddrMap failed.\n");
#endif // APPLEMB_VERBOSE
        return false;
    }

    if (myMacIO == NULL) {
#ifdef APPLEMB_VERBOSE
        IOLog("myMacIO == NULL.\n");
#endif // APPLEMB_VERBOSE
        return false;
    }

    workloop = IOWorkLoop::workLoop();      // make the workloop
    if(!workloop) {
#ifdef APPLEMB_VERBOSE
        IOLog("Error creating workloop.\n");
#endif // APPLEMB_VERBOSE
        return false;
    }
    
    intSource = IOInterruptEventSource::interruptEventSource
                (this, (IOInterruptEventAction) &handleInterrupt,
                 provider);

    if ((intSource == NULL) || (workloop->addEventSource(intSource) != kIOReturnSuccess)) {
#ifdef APPLEMB_VERBOSE
        IOLog("Problem adding interrupt event source...\n");
#endif // APPLEMB_VERBOSE

        return false;
    }
    else
        workloop->enableAllInterrupts();

    // Creates the command gate for the events that need to be in the queue
    commandGate = IOCommandGate::commandGate(this, commandGateCaller);

    // and adds it to the workloop:
    if ((commandGate == NULL) || 
        (workloop->addEventSource(commandGate) != kIOReturnSuccess))
    {
#ifdef VERBOSE_LOGS_ON_PMU_INT
        IOLog("Can not add a new IOCommandGate\n");
#endif // VERBOSE_LOGS_ON_PMU_INT
        return false;
    }

    // by default we start without client:
    ioClient = NULL;
    
    // The symbol to change power to the media bay:
    powerMediaBay = OSSymbol::withCString("powerMediaBay");

    // Remember with wich device we are starting:
    mbCurrentDevice = readMBID();

    // Starts the power managment. NOTE: to assure the correct behavior
    // of the driver initForPM should be called AFTER mbCurrentDevice is
    // set to its initial value.
    if (!initForPM(provider)) {
#ifdef APPLEMB_VERBOSE
        IOLog("Error joining the power managment tree.\n");
#endif // APPLEMB_VERBOSE
        return false;
    }
    
    // calls registerService so that the ata nubs can find this driver
    // and register with it:
    registerService();
    
    return true;
}

// --------------------------------------------------------------------------
//
// Method: initForPM
//                       
// Purpose:
//          sets up the conditions for the power managment (or, better power
//          behavior since there is no power control in this driver).
bool
AppleMediaBay::initForPM(IOService *provider)
{
    
    PMinit();                   // initialize superclass variables
    provider->joinPMtree(this); // attach into the power management hierarchy

    // were we able to init the power manager for this driver ? 
    if (pm_vars == NULL)
        return false;

    #define number_of_power_states 2
    static IOPMPowerState ourPowerStates[number_of_power_states] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
    };

    // register ourselves with ourself as policy-maker        
    registerPowerDriver(this, ourPowerStates, number_of_power_states);

    return true;
}

// --------------------------------------------------------------------------
//
// Method: setPowerState
//                       
// Purpose:
//        we do not actually control the power of the devices inserted in the
//        mediabay. The driver for each of those should do the right thing.
//        so on power off we do not really do anything. But on power on we
//        check that the device in the media bay did not change. And if it
//        did we handle the case.
IOReturn
AppleMediaBay::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
    // This is the only power state we care about:
    if (powerStateOrdinal == 1)
    {
        MediaBayDeviceType  tmpCurrentDevice = readMBID();
        
        // Check the device ID in the media bay:
        if (mbCurrentDevice != tmpCurrentDevice) {

#ifdef APPLEMB_VERBOSE
            IOLog("AppleMediaBay::setPowerState to %d 0x%04x 0x%04x\n",  powerStateOrdinal, mbCurrentDevice, tmpCurrentDevice);
            IOSleep(1000);
#endif // APPLEMB_VERBOSE

            // it is different than before. If the new state is
            // not deviceNone (so the user ejected and re-inserted
            // the device while asleep) we have to handle the ejection:
            
            // First we do not want to handle interrupts in this sequence:
            intSource->disable();

            if (tmpCurrentDevice == deviceNone) {
                // sets everythin up as for a device removal:
                mbPreviousDevice = mbCurrentDevice;
                mbCurrentDevice = deviceNone;
                
                // and handles the ejection:
                handleDeviceEjection();
            }
            
            // Now we are in a normal device switch (since the insertion/removal)
            // was already handled few lines above. So we can call the interrupt dispathcer
            // to handle this case:
            dispatchInterrupt();

            // re-enable the interrupts:
            intSource->enable();
        }
        else if (mbCurrentDevice != deviceNone) {
             // re-power media bay on:
             setMediaBayDevicePower(true, mbCurrentDevice); 
        }
    }
    else {
         // Power the media bay off:
         setMediaBayDevicePower(false, deviceNone);
    }

    return IOPMAckImplied;
}

// --------------------------------------------------------------------------
//
// Method: registerMediaNub
//                       
// Purpose:
//          calls registerMBClient passing trough the command gate (see below).
IOReturn
AppleMediaBay::registerMediaNub(AppleMacIODevice *registerMe)
{
    IOReturn returnValue = kIOReturnError;
    
    if (commandGate != NULL)
        returnValue = commandGate->runCommand((void*)registerNub , registerMe);
    
    return returnValue;
}

// --------------------------------------------------------------------------
//
// Method: registerMediaNub
//                       
// Purpose:
//          calls deRegisterMBClient passing trough the command gate (see below).
IOReturn
AppleMediaBay::deRegisterMediaNub(AppleMacIODevice *deRegisterMe)
{
    IOReturn returnValue = kIOReturnError;
    
    if (commandGate != NULL)
        returnValue = commandGate->runCommand((void*)deRegisterNub , deRegisterMe);
    
    return returnValue;
}

// --------------------------------------------------------------------------
//
// Method: registerMBClient
//                       
// Purpose:
// 	An AppleMediaBayATANub nub register with the media bay driver
//      though this call. The call fails if a client already registerd.
bool
AppleMediaBay::registerMBClient(AppleMediaBayATANub *registerMe)
{
    if (ioClient == NULL) {
        ioClient = registerMe;

#ifdef APPLEMB_VERBOSE
        IOLog("Registering %s.\n", registerMe->getName());
#endif // APPLEMB_VERBOSE
    
        // Remember with wich device we are starting:
        mbCurrentDevice = readMBID();

        // If there is not a device inserted power off the media,
        // otherwise make it behave as an insertion.
        if (mbCurrentDevice == deviceNone)
            handleDeviceEjection();
        else 
            handleDeviceInsertion();

        return true;
    }

#ifdef APPLEMB_VERBOSE
    IOLog("Attempting to register a client twice.\n");
#endif // APPLEMB_VERBOSE
    return false;
}

// --------------------------------------------------------------------------
//
// Method: deRegisterMBClient
//                       
// Purpose:
// 	An AppleMediaBayATANub nub can deregiter with the media bay driver
//      though this call. The call fails if the client that attempts to
//      dergister is not the regisred one.
bool 
AppleMediaBay::deRegisterMBClient(AppleMediaBayATANub *deRegisterMe)
{
    if (ioClient == deRegisterMe) {
        ioClient = NULL;

#ifdef APPLEMB_VERBOSE
        IOLog("De-registering %s.\n", deRegisterMe->getName());
#endif // APPLEMB_VERBOSE

        return true;
    }

#ifdef APPLEMB_VERBOSE
    IOLog("Attempting to de-register an unregistered client.\n");
#endif // APPLEMB_VERBOSE
    return false;
}
    
// --------------------------------------------------------------------------
//
// Method: isFloppyOn, isATAOn, isPCIOn, isSoundOn, isPowerOn
//                       
// Purpose:
// 	The following set of functions return the
// 	current mediabay device. Since we do not
// 	wish to publish the interal IDs (that may
// 	change) we have a method for each possibl
// 	device:
bool
AppleMediaBay::isFloppyOn(void)
{
    return ((mbCurrentDevice == deviceAutoFloppy) || (mbCurrentDevice == deviceManualFloppy));
}

bool
AppleMediaBay::isATAOn(void)
{
    return (mbCurrentDevice == deviceATA);
}

bool
AppleMediaBay::isPCIOn(void)
{
    return (mbCurrentDevice == devicePCI);
}

bool
AppleMediaBay::isSoundOn(void)
{
    return (mbCurrentDevice == deviceSound);
}

bool
AppleMediaBay::isPowerOn(void)
{
    return (mbCurrentDevice == devicePower);
}

// --------------------------------------------------------------------------
//
// Method: dispatchMBCommand
//                       
// Purpose:
//        Dispatches commands to the ATA the arguments are the command ID
//        and three possible pointers:
IOReturn
AppleMediaBay::dispatchMBCommand(int commandID, void *arg1, void *arg2, void *arg3r)
{
    IOReturn returnValue = kIOReturnBadArgument;
    
    switch (commandID)
    {
        case registerNub:
        {
            AppleMediaBayATANub *myDevice = OSDynamicCast(AppleMediaBayATANub, (OSObject*)arg1);
            
            // If the type of device that registers is correct it calls registerMBClient
            // that does the real job. 
            if (myDevice != NULL)
                returnValue = (registerMBClient(myDevice) ? kIOReturnSuccess : kIOReturnError);
        }
        break;
        
        case deRegisterNub:
        {
            AppleMediaBayATANub *myDevice = OSDynamicCast(AppleMediaBayATANub, (OSObject*)arg1);
            
            // If the type of device that registers is correct it calls deRegisterMBClient
            // that does the real job. 
            if (myDevice != NULL)
                returnValue = (deRegisterMBClient(myDevice) ? kIOReturnSuccess : kIOReturnError);
        }
        break;
        
        case powerOn:
        break;
        
        case powerOff: 
        break;  
    }
    
    // Returns with the return value found from the previous
    // commands.
    return returnValue;
}
    
// --------------------------------------------------------------------------
//
// Method: dispatchInterrupt
//                       
// Purpose:
//         Interrupt handler/dispatcher. When a device is inserted (or removed)
//         from the media bay this dispatches the calls to the right method:
void
AppleMediaBay::dispatchInterrupt()
{
    // A temporary holder for the new device id until we are sure we are
    // holding the right thing:
     MediaBayDeviceType  tmpCurrentDevice;

    // Updates the previous device with the current device (assuming we are
    // switching:
    mbPreviousDevice = mbCurrentDevice;

    // Wait 500 milliseconds to let thing settle.
    IOSleep(500);
        
    tmpCurrentDevice = readMBID();

#ifdef APPLEMB_VERBOSE
    IOLog("AppleMediaBay::dispatchInterrupt mbPreviousDevice = 0x%04x tmpCurrentDevice = 0x%04x\n", 
            mbPreviousDevice , tmpCurrentDevice);
#endif // APPLEMB_VERBOSE

    // There are 4 possible configuration for the media bay events, but
    // only 2 are valid:
    if ((mbPreviousDevice == deviceNone) && (tmpCurrentDevice != deviceNone)) {
        // Register the current device:
        mbCurrentDevice = tmpCurrentDevice;

        // Obviously if before we did not have any device in the media bay
        // and now we have something it must mean that we inserted a device:
        handleDeviceInsertion();
    }
    else if ((mbPreviousDevice != deviceNone) && (tmpCurrentDevice == deviceNone)) {
        // Register the current device:
        mbCurrentDevice = tmpCurrentDevice;

        // This is exaclty the opposite logic, if the id now is off and
        // before it was on I should assume that the device was ejected:
        handleDeviceEjection();
    }
    else /* if (mbPreviousDevice == tmpCurrentDevice)  */ {
        // Why are we here ? We got an interrupt but the ID did not change
        // so I assume that the user is in the process of inserting (or
        // ejecting a device). So we keep looping until we have a "winner".
    }
}

// --------------------------------------------------------------------------
//
// Method: handleDeviceInsertion
//                       
// Purpose:
void
AppleMediaBay::handleDeviceInsertion()
{    
#ifdef APPLEMB_VERBOSE
    IOLog("AppleMediaBay::handleDeviceInsertion of device  0x%04x\n",  mbCurrentDevice);
    IOSleep(1000);
#endif // APPLEMB_VERBOSE

    // Power the media bay on:
    setMediaBayDevicePower(true, mbCurrentDevice);

    if (ioClient != NULL)
        ioClient->handleDeviceInsertion();
#ifdef APPLEMB_VERBOSE
    else
        IOLog("AppleMediaBay::handleDeviceInsertion missing client can't perform\n");
#endif // APPLEMB_VERBOSE
    
}

// --------------------------------------------------------------------------
//
// Method: handleDeviceEjection
//                       
// Purpose:
void
AppleMediaBay::handleDeviceEjection()
{
#ifdef APPLEMB_VERBOSE
    IOLog("AppleMediaBay::handleDeviceEjection of device  0x%04x\n",  mbPreviousDevice);
    IOSleep(1000);
#endif // APPLEMB_VERBOSE

    if (ioClient != NULL)
        ioClient->handleDeviceEjection();
#ifdef APPLEMB_VERBOSE
    else
        IOLog("AppleMediaBay::handleDeviceEjection missing client can't perform\n");
#endif // APPLEMB_VERBOSE

    // Power the media bay off:
    setMediaBayDevicePower(false, mbPreviousDevice);
}

// --------------------------------------------------------------------------
//
// Method: setMediaBayDevicePower
//                       
// Purpose:
//         Sets the power on the given device off or on:
void 
AppleMediaBay::setMediaBayDevicePower(bool powerUp, MediaBayDeviceType thisDevice)
{
    if (powerUp) {
        // decides which device to power on:
        UInt8 busPower;
        switch (thisDevice)
        {
            case deviceAutoFloppy:
            case deviceManualFloppy:
                busPower = floppyOn;
            break;
            
            case deviceSound:
                busPower = mbSoundOn;
            break;
            
            case deviceATA:
                busPower = ATAOn;
            break;
    
            case devicePCI:
                busPower = PCIOn;
            break;
            
            default:
                busPower = mbOff;
            break;
        }

        // Power on the media bay:
        myMacIO->callPlatformFunction(powerMediaBay, false, (void*)true, (void*)busPower, NULL, NULL);
        
         // Re-enable media bay ID Register by returning it to its input state
        if (mbControllerType == kMBControllerKeyLargo)
            *configAddr &= ~(0x0F000000);
        else
            *configAddr &= ~(0x000F0000);
	eieio();
        IOSleep(500);
    }
    else {
        // Powering the media bay off:
        myMacIO->callPlatformFunction(powerMediaBay, false, (void*)false, (void*)mbOff, NULL, NULL);
    }
}

// --------------------------------------------------------------------------
//
// Method: readMBID
//                       
// Purpose:
//          reads the mediaID to find out which device (or devices) is
//          currently inserted in the mediabay.
AppleMediaBay::MediaBayDeviceType
AppleMediaBay::readMBID()
{
    UInt16 mediaBayID;

    if (mbControllerType == kMBControllerKeyLargo)
            mediaBayID = ((*configAddr) & mediaBayIDMask_KeyLargo ) >> 28;
    else
            mediaBayID = ((*configAddr) & mediaBayIDMask ) >> 20;
    
    return (MediaBayDeviceType)mediaBayID;
}

// --------------------------------------------------------------------------
//
// Method: handleInterrupt
//                       
// Purpose:
//           This is the static interrupt handler. It does not do anything
//           special, just defers the call to the interrupt handler in the
//           AppleMediaBay object:
/* static */ void
AppleMediaBay::handleInterrupt(OSObject *owner, IOInterruptEventSource *src, int count)
{
    AppleMediaBay *myThis = OSDynamicCast(AppleMediaBay, owner);

#ifdef APPLEMB_VERBOSE
    IOLog("AppleMediaBay::handleInterrupt(0x%08lx, 0x%08lx, %d)\n",  owner, owner, count);
    IOSleep(1000);
#endif // APPLEMB_VERBOSE

    // If the owner is the right object we can go on and process the interrupt:
    if (myThis != NULL)
        myThis->dispatchInterrupt();
}
// --------------------------------------------------------------------------
//  
// Method: commandGateCaller
//
// Purpose:
//          we need a static function to serve the command gate. So this is it
//          just checks that the object type is correct and defers the dipatch
//          of the commands to the dispatchMBCommand method.
/* static */ IOReturn
AppleMediaBay::commandGateCaller(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOReturn retu = kIOReturnError;
    AppleMediaBay *myThis = OSDynamicCast(AppleMediaBay, owner);
    
    // If the owner is the right object we can go on and process the interrupt:
    if (myThis != NULL)
        retu = myThis->dispatchMBCommand((int)arg0, arg1, arg2, arg3);
        
    return retu;
}  
