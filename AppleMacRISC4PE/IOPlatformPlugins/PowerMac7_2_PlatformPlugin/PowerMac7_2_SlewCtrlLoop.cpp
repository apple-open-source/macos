/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: PowerMac7_2_SlewCtrlLoop.cpp,v $
//		Revision 1.7  2003/07/20 23:41:11  eem
//		[3273577] Q37: Systems need to run at Full Speed during test
//		
//		Revision 1.6  2003/07/16 02:02:10  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.5  2003/06/25 02:16:25  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.4.4.3  2003/06/21 01:42:08  eem
//		Final Fan Tweaks.
//		
//		Revision 1.4.4.2  2003/06/20 09:07:37  eem
//		Added rising/falling slew limiters, integral clipping, etc.
//		
//		Revision 1.4.4.1  2003/06/20 01:40:01  eem
//		Although commented out in this submision, there is support here to nap
//		the processors if the fans are at min, with the intent of keeping the
//		heat sinks up to temperature.
//		
//		Revision 1.4  2003/06/07 01:30:58  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.3.2.5  2003/06/06 08:17:58  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.3.2.4  2003/05/31 08:11:38  eem
//		Initial pass at integrating deadline-based timer callbacks for PID loops.
//		
//		Revision 1.3.2.3  2003/05/29 03:51:36  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.3.2.2  2003/05/26 10:07:17  eem
//		Fixed most of the bugs after the last cleanup/reorg.
//		
//		Revision 1.3.2.1  2003/05/23 06:36:59  eem
//		More registration notification stuff.
//		
//		Revision 1.3  2003/05/21 21:58:55  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.2.2.2  2003/05/16 07:08:48  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.2.2.1  2003/05/14 22:07:55  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.2  2003/05/13 02:13:52  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.1.2.1  2003/05/12 11:21:12  eem
//		Support for slewing.
//		
//
//

#include <IOKit/IOLib.h>
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include <ppc/machine_routines.h>
#include "PowerMac7_2_SlewCtrlLoop.h"

#define super IOPlatformCtrlLoop
OSDefineMetaClassAndStructors(PowerMac7_2_SlewCtrlLoop, IOPlatformCtrlLoop)

//extern const OSSymbol * gPM72EnvShroudRemoved;
//extern const OSSymbol * gPM72EnvAllowNapping;

bool PowerMac7_2_SlewCtrlLoop::init( void )
{
	if (!super::init()) return(false);

	slewControl = NULL;

	return(true);
}

void PowerMac7_2_SlewCtrlLoop::free( void )
{
	if (slewControl)
	{
		slewControl->release();
		slewControl = NULL;
	}

	super::free();
}

IOReturn PowerMac7_2_SlewCtrlLoop::initPlatformCtrlLoop(const OSDictionary *dict)
{
	IOReturn status;
	OSArray * array;

	status = super::initPlatformCtrlLoop(dict);

	// the slew control's control id is in our control id array
	if ((array = OSDynamicCast(OSArray, dict->getObject(kIOPPluginThermalControlIDsKey))) == NULL)
	{
		CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::initPlatformCtrlLoop no control ID!!\n");
		status = kIOReturnError;
	}
	else
	{
		slewControl = OSDynamicCast(IOPlatformSlewClockControl, 
				platformPlugin->lookupControlByID( OSDynamicCast(OSNumber, array->getObject(0)) ));
		if (slewControl == NULL)
		{
			CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::initPlatformCtrlLoop bad control ID!!\n");
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

bool PowerMac7_2_SlewCtrlLoop::updateMetaState( void )
{
	bool intOverTemp, extOverTemp;
	//OSBoolean * shroudRemoved;
	const OSNumber * newMetaState;

	// we are in metastate 0 by default.  If any of the following are true, we are in metastate 1:
	//    internal overtemp
	//    external overtemp
	//    shroud removed

	intOverTemp = platformPlugin->envArrayCondIsTrue(gIOPPluginEnvInternalOvertemp);
	extOverTemp = platformPlugin->envArrayCondIsTrue(gIOPPluginEnvExternalOvertemp);
	//shroudRemoved = OSDynamicCast(OSBoolean, platformPlugin->getEnv(gPM72EnvShroudRemoved));

	//if (intOverTemp || extOverTemp || shroudRemoved == kOSBooleanTrue)
	if (intOverTemp || extOverTemp)
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

void PowerMac7_2_SlewCtrlLoop::adjustControls( void )
{
	// if meta state is 1, slew target is 1
	// if meta state is 0, slew target is equal to dynamic power step environment condition

	if (ctrlloopState == kIOPCtrlLoopNotReady)
	{
		CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::adjustControls slew controller not yet registered\n");
		return;
	}

	const OSNumber * curDPS = OSDynamicCast(OSNumber, platformPlugin->getEnv(gIOPPluginEnvDynamicPowerStep));
	//bool allowNapping = platformPlugin->envArrayCondIsTrue(gPM72EnvAllowNapping);
	const OSNumber * curTarget = slewControl->getTargetValue();
	const OSNumber * newTarget = gIOPPluginZero;

	if (!curDPS || !curTarget)
	{
		CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::adjustControls failed fetching params\n");
		return;
	}

/*
	// To Nap or Not to Nap?  That is the question.
	if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
	    lastAllowedNapping != allowNapping)
	{
		int i, nCpus;

		nCpus = platformPlugin->getConfig();	// config tells us the number of processors

		lastAllowedNapping = allowNapping;
		CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::adjustControls %s NAPPING\n",
				allowNapping ? "ENABLING" : "DISABLING");
		for (i=0; i<nCpus; i++)
		{
			(void) ml_enable_nap( i, allowNapping );
		}
	}
*/
	// this is an abbreviated technique of calculating the algorithm described above, basically it is
	// a truth table that says "if curMetaSate==1 OR curDPS==1 THEN newTarget<-1, ELSE newTarget<-0"
	if (getMetaState()->isEqualTo(gIOPPluginOne) || curDPS->isEqualTo(gIOPPluginOne))
	{
		newTarget = gIOPPluginOne;
	}

	//CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::adjustControls chose speed %u\n", newTarget->unsigned16BitValue());

	// if target value changed, send it down to the slew controller
	if (ctrlloopState == kIOPCtrlLoopFirstAdjustment ||
	    ctrlloopState == kIOPCtrlLoopDidWake ||
	    !curTarget->isEqualTo(newTarget))
	{
		//CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::adjustControls MESSAGING SLEW DRIVER\n");

		if (slewControl->sendTargetValue(newTarget))
		{
			//CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::adjustControls SLEW DRIVER NOTIFIED\n");
			slewControl->setTargetValue(newTarget);
			ctrlloopState = kIOPCtrlLoopAllRegistered;
		}
		else
		{
			CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::adjustControls FAILED MESSAGING SLEW DRIVER!!\n");
		}
	}
}

void PowerMac7_2_SlewCtrlLoop::controlRegistered( IOPlatformControl * aControl )
{
	CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::controlRegistered - entered\n");

	if (aControl == slewControl)
	{
		CTRLLOOP_DLOG("PowerMac7_2_SlewCtrlLoop::controlRegistered allRegistered!\n");

		ctrlloopState = kIOPCtrlLoopFirstAdjustment;
		// Something borks in the slew driver if we try to set it from its registration
		// thread.  Hopefully DSP will wait till after the slew driver registers to send
		// the speed setting down.
		adjustControls();
	}
}