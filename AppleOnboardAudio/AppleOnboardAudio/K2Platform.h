/*
 *  K2Platform.h
 *  
 *
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterface.h"
#include "AudioHardwareCommon.h"
#include "AppleOnboardAudioUserClient.h"
#include <IOKit/i2c/PPCI2CInterface.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/ppc/IODBDMA.h>

#ifndef _APPLEI2S_H
#define _APPLEI2S_H

#include <IOKit/IOService.h>

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
//#define APPLEI2S_DEBUG 1

#ifdef APPLEI2S_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

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

/* --- this is the i2s device nub class that i2s-a or i2s-b drivers attach to --- */
//class AppleI2SDevice : public IOService
//{
//	OSDeclareDefaultStructors(AppleI2SDevice)
//	virtual bool compareName(OSString *name, OSString **matched) const;
//
//};

// callPlatformFunction symbols to access key largo or K2 registers
#define kSafeWriteRegUInt32 	"keyLargo_safeWriteRegUInt32"
#define kSafeReadRegUInt32		"keyLargo_safeReadRegUInt32"

// K2 register write is always passed  noMask for mask value
#define 	noMask 					0xFFFFFFFF

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

// I2S Register offsets within keyLargo or K2
#define		kI2SIntCtlOffset		0x0000
#define		kI2SSerialFormatOffset	0x0010
#define		kI2SCodecMsgOutOffset	0x0020
#define		kI2SCodecMsgInOffset	0x0030
#define		kI2SFrameCountOffset	0x0040
#define		kI2SFrameMatchOffset	0x0050
#define		kI2SDataWordSizesOffset	0x0060
#define		kI2SPeakLevelSelOffset	0x0070
#define		kI2SPeakLevelIn0Offset	0x0080
#define		kI2SPeakLevelIn1Offset	0x0090

// The following is actually read from the "reg" property in the tree.
#define		kI2SBaseOffset			0x10000

#define	kIOPFInterruptRegister		"IOPFInterruptRegister"
#define	kIOPFInterruptUnRegister	"IOPFInterruptUnRegister"
#define	kIOPFInterruptEnable		"IOPFInterruptEnable"
#define	kIOPFInterruptDisable		"IOPFInterruptDisable"

#endif /* ! _APPLEI2S_H */


#ifndef __K2_PLATFORM
#define	__K2_PLATFORM

#define	kIODMAInputOffset			0x00000100					/*	offset from I2S 0 Tx DMA channel registers to Rx DMA channel registers	*/
#define kIODMASizeOfChannelBuffer			256
	
typedef enum {
	kK2I2SClockSource_45MHz					= 0,			//	compatible with K2 driver
	kK2I2SClockSource_49MHz					= 1,			//	compatible with K2 driver
	kK2I2SClockSource_18MHz 				= 2			//	compatible with K2 driver
} K2I2SClockSource;

class K2Platform : public PlatformInterface {

    OSDeclareDefaultStructors(K2Platform);

public:

	virtual bool			init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex);
	virtual	void			free();

	virtual IOReturn		performPlatformSleep ( void );
	virtual IOReturn		performPlatformWake ( IOService * device );

	//
	// Codec Methods
	//
	virtual bool			readCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode);
	virtual bool			writeCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode);

	virtual IOReturn		setCodecReset ( CODEC_RESET target, GpioAttributes reset );
	virtual GpioAttributes	getCodecReset ( CODEC_RESET target );
	//
	// I2S Methods: FCR3
	//
	virtual IOReturn		requestI2SClockSource(I2SClockFrequency inFrequency);
	virtual IOReturn		releaseI2SClockSource(I2SClockFrequency inFrequency);
	//
	// I2S Methods: FCR1
	//
	virtual IOReturn		setI2SEnable(bool enable);
	virtual bool			getI2SEnable();

	virtual IOReturn		setI2SClockEnable(bool enable);
	virtual bool			getI2SClockEnable();

	virtual IOReturn		setI2SCellEnable(bool enable);
	virtual bool			getI2SCellEnable();
	
	virtual IOReturn		setI2SSWReset(bool enable);
	virtual bool			getI2SSWReset();
	//
	// I2S Methods: IOM Control
	//
	virtual IOReturn		setSerialFormatRegister(UInt32 serialFormat);
	virtual UInt32			getSerialFormatRegister();

	virtual IOReturn		setDataWordSizes(UInt32 dataWordSizes);
	virtual UInt32			getDataWordSizes();
	
	virtual IOReturn		setFrameCount(UInt32 value);
	virtual UInt32			getFrameCount();

	virtual IOReturn		setI2SIOMIntControl(UInt32 intCntrl);
	virtual UInt32			getI2SIOMIntControl();
	//
	// GPIO Methods
	//

	virtual	GpioAttributes	getComboInJackTypeConnected();
	virtual	GpioAttributes	getComboOutJackTypeConnected();
	virtual	GpioAttributes	getDigitalInConnected();
	virtual	GpioAttributes	getDigitalOutConnected();
	virtual GpioAttributes	getLineInConnected();
	virtual GpioAttributes	getLineOutConnected(bool ignoreCombo = false);
	virtual GpioAttributes 	getHeadphoneConnected();
	virtual GpioAttributes	getSpeakerConnected();
	virtual GpioAttributes	getCodecInterrupt();
	virtual GpioAttributes	getCodecErrorInterrupt();
	virtual IOReturn 		setHeadphoneMuteState(GpioAttributes muteState);
	virtual GpioAttributes 	getHeadphoneMuteState();
	virtual IOReturn		setInputDataMux(GpioAttributes muxState);
	virtual GpioAttributes	getInputDataMux();
	virtual IOReturn 		setLineOutMuteState(GpioAttributes muteState);
	virtual GpioAttributes 	getLineOutMuteState();
	virtual IOReturn 		setSpeakerMuteState(GpioAttributes muteState);
	virtual GpioAttributes 	getSpeakerMuteState();
	virtual IOReturn		setClockMux(GpioAttributes muxState);
	virtual GpioAttributes	getClockMux();

//	virtual bool	 		getInternalSpeakerID();

	//
	// Set Interrupt Handler Methods
	//
	
	virtual IOReturn		disableInterrupt ( PlatformInterruptSource source );
	virtual IOReturn		enableInterrupt ( PlatformInterruptSource source );
	virtual IOReturn		registerInterruptHandler (IOService * theDevice, void * interruptHandler, PlatformInterruptSource source );
	virtual IOReturn		unregisterInterruptHandler (IOService * theDevice, void * interruptHandler, PlatformInterruptSource source );

	inline 	const OSSymbol* makeFunctionSymbolName(const char * name,UInt32 pHandle);

	//
	// DBDMA Memory Address Acquisition Methods
	//
	virtual	IODBDMAChannelRegisters *	GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );
	virtual	IODBDMAChannelRegisters *	GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );

	//
	//	User Client Support
	//
	virtual IOReturn			getPlatformState ( PlatformStateStructPtr outState );
	virtual IOReturn			setPlatformState ( PlatformStateStructPtr inState );

        virtual PlatformInterfaceObjectType	getPlatformInterfaceType () { return kPlatformInterfaceType_K2; }
protected:

	typedef enum {
		kDMADeviceIndex		= 0,
		kDMAOutputIndex		= 1,
		kDMAInputIndex		= 2,
		kDMANumberOfIndexes	= 3
	} PlatformDMAIndexes;

    IOService *					mK2Service;
	UInt32						mI2SCell;
	IORegistryEntry				*mI2S;
	UInt32						mI2SPHandle;
	UInt32						mI2SOffset;
	UInt32						mMacIOPHandle;
	UInt32						mMacIOOffset;
	IODBDMAChannelRegisters *	mIOBaseDMA[4];

	static const char * 	kAppleK2pHandle;
	static const char * 	kAppleI2S0pHandle;
	static const char * 	kAppleGPIOpHandle;
	static const char * 	kAppleI2S_Enable;
	static const char * 	kAppleI2S_Disable;
	static const char * 	kAppleI2S_ClockEnable;
	static const char * 	kAppleI2S_ClockDisable;
	static const char * 	kAppleI2S_Reset;
	static const char * 	kAppleI2S_Run;
	static const char * 	kAppleI2S_CellEnable;
	static const char * 	kAppleI2S_CellDisable;
	static const char * 	kAppleI2S_GetEnable;
	static const char * 	kAppleI2S_GetClockEnable;
	static const char * 	kAppleI2S_GetReset;
	static const char * 	kAppleI2S_GetCellEnable;
	static const char * 	kAppleI2S_SetIntCtrl;
	static const char * 	kAppleI2S_SetSerialFormat;
	static const char * 	kAppleI2S_SetCodecMessageOut;
	static const char * 	kAppleI2S_SetCodecMessageIn;
	static const char * 	kAppleI2S_SetFrameCount;
	static const char * 	kAppleI2S_SetFrameCountToMatch;
	static const char * 	kAppleI2S_SetDataWordSizes;
	static const char * 	kAppleI2S_SetPeakLevelSFSelect;
	static const char * 	kAppleI2S_SetPeakLevelIn0;
	static const char * 	kAppleI2S_SetPeakLevelIn1;
	static const char * 	kAppleI2S_GetIntCtrl;
	static const char * 	kAppleI2S_GetSerialFormat;
	static const char * 	kAppleI2S_GetCodecMessageOut;
	static const char * 	kAppleI2S_GetCodecMessageIn;
	static const char * 	kAppleI2S_GetFrameCount;
	static const char * 	kAppleI2S_GetFrameCountToMatch;
	static const char * 	kAppleI2S_GetDataWordSizes;
	static const char * 	kAppleI2S_GetPeakLevelSFSelect;
	static const char * 	kAppleI2S_GetPeakLevelIn0;
	static const char * 	kAppleI2S_GetPeakLevelIn1;
	
	//	¥	GPIO
	
	static const char * 	kAppleGPIO_DisableSpeakerDetect;			
	static const char * 	kAppleGPIO_EnableSpeakerDetect;			
	static const char * 	kAppleGPIO_GetSpeakerDetect;
	static const char * 	kAppleGPIO_RegisterSpeakerDetect;			
	static const char * 	kAppleGPIO_UnregisterSpeakerDetect;		

	static const char * 	kAppleGPIO_DisableDigitalInDetect;
	static const char * 	kAppleGPIO_EnableDigitalInDetect;
	static const char * 	kAppleGPIO_GetDigitalInDetect;	
	static const char * 	kAppleGPIO_RegisterDigitalInDetect;
	static const char * 	kAppleGPIO_UnregisterDigitalInDetect;

	static const char * 	kAppleGPIO_GetComboInJackType;	
	static const char * 	kAppleGPIO_GetComboOutJackType;	

	static const char * 	kAppleGPIO_DisableDigitalOutDetect;
	static const char * 	kAppleGPIO_EnableDigitalOutDetect;
	static const char * 	kAppleGPIO_GetDigitalOutDetect;
	static const char * 	kAppleGPIO_RegisterDigitalOutDetect;
	static const char * 	kAppleGPIO_UnregisterDigitalOutDetect;

	static const char * 	kAppleGPIO_DisableLineInDetect;			
	static const char * 	kAppleGPIO_EnableLineInDetect;			
	static const char * 	kAppleGPIO_GetLineInDetect;				
	static const char * 	kAppleGPIO_RegisterLineInDetect;			
	static const char * 	kAppleGPIO_UnregisterLineInDetect;		

	static const char * 	kAppleGPIO_DisableLineOutDetect;			
	static const char * 	kAppleGPIO_EnableLineOutDetect;			
	static const char * 	kAppleGPIO_GetLineOutDetect;				
	static const char * 	kAppleGPIO_RegisterLineOutDetect;		
	static const char * 	kAppleGPIO_UnregisterLineOutDetect;		

	static const char * 	kAppleGPIO_DisableHeadphoneDetect;		
	static const char * 	kAppleGPIO_EnableHeadphoneDetect;		
	static const char * 	kAppleGPIO_GetHeadphoneDetect;			
	static const char * 	kAppleGPIO_RegisterHeadphoneDetect;		
	static const char * 	kAppleGPIO_UnregisterHeadphoneDetect;	

	static const char * 	kAppleGPIO_SetHeadphoneMute;				
	static const char * 	kAppleGPIO_GetHeadphoneMute;				

	static const char * 	kAppleGPIO_SetAmpMute;					
	static const char * 	kAppleGPIO_GetAmpMute;					

	static const char * 	kAppleGPIO_SetAudioHwReset;
	static const char *		kAppleGPIO_GetAudioHwReset;				

	static const char * 	kAppleGPIO_SetAudioDigHwReset;			
	static const char * 	kAppleGPIO_GetAudioDigHwReset;			

	static const char * 	kAppleGPIO_SetLineOutMute;				
	static const char * 	kAppleGPIO_GetLineOutMute;				

	static const char * 	kAppleGPIO_DisableCodecIRQ;				
	static const char * 	kAppleGPIO_EnableCodecIRQ;				
	static const char * 	kAppleGPIO_GetCodecIRQ;					
	static const char * 	kAppleGPIO_RegisterCodecIRQ;				
	static const char * 	kAppleGPIO_UnregisterCodecIRQ;			

	static const char * 	kAppleGPIO_EnableCodecErrorIRQ;			
	static const char * 	kAppleGPIO_DisableCodecErrorIRQ;			
	static const char * 	kAppleGPIO_GetCodecErrorIRQ;				
	static const char * 	kAppleGPIO_RegisterCodecErrorIRQ;			
	static const char * 	kAppleGPIO_UnregisterCodecErrorIRQ;			

	static const char * 	kAppleGPIO_SetCodecClockMux;				
	static const char * 	kAppleGPIO_GetCodecClockMux;				

	static const char * 	kAppleGPIO_SetCodecInputDataMux;
	static const char * 	kAppleGPIO_GetCodecInputDataMux;
	
	static const char *		kAppleGPIO_GetInternalSpeakerID;
	
	bool					mAppleI2S_Enable;
	bool					mAppleI2S_ClockEnable;
	bool					mAppleI2S_Reset;
	bool					mAppleI2S_CellEnable;

	GpioAttributes			mAppleGPIO_AmpMute;
	GpioAttributes			mAppleGPIO_AnalogCodecReset;
	GpioAttributes			mAppleGPIO_CodecClockMux;
	GpioAttributes			mAppleGPIO_CodecInputDataMux;
	GpioAttributes			mAppleGPIO_DigitalCodecReset;
	GpioAttributes			mAppleGPIO_HeadphoneMute;
	GpioAttributes			mAppleGPIO_LineOutMute;
	GpioAttributes			mAppleGPIO_InternalSpeakerID;
	GpioAttributes			mAppleGPIO_SpeakerID;

	//
	// I2C
	//
	bool					findAndAttachI2C();
	bool					detachFromI2C();
	bool					openI2C();
	void					closeI2C();
	
	UInt32					mI2CPort;
	PPCI2CInterface*		mI2CInterface;
	bool					mI2C_lastTransactionResult;

	//
	// I2S
	//
	typedef enum i2sReference {
		kUseI2SCell0			=	0,
		kUseI2SCell1			=	1,
		kUseI2SCell2			=	2,	// aml, added for neoborg		
		kNoI2SCell				=	0xFFFFFFFF
	} I2SCell;
	
	I2SCell					mI2SInterfaceNumber;
	
	IOReturn 				initI2S(IOMemoryMap* map);

	// Sound Formats:
	typedef enum SoundFormat 
	{
		kSndIOFormatI2SSony,
		kSndIOFormatI2S64x,
		kSndIOFormatI2S32x,
	
		// This says "we never decided for a sound format before"
		kSndIOFormatUnknown
	} SoundFormat;

	bool					findAndAttachI2S();
	bool					detachFromI2S();
	bool					openI2S();
	void					closeI2S();
	
	AppleI2S *				mI2SInterface;

	GpioAttributes			GetCachedAttribute ( GPIOSelector selector, GpioAttributes defaultResult );
	static void				gpioTimerCallback ( OSObject *target, IOAudioDevice *device );
	bool					interruptUsesTimerPolling( PlatformInterruptSource source );
	void					poll ( void );
	void					pollGpioInterrupts ( void );
	GpioAttributes			readGpioState ( GPIOSelector selector );
	IOReturn				writeGpioState ( GPIOSelector selector, GpioAttributes gpioState );
	IOReturn				translateGpioAttributeToGpioState ( GPIOType gpioType, GpioAttributes gpioAttribute, UInt32 * valuePtr );
	volatile UInt8 *		mHwPtr;
	volatile UInt8 *		mHwI2SPtr;
	IOReturn				setupI2SClockSource( UInt32 cell, bool requestClock, UInt32 clockSource );

	volatile UInt32 *				mFcr1;
	
	volatile UInt32 *				mFcr3;
	
	volatile UInt32 *				mSerialFormat;
	volatile UInt32 *				mI2SIntCtrl;
	volatile UInt32 *				mDataWordSize;
	volatile UInt32 *				mFrameCounter;
	
	IOTimerEventSource *			mGpioPollTimer;

	void *							mCodecInterruptHandler;
	void *							mCodecErrorInterruptHandler;
	void *							mDigitalInDetectInterruptHandler;
	void *							mDigitalOutDetectInterruptHandler;
	void *							mHeadphoneDetectInterruptHandler;
	void *							mLineInputDetectInterruptHandler;
	void *							mLineOutputDetectInterruptHandler;
	void *							mSpeakerDetectInterruptHandler;

	bool							mCodecInterruptEnable;
	bool							mCodecErrorInterruptEnable;
	bool							mDigitalInDetectInterruptEnable;
	bool							mDigitalOutDetectInterruptEnable;
	bool							mHeadphoneDetectInterruptEnable;
	bool							mLineInputDetectInterruptEnable;
	bool							mLineOutputDetectInterruptEnable;
	bool							mSpeakerDetectInterruptEnable;
};

#endif

