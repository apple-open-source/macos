/*
 *	PlatformInterfaceFCR.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_FCR_PlatformFunction__
#define	__PLATFORMINTERFACE_FCR_PlatformFunction__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceFCR.h"
#include	"AudioHardwareCommon.h"
#include	"AppleOnboardAudioUserClient.h"
#include	<IOKit/i2c/PPCI2CInterface.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	<IOKit/IOFilterInterruptEventSource.h>
#include	<IOKit/IOWorkLoop.h>
#include	<IOKit/IORegistryEntry.h>
#include	<IOKit/IOCommandGate.h>
#include	<IOKit/ppc/IODBDMA.h>
#include	"PlatformInterfaceSupportPlatformFunctionCommon.h"

class PlatformInterfaceFCR_PlatformFunction : public PlatformInterfaceFCR {

    OSDeclareDefaultStructors ( PlatformInterfaceFCR_PlatformFunction );

public:	
	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free();

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState );

	//
	// I2S Methods: FCR1
	//
	virtual bool						getI2SCellEnable();
	virtual bool						getI2SClockEnable();
	virtual bool						getI2SEnable ();
	virtual bool						getI2SSWReset ();

	virtual IOReturn					setI2SCellEnable ( bool enable );
	virtual IOReturn					setI2SClockEnable ( bool enable );
	virtual IOReturn					setI2SEnable ( bool enable );
	virtual IOReturn					setI2SSWReset ( bool enable );
	
	virtual IOReturn					releaseI2SClockSource ( I2SClockFrequency inFrequency );
	virtual IOReturn					requestI2SClockSource ( I2SClockFrequency inFrequency );
	
protected:

	virtual IOReturn					makeSymbolAndCallPlatformFunctionNoWait ( const char * name, void * param1, void * param2, void * param3, void * param4 );
	inline 	const OSSymbol*				makeFunctionSymbolName ( const char * name,UInt32 pHandle );
	virtual IOReturn					setupI2SClockSource( UInt32 cell, bool requestClock, UInt32 clockSource );

	static const char *					kAppleI2S_Enable;
	static const char *					kAppleI2S_Disable;
	static const char *					kAppleI2S_ClockEnable;
	static const char *					kAppleI2S_ClockDisable;
	static const char *					kAppleI2S_Reset;
	static const char *					kAppleI2S_Run;
	static const char *					kAppleI2S_CellEnable;
	static const char *					kAppleI2S_CellDisable;
	static const char *					kAppleI2S_GetEnable;
	static const char *					kAppleI2S_GetClockEnable;
	static const char *					kAppleI2S_GetReset;
	static const char *					kAppleI2S_GetCellEnable;

    IOService *							mSystemIOControllerService;
	IORegistryEntry *					mI2S;
	UInt32								mI2SPHandle;
	UInt32								mI2SOffset;
	UInt32								mMacIOPHandle;
	UInt32								mMacIOOffset;

	typedef enum {
		kK2I2SClockSource_45MHz			= 0,			//	compatible with K2 driver
		kK2I2SClockSource_49MHz			= 1,			//	compatible with K2 driver
		kK2I2SClockSource_18MHz 		= 2				//	compatible with K2 driver
	} K2I2SClockSource;

	I2SCell								mI2SInterfaceNumber;
	
	bool								mAppleI2S_CellEnable;
	bool								mAppleI2S_ClockEnable;
	bool								mAppleI2S_Enable;
	bool								mAppleI2S_Reset;

};

#endif	/*	__PLATFORMINTERFACE_FCR_PlatformFunction__	*/
