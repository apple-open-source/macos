/*
 *  AppleSMUMonitor.h
 *
 *  Created by William Gulland on Mon Feb 16 2004.
 *  Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 */
//		$Log: AppleSMUMonitor.h,v $
//		Revision 1.4  2004/11/05 23:14:31  townsle1
//		
//		Version 1.3.5d1: Add AppleHWSensor personalities for ControlTypeALS and
//		SensorTypeALS (3642655).  Add kBatt_sensor fType to AppleSMUMonitor for
//		handling 32 bit values (16.16) returned from batt-sensors (3778115).
//		
//		Revision 1.3  2004/05/04 20:35:42  wgulland
//		Send command to SMU in 2nd and 3rd byte of param1
//		
//		Revision 1.2  2004/04/20 00:25:31  wgulland
//		Use AppleSMUDevice's reg property when calling AppleSMU
//		
//		Revision 1.1  2004/02/17 20:44:34  wgulland
//		Add AppleSMUMonitor again
//		
#ifndef _APPLESMUMONITOR_H
#define _APPLESMUMONITOR_H

#include <IOKit/IOService.h>
class AppleSMUMonitor : public IOService
{
    OSDeclareDefaultStructors(AppleSMUMonitor)

protected:
    enum DeviceType {
        kUnknown = 0,
        kTemperatureSensor = 1,
        kRPMFan = 2,
        kPWMFan = 3,
        kBATT_sensor = 4,
    };

    enum FanCommand {
        kSetTarget = 0,
        kGetCurrent = 1,
        kGetTarget = 2,
        kCommandShift = 8,
    };
    
    IOService *fProvider;
    const OSSymbol *fSMUGetSensor;
    const OSSymbol *fSMUGetControl;
    const OSSymbol *fSMUSetControl;
    const OSSymbol *fGetSensorValue;
    const OSSymbol *fGetCurrentValue;
    const OSSymbol *fGetTargetValue;
    const OSSymbol *fSetTargetValue;
    
    UInt32 fCommand;
    DeviceType fType;
    
public:
    IOReturn callPlatformFunction(const OSSymbol *functionName,
                                             bool waitForFunction, 
                                             void *param1, void *param2,
                                             void *param3, void *param4);
                                             
    
    bool start(IOService *provider);
};

#endif // _APPLESMUMONITOR_H