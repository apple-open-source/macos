/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: RackMac3_1_SlewCtrlLoop.cpp,v 1.8 2004/12/03 23:19:46 raddog Exp $
 */


#include <IOKit/IOLib.h>
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include <machine/machine_routines.h>
#include "RackMac3_1_SlewCtrlLoop.h"

#define super IOPlatformCtrlLoop
OSDefineMetaClassAndStructors(RackMac3_1_SlewCtrlLoop, IOPlatformCtrlLoop)

extern const OSSymbol * gRM31EnableSlewing;

bool RackMac3_1_SlewCtrlLoop::init( void )
{
    if (!super::init()) return(false);

    slewControl = NULL;

    return(true);
}

void RackMac3_1_SlewCtrlLoop::free( void )
{
    if (slewControl)
    {
        slewControl->release();
        slewControl = NULL;
    }

    super::free();
}

IOReturn RackMac3_1_SlewCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
    IOReturn status;
    OSArray * array;

    status = super::initPlatformCtrlLoop(dict);

    // the slew control's control id is in our control id array
    if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL)
    {
        CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::initPlatformCtrlLoop no control ID!!\n");
        status = kIOReturnError;
    }
    else
    {
        slewControl = OSDynamicCast(IOPlatformSlewClockControl, 
                        platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(0)) ));
        if (slewControl == NULL)
        {
            CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::initPlatformCtrlLoop bad control ID!!\n");
            status = kIOReturnError;
        }
        else
        {
            slewControl->retain();
            addControl( slewControl );
        }
    }

    return(status);
}

bool RackMac3_1_SlewCtrlLoop::updateMetaState( void )
{
    bool intOverTemp, enableSlewing;
    const OSNumber * newMetaState;

    // we are in metastate 0 by default.  If any of the following are true, we are in metastate 1:
    //    internal overtemp
    //    external overtemp

    intOverTemp = platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp);
    enableSlewing = platformPlugin->envArrayCondIsTrue(gRM31EnableSlewing);

    if (intOverTemp || enableSlewing)
    {
        newMetaState = gIOPPluginOne;
    }
    else
    {
        newMetaState = gIOPPluginZero;
    }

    if (!newMetaState->isEqualTo(getMetaState()))
    {
        setMetaState( newMetaState );
        return(true);
    }
    else
    {
        return(false);
    }

}

void RackMac3_1_SlewCtrlLoop::adjustControls( void )
{
    // if meta state is 1, slew target is 1
    // if meta state is 0, slew target is equal to dynamic power step environment condition

    if (ctrlloopState == kIOPCtrlLoopNotReady)
    {
        CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::adjustControls slew controller not yet registered\n");
        return;
    }

	const OSNumber * curDPSNum = OSDynamicCast(OSNumber, platformPlugin->getEnv(gIOPPluginEnvDynamicPowerStep));
	//bool allowNapping = platformPlugin->envArrayCondIsTrue(gPM72EnvAllowNapping);
	ControlValue curTarget = slewControl->getTargetValue();
	ControlValue newTarget = 0x0;

	if (!curDPSNum)
    {
        CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::adjustControls failed fetching params\n");
        return;
    }

	ControlValue curDPS = curDPSNum->unsigned32BitValue();

    // this is an abbreviated technique of calculating the algorithm described above, basically it is
    // a truth table that says "if curMetaSate==1 OR curDPS==1 THEN newTarget<-1, ELSE newTarget<-0"
	if (getMetaState()->isEqualTo(gIOPPluginOne) || curDPS == 0x1 )
	{
		newTarget = 0x1;
	}

    //CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::adjustControls chose speed %u\n", newTarget->unsigned16BitValue());

    // if target value changed, send it down to the slew controller
    if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
        ctrlloopState == kIOPCtrlLoopDidWake ||
        curTarget != newTarget )
    {
        //CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::adjustControls MESSAGING SLEW DRIVER\n");

        if (slewControl->sendTargetValue(newTarget))
        {
            //CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::adjustControls SLEW DRIVER NOTIFIED\n");
            slewControl->setTargetValue(newTarget);
            ctrlloopState = kIOPCtrlLoopAllRegistered;
        }
        else
        {
            CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::adjustControls FAILED MESSAGING SLEW DRIVER!!\n");
        }
    }
}

void RackMac3_1_SlewCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
    //CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::controlRegistered - entered\n");

	if( slewControl->isRegistered() == kOSBooleanTrue )
    {
        //CTRLLOOP_DLOG("RackMac3_1_SlewCtrlLoop::controlRegistered allRegistered!\n");

        ctrlloopState = kIOPCtrlLoopFirstAdjustment;
        // Something borks in the slew driver if we try to set it from its registration
        // thread.  Hopefully DSP will wait till after the slew driver registers to send
        // the speed setting down.
        adjustControls();
    }
}
