/*
 *  AppleTopazPlugin.h
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Tue Oct 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef __AppleTopazPlugin
#define __AppleTopazPlugin

#include "AudioHardwareUtilities.h"
#include <IOKit/IOService.h>
#include "PlatformInterface.h"
#include "AppleOnboardAudio.h"
#include "AppleOnboardAudioUserClient.h"

enum CS84xx_ID_VERSION {
	map_ID_VERSION		=	0x7F,			//	Memory Address Pointer:					I.D. and Version Register
	baID				=	4,				//	bit addressed field:					ID code for CS8420 (%0001)
	cs8420_id			=	0x01,			//	USE:	( cs8420_id << baID )
	cs8416_id			=	0x02,			//	USE:	( cs8416_id << baID )
	cs8406_id			=	0x0E,			//	USE:	( cs8406_id << baID )
	baVersion			=	0				//	bit addressed field:					Version of CS8420
};

enum CS84xx_I2C_ADDRESS {
	kCS84xx_I2C_BASE_ADDRESS	=	0x20,
	kCS84xx_AD0_STRAP			=	0,			//	AD0 strapped 'LOW'
	kCS84xx_AD1_STRAP			=	0			//	AD1 strapped 'LOW'
};

//
//	Conventional I2C address nomenclature concatenates a 7 bit address to a 1 bit read/*write bit
//	 ___ ___ ___ ___ ___ ___ ___ ___
//	|   |   |   |   |   |   |   |   |
//	| 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
//	|___|___|___|___|___|___|___|___|
//	  |   |   |   |   |   |   |   |____	1 = Read, 0 = Write
//	  |___|___|___|___|___|___|________	7 bit address
//
//	The conventional method of referring to the I2C address is to read the address in
//	place without any shifting of the address to compensate for the Read/*Write bit.
//	The I2C driver does not use this standardized method of referring to the address
//	and instead, requires shifting the address field right 1 bit so that the Read/*Write
//	bit is not passed to the I2C driver as part of the address field.
//
#define	kCS84xx_I2C_ADDRESS		( ( kCS84xx_I2C_BASE_ADDRESS	| ( kCS84xx_AD1_STRAP << 2 ) | ( kCS84xx_AD0_STRAP << 1 ) ) )

//	NOTE:	Do not modify the order of the TOPAZ_CODEC_TYPES enumerations.  These 
//			enumerated values exist in external source (i.e. AOA Viewer).  Any change
//			in enumeration order will cause AOA Viewer to display the incorrect codec type!
typedef enum {
	kCS8406_CODEC = 0,
	kCS8420_CODEC,
	kCS8416_CODEC
} TOPAZ_CODEC_TYPES;

typedef struct {
	UInt32		sampleRate;
	UInt32		sampleDepth;
	bool		nonAudio;
	bool		consumerMode;
} ChanStatusStruct;
typedef ChanStatusStruct * ChanStatusStructPtr;


#define	kMASK_ALL								0x00
#define	kMASK_NONE								0xFF
#define	kCS84XX_ID_MASK							0xF0
#define kCS84XX_BIT_MASK						0x01
#define kCS84XX_TWO_BIT_MASK					0x03

//	====================================================================================================
//
//	Control Port Register Bit Definitions.
//
//	A register within the CS84xx can only be accessed after setting the value of
//	the 'Memory Address Pointer' (MAP) register to target a specific register.
//	If the most significant bit of the MAP is set to a '1' then the register address
//	will auto-increment within a single read or write transaction.  Auto increment
//	should be enabled to retrieve data from the channel status or user status 
//	registers.  For example, accessing the channel status registers should:
//	
//	1.	Perform a WRITE operation to set the MAP to ( kMAP_CHANNEL_STATUS | kMAP_AUTO_INCREMENT_ENABLE )
//	2.	Perform a READ operation of 24 bytes in length
//	
//	MAP register values are indicated in the register enumerations by a 'map_' prefix.

#define	kMAP_AUTO_INCREMENT_DISABLE			0x00
#define	kMAP_AUTO_INCREMENT_ENABLE			0x80

//	====================================================================================================


//	====================================================================================================
//
//	PROFESSIONAL CHANNEL STATUS BLOCK STRUCTURE:	(NOTE: '*' prefix indicates active low)
//
//	byte\bit      0          1          2          3          4          5          6          7
//			 __________ __________ ________________________________ __________ _____________________ 
//			|          |          |                                |          |                     |
//		0	|  PRO=1   |  *Audio  |            Emphasis            |  *Lock   |  Frequency Sample   |
//			|__________|__________|________________________________|__________|_____________________|
//			|                                           |                                           |
//		1	|               Channel Mode                |           User Bit Management             |
//			|___________________________________________|___________________________________________|
//			|                                |                                |                     |
//		2	|           AUX Use              |          Word Length           |       Reserved      |
//			|________________________________|________________________________|_____________________|
//			|                                                                                       |
//		3	|                                           Reserved                                    |
//			|_______________________________________________________________________________________|
//			|                     |                                                                 |
//		4	|      Reference      |                     Reserved                                    |
//			|_____________________|_________________________________________________________________|
//			|                                                                                       |
//		5	|                                           Reserved                                    |
//			|_______________________________________________________________________________________|
//			|                                                                                       |
//		6	|                            Alphanumeric channel origin data                           |
//			|                                                                                       |
//			|                                                                                       |
//		7	|                                                                                       |
//			|                                                                                       |
//			|                                                                                       |
//		8	|                                                                                       |
//			|                                                                                       |
//			|                                                                                       |
//		9	|                                                                                       |
//			|_______________________________________________________________________________________|
//			|                                                                                       |
//		10	|                          Alphanumeric channel destination data                        |
//			|                                                                                       |
//			|                                                                                       |
//		11	|                                                                                       |
//			|                                                                                       |
//			|                                                                                       |
//		12	|                                                                                       |
//			|                                                                                       |
//			|                                                                                       |
//		13	|                                                                                       |
//			|_______________________________________________________________________________________|
//			|                                                                                       |
//		14	|                                Local sample address code                              |
//			|                                    (32-bit binary)                                    |
//			|                                                                                       |
//		15	|                                                                                       |
//			|                                                                                       |
//			|                                                                                       |
//		16	|                                                                                       |
//			|                                                                                       |
//			|                                                                                       |
//		17	|                                                                                       |
//			|_______________________________________________________________________________________|
//			|                                                                                       |
//		18	|                                    Time of day code                                   |
//			|                                    (32-bit binary)                                    |
//			|                                                                                       |
//		19	|                                                                                       |
//			|                                                                                       |
//			|                                                                                       |
//		20	|                                                                                       |
//			|                                                                                       |
//			|                                                                                       |
//		21	|                                                                                       |
//			|_______________________________________________________________________________________|
//			|                                           |                                           |
//		22	|                  Reserved                 |             Reliability Flags             |
//			|___________________________________________|___________________________________________|
//			|                                                                                       |
//		23	|                             Cyclic redundancy check character                         |
//			|_______________________________________________________________________________________|
//
//	====================================================================================================

enum PRO_CHANNEL_STATUS_BYTE_0 {
	baProSampleFrequency			=	6,
	pSampleFrequency_NotIndicate	=	0,
	pSampleFrequency_48Khz			=	2,
	pSampleFrequency_44Khz			=	1,
	pSampleFrequency_32Khz			=	3
};

enum CONSUMER_CHANNEL_STATUS_BYTE_3 {
	baConsumerSampleFrequency		=	0x00,
	cSampleFrequency_44Khz			=	0x00,
	cSampleFrequency_48Khz			=	0x40,
	cSampleFrequency_32Khz			=	0xC0
};

enum CONSUMER_CHANNEL_STATUS_BYTE_4 {
	cWordLength_24Max_notIndicated	=	0x01,
	cWordLength_24Max_24bits		=	0x0B,
	cWordLength_24Max_23bits		=	0x03,
	cWordLength_24Max_22bits		=	0x05,
	cWordLength_24Max_21bits		=	0x07,
	cWordLength_24Max_20bits		=	0x09,
	cWordLength_20Max_notIndicated	=	0x00,
	cWordLength_20Max_20bits		=	0x0A,
	cWordLength_20Max_19bits		=	0x02,
	cWordLength_20Max_18bits		=	0x04,
	cWordLength_20Max_17bits		=	0x06,
	cWordLength_20Max_16bits		=	0x08
};

enum PRO_CHANNEL_STATUS_BYTE_2 {
	baUseOfAuxSampleBits			=	0,
	baSourceWordLength				=	3
};

#define	kBANonAudio				1
#define	kConsumerMode_audio		0
#define	kConsumerMode_nonAudio	1

#define	kBACopyright			2
#define	kCopyPermited			1

#define	kConsumer				0
#define	kProfessional			1
#define	kBAProConsumer			0


class AppleTopazPlugin : public OSObject {
    OSDeclareDefaultStructors ( AppleTopazPlugin );

public:

	virtual	bool 			init ( OSDictionary *properties );
	virtual	bool			start ( IOService * provider ) { return false; }
    virtual void			free ( void );
	
	void					initPlugin ( PlatformInterface* inPlatformObject );
	virtual bool			preDMAEngineInit ( void ) { return false; }
	
	virtual IOReturn		initCodecRegisterCache ( void ) { return kIOReturnError; }
	virtual IOReturn		setMute ( bool muteState ) { return kIOReturnError; }
	virtual IOReturn		performDeviceSleep ( void ) { return kIOReturnError; }
	virtual IOReturn		performDeviceWake ( void ) { return kIOReturnError; }
	virtual IOReturn		setChannelStatus ( ChanStatusStructPtr channelStatus ) { return kIOReturnError; }
	virtual IOReturn		breakClockSelect ( UInt32 clockSource ) { return kIOReturnError; }
	virtual IOReturn		makeClockSelectPreLock ( UInt32 clockSource ) { return kIOReturnError; }
	virtual IOReturn		makeClockSelectPostLock ( UInt32 clockSource ) { return kIOReturnError; }
	virtual void			setRunMode ( UInt8 mode ) { return; }
	virtual UInt8			setStopMode ( void ) { return 0; }
	virtual UInt32			getClockLock ( void ) { return 0; }
	virtual IOReturn		getCodecErrorStatus ( UInt32 * dataPtr ) { return kIOReturnError; }
	virtual void			disableReceiverError ( void ) { return; }
	virtual IOReturn		flushControlRegisters ( void ) { return kIOReturnError; }
	virtual void			useExternalCLK ( void ) { return; }
	virtual void			useInternalCLK ( void ) { return; }
	
	virtual bool			phaseLocked ( void ) { return false; }
	virtual bool			confidenceError ( void ) { return false; }
	virtual bool			biphaseError ( void ) { return false; }
	
	virtual UInt8			CODEC_GetDataMask ( UInt8 regAddr ) { return 0; }
	virtual IOReturn		CODEC_GetRegSize ( UInt8 regAddr, UInt32 * codecRegSizePtr ) { return kIOReturnError; }
	virtual IOReturn 		CODEC_IsControlRegister ( UInt8 regAddr ) { return kIOReturnError; }
	virtual IOReturn 		CODEC_IsStatusRegister ( UInt8 regAddr ) { return kIOReturnError; }
	
	virtual IOReturn		getPluginState ( HardwarePluginDescriptorPtr outState ) { return kIOReturnError; }
	virtual IOReturn		setPluginState ( HardwarePluginDescriptorPtr inState ) { return kIOReturnError; }
	
	virtual bool			supportsDigitalInput ( void ) { return false; }
	virtual bool			supportsDigitalOutput ( void ) { return false; }

	virtual void			poll ( void ) { return; }

	//	IMPORTANT:		The following methods exist ONLY within the base class:
	
	IOReturn 				CODEC_ReadRegister ( UInt8 regAddr, UInt8 * registerData, UInt32 size );	//	in base class only
	IOReturn 				CODEC_WriteRegister ( UInt8 regAddr, UInt8 registerData );					//	in base class only
	UInt8					getMemoryAddressPointer ( void );											//	in base class only
	IOReturn				setMemoryAddressPointer ( UInt8 map );										//	in base class only
	
protected:

	bool					mRecoveryInProcess;
	bool					mMuteState;
	AppleOnboardAudio *		mAudioDeviceProvider;
	UInt8					mCurrentMAP;
	UInt8					mShadowRegs[256];	//	write through shadow registers for AppleTopazPlugin
	PlatformInterface *		mPlatformInterface;
	
private:

};

#endif


