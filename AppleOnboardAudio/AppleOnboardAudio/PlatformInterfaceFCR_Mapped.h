/*
 *	PlatformInterfaceFCR_Mapped.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_FCR_Mapped__
#define	__PLATFORMINTERFACE_FCR_Mapped__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceFCR.h"
#include	"PlatformInterfaceSupportMappedCommon.h"

class PlatformInterfaceFCR_Mapped : public PlatformInterfaceFCR {

    OSDeclareDefaultStructors ( PlatformInterfaceFCR_Mapped );

public:	
	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free();

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState );

	//
	// FCR Methods:
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

	inline UInt32						getFCR1 ();
	inline UInt32						getFCR3 ();
	inline UInt32						getKeyLargoRegister ( void *klRegister );

	inline void							setFCR1 ( UInt32 value );
	inline void							setFCR3 ( UInt32 value );
	inline void							setKeyLargoRegister ( void *klRegister, UInt32 value );	

	virtual bool						getI2SCellBitState ( UInt32 bitMask );
	virtual	IOReturn					setI2SCellBitState ( bool bitState, UInt32 bitMask );

    IOService *							mKeyLargoService;
	const OSSymbol *					mKLI2SPowerSymbolName;
	void *								mSoundConfigSpace;		  		// address of sound config space
	void *								mIOBaseAddress;		   			// base address of our I/O controller
	IODeviceMemory *					mIOBaseAddressMemory;			// Have to free this in free()
	I2SCell								mI2SInterfaceNumber;
	void *								mIOConfigurationBaseAddress;	// base address for the configuration registers
	IODeviceMemory *					mIOI2SBaseAddressMemory;
	void *								mI2SBaseAddress;				//	base address of I2S I/O Module
	
	static const UInt16 kAPPLE_IO_CONFIGURATION_SIZE;
	static const UInt16 kI2S_IO_CONFIGURATION_SIZE;

	static const UInt32 kFCR0Offset;
	static const UInt32 kFCR1Offset;
	static const UInt32 kFCR2Offset;
	static const UInt32 kFCR3Offset;
	static const UInt32 kFCR4Offset;
	
	static const UInt32 kI2S0BaseOffset;							/*	mapped by AudioI2SControl	*/
	static const UInt32 kI2S1BaseOffset;							/*	mapped by AudioI2SControl	*/

	static const UInt32 kI2SClockOffset;
	static const UInt32 kI2S0ClockEnable;
	static const UInt32 kI2S1ClockEnable;
	static const UInt32 kI2S0CellEnable;
	static const UInt32 kI2S1CellEnable;
	static const UInt32 kI2S0InterfaceEnable;
	static const UInt32 kI2S1InterfaceEnable;
	static const UInt32 kI2S0SwReset;
	static const UInt32 kI2S1SwReset;

};

#endif	/*	__PLATFORMINTERFACE_FCR_Mapped__	*/
