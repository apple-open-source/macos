/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOFIREWIREIPUNIT_H
#define _IOKIT_IOFIREWIREIPUNIT_H

#include <IOKit/IOService.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFWRegs.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include "IOFireWireIP.h"
#include "ip_firewire.h"

class IOFireWireNub;


/*!
    @class IOFireWireIPUnit
    @abstract nub for IP devices
*/
class IOFireWireIPUnit : public IOService
{
    OSDeclareDefaultStructors(IOFireWireIPUnit)

protected:
    IOFireWireNub		*fDevice;
    IOFireWireIP		*fIPLocalNode;
    OSData				*buf;
    IOConfigDirectory	*dir;
	DRB					*fDrb;
	bool				fIPUnitState;
    
/*! @struct hwAddr
    @discussion Firewire IP Unit Dependent info key's "kConfigUnitDependentInfoKey" value stored here.
    */    
    IP1394_HDW_ADDR    *hwAddr;
    
/*! @bool special
    @discussion State to maintain if the IP unit implements 
    @"kConfigUnitDependentInfoKey".
    */  
    bool special;
    
/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the class in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;
    
    virtual void free(void);
    
public:
    // IOService overrides
    virtual bool start(IOService *provider);
    virtual IOReturn message(UInt32 type, IOService *provider, void *argument);

/*! @function handleOpen
    @abstract Overrideable method to control the open / close behaviour of an IOService.
    @discussion See IOService for discussion.		
    @param forClient Designates the client of the provider requesting the open.
    @param options Options for the open, may be interpreted by the implementor of handleOpen.
    @result Return true if the open was successful, false otherwise.
*/

    virtual bool handleOpen(  IOService *	  forClient,
                              IOOptionBits	  options,
                              void *		  arg );
/*! 
    @function handleClose
    @abstract Overrideable method to control the open / close behaviour of an IOService.
    @discussion See IOService for discussion.
    @param forClient Designates the client of the provider requesting the close.
    @param options Options for the close, may be interpreted by the implementor of handleOpen. 
*/

    virtual void handleClose(   IOService *		forClient,
                                IOOptionBits	options );
    

/*!
    @function matchPropertyTable
    @abstract Matching language support
	Match on the following properties of the unit:
	Vendor_ID
	GUID
	Unit_Type
	and available sub-units, match if the device has at least the requested number of a sub-unit type:
	AVCSubUnit_0 -> AVCSubUnit_1f
*/
    virtual bool matchPropertyTable(OSDictionary * table);

/*!
    @function getDevice
    @abstract Returns the FireWire device nub that is this object's provider .
*/
    IOFireWireNub* getDevice() const
        {return fDevice;};
        
/*!
    @function setLocalNode
    @abstract Sets the IP Local node in the FireWire IP unit
*/
    void setLocalNode(IOFireWireIP * fNode)
        {fIPLocalNode = fNode;};

/*!
    @function setDrb
    @abstract Sets the device reference block in the FireWire IP unit
*/
    void setDrb(DRB * drb)
        {fDrb = drb;};
		
/*!
    @function getDrb
    @abstract Gets the device reference block from the FireWire IP unit
*/
    DRB *getDrb()
        {return fDrb;};

/*!
    @function getUnitState
    @abstract gets the unit state
*/
	bool getUnitState()
	{return fIPUnitState;};
	
/*!
    @function setUnitState
    @abstract sets the unit state
*/
	void setUnitState(bool state)
	{fIPUnitState = state;};

/*!
    @function updateDrb
    @abstract Updates the device reference block in the FireWire IP unit
*/
    void updateDrb();

/*!
    @function isSpecial
    @abstract returns whether the node is mac or not
*/
    bool isSpecial()
        {return special;};
        
/*!
    @function getHDWAddr
    @abstract returns IP over Firewire hardware address
*/
    void *getHDWAddr()
        {return hwAddr;};


    
private:
    OSMetaClassDeclareReservedUnused(IOFireWireIPUnit, 0);
    OSMetaClassDeclareReservedUnused(IOFireWireIPUnit, 1);
    OSMetaClassDeclareReservedUnused(IOFireWireIPUnit, 2);
    OSMetaClassDeclareReservedUnused(IOFireWireIPUnit, 3);

};

#endif // _IOKIT_IOFIREWIREIPUNIT_H

