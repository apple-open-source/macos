/*
 *  AppleTopazPluginCS8406.h
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Tue Oct 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */


#ifndef __AppleTopazPluginCS8406
#define __AppleTopazPluginCS8406

#include "AppleTopazPlugin.h"
#include <IOKit/IOService.h>

//	====================================================================================================
//
//	Register bit fields
//	NOTE:	Driver default settings indicated with '¥'.
//

enum CS8406_MISC_CNTRL_1 {
	map_CS8406_MISC_CNTRL_1			=	1,				//	Memory Address Pointer:					Miscellaneous Control Register 1
	baCS8406_VSET					=	6,				//	bit addressed field:					Transmitted 'V' bit level
		bvCS8406_vbitValid			=	0,				//		( vbitValid << baVSET )			¥	'V' bit indicates VALID / PCM (default)
		bvCS8406_vbitInvalid		=	1,				//		( vbitInvalid << baVSET )			'V' bit indicates INVALID / NON-PCM
	baCS8406_MuteAES				=	4,				//	bit addressed field:					Mute AES3 Transmitter Output
		bvCS8406_normalAES3			=	0,				//		( normalAES3 << baMuteAES )		¥	AES3 Output not muted (default)
		bvCS8406_muteAES3			=	1,				//		( muteAES3 << baMuteAES )			OMAES3 Output mutedCK
	baCS8406_INT					=	1,				//	bit addressed field:					interrupt output pin control
		bvCS8406_activeHighINT		=	0,				//		( activeHighINT << baINT )		¥	active high interrupt output (default)
		bvCS8406_activeLowINT		=	1,				//		( activeLowINT << baINT )			active low interrupt output
		bvCS8406_activeLowODINT		=	2,				//		( activeLowODINT << baINT )			active low open drain interrupt output
	baCS8406_TCBLD					=	0,				//	bit addressed field:					Transmit channel status block direction
		bvCS8406_inputTCBLD			=	0,				//		( inputTCBLD << baTCBLD )		¥	Transmit channel status block pin is input (default)
		bvCS8406_outputTCBLD		=	1				//		( outputTCBLD << baTCBLD )			Transmit channel status block pin is output
};

enum CS8406_MISC_CNTRL_2 {
	map_CS8406_MISC_CNTRL_2			=	2,				//	Memory Address Pointer:					Miscellaneous Control Register 2
	baCS8406_MMT					=	2,				//	bit addressed field:					Select AES transmitter mono or stereo
		bvCS8406_mmtStereo			=	0,				//		( mmtStereo << baMMT )			¥	A = Left and B = Right for stereo (default)
		bvCS8406_mmtUseMMTLR		=	1,				//		( mmtUseMMTLR << baMMT )			MMTLR selects source
	baCS8406_MMTCS					=	1,				//	bit addressed field:					Select A or B channel status data to transmit in mono mode
		bvCS8406_discrete			=	0,				//		( normalAES3 << baMMTCS )		¥	Use ch. A CS for A sub-frame, B CS for B sub-frame(default)
		bvCS8406_useMMTLR			=	1,				//		( muteAES3 << baMMTCS )				Use MMTLR
	baCS8406_MMTLR					=	0,				//	bit addressed field:					Channel selection for AES transmitter mono mode
		bvCS8406_useLeft			=	0,				//		( useLeft << baMMTLR )			¥	Use left channel input data for consecutive sub-frame outputs (default)
		bvCS8406_useRight			=	1				//		( useRight << baMMTLR )				Use right channel input data for consecutive sub-frame outputs
};

enum CS8406_DATA_FLOW_CTRL {
	map_CS8406_DATA_FLOW_CTRL		=	3,				//	Memory Address Pointer:					Data Flow Control
	baCS8406_TXOFF					=	6,				//	bit addressed field:					AES3 Transmitter Output Driver Control
		bvCS8406_aes3TXNormal		=	0,				//		( aes3TXNormal << baTXOFF )		¥	AES3 transmitter output pin drivers normal operation (default)
		bvCS8406_aes3TX0v			=	1,				//		( aes3TX0v << baTXOFF )				AES3 transmitter output pin drivers at 0 volts
	baCS8406_AESBP					=	5,				//	bit addressed field:					AES3 bypass mode selection
		bvCS8406_normalBP			=	0,				//		( normalBP << baAESBP )			¥	normal operation (default)
		bvCS8406_rxpPB				=	1				//		( rxpPB << baAESBP )				connect AES3 ttx to RXP pin
};

enum CS8406_CLOCK_SOURCE_CTRL {
	map_CS8406_CLOCK_SOURCE_CTRL	=	4,			//	Memory Address Pointer:					Clock Source Control
	baCS8406_RUN					=	6,				//	bit addressed field:					Clock Control [Power Management]
		bvCS8406_runSTOP			=	0,				//		( runSTOP << baRUN )				Low Power operation (default)
		bvCS8406_runNORMAL			=	1,				//		( runNORMAL << baRUN )			¥	Normal operation
	baCS8406_CLK					=	4,				//	bit addressed field:					Output master clock input frequency to output sample rate ratio
		bvCS8406_omck256fso			=	0,				//		( omck256fso << baCLK )			¥	OMCK frequency is 256 * Fso (default)
		bvCS8406_omck384fso			=	1,				//		( omck384fso << baCLK )				OMCK frequency is 384 * Fso
		bvCS8406_omck512fs0			=	2				//		( omck512fs0 << baCLK )				OMCK frequency is 512 * Fso
};

enum CS8406_SERIAL_AUDIO_INPUT_FORMAT {
	map_CS8406_SERIAL_INPUT_FMT		=	5,				//	Memory Address Pointer:					Serial Audio Input Port Data Format
	baCS8406_SIMS					=	7,				//	bit addressed field:					Serial Input Master / Slave Mode Selector
		bvCS8406_inputSlave			=	0,				//		( inputSlave << baSIMS )		¥	Serial audio input slave mode (default)
		bvCS8406_inputMaster		=	1,				//		( inputMaster << baSIMS )			Serial audio input master mode
	baCS8406_SISF					=	6,				//	bit addressed field:					ISCLK frequency for Master Mode
		bvCS8406_isclk64fsi			=	0,				//		( isclk64fsi << baSISF )		¥	64 * Fsi (default)
		bvCS8406_isclk128fsi		=	1,				//		( isclk128fsi << baSISF )			128 * Fsi
	baCS8406_SIRES					=	4,				//	bit addressed field:					Resolution of the input data, for right justified formats
		bvCS8406_input24bit			=	0,				//		( input24bit << baSIRES )		¥	24 bit resolution (default)
		bvCS8406_input20bit			=	1,				//		( input20bit << baSIRES )			20 bit resolution
		bvCS8406_input16bit			=	2,				//		( input16bit << baSIRES )			16 bit resolution
	baCS8406_SIJUST					=	3,				//	bit addressed field:					Justification of SDIN data relative to ILRCK
		bvCS8406_siLeftJust			=	0,				//		( siLeftJust << baSIJUST )		¥	Left justified (default)
		bvCS8406_siRightJust		=	1,				//		( siRightJust << baSIJUST )			Right justified
	baCS8406_SIDEL					=	2,				//	bit addressed field:					Delay of SDIN data relative to ILRCK, for left justified data formats (i.e. I2S)
		bvCS8406_siMsb1stCk			=	0,				//		( siMsb1stCk << baSIDEL )			MSB of SDIN data occurs at first ISCLK after ILRCK (default)
		bvCS8406_siMsb2ndCk			=	1,				//		( siMsb2ndCk << baSIDEL )		¥	MSB of SDIN data occurs at second ISCLK after ILRCK
	baCS8406_SISPOL					=	1,				//	bit addressed field:					ISCLK clock polarity
		bvCS8406_siRising			=	0,				//		( siRising << baSISPOL )		¥	SDIN sampled on rising edge of ISCLK (default)
		bvCS8406_siFalling			=	1,				//		( siFalling << baSISPOL )			SDIN sampled on falling edge of ISCLK
	baCS8406_SILRPOL				=	0,				//	bit addressed field:					ILRCK clock polarity
		bvCS8406_siLeftILRCK		=	0,				//		( siLeftILRCK << baSILRPOL )		SDIN data is for left channel when ILRCK is high (default)
		bvCS8406_siRightILRCK		=	1				//		( siRightILRCK << baSILRPOL )	¥	SDIN data is for right channel when ILRCK is high
};

enum CS8406_IRQ_STATUS1 {
	map_CS8406_IRQ1_STATUS		=	7,				//	Memory Address Pointer:					Interrupt 1 Register Status (all bits active high)
	baCS8406_TSLIP				=	7,				//	bit addressed field:					AES3 transmitter souce data slip interrupt
	baCS8406_EFTC				=	1				//	bit addressed field:					E to F C-buffer transfer interrupt
};

enum CS8406_IRQ_STATUS2 {
	map_CS8406_IRQ2_STATUS		=	8,				//	Memory Address Pointer:					Interrupt 2 Register Status (all bits active high)
	baCS8406_EFTU				=	2				//	bit addressed field:					E to F U-buffer transfer error
};

enum CS8406_IRQ_MASK1 {
	map_CS8406_IRQ1_MASK		=	9,				//	Memory Address Pointer:					Interrupt 1 Register Mask (set bit field to '0' to mask, '1' to enable interrupt)
	baCS8406_TSLIPM				=	7,				//	bit addressed field:					'0' to mask, '1' to enable AES3 tx source data slip interrupt
	baCS8406_EFTCM				=	1				//	bit addressed field:					'0' to mask, '1' to enable E to F C-buffer transfer interrupt
};

enum CS8406_IRQ_MODE1_MSB {
	map_CS8406_IRQ1_MODE_MSB	=	10,				//	Memory Address Pointer:					Interrupt Register 1 Mode Register MSB
	baCS8406_TSLIP1				=	7,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8406_EFTC1				=	1				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8406_IRQ_MODE1_LSB {
	map_CS8406_IRQ1_MODE_LSB	=	11,				//	Memory Address Pointer:					Interrupt Register 1 Mode Register LSB
	baCS8406_TSLIP0				=	7,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8406_EFTC0				=	1				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8406_IRQ_MASK2 {
	map_CS8406_IRQ2_MASK		=	12,				//	Memory Address Pointer:					Interrupt 2 Register Mask (set bit field to '0' to mask, '1' to enable interrupt)
	baCS8406_EFTUM				=	2				//	bit addressed field:					'0' to mask, '1' to enable E to F U-buffer transfer error
};

enum CS8406_IRQ_MODE2_MSB {
	map_CS8406_IRQ2_MODE_MSB	=	13,				//	Memory Address Pointer:					Interrupt Register 2 Mode Register MSB
	baCS8406_EFTUM1				=	2				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8406_IRQ_MODE2_LSB {
	map_CS8406_IRQ2_MODE_LSB	=	14,				//	Memory Address Pointer:					Interrupt Register 2 Mode Register LSB
	baCS8406_EFTUM0				=	2				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8406_CH_STATUS_DATA_BUF_CTRL {
	map_CS8406_CH_STATUS_DATA_BUF_CTRL	=	18,		//	Memory Address Pointer:					Channel Status Data Buffer Control
	baCS8406_BSEL				=	5,				//	bit addressed field:					'0' to mask, '1' to enable 
		bvCS8406_bselChStat		=	0,				//		( bselChStat << baBSEL )			Data buffer address space contains channel status data (default)
		bvCS8406_bselUserStat	=	1,				//		( bselUserStat << baBSEL )			Data buffer address space contains user status data
	baCS8406_EFTCI				=	2,				//	bit addressed field:					E to F C-data buffer transfer inhibit bit
		bvCS8406_enCDataEtoF	=	0,				//		( enCDataEtoF << baEFTCI )			Allow C-data E to E buffer transfers (default)
		bvCS8406_disCDataEtoF	=	1,				//		( disCDataEtoF << baEFTCI )			Inhibit C-data E to E buffer transfers
	baCS8406_CAM				=	1,				//	bit addressed field:					C-data buffer control port access mode
		bvCS8406_onebyte		=	0,				//		( onebyte << baCAM )				One byte mode (default)
		bvCS8406_twoByte		=	1				//		( twoByte << baCAM )				Two byte mode
};

enum CS8406_USER_DATA_BUF_CTRL {
	map_CS8406_USER_DATA_BUF_CTRL	=	19,			//	Memory Address Pointer:					User Data Buffer control
	baCS8406_UD					=	4,				//	bit addressed field:					User data pin data direction specifier
		bvCS8406_udataIn		=	0,				//		( udataIn << baUD )					U-pin is an input (default)
		bvCS8406_udataOut		=	1,				//		( udataOut << baUD )				U-pin is an output
	baCS8406_UBM				=	2,				//	bit addressed field:					AES3 U bit manager operating mode
		bvCS8406_ubmTX0			=	0,				//		( ubmTX0 << baUBM )					Transmit all zeros mode (default)
		bvCS8406_ubmBlock		=	1,				//		( ubmBlock << baUBM )				Block mode
		bvCS8406_ubmIECmode4	=	3,				//		( ubmIECmode4 << baUBM )			IEC consumer mode 4
	baCS8406_EFTUI				=	0,				//	bit addressed field:					E to F U-data buffer transfer inhibit bit (valid in block mode only)
		bvCS8406_enUDataEtoF	=	0,				//		( enUDataEtoF << baEFTUI )			Allow U-data E to F buffer transfers (default)
		bvCS8406_disUDataEtoF	=	1				//		( disUDataEtoF << baEFTUI )			Inhibit U-data E to F buffer transfers
};

enum CS8406_C_OR_U_BIT_BUFFER {					//	NOTE:  The channel / user status buffer should be read as a single 24 byte transaction with auto-increment enabled!
	map_CS8406_BUFFER_0			=	32,			//	Memory Address Pointer:						Channel Status or User Status Buffer  0	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_1			=	33,			//	Memory Address Pointer:						Channel Status or User Status Buffer  1	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_2			=	34,			//	Memory Address Pointer:						Channel Status or User Status Buffer  2	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_3			=	35,			//	Memory Address Pointer:						Channel Status or User Status Buffer  3	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_4			=	36,			//	Memory Address Pointer:						Channel Status or User Status Buffer  4	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_5			=	37,			//	Memory Address Pointer:						Channel Status or User Status Buffer  5	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_6			=	38,			//	Memory Address Pointer:						Channel Status or User Status Buffer  6	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_7			=	39,			//	Memory Address Pointer:						Channel Status or User Status Buffer  7	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_8			=	40,			//	Memory Address Pointer:						Channel Status or User Status Buffer  8	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_9			=	41,			//	Memory Address Pointer:						Channel Status or User Status Buffer  9	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_10		=	42,			//	Memory Address Pointer:						Channel Status or User Status Buffer 10	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_11		=	43,			//	Memory Address Pointer:						Channel Status or User Status Buffer 11	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_12		=	44,			//	Memory Address Pointer:						Channel Status or User Status Buffer 12	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_13		=	45,			//	Memory Address Pointer:						Channel Status or User Status Buffer 13	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_14		=	46,			//	Memory Address Pointer:						Channel Status or User Status Buffer 14	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_15		=	47,			//	Memory Address Pointer:						Channel Status or User Status Buffer 15	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_16		=	48,			//	Memory Address Pointer:						Channel Status or User Status Buffer 16	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_17		=	49,			//	Memory Address Pointer:						Channel Status or User Status Buffer 17	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_18		=	50,			//	Memory Address Pointer:						Channel Status or User Status Buffer 18	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_19		=	51,			//	Memory Address Pointer:						Channel Status or User Status Buffer 19	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_20		=	52,			//	Memory Address Pointer:						Channel Status or User Status Buffer 20	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_21		=	53,			//	Memory Address Pointer:						Channel Status or User Status Buffer 21	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_22		=	54,			//	Memory Address Pointer:						Channel Status or User Status Buffer 22	(see Channel Status Block Structure below)
	map_CS8406_BUFFER_23		=	55			//	Memory Address Pointer:						Channel Status or User Status Buffer 23	(see Channel Status Block Structure below)
};

#define	kCS8406_MISC_CNTRL_1_INIT				(	( bvCS8406_vbitValid << baCS8406_VSET ) | \
													( bvCS8406_muteAES3 << baCS8406_MuteAES ) | \
													( bvCS8406_activeLowINT << baCS8406_INT ) | \
													( bvCS8406_outputTCBLD << baCS8406_TCBLD ) )

#define	kCS8406_MISC_CNTRL_2_INIT				(	( bvCS8406_mmtStereo << baCS8406_MMT ) | \
													( bvCS8406_discrete << baCS8406_MMTCS ) | \
													( bvCS8406_useLeft << baCS8406_MMTLR ) )

#define	kCS8406_DATA_FLOW_CTRL_INIT				(	( bvCS8406_aes3TXNormal << baCS8406_TXOFF ) | \
													( bvCS8406_normalBP << baCS8406_AESBP ) )

#define	kCS8406_CLOCK_SOURCE_CTRL_INIT_STOP		(	( bvCS8406_runSTOP << baCS8406_RUN ) | \
													( bvCS8406_omck256fso << baCS8406_CLK ) )

#define	kCS8406_CLOCK_SOURCE_CTRL_INIT			(	( bvCS8406_runNORMAL << baCS8406_RUN ) | \
													( bvCS8406_omck256fso << baCS8406_CLK ) )

#define	kCS8406_SERIAL_AUDIO_INPUT_FORMAT_INIT	(	( bvCS8406_inputSlave << baCS8406_SIMS ) | \
													( bvCS8406_isclk64fsi << baCS8406_SISF ) | \
													( bvCS8406_input24bit << baCS8406_SIRES ) | \
													( bvCS8406_siLeftJust << baCS8406_SIJUST ) | \
													( bvCS8406_siMsb2ndCk << baCS8406_SIDEL ) | \
													( bvCS8406_siRising << baCS8406_SISPOL ) | \
													( bvCS8406_siRightILRCK << baCS8406_SILRPOL ) )

#define	kMISC_CNTRL_1_INIT_8406_MASK			(	( bvCS8406_vbitInvalid << baCS8406_VSET ) | \
													( bvCS8406_muteAES3 << baCS8406_MuteAES ) | \
													( bvCS8406_activeLowODINT << baCS8406_INT ) | \
													( bvCS8406_outputTCBLD << baCS8406_TCBLD ) )
													
#define	kCS8406_MISC_CNTRL_2_INIT_MASK			(	( bvCS8406_mmtUseMMTLR << baCS8406_MMT ) | ( bvCS8406_useMMTLR << baCS8406_MMTCS ) | ( bvCS8406_useRight << baCS8406_MMTLR ) )
#define	kCS8406_DATA_FLOW_CTR_MASK				(	( bvCS8406_aes3TX0v << baCS8406_TXOFF ) | ( bvCS8406_rxpPB << baCS8406_AESBP ) )
#define	kCS8406_CLOCK_SOURCE_CTR_MASK			(	( bvCS8406_runNORMAL << baCS8406_RUN ) | ( bvCS8406_omck384fso << baCS8406_CLK ) | ( bvCS8406_omck512fs0 << baCS8406_CLK ) )
#define	kCS8406_SERIAL_AUDIO_INPUT_FORMAT_MASK	kMASK_NONE
#define	kCS8406_IRQ1_8406_MASK_MASK				(	( kCS84XX_BIT_MASK << baCS8406_TSLIP ) | ( kCS84XX_BIT_MASK << baCS8406_EFTC ) )
#define	kCS8406_IRQ2_8406_MASK_MASK				( kCS84XX_BIT_MASK<< baCS8406_EFTU )
#define	kCS8406_CH_STATUS_DATA_BUF_CTRL_MASK	(	( bvCS8406_bselUserStat << baCS8406_BSEL ) | ( bvCS8406_disCDataEtoF << baCS8406_EFTCI ) | ( bvCS8406_twoByte << baCS8406_CAM ) )
#define	kCS8406_USER_DATA_BUF_CTRLL_MASK		(	( bvCS8406_udataOut << baCS8406_UD ) | ( bvCS8406_ubmIECmode4 << baCS8406_UBM ) | ( bvCS8406_disUDataEtoF << baCS8406_EFTUI ) )

class AppleTopazPluginCS8406 : public AppleTopazPlugin {
    OSDeclareDefaultStructors ( AppleTopazPluginCS8406 );

public:

	virtual	bool 			init ( OSDictionary *properties );
	virtual	bool			start ( IOService * provider );
    virtual void			free ( void );
	
	virtual bool			preDMAEngineInit ( void );
	virtual IOReturn		initCodecRegisterCache ( void );
	virtual IOReturn		setMute ( bool muteState );
	virtual IOReturn		performDeviceSleep ( void );
	virtual IOReturn		performDeviceWake ( void );
	virtual IOReturn		setChannelStatus ( ChanStatusStructPtr channelStatus );
	virtual IOReturn		breakClockSelect ( UInt32 clockSource );
	virtual IOReturn		makeClockSelectPreLock ( UInt32 clockSource );
	virtual IOReturn		makeClockSelectPostLock ( UInt32 clockSource );
	virtual void			setRunMode ( UInt8 mode );
	virtual UInt8			setStopMode ( void );
	virtual UInt32			getClockLock ( void ) { return 1; }
	virtual IOReturn		getCodecErrorStatus ( UInt32 * dataPtr );
	virtual void			disableReceiverError ( void );

	virtual void			useExternalCLK ( void );
	virtual void			useInternalCLK ( void );

	virtual IOReturn		flushControlRegisters ( void );

	virtual	UInt8			CODEC_GetDataMask ( UInt8 regAddr );
	virtual IOReturn		CODEC_GetRegSize ( UInt8 regAddr, UInt32 * codecRegSizePtr );
	virtual IOReturn 		CODEC_IsControlRegister ( UInt8 regAddr );
	virtual IOReturn 		CODEC_IsStatusRegister ( UInt8 regAddr );
	
	virtual IOReturn		getPluginState ( HardwarePluginDescriptorPtr outState );
	virtual IOReturn		setPluginState ( HardwarePluginDescriptorPtr inState );

	virtual bool			supportsDigitalInput ( void ) { return FALSE; }
	virtual bool			supportsDigitalOutput ( void ) { return TRUE; }

protected:

	ChanStatusStruct		mChanStatusStruct;			//  [3666183]

private:

};



#endif


