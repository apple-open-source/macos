/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
#include "IOPMSlotsMacRISC4.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IODeviceTreeSupport.h>
#include "IOKit/pwr_mgt/IOPowerConnection.h"
#include "IOKit/pwr_mgt/IOPM.h"
#include "IOKit/pci/IOPCIDevice.h"

bool auxDriverHasRoot( OSObject * us, void *, IOService * yourDevice );

#define number_of_power_states 3

static IOPMPowerState ourPowerStates[number_of_power_states] =
{
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMPowerOn,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0},
    {1,IOPMPowerOn,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

// The following aux current table values come from the PCI Bus Power Management Interface Spec 
// (PMIS) version 1.1, Pg. 26. The values are in milliamps.

static const UInt32		kPCIPowerCapabilitiesAuxCurrentTable[] = { 0, 55, 100, 160, 220, 270, 320, 375 };

// The following max auxiliary power scaling factor table values come from the Boot Flash System
// Configuration Block document. They are set up to convert a watts value to a milliwatts value so
// the values are reversed and used as a multiplier instead of a divisor.

static const UInt32		kMaxAuxPowerScalingFactorTable[] = { 1000, 100, 10, 1 };

#define super IOService
OSDefineMetaClassAndStructors(IOPMSlotsMacRISC4,IOService)

#ifndef kIOPMIsPowerManagedKey
#define kIOPMIsPowerManagedKey	"IOPMIsPowerManaged"
#endif

// **********************************************************************************
// start
//
// **********************************************************************************
bool IOPMSlotsMacRISC4::start ( IOService * nub )
{
    OSData *		prop;
    IORegistryEntry *	node;
    UInt32		x;
    
    auxCapacity = 0;
    rootDomain = NULL;
    
    super::start(nub);
    PMinit();

    registerPowerDriver(this,ourPowerStates,number_of_power_states);

    //   changePowerStateTo( number_of_power_states-1);			// clamp power on
    clampPowerOn (300*NSEC_PER_SEC);

    setProperty ("IOClass", "IOPMSlotsMacRISC4");

    addNotification( gIOPublishNotification,serviceMatching("IOPMrootDomain"),	// look for the Root Domain
                 (IOServiceNotificationHandler)auxDriverHasRoot, this, 0 );

    node = IORegistryEntry::fromPath("mac-io/via-pmu/power-mgt", gIODTPlane);
    if ( node != NULL ) {
        prop = OSDynamicCast(OSData, node->getProperty("power-supply-millivolts"));
        if ( prop != NULL ) {
            powerSupplyMillivolts = *((UInt32 *)(prop->getBytesNoCopy()));
        }
        else {
            powerSupplyMillivolts = kSawtoothPowerSupplyMillivolts;
	}
        prop = OSDynamicCast(OSData, node->getProperty("max-aux-power"));
        if ( prop != NULL ) {
            x = *((UInt32 *)(prop->getBytesNoCopy()));
            auxCapacity = ( x & ~kMaxAuxPowerScalingFactorBitMask ) * 
			   kMaxAuxPowerScalingFactorTable[ x & kMaxAuxPowerScalingFactorBitMask ];
        }
    }
    
    // If the system is portable, then both platform functions will return false, and
    // the aux capacity will not be checked.

    checkAuxCapacity = ((true == getPlatform()->hasPrivPMFeature( kPMHasLegacyDesktopSleepMask ))
                     || (true == getPlatform()->hasPMFeature( kPMCanPowerOffPCIBusMask )));

    return true;
}


bool auxDriverHasRoot( OSObject * us, void *, IOService * yourDevice )
{
    if ( yourDevice != NULL ) {
        ((IOPMSlotsMacRISC4 *)us)->rootDomain = (IOPMrootDomain *)yourDevice;
    }
    return true;
}


// **********************************************************************************
// setPowerState
//
// **********************************************************************************
IOReturn IOPMSlotsMacRISC4::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice)
{
    OSIterator *	iter;
    OSObject *		next;
    IOPowerConnection *	connection;
    IOService *		nub;
    bool		canSleep = true;
    IOPCIDevice *	PCInub;
    unsigned long	totalPower = 0;
    unsigned long	childPower;
    OSIterator *    	pciChildIter;
    OSObject *		obj;

    if ( powerStateOrdinal == 0 ) {
        iter = getChildIterator(gIOPowerPlane);
        if ( iter ) {
            while ( (next = iter->getNextObject()) ) {
                if ( (connection = OSDynamicCast(IOPowerConnection,next)) ) {
                    nub = ((IOService *)(connection->getChildEntry(gIOPowerPlane)));
                    if ( (PCInub = OSDynamicCast(IOPCIDevice,nub)) ) {
                        childPower = PCInub->currentPowerConsumption();
                        if (  childPower != kIOPMUnknown ) {
                            if ( checkAuxCapacity )
                                totalPower += childPower;
                        }
                        else {
                            if ( checkAuxCapacity ) probePCIhardware(PCInub,&canSleep,&totalPower);

                            if ((obj = nub->getProperty( kIOPMIsPowerManagedKey )))
                            {
                                if (obj != kOSBooleanTrue)
                                {
                                    IOLog("PCI sleep prevented by non-power-managed %s (3)\n",nub->getName());
                                    canSleep = false;
                                }
                            }
                            else if ( (pciChildIter = nub->getChildIterator( gIOServicePlane )) )
                            {
                                IORegistryEntry * child;
                                UInt32            childCount = 0;

                                while (( child = (IORegistryEntry *) pciChildIter->getNextObject() ))
                                {
                                    childCount++;
                                    if ( child->inPlane( gIOPowerPlane ) ) break;
                                }
                                if ( childCount && (child == 0) )
                                {
                                    IOLog("PCI sleep prevented by non-power-managed %s (1)\n",nub->getName());
                                    canSleep = false;
                                }
                                pciChildIter->release();
                            }

                            if ( ! connection->childHasRequestedPower() ) {
                                IOLog("PCI sleep prevented by non-power-managed %s (2)\n",nub->getName());
                                canSleep = false;
                            }
                        }
                    }
                    else {
                        canSleep = false; 	// something wrong with the power plane
                    }
                }
            }
            iter->release();
        }
        if ( totalPower > auxCapacity ) {
            IOLog("PCI sleep prevented by high-power expansion cards %ld %ld (4)\n",totalPower,auxCapacity);
            canSleep = false;
        }

        if ( ! canSleep ) {
            if ( rootDomain != NULL ) {
                rootDomain->setSleepSupported(kPCICantSleep);
            }
        }
    }
  return IOPMAckImplied;
}


// **********************************************************************************
// probePCIhardware
//
// **********************************************************************************
void IOPMSlotsMacRISC4::probePCIhardware ( IOPCIDevice * PCInub, bool * canSleep, unsigned long * totalPower )
{
    UInt32	data;
    UInt32	tempData;
    UInt32	milliwatts;
    UInt8	offset;
    UInt8	nextOffset;
    int		loopCount;
    
    data = PCInub->configRead16(kPCIStatusConfigOffset);
    if( !( data & kPCIStatusPowerCapabilitiesSupportBitMask ) ) {
#if 0
        // Legacy PCI devices that does not support the PCI Power Management
        // Specification can still support D0 and D3 states with support from
        // the device driver. This should not prevent system sleep.
        IOLog("PCI sleep prevented by non-power-managed %s (3)\n",PCInub->getName());
        *canSleep = false;
#endif
        return;
    }
    data = PCInub->configRead8(kPCIHeaderTypeConfigOffset);
    data &= kPCIHeaderTypeBitMask;
    switch ( data ) {
    	case kPCIStandardHeaderType:
            offset = kPCIPowerCapabilitiesPtrStandardConfigOffset;
            break;
    
    	case kPCItoPCIBridgeHeaderType:
            offset = kPCIPowerCapabilitiesPtrPCItoPCIBridgeConfigOffset;
            break;
    
        case kPCICardBusBridgeHeaderType:
            offset = kPCIPowerCapabilitiesPtrCardBusBridgeConfigOffset;
            break;
        
        default:
            *canSleep = false;
            return;
    }
    data = PCInub->configRead8(offset);
    if( ( data == 0 ) || 
            ( data < kPCIPowerCapabilitiesMinOffset ) || 
            ( data > kPCIPowerCapabilitiesMaxOffset ) ) {
        *canSleep = false;
        return;
    }
    offset = data;
    loopCount = 0;
    while ( true ) {

// Read the capabilities id to determine if it's a power capability. Read the 
// byte following the id as it is the offset to the next capability or zero to
// indicate this is the last entry in the linked list.

        data = PCInub->configRead8(offset);
        nextOffset = PCInub->configRead8(offset + 1);
        if( data == kPCIPowerCapabilityID ) {
            break;
        }
        if( nextOffset == 0 ) {
            *canSleep = false;
            return;
        }
        if( ( nextOffset < kPCIPowerCapabilitiesMinOffset ) || 
                ( nextOffset > kPCIPowerCapabilitiesMaxOffset ) ) {
            *canSleep = false;
            return;
        }
        ++loopCount;
        if( (offset == nextOffset) || (loopCount > kMaxPowerCapabilitiesListLoopCount) ) {
            *canSleep = false;
            return;
        }
        offset = nextOffset;
    }
	
    // offset now points to the Power Management Register Block
    
    if ( ( PCInub->configRead16( offset + kPCIPowerCapabilitiesPMCRegisterOffset ) & 
           kPCIPowerCapabilitiesPMESupportD3ColdBitMask ) == 0 )
    {
        // If device cannot assert PME# from D3 (cold) state, then the device
        // is not allowed to draw power from the auxiliary power source.
        milliwatts = 0;
    }
    else if ( dataRegisterPresent(PCInub,offset) ) {
        milliwatts = getD3power(PCInub,offset);
    }
    else {
        data = PCInub->configRead16(offset + kPCIPowerCapabilitiesPMCRegisterOffset);
        if( ( data & kPCIPowerCapabilitiesPMCVersionBitMask ) >= kMinPCIPowerCapabilitiesVersion ) {
            data = PCInub->configRead16(offset + kPCIPowerCapabilitiesPMCRegisterOffset);
            tempData = ( data >> kPCIPowerCapabilitiesPMCAuxCurrentOffset ) & 7;
            milliwatts = kPCIPowerCapabilitiesAuxCurrentTable[tempData] * powerSupplyMillivolts / kMillivoltsPerVolt;
        }
        else {	// Device does not support PMIS so assume it may require up to 20mA.
            milliwatts = kPCIStandardSleepCurrentNeeded * powerSupplyMillivolts / kMillivoltsPerVolt;
        }
    }
    
    // If the assertion of PME# is not enabled, then the device can only draw 20mA
    // from the 3.3Vaux line.

    if ( ( PCInub->configRead16( offset + kPCIPowerCapabilitiesPMCSROffset ) &
           kPCIPowerCapabilitiesPMEEnableBitMask ) == 0 )
    {
        milliwatts = min( milliwatts, 66 /* 20mA @ 3.3V */ );
    }

    *totalPower += milliwatts;
}


// **********************************************************************************
// dataRegisterPresent
//
// **********************************************************************************
bool IOPMSlotsMacRISC4::dataRegisterPresent ( IOPCIDevice * PCInub, UInt8 offset )
{
    UInt16		data;
    UInt16		tempData;
    UInt32		index;
    
    // Read the current value of the data surrounding the Data_Select bits so we don't wipe them
    // out when we write new values to the register to test for data register support.
    
    data = PCInub->configRead16(offset + kPCIPowerCapabilitiesPMCSROffset);
    
    // Perform an exhaustive search for data register support by trying every possible Data_Select
    // value and checking if any bits are set when we read back the Data_Scale value.
    
    for( index = 0; index < kPCIPowerCapabilitiesDataSelectMaxCombinations; ++index ) {
        
        // Write the Data_Select register with our test value.
        
        data &= ~kPCIPowerCapabilitiesDataSelectBitMask;
        data |= (index << kPCIPowerCapabilitiesDataSelectBitShift);
        PCInub->configWrite16(offset + kPCIPowerCapabilitiesPMCSROffset,data);
        
        // Read the data scale register and check if any Data_Scale bits were set.
        
        tempData = PCInub->configRead16(offset + kPCIPowerCapabilitiesPMCSROffset);
        if( tempData & kPCIPowerCapabilitiesDataScaleBitMask ) {
            return true;     // A non-zero Data_Scale value was returned so the data register is implemented.
        }
    }
    
    return false;
}


// **********************************************************************************
// getD3power
//
// Return milliwatts of power consumed in D3 (off) state
// **********************************************************************************
UInt32 IOPMSlotsMacRISC4::getD3power ( IOPCIDevice * PCInub, UInt8 offset )
{
    UInt16  pmcsr;
    UInt16  dataScale;
    UInt32  power;

    // Read the current value of the data surrounding the Data_Select bits so we don't wipe them
    // out when we write new values to the register.
    
    pmcsr = PCInub->configRead16(offset + kPCIPowerCapabilitiesPMCSROffset);

    // Write the Data_Select register.

    pmcsr &= ~kPCIPowerCapabilitiesDataSelectBitMask;
    pmcsr |= (kPCIPowerCapabilitiesDataSelectD3PowerConsumed << kPCIPowerCapabilitiesDataSelectBitShift);
    PCInub->configWrite16(offset + kPCIPowerCapabilitiesPMCSROffset, pmcsr );

    // Read the data scale register.

    dataScale = PCInub->configRead16(offset + kPCIPowerCapabilitiesPMCSROffset);
    dataScale &= kPCIPowerCapabilitiesDataScaleBitMask;
    dataScale >>= kPCIPowerCapabilitiesDataScaleBitShift;

    // Read the data register.

    power = PCInub->configRead8(offset + kPCIPowerCapabilitiesDataRegisterOffset);

    if( dataScale == kPCIPowerCapabilitiesDataScale10Divisor ) {	// make it milliwatts
        power *= 100;
    }
    if( dataScale == kPCIPowerCapabilitiesDataScale100Divisor ) {
        power *= 10;
    }

    return power;
}

