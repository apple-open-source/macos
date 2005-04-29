/* 
 *Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 *@APPLE_LICENSE_HEADER_START@
 *
 *The contents of this file constitute Original Code as defined in and
 *are subject to the Apple Public Source License Version 1.1 (the
 *"License").  You may not use this file except in compliance with the
 *License.  Please obtain a copy of the License at
 *http://www.apple.com/publicsource and read it before using this file.
 *
 *This Original Code and all software distributed under the License are
 *distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *License for the specific language governing rights and limitations
 *under the License.
 *
 *@APPLE_LICENSE_HEADER_END@
 */
/*
 *AppleSCCModem.cpp
 *
 *MacOSX implementation of Modem Port driver
 *
 * 31-Jan-01	Paul Sun		Fixed bug # 2610330 -- Put the delay in the dequeueData()
 *								instead of the acquirePort().
 *
 * 22-Jan-01	Paul Sun		Fixed bug # 2560437 -- took out all the codes that powers on
 *								the modem. it is now supported by PlatformExpert.
 *
 * Godfrey van der Linden		Original version
 *
 *Copyright ©: 	1999 Apple Computer, Inc.  all rights reserved.
 */

#include <libkern/OSByteOrder.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>
#include <IOKit/serial/IOModemSerialStreamSync.h>

#include "PPCSerialPort.h"

#define fRS232Stream ((IORS232SerialStreamSync *) fProvider)

class AppleSCCModem : public IOModemSerialStreamSync
{
    OSDeclareDefaultStructors(AppleSCCModem)

protected:
//    IOPhysicalAddress	powerRegister;
//    UInt32				powerMask;
	bool				portOpened;
	UInt32				startingTime;
	bool				bModemReady;
    IOService 			*audio;
	IOService			*myProvider;

	virtual bool initForPM(IOService *provider);
	virtual unsigned long initialPowerStateForDomainState ( IOPMPowerFlags );
	virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);
    static void switchModeminputSource(OSObject *self, bool carrier);

public:
    virtual bool attach(IOService *provider);
    virtual void detach(IOService *provider);
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
    virtual IOService *probe(IOService *provider, SInt32 *score);

    // IOModemSerialStreamSync overridden member functions
    virtual IOReturn acquirePort(bool sleep);
    virtual IOReturn releasePort();
    
    virtual UInt32 getState();
    virtual IOReturn setState(UInt32 state, UInt32 mask);
    virtual IOReturn watchState(UInt32 *state, UInt32 mask);

    virtual UInt32 nextEvent();
    virtual IOReturn executeEvent(UInt32 event, UInt32 data);
    virtual IOReturn requestEvent(UInt32 event, UInt32 *data);

    virtual IOReturn enqueueEvent(UInt32 event, UInt32 data, bool sleep);
    virtual IOReturn dequeueEvent(UInt32 *event, UInt32 *data, bool sleep);

    virtual IOReturn enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep);
    virtual IOReturn dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);
    
};

OSDefineMetaClassAndStructors(AppleSCCModem, IOModemSerialStreamSync)
#define super IOModemSerialStreamSync

static inline UInt64 getDebugFlagsTable(OSDictionary *props)
{
    OSNumber *debugProp;
    UInt64    debugFlags = gIOKitDebug;

    debugProp = OSDynamicCast(OSNumber, props->getObject(gIOKitDebugKey));
    if (debugProp)
	debugFlags = debugProp->unsigned64BitValue();

    return debugFlags;
}

#define getDebugFlags() (getDebugFlagsTable(getPropertyTable()))

#define IOLogCond(cond, x) do { if (cond) (IOLog x); } while (0)


bool AppleSCCModem::attach(IOService *provider)
{
    // Skip the 'IOSerialStream's super classes, they aren't 'driver side'
    return IOService::attach(provider);
}

void AppleSCCModem::detach(IOService *provider)
{
    // Skip the 'IOSerialStream's super classes, they aren't 'driver side'
    IOService::detach(provider);
}

void AppleSCCModem::stop(IOService *provider)
{
	PMstop();

     // Skip the 'IOSerialStream's super classes, they aren't 'driver side'
    IOService::stop(provider);
}

bool AppleSCCModem::start(IOService *provider)
{
    bool 	logStart = (0 != (kIOLogStart & getDebugFlags()));

    IOLogCond(logStart, ("%s(%x): Start is called\n", getName(), (unsigned)this));
    portOpened = false;
	audio = NULL;
    myProvider = provider;

	
    // Skip the 'IOSerialStream's super classes, they aren't 'driver side'
    if (!IOService::start(provider))
   {
         return false;
    }

    
   fRS232Stream = OSDynamicCast(IORS232SerialStreamSync, provider);
    if (!fRS232Stream) {
        IOLogCond(logStart,
                 ("%s(%x): Provider not IORS232?\n", getName(), (unsigned)this));
        return false;
    }
    
    // We are done name & register ourselves and then return
    setProperty(kIOTTYBaseNameKey, "modem");
    registerService();
	initForPM(provider);
	
    return true;
}



IOService * AppleSCCModem::probe(IOService *provider, SInt32 *)
{
    OSString	*matchEntry = 0;
    bool logProbe = (0 != (kIOLogProbe & getDebugFlags()));

	IOLogCond(logProbe,
		 ("AppleSCCModem::probe called\n"));

	matchEntry = OSDynamicCast (OSString, getProperty (kIONameMatchedKey));
    if ((matchEntry == 0) || (matchEntry->isEqualTo("chrp,es2") == false))
        return this;
    
    OSData *s = OSDynamicCast(OSData, provider->getProperty("slot-names"));
    if (!s)
	return 0;

    UInt32 *nWords = (UInt32*)s->getBytesNoCopy();

    if (*nWords < 1) {
	IOLogCond(logProbe,
		 ("AppleSCCModem::probe chrp,es2 failed no slot-name\n"));
        return 0;
    }
    
    // If there is more than one entry
    char *tmpPtr, *tmpName;

    tmpName = (char *) s->getBytesNoCopy() + sizeof(UInt32);

    // To make parsing easy, sets the sting in low case:
    for (tmpPtr = tmpName; *tmpPtr != 0; tmpPtr++)
        *tmpPtr |= 32;

    if (strncmp (tmpName, "modem", 5) == 0)
        return this;
    else {
        IOLogCond(logProbe, ("\n\nAppleSCCModem::probe -- did not match with modem string \n"));
        return 0;
    }
}


// --------------------------------------------------------------------------
//
// Method: initialPowerStateForDomainState
//                       
// Purpose:
//          sets up the initial power condition for the modem.



unsigned long AppleSCCModem::initialPowerStateForDomainState ( IOPMPowerFlags )
{
	//IOLog("AppleSCCModem::initialPowerStateForDomainState -- is called\n");
	
	return 0;
}



// --------------------------------------------------------------------------
//
// Method: initForPM
//                       
// Purpose:
//          sets up the conditions for the power managment for the modem.

bool
AppleSCCModem::initForPM(IOService *provider)
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
// Purpose: set the power state depending on the port being opened or not.

IOReturn AppleSCCModem::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
//	IOLog("%s(%x): setPowerState is called -- powerStateOrdinal = %ld\n", getName(), (unsigned)this, powerStateOrdinal);

    // This is the only power state we care about:
    if (powerStateOrdinal == 1)
    {
        if (portOpened)
        {
            callPlatformFunction("PowerModem", false, (void *)true, 0, 0, 0);
            IOSleep(250);	// wait 250 milli-seconds 
            callPlatformFunction("ModemResetHigh", false, 0, 0, 0, 0);
            IOSleep(250);	// wait 250 milli-seconds 
            callPlatformFunction("ModemResetLow", false, 0, 0, 0, 0);
            IOSleep(250);	// wait 250 milli-seconds 
            callPlatformFunction("ModemResetHigh", false, 0, 0, 0, 0);
            IOSleep(250);	// wait 250 milli-seconds 
        }
    }
    else if (powerStateOrdinal == 0)
    {
        if (!portOpened)
        {
            callPlatformFunction("ModemResetLow", false, 0, 0, 0, 0);
            IOSleep(250);	// wait 250 milli-seconds 
            callPlatformFunction("PowerModem", false, (void *)false, 0, 0, 0);
        }
    }

    return IOPMAckImplied;
}

IOReturn AppleSCCModem::acquirePort(bool sleep)
{
	IOReturn	pwrResult;
    IOReturn	rtn = fRS232Stream->acquirePort(sleep);

	if (myProvider)
	{
		AppleSCCSerial *scc = OSDynamicCast(AppleSCCSerial, myProvider->getProvider());
		if (scc)
		{
			//IOLog("AppleSCCModem::acquirePort -- **scc != NULL** -- setting up the link\n");
			scc->setCarrierHack(this, &AppleSCCModem::switchModeminputSource);
		}
	}
    if (rtn == kIOReturnSuccess)
	{
		//IOLog("%s(%x): acquirePort returned kIOReturnSuccess\n", getName(), (unsigned)this);
		portOpened = true;

//		UInt32	theTimeIs = SCC_GetSystemTime() / 1000;
//		IOLog("%s(%x): acquirePort -- Before powering up -- theTimeIs = %ld\n", getName(), (unsigned)this, theTimeIs);
		
		pwrResult = changePowerStateTo(1);
		bModemReady = false;			// no delay in the enqueueData routine
		startingTime = SCC_GetSystemTime() / 1000;		// convert to second
                
//		theTimeIs = SCC_GetSystemTime() / 1000;
//		IOLog("%s(%x): acquirePort -- After powering up -- theTimeIs = %ld\n", getName(), (unsigned)this, theTimeIs);
		
		//IOLog("%s(%x): acquirePort -- changePowerStateTo returned %d\n", getName(), (unsigned)this, pwrResult);       
	}
//	else
//	{
//		IOLog("%s(%x): acquirePort did not return kIOReturnSuccess %d\n", getName(), (unsigned)this, rtn);
//	}
    return rtn;
}

IOReturn AppleSCCModem::releasePort()
{
    IOReturn rtn = fRS232Stream->releasePort();
    if (rtn == kIOReturnSuccess)
	{
		//IOLog("%s(%x): releasePort returned kIOReturnSuccess\n", getName(), (unsigned)this);
		portOpened = false;
		bModemReady = false;

        if (NULL != audio) 
        {
            //IOLog("AppleSCCModem::releasePort -- **setModemSound** -- turn off the modem sound\n");
            audio->callPlatformFunction (OSSymbol::withCString ("setModemSound"), false, (void *)false, 0, 0, 0);
        }
        
		changePowerStateTo(0);                
	}

//	else
//	{
//		IOLog("%s(%x): releasePort did not return kIOReturnSuccess %d\n", getName(), (unsigned)this, rtn);
//	}

    return rtn;
}
    
UInt32 AppleSCCModem::getState()
    { return fRS232Stream->getState(); }

IOReturn AppleSCCModem::setState(UInt32 state, UInt32 mask)
    { return fRS232Stream->setState(state, mask); }

IOReturn AppleSCCModem::watchState(UInt32 *state, UInt32 mask)
    { return fRS232Stream->watchState(state, mask); }

UInt32   AppleSCCModem::nextEvent()
    { return fRS232Stream->nextEvent(); }

IOReturn AppleSCCModem::executeEvent(UInt32 event, UInt32 data)
    { return fRS232Stream->executeEvent(event, data); }

IOReturn AppleSCCModem::requestEvent(UInt32 event, UInt32 *data)
    { return fRS232Stream->requestEvent(event, data); }

IOReturn AppleSCCModem::enqueueEvent(UInt32 event, UInt32 data, bool sleep)
    { return fRS232Stream->enqueueEvent(event, data, sleep); }

IOReturn AppleSCCModem::dequeueEvent(UInt32 *event, UInt32 *data, bool sleep)
    { return fRS232Stream->dequeueEvent(event, data, sleep); }

IOReturn AppleSCCModem::
enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep) 
{ 
	
	if (bModemReady)
	{
//		IOLog("%s(%x): enqueueData -- bModemReady is not true\n", getName(), (unsigned)this);
	}
	else
	{
		UInt32	currentTime = SCC_GetSystemTime() / 1000;		// convert to second;
		UInt32	diffTime = currentTime - startingTime;
		mach_timespec_t		timeOut;
		
		timeOut.tv_sec = 0;
		timeOut.tv_nsec = 1000;

		//IOLog("enqueueData -- before serviceMatching call -- current time is %ld\n", SCC_GetSystemTime());
		
		if (audio == NULL)
		{
			//IOLog("enqueueData -- serviceMatching call for AppleBurgundyAudio\n");
			audio = IOService::waitForService (IOService::serviceMatching ("AppleBurgundyAudio"), &timeOut);
			//if (audio != NULL)
			//	IOLog("enqueueData -- found AppleBurgundyAudio\n");
		}
		if (audio == NULL)
		{
			//IOLog("enqueueData -- serviceMatching call for AppleDACAAudio\n");
			audio = IOService::waitForService (IOService::serviceMatching ("AppleDACAAudio"), &timeOut);
			//if (audio != NULL)
			//	IOLog("enqueueData -- found AppleDACAAudio\n");
		}
		if (audio == NULL)
		{
			//IOLog("enqueueData -- serviceMatching call for AppleScreamerAudio\n");
			audio = IOService::waitForService (IOService::serviceMatching ("AppleScreamerAudio"), &timeOut);
			//if (audio != NULL)
			//	IOLog("enqueueData -- found AppleScreamerAudio\n");
		}
		if (audio == NULL)
		{
			//IOLog("enqueueData -- serviceMatching call for AppleTexasAudio\n");
			audio = IOService::waitForService (IOService::serviceMatching ("AppleTexasAudio"), &timeOut);
			//if (audio != NULL)
			//	IOLog("enqueueData -- found AppleTexasAudio\n");
		}
		if (audio == NULL)
		{
			//IOLog("enqueueData -- serviceMatching call for AppleTexas2Audio\n");
			audio = IOService::waitForService (IOService::serviceMatching ("AppleTexas2Audio"), &timeOut);
			//if (audio != NULL)
			//	IOLog("enqueueData -- found AppleTexas2Audio\n");
		}
			
		//IOLog("enqueueData -- after serviceMatching call -- current time is %ld\n", SCC_GetSystemTime());
		
//		IOLog("%s(%x): enqueueData -- startTime = %ld    theTimeIs = %ld     diffTime = %ld\n", getName(), (unsigned)this, startingTime, currentTime, diffTime);
		if (diffTime < 3)
		{
			switch (diffTime)
			{
				case 2	: //IOLog("%s(%x): enqueueData -- sleep 1 second\n", getName(), (unsigned)this);
						  IOSleep (1000); break;
				case 1	: //IOLog("%s(%x): enqueueData -- sleep 2 seconds\n", getName(), (unsigned)this);
						  IOSleep (2000); break;
				default	: //IOLog("%s(%x): enqueueData -- sleep 3 seconds\n", getName(), (unsigned)this);
						  IOSleep (3000);
			}
		}
//		theTimeIs = SCC_GetSystemTime() / 1000;
//		IOLog("%s(%x): enqueueData -- bModemReady -- current time is = %ld \n", getName(), (unsigned)this, theTimeIs);
        // Switches the modem sound source to internal modem.
                
        if (NULL != audio) 
        {
            //IOLog("AppleSCCModem::enqueueData -- **setModemSound** -- turn on the modem sound\n");
            audio->callPlatformFunction (OSSymbol::withCString ("setModemSound"), false, (void *)true, 0, 0, 0);
        }
//		else 
//		{
//			IOLog("audio == NULL -- could not set modem sound\n");
//		}
		bModemReady = true;
	}
	return fRS232Stream->enqueueData(buffer, size, count, sleep);
	
}	// end of AppleSCCModem::enqueueData


IOReturn AppleSCCModem::
dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
    { return fRS232Stream->dequeueData(buffer, size, count, min); }
    

//This thread gets scheduled when the carrierHack is called from the SCCSerialDriver
void AppleSCCModem::switchModeminputSource(OSObject *target, bool carrier)
{
    AppleSCCModem *self = (AppleSCCModem *) target;
    
  //  DLOG("switchModeminputSource %d\n",carrier);
    
    if (!self)
        return;        

	if (carrier != false)
	{
		//IOLog("AppleSCCModem::switchModeminputSource -- **setModemSound** -- turn off the modem sound\n");
        self->callPlatformFunction("setModemSound", false, (void *)false, 0, 0, 0);
	}
	else
	{
		//IOLog("AppleSCCModem::switchModeminputSource -- **setModemSound** -- turn on the modem sound\n");
        self->callPlatformFunction("setModemSound", false, (void *)true, 0, 0, 0);
	}
}
//TBD Add support for PD_RS232_S_DCD state change
//changeState(port, PD_RS232_S_DCD);


