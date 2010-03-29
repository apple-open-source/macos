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
//		$Log: AppleHWSensor.cpp,v $
//		Revision 1.24  2007/03/16 21:40:09  raddog
//		[5056773]IOHWMonitor::updateValue() may call callPlatformFunction with NULL key
//		
//		Revision 1.23  2005/11/11 21:13:11  ialigaen
//		Fixed endian problems w/ HWSensor when reading properties for the IOHWSensor class.
//		
//		Revision 1.22  2005/04/27 17:43:27  mpontil
//		When failing to read a sensor value (and so failing to start) print the
//		error code too.
//		
//		Revision 1.21  2005/03/01 01:02:53  raddog
//		Bug #4019053 (Kernel Panic on Q54 netboot): Add retries when we get a failed sensor reading in start routine and updateValue
//		
//		Revision 1.20  2004/08/05 23:54:19  eem
//		Merge in PR-3738382 branch to TOT.
//		
//		Revision 1.19.2.1  2004/08/05 22:14:17  eem
//		<rdar://problem/3738382> Eliminate field accesses of AbsoluteTime values.
//		
//		Revision 1.19  2004/07/26 16:25:20  eem
//		Merge Strider changes from AppleHWSensor-130_1_2 to TOT. Bump version to
//		1.3.0a2.
//		
//		Revision 1.18.2.1  2004/07/23 22:18:17  eem
//		<rdar://problem/3725357> Cancel pending thread callouts across sleep
//		<rdar://problem/3737220> Move setTimeout() outside of setPollingPeriod()
//		and setPollingPeriodNS() to prevent thread callout from being scheduled
//		prematurely.
//		
//		Revision 1.18  2004/01/30 23:52:00  eem
//		[3542678] IOHWSensor/IOHWControl should use "reg" with version 2 thermal parameters
//		Remove AppleSMUSensor/AppleSMUFan since that code will be in AppleSMUDevice class.
//		Fix IOHWMonitor, AppleMaxim6690, AppleAD741x to use setPowerState() API instead of
//		unsynchronized powerStateWIllChangeTo() API.
//		
//		Revision 1.17  2004/01/28 22:03:22  wgulland
//		Fix for cpus with sensor with no NS polling
//		
//		Revision 1.16  2004/01/27 02:37:05  tsherman
//		3540596 - AppleHWSensor's setTimeOut() method has erroneous conditional
//		
//		Revision 1.15  2003/12/02 02:02:28  tsherman
//		3497295 - Q42 Task - AppleHWSensor's AppleLM8x (NEW) driver needs to be revised to comply with Thermal API
//		
//		Revision 1.14  2003/10/23 20:08:18  wgulland
//		Adding IOHWControl and a base class for IOHWSensor and IOHWControl
//		
//		Revision 1.13  2003/08/22 19:57:31  eem
//		3387753 IOHWSensor remove PMinit() and joinPMtree(), bump version to 1.0.4b2
//		
//		Revision 1.12  2003/08/12 01:23:30  wgulland
//		Add code to handle notification via phandle in notify-xxx property
//		
//		Revision 1.11  2003/07/14 22:26:42  tsherman
//		3321185 - Q37: AppleHWSensor needs to stop polling at restart/shutdown
//		
//		Revision 1.10  2003/07/02 22:25:37  dirty
//		Fix warnings.
//		
//		Revision 1.9  2003/06/17 20:01:55  raddog
//		[3292803] Disable sensor reading across sleep
//		

#include <sys/cdefs.h>

__BEGIN_DECLS
#include <kern/thread_call.h>
__END_DECLS

#include <IOKit/IODeviceTreeSupport.h>
#include "AppleHWSensor.h"

static const OSSymbol *sSensorID;
static const OSSymbol *sLowThreshold;
static const OSSymbol *sHighThreshold;
static const OSSymbol *sPollingPeriod;
static const OSSymbol *sPollingPeriodNS;
static const OSSymbol *sCurrentValue;
static const OSSymbol *sGetSensorValue;
static const OSSymbol *sThreshold;
static const OSSymbol *sTempSensor;
static const OSSymbol *sSensorType;
static const OSSymbol *sForceUpdate;

OSDefineMetaClassAndStructors(IOHWSensor,IOHWMonitor)

#ifdef ENABLENOTIFY
static IORegistryEntry *objFromHandle(UInt32 search)
{
    const OSSymbol *handleSym = NULL;
    OSIterator *iter = NULL;
    IORegistryEntry *found = NULL;
    
    do {
        IORegistryEntry *obj;
        handleSym = OSSymbol::withCString("AAPL,phandle");
        if(!handleSym)
            break;
        iter = IORegistryIterator::iterateOver(gIODTPlane, kIORegistryIterateRecursively);
        if(!iter)
            break;
        while(obj = (IORegistryEntry *)iter->getNextObject())
        {
            OSData *han;
            han = OSDynamicCast(OSData, obj->getProperty(handleSym));
            if(han)
            {
                UInt32 *handle = (UInt32 *)han->getBytesNoCopy();
                if(*handle == search)
                {
                    found = obj;
                    break;
                }
            }
        }
    } while (false);
    if(handleSym)
        handleSym->release();
    if(iter)
        iter->release();
        
    return found;
}
#endif

void IOHWSensor::timerCallback(void *self)
{
    IOHWSensor * me = OSDynamicCast(IOHWSensor, (OSMetaClassBase *) self);
    
	//IOLog ("IOHWSensor::timeout\n");
	if (me)
	{
		me->updateCurrentValue();
		me->setTimeout();
	}
}

bool IOHWSensor::start(IOService *provider)
{
	OSDictionary *dict;
	bool doPolling;
	IOReturn ret;
	SInt32 val, retryCount;

	// initialize symbols
    if(!sSensorID)
        sSensorID = OSSymbol::withCString("sensor-id");
    if(!sLowThreshold)
        sLowThreshold = OSSymbol::withCString("low-threshold");
    if(!sHighThreshold)
        sHighThreshold = OSSymbol::withCString("high-threshold");
    if(!sThreshold)
        sThreshold = OSSymbol::withCString("threshold-value");
    if(!sPollingPeriod)
        sPollingPeriod = OSSymbol::withCString("polling-period");
    if(!sPollingPeriodNS)
        sPollingPeriodNS = OSSymbol::withCString("polling-period-ns");
    if(!sCurrentValue)
        sCurrentValue = OSSymbol::withCString("current-value");
    if(!sGetSensorValue)
        sGetSensorValue = OSSymbol::withCString("getSensorValue");
    if(!sTempSensor)
        sTempSensor = OSSymbol::withCString("temp-sensor");
    if(!sSensorType)
        sSensorType = OSSymbol::withCString("sensor-type");
	if(!sForceUpdate)
		sForceUpdate = OSSymbol::withCString("force-update");

    if ( !(IOHWMonitor::start(provider)) )
        return false;

	DLOG("IOHWSensor::start(%s) - entered\n", fDebugID);

    // allocate the thread callout
    fCalloutEntry = thread_call_allocate((thread_call_func_t) IOHWSensor::timerCallback,
                                                 (thread_call_param_t) this);

	if (fCalloutEntry == NULL)
	{
		IOLog("IOHWSensor::start failed to allocate thread callout\n");
		return(false);
	}

    fLowThreshold = kNoThreshold;
    fHighThreshold = kNoThreshold;

    // Copy over the properties from our provider

	DLOG("IOHWSensor::start(%s) - parsing sensor nub properties\n", fDebugID);

	bool parseSuccess = FALSE;

	OSNumber *num;
	OSData *data;

	doPolling = false;
	
	do {

		// sensor id - required
		data = OSDynamicCast(OSData, provider->getProperty(sSensorID));
		if (!data)
		{
			DLOG("IOHWSensor - no Sensor ID !!\n");
			break;
		}
		// Changed this to OSReadBigInt32 from *(UInt32 *) to take care of endian problems
        fID = OSReadBigInt32(data->getBytesNoCopy(),0);
        num = OSNumber::withNumber(fID, 32);
		if (!num)
		{
			IOLog("IOHWSensor - can't set Sensor ID !!\n");
			break;
		}

		setProperty(sSensorID, num);
		num->release();

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
			if (!data) break;
			// Changed this to OSReadBigInt32 from *(UInt32 *) to take care of endian problems
			fChannel = OSReadBigInt32(data->getBytesNoCopy(),0);
		}
		else
		{
			IOLog("IOHWSensor - version 2 but no reg property !!\n");
			break;
		}

		// all required properties found
		parseSuccess = TRUE;

		// IOHWSensor property: "polling-period", ""polling-period-ns"
		// Description: Polling period of sensor node
		// Input Format: OSData
		// Output Format: OSNumber(s)
		// Required: No
		fPollingPeriod = kNoPolling;
		fPollingPeriodNS = kNoPolling;
		
		data = OSDynamicCast(OSData, provider->getProperty(sPollingPeriod));
		if(data)
		{
			unsigned int length = (data->getLength() / sizeof(UInt32));
	
			if (length == 2)
			{
				// Changed this to OSReadBigInt32 from *(UInt32 *) to take care of endian problems
				fPollingPeriodNS = OSReadBigInt32(data->getBytesNoCopy(),4);	// get the second UInt32
				num = OSNumber::withNumber(fPollingPeriodNS, 32);
				if(num)
				{
					setPollingPeriodNS(num);
					num->release();
				}
			}

			// Changed this to OSReadBigInt32 from *(UInt32 *) to take care of endian problems
			fPollingPeriod = OSReadBigInt32(data->getBytesNoCopy(),0);	// get the first UInt32
			num = OSNumber::withNumber(fPollingPeriod, 32);

			if(num)
			{
				setPollingPeriod(num);
				num->release();
			}

			doPolling = ((fPollingPeriod != kNoPolling) || (fPollingPeriodNS != kNoPolling));
		}
	} while (0);

	if (parseSuccess == FALSE)
	{
		if (fCalloutEntry)
		{
			thread_call_cancel (fCalloutEntry);
			thread_call_free (fCalloutEntry);
		}
		fCalloutEntry = NULL;

		return(false);
	}

#ifdef ENABLENOTIFY
/*
 * Look for a node that wants to be notified of the sensor value.
 * This is specified in OpenFirmware by a property whose name starts with notify-,
 * and whose value is an OSData consisting of two UInt32s, the first one is the pHandle of
 * the node to notify, the second is the sensor ID, which must match fID.
 *
 * The property in the notified node is the same as the notify property, with the initial 'notify-'
 * replaced with 'AAPL,'
 *
 * To find notify-xxx properties we climb the service plane until we find an ancestor in the device
 * tree plane, then we search its properties for ones beginning with notify-.
 */
    do
    {
        IORegistryEntry *notify_ancestor = provider;
        OSDictionary *dict;
        char *buf;
        while(!notify_ancestor->inPlane(gIODTPlane))
            notify_ancestor = notify_ancestor->getParentEntry(gIOServicePlane);
        
        dict = notify_ancestor->dictionaryWithProperties();
        if(!dict)
            break;
            
        // No way to iterate over the dictionary's keys, so serialize and search that.
        OSSerialize *serial = OSSerialize::withCapacity(1000);
        if(!serial)
        {
            dict->release();
            break;
        }
        dict->serialize(serial);
        buf = serial->text();
        while(*buf)
        {
            if(strncmp("<key>notify-", buf, 12)==0)
            {
                char notify[128];
                char key[135]; // 128 + strlen("notify-")
                int len;
                buf += 12;
                len = 0;
                while ((*buf != '<') && (*buf != 0) && (len < 128))
                    notify[len++] = *buf++;
                notify[len] = 0;
                strcpy(key, "notify-");
                strcat(key, notify);
                OSData *obj = OSDynamicCast(OSData, dict->getObject(key));
                if(obj)
                {
                    UInt32 *ptr = (UInt32*)obj->getBytesNoCopy();
                    if((UInt32)ptr[1] == fID)
                    {
                        fNotifyObj = objFromHandle(ptr[0]);
                        strcpy(key, "AAPL,");
                        strcat(key, notify);
                        fNotifySym = OSSymbol::withCString(key);
                    }
                }
            }
            buf++;
        }
        serial->release();
        dict->release();
    }
    while (false);
 #endif
        
	DLOG("IOHWSensor::start(%s) - polling initial sensor value\n", fDebugID);

	/*
	 * [4019053] We sometimes, early in boot, fail to get a sensor reading
	 * This corrects itself pretty quickly so we just retry a few times
	 * After that, if it still isn't responding it's probably not there
	 * and we bail.
	 */
	retryCount = 0;
	do {
		ret = callPlatformFunction(sGetSensorValue, FALSE, (void *)fChannel, &val, NULL, NULL);
		if (ret == kIOReturnSuccess)
			break;		// Proceed
		
		IOSleep (250);	// Sleep and try again
		retryCount++;
	} while (retryCount < 10);

	//IOLog("read sensor %d, ret %x value is %d\n", fID, ret, val);
    if(ret != kIOReturnSuccess)
	{
		IOLog("AppleHWSensor::start failed to read initial sensor value! Got error %08lx\n", (UInt32)ret);

		if (fCalloutEntry)
		{
			thread_call_cancel (fCalloutEntry);
			thread_call_free (fCalloutEntry);
		}
		fCalloutEntry = NULL;

		return(false);
	}

	DLOG("IOHWSensor::start(%s) - messaging pmon\n", fDebugID);

	// create the registration message dictionary
    dict = OSDictionary::withCapacity(3);

    num = OSNumber::withNumber(fID, 32);
    dict->setObject(sSensorID, num);
    num->release();

    num = OSNumber::withNumber(val, 32);
	setProperty(sCurrentValue, num);	// save the current value while we're at it
    dict->setObject(sCurrentValue, num);
    num->release();
    
	// XXX
	// XXX This shouldn't always be a temperature sensor, but I don't want to change it
	// XXX for fear of breaking something...
	// XXX
    dict->setObject(sSensorType, sTempSensor);

    if(fIOPMon)
        messageClient(kRegisterSensor, fIOPMon, dict);
    dict->release();
    
    // make us findable by applications
    registerService();
	
	fInited = true;		// Good to go

	// Fire off timer at very end when everything is ready
	if (doPolling) setTimeout();

	DLOG("IOHWSensor::start(%s) - done\n", fDebugID);
    return true;
}

void IOHWSensor::stop(IOService *provider)
{
    if (fCalloutEntry) {
        thread_call_cancel (fCalloutEntry);
        thread_call_free (fCalloutEntry);
    }
    fCalloutEntry = NULL;
}

    // Override to allow setting of some properties
    // A dictionary is expected, containing the new properties
IOReturn IOHWSensor::setProperties( OSObject * properties )
{
    OSDictionary * dict = OSDynamicCast(OSDictionary, properties);
    OSNumber *num, *sec, *nsec;
    if(!dict)
        return kIOReturnBadArgument;
    
    num = OSDynamicCast(OSNumber, dict->getObject(sLowThreshold));
    if(num)
        setLowThreshold(num);

    num = OSDynamicCast(OSNumber, dict->getObject(sHighThreshold));
    if(num)
        setHighThreshold(num);

    sec = OSDynamicCast(OSNumber, dict->getObject(sPollingPeriod));
    if(sec)
        setPollingPeriod(sec);

    nsec = OSDynamicCast(OSNumber, dict->getObject(sPollingPeriodNS));
    if(nsec)
        setPollingPeriodNS(nsec);

	if ( sec || nsec )
		setTimeout();

    if(dict->getObject(sCurrentValue) || dict->getObject(sForceUpdate))
        updateCurrentValue();
        
    return kIOReturnSuccess;
}

SInt32 IOHWSensor::updateCurrentValue()
{
    SInt32 val;
    OSNumber *num;
    IOReturn ret;

	/*
	 * [4019053] We sometimes, early in boot, fail to get a sensor reading and no
	 * current value property gets set.  This corrects itself pretty quickly so we
	 * just retry until a good value gets set
	 */
	do {
    	ret = updateValue(sGetSensorValue, sCurrentValue);
		if (num = (OSNumber *)getProperty(sCurrentValue))
			break;
		
		IOSleep (250);	// Sleep and try again
	} while (1);
    
#ifdef ENABLENOTIFY
    if(fNotifyObj && fNotifySym)
        fNotifyObj->setProperty(fNotifySym, num);
#endif

    val = num->unsigned32BitValue();
    if(fLowThreshold != kNoThreshold && val <= fLowThreshold)
        sendMessage(kLowThresholdHit, val, fLowThreshold);
        
    if(fHighThreshold != kNoThreshold && val >= fHighThreshold)
       sendMessage(kHighThresholdHit, val, fHighThreshold);
       
    return val;
}

void IOHWSensor::sendMessage(UInt32 msg, SInt32 val, SInt32 threshold)
{
    OSDictionary *dict;
    OSNumber *num;
    
    //IOLog("Sensor %d sending message %d\n", fID, msg);
    
    dict = OSDictionary::withCapacity(3);
    
    num = OSNumber::withNumber(fID, 32);
    dict->setObject(sSensorID, num);
    num->release();
    
    num = OSNumber::withNumber(val, 32);
    dict->setObject(sCurrentValue, num);
    num->release();
    
    num = OSNumber::withNumber(threshold, 32);
    dict->setObject(sThreshold, num);
    num->release();
    
    if(fIOPMon)
        messageClient(msg, fIOPMon, dict);
    dict->release();
}

void IOHWSensor::setLowThreshold(OSNumber *val)
{
    fLowThreshold = (SInt32)val->unsigned32BitValue();
    setProperty(sLowThreshold, val);
}

void IOHWSensor::setHighThreshold(OSNumber *val)
{
    fHighThreshold = (SInt32)val->unsigned32BitValue();
    setProperty(sHighThreshold, val);
}


void IOHWSensor::setPollingPeriod(OSNumber *val)
{
	fPollingPeriod = val->unsigned32BitValue();
	setProperty(sPollingPeriod, val);
}

void IOHWSensor::setPollingPeriodNS(OSNumber *val)
{
	fPollingPeriodNS = val->unsigned32BitValue();
	setProperty(sPollingPeriodNS, val);
}

void IOHWSensor::setTimeout()
{
	AbsoluteTime interval, end;
	
	if (!fCalloutEntry) return;		// We've been stop()ped so nothing to do

	AbsoluteTime_to_scalar( &interval ) = 0;
	AbsoluteTime_to_scalar( &end ) = 0;

	thread_call_cancel((thread_call_t) fCalloutEntry);

	if( (fPollingPeriod == kNoPolling) && (fPollingPeriodNS == kNoPolling) )
		return;

	if(fPollingPeriod != kNoPolling)
	{
		clock_interval_to_absolutetime_interval( fPollingPeriod /* seconds */, NSEC_PER_SEC, &interval );       // convert the seconds
	}

	if(fPollingPeriodNS != kNoPolling)
	{
		AbsoluteTime scratch;
		AbsoluteTime_to_scalar( &scratch ) = 0;

		clock_interval_to_absolutetime_interval( fPollingPeriodNS /* nanoseconds */, 1, &scratch );    // convert the nanoseconds
		ADD_ABSOLUTETIME(&interval, &scratch);  // add the nanoseconds to the seconds
	}

	clock_absolutetime_interval_to_deadline(interval, &end);        // calculate the deadline (now + interval)

	thread_call_enter_delayed((thread_call_t) fCalloutEntry, end);      // schedule the thread callout
}

IOReturn IOHWSensor::setPowerState(unsigned long whatState, IOService *policyMaker)
{
	IOReturn status;
	
	/* 
	 * Don't care about setPowerState calls during boot.  This protects us from calls
	 * that come in during start (as soon as joinPMTree is called)
	 */
	if (!fInited)
		return IOPMAckImplied;
	
	status = IOHWMonitor::setPowerState( whatState, policyMaker );

	if ( fCalloutEntry )
	{
		if ( sleeping )
			thread_call_cancel( fCalloutEntry );
		else
			setTimeout();
	}

	return status;
}
