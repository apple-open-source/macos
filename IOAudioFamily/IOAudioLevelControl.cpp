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
    
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>

#define super IOAudioControl

OSDefineMetaClassAndStructors(IOAudioLevelControl, IOAudioControl)

IOAudioLevelControl *IOAudioLevelControl::create(UInt32 initialValue,
                                                 UInt32 minValue,
                                                 UInt32 maxValue,
                                                 IOFixed minDB,
                                                 IOFixed maxDB,
                                                 UInt32 channelID,
                                                 const char *channelName,
                                                 UInt32 cntrlID)
{
    IOAudioLevelControl *control;

    control = new IOAudioLevelControl;

    if (control) {
        if (!control->init(initialValue,
                           minValue,
                           maxValue,
                           minDB,
                           maxDB,
                           channelID,
                           channelName,
                           cntrlID)) {
            control->release();
            control = 0;
        }
    }

    return control;
}

bool IOAudioLevelControl::init(UInt32 initialValue,
                               UInt32 minValue,
                               UInt32 maxValue,
                               IOFixed minDB,
                               IOFixed maxDB,
                               UInt32 channelID,
                               const char *channelName,
                               UInt32 cntrlID,
                               OSDictionary *properties)
{
    if (!super::init(IOAUDIOCONTROL_TYPE_LEVEL, initialValue, channelID, channelName, cntrlID, properties)) {
        return false;
    }

    setMinValue(minValue);
    setMaxValue(maxValue);
    setMinDB(minDB);
    setMaxDB(maxDB);

    master = false;

    return true;
}
                   
void IOAudioLevelControl::setMinValue(UInt32 newMinValue)
{
    minValue = newMinValue;
    setProperty(IOAUDIOCONTROL_MIN_VALUE_KEY, newMinValue, sizeof(UInt32)*8);
}

UInt32 IOAudioLevelControl::getMinValue()
{
    return minValue;
}
    
void IOAudioLevelControl::setMaxValue(UInt32 newMaxValue)
{
    maxValue = newMaxValue;
    setProperty(IOAUDIOCONTROL_MAX_VALUE_KEY, newMaxValue, sizeof(UInt32)*8);
}

UInt32 IOAudioLevelControl::getMaxValue()
{
    return maxValue;
}
    
void IOAudioLevelControl::setMinDB(IOFixed newMinDB)
{
    minDB = newMinDB;
    setProperty(IOAUDIOCONTROL_MIN_DB_KEY, newMinDB, sizeof(IOFixed)*8);
}

IOFixed IOAudioLevelControl::getMinDB()
{
    return minDB;
}
    
void IOAudioLevelControl::setMaxDB(IOFixed newMaxDB)
{
    setProperty(IOAUDIOCONTROL_MAX_DB_KEY, newMaxDB, sizeof(IOFixed)*8);
}

IOFixed IOAudioLevelControl::getMaxDB()
{
    return maxDB;
}

void IOAudioLevelControl::setMaster(bool isMaster)
{
    master = isMaster;
    //setProperty(IOAUDIOCONTROL_MASTER_KEY, isMaster, 8);
}

bool IOAudioLevelControl::isMaster()
{
    return master;
}


