/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CDriveBayMGPIO.h,v 1.1 2004/09/18 00:27:51 jlehrer Exp $
 *
 *		$Log: IOI2CDriveBayMGPIO.h,v $
 *		Revision 1.1  2004/09/18 00:27:51  jlehrer
 *		Initial checkin
 *		
 *		
 *
 */

#ifndef _IOI2CDriveBayMGPIO_H
#define _IOI2CDriveBayMGPIO_H

#include <IOKit/IOService.h>
#include "GPIOParent.h"

class IOI2CDriveBayMGPIO : public IOService
{
	OSDeclareDefaultStructors(IOI2CDriveBayMGPIO)

public:
	virtual bool init(OSDictionary *dict);
	virtual void free(void);
	virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);

	using IOService::callPlatformFunction;
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

	IOService	*fApplePMU;
	IOService	*fKeyswitch;

	bool		fC3Mapping;
	UInt32		fClientCount;
	IOLock		*fClientLock;

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

	// my gpio children who have registered for interrupt notification
	PCA9554CallbackInfo	**fClient;

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

	void processApplePMUInterrupt(
		UInt8				newState);
};

#endif // _IOI2CPCA9554_H