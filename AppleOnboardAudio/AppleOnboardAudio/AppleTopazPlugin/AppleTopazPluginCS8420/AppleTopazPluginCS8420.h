/*
 *  AppleTopazPluginCS8420.h
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Tue Oct 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */


#ifndef __AppleTopazPluginCS8420
#define __AppleTopazPluginCS8420

#include "AppleTopazPlugin.h"
#include <IOKit/IOService.h>

//	====================================================================================================
//
//	Register bit fields
//	NOTE:	Driver default settings indicated with '¥'.
//

enum CS8420_MISC_CNTRL_1 {
	map_CS8420_MISC_CNTRL_1			=	1,				//	Memory Address Pointer:					Miscellaneous Control Register 1
	baCS8420_SWCLK					=	7,				//	bit addressed field:					Clock Control
		bvCS8420_swclkRMCK			=	0,				//		( swclkRMCK << baSWCLK )			RMCK (default)
		bvCS8420_swclkOMCK			=	1,				//		( swclkOMCK << baSWCLK )		¥	OMCK
	baCS8420_VSET					=	6,				//	bit addressed field:					Transmitted 'V' bit level
		bvCS8420_vbitValid			=	0,				//		( vbitValid << baVSET )			¥	'V' bit indicates VALID / PCM (default)
		bvCS8420_vbitInvalid		=	1,				//		( vbitInvalid << baVSET )			'V' bit indicates INVALID / NON-PCM
	baCS8420_MuteSAO				=	5,				//	bit addressed field:					Mute Serial Audio Output
		bvCS8420_normalSAO			=	0,				//		( normalSAO << baMuteSAO )		¥	Serial Audio Output not muted (default)
		bvCS8420_muteSAO			=	1,				//		( muteSAO << baMuteSAO )			Serial Audio Output muted
	baCS8420_MuteAES				=	4,				//	bit addressed field:					Mute AES3 Transmitter Output
		bvCS8420_normalAES3			=	0,				//		( normalAES3 << baMuteAES )		¥	AES3 Output not muted (default)
		bvCS8420_muteAES3			=	1,				//		( muteAES3 << baMuteAES )			OMAES3 Output mutedCK
	baCS8420_DITH					=	3,				//	bit addressed field:					Dither control
		bvCS8420_enableDITH			=	0,				//		( enableDITH << baDITH )		¥	Enable triangle dither (default)
		bvCS8420_disableDITH		=	1,				//		( disableDITH << baDITH )			Disable triangle dither
	baCS8420_INT					=	1,				//	bit addressed field:					interrupt output pin control
		bvCS8420_activeHighINT		=	0,				//		( activeHighINT << baINT )		¥	active high interrupt output (default)
		bvCS8420_activeLowINT		=	1,				//		( activeLowINT << baINT )			active low interrupt output
		bvCS8420_activeLowODINT		=	2,				//		( activeLowODINT << baINT )			active low open drain interrupt output
	baCS8420_TCBLD					=	0,				//	bit addressed field:					Transmit channel status block direction
		bvCS8420_inputTCBLD			=	0,				//		( inputTCBLD << baTCBLD )		¥	Transmit channel status block pin is input (default)
		bvCS8420_outputTCBLD		=	1				//		( outputTCBLD << baTCBLD )			Transmit channel status block pin is output
};

enum CS8420_MISC_CNTRL_2 {
	map_CS8420_MISC_CNTRL_2			=	2,				//	Memory Address Pointer:					Miscellaneous Control Register 2
	baCS8420_TRUNC					=	7,				//	bit addressed field:					Word Length
		bvCS8420_notTruncated		=	0,				//		( notTruncated << baTRUNC )		¥	Data to the SRC is not truncated (default)
		bvCS8420_useAUX				=	1,				//		( useAUX << baTRUNC )				Data to the SRC is set according to AUX field in data stream
	baCS8420_HOLD					=	5,				//	bit addressed field:					Receiver Error Handling Action
		bvCS8420_lastValid			=	0,				//		( lastValid << baHOLD )				Hold the last valid audio sample (default)
		bvCS8420_replaceMute		=	1,				//		( replaceMute << baHOLD )		¥	Replace the current audio sample with mute
		bvCS8420_unchanged			=	2,				//		( unchanged << baHOLD )				Do not change the current audio sample
	baCS8420_RMCKF					=	4,				//	bit addressed field:					Recovered master clock output frequency
		bvCS8420_fsi256				=	0,				//		( fsi256 << baRMCKF )			¥	256 * frequency of input sample rate (default)
		bvCS8420_fsi128				=	1,				//		( fsi128 << baRMCKF )				128 * frequency of input sample rate
	baCS8420_MMR					=	3,				//	bit addressed field:					Select AES receiver mono or stereo
		bvCS8420_mmrStereo			=	0,				//		( mmrStereo << baMMR )			¥	A and B subframes stereo (default)
		bvCS8420_mmrMono			=	1,				//		( mmrMono << baMMR )				A and B subframes mono
	baCS8420_MMT					=	2,				//	bit addressed field:					Select AES transmitter mono or stereo
		bvCS8420_mmtStereo			=	0,				//		( mmtStereo << baMMT )			¥	A = Left and B = Right for stereo (default)
		bvCS8420_mmtUseMMTLR		=	1,				//		( mmtUseMMTLR << baMMT )			MMTLR selects source
	baCS8420_MMTCS					=	1,				//	bit addressed field:					Select A or B channel status data to transmit in mono mode
		bvCS8420_discrete			=	0,				//		( normalAES3 << baMMTCS )		¥	Use ch. A CS for A sub-frame, B CS for B sub-frame(default)
		bvCS8420_useMMTLR			=	1,				//		( muteAES3 << baMMTCS )				Use MMTLR
	baCS8420_MMTLR					=	0,				//	bit addressed field:					Channel selection for AES transmitter mono mode
		bvCS8420_useLeft			=	0,				//		( useLeft << baMMTLR )			¥	Use left channel input data for consecutive sub-frame outputs (default)
		bvCS8420_useRight			=	1				//		( useRight << baMMTLR )				Use right channel input data for consecutive sub-frame outputs
};

enum CS8420_DATA_FLOW_CTRL {
	map_CS8420_DATA_FLOW_CTRL		=	3,				//	Memory Address Pointer:					Data Flow Control
	baCS8420_AMLL					=	7,				//	bit addressed field:					Auto Mute Lock Lost
		bvCS8420_disAutoMute		=	0,				//		( disAutoMute << baAMLL )			Disable auto mute on loss of lock (default)
		bvCS8420_enAutoMute			=	1,				//		( enAutoMute << baAMLL )		¥	Enable auto mute on loss of lock
	baCS8420_TXOFF					=	6,				//	bit addressed field:					AES3 Transmitter Output Driver Control
		bvCS8420_aes3TXNormal		=	0,				//		( aes3TXNormal << baTXOFF )		¥	AES3 transmitter output pin drivers normal operation (default)
		bvCS8420_aes3TX0v			=	1,				//		( aes3TX0v << baTXOFF )				AES3 transmitter output pin drivers at 0 volts
	baCS8420_AESBP					=	5,				//	bit addressed field:					AES3 bypass mode selection
		bvCS8420_normalBP			=	0,				//		( normalBP << baAESBP )			¥	normal operation (default)
		bvCS8420_rxpPB				=	1,				//		( rxpPB << baAESBP )				connect AES3 ttx to RXP pin
	baCS8420_TXD					=	3,				//	bit addressed field:					AES3 Transmitter Data Source
		bvCS8420_txdSrcOut			=	0,				//		( txdSrcOut << baTXD )				SRC Output (default)
		bvCS8420_txdSAI				=	1,				//		( txdSAI << baTXD )				¥	Serial Audio Input port
		bvCS8420_txdAES3			=	2,				//		( txdAES3 << baTXD )				AES3 receiver
	baCS8420_SPD					=	1,				//	bit addressed field:					Serial Audio Output Port Data Source
		bvCS8420_spdSrcOut			=	0,				//		( spdSrcOut << baSPD )			¥	SRC Output (default) (use when cpu is clock master & CS8420 is slave)
		bvCS8420_spdSAI				=	1,				//		( spdSAI << baSPD )					Serial Audio Input port
		bvCS8420_spdAES3			=	2,				//		( spdAES3 << baSPD )				AES3 receiver (use when CS8420 is clock master and cpu is slave)
		bvCS8420_spdMASK			=	3,				//		( spdMASK << baSPD )
	baCS8420_SRCD					=	0,				//	bit addressed field:					Input Data Source for Sample Rate Converter (SRC)
		bvCS8420_srcdSAIP			=	0,				//		( srcdSAIP << baSRCD )			¥	Serial Audio Input Port (default)
		bvCS8420_srcdAES3			=	1				//		( srcdAES3 << baSRCD )				AES3 Receiver
};

enum CS8420_CLOCK_SOURCE_CTRL {
	map_CS8420_CLOCK_SOURCE_CTRL	=	4,			//	Memory Address Pointer:					Clock Source Control
	baCS8420_RUN					=	6,				//	bit addressed field:					Clock Control [Power Management]
		bvCS8420_runSTOP			=	0,				//		( runSTOP << baRUN )				Low Power operation (default)
		bvCS8420_runNORMAL			=	1,				//		( runNORMAL << baRUN )			¥	Normal operation
	baCS8420_CLK					=	4,				//	bit addressed field:					Output master clock input frequency to output sample rate ratio
		bvCS8420_omck256fso			=	0,				//		( omck256fso << baCLK )			¥	OMCK frequency is 256 * Fso (default)
		bvCS8420_omck384fso			=	1,				//		( omck384fso << baCLK )				OMCK frequency is 384 * Fso
		bvCS8420_omck512fs0			=	2,				//		( omck512fs0 << baCLK )				OMCK frequency is 512 * Fso
	baCS8420_OUTC					=	3,				//	bit addressed field:					Output Time Base
		bvCS8420_outcOmckXbaCLK		=	0,				//		( outcOmckXbaCLK << baOUTC )	¥	OMCK input pin modified by the selected divide ratio indicated in baCLK (default)
		bvCS8420_outcRecIC			=	1,				//		( outcRecIC << baOUTC )				Recovered Input Clock
	baCS8420_INC					=	2,				//	bit addressed field:					Input Time Base Clock Source
		bvCS8420_incRecIC			=	0,				//		( incRecIC << baINC )			¥	Recovered Input clock (default)
		bvCS8420_incOmckXbaCLK		=	1,				//		( incOmckXbaCLK << baINC )			OMCK input pin modified by selected divide ratio indicated in baCLK
	baCS8420_RXD					=	0,				//	bit addressed field:					Recovered Input Clock Source
		bvCS8420_rxd256fsiILRCLK	=	0,				//		( rxd256fsiILRCLK << baRXD )		256 * Fsi derived from ILRCK pin [must be slave mode] (default)
		bvCS8420_rxd256fsiAES3		=	1,				//		( rxd256fsiAES3 << baRXD )		¥	256 * Fsi derived from AES3 input frame rate
		bvCS8420_rxd256fsiExt		=	2				//		( rxd256fsiExt << baRXD )			Bypass PLL and apply external 256 * Fsi derived from RMCK
};

enum CS8420_SERIAL_AUDIO_INPUT_FORMAT {
	map_CS8420_SERIAL_INPUT_FMT		=	5,				//	Memory Address Pointer:					Serial Audio Input Port Data Format
	baCS8420_SIMS					=	7,				//	bit addressed field:					Serial Input Master / Slave Mode Selector
		bvCS8420_inputSlave			=	0,				//		( inputSlave << baSIMS )		¥	Serial audio input slave mode (default)
		bvCS8420_inputMaster		=	1,				//		( inputMaster << baSIMS )			Serial audio input master mode
	baCS8420_SISF					=	6,				//	bit addressed field:					ISCLK frequency for Master Mode
		bvCS8420_isclk64fsi			=	0,				//		( isclk64fsi << baSISF )		¥	64 * Fsi (default)
		bvCS8420_isclk128fsi		=	1,				//		( isclk128fsi << baSISF )			128 * Fsi
	baCS8420_SIRES					=	4,				//	bit addressed field:					Resolution of the input data, for right justified formats
		bvCS8420_input24bit			=	0,				//		( input24bit << baSIRES )		¥	24 bit resolution (default)
		bvCS8420_input20bit			=	1,				//		( input20bit << baSIRES )			20 bit resolution
		bvCS8420_input16bit			=	2,				//		( input16bit << baSIRES )			16 bit resolution
	baCS8420_SIJUST					=	3,				//	bit addressed field:					Justification of SDIN data relative to ILRCK
		bvCS8420_siLeftJust			=	0,				//		( siLeftJust << baSIJUST )		¥	Left justified (default)
		bvCS8420_siRightJust		=	1,				//		( siRightJust << baSIJUST )			Right justified
	baCS8420_SIDEL					=	2,				//	bit addressed field:					Delay of SDIN data relative to ILRCK, for left justified data formats (i.e. I2S)
		bvCS8420_siMsb1stCk			=	0,				//		( siMsb1stCk << baSIDEL )			MSB of SDIN data occurs at first ISCLK after ILRCK (default)
		bvCS8420_siMsb2ndCk			=	1,				//		( siMsb2ndCk << baSIDEL )		¥	MSB of SDIN data occurs at second ISCLK after ILRCK
	baCS8420_SISPOL					=	1,				//	bit addressed field:					ISCLK clock polarity
		bvCS8420_siRising			=	0,				//		( siRising << baSISPOL )		¥	SDIN sampled on rising edge of ISCLK (default)
		bvCS8420_siFalling			=	1,				//		( siFalling << baSISPOL )			SDIN sampled on falling edge of ISCLK
	baCS8420_SILRPOL				=	0,				//	bit addressed field:					ILRCK clock polarity
		bvCS8420_siLeftILRCK		=	0,				//		( siLeftILRCK << baSILRPOL )		SDIN data is for left channel when ILRCK is high (default)
		bvCS8420_siRightILRCK		=	1				//		( siRightILRCK << baSILRPOL )	¥	SDIN data is for right channel when ILRCK is high
};

enum CS8420_SERIAL_AUDIO_OUTPUT_FORMAT {
	map_CS8420_SERIAL_OUTPUT_FMT	=	6,				//	Memory Address Pointer:					Serial Audio Output Port Data Format
	baCS8420_SOMS					=	7,				//	bit addressed field:					Serial Output Master / Slave Mode Selector
		bvCS8420_somsSlave			=	0,				//		( somsSlave << baSOMS )			¥	Serial audio output port is in slave mode (default)
		bvCS8420_somsMaster			=	1,				//		( somsMaster << baSOMS )			Serial audio output port is in master mode
	baCS8420_SOSF					=	6,				//	bit addressed field:					OSCLK frequency for master mode
		bvCS8420_osclk64Fso			=	0,				//		( osclk64Fso << baSOSF )		¥	64 * Fso (default)
		bvCS8420_osclk128Fso		=	1,				//		( osclk128Fso << baSOSF )			128 * Fso
	baCS8420_SORES					=	4,				//	bit addressed field:					Resolution of the output data on SDOUT and the AES3 output
		bvCS8420_out24bit			=	0,				//		( out24bit << baSORES )			¥	24 bit resolution (default)
		bvCS8420_out20bit			=	1,				//		( out20bit << baSORES )				20 bit resolution
		bvCS8420_out16bit			=	2,				//		( out16bit << baSORES )				16 bit resolution
		bvCS8420_outAES3			=	3,				//		( outAES3 << baSORES )				Direct copy of the received NRZ data from the AES3 receiver
	baCS8420_SOJUST					=	3,				//	bit addressed field:					Justification of the SDOUT data relative tot he OLRCK
		bvCS8420_soLeftJust			=	0,				//		( soLeftJust << baSOJUST )		¥	Left justified (default)
		bvCS8420_soRightJust		=	1,				//		( soRightJust << baSOJUST )			Right justified
	baCS8420_SODEL					=	2,				//	bit addressed field:					Delay of SDOUT data relative to OLRCK for left justified (i.e. I2S) data formats
		bvCS8420_soMsb1stCk			=	0,				//		( soMsb1stCk << baSODEL )			MSB of SDOUT data occurs on the first OSCLK after OLRCK edge (default)
		bvCS8420_soMsb2ndCk			=	1,				//		( soMsb2ndCk << baSODEL )		¥	MSB of SDOUT data occurs on the second OSCLK after OLRCK edge
	baCS8420_SOSPOL					=	1,				//	bit addressed field:					OSCLK clock polarity
		bvCS8420_sdoutFalling		=	0,				//		( sdoutFalling << baSOSPOL )	¥	SDOUT transitions occur on falling edges of OSCLK (default)
		bvCS8420_sdoutRising		=	1,				//		( sdoutRising << baSOSPOL )			SDOUT transitions occur on rising edges of OSCLK
	baCS8420_SOLRPOL				=	0,				//	bit addressed field:					OLRCK clock polarity
		bvCS8420_soLeftOLRCK		=	0,				//		( soLeftOLRCK << baSOLRPOL )		SDOUT data is for the left channel when OLRCK is high (default)
		bvCS8420_soRightOLRCK		=	1				//		( soRightOLRCK << baSOLRPOL )	¥	SDOUT data is for the right channel when OLRCK is high
};

enum CS8420_IRQ_STATUS1 {
	map_CS8420_IRQ1_STATUS		=	7,				//	Memory Address Pointer:					Interrupt 1 Register Status (all bits active high)
	baCS8420_TSLIP				=	7,				//	bit addressed field:					AES3 transmitter souce data slip interrupt
	baCS8420_OSLIP				=	6,				//	bit addressed field:					Serial audio output port data slip interrupt
	baCS8420_SRE				=	5,				//	bit addressed field:					Sample Rate range exceeded indicator (( 3 > ( Fsi/Fso )) | ( 3 > ( Fso/Fsi )))
	baCS8420_OVRGL				=	4,				//	bit addressed field:					Over range indicator for left (A) channel SRC output
	baCS8420_OVRGR				=	3,				//	bit addressed field:					Over range indicator for Right (B) channel SRC output
	baCS8420_DETC				=	2,				//	bit addressed field:					D to E C-buffer transfer interrupt
	baCS8420_EFTC				=	1,				//	bit addressed field:					E to F C-buffer transfer interrupt
	baCS8420_RERR				=	0				//	bit addressed field:					Receiver error -> requires read of receiver error register
};

enum CS8420_IRQ_STATUS2 {
	map_CS8420_IRQ2_STATUS		=	8,				//	Memory Address Pointer:					Interrupt 2 Register Status (all bits active high)
	baCS8420_VFIFO				=	5,				//	bit addressed field:					Varispeed FIFO overflow indicator (SRC data buffer overflow)
	baCS8420_REUNLOCK			=	4,				//	bit addressed field:					Sample rate converter unlock
	baCS8420_DETU				=	3,				//	bit addressed field:					D to E U-buffer transfer error
	baCS8420_EFTU				=	2,				//	bit addressed field:					E to F U-buffer transfer error
	baCS8420_QCH				=	1,				//	bit addressed field:					A new block of Q-subcode data is available for reading
	baCS8420_UOVW				=	0				//	bit addressed field:					U-bit FIFO overwrite occurs on an overwrite in the U-bit FIFO
};

enum CS8420_IRQ_MASK1 {
	map_CS8420_IRQ1_MASK		=	9,				//	Memory Address Pointer:					Interrupt 1 Register Mask (set bit field to '0' to mask, '1' to enable interrupt)
	baCS8420_TSLIPM				=	7,				//	bit addressed field:					'0' to mask, '1' to enable AES3 tx source data slip interrupt
	baCS8420_OSLIPM				=	6,				//	bit addressed field:					'0' to mask, '1' to enable Serial audio output port data slip interrupt
	baCS8420_SREM				=	5,				//	bit addressed field:					'0' to mask, '1' to enable Sample Rate range exceeded indicator
	baCS8420_OVRGLM				=	4,				//	bit addressed field:					'0' to mask, '1' to enable Over range indicator for left (A) channel SRC output
	baCS8420_OVRGRM				=	3,				//	bit addressed field:					'0' to mask, '1' to enable Over range indicator for right (A) channel SRC output
	baCS8420_DETCM				=	2,				//	bit addressed field:					'0' to mask, '1' to enable D to E C-buffer transfer interrupt
	baCS8420_EFTCM				=	1,				//	bit addressed field:					'0' to mask, '1' to enable E to F C-buffer transfer interrupt
	baCS8420_RERRM				=	0				//	bit addressed field:					'0' to mask, '1' to enable Receiver error -> requires read of receiver error register
};

enum CS8420_IRQ_MODE1_MSB {
	map_CS8420_IRQ1_MODE_MSB	=	10,				//	Memory Address Pointer:					Interrupt Register 1 Mode Register MSB
	baCS8420_TSLIP1				=	7,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_OSLIP1				=	6,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_SRE1				=	5,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_OVRGL1				=	4,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_OVRGR1				=	3,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_DETC1				=	2,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_EFTC1				=	1,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_RERR1				=	0				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8420_IRQ_MODE1_LSB {
	map_CS8420_IRQ1_MODE_LSB	=	11,				//	Memory Address Pointer:					Interrupt Register 1 Mode Register LSB
	baCS8420_TSLIP0				=	7,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_OSLIP0				=	6,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_SRE0				=	5,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_OVRGL0				=	4,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_OVRGR0				=	3,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_DETC0				=	2,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_EFTC0				=	1,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_RERR0				=	0				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8420_IRQ_MASK2 {
	map_CS8420_IRQ2_MASK		=	12,				//	Memory Address Pointer:					Interrupt 2 Register Mask (set bit field to '0' to mask, '1' to enable interrupt)
	baCS8420_VFIFOM				=	5,				//	bit addressed field:					'0' to mask, '1' to enable Varispeed FIFO overflow indicator (SRC data buffer overflow)
	baCS8420_REUNLOCKM			=	4,				//	bit addressed field:					'0' to mask, '1' to enable Sample rate converter unlock
	baCS8420_DETUM				=	3,				//	bit addressed field:					'0' to mask, '1' to enable D to E U-buffer transfer error
	baCS8420_EFTUM				=	2,				//	bit addressed field:					'0' to mask, '1' to enable E to F U-buffer transfer error
	baCS8420_QCHM				=	1,				//	bit addressed field:					'0' to mask, '1' to enable A new block of Q-subcode data is available for reading
	baCS8420_UOVWM				=	0				//	bit addressed field:					'0' to mask, '1' to enable U-bit FIFO overwrite occurs on an overwrite in the U-bit FIFO
};

enum CS8420_IRQ_MODE2_MSB {
	map_CS8420_IRQ2_MODE_MSB	=	13,				//	Memory Address Pointer:					Interrupt Register 2 Mode Register MSB
	baCS8420_VFIFOM1			=	5,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_REUNLOCKM1			=	4,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_DETUM1				=	3,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_EFTUM1				=	2,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_QCHM1				=	1,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_UOVWM1				=	0				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8420_IRQ_MODE2_LSB {
	map_CS8420_IRQ2_MODE_LSB	=	14,				//	Memory Address Pointer:					Interrupt Register 2 Mode Register LSB
	baCS8420_VFIFOM0			=	5,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_REUNLOCKM0			=	4,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_DETUM0				=	3,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_EFTUM0				=	2,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_QCHM0				=	1,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baCS8420_UOVWM0				=	0				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8420_RX_CH_STATUS {
	map_CS8420_RX_CH_STATUS		=	15,				//	Memory Address Pointer:					Receiver channel status
	baCS8420_AUX				=	4,				//	bit addressed field:					Indicates the width of incoming auxiliary data field derived from AES3 channel status bits (0 to 8)
	baCS8420_PPRO				=	3,				//	bit addressed field:					Channel status block format indicator
		bvCS8420_consumer		=	0,				//		( consumer << baPPRO )				Received data is in consumer format
		bvCS8420_professional	=	1,				//		( professional << baPPRO )			Received data is in consumer format
	baCS8420_AUDIO				=	2,				//	bit addressed field:					Audio indicator
		bvCS8420_pcmAudio		=	0,				//		( pcmAudio << baAUDIO )				Received data is linearly coded PCM data
		bvCS8420_encodedAudio	=	1,				//		( encodedAudio << baAUDIO )			Received data is not linearly coded PCM data
	baCS8420_COPY				=	1,				//	bit addressed field:					SCMS copyright indicator
		bvCS8420_copyrighted	=	0,				//		( copyrighted << baCOPY )			Copyright asserted
		bvCS8420_publicDomain	=	1,				//		( publicDomain << baCOPY )			Copyright not asserted
	baCS8420_ORIG				=	0,				//	bit addressed field:					SCMS generation indicator as decoded from category code in 'L' bit
		bvCS8420_notOriginal	=	0,				//		( notOriginal << baORIG )			Received data is 1st generation or higher copy
		bvCS8420_original		=	1				//		( original << baORIG )				Received data is original
};

enum CS8420_RX_ERROR {
	map_CS8420_RX_ERROR			=	16,				//	Memory Address Pointer:					Receiver Error
	baCS8420_QCRC				=	6,				//	bit addressed field:					QCRC Q-subcode data CRC error updated on Q-subcode block boundaries
		bvCS8420_qcrcNoErr		=	0,				//		( qcrcNoErr << baQCRC )				No Error
		bvCS8420_qcrcError		=	1,				//		( qcrcError << baQCRC )				Error occurred
	baCS8420_CCRC				=	5,				//	bit addressed field:					CCRC channel status block CRC error updated on channel status block boundaries
		bvCS8420_ccrcNoErr		=	0,				//		( ccrcNoErr << baCCRC )				No Error - valid only in PROFESSIONAL mode ONLY
		bvCS8420_ccrcError		=	1,				//		( ccrcError << baCCRC )				Error occurred - valid only in PROFESSIONAL mode ONLY
	baCS8420_UNLOCK				=	4,				//	bit addressed field:					PLL lock status bit updated on channel status block boundaries
		bvCS8420_pllLocked		=	0,				//		( pllLocked << baQCRC )				phase locked loop locked
		bvCS8420_pllUnlocked	=	1,				//		( pllUnlocked << baQCRC )			phase locked loop not locked
	baCS8420_VALID				=	3,				//	bit addressed field:					Received AES3 validity bit status updated on sub-frame boundaries
		bvCS8420_dataValid		=	0,				//		( dataValid << baVALID )			Data is valid and is normally linear coded PCM audio
		bvCS8420_dataInvalid	=	1,				//		( dataInvalid << baVALID )			Data is invalid OR may be valid compressed (non-PCM) audio
	baCS8420_CONF				=	2,				//	bit addressed field:					Confidence bit updated on sub-frame boundaries
		bvCS8420_confNoErr		=	0,				//		( confNoErr << baCONF )				No Error
		bvCS8420_confError		=	1,				//		( confError << baCONF )				Confidence error indicates received data eye opening is less than 
	baCS8420_BIP				=	1,				//	bit addressed field:					Bi-phase error updated on sub-frame boundaries
		bvCS8420_bipNoErr		=	0,				//		( bipNoErr << baBIP )				No error
		bvCS8420_bipError		=	1,				//		( bipError << baBIP )				Error in bi-phase coding
	baCS8420_PARITY				=	0,				//	bit addressed field:					Parity error updated on sub-frame boundaries
		bvCS8420_parNoErr		=	0,				//		( parNoErr << baPARITY )			No error
		bvCS8420_parError		=	1				//		( parError << baPARITY )			Parity error
};

enum CS8420_RX_ERROR_MASK {
	map_CS8420_RX_ERROR_MASK	=	17,				//	Memory Address Pointer:					Receiver Error Mask
	baCS8420_QCRCM				=	6,				//	bit addressed field:				1	'0' to mask, '1' to enable QCRC Q-subcode data CRC error updated on Q-subcode block boundaries
	baCS8420_CCRCM				=	5,				//	bit addressed field:				1	'0' to mask, '1' to enable CCRC channel status block CRC error updated on channel status block boundaries
	baCS8420_UNLOCKM			=	4,				//	bit addressed field:				1	'0' to mask, '1' to enable PLL lock status bit updated on channel status block boundaries
	baCS8420_VALIDM				=	3,				//	bit addressed field:				1	'0' to mask, '1' to enable Received AES3 validity bit status updated on sub-frame boundaries
	baCS8420_CONFM				=	2,				//	bit addressed field:				1	'0' to mask, '1' to enable Confidence bit updated on sub-frame boundaries
	baCS8420_BIPM				=	1,				//	bit addressed field:				1	'0' to mask, '1' to enable Bi-phase error updated on sub-frame boundaries
	baCS8420_PARITYM			=	0,				//	bit addressed field:				1	'0' to mask, '1' to enable Parity error updated on sub-frame boundaries
		bvCS8420_maskRxError	=	0,				//		( maskRxError >> ??? )				Mask the error interrupt
		bvCS8420_enRxError		=	1				//		( enRxError >> ??? )				Enable the error interrupt
};

enum CS8420_CH_STATUS_DATA_BUF_CTRL {
	map_CS8420_CH_STATUS_DATA_BUF_CTRL	=	18,		//	Memory Address Pointer:					Channel Status Data Buffer Control
	baCS8420_BSEL				=	5,				//	bit addressed field:					'0' to mask, '1' to enable 
		bvCS8420_bselChStat		=	0,				//		( bselChStat << baBSEL )			Data buffer address space contains channel status data (default)
		bvCS8420_bselUserStat	=	1,				//		( bselUserStat << baBSEL )			Data buffer address space contains user status data
	baCS8420_CBMR				=	4,				//	bit addressed field:					Control the first 5 bytes of the channel status "E" buffer
		bvCS8420_first5DtoE		=	0,				//		( first5DtoE << baCBMR )			Allow D to E buffer transfers to overwrite the first 5 bytes of channel status data (default)
		bvCS8420_first5ChStat	=	1,				//		( first5ChStat << baCBMR )			Prevent D to E buffer transfers from overwriting the first 5 bytes of channel status data
	baCS8420_DETCI				=	3,				//	bit addressed field:					D to E C-data buffer transfer inhibit bit
		bvCS8420_enCDataDtoE	=	0,				//		( enCDataDtoE << baDETCI )			Allow C-data D to E buffer transfers (default)
		bvCS8420_disCDataDtoE	=	1,				//		( disCDataDtoE << baDETCI )			Inhibit C-data D to E buffer transfers
	baCS8420_EFTCI				=	2,				//	bit addressed field:					E to F C-data buffer transfer inhibit bit
		bvCS8420_enCDataEtoF	=	0,				//		( enCDataEtoF << baEFTCI )			Allow C-data E to E buffer transfers (default)
		bvCS8420_disCDataEtoF	=	1,				//		( disCDataEtoF << baEFTCI )			Inhibit C-data E to E buffer transfers
	baCS8420_CAM				=	1,				//	bit addressed field:					C-data buffer control port access mode
		bvCS8420_onebyte		=	0,				//		( onebyte << baCAM )				One byte mode (default)
		bvCS8420_twoByte		=	1,				//		( twoByte << baCAM )				Two byte mode
	baCS8420_CHS				=	0,				//	bit addressed field:					Channel select bit
		bvCS8420_channelA		=	0,				//		( channelA << baCHS )				Channel A info at *EMPH pin and RX channel status register (default)
		bvCS8420_channelB		=	1				//		( channelB << baCHS )				Channel B info at *EMPH pin and RX channel status register
};

enum CS8420_USER_DATA_BUF_CTRL {
	map_CS8420_USER_DATA_BUF_CTRL	=	19,			//	Memory Address Pointer:					User Data Buffer control
	baCS8420_UD					=	4,				//	bit addressed field:					User data pin data direction specifier
		bvCS8420_udataIn		=	0,				//		( udataIn << baUD )					U-pin is an input (default)
		bvCS8420_udataOut		=	1,				//		( udataOut << baUD )				U-pin is an output
	baCS8420_UBM				=	2,				//	bit addressed field:					AES3 U bit manager operating mode
		bvCS8420_ubmTX0			=	0,				//		( ubmTX0 << baUBM )					Transmit all zeros mode (default)
		bvCS8420_ubmBlock		=	1,				//		( ubmBlock << baUBM )				Block mode
		bvCS8420_ubmIECmode4	=	3,				//		( ubmIECmode4 << baUBM )			IEC consumer mode 4
	baCS8420_DETUI				=	1,				//	bit addressed field:					D to E U-data buffer transfer inhibit bit (valid in block mode only)
		bvCS8420_enUDataDtoE	=	0,				//		( enUDataDtoE << baDETUI )			Allow U-data D to E buffer transfers (default)
		bvCS8420_disUDataDtoE	=	1,				//		( disUDataDtoE << baDETUI )			Inhibit U-data D to E buffer transfers
	baCS8420_EFTUI				=	0,				//	bit addressed field:					E to F U-data buffer transfer inhibit bit (valid in block mode only)
		bvCS8420_enUDataEtoF	=	0,				//		( enUDataEtoF << baEFTUI )			Allow U-data E to F buffer transfers (default)
		bvCS8420_disUDataEtoF	=	1				//		( disUDataEtoF << baEFTUI )			Inhibit U-data E to F buffer transfers
};

enum CS8420_Q_CHANNEL_SUBCODE {
	map_CS8420_Q_CHANNEL_SUBCODE_AC			=	20,		//	Memory Address Pointer:					Q-Channel subcode byte 0 - Address & Control
	baCS8420_QChAddress		=	4,						//	bit address field:						Address
	baCS8420_QChControl		=	0,						//	bit addressed field:					Control
	map_CS8420_Q_CHANNEL_SUBCODE_TRK		=	21,		//	Memory Address Pointer:					Q-Channel subcode byte 1
	map_CS8420_Q_CHANNEL_SUBCODE_INDEX		=	22,		//	Memory Address Pointer:					Q-Channel subcode byte 2
	map_CS8420_Q_CHANNEL_SUBCODE_MIN		=	23,		//	Memory Address Pointer:					Q-Channel subcode byte 3
	map_CS8420_Q_CHANNEL_SUBCODE_SEC		=	24,		//	Memory Address Pointer:					Q-Channel subcode byte 4
	map_CS8420_Q_CHANNEL_SUBCODE_FRAME		=	25,		//	Memory Address Pointer:					Q-Channel subcode byte 5
	map_CS8420_Q_CHANNEL_SUBCODE_ZERO		=	26,		//	Memory Address Pointer:					Q-Channel subcode byte 6
	map_CS8420_Q_CHANNEL_SUBCODE_ABS_MIN	=	27,		//	Memory Address Pointer:					Q-Channel subcode byte 7
	map_CS8420_Q_CHANNEL_SUBCODE_ABS_SEC	=	28,		//	Memory Address Pointer:					Q-Channel subcode byte 8
	map_CS8420_Q_CHANNEL_SUBCODE_ABS_FRAME	=	29		//	Memory Address Pointer:					Q-Channel subcode byte 9
};

enum CS8420_SAMPLE_RATE_RATIO {
	map_CS8420_SAMPLE_RATE_RATIO	=	30,			//	Memory Address Pointer:					Sample Rate Ratio
	baCS8420_Integer			=	6,				//	bit addressed field:					Integer portion of Fso / Fsi when PLL and SRC have reached lock and SRC is in use
	baCS8420_Fraction			=	0				//	bit addressed field:					Fraction portion of Fso / Fsi when PLL and SRC have reached lock and SRC is in use
};

enum CS8420_RESERVED_1F {
	map_CS8420_RESERVED_1F		=	31
};

enum CS8420_C_OR_U_BIT_BUFFER {					//	NOTE:  The channel / user status buffer should be read as a single 24 byte transaction with auto-increment enabled!
	map_CS8420_BUFFER_0			=	32,			//	Memory Address Pointer:						Channel Status or User Status Buffer  0	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_1			=	33,			//	Memory Address Pointer:						Channel Status or User Status Buffer  1	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_2			=	34,			//	Memory Address Pointer:						Channel Status or User Status Buffer  2	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_3			=	35,			//	Memory Address Pointer:						Channel Status or User Status Buffer  3	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_4			=	36,			//	Memory Address Pointer:						Channel Status or User Status Buffer  4	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_5			=	37,			//	Memory Address Pointer:						Channel Status or User Status Buffer  5	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_6			=	38,			//	Memory Address Pointer:						Channel Status or User Status Buffer  6	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_7			=	39,			//	Memory Address Pointer:						Channel Status or User Status Buffer  7	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_8			=	40,			//	Memory Address Pointer:						Channel Status or User Status Buffer  8	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_9			=	41,			//	Memory Address Pointer:						Channel Status or User Status Buffer  9	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_10		=	42,			//	Memory Address Pointer:						Channel Status or User Status Buffer 10	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_11		=	43,			//	Memory Address Pointer:						Channel Status or User Status Buffer 11	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_12		=	44,			//	Memory Address Pointer:						Channel Status or User Status Buffer 12	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_13		=	45,			//	Memory Address Pointer:						Channel Status or User Status Buffer 13	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_14		=	46,			//	Memory Address Pointer:						Channel Status or User Status Buffer 14	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_15		=	47,			//	Memory Address Pointer:						Channel Status or User Status Buffer 15	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_16		=	48,			//	Memory Address Pointer:						Channel Status or User Status Buffer 16	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_17		=	49,			//	Memory Address Pointer:						Channel Status or User Status Buffer 17	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_18		=	50,			//	Memory Address Pointer:						Channel Status or User Status Buffer 18	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_19		=	51,			//	Memory Address Pointer:						Channel Status or User Status Buffer 19	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_20		=	52,			//	Memory Address Pointer:						Channel Status or User Status Buffer 20	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_21		=	53,			//	Memory Address Pointer:						Channel Status or User Status Buffer 21	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_22		=	54,			//	Memory Address Pointer:						Channel Status or User Status Buffer 22	(see Channel Status Block Structure below)
	map_CS8420_BUFFER_23		=	55			//	Memory Address Pointer:						Channel Status or User Status Buffer 23	(see Channel Status Block Structure below)
};

#define	kCS8420_MISC_CNTRL_1_INIT				(	( bvCS8420_swclkOMCK << baCS8420_SWCLK ) | \
													( bvCS8420_vbitValid << baCS8420_VSET ) | \
													( bvCS8420_normalSAO << baCS8420_MuteSAO ) | \
													( bvCS8420_muteAES3 << baCS8420_MuteAES ) | \
													( bvCS8420_enableDITH << baCS8420_DITH ) | \
													( bvCS8420_activeLowINT << baCS8420_INT ) | \
													( bvCS8420_outputTCBLD << baCS8420_TCBLD ) )
													
#define	kCS8420_MISC_CNTRL_2_INIT				(	( bvCS8420_notTruncated << baCS8420_TRUNC ) | \
													( bvCS8420_replaceMute << baCS8420_HOLD ) | \
													( bvCS8420_fsi256 << baCS8420_RMCKF ) | \
													( bvCS8420_mmrStereo << baCS8420_MMR ) | \
													( bvCS8420_mmtStereo << baCS8420_MMT ) | \
													( bvCS8420_discrete << baCS8420_MMTCS ) | \
													( bvCS8420_useLeft << baCS8420_MMTLR ) )
													
#define	kCS8420_DATA_FLOW_CTRL_INIT				(	( bvCS8420_enAutoMute << baCS8420_AMLL ) | \
													( bvCS8420_aes3TXNormal << baCS8420_TXOFF ) | \
													( bvCS8420_normalBP << baCS8420_AESBP ) | \
													( bvCS8420_txdSAI << baCS8420_TXD ) | \
													( bvCS8420_spdSrcOut << baCS8420_SPD ) | \
													( bvCS8420_srcdAES3 << baCS8420_SRCD ) )
													
#define	kCS8420_CLOCK_SOURCE_CTRL_INIT_STOP		(	( bvCS8420_runSTOP << baCS8420_RUN ) | \
													( bvCS8420_omck256fso << baCS8420_CLK ) | \
													( bvCS8420_outcOmckXbaCLK << baCS8420_OUTC ) | \
													( bvCS8420_incRecIC << baCS8420_INC ) | \
													( bvCS8420_rxd256fsiAES3 << baCS8420_RXD ) )
													
#define	kCS8420_CLOCK_SOURCE_CTRL_INIT			(	( bvCS8420_runNORMAL << baCS8420_RUN ) | \
													( bvCS8420_omck256fso << baCS8420_CLK ) | \
													( bvCS8420_outcOmckXbaCLK << baCS8420_OUTC ) | \
													( bvCS8420_incRecIC << baCS8420_INC ) | \
													( bvCS8420_rxd256fsiAES3 << baCS8420_RXD ) )
													
#define	kCS8420_SERIAL_AUDIO_INPUT_FORMAT_INIT	(	( bvCS8420_inputSlave << baCS8420_SIMS ) | \
													( bvCS8420_isclk64fsi << baCS8420_SISF ) | \
													( bvCS8420_input24bit << baCS8420_SIRES ) | \
													( bvCS8420_siLeftJust << baCS8420_SIJUST ) | \
													( bvCS8420_siMsb2ndCk << baCS8420_SIDEL ) | \
													( bvCS8420_siRising << baCS8420_SISPOL ) | \
													( bvCS8420_siRightILRCK << baCS8420_SILRPOL ) )
													
#define	kCS8420_SERIAL_AUDIO_OUTPUT_FORMAT_INIT	(	( bvCS8420_somsSlave << baCS8420_SOMS ) | \
													( bvCS8420_osclk64Fso << baCS8420_SOSF ) | \
													( bvCS8420_out24bit << baCS8420_SORES ) | \
													( bvCS8420_soLeftJust << baCS8420_SOJUST ) | \
													( bvCS8420_soMsb2ndCk << baCS8420_SODEL ) | \
													( bvCS8420_sdoutFalling << baCS8420_SOSPOL ) | \
													( bvCS8420_soRightOLRCK << baCS8420_SOLRPOL ) )
													
#define	kCS8420_RX_ERROR_MASK_DISABLE_RERR		(	( bvCS8420_maskRxError << baCS8420_QCRCM ) | \
													( bvCS8420_maskRxError << baCS8420_CCRCM ) | \
													( bvCS8420_maskRxError << baCS8420_UNLOCKM ) | \
													( bvCS8420_maskRxError << baCS8420_VALIDM ) | \
													( bvCS8420_maskRxError << baCS8420_CONFM ) | \
													( bvCS8420_maskRxError << baCS8420_BIPM ) | \
													( bvCS8420_maskRxError << baCS8420_PARITYM ) )
													
#define	kCS8420_RX_ERROR_MASK_ENABLE_RERR		(	( bvCS8420_maskRxError << baCS8420_QCRCM ) | \
													( bvCS8420_maskRxError << baCS8420_CCRCM ) | \
													( bvCS8420_enRxError << baCS8420_UNLOCKM ) | \
													( bvCS8420_maskRxError << baCS8420_VALIDM ) | \
													( bvCS8420_maskRxError << baCS8420_CONFM ) | \
													( bvCS8420_maskRxError << baCS8420_BIPM ) | \
													( bvCS8420_maskRxError << baCS8420_PARITYM ) )

#define	kCLOCK_UNLOCK_ERROR_TERMINAL_COUNT		3

class AppleTopazPluginCS8420 : public AppleTopazPlugin {
    OSDeclareDefaultStructors ( AppleTopazPluginCS8420 );

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

	virtual void			poll ( void );
	virtual void			notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue );
	
	virtual bool			supportsDigitalInput ( void ) { return TRUE; }
	virtual bool			supportsDigitalOutput ( void ) { return TRUE; }

	virtual UInt32			getClockLockTerminalCount () { return kCLOCK_UNLOCK_ERROR_TERMINAL_COUNT; }

protected:

	UInt32					mUnlockErrorCount;
	UInt32					mLockErrorCount;
	bool					mLockStatus;


	ChanStatusStruct		mChanStatusStruct;			//  [3669626]

private:

};





#endif

