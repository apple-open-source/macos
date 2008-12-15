/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 */
//		$Log: AppleHWControl.cpp,v $
//		Revision 1.7  2008/04/18 23:25:31  raddog
//		<rdar://problem/5828356> AppleHWSensor - control code needs to deal with endian issues
//		
//		Revision 1.6  2007/05/07 18:29:38  pmr
//		change acknowledgePowerChange to acknowledgeSetPowerState
//		
//		Revision 1.5  2004/06/03 21:32:07  eem
//		[3671325] Remember control's "safe-value" property
//		
//		Revision 1.4  2004/04/21 21:13:39  dirty
//		If max-value or min-value properties exist in the device tree node, then add
//		them to the IOHWControl registration dictionary.
//		
//		Revision 1.3  2004/02/12 01:17:01  eem
//		Merge Rohan changes from tag MERGED-FROM-rohan-branch-TO-TOT-1
//		
//		Revision 1.2  2004/01/30 23:52:00  eem
//		[3542678] IOHWSensor/IOHWControl should use "reg" with version 2 thermal parameters
//		Remove AppleSMUSensor/AppleSMUFan since that code will be in AppleSMUDevice class.
//		Fix IOHWMonitor, AppleMaxim6690, AppleAD741x to use setPowerState() API instead of
//		unsynchronized powerStateWIllChangeTo() API.
//		
//		Revision 1.1.4.1  2004/02/10 09:58:00  eem
//		3548562, 3554178 - prevent extra OSNumber allocations
//		
//		Revision 1.1  2003/10/23 20:08:18  wgulland
//		Adding IOHWControl and a base class for IOHWSensor and IOHWControl
//		
//		

#include <sys/cdefs.h>

#include <IOKit/IODeviceTreeSupport.h>
#include "AppleHWControl.h"

static const OSSymbol *sControlID;
static const OSSymbol *sCurrentValue;
static const OSSymbol *sGetCurrentValue;
static const OSSymbol *sTargetValue;
static const OSSymbol *sGetTargetValue;
static const OSSymbol *sSetTargetValue;
static const OSSymbol *sForceUpdate;
static const OSSymbol *sMinValue;
static const OSSymbol *sMaxValue;
static const OSSymbol *sSafeValue;

OSDefineMetaClassAndStructors(IOHWControl,IOHWMonitor)

bool IOHWControl::start(IOService *provider)
{
	OSDictionary *dict;
	IOReturn ret;

    if ( !(IOHWMonitor::start(provider)) )
        return false;

	DLOG("IOHWControl::start(%s) - entered\n", fDebugID);

	// initialize symbols
    if(!sControlID)
        sControlID = OSSymbol::withCString("control-id");
    if(!sCurrentValue)
        sCurrentValue = OSSymbol::withCString("current-value");
    if(!sGetCurrentValue)
        sGetCurrentValue = OSSymbol::withCString("getCurrentValue");
    if(!sTargetValue)
        sTargetValue = OSSymbol::withCString("target-value");
    if(!sGetTargetValue)
        sGetTargetValue = OSSymbol::withCString("getTargetValue");
    if(!sSetTargetValue)
        sSetTargetValue = OSSymbol::withCString("setTargetValue");
	if(!sForceUpdate)
		sForceUpdate = OSSymbol::withCString("force-update");
	if(!sMinValue)
		sMinValue = OSSymbol::withCString("min-value");
	if(!sMaxValue)
		sMaxValue = OSSymbol::withCString("max-value");
	if(!sSafeValue)
		sSafeValue = OSSymbol::withCString("safe-value");

    // Copy over the properties from our provider

	DLOG("IOHWControl::start(%s) - parsing Control nub properties\n", fDebugID);

	bool parseSuccess = FALSE;

	OSNumber *num;
	OSData *data;

	do {

		// Control id - required
		data = OSDynamicCast(OSData, provider->getProperty(sControlID));
		if (!data)
#if defined( __ppc__ )
		{
			IOLog("IOHWControl - no Control ID !!\n");
			break;
		}

        fID = *(UInt32 *)data->getBytesNoCopy();
#else
		// if (!data)
		{
			if (num = OSDynamicCast (OSNumber, provider->getProperty(sControlID)))
				fID = num->unsigned32BitValue();
			else {
				IOLog("IOHWControl - no Control ID !!\n");
				break;
			}
		} else {
			fID = OSReadBigInt32(data->getBytesNoCopy(), 0);
		}
#endif
       num = OSNumber::withNumber(fID, 32);
		if (!num)
		{
			IOLog("IOHWControl - can't set Control ID !!\n");
			break;
		}

		setProperty(sControlID, num);
		num->release();

		// If min-value/max-value/safe-value exist, add them as properties.

		if ( ( data = OSDynamicCast(OSData, provider->getProperty( sMinValue ) ) ) != NULL )
			{
			UInt32					minValue;

			minValue = *( UInt32 * ) data->getBytesNoCopy();
			if ( ( num = OSNumber::withNumber( minValue, 32 ) ) == NULL )
				break;

			setProperty( sMinValue, num );
			num->release();
			}
#if !defined( __ppc__ )
			else	// !data
			{
			if ( ( num = OSDynamicCast(OSNumber, provider->getProperty( sMinValue ) ) ) != NULL )
				{
				UInt32					minValue;

				minValue = num->unsigned32BitValue();
				if ( ( num = OSNumber::withNumber( minValue, 32 ) ) == NULL )
					break;

				setProperty( sMinValue, num );
				num->release();
				}
			}
#endif

		if ( ( data = OSDynamicCast(OSData, provider->getProperty( sMaxValue ) ) ) != NULL )
			{
			UInt32					maxValue;

			maxValue = *( UInt32 * ) data->getBytesNoCopy();
			if ( ( num = OSNumber::withNumber( maxValue, 32 ) ) == NULL )
				break;

			setProperty( sMaxValue, num );
			num->release();
			}
#if !defined( __ppc__ )
			else 
			{
			if ( ( num = OSDynamicCast(OSNumber, provider->getProperty( sMaxValue ) ) ) != NULL )
				{
				UInt32					maxValue;

				maxValue = num->unsigned32BitValue();
				if ( ( num = OSNumber::withNumber( maxValue, 32 ) ) == NULL )
					break;

				setProperty( sMaxValue, num );
				num->release();
				}
			}
#endif

		if ( ( data = OSDynamicCast(OSData, provider->getProperty( sSafeValue ) ) ) != NULL )
			{
			UInt32					safeValue;

			safeValue = *( UInt32 * ) data->getBytesNoCopy();
			if ( ( num = OSNumber::withNumber( safeValue, 32 ) ) == NULL )
				break;

			setProperty( sSafeValue, num );
			num->release();
			}
#if !defined( __ppc__ )
			else 
			{
			if ( ( num = OSDynamicCast(OSNumber, provider->getProperty( sSafeValue ) ) ) != NULL )
				{
				UInt32					safeValue;

				safeValue = num->unsigned32BitValue();
				if ( ( num = OSNumber::withNumber( safeValue, 32 ) ) == NULL )
					break;

				setProperty( sSafeValue, num );
				num->release();
				}
			}
#endif

		// set the channel id
		// If we've got version 1 encoding, channel id is sensor id
		// if we've got version 2 encoding, channel id is reg property
		num = (OSNumber *)getProperty("version");
		if (num->unsigned32BitValue() == 1)
		{
			fChannel = fID;
		}
		else if (num->unsigned32BitValue() == 2)
		{
			data = OSDynamicCast(OSData, provider->getProperty("reg"));
			if (!data) 
#if defined( __ppc__ )
			{
				IOLog("IOHWControl - version 2 but no reg property !!\n");
				break;
			}

			fChannel = *(UInt32 *)data->getBytesNoCopy();
#else
			// if (!data)
			{
				if (num = OSDynamicCast (OSNumber, provider->getProperty("reg")))
					fChannel = num->unsigned32BitValue();
				else {
					IOLog("IOHWControl - version 2 but no reg property !!\n");
					break;
				}
			} else {
				fChannel = OSReadBigInt32(data->getBytesNoCopy(), 0);
			}
#endif
		}
		else	// version != 2
		{
			IOLog("IOHWControl - unrecognized version %d !!\n", num->unsigned32BitValue());
			break;
		}

		// all required properties found
		parseSuccess = TRUE;


	} while (0);

	if (parseSuccess == FALSE)
	{
		return(false);
	}

	DLOG("IOHWControl::start(%s) - polling initial current value\n", fDebugID);

    ret = updateCurrentValue();

    if(ret != kIOReturnSuccess)
	{
		IOLog("IOHWControl::start failed to read initial current value: 0x%x!\n", ret);

		return(false);
	}
    ret = updateTargetValue();
    if(ret != kIOReturnSuccess)
	{
		IOLog("IOHWControl::start failed to read initial target value!\n");

		return(false);
	}

	DLOG("IOHWControl::start(%s) - messaging pmon\n", fDebugID);

	// create the registration message dictionary
    dict = OSDictionary::withDictionary(getPropertyTable());

    if(fIOPMon)
        messageClient(kRegisterControl, fIOPMon, dict);
    dict->release();
    
    // make us findable by applications
    registerService();
    
	DLOG("IOHWControl::start(%s) - done\n", fDebugID);
    return true;
}

    // Override to allow setting of some properties
    // A dictionary is expected, containing the new properties
IOReturn IOHWControl::setProperties( OSObject * properties )
{
    OSDictionary * dict = OSDynamicCast(OSDictionary, properties);
    OSNumber *num;
    
    if(!dict)
        return kIOReturnBadArgument;
    if (sleeping)
		return(kIOReturnOffline);
    if(dict->getObject(sCurrentValue) || dict->getObject(sForceUpdate))
        updateCurrentValue();
    num = OSDynamicCast(OSNumber, dict->getObject(sTargetValue));
    if(num)
        setTargetValue(num);
    return kIOReturnSuccess;
}

IOReturn IOHWControl::updateCurrentValue()
{
    return updateValue(sGetCurrentValue, sCurrentValue);
}

IOReturn IOHWControl::updateTargetValue()
{
    return updateValue(sGetTargetValue, sTargetValue);
}

IOReturn IOHWControl::setTargetValue(OSNumber *val)
{
    IOReturn ret;

	/*
	 * If the system is going to sleep or waking from sleep, do nothing - leave the value unchanged
	 *
	 * This prevents untimely I/O during sleep/wake
	 *
	 * NOTE for Intel Systems - we capture the values even during sleep so we can restore them on wake
	 */
#if !defined( __ppc__ )
	lastValue = val->unsigned32BitValue();
#endif	
         
	if (sleeping || systemIsRestarting)
        return kIOReturnOffline;
            
	busy = true;
    ret = callPlatformFunction(sSetTargetValue, FALSE, 
                                    (void *)fChannel, (void *)val->unsigned32BitValue(), NULL, NULL);
	busy = false;
	
	/*
	 * If PM called us while we were reading the sensor, then sleeping may now be true
	 * in which case we have to ack the PM so it knows we can sleep.
	 */
	if (sleeping && powerPolicyMaker)
		powerPolicyMaker->acknowledgeSetPowerState ();
		
    if(ret == kIOReturnSuccess)
        setNumber(sTargetValue, val->unsigned32BitValue());
        
    return ret;
}

#if !defined( __ppc__ )
IOReturn IOHWControl::setPowerState(unsigned long whatState, IOService *policyMaker)
{
	IOReturn ret;
	bool goingToSleep;
	
	ret = IOHWMonitor::setPowerState (whatState, policyMaker);
	
	goingToSleep = (whatState == kIOHWMonitorOffState);
	
	if ((ret == IOPMAckImplied) && !goingToSleep) {
		// On wake re-send most recent target value;
		OSNumber *lastNum;
		
		lastNum = OSNumber::withNumber (lastValue, 32);
		if (lastNum) {
			DLOG ("IOHWControl::setPowerState - resending target value 0x%x on wake\n", lastNum->unsigned32BitValue());
			setTargetValue (lastNum);
		}
	}
	
	return ret;
}
#endif
