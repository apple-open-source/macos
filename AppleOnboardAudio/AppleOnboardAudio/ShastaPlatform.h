/*
 *  ShastaPlatform.h
 *  
 *
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 *
 *  [3629501]   The 'ShastaPlatform' class, derived from the 'K2Platform' class, was originally
 *				created to correct a math error that existed in the device tree description of
 *				the DMA addresses used for the I2S transport that was not observed when only the
 *				'i2s-a' transport was used but exhibited failed execution when both 'i2s-a' and
 *				'i2s-c' were used.  The K2Platform object is only used on the earliest model CPU
 *				that uses Platform Functions to provide software access to hardware memory mapped
 *				I/O registers.  The CODEC used on the K2Platform CPUs requires periodic polling
 *				through a timer task while later CPUs that use the ShastaPlatform object requires
 *				CODEC hardware interrupt servicing.  Radar 3629501 adds hardware interrupt support
 *				for CODEC Error Interrupts and CODEC Interrupts.  This is done by overriding the
 *				'interruptUsesTimerPolling' method and returning FALSE for all interrupt sources
 *				to indicate that hardware interrupt services are required rather than timer 
 *				polling interrupt services.  AppleOnboardAudio-225b13 had removed the polling
 *				capability from the K2Platform object but that capability has now been restored.
 *
 */
#include "K2Platform.h"

#ifndef _SHASTA_PLATFORM_H
#define _SHASTA_PLATFORM_H

class ShastaPlatform : public K2Platform {

    OSDeclareDefaultStructors(ShastaPlatform);

public:

	virtual bool						init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex);
	virtual	void						free();

	//
	// Set Interrupt Handler Methods	[3629501]
	//
	
	virtual bool						interruptUsesTimerPolling( PlatformInterruptSource source );					//  [3629501]   now virtual!

	//
	// DBDMA Memory Address Acquisition Methods
	//
	
	virtual	IODBDMAChannelRegisters *	GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );
	virtual	IODBDMAChannelRegisters *	GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );

	virtual PlatformInterfaceObjectType	getPlatformInterfaceType () { return kPlatformInterfaceType_Shasta; }
	
protected:

	IODBDMAChannelRegisters *			GetChannelRegistersVirtualAddress ( IOService * dbdmaProvider, UInt32 index );
};

#endif

