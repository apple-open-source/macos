/*
 *	PlatformInterfaceI2S_PlatformFunction.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#ifndef __PLATFORMINTERFACE_I2S_PlatformFunction__
#define	__PLATFORMINTERFACE_I2S_PlatformFunction__
 
#include	<IOKit/IOService.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	"AudioHardwareConstants.h"
#include	"AudioHardwareUtilities.h"
#include	"PlatformInterfaceI2S.h"
#include	"PlatformInterfaceSupportPlatformFunctionCommon.h"

#include	<IOKit/i2c/PPCI2CInterface.h>
#include	<IOKit/IOInterruptEventSource.h>
#include	<IOKit/IOFilterInterruptEventSource.h>
#include	<IOKit/IOWorkLoop.h>
#include	<IOKit/IORegistryEntry.h>
#include	<IOKit/IOCommandGate.h>

// callPlatformFunction symbols for AppleI2S
#define kI2SGetIntCtlReg			"I2SGetIntCtlReg"
#define kI2SSetIntCtlReg			"I2SSetIntCtlReg"
#define kI2SGetSerialFormatReg		"I2SGetSerialFormatReg"
#define kI2SSetSerialFormatReg		"I2SSetSerialFormatReg"
#define kI2SGetCodecMsgOutReg		"I2SGetCodecMsgOutReg"
#define kI2SSetCodecMsgOutReg		"I2SSetCodecMsgOutReg"
#define kI2SGetCodecMsgInReg		"I2SGetCodecMsgInReg"
#define kI2SSetCodecMsgInReg		"I2SSetCodecMsgInReg"
#define kI2SGetFrameCountReg		"I2SGetFrameCountReg"
#define kI2SSetFrameCountReg		"I2SSetFrameCountReg"
#define kI2SGetFrameMatchReg		"I2SGetFrameMatchReg"
#define kI2SSetFrameMatchReg		"I2SSetFrameMatchReg"
#define kI2SGetDataWordSizesReg		"I2SGetDataWordSizesReg"
#define kI2SSetDataWordSizesReg		"I2SSetDataWordSizesReg"
#define kI2SGetPeakLevelSelReg		"I2SGetPeakLevelSelReg"
#define kI2SSetPeakLevelSelReg		"I2SSetPeakLevelSelReg"
#define kI2SGetPeakLevelIn0Reg		"I2SGetPeakLevelIn0Reg"
#define kI2SSetPeakLevelIn0Reg		"I2SSetPeakLevelIn0Reg"
#define kI2SGetPeakLevelIn1Reg		"I2SGetPeakLevelIn1Reg"
#define kI2SSetPeakLevelIn1Reg		"I2SSetPeakLevelIn1Reg"

class AppleI2S : public IOService
{
	OSDeclareDefaultStructors(AppleI2S)

	private:
		// key largo gives us register access
    	IOService	*keyLargoDrv;			

		// i2s register space offset relative to key largo base address
		UInt32 i2sBaseAddress;
		
		// child publishing methods
    	void publishBelow(IOService *provider);

	public:
        virtual bool init(OSDictionary *dict);
        virtual void free(void);
        virtual IOService *probe(IOService *provider, SInt32 *score);
        virtual bool start(IOService *provider);
        virtual void stop(IOService *provider);
        
		// AppleI2S reads and writes are services through the callPlatformFunction
		virtual IOReturn callPlatformFunction( const char *functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

		virtual IOReturn callPlatformFunction( const OSSymbol *functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

};

class PlatformInterfaceI2S_PlatformFunction : public PlatformInterfaceI2S {

    OSDeclareDefaultStructors ( PlatformInterfaceI2S_PlatformFunction );

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

	virtual IOReturn					makeSymbolAndCallPlatformFunctionNoWait ( const char * name, void * param1, void * param2, void * param3, void * param4 );
	virtual bool						findAndAttachI2S ();
	virtual bool						openI2S ();
	virtual void						closeI2S ();
	virtual bool						detachFromI2S();

    IOService *							mSystemIOControllerService
	;
	UInt32								mI2SCell;
	IORegistryEntry *					mI2S;
	UInt32								mI2SPHandle;
	UInt32								mI2SOffset;
	UInt32								mMacIOPHandle;
	UInt32								mMacIOOffset;
	AppleI2S *							mI2SInterface;
	I2SCell								mI2SInterfaceNumber;

};

#endif	/*	__PLATFORMINTERFACE_I2S_PlatformFunction__	*/
