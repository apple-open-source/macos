/*
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef _I2CGPIO_H
#define _I2CGPIO_H

#include <IOKit/IOService.h>
#include "GPIOParent.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
// #define I2CGPIO_DEBUG 1

#ifdef I2CGPIO_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// Strings for calling into the I2C driver
#define kOpenI2CBus			"openI2CBus"
#define kReadI2CBus			"readI2CBus"
#define kWriteI2CBus		"writeI2CBus"
#define kCloseI2CBus		"closeI2CBus"
#define kSetCombinedMode	"setCombinedMode"
#define kSetStandardSubMode	"setStandardSubMode"
#define kRegisterForInts	"registerForI2cInterrupts"
#define kDeRegisterForInts	"deRegisterI2cClient"

// property key for combined monitor gpio
#define kI2CGPIOCombined	"i2c-combined"

enum {
	// I2C Transaction Retry Count
	kI2CReadRetryCount	= 10,
	kI2CWriteRetryCount	= 10,

	// command bytes, otherwise known as sub address
	k9554InputPort		= 0x00,	// for reading
	k9554OutputPort		= 0x01,	// for writing
	k9554PolarityInv	= 0x02, // polarity inversion register
	k9554Config			= 0x03	// direction bits
};

// structure to store info about AppleGPIO interrupt clients
typedef struct {
	GPIOEventHandler	handler;
	void				*self;
	bool				isEnabled;
} I2CGPIOCallbackInfo;

// the 9554 has 8 gpio bits
#define kNumGPIOs	8

// gpio id 3 is drivePresent, gpio id 4 is driveSwitch
enum {
	kPresent = 3,
	kSwitch = 4
};

// power states
enum {
	kI2CGPIOPowerOff = 0,
	kI2CGPIOPowerOn = 1,
	kI2CGPIONumPowerStates = 2
};

class I2CGPIO : public IOService
{
	OSDeclareDefaultStructors(I2CGPIO)

	private:
					
		// i2c addresses
		UInt8	fI2CBus;
		UInt8	fI2CAddress;
		
		// the pmu i2c interface object
		IOService *fI2CInterface;
		IOService *fKeyswitch;

		// Symbols used to talk to the I2C driver.

		const OSSymbol *fSymOpenI2CBus;
		const OSSymbol *fSymReadI2CBus;
		const OSSymbol *fSymWriteI2CBus;
		const OSSymbol *fSymCloseI2CBus;
		const OSSymbol *fSymSetCombinedMode;
		const OSSymbol *fSymSetStandardSubMode;
		const OSSymbol *fSymRegisterForInts;
		const OSSymbol *fSymDeRegisterForInts;

		// power management stuff
		unsigned long fCurrentPowerState;
		UInt8 fConfigReg;
		UInt8 fPolarityReg;
		UInt8 fOutputReg;

#ifdef I2CGPIO_DEBUG
		UInt32	fWriteCookie;
		UInt32	fReadCookie;
#endif

		// i2c interrupt handling
		bool	fC3Mapping;
		bool	fRegisteredWithI2C;		// set to true if I am registered for
										// my interrupts
		IOLock	*fRegisteredWithI2CLock;

		UInt32	fBayIndex;
		UInt32	fIntAddrInfo;	// i2c address of consolidated register
								// pmu polls this register and generates interrupts
								// when it changes.  The value is as follows:
								// 0x00014900
								//    | | | +---- sub address
								//    | | +------ address (with read bit set)
								//    | +-------- bus number
								//    +---------- ignored
		UInt8	fIntRegState;	// last known state of the consolidated register

		// my gpio children who have registered for interrupt notification
		I2CGPIOCallbackInfo	*fClients[kNumGPIOs];

		// read and write to the i2c bus
		IOReturn doI2CWrite(UInt8 busNo, UInt8 addr, UInt8 subAddr, UInt8 mask, UInt8 value);
		IOReturn doI2CRead(UInt8 busNo, UInt8 addr, UInt8 subAddr, UInt8 *value);

		// registration routines used when children ask for notification
		bool registerClient(void *param1, void *param2, void *param3, void *param4);
		bool unregisterClient(void *param1, void *param2, void *param3, void *param4);

		bool enableClient(void *param1, void *param2, void *param3,
				void *param4);

		bool disableClient(void *param1, void *param2, void *param3,
				void *param4);

		// instance event handler method, called by sI2CEventOccured
		void handleEvent(UInt32 addressInfo, UInt32 length, UInt8 *buffer);

		// register with I2C driver for interrupt event notification
		bool registerWithI2C(void);
		void unregisterWithI2C(void);

		// set up child device nubs
		IOService *createNub(IORegistryEntry *from);
		void processNub(IOService *myNub);
		void publishBelow(IORegistryEntry *root);
		
		// power management
		//virtual IOReturn powerStateWillChangeTo(IOPMPowerFlags flags,
		//		unsigned long stateNumber, IOService *whatDevice);
		//virtual IOReturn powerStateDidChangeTo(IOPMPowerFlags flags,
		//		unsigned long stateNumber, IOService *whatDevice);
		void doPowerDown(void);
		void doPowerUp(void);

	public:
        virtual bool init(OSDictionary *dict);
        virtual void free(void);
        virtual IOService *probe(IOService *provider, SInt32 *score);
        virtual bool start(IOService *provider);
        virtual void stop(IOService *provider);

		// power controller callback
		virtual IOReturn setPowerState(unsigned long powerStateOrdinal,
				IOService *whatDevice);

		// GPIO reads and writes are services through the callPlatformFunction
		// mechanism
		virtual IOReturn callPlatformFunction( const OSSymbol *functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

		virtual IOReturn callPlatformFunction( const char *functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

		// static event handler - it just calls the instance method of the
		// interested driver
		static void sI2CEventOccured(I2CGPIO *client, UInt32 addressInfo,
				UInt32 length, UInt8 *buffer);
};

// This is a nub class for AppleGPIO to attach to
class I2CGPIODevice : public IOService
{
	OSDeclareDefaultStructors(I2CGPIODevice)

	virtual bool compareName(OSString *name, OSString **matched) const;
};

#endif // _I2CGPIO_H