/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CDriveBayGPIO.h,v 1.1 2004/09/18 00:27:51 jlehrer Exp $
 *
 *		$Log: IOI2CDriveBayGPIO.h,v $
 *		Revision 1.1  2004/09/18 00:27:51  jlehrer
 *		Initial checkin
 *		
 *		
 *
 */


#ifndef _IOI2CDriveBayGPIO_H
#define _IOI2CDriveBayGPIO_H

#include <IOKit/IOService.h>
#include <IOI2C/IOI2CDevice.h>
#include "GPIOParent.h"

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

class IOI2CDriveBayGPIO : public IOI2CDevice
{
	OSDeclareDefaultStructors(IOI2CDriveBayGPIO)

public:
//	virtual bool init(OSDictionary *dict);
	virtual void free(void);
	virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);

	using IOI2CDevice::callPlatformFunction;
	virtual IOReturn callPlatformFunction( const OSSymbol *functionName,
					bool waitForFunction,
					void *param1, void *param2,
					void *param3, void *param4 );

private:
	IOService	*fPCA9554M;

	UInt8 fConfigReg;
	UInt8 fPolarityReg;
	UInt8 fOutputReg;

	bool	fC3Mapping;
	UInt32	fBayIndex;
	UInt32	fIntAddrInfo;	// i2c address of consolidated register
							// pmu polls this register and generates interrupts
							// when it changes.  The value is as follows:
							// 0x00014900
							//    | | | +---- sub address
							//    | | +------ address (with read bit set)
							//    | +-------- bus number
							//    +---------- ignored

	IOLock				*fClientLock;
	I2CGPIOCallbackInfo	*fClient[kNumGPIOs];
	UInt32				 fClientsEnabled;

	IOReturn registerGPIOClient(
		UInt32				id,
		GPIOEventHandler	handler,
		IOService			*client,
		bool				isRegister);

	IOReturn enableGPIOClient(
		UInt32				id,
		bool				isEnable);

	static void sProcess9554MInterrupt(
		IOI2CDriveBayGPIO	*client,
		UInt8				eventMask,
		UInt8				newState);

	void process9554MInterrupt(
		UInt8				eventMask,
		UInt8				newState);


	IOReturn readModifyWriteI2C(
		UInt8		subAddr,
		UInt8		value,
		UInt8		mask);

	// set up child device nubs
	IOReturn publishChildren(IOService *nub);
	
	virtual void processPowerEvent(UInt32 eventType);
	void doPowerDown(void);
	void doPowerUp(void);
};

#endif // _IOI2CDriveBayGPIO_H