/*
 *  KeyLargoPlatform.h
 *  
 *
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterface.h"

#include <IOKit/i2c/PPCI2CInterface.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/ppc/IODBDMA.h>
#include "AppleOnboardAudioUserClient.h"

#define	kKL_AUDIO_MAC_IO_BASE_ADDRESS			0x80000000
#define	kKL_AUDIO_MAC_IO_SIZE					256

class KeyLargoPlatform : public PlatformInterface {

    OSDeclareDefaultStructors(KeyLargoPlatform);

	typedef 						bool GpioActiveState;
	typedef 						UInt8* GpioPtr;

public:

	static void						testInterruptHandler();

	virtual bool					init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex);
	
	virtual	void					setWorkLoop(IOWorkLoop* inWorkLoop) {mWorkLoop = inWorkLoop; return;}					
																
	virtual	void					free();
	//
	// Codec Methods
	//
	virtual bool					readCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode);
	virtual bool					writeCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode);

	virtual IOReturn				setCodecReset ( CODEC_RESET target, GpioAttributes reset );
	virtual GpioAttributes			getCodecReset ( CODEC_RESET target );
	//
	// I2S Methods: FCR3
	//
	virtual IOReturn				requestI2SClockSource(I2SClockFrequency inFrequency);
	virtual IOReturn				releaseI2SClockSource(I2SClockFrequency inFrequency);
	//
	// I2S Methods: FCR1
	//
	virtual IOReturn				setI2SEnable(bool enable);
	virtual bool					getI2SEnable();

	virtual IOReturn				setI2SClockEnable(bool enable);
	virtual bool					getI2SClockEnable();

	virtual IOReturn				setI2SCellEnable(bool enable);
	virtual bool					getI2SCellEnable();
	
//	virtual IOReturn				setI2SSWReset(bool enable);
//	virtual bool					getI2SSWReset();
	//
	// I2S Methods: IOM Control (non-FCR1)
	//
	virtual IOReturn				setSerialFormatRegister(UInt32 serialFormat);
	virtual UInt32					getSerialFormatRegister();

	virtual IOReturn				setDataWordSizes(UInt32 dataWordSizes);
	virtual UInt32					getDataWordSizes();
	
	virtual IOReturn				setFrameCount(UInt32 value);
	virtual UInt32					getFrameCount();

	virtual IOReturn				setI2SIOMIntControl(UInt32 intCntrl);
	virtual UInt32					getI2SIOMIntControl();
	//
	// GPIO Methods
	//

	virtual void					initAudioGpioPtr ( const IORegistryEntry * start, const char * gpioName, GpioPtr* gpioH, GpioActiveState* gpioActiveStatePtr );
	virtual IOReturn				getGpioPtrAndActiveState ( GPIOSelector theGpio, GpioPtr * gpioPtrPtr, GpioActiveState * activeStatePtr ) ;
	virtual GpioAttributes			getGpioAttributes ( GPIOSelector theGpio );
	virtual IOReturn				setGpioAttributes ( GPIOSelector theGpio, GpioAttributes attributes );

	virtual IOReturn				setClockMux(GpioAttributes muxState);
	virtual GpioAttributes			getClockMux();

	virtual GpioAttributes			getCodecErrorInterrupt();

	virtual GpioAttributes			getCodecInterrupt();

	virtual	GpioAttributes			getComboInJackTypeConnected();

	virtual	GpioAttributes			getComboOutJackTypeConnected();

	virtual	GpioAttributes			getDigitalInConnected();

	virtual	GpioAttributes			getDigitalOutConnected();

	virtual GpioAttributes		 	getHeadphoneConnected();

	virtual IOReturn 				setHeadphoneMuteState(GpioAttributes muteState);
	virtual GpioAttributes		 	getHeadphoneMuteState();

	virtual IOReturn				setInputDataMux(GpioAttributes muxState) ;
	virtual GpioAttributes			getInputDataMux();

	virtual GpioAttributes			getInternalSpeakerID();

	virtual GpioAttributes			getLineInConnected();
	
	virtual GpioAttributes			getLineOutConnected();

	virtual IOReturn 				setLineOutMuteState(GpioAttributes muteState);
	virtual GpioAttributes		 	getLineOutMuteState();

	virtual GpioAttributes			getSpeakerConnected();

	virtual IOReturn 				setSpeakerMuteState(GpioAttributes  muteState);
	virtual GpioAttributes		 	getSpeakerMuteState();
	

	//
	// Non-inherited public User Client support	
	//
	UInt8 							userClientReadGPIO (UInt32 selector);
	void 							userClientWriteGPIO (UInt32 selector, UInt8 data);

	bool							getGPIOActiveState (UInt32 gpioSelector);
	void 							setGPIOActiveState (UInt32 selector, UInt8 gpioActiveState);
	
	UInt8 *							getGPIOAddress (UInt32 gpioSelector);
	bool							checkGpioAvailable (UInt32 selector);

	virtual void 					logFCR1();
	virtual void 					logFCR3();

	//
	// Set Interrupt Handler Methods
	//
	virtual IOReturn				registerInterruptHandler (IOService * theDevice, void * interruptHandler, PlatformInterruptSource source );
	virtual IOReturn				unregisterInterruptHandler (IOService * theDevice, void * interruptHandler, PlatformInterruptSource source );

	//
	// DBDMA Memory Address Acquisition Methods
	//
	virtual	IODBDMAChannelRegisters *	GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );
	virtual	IODBDMAChannelRegisters *	GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );

	virtual void					LogDBDMAChannelRegisters ( void );

	//	
	//	User Client Support
	//
	virtual IOReturn				getPlatformState ( PlatformStateStructPtr outState );
	virtual IOReturn				setPlatformState ( PlatformStateStructPtr inState );
	
private:

	IOWorkLoop*						mWorkLoop;
    IOService*						mKeyLargoService;
	IODBDMAChannelRegisters *		mIOBaseDMAOutput;
	const OSSymbol *				mKLI2SPowerSymbolName;	// [3324205]
	//
	// GPIO
	//

	IOReturn						gpioWrite( UInt8* gpioAddress, UInt8 data );

	GpioPtr							mAmplifierMuteGpio;
	GpioPtr							mAnalogResetGpio;
	GpioPtr							mClockMuxGpio;
	GpioPtr							mComboInJackTypeGpio;							
	GpioPtr							mComboOutJackTypeGpio;							
	GpioPtr							mCodecErrorInterruptGpio;
	GpioPtr							mCodecInterruptGpio;
	GpioPtr							mDigitalInDetectGpio;							
	GpioPtr							mDigitalOutDetectGpio;							
	GpioPtr							mDigitalResetGpio;
	GpioPtr							mHeadphoneDetectGpio;
	GpioPtr							mHeadphoneMuteGpio;
	GpioPtr							mInputDataMuxGpio;
	GpioPtr							mInternalSpeakerIDGpio;
	GpioPtr							mLineInDetectGpio;								
	GpioPtr							mLineOutDetectGpio;	
	GpioPtr							mLineOutMuteGpio;								
	GpioPtr							mSpeakerDetectGpio;

	GpioActiveState					mAmplifierMuteActiveState;									
	GpioActiveState					mAnalogResetActiveState;
	GpioActiveState					mClockMuxActiveState;
	GpioActiveState					mCodecErrorInterruptActiveState;					
	GpioActiveState					mCodecInterruptActiveState;					
	GpioActiveState					mDigitalInDetectActiveState;					
	GpioActiveState					mComboInJackTypeActiveState;					
	GpioActiveState					mDigitalOutDetectActiveState;					
	GpioActiveState					mComboOutJackTypeActiveState;					
	GpioActiveState					mDigitalResetActiveState;
	GpioActiveState					mHeadphoneDetectActiveState;
	GpioActiveState					mHeadphoneMuteActiveState;	
	GpioActiveState					mInputDataMuxActiveState;							
	GpioActiveState					mInternalSpeakerIDActiveState;					
	GpioActiveState					mLineInDetectActiveState;						
	GpioActiveState					mLineOutDetectActiveState;						
	GpioActiveState					mLineOutMuteActiveState;							
	GpioActiveState					mSpeakerDetectActiveState;									

	volatile UInt8 *		mHwPtr;								//	remove after burning real rom on real hardware

	IOService *				mHeadphoneDetectIntProvider;
	IOService *				mSpeakerDetectIntProvider;
	IOService *				mLineOutDetectIntProvider;								
	IOService *				mLineInDetectIntProvider;								
	IOService *				mDigitalOutDetectIntProvider;								
	IOService *				mDigitalInDetectIntProvider;								
	IOService *				mCodecIntProvider;								
	IOService *				mCodecErrorIntProvider;								

	IOInterruptEventSource *mHeadphoneDetectIntEventSource;
	IOInterruptEventSource *mSpeakerDetectIntEventSource;
	IOInterruptEventSource *mLineOutDetectIntEventSource;							
	IOInterruptEventSource *mLineInDetectIntEventSource;							
	IOInterruptEventSource *mDigitalOutDetectIntEventSource;								
	IOInterruptEventSource *mDigitalInDetectIntEventSource;								
	IOInterruptEventSource *mCodecInterruptEventSource;								
	IOInterruptEventSource *mCodecErrorInterruptEventSource;								

	enum extInt_gpio {
			intEdgeSEL				=	7,		//	bit address:	R/W Enable Dual Edge
			positiveEdge			=	0,		//		0 = positive edge detect for ExtInt interrupt sources (default)
			dualEdge				=	1		//		1 = enable both edges
	};
	
	enum gpio {
			gpioOS					=	4,		//	bit address:	output select
			gpioBit0isOutput		=	0,		//		use gpio bit 0 as output (default)
			gpioMediaBayIsOutput	=	1,		//		use media bay power
			gpioReservedOutputSel	=	2,		//		reserved
			gpioMPICopenCollector	=	3,		//		MPIC CPUInt2_1 (open collector)
			
			gpioAltOE				=	3,		//	bit address:	alternate output enable
			gpioOE_DDR				=	0,		//		use DDR for output enable
			gpioOE_Use_OS			=	1,		//		use gpioOS for output enable
			
			gpioDDR					=	2,		//	bit address:	r/w data direction
			gpioDDR_INPUT			=	0,		//		use for input (default)
			gpioDDR_OUTPUT			=	1,		//		use for output
			
			gpioPIN_RO				=	1,		//	bit address:	read only level on pin
			
			gpioDATA				=	0,		//	bit address:	the gpio itself
			
			gpioBIT_MASK			=	1		//	value shifted by bit position to be used to determine a GPIO bit state
	};

	//
	// I2S
	//
	typedef enum i2sReference {
		kUseI2SCell0			=	0,
		kUseI2SCell1			=	1,
		kNoI2SCell				=	0xFFFFFFFF
	} I2SCell;
	
	I2SCell					mI2SInterfaceNumber;
	
	bool					findAndAttachI2C();
	bool					detachFromI2C();
	bool					openI2C();
	void					closeI2C();

	IOReturn 				initI2S(IOMemoryMap* map);
	
	UInt32					mI2CPort;
	bool					mI2C_lastTransactionResult;

	PPCI2CInterface* mI2CInterface;

	// Sound Formats:
	typedef enum SoundFormat 
	{
		kSndIOFormatI2SSony,
		kSndIOFormatI2S64x,
		kSndIOFormatI2S32x,
	
		// This says "we never decided for a sound format before"
		kSndIOFormatUnknown
	} SoundFormat;
	
	// Characteristic constants:
	typedef enum TicksPerFrame 
	{
		k64TicksPerFrame		= 64,			// 64 ticks per frame
		k32TicksPerFrame		= 32 			// 32 ticks per frame
	} TicksPerFrame;
	
	typedef enum ClockSourceValue 
	{
		kClock49MHz				= 49152000,		// 49 MHz clock source
		kClock45MHz				= 45158400,		// 45 MHz clock source
		kClock18MHz				= 18432000		// 18 MHz clock source
	} ClockSourceValue;

	void *					mSoundConfigSpace;		  		// address of sound config space
	void *					mIOBaseAddress;		   			// base address of our I/O controller
	void *					mIOConfigurationBaseAddress;	// base address for the configuration registers
	void *					mI2SBaseAddress;				//	base address of I2S I/O Module
	IODeviceMemory *		mIOBaseAddressMemory;			// Have to free this in free()
	IODeviceMemory *		mIOI2SBaseAddressMemory;

	static const UInt32 kFCR0Offset;
	static const UInt32 kFCR1Offset;
	static const UInt32 kFCR2Offset;
	static const UInt32 kFCR3Offset;
	static const UInt32 kFCR4Offset;
	
	enum FCR1_Bit_Addresses {					//	bit addresses
		kI2S1Enable					=	20,			//	1 = normal, 0 = tristate
		kI2S1ClkEnBit				=	19,			//	1 = normal, 0 = stopped low
		kI2S1SwReset				=	18,			//	1 = reset, 0 = run
		kI2S1CellEn					=	17,			//	1 = clock running, 0 = clock stopped
		kI2S0Enable					=	13,			//	1 = normal, 0 = tristate
		kI2S0ClkEnBit				=	12,			//	1 = normal, 0 = stopped low
		kI2S0SwReset				=	11,			//	1 = reset, 0 = run
		kI2S0CellEn					=	10,			//	1 = clock running, 0 = clock stopped
		kChooseI2S0					=	 9,			//	1 = I2S0 drives clock out, 0 = SccB or IrDA drives clock out
		kChooseAudio				=	 7,			//	1 = DAV audio, 0 = I2S0
		kAUDIOCellEN				=	 6,			//	1 = DAV clock running, 0 = DAV clocks stopped
		kAudioClkOut_EN_h			=	 5,			//	1 = DAV AudioClkOut active, 0 = DAV AudioClkOut tristate
		kAudioSW_Reset_h			=	 4,			//	1 = DAV reset, 0 = run
		kAudioClkEnBit_h			=	 3,			//	1 = normal, 0 = stopped low
		kAudioClkDiv2_h				=	 2,			//	1 = divided by 4, 0 = divided by 2
		kAudio_Sel22MClk			=	 1,
		kAudioClkOut1X_h			=	 0
	};
	
	enum FCR1_Field_Width {
		kI2S1Enable_bitWidth		=	1,			//	
		kI2S1ClkEnBit_bitWidth		=	1,			//	
		kI2S1SwReset_bitWidth		=	1,			//	
		kI2S1CellEn_bitWidth		=	1,			//	
		kI2S0Enable_bitWidth		=	1,			//	
		kI2S0ClkEnBit_bitWidth		=	1,			//	
		kI2S0SwReset_bitWidth		=	1,			//	
		kI2S0CellEn_bitWidth		=	1,			//	
		kChooseI2S0_bitWidth		=	1,			//	
		kChooseAudio_bitWidth		=	1,			//	
		kAUDIOCellEN_bitWidth		=	1			//	
	};
	
	enum FCR3_Bit_Addresses {
		kClk18_EN_h					=	14,			//	1 = enable 18.4320 MHz clock to the 12S0 cell
		kI2S1_Clk18_EN_h			=	13,			//	1 = enable 18.4320 MHz clock to the 12S1 cell
		kClk45_EN_h					=	10,			//	1 = enable 45.1584 MHz clock to Audio, I2S0, I2S1 and SCC
		kClk49_EN_h					=	 9,			//	1 = enable 49.1520 MHz clock to Audio, I2S0 
		kShutdown_PLLKW4			=	 2,			//	1 = shutdown the 45.1584 MHz PLL
		kShutdown_PLLKW6			=	 1,			//	1 = shutdown the 49.1520 MHz PLL
		kShutdown_PLL_Total			=	 0			//	1 = shutdown all five PLL modules
	};
	
	enum FCR3_FieldWidth {
		kClk18_EN_h_bitWidth			=	1,		//	
		kI2S1_Clk18_EN_h_bitWidth		=	1,		//	
		kClk45_EN_h_bitWidth			=	1,		//	
		kClk49_EN_h_bitWidth			=	1,		//	
		kShutdown_PLLKW4_bitWidth		=	1,		//	
		kShutdown_PLLKW6_bitWidth		=	1,		//	
		kShutdown_PLL_Total_bitWidth	=	1		//	
	};

	static const char*	kAmpMuteEntry;
	static const char*  kAnalogHWResetEntry;
	static const char*  kComboInJackTypeEntry;
	static const char*  kComboOutJackTypeEntry;
	static const char*  kDigitalHWResetEntry;
	static const char*  kDigitalInDetectEntry;
	static const char*  kDigitalOutDetectEntry;
	static const char*  kHeadphoneDetectInt;
	static const char* 	kHeadphoneMuteEntry;
	static const char*	kInternalSpeakerIDEntry;
	static const char*  kLineInDetectInt;
	static const char*  kLineOutDetectInt;
	static const char*  kLineOutMuteEntry;
	static const char*  kSpeakerDetectEntry;

	static const char*  kNumInputs;
	static const char*  kDeviceID;
	static const char*  kSpeakerID;
	static const char*  kCompatible;
	static const char*  kI2CAddress;
	static const char*  kAudioGPIO;
	static const char*  kAudioGPIOActiveState;
	static const char*  kIOInterruptControllers;
	
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

	static const UInt32 kI2SClockOffset;							/*	FCR1 offset (not mapped by AudioI2SControl)	*/
	static const UInt32 kI2S0ClockEnable;
	static const UInt32 kI2S1ClockEnable;
	static const UInt32 kI2S0CellEnable;
	static const UInt32 kI2S1CellEnable;
	static const UInt32 kI2S0InterfaceEnable;
	static const UInt32 kI2S1InterfaceEnable;
	//
	// Utilties
	//
	virtual IOReturn				setHeadphoneDetectInterruptHandler(IOService* theDevice, void* interruptHandler);

	virtual	IOReturn 				setSpeakerDetectInterruptHandler (IOService* theDevice, void* interruptHandler);
	
	virtual IOReturn				setLineOutDetectInterruptHandler(IOService* theDevice, void* interruptHandler);

	virtual IOReturn				setLineInDetectInterruptHandler(IOService* theDevice, void* interruptHandler);

	virtual IOReturn				setDigitalOutDetectInterruptHandler(IOService* theDevice, void* interruptHandler);

	virtual IOReturn				setDigitalInDetectInterruptHandler(IOService* theDevice, void* interruptHandler);

	virtual IOReturn				setCodecInterruptHandler(IOService* theDevice, void* interruptHandler);

	virtual IOReturn				setCodecErrorInterruptHandler(IOService* theDevice, void* interruptHandler);
	
	inline void 		setKeyLargoRegister(void *klRegister, UInt32 value);	
	inline UInt32 		getKeyLargoRegister(void *klRegister);
	inline UInt32 		getFCR1();
	inline void 		setFCR1(UInt32 value);
	inline UInt32 		getFCR3();
	inline void 		setFCR3(UInt32 value);

	inline UInt8 		assertGPIO(GpioActiveState inState) {return ((0 == inState) ? 0 : 1);}
	inline UInt8 		negateGPIO(GpioActiveState inState) {return ((0 == inState) ? 1 : 0);}

	IORegistryEntry*	FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value);

};
