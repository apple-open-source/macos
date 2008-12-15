/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSData.h>

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOKitKeys.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#include <IOKit/usb/IOUSBLog.h>

#define super	IOService

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOUSBNub, IOService )
OSDefineAbstractStructors(IOUSBNub, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 ** Matching methods
 **/

bool 
IOUSBNub::USBCompareProperty( OSDictionary   * matching, const char     * key )
{
    // We return success iff we match the key in the dictionary with the key in
    // the property table.
    //
    OSObject 	*value;
    bool		matches = false;
	OSObject	*myProperty = NULL;

    value = matching->getObject( key );
	
    if( value)
	{
		myProperty = copyProperty(key);
		if (myProperty)
		{
			matches = value->isEqualTo( myProperty);
			myProperty->release();
		}
	}
    else
        matches = false;

    return matches;
}

bool 
IOUSBNub::IsWildCardMatch( OSDictionary   * matching, const char     * key )
{
    // We return success iff the  key in the dictionary exists AND it is a OSString "*"
    // the property table.
    //
    OSString 	*theString;
    bool		matches;
    
    theString = OSDynamicCast(OSString, matching->getObject( key ));
    
    if( theString)
        matches = theString->isEqualTo("*");
    else
        matches = false;
    
    return matches;
}  

bool
IOUSBNub::USBComparePropertyWithMask( OSDictionary *matching, const char *key, const char * maskKey )
{
    // This routine will return success if the "key" in the dictionary  matches the key in the property table
    // while applying the "mask" value to both
    // First, check to see if we have both keys
    //
    OSNumber *	registryProperty = NULL;
    OSNumber *	dictionaryProperty = NULL;
    OSNumber *	dictionaryMask = NULL;
    
    registryProperty = OSDynamicCast(OSNumber,  getProperty(key));
    dictionaryProperty = OSDynamicCast(OSNumber, matching->getObject(key));
    dictionaryMask = OSDynamicCast(OSNumber, matching->getObject(maskKey));
    
    if ( registryProperty && dictionaryProperty && dictionaryMask )
    {
		// If all our values are OSNumbers, then get their actual value and do the masking
		// to see if they are equal
		//
		UInt32  registryValue = registryProperty->unsigned32BitValue();
		UInt32  dictionaryValue = dictionaryProperty->unsigned32BitValue();
		UInt32  mask = dictionaryMask->unsigned32BitValue();
		
		if ( (registryValue & mask) == (dictionaryValue & mask) )
		{
			return true;
		}
    }
    
    return false;
}



// this method will override the Platform specific implementation of joinPMtree
// it will cause any IOUSBDevice and IOUSBInterface clients to join the IOPower tree as
// children of the IOUSHubPolicyMaker for the hub to which they are attached
void
IOUSBNub::joinPMtree ( IOService * driver )
{
	const IORegistryPlane		*usbPlane = NULL;
	IOUSBHubDevice				*hubDevice = NULL;
	IOUSBHubPolicyMaker			*hubPolicyMaker = NULL;
	
	
	usbPlane = getPlane(kIOUSBPlane);
	if (usbPlane)
	{
		// This call will retrieve the IOUSBHubDevice in the case that we are an IOUSBDevice nub
		hubDevice = OSDynamicCast(IOUSBHubDevice, getParentEntry(usbPlane));
		if (!hubDevice && getProvider())
		{
			// This call will retrieve the IOUSBHubDevice in the case that we are an IOUSBInterface nub
			hubDevice = OSDynamicCast(IOUSBHubDevice, getProvider()->getParentEntry(usbPlane));
		}
		if (hubDevice)
		{
			hubPolicyMaker = hubDevice->GetPolicyMaker();
		}
		else
		{
			USBError(1, "%s[%p]::joinPMtree - could not find the hub device", getName(), this);
		}
	}

	if (hubPolicyMaker)
	{
		hubPolicyMaker->joinPMtree(driver);
	}
	else
	{
		USBLog(1, "%s[%p]::joinPMtree - no hub policy maker - calling through to super::joinPMtree", getName(), this);
		super::joinPMtree(driver);
	}
	
}


const char * 
IOUSBNub::stringFromReturn( IOReturn rtn )
{
	static const IONamedValue USBReturn_values[] = { 
		{kIOUSBUnknownPipeErr,								"Pipe is invalid"														},
		{kIOUSBTooManyPipesErr,								"Device specified too many endpoints"									},
		{kIOUSBNoAsyncPortErr,								"Async Port has not been specified"        								},
		{kIOUSBNotEnoughPipesErr,							"Desired pipe was not found"        									},
		{kIOUSBNotEnoughPowerErr,							"There is not enough power for the device"        						},
		{kIOUSBEndpointNotFound,							"Endpoint does not exist"												},
		{kIOUSBConfigNotFound,								"Configuration does not exist"											},
		{kIOUSBTransactionTimeout,							"Rrequest did not finish"												},
		{kIOUSBTransactionReturned,							"Request has been returned to the caller"								},
		{kIOUSBPipeStalled,									"Request returned a STALL"												},
		{kIOUSBInterfaceNotFound,							"Requested interface was not found"       								},
		{kIOUSBLowLatencyBufferNotPreviouslyAllocated,		"The buffer was not pre-allocated"										},
		{kIOUSBLowLatencyFrameListNotPreviouslyAllocated,	"The frame list was not pre-allocated"									},
		{kIOUSBHighSpeedSplitError,							"High Speed hub returned a split transaction error"						},
		{kIOUSBSyncRequestOnWLThread,						"Synchronous request was issued while holding from within the workloop"	},
		{kIOUSBHighSpeedSplitError,							"High Speed hub returned a split transaction error"						},
		{kIOUSBLinkErr,										"USB controller error"       											},
		{kIOUSBNotSent1Err,									"The isoch transfer did not occur, scheduled too late"					},
		{kIOUSBNotSent2Err,									"The isoch transfer did not occur, scheduled too late"					},
		{kIOUSBBufferUnderrunErr,							"Buffer Underrun (Host hardware failure on data out, PCI busy?"			},
		{kIOUSBBufferOverrunErr,							"Buffer Overrun (Host hardware failure on data out, PCI busy?"			},
		{kIOUSBReserved2Err,								"Reserved error #1"														},
		{kIOUSBReserved1Err,								"Reserved error #2"        												},
		{kIOUSBWrongPIDErr,									"Pipe stall, Bad or wrong PID"        									},
		{kIOUSBPIDCheckErr	,								"Pipe stall, PID CRC error"        										},
		{kIOUSBDataToggleErr,								"Pipe stall, Bad data toggle"       									},
		{kIOUSBBitstufErr,									"Pipe stall, bitstuffing"												},
		{kIOUSBCRCErr,										"Pipe stall, bad CRC"        											},
		{kIOUSBDeviceNotHighSpeed,							"Device is not a high speed device"        								},
		{0,													NULL																	}
	};
	
	
	const char *	returnName = super::stringFromReturn(rtn);
	
	if ( returnName =="" )
		returnName = IOFindNameForValue(rtn, USBReturn_values);
	
	return returnName;
}



