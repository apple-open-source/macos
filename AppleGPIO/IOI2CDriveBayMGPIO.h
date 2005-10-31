/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CDriveBayMGPIO.h,v 1.2 2005/02/09 02:21:41 jlehrer Exp $
 *
 *		$Log: IOI2CDriveBayMGPIO.h,v $
 *		Revision 1.2  2005/02/09 02:21:41  jlehrer
 *		Updated interrupt processing for mac-io gpio-16
 *		
 *		Revision 1.1  2004/09/18 00:27:51  jlehrer
 *		Initial checkin
 *		
 *		
 *
 */

#ifndef _IOI2CDriveBayMGPIO_H
#define _IOI2CDriveBayMGPIO_H

#include <IOKit/IOService.h>
#include <IOI2C/IOI2CDevice.h>
#include "GPIOParent.h"

class IOI2CDriveBayMGPIO : public IOI2CDevice
{
	OSDeclareDefaultStructors(IOI2CDriveBayMGPIO)

public:
//	virtual bool init(OSDictionary *dict);
	virtual void free(void);
	virtual bool start(IOService *provider);

	using IOI2CDevice::callPlatformFunction;
	virtual IOReturn callPlatformFunction(
		const OSSymbol *functionName,
		bool waitForFunction,
		void *param1, void *param2,
		void *param3, void *param4 );

private:
	typedef void (*PCA9554ClientCallback)(IOService *, UInt8, UInt8);

	// structure to store info about AppleGPIO interrupt clients
	typedef struct
	{
		UInt32					reg;
		PCA9554ClientCallback	handler;
		IOService				*client;
		bool					isEnabled;
	} PCA9554CallbackInfo;

	// gpio id 3 is drivePresent, gpio id 4 is driveSwitch
	enum
	{
		kPresent = 3,
		kSwitch = 4
	};

	IOService			*fApplePMU;
	IOService			*fKeyswitch;
	IOService			*fDrivebaySense;
	const OSSymbol		*fDrivebaySenseSym;

	// Symbols for interrupt registration and enable/disable support
	const OSSymbol		*fSymIntRegister;
	const OSSymbol		*fSymIntUnRegister;
	const OSSymbol		*fSymIntEnable;
	const OSSymbol		*fSymIntDisable;

	bool				fC3Mapping;
	UInt32				fClientCount;
	IOLock				*fClientLock;

	enum
	{
		kFlag_InterruptsRegistered		= (1 << 0),
		kFlag_InterruptsEnabled			= (1 << 1),
	};
	UInt32				fFlags;

	// i2c address of consolidated register
	// pmu automagically polls this register and generates interrupts
	// when it changes.  The value is as follows:
	// 0x00014900
	//    | | | +---- sub address
	//    | | +------ address (with read bit set)
	//    | +-------- bus number
	//    +---------- ignored
	UInt32		fIntAddrInfo;
	UInt8		fIntRegState;	// last known state of the consolidated register

	UInt8 fConfigReg;
	UInt8 fPolarityReg;
	UInt8 fOutputReg;

	// my gpio children who have registered for interrupt notification
	#define kCLIENT_MAX 4
	PCA9554CallbackInfo	fClient[kCLIENT_MAX];

	// Client API
	IOReturn registerClient(
		UInt32				id,
		PCA9554ClientCallback	handler,
		IOService			*client,
		bool				isRegister);

	IOReturn enableClient(
		UInt32				id,
		bool				isEnable);

	static void sProcessApplePMUInterrupt(
		IOService			*client,
		UInt8				addressInfo,
		UInt32				length,
		UInt8				*buffer);

	static void sProcessGPIOInterrupt(
		IOI2CDriveBayMGPIO	*self,
		void				*param2,
		void				*param3,
		UInt8				newData);

	void processGPIOInterrupt(
		UInt8				newState);

	virtual void processPowerEvent(UInt32 eventType);
};

#endif // _IOI2CPCA9554_H