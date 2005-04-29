/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Implementation of the interface for the keylargo I2C interface
 *
 * HISTORY
 *
 */

#include "AppleThermal.h"

#define super IOService
OSDefineMetaClassAndStructors( AppleThermal, IOService )

/* ===============
 * Private Methods
 * =============== */

// --------------------------------------------------------------------------
// Method: findAndAttachI2C
//
// Purpose:
//   Attaches to the i2c interface:
bool
AppleThermal::findAndAttachI2C(IOService *provider)
{
    const OSSymbol *i2cDriverName;
    IOService *i2cCandidate;

    // Searches the i2c:
    i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-uni-n");
    i2cCandidate = waitForService(resourceMatching(i2cDriverName));
    //interface = OSDynamicCast(PPCI2CInterface, i2cCandidate->getProperty(i2cDriverName));
    interface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

    if (interface == NULL) {
#ifdef DEBUGMODE
        IOLog("AppleThermal::findAndAttachI2C can't find the i2c in the registry\n");
#endif // DEBUGMODE
        return false;
    }

    // Make susre this will not get lost:
    interface->retain();

    return true;
}

// --------------------------------------------------------------------------
// Method: detachFromI2C
//
// Purpose:
//   detaches from the I2C
bool
AppleThermal::detachFromI2C(IOService* /*provider*/)
{
    if (interface) {
        //delete interface;
        interface->release();

        interface = 0;
    }

    return (true);
}

// --------------------------------------------------------------------------
// Method: openI2C
//
// Purpose:
//        opens and sets up the i2c bus
bool
AppleThermal::openI2C(UInt8 id)
{
    if (interface != NULL) {
        // Open the interface and sets it in the wanted mode:
        interface->openI2CBus(id);
        interface->setStandardMode();
        
        // the i2c driver does not support well read in interrupt mode
        // so it is better to "go polling" (read does not timeout on errors
        // in interrupt mode).
        interface->setPollingMode(true);
 
        return true;
    }

    return false;
}


// --------------------------------------------------------------------------
// Method: closeI2C
//
// Purpose:
//        closes the i2c bus
void
AppleThermal::closeI2C()
{
    // Closes the bus so other can access to it:
    if (interface != NULL)
        interface->closeI2CBus();
}

// --------------------------------------------------------------------------
// Method: writeI2C
//
// Purpose:
//        sends "size" bytes in the i2c bus. There is not really the need for
//        such a function, but it makes much much more easy to log and track
//        errors.
bool
AppleThermal::writeI2C(SInt8 *data, UInt32 size)
{
    bool success = false;

    if (interface != NULL) {
        success = interface->writeI2CBus((UInt8)kThermostatAddress, 0, (UInt8*)data, size);

#ifdef DEBUGMODE
        if (!success)
            IOLog("AppleThermal::writeI2C fails on %d of %d %d\n", size, data[0], data[1]);
#endif //DEBUGMODE
    }

    return success;
}

// --------------------------------------------------------------------------
// Method: readI2C
//
// Purpose:
//        reads "size" bytes from the i2c bus. There is not really the need for
//        such a function, but it makes much much more easy to log and track
//        errors.
bool
AppleThermal::readI2C(SInt8 *data, UInt32 size)
{
    bool success = false;

    if (interface != NULL) {
        success = interface->readI2CBus((UInt8)kThermostatAddress, 0, (UInt8*)data, size);

#ifdef DEBUGMODE
        if (!success)
            IOLog("AppleThermal::readI2C fails on %d of %d %d\n", size, data[0], data[1]);
#endif //DEBUGMODE
    }
    return success;
}

// --------------------------------------------------------------------------
// Method: dataIsValid
//
// Purpose:
//   checks if we are actually holding the temperature ranges for the machine
//   we are running on:
bool
AppleThermal::dataIsValid()
{
    // Since the PE does not publish the machine we are running on
    // I got to do it by hand:
    IOService *topProvider = NULL, *provider = getProvider();

    // See if we find the top of the tree (with the machine type)
    // iterating all the way up:
    while (provider != NULL) {
        topProvider = provider;
        provider = provider->getProvider();
#ifdef BEPARANOID
        IOLog("AppleThermal::dataIsValid looking at %s\n", topProvider->getName());
#endif
    }

    if (topProvider != NULL) {
        if (IODTMatchNubWithKeys(topProvider, "'PowerBook3,1'")) {
            // This is the PowerbookG3 2000 so let's see if the
            // AppleThermal mach:
            if ((thermalDesign == kThermalDesign_Pismo) ||
                (thermalDesign == kThermalDesign_2000))
                return true;
            else {
#ifdef DEBUGMODE
                IOLog("AppleThermal::dataIsValid machine type is supported but the data is not maching\n");
#endif //DEBUGMODE
            }
        }
        else {
#ifdef DEBUGMODE
            IOLog("AppleThermal::dataIsValid unsupported machine type\n");
#endif //DEBUGMODE
        }
    }
    else {
#ifdef DEBUGMODE
        IOLog("AppleThermal::dataIsValid can't read the machine name at the top of the tree\n");
#endif //DEBUGMODE
    }

    // If we are here something went wrong:
    return false;
}

// --------------------------------------------------------------------------
// Method: setThermostat
//
// Purpose:
//    programs a set the on/off threshhold values for a thermostat.
bool
AppleThermal::setThermostat(UInt32 id, SInt8 on, SInt8 off)
{
    bool success = false;

    if (openI2C(id)) {
        // We need a local copy of the data as a set of commands:
        SInt8 dataOn[3] = {3, on, 0};	// 3 is the ON register
        SInt8 dataOff[3] = {2, off, 0};	// 2 is the OFF register
        SInt8 dataClr[2] = {1, 0};	// clears the config register:

        // and writes the data
        success = writeI2C((SInt8*)&dataOn, sizeof(dataOn));    // set on temperature
        success &= writeI2C((SInt8*)&dataOff, sizeof(dataOff)); // set off temperature
        success &= writeI2C((SInt8*)&dataClr, sizeof(dataClr)); // clear config register to make sure it's on
        closeI2C();
    }

    return (success);
}

// --------------------------------------------------------------------------
// Method: getThermostat
//
// Purpose:
//    reads a set the on/off threshhold values for a thermostat.
bool
AppleThermal::getThermostat(UInt32 id, SInt8 *on, SInt8 *off)
{
    bool success = false;

    if (openI2C(id)) {
        // We need a local copy of the data:
        SInt8 tmp, data[2]={0,0};

        tmp = 3;                                                            // On temp
        success = writeI2C((SInt8*)&tmp, 1);  // set pointer to register for ON temp
        success &= readI2C((SInt8*)&data, 2); // get on temperature

#ifdef BEPARANOID
        IOLog("AppleThermal::getThermostat reads for ON %d %d\n", data[0], data[1]);
#endif
        
        // and if the given pointers were correct stores it:
        if ((on != NULL) && (success))
            *on = data[0];

        tmp = 2;                                                            // Off temp:
        success &= writeI2C((SInt8*)&tmp, 1); // set pointer to register for OFF temp
        success &= readI2C((SInt8*)&data, 2); // get off temperature

#ifdef BEPARANOID
        IOLog("AppleThermal::getThermostat reads for OFF %d %d\n", data[0], data[1]);
#endif

        // and if the given pointers were correct stores it:
        if ((off != NULL) && (success))
            *off = data[0];

        closeI2C();
    }

    return success;
}

// --------------------------------------------------------------------------
// Method: retriveThermalProperty
//
// Purpose:
//   retrives the thermal properties from the provider and stores the data we
//   need locally:
bool
AppleThermal::retriveThermalProperty(IOService *provider)
{
    OSData 	*t;
    ThermalInfoPtr thisThermalInfo;
    int i;

    // sets up the interface:
    t = OSDynamicCast(OSData, provider->getProperty("thermal-info"));
    if (t == NULL) {
#ifdef DEBUGMODE
        IOLog( "AppleThermal::retriveThermalProperty missing property thermal-info in the registry.\n");
        IOLog( "                                assuming that this hardware does not support it.\n");
#endif
        return false;
    }

    // If we are here we got the properties so we can make a local
    // copy and release the property:
    thisThermalInfo = (ThermalInfoPtr)t->getBytesNoCopy();
    if (thisThermalInfo == NULL)  {
#ifdef DEBUGMODE
        IOLog( "AppleThermal::retriveThermalProperty property thermal-info is present but empty.\n");
#endif
        return false;
    }

    thermalDesign = thisThermalInfo->thermalDesign;         // unique ID per implementation
    numberFans = thisThermalInfo->numberFans;               // total number of software-controllable fans in system
    numberThermostats = thisThermalInfo->numberThermostats; // number of software-controllable thermostats/thermometers in system

#ifdef BEPARANOID
    IOLog( "AppleThermal::retriveThermalProperty number of fans: %d number of thermostats %d.\n", numberFans, numberThermostats);
#endif //BEPARANOID

    // Since we have anyway to make a copy of the temperatures we can also make it in the "right way":
    tempRanges = (TempRangesPtr)IOMalloc(sizeof(TempRanges) * numberThermostats);
    if (tempRanges == NULL)  {
#ifdef DEBUGMODE
        IOLog( "AppleThermal::retriveThermalProperty there is not memory to allocate the TempRanges.\n");
#endif
        return false;
    }

    // Makes a local copy of the temperature ranges for each termostat:
    for (i = 0 ; i < numberThermostats; i++) {
#ifdef DOTEST
        // Use values that are easy to test:
        tempRanges[i].on  = 10;
        tempRanges[i].off = 0;        
#else
        // Use the real values:
        tempRanges[i].on  = thisThermalInfo->onOffTemperatures[i] >> 8;
        tempRanges[i].off = thisThermalInfo->onOffTemperatures[i] & 0xFF;
#endif
        
#ifdef BEPARANOID
        IOLog( "AppleThermal::retriveThermalProperty termostat %d on at %d off at %d.\n", i, tempRanges[i].on, tempRanges[i].off);
#endif //BEPARANOID
    }

    // and we can actually returns happy:
    return true;
}

// --------------------------------------------------------------------------
// Method: setSleepMode
//
// Purpose:
//        puts the thermostat in sleep mode:
bool
AppleThermal::setSleepMode()
{
    bool  success = true;
    int i;

    for (i = 0 ; i < numberThermostats; i++) {
        if (openI2C(i)) {
             SInt8 dataSD[2] = {1, 0x01};	// set SD (shutdown) bit in config register:

             // and writes the data
             success = writeI2C((SInt8*)dataSD, sizeof(dataSD));    // shutdown
             closeI2C();

             if (!success) {
 #ifdef DEBUGMODE
                 IOLog("AppleThermal::setSleepMode setThermostat(%d) in sleep fails !!\n", i);
 #endif // DEBUGMODE
             }
         }
    }
    
    return success;
}

// --------------------------------------------------------------------------
// Method: setThermostats
//
// Purpose:
//         programs all the thermostats
bool
AppleThermal::setThermostats()
{
    int i;
    for (i = 0 ; i < numberThermostats; i++) {
        if (!setThermostat(i,  tempRanges[i].on, tempRanges[i].off)) {
#ifdef DEBUGMODE
            IOLog("AppleThermal::setThermostats setThermostat(%d, %d, %d) fails !!\n", i,  tempRanges[i].on, tempRanges[i].off);
#endif // DEBUGMODE
            return (false);
        }

#ifdef BEPARANOID
        SInt8 localON, localOFF;
        getThermostat(i, &localON, &localOFF);
        IOLog( "AppleThermal::setThermostats termostat %d set  on at %d off at %d.\n", i, tempRanges[i].on, tempRanges[i].off);
        IOLog( "AppleThermal::setThermostats termostat %d read on at %d off at %d.\n", i, localON, localOFF);
#endif //BEPARANOID

    }

    // if we are here everythgin went fine:
    return true;
}

/* ===============
 * Public Methods
 * =============== */

// --------------------------------------------------------------------------
// Method: free
//
// Purpose:
//   releases all the allocated resources:
void
AppleThermal::free()
{
    // detaches the I2C:
    detachFromI2C(getProvider());

    // Releases the allocated memory:
    if (tempRanges) {
        IOFree(tempRanges, sizeof(TempRanges) * numberThermostats);
        tempRanges = NULL;
    }

    super::free();
}

// --------------------------------------------------------------------------
// Method: start
//
// Purpose:
//   retrives the thermal properties from the device property and waits for
//   the i2c driver. When the i2c diver is ready it uses it to program the
bool
AppleThermal::start(IOService *provider)
{
    // The rule calls for the super::start at the begin:
    if (!super::start(provider))
        return false;

    // When an object is created is already filled with 0s but an extra will
    // not hurt:
    tempRanges = NULL;

    // See if the property is availabe, and if it is we setup the temperature
    // ranges as we need them:
    if (!retriveThermalProperty(provider)) {
        return false;
    }

    // Perfroms a sanity check:
    if (!dataIsValid())
        return false;

    // We have all the data we need, all is left to do is to transfer it
    // to the thermostats so we got to get the interface
    if (!findAndAttachI2C(provider))  {
#ifdef DEBUGMODE
        IOLog("AppleThermal::start if (!findAndAttachI2C(IOService *provider)) fails !!\n");
#endif // DEBUGMODE
        return (false);
    }

#if 0
    // This code ran once to find the address of the therms
    openI2C(0);
    int i;
    for (i = 0; i <255 ; i++) {
        SInt8 dataSD[2] = {1, 0x01};

        if (interface->writeI2CBus((UInt8)i, 0, (UInt8*)dataSD, sizeof(dataSD)))
            IOLog("AppleThermal::start interface->writeI2CBus(0x%02x, 0) succeeded!!\n",(UInt8)i);
    }
    closeI2C();
#endif
    
    // All it is left to do is to program all the thermostats:
    if (!setThermostats()) {
#ifdef DEBUGMODE
        IOLog("AppleThermal::start setThermostats fails !!\n");
#endif // DEBUGMODE
            return (false);
    }

#ifdef SUPPORTS_POWER_MANAGER
    // If we decide to support the power manager and attach to the
    // PM tree this is what we need:
    PMinit();                   // initialize superclass variables
    provider->joinPMtree(this); // attach into the power management hierarchy

#define number_of_power_states 2

    static IOPMPowerState ourPowerStates[number_of_power_states] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
    };


    // register ourselves with ourself as policy-maker
    if (pm_vars != NULL)
        registerPowerDriver(this, ourPowerStates, number_of_power_states);


    // We are starting up, so not waking up:
    wakingUpFromSleep = false;

    return true;
#else // ! SUPPORTS_POWER_MANAGER
      // And now (surprise!) we return false anyway. This will force the driver to unload
      // which is o.k. since we already did everything we needed to do there is no real
      // reason to stick around:
    return false;
#endif SUPPORTS_POWER_MANAGER
}


// --------------------------------------------------------------------------
// Method: readThermostatTemperature
//
// Purpose:
//        read the temperature (C) of the thermostat of the id given
UInt32
AppleThermal::readThermostatTemperature (UInt8 inID)
{
    bool  success = true;

    // The array will contain the integer part of the
    // temperature on the first byte and the fractional part
    // in the second byte.
    SInt8 temperature[2];
    
    if (openI2C(inID)) {
        SInt8 registerToReadFrom = 0;

        // specify which register we are to read temperature from
        success &= writeI2C((SInt8*)&registerToReadFrom, sizeof(registerToReadFrom));

        if (success) {
            success &= readI2C((SInt8*)&temperature, 2);
        }
    }

    if (success)
        return (UInt32)temperature[0];
    else
        return 0;
}

// --------------------------------------------------------------------------
// Method: readThermostatSettings
//
// Purpose:
//        read the settings of the given thermostat.
bool
AppleThermal::readThermostatSettings (UInt8 id, SInt8 *lOn,SInt8 *lOff)
{
    return getThermostat(id, lOn, lOff);
}

// --------------------------------------------------------------------------
// Method: setPowerState
//
// Purpose:
//          this routine handles all Power Manager calls for the thermostat
//          hardware (Mainly putting thermostats in standby mode across).
IOReturn
AppleThermal::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
#ifdef SUPPORTS_POWER_MANAGER
    if (powerStateOrdinal == 0)
    {
        setSleepMode();
        wakingUpFromSleep = true;
    }

    if ((powerStateOrdinal == 1) && (wakingUpFromSleep))
    {
        setThermostats();
        wakingUpFromSleep = false;
    }
#endif // SUPPORTS_POWER_MANAGER
    
    return IOPMAckImplied;
}
