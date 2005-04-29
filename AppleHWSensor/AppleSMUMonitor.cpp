/*
 *  AppleSMUMonitor.cpp
 *
 *  Created by William Gulland on Mon Feb 16 2004.
 *  Copyright (c) 2004 Apple Computer, Inc.. All rights reserved.
 *
 */
//		$Log: AppleSMUMonitor.cpp,v $
//		Revision 1.5  2004/11/05 23:14:31  townsle1
//		
//		Version 1.3.5d1: Add AppleHWSensor personalities for ControlTypeALS and
//		SensorTypeALS (3642655).  Add kBatt_sensor fType to AppleSMUMonitor for
//		handling 32 bit values (16.16) returned from batt-sensors (3778115).
//		
//		Revision 1.4  2004/05/04 20:35:42  wgulland
//		Send command to SMU in 2nd and 3rd byte of param1
//		
//		Revision 1.3  2004/04/20 00:25:31  wgulland
//		Use AppleSMUDevice's reg property when calling AppleSMU
//		
//		Revision 1.2  2004/03/13 01:55:30  dirty
//		Stop treating temperature as special: don't shift it's value by 8-bits anymore.
//		
//		Revision 1.1  2004/02/17 20:44:34  wgulland
//		Add AppleSMUMonitor again
//		

#include "AppleSMUMonitor.h"
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>

OSDefineMetaClassAndStructors(AppleSMUMonitor, IOService);

bool AppleSMUMonitor::start(IOService *provider)
{
    OSData *data;
    if(!IOService::start(provider))
        return false;

    data = OSDynamicCast(OSData, provider->getProperty("reg"));
    if (!data) {
        IOLog("No reg property for AppleSMUDevice\n");
        return false;
    }
    
    fCommand = *(UInt32 *)data->getBytesNoCopy();
    
    fProvider = provider;
    
	fType = kUnknown;

	// Get device type
	if ( ( data = OSDynamicCast( OSData, provider->getProperty( "device_type" ) ) ) != NULL )
		{
		if ( strcmp( "batt-sensors", ( const char * ) data->getBytesNoCopy() ) == 0 )
			fType = kBATT_sensor;
		}
		
    // Create symbols for platform funcs
    fSMUGetSensor = OSSymbol::withCString("getSensor");
    fSMUGetControl = OSSymbol::withCString("getControl");
    fSMUSetControl = OSSymbol::withCString("setControl");
    fGetSensorValue = OSSymbol::withCString("getSensorValue");
    fGetCurrentValue = OSSymbol::withCString("getCurrentValue");
    fGetTargetValue = OSSymbol::withCString("getTargetValue");
    fSetTargetValue = OSSymbol::withCString("setTargetValue");
    
    // Publish device tree children into service plane
    // Keep track of children by reg property.
    OSIterator *iter;
    IOService *child;
    iter = provider->getChildIterator(gIODTPlane);
    if(!iter)
        return false;
    while(child = (IOService *)iter->getNextObject()) {
        child->attach(this);
        child->start(this);
        child->registerService();                                
    }
    iter->release();
    return true;
}

IOReturn AppleSMUMonitor::callPlatformFunction(const OSSymbol *functionName,
                                             bool waitForFunction, 
                                             void *param1, void *param2,
                                             void *param3, void *param4)
{
    IOReturn ret;
    void *command;
    // val32 allows 16.16 battery data to be passed to sensor logger...
    SInt16 val;
	SInt32 val32;
    
    if(functionName == fGetSensorValue) {
        // Add command value
        command = (void *)((UInt32)param1 | fCommand);
		
		if (fType == kBATT_sensor)
				{
				ret = fProvider->callPlatformFunction(fSMUGetSensor, waitForFunction, command, &val32, (void *)2, NULL);
				if (ret == kIOReturnSuccess) 
					*(SInt32*)param2 = val32;
				}

		else
			{
			ret = fProvider->callPlatformFunction(fSMUGetSensor, waitForFunction, command, &val, (void *)2, NULL);
			if (ret == kIOReturnSuccess) 
				*(SInt32*)param2 = val;
			}
        //IOLog("AppleSMUMonitor::GetSensorValue() command 0x%x read 0x%x\n", ((UInt32)param1 | fCommand), val);
        
        return ret;
    }
    else if(functionName == fGetCurrentValue) {
        // Add command value
        command = (void *)((UInt32)param1 | ((fCommand | kGetCurrent) << kCommandShift));
        //IOLog("AppleSMUMonitor::GetCurrentValue()\n");
        ret = fProvider->callPlatformFunction(fSMUGetControl, waitForFunction,
            command, &val, (void *)2, NULL);
        if(ret == kIOReturnSuccess)
            *(SInt32*)param2 = val;
        
        return ret;
    }
    else if(functionName == fGetTargetValue) {
        //IOLog("AppleSMUMonitor::GetTargetValue()\n");
        // Add command value
        command = (void *)((UInt32)param1 | ((fCommand | kGetTarget) << kCommandShift));
        ret = fProvider->callPlatformFunction(fSMUGetControl, waitForFunction,
            command, &val, (void *)2, NULL);
        if(ret == kIOReturnSuccess)
            *(SInt32*)param2 = val;
        return(ret);
    }
    else if(functionName == fSetTargetValue) {
        //IOLog("AppleSMUMonitor::SetTargetValue()\n");
        val = (SInt32)param2;
        // Add command value
        command = (void *)((UInt32)param1 | ((fCommand | kSetTarget) << kCommandShift));
        ret = fProvider->callPlatformFunction(fSMUSetControl, waitForFunction,
            command, &val, (void *)2, NULL);
        return ret;
    }
    else
        return IOService::callPlatformFunction(functionName, waitForFunction, 
                                             param1, param2, param3, param4);
}

