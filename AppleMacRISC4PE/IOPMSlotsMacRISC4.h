/*
 * Copyright (c) 1998-2007 Apple Inc. All rights reserved.
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
#ifndef _IOKIT_SLOTSMACRISC4DOMAIN_H
#define _IOKIT_SLOTSMACRISC4DOMAIN_H

#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/IOPM.h>

class IOPMrootDomain;
class IOPCIDevice;

enum
{
	// PCI constants
	
	kPCIStatusConfigOffset								= 0x6,
	kPCIStatusPowerCapabilitiesSupportBitMask 			= ( 1L << 4 ),
	kPCIHeaderTypeConfigOffset							= 0xE,
	kPCIHeaderTypeBitMask								= 0x7F,
	kPCIStandardHeaderType								= 0,
	kPCItoPCIBridgeHeaderType							= 1,
	kPCICardBusBridgeHeaderType							= 2,
	
	kPCIPowerCapabilitiesPtrStandardConfigOffset		= 0x34,
	kPCIPowerCapabilitiesPtrPCItoPCIBridgeConfigOffset	= 0x34,
	kPCIPowerCapabilitiesPtrCardBusBridgeConfigOffset	= 0x14,
	
	kPCIPowerCapabilityID								= 0x01,
	kPCIPowerCapabilitiesMinOffset						= 0x40,
	kPCIPowerCapabilitiesMaxOffset						= 0xF8,		// ( 256 - 8 )
        
	kPCIPowerCapabilitiesPMCRegisterOffset				= 2,
	kPCIPowerCapabilitiesDataRegisterOffset				= 7,

	kMaxPowerCapabilitiesListLoopCount					= 64,
	kMinPCIPowerCapabilitiesVersion						= 2,
        
	kPCIPowerCapabilitiesPMCVersionBitMask				= 0x7,
	kPCIPowerCapabilitiesPMCAuxCurrentOffset			= 6,
	
	kPCIStandardSleepCurrentNeeded						= 20,			// 20 milliamps (mA)

	kMaxAuxPowerScalingFactorBitMask					= 0x3,
	kSawtoothPowerSupplyMillivolts						= 5000,			// 5 volts
	kMillivoltsPerVolt									= 1000, 

	kPCIPowerCapabilitiesPMCSROffset					= 4,
	kPCIPowerCapabilitiesDataSelectBitShift				= 9,
	kPCIPowerCapabilitiesDataSelectBitMask				= 0x1E00,
	kPCIPowerCapabilitiesDataSelectMaxCombinations		= 16,
	kPCIPowerCapabilitiesDataSelectD3PowerConsumed		= 3,

	kPCIPowerCapabilitiesDataScaleBitShift				= 13,
	kPCIPowerCapabilitiesDataScaleBitMask				= 0x6000,
	kPCIPowerCapabilitiesDataScale10Divisor				= 1,
	kPCIPowerCapabilitiesDataScale100Divisor			= 2,

    kPCIPowerCapabilitiesPMESupportD3ColdBitMask        = 0x8000,  // PMC bit 15
    kPCIPowerCapabilitiesPMEEnableBitMask               = 0x0100,  // PMCSR bit 8
};


class IOPMSlotsMacRISC4: public IOService
{
    OSDeclareDefaultStructors(IOPMSlotsMacRISC4)

public:

    IOPMrootDomain *	rootDomain;			// points to Root Power Domain
    unsigned long	auxCapacity;			// capacity of the aux power supply in milliwatts
    unsigned long	powerSupplyMillivolts;
    bool            checkAuxCapacity;
      
    virtual  bool start( IOService * provider );

    virtual  IOReturn determineSleepSupport ( void );

private:

    virtual void probePCIhardware ( IOPCIDevice *, bool *, unsigned long * );
    virtual bool dataRegisterPresent ( IOPCIDevice *, UInt8 );
    virtual UInt32 getD3power ( IOPCIDevice *, UInt8 );
    
};

#endif /*  _IOKIT_SLOTSMACRISC4DOMAIN_H */
