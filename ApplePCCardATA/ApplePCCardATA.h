/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
 
 #ifndef _DRV_APPLEPCCARD_ATA_H
#define _DRV_APPLEPCCARD_ATA_H

#include <IOKit/IOTypes.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATABusInfo.h>
#include  <IOKit/ata/IOATAController.h>
#include <IOKit/IOInterruptEventSource.h>

/*! @class ApplePCCardATA : public IOATAController
    @abstract The specific driver for PCCard ATA controllers.
*/ 



#endif //_DRV_APPLEPCCARD_ATA_H


class ApplePCCardATA : public IOATAController
{
    OSDeclareDefaultStructors(ApplePCCardATA)

public:

	/*--- Overrides from IOService ---*/
	virtual bool init(OSDictionary* properties);  //

	// checks for the compatible property of "pccard-ata" 
	// in the device tree.
	virtual IOService* probe( IOService* provider,	SInt32*	score ); //
	virtual bool start( IOService* provider ); //
	virtual IOWorkLoop*	getWorkLoop() const;  //
	// listen for device removed message
	virtual IOReturn message (UInt32 type, IOService* provider, void* argument = 0);


	// set and get bus timing configuration for a specific unit 
	virtual IOReturn selectConfig( IOATADevConfig* configRequest, UInt32 unitNumber);  //
	virtual IOReturn getConfig( IOATADevConfig* configRequest, UInt32 unitNumber);  //

	// provide information on bus capability
	virtual IOReturn provideBusInfo( IOATABusInfo* infoOut); //



protected:
	IOInterruptEventSource* _devIntSrc;
	bool	isBusOnline; // true = card in slot, false = ejected
	IOMemoryMap* baseAddressMap;
	IOMemoryMap* baseAddressMap2;

	//OSObject overrides
	virtual void free();  //

	// connects device with registers
	virtual bool configureTFPointers(void);   //
	// connect the device (drive) interrupt to our workloop
	
	virtual bool createDeviceInterrupt(void); //
	static void deviceInterruptOccurred(OSObject*, IOInterruptEventSource *, int count); //


	
	// override for special case device.
	virtual IOATAController::transState determineATAPIState(void); //
	// clear interrupt by reading status even if no transaction pending.
	virtual IOReturn handleDeviceInterrupt(void); //
	// override for hardware specific issue.
	virtual IOReturn synchronousIO(void);	//
	// override for hardware specific issue.
	virtual IOReturn selectDevice( ataUnitID unit );  //

	// removable bus support
	virtual void handleTimeout(void); //
	virtual IOReturn executeCommand(IOATADevice* nub, IOATABusCommand* command); //
	virtual IOReturn handleQueueFlush( void ); //
	virtual bool checkTimeout( void );  //
	static void cleanUpAction(OSObject * owner, void*, void*, void*, void*); //
	virtual void cleanUpBus(void); //

// debug only
	virtual UInt32 scanForDrives(void);

};
