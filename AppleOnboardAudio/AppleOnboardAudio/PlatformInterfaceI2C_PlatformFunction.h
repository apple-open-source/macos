/*
 *	PlatformInterfaceI2C_PlatformFunction_PlatformFunction.h
 *
 *	
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_I2C_PlatformFunction__
#define	__PLATFORMINTERFACE_I2C_PlatformFunction__
 
#include	<IOKit/IOService.h>
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceI2C.h"
#include	"PlatformInterfaceSupportPlatformFunctionCommon.h"

#include	<IOKit/i2c/PPCI2CInterface.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	<IOKit/IOFilterInterruptEventSource.h>
#include	<IOKit/IOWorkLoop.h>
#include	<IOKit/IORegistryEntry.h>
#include	<IOKit/IOCommandGate.h>

class PlatformInterfaceI2C_PlatformFunction : public PlatformInterfaceI2C {

    OSDeclareDefaultStructors ( PlatformInterfaceI2C_PlatformFunction );

public:	

	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free();

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState );

	//
	// Codec Methods
	//

	virtual IOReturn					readCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ); 
	virtual IOReturn					WriteCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ); 
	virtual IOReturn					setMAP ( UInt32 codecRef, UInt8 subAddress );

protected:

	inline 	const OSSymbol*				makeFunctionSymbolName ( const char * name,UInt32 pHandle );
	virtual	bool						codecHasMAP ( UInt32 codecRef );

    IOService *							mSystemIOControllerService;
	UInt32								mI2SCell;
	IORegistryEntry *					mI2S;
	UInt32								mI2SPHandle;
	UInt32								mI2SOffset;
	UInt32								mMacIOPHandle;
	UInt32								mMacIOOffset;
	I2SCell								mI2SInterfaceNumber;

	IOService *							mCodecIOServiceArray[kCodec_NumberOfTypes];
	
	static const char *					kPlatformTas3004CodecRef;
	static const char *					kPlatformTopazCodecRef;
	static const char *					kPlatformOnyxCodecRef;

	static const char *					kPlatformDoTasCodecRef;
	static const char *					kPlatformDoTopazCodecRef;
	static const char *					kPlatformDoOnyxCodecRef;

	UInt32								mNumberOfPowerParents;
	UInt32								mPowerParentKeys[kCodec_NumberOfTypes];

private:

	typedef struct
	{
		UInt32		_reserved[4];
		UInt32		command;
		UInt32		bus;
		UInt32		address;
		UInt32		subAddress;
		UInt8		*buffer;
		UInt32		count;
		UInt32		mode;
		UInt32		retries;
		UInt32		timeout_uS;
		UInt32		speed;
		UInt32		options;
		UInt32		reserved[4];

	} IOI2CCommand;

	/*! @enum kI2CUCxxx Indices for IOI2CUserClient externally accessible functions... */
	enum
	{
		kI2CUCLock,			// ScalarIScalarO
		kI2CUCUnlock,		// ScalarIScalarO
		kI2CUCRead,			// StructIStructO
		kI2CUCWrite,		// StructIStructO
		kI2CUCRMW,			// StructIStructO

		kI2CUCNumMethods
	};

	/*! @enum kIOI2CCommand_xxx I2C transaction constants. */
	enum
	{
		kI2CCommand_Read		= 0,
		kI2CCommand_Write		= 1,
	};

	/*! @enum kI2CMode_xxx I2C transaction mode constants. */
	enum
	{
		kI2CMode_Unspecified	= 0,
		kI2CMode_Standard		= 1,
		kI2CMode_StandardSub	= 2,
		kI2CMode_Combined		= 3,
	};



};

#endif	/*	__PLATFORMINTERFACE_I2C_PlatformFunction__	*/
