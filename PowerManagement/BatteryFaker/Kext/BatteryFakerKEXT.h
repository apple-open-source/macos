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

#ifndef __BatteryFaker__
#define __BatteryFaker__

#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>

#define kNumMaxBatteries        2
#define kUseNumBatteries        1

class BatteryFakerObject;

// The parent of all batteries
class BatteryFaker : public IOService {
    OSDeclareDefaultStructors(BatteryFaker)
    
protected:
    IOService                   *fProvider;
    BatteryFakerObject          *batteries[kNumMaxBatteries];
    
public:
    virtual bool start(IOService *provider);
    virtual void stop( IOService *provider );
    virtual IOReturn setProperties(OSObject *properties);
};


// Each individual battery
class BatteryFakerObject : public IOPMPowerSource {
    OSDeclareDefaultStructors(BatteryFakerObject)
    
public:
    static BatteryFakerObject  *fakerObject(int i);
    IOReturn    setBatteryProperties(OSDictionary *);
};

#endif
