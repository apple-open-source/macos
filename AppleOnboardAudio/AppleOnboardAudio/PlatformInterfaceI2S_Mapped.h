/*
 *	PlatformInterfaceI2S_Mapped.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_I2S_Mapped__
#define	__PLATFORMINTERFACE_I2S_Mapped__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	"AudioHardwareConstants.h"
#include	"PlatformInterfaceI2S.h"
#include	"PlatformInterfaceSupportMappedCommon.h"

class PlatformInterfaceI2S_Mapped : public PlatformInterfaceI2S {

    OSDeclareDefaultStructors ( PlatformInterfaceI2S_Mapped );

public:	

	virtual bool						init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex );
	virtual	void						free();

	virtual IOReturn					performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState );

	//
	// I2S Methods: IOM Control
	//
	virtual IOReturn					setSerialFormatRegister ( UInt32 serialFormat );
	virtual UInt32						getSerialFormatRegister ();

	virtual IOReturn					setDataWordSizes ( UInt32 dataWordSizes );
	virtual UInt32						getDataWordSizes();
	
	virtual IOReturn					setFrameCount ( UInt32 value );
	virtual UInt32						getFrameCount ();

	virtual IOReturn					setI2SIOMIntControl ( UInt32 intCntrl );
	virtual UInt32						getI2SIOMIntControl ();
	
	virtual IOReturn					setPeakLevel ( UInt32 channelTarget, UInt32 levelMeterValue );
	virtual UInt32						getPeakLevel ( UInt32 channelTarget );
	
	virtual IOReturn					setI2SIOM_CodecMsgOut ( UInt32 value );
	virtual UInt32						getI2SIOM_CodecMsgOut ();
	
	virtual IOReturn					setI2SIOM_CodecMsgIn ( UInt32 value );
	virtual UInt32						getI2SIOM_CodecMsgIn ();
	
	virtual IOReturn					setI2SIOM_FrameMatch ( UInt32 value );
	virtual UInt32						getI2SIOM_FrameMatch ();
	
	virtual IOReturn					setI2SIOM_PeakLevelSel ( UInt32 value );
	virtual UInt32						getI2SIOM_PeakLevelSel ();
	
	virtual IOReturn					setI2SIOM_PeakLevelIn0 ( UInt32 value );
	virtual UInt32						getI2SIOM_PeakLevelIn0 ();
	
	virtual IOReturn					setI2SIOM_PeakLevelIn1 ( UInt32 value );
	virtual UInt32						getI2SIOM_PeakLevelIn1 ();
	
protected:

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

	static const UInt32 kI2SIntCtlOffset;
	static const UInt32 kI2SSerialFormatOffset;
	static const UInt32 kI2SCodecMsgOutOffset;
	static const UInt32 kI2SCodecMsgInOffset;
	static const UInt32 kI2SFrameCountOffset;
	static const UInt32 kI2SFrameMatchOffset;
	static const UInt32 kI2SDataWordSizesOffset;
	static const UInt32 kI2SPeakLevelSelOffset;
	static const UInt32 kI2SPeakLevelIn0Offset;
	static const UInt32 kI2SPeakLevelIn1Offset;

};

#endif	/*	__PLATFORMINTERFACE_I2S_Mapped__	*/
