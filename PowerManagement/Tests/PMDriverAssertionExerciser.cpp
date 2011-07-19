/*
 * Copyright (c) 2009 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOReturn.h>
#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>

#include "PMDriverAssertionExerciser.h"

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>

#define kTestRepeatRateMS     15*1000

#define super IOService
OSDefineMetaClassAndStructors(PMDriverAssertionExerciser, IOService)

/* Returns false on catastrophic failure */
bool PMDriverAssertionExerciser::doGetAndSetTest(void)
{
    IOPMDriverAssertionLevel      _l = kIOPMDriverAssertionLevelOff;
    IOPMDriverAssertionLevel      _newLevel = kIOPMDriverAssertionLevelOff;
    IOReturn                        ret;
    
    _l = rootDomain->getPMAssertionLevel(kIOPMDriverAssertionCPUBit);
    
    IOLog("PMDriverAssertionExerciser:: Performing Set Assertion Level (current level=%d)", _l);    

    if (kIOPMDriverAssertionLevelOn == _l)
    {
        _newLevel = kIOPMDriverAssertionLevelOff;
    } else if (kIOPMDriverAssertionLevelOff == _l) {
        _newLevel = kIOPMDriverAssertionLevelOn;
    } else {
        IOLog("[FAIL] AssertionExerciser: Bogus - assertion level for 0x%04x is %d\n",
                    kIOPMDriverAssertionCPUBit, _l);
    }

    ret = rootDomain->setPMAssertionLevel(myAssertion, _newLevel);
    
    if (kIOReturnSuccess == ret)
    {
        IOLog("[PASS] IOPMrootDomain::setPMAssertionLevel %d returns kIOReturnSuccess\n", (uint32_t)_newLevel);
    } else {
        IOLog("[FAIL] IOPMrootDomain::setPMAssertionLevel %d error 0x%08x\n", (uint32_t)_newLevel, ret);
    }

    return true;
}

/* Returns true on catastrophic failure */
bool PMDriverAssertionExerciser::doCreateAndReleaseTest(void)
{
    IOPMDriverAssertionID       newAssertion;
    IOReturn                    ret;

    IOLog("PMDriverAssertionExerciser:: Performing Create & Release Test");
    
    newAssertion = rootDomain->createPMAssertion(kIOPMDriverAssertionCPUBit, kIOPMDriverAssertionLevelOn, this,
                                        "com.apple.debug.PMDriverAssertionExerciser.create-release");

    if (0 == myAssertion)
    {
        IOLog("[FAIL] IOPMrootDomain::createPMAssertion error returned NULL assertion");
        goto exit;
    }

    ret = rootDomain->releasePMAssertion(myAssertion);
    if (kIOReturnSuccess == ret) {
        IOLog("[PASS] Created and Released a PM Assertion\n");
    } else {
        IOLog("[FAIL] Created PM Assertion, but Release returned fail code 0x%08x\n", ret);
    }

exit:
    return true;
}

void PMDriverAssertionExerciser::PMDriverTimerAction(OSObject *gifted __unused, IOTimerEventSource *eventSource __unused)
{

    this->doGetAndSetTest();

    this->doCreateAndReleaseTest();

    // Let's do this again in a few seconds.
    myTimer->setTimeoutMS(kTestRepeatRateMS);
}

bool PMDriverAssertionExerciser::start( IOService *provider )
{
    IOPMDriverAssertionLevel  level = kIOPMDriverAssertionLevelOn;
    IOPMDriverAssertionType   keepCPU = kIOPMDriverAssertionCPUBit;
    IOReturn            ret;
    
    IOWorkLoop          *wl = NULL;

    rootDomain = getPMRootDomain();
    if(!rootDomain) 
        return false;

    myAssertion = rootDomain->createPMAssertion(keepCPU, level, this,
                "com.apple.debug.PMDriverAssertionExerciser.loop-on-off");
                
    if (0 == myAssertion)
    {
        IOLog("IOPMrootDomain::createPMAssertion error returned NULL assertion");
        return true;
    }
    
    if (!(wl = getWorkLoop()))
        return true;

    myTimer = IOTimerEventSource::timerEventSource(this, 
                OSMemberFunctionCast(IOTimerEventSource::Action, this, 
                    &PMDriverAssertionExerciser::PMDriverTimerAction));

    if (myTimer) {
        myTimer->setTimeoutMS(15*1000);
        wl->addEventSource(myTimer);
    }


    /*
     * join PM tree
     */
    PMinit();
    provider->joinPMtree(this);
    
    
    return true;
}

void PMDriverAssertionExerciser::stop( IOService *provider )
{
    IOReturn    ret;
    
    if (myAssertion)
        ret = rootDomain->releasePMAssertion(myAssertion);

    return;
}



