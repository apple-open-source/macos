/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include "BatteryFakerKEXT.h"

static const OSSymbol *fake_batt_dict_sym = 
             OSSymbol::withCString("Battery Properties");

#define super IOService
OSDefineMetaClassAndStructors(BatteryFaker,IOService)

/******************************************************************************
 * BatteryFakerKEXT::start
 *
 ******************************************************************************/

bool BatteryFaker::start(IOService *provider)
{
    int i;
    
    if( !provider || !super::start( provider ) ) {
        return false;
    }
    
    fProvider = provider;
    
    bzero(&batteries, sizeof(batteries));
    
    for(i=0; i<kUseNumBatteries; i++)
    {
        batteries[i] = BatteryFakerObject::fakerObject( i );
        if(NULL == batteries[i]) {
            return false;
        }
        batteries[i]->attach(this);
        batteries[i]->start(this);
        batteries[i]->setProperty("AppleSoftwareSimulatedBattery", kOSBooleanTrue);       
        batteries[i]->registerService(0);
    }
    
    this->registerService(0);
    return true;
}

void BatteryFaker::stop( IOService *provider )
{
    int i;
    
    for(i=0; i<kUseNumBatteries; i++)
    {
        if(batteries[i]) {
            batteries[i]->detach(this);
            batteries[i]->release();
        }
    }

    super::stop(provider);
    
    IOLog("BatteryFaker unloading.\n");
    
    return;
}

IOReturn BatteryFaker::setProperties(OSObject *arg_props)
{
    OSDictionary        *dict = OSDynamicCast(OSDictionary, arg_props);
    OSDictionary        *d;
    // Some user space application has specified the faked out battery state
    // we trust that this dictionary is valid & complete, and we automatically
    // set it as our state and signal an update.

    if(fake_batt_dict_sym 
       && (d = OSDynamicCast(OSDictionary, dict->getObject(fake_batt_dict_sym)) ))
    {
        batteries[0]->setBatteryProperties(d);
    }
    return kIOReturnSuccess;
}


/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 ******************************************************************************/

/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 ******************************************************************************/


#undef super
#define super IOPMPowerSource
OSDefineMetaClassAndStructors(BatteryFakerObject, IOPMPowerSource)


/******************************************************************************
 * BatteryFakerObject::start
 *
 ******************************************************************************/

BatteryFakerObject  *BatteryFakerObject::fakerObject(int i)
{
    BatteryFakerObject  *ret_obj = NULL;
    
    ret_obj = new BatteryFakerObject;

    if(ret_obj && !ret_obj->init())
    {
        ret_obj->release();
        return NULL;
    }
    return ret_obj;
}

IOReturn BatteryFakerObject::setBatteryProperties(OSDictionary *d)
{
    if(!d) {
        // An empty dictionary indicates we should disappear ourselves.
        // We should pretend this battery no longer exists, so we should
        // remove properties.
        
        // TODO: remove properties.
        return 0;
    }
    
    setProperty("PropertiesDict", d);

    d = OSDictionary::withDictionary(d);

    if(properties) {
        properties->release();
    }
    properties = d;
    
    // And trigger the update with the new fake dictionary
    this->settingsChangedSinceUpdate = true;
    this->updateStatus();
    
    return 0;
}



