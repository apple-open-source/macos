/*
 *	PlatformInterfaceI2C_Mapped.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_I2C_Mapped__
#define	__PLATFORMINTERFACE_I2C_Mapped__
 
#include	<IOKit/i2c/PPCI2CInterface.h>
#include	<IOKit/IOService.h>
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceI2C.h"
#include	"PlatformInterfaceSupportMappedCommon.h"

class PlatformInterfaceI2C_Mapped : public PlatformInterfaceI2C {

    OSDeclareDefaultStructors ( PlatformInterfaceI2C_Mapped );

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

	virtual IOReturn					setupForI2CTransaction ( UInt32 codecRef, UInt8 * i2cDeviceAddress, UInt8 * i2cBusProtocol, bool direction );
	virtual bool						findAndAttachI2C();
	virtual bool						detachFromI2C();
	virtual bool						openI2C();
	virtual void						closeI2C();
	virtual	bool						codecHasMAP ( UInt32 codecRef );

	UInt32								mI2CPort;
	bool								mI2C_lastTransactionResult;
	PPCI2CInterface *					mI2CInterface;

    IOService *							mKeyLargoService;
	const OSSymbol *					mKLI2SPowerSymbolName;
	void *								mSoundConfigSpace;		  		// address of sound config space
	void *								mIOBaseAddress;		   			// base address of our I/O controller
	void *								mIOConfigurationBaseAddress;	// base address for the configuration registers
	void *								mI2SBaseAddress;				//	base address of I2S I/O Module
	IODeviceMemory *					mIOBaseAddressMemory;			// Have to free this in free()
	IODeviceMemory *					mIOI2SBaseAddressMemory;
	I2SCell								mI2SInterfaceNumber;

	static const UInt16 kAPPLE_IO_CONFIGURATION_SIZE;
	static const UInt16 kI2S_IO_CONFIGURATION_SIZE;

	static const UInt32 kI2S0BaseOffset;							/*	mapped by AudioI2SControl	*/
	static const UInt32 kI2S1BaseOffset;							/*	mapped by AudioI2SControl	*/

};

#endif	/*	__PLATFORMINTERFACE_I2C_Mapped__	*/
