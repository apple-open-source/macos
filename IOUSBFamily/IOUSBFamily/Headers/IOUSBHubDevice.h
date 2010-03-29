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



#ifndef _IOKIT_IOUSBHUBDEVICE_H
#define _IOKIT_IOUSBHUBDEVICE_H

#include <IOKit/usb/IOUSBDevice.h>						// our superclass

// forward refeference. we don't include the header file because it would cause a circular reference
class IOUSBHubPolicyMaker;

enum {
	kIOUSBHubDeviceIsRootHub			=	0x0001,
	kIOUSBHubDeviceIsOnHighSpeedBus		=	0x0002,
	kIOUSBHubDeviceCanSleep				=	0x0004
};

/*!
    @class IOUSBHubDevice
    @abstract New in MAC OS X 10.5. The IOKit object representing a hub device on the USB bus. It is a subclass of IOUSBDevice.
    @discussion With the exception of the IOUSBRootHubDevice objects representing the root hubs, every IOUSBDevice
	will have an IOUSBHubDevice as its parent in the IOUSB plane. USB KEXT drivers will have the ability to 
	query this device for characteristics of the hub into which they are connected. This class will also give a dive driver
	the abilty to get an IOService* pointing to the hub driver itself, which can then be used as the parent in the power plane.
*/

class IOUSBHubDevice : public IOUSBDevice
{
	friend class IOUSBHubPolicyMaker;
	friend class AppleUSBHub;
	friend class IOUSBController;								// for the "can sleep" characteristic
	
private:
    OSDeclareDefaultStructors(IOUSBHubDevice)

	UInt32					_myCharacteristics;					// bitmap of my characteristics
	IOUSBHubPolicyMaker		*_myPolicyMaker;					// pointer to the policy maker in the IOPower tree for this hub

    struct ExpansionData 
	{ 
		UInt32					_maxPortCurrent;				// maximum current in milliamps per downstream port
		UInt32					_totalExtraCurrent;				// total amount of current above the spec'ed current per port available (during normal operation)
		UInt32					_totalSleepCurrent;				// total amount of current that can be drawn during sleep
		UInt32					_canRequestExtraPower;			// If 0, this hub does not support requesting extra power from its parent, non-zero:  how much power we need to request in order to give out _extraPowerForPorts
		UInt32					_extraPowerForPorts;			// Of the power requested from our parent, how much can we parcel out -- the rest is consumed by voltage drop thru the cable
		UInt32					_extraPowerAllocated;			// Amount of power that we actually got from our parent
		bool					_requestFromParent;				// True if we are to request the extra power from our parent, without modifying the request.  Used for RMHs
	};
    ExpansionData			*_expansionData;
	
protected:
	// IOUSBHubDevice methods which will be used by the hub driver (which is also the Policy Maker)
	virtual	void			SetPolicyMaker(IOUSBHubPolicyMaker *policyMaker);
	virtual void			SetHubCharacteristics(UInt32);
	virtual bool			InitializeCharacteristics(void);					// used at start
	
public:
	// static constructor
    static IOUSBHubDevice	*NewHubDevice(void);

	// IOKit methods
    virtual bool			init();
	virtual bool			start( IOService * provider );
    virtual void			stop( IOService *provider );
    virtual void			free();
	
	// public IOUSBHubDevice methods

	void					SetTotalSleepCurrent(UInt32 sleepCurrent);
	UInt32					GetTotalSleepCurrent();
	
    /*!
	 @function GetPolicyMaker
	 returns a pointer to the policy maker for the hub, which can be used as the power plane parent.
	 @result returns an IOUSBHubPolicyMaker* pointing to the policy maker for this hub. returns NULL is no policy maker is active on the hub device.
	 */
	virtual IOUSBHubPolicyMaker *GetPolicyMaker(void);
	
    /*!
	@function GetHubCharacteristics
	returns characteristics of the hub device which might be useful for the driver of a device connected to the hub.
		kIOUSBHubDeviceIsRootHub indicates that the hub is a root hub
		kIOUSBHubDeviceIsOnHighSpeedBus indicates that the hub is running on a High Speed bus. If this bit is set and kIOUSBHubDeviceIsHighSpeed is clear, then this is a Classic Speed hub running on a High Speed bus, which means that Split Transactions will be used to communicate with downstream devices.

	@result returns a bitmap of characteristics
    */
	virtual	UInt32			GetHubCharacteristics();
	
    /*!
	@function GetMaxProvidedPower
	returns the maximum amount of power available on any downstream port of this hub
	@result the power is returned in milliamps - usually 100ma or 500 ma.
    */
	virtual UInt32			GetMaxProvidedPower();

    /*!
	@function RequestProvidedPower
	requests power from the hub device
	@param requestedPower - the amount of power requested in milliamps (usually 100ma or 500 ma)
	@result the amount of power allocated for this driver in milliamps (usually 100ma or 500 ma)
    */
	virtual UInt32			RequestProvidedPower(UInt32 requestedPower);

	virtual UInt32			RequestExtraPower(UInt32 requestedPower);

	virtual void			ReturnExtraPower(UInt32 returnedPower);
	
    OSMetaClassDeclareReservedUsed(IOUSBHubDevice,  0);
	
	virtual void			InitializeExtraPower(UInt32 maxPortCurrent, UInt32 totalExtraCurrent);
	
    OSMetaClassDeclareReservedUsed(IOUSBHubDevice,  1);
	virtual UInt32			RequestSleepPower(UInt32 requestedPower);

    OSMetaClassDeclareReservedUsed(IOUSBHubDevice,  2);
	virtual void			ReturnSleepPower(UInt32 returnedPower);

    OSMetaClassDeclareReservedUsed(IOUSBHubDevice,  3);
	virtual void			SetSleepCurrent(UInt32 sleepCurrent);
	
    OSMetaClassDeclareReservedUsed(IOUSBHubDevice,  4);
	virtual	UInt32			GetSleepCurrent();
	
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  5);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  6);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  7);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  8);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  9);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  10);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  11);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  12);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  13);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  14);
    OSMetaClassDeclareReservedUnused(IOUSBHubDevice,  15);
};

#endif /* _IOKIT_IOUSBROOTHUBDEVICE_H */

