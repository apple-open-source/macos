/*
 *  CS8420_hw.h
 *  AppleOnboardAudio
 *
 *  Created by Raymond Montagne on Wed Feb 19 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef __CS8420
#define	__CS8420

#include <libkern/OSTypes.h>

//	====================================================================================================
//
//	Control Port Register Bit Definitions.
//
//	A register within the CS8420 can only be accessed after setting the value of
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
//
//	Register bit fields
//	NOTE:	Driver default settings indicated with '¥'.
//

enum CS8420_MISC_CNTRL_1 {
	map_MISC_CNTRL_1	=	1,				//	Memory Address Pointer:					Miscellaneous Control Register 1
	baSWCLK				=	7,				//	bit addressed field:					Clock Control
		swclkRMCK		=	0,				//		( swclkRMCK << baSWCLK )			RMCK (default)
		swclkOMCK		=	1,				//		( swclkOMCK << baSWCLK )		¥	OMCK
	baVSET				=	6,				//	bit addressed field:					Transmitted 'V' bit level
		vbitValid		=	0,				//		( vbitValid << baVSET )			¥	'V' bit indicates VALID / PCM (default)
		vbitInvalid		=	1,				//		( vbitInvalid << baVSET )			'V' bit indicates INVALID / NON-PCM
	baMuteSAO			=	5,				//	bit addressed field:					Mute Serial Audio Output
		normalSAO		=	0,				//		( normalSAO << baMuteSAO )		¥	Serial Audio Output not muted (default)
		muteSAO			=	1,				//		( muteSAO << baMuteSAO )			Serial Audio Output muted
	baMuteAES			=	4,				//	bit addressed field:					Mute AES3 Transmitter Output
		normalAES3		=	0,				//		( normalAES3 << baMuteAES )		¥	AES3 Output not muted (default)
		muteAES3		=	1,				//		( muteAES3 << baMuteAES )			OMAES3 Output mutedCK
	baDITH				=	3,				//	bit addressed field:					Dither control
		enableDITH		=	0,				//		( enableDITH << baDITH )		¥	Enable triangle dither (default)
		disableDITH		=	1,				//		( disableDITH << baDITH )			Disable triangle dither
	baINT				=	1,				//	bit addressed field:					interrupt output pin control
		activeHighINT	=	0,				//		( activeHighINT << baINT )		¥	active high interrupt output (default)
		activeLowINT	=	1,				//		( activeLowINT << baINT )			active low interrupt output
		activeLowODINT	=	2,				//		( activeLowODINT << baINT )			active low open drain interrupt output
	baTCBLD				=	0,				//	bit addressed field:					Transmit channel status block direction
		inputTCBLD		=	0,				//		( inputTCBLD << baTCBLD )		¥	Transmit channel status block pin is input (default)
		outputTCBLD		=	1				//		( outputTCBLD << baTCBLD )			Transmit channel status block pin is output
};

enum CS8420_MISC_CNTRL_2 {
	map_MISC_CNTRL_2	=	2,				//	Memory Address Pointer:					Miscellaneous Control Register 2
	baTRUNC				=	7,				//	bit addressed field:					Word Length
		notTruncated	=	0,				//		( notTruncated << baTRUNC )		¥	Data to the SRC is not truncated (default)
		useAUX			=	1,				//		( useAUX << baTRUNC )				Data to the SRC is set according to AUX field in data stream
	baHOLD				=	5,				//	bit addressed field:					Receiver Error Handling Action
		lastValid		=	0,				//		( lastValid << baHOLD )				Hold the last valid audio sample (default)
		replaceMute		=	1,				//		( replaceMute << baHOLD )		¥	Replace the current audio sample with mute
		unchanged		=	2,				//		( unchanged << baHOLD )				Do not change the current audio sample
	baRMCKF				=	4,				//	bit addressed field:					Recovered master clock output frequency
		fsi256			=	0,				//		( fsi256 << baRMCKF )			¥	256 * frequency of input sample rate (default)
		fsi128			=	1,				//		( fsi128 << baRMCKF )				128 * frequency of input sample rate
	baMMR				=	3,				//	bit addressed field:					Select AES receiver mono or stereo
		mmrStereo		=	0,				//		( mmrStereo << baMMR )			¥	A and B subframes stereo (default)
		mmrMono			=	1,				//		( mmrMono << baMMR )				A and B subframes mono
	baMMT				=	2,				//	bit addressed field:					Select AES transmitter mono or stereo
		mmtStereo		=	0,				//		( mmtStereo << baMMT )			¥	A = Left and B = Right for stereo (default)
		mmtUseMMTLR		=	1,				//		( mmtUseMMTLR << baMMT )			MMTLR selects source
	baMMTCS				=	1,				//	bit addressed field:					Select A or B channel status data to transmit in mono mode
		discrete		=	0,				//		( normalAES3 << baMMTCS )		¥	Use ch. A CS for A sub-frame, B CS for B sub-frame(default)
		useMMTLR		=	1,				//		( muteAES3 << baMMTCS )				Use MMTLR
	baMMTLR				=	0,				//	bit addressed field:					Channel selection for AES transmitter mono mode
		useLeft			=	0,				//		( useLeft << baMMTLR )			¥	Use left channel input data for consecutive sub-frame outputs (default)
		useRight		=	1				//		( useRight << baMMTLR )				Use right channel input data for consecutive sub-frame outputs
};

enum CS8420_DATA_FLOW_CTRL {
	map_DATA_FLOW_CTRL	=	3,				//	Memory Address Pointer:					Data Flow Control
	baAMLL				=	7,				//	bit addressed field:					Auto Mute Lock Lost
		disAutoMute		=	0,				//		( disAutoMute << baAMLL )			Disable auto mute on loss of lock (default)
		enAutoMute		=	1,				//		( enAutoMute << baAMLL )		¥	Enable auto mute on loss of lock
	baTXOFF				=	6,				//	bit addressed field:					AES3 Transmitter Output Driver Control
		aes3TXNormal	=	0,				//		( aes3TXNormal << baTXOFF )		¥	AES3 transmitter output pin drivers normal operation (default)
		aes3TX0v		=	1,				//		( aes3TX0v << baTXOFF )				AES3 transmitter output pin drivers at 0 volts
	baAESBP				=	5,				//	bit addressed field:					AES3 bypass mode selection
		normalBP		=	0,				//		( normalBP << baAESBP )			¥	normal operation (default)
		rxpPB			=	1,				//		( rxpPB << baAESBP )				connect AES3 ttx to RXP pin
	baTXD				=	3,				//	bit addressed field:					AES3 Transmitter Data Source
		txdSrcOut		=	0,				//		( txdSrcOut << baTXD )				SRC Output (default)
		txdSAI			=	1,				//		( txdSAI << baTXD )				¥	Serial Audio Input port
		txdAES3			=	2,				//		( txdAES3 << baTXD )				AES3 receiver
	baSPD				=	1,				//	bit addressed field:					Serial Audio Output Port Data Source
		spdSrcOut		=	0,				//		( spdSrcOut << baSPD )			¥	SRC Output (default) (use when cpu is clock master & CS8420 is slave)
		spdSAI			=	1,				//		( spdSAI << baSPD )					Serial Audio Input port
		spdAES3			=	2,				//		( spdAES3 << baSPD )				AES3 receiver (use when CS8420 is clock master and cpu is slave)
		spdMASK			=	3,				//		( spdMASK << baSPD )
	baSRCD				=	0,				//	bit addressed field:					Input Data Source for Sample Rate Converter (SRC)
		srcdSAIP		=	0,				//		( srcdSAIP << baSRCD )			¥	Serial Audio Input Port (default)
		srcdAES3		=	1				//		( srcdAES3 << baSRCD )				AES3 Receiver
};

enum CS8420_CLOCK_SOURCE_CTRL {
	map_CLOCK_SOURCE_CTRL	=	4,			//	Memory Address Pointer:					Clock Source Control
	baRUN				=	6,				//	bit addressed field:					Clock Control [Power Management]
		runSTOP			=	0,				//		( runSTOP << baRUN )				Low Power operation (default)
		runNORMAL		=	1,				//		( runNORMAL << baRUN )			¥	Normal operation
	baCLK				=	4,				//	bit addressed field:					Output master clock input frequency to output sample rate ratio
		omck256fso		=	0,				//		( omck256fso << baCLK )			¥	OMCK frequency is 256 * Fso (default)
		omck384fso		=	1,				//		( omck384fso << baCLK )				OMCK frequency is 384 * Fso
		omck512fs0		=	2,				//		( omck512fs0 << baCLK )				OMCK frequency is 512 * Fso
	baOUTC				=	3,				//	bit addressed field:					Output Time Base
		outcOmckXbaCLK	=	0,				//		( outcOmckXbaCLK << baOUTC )	¥	OMCK input pin modified by the selected divide ratio indicated in baCLK (default)
		outcRecIC		=	1,				//		( outcRecIC << baOUTC )				Recovered Input Clock
	baINC				=	2,				//	bit addressed field:					Input Time Base Clock Source
		incRecIC		=	0,				//		( incRecIC << baINC )			¥	Recovered Input clock (default)
		incOmckXbaCLK	=	1,				//		( incOmckXbaCLK << baINC )			OMCK input pin modified by selected divide ratio indicated in baCLK
	baRXD				=	0,				//	bit addressed field:					Recovered Input Clock Source
		rxd256fsiILRCLK	=	0,				//		( rxd256fsiILRCLK << baRXD )		256 * Fsi derived from ILRCK pin [must be slave mode] (default)
		rxd256fsiAES3	=	1,				//		( rxd256fsiAES3 << baRXD )		¥	256 * Fsi derived from AES3 input frame rate
		rxd256fsiExt	=	2				//		( rxd256fsiExt << baRXD )			Bypass PLL and apply external 256 * Fsi derived from RMCK
};

enum CS8420_SERIAL_AUDIO_INPUT_FORMAT {
	map_SERIAL_INPUT_FMT	=	5,			//	Memory Address Pointer:					Serial Audio Input Port Data Format
	baSIMS				=	7,				//	bit addressed field:					Serial Input Master / Slave Mode Selector
		inputSlave		=	0,				//		( inputSlave << baSIMS )		¥	Serial audio input slave mode (default)
		inputMaster		=	1,				//		( inputMaster << baSIMS )			Serial audio input master mode
	baSISF				=	6,				//	bit addressed field:					ISCLK frequency for Master Mode
		isclk64fsi		=	0,				//		( isclk64fsi << baSISF )		¥	64 * Fsi (default)
		isclk128fsi		=	1,				//		( isclk128fsi << baSISF )			128 * Fsi
	baSIRES				=	4,				//	bit addressed field:					Resolution of the input data, for right justified formats
		input24bit		=	0,				//		( input24bit << baSIRES )		¥	24 bit resolution (default)
		input20bit		=	1,				//		( input20bit << baSIRES )			20 bit resolution
		input16bit		=	2,				//		( input16bit << baSIRES )			16 bit resolution
	baSIJUST			=	3,				//	bit addressed field:					Justification of SDIN data relative to ILRCK
		siLeftJust		=	0,				//		( siLeftJust << baSIJUST )		¥	Left justified (default)
		siRightJust		=	1,				//		( siRightJust << baSIJUST )			Right justified
	baSIDEL				=	2,				//	bit addressed field:					Delay of SDIN data relative to ILRCK, for left justified data formats (i.e. I2S)
		siMsb1stCk		=	0,				//		( siMsb1stCk << baSIDEL )			MSB of SDIN data occurs at first ISCLK after ILRCK (default)
		siMsb2ndCk		=	1,				//		( siMsb2ndCk << baSIDEL )		¥	MSB of SDIN data occurs at second ISCLK after ILRCK
	baSISPOL			=	1,				//	bit addressed field:					ISCLK clock polarity
		siRising		=	0,				//		( siRising << baSISPOL )		¥	SDIN sampled on rising edge of ISCLK (default)
		siFalling		=	1,				//		( siFalling << baSISPOL )			SDIN sampled on falling edge of ISCLK
	baSILRPOL			=	0,				//	bit addressed field:					ILRCK clock polarity
		siLeftILRCK		=	0,				//		( siLeftILRCK << baSILRPOL )		SDIN data is for left channel when ILRCK is high (default)
		siRightILRCK	=	1				//		( siRightILRCK << baSILRPOL )	¥	SDIN data is for right channel when ILRCK is high
};

enum CS8420_SERIAL_AUDIO_OUTPUT_FORMAT {
	map_SERIAL_OUTPUT_FMT	=	6,			//	Memory Address Pointer:					Serial Audio Output Port Data Format
	baSOMS				=	7,				//	bit addressed field:					Serial Output Master / Slave Mode Selector
		somsSlave		=	0,				//		( somsSlave << baSOMS )			¥	Serial audio output port is in slave mode (default)
		somsMaster		=	1,				//		( somsMaster << baSOMS )			Serial audio output port is in master mode
	baSOSF				=	6,				//	bit addressed field:					OSCLK frequency for master mode
		osclk64Fso		=	0,				//		( osclk64Fso << baSOSF )		¥	64 * Fso (default)
		osclk128Fso		=	1,				//		( osclk128Fso << baSOSF )			128 * Fso
	baSORES				=	4,				//	bit addressed field:					Resolution of the output data on SDOUT and the AES3 output
		out24bit		=	0,				//		( out24bit << baSORES )			¥	24 bit resolution (default)
		out20bit		=	1,				//		( out20bit << baSORES )				20 bit resolution
		out16bit		=	2,				//		( out16bit << baSORES )				16 bit resolution
		outAES3			=	3,				//		( outAES3 << baSORES )				Direct copy of the received NRZ data from the AES3 receiver
	baSOJUST			=	3,				//	bit addressed field:					Justification of the SDOUT data relative tot he OLRCK
		soLeftJust		=	0,				//		( soLeftJust << baSOJUST )		¥	Left justified (default)
		soRightJust		=	1,				//		( soRightJust << baSOJUST )			Right justified
	baSODEL				=	2,				//	bit addressed field:					Delay of SDOUT data relative to OLRCK for left justified (i.e. I2S) data formats
		soMsb1stCk		=	0,				//		( soMsb1stCk << baSODEL )			MSB of SDOUT data occurs on the first OSCLK after OLRCK edge (default)
		soMsb2ndCk		=	1,				//		( soMsb2ndCk << baSODEL )		¥	MSB of SDOUT data occurs on the second OSCLK after OLRCK edge
	baSOSPOL			=	1,				//	bit addressed field:					OSCLK clock polarity
		sdoutFalling	=	0,				//		( sdoutFalling << baSOSPOL )	¥	SDOUT transitions occur on falling edges of OSCLK (default)
		sdoutRising		=	1,				//		( sdoutRising << baSOSPOL )			SDOUT transitions occur on rising edges of OSCLK
	baSOLRPOL			=	0,				//	bit addressed field:					OLRCK clock polarity
		soLeftOLRCK		=	0,				//		( soLeftOLRCK << baSOLRPOL )		SDOUT data is for the left channel when OLRCK is high (default)
		soRightOLRCK	=	1				//		( soRightOLRCK << baSOLRPOL )	¥	SDOUT data is for the right channel when OLRCK is high
};

enum CS8420_IRQ_STATUS1 {
	map_IRQ1_STATUS		=	7,				//	Memory Address Pointer:					Interrupt 1 Register Status (all bits active high)
	baTSLIP				=	7,				//	bit addressed field:					AES3 transmitter souce data slip interrupt
	baOSLIP				=	6,				//	bit addressed field:					Serial audio output port data slip interrupt
	baSRE				=	5,				//	bit addressed field:					Sample Rate range exceeded indicator (( 3 > ( Fsi/Fso )) | ( 3 > ( Fso/Fsi )))
	baOVRGL				=	4,				//	bit addressed field:					Over range indicator for left (A) channel SRC output
	baOVRGR				=	3,				//	bit addressed field:					Over range indicator for Right (B) channel SRC output
	baDETC				=	2,				//	bit addressed field:					D to E C-buffer transfer interrupt
	baEFTC				=	1,				//	bit addressed field:					E to F C-buffer transfer interrupt
	baRERR				=	0				//	bit addressed field:					Receiver error -> requires read of receiver error register
};

enum CS8420_IRQ_STATUS2 {
	map_IRQ2_STATUS		=	8,				//	Memory Address Pointer:					Interrupt 2 Register Status (all bits active high)
	baVFIFO				=	5,				//	bit addressed field:					Varispeed FIFO overflow indicator (SRC data buffer overflow)
	baREUNLOCK			=	4,				//	bit addressed field:					Sample rate converter unlock
	baDETU				=	3,				//	bit addressed field:					D to E U-buffer transfer error
	baEFTU				=	2,				//	bit addressed field:					E to F U-buffer transfer error
	baQCH				=	1,				//	bit addressed field:					A new block of Q-subcode data is available for reading
	baUOVW				=	0				//	bit addressed field:					U-bit FIFO overwrite occurs on an overwrite in the U-bit FIFO
};

enum CS8420_IRQ_MASK1 {
	map_IRQ1_MASK		=	9,				//	Memory Address Pointer:					Interrupt 1 Register Mask (set bit field to '0' to mask, '1' to enable interrupt)
	baTSLIPM			=	7,				//	bit addressed field:					'0' to mask, '1' to enable AES3 tx source data slip interrupt
	baOSLIPM			=	6,				//	bit addressed field:					'0' to mask, '1' to enable Serial audio output port data slip interrupt
	baSREM				=	5,				//	bit addressed field:					'0' to mask, '1' to enable Sample Rate range exceeded indicator
	baOVRGLM			=	4,				//	bit addressed field:					'0' to mask, '1' to enable Over range indicator for left (A) channel SRC output
	baOVRGRM			=	3,				//	bit addressed field:					'0' to mask, '1' to enable Over range indicator for right (A) channel SRC output
	baDETCM				=	2,				//	bit addressed field:					'0' to mask, '1' to enable D to E C-buffer transfer interrupt
	baEFTCM				=	1,				//	bit addressed field:					'0' to mask, '1' to enable E to F C-buffer transfer interrupt
	baRERRM				=	0				//	bit addressed field:					'0' to mask, '1' to enable Receiver error -> requires read of receiver error register
};

enum CS8420_IRQ_MODE1_MSB {
	map_IRQ1_MODE_MSB	=	10,				//	Memory Address Pointer:					Interrupt Register 1 Mode Register MSB
	baTSLIP1			=	7,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baOSLIP1			=	6,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baSRE1				=	5,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baOVRGL1			=	4,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baOVRGR1			=	3,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baDETC1				=	2,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baEFTC1				=	1,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baRERR1				=	0				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8420_IRQ_MODE1_LSB {
	map_IRQ1_MODE_LSB	=	11,				//	Memory Address Pointer:					Interrupt Register 1 Mode Register LSB
	baTSLIP0			=	7,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baOSLIP0			=	6,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baSRE0				=	5,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baOVRGL0			=	4,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baOVRGR0			=	3,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baDETC0				=	2,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baEFTC0				=	1,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baRERR0				=	0				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8420_IRQ_MASK2 {
	map_IRQ2_MASK		=	12,				//	Memory Address Pointer:					Interrupt 2 Register Mask (set bit field to '0' to mask, '1' to enable interrupt)
	baVFIFOM			=	5,				//	bit addressed field:					'0' to mask, '1' to enable Varispeed FIFO overflow indicator (SRC data buffer overflow)
	baREUNLOCKM			=	4,				//	bit addressed field:					'0' to mask, '1' to enable Sample rate converter unlock
	baDETUM				=	3,				//	bit addressed field:					'0' to mask, '1' to enable D to E U-buffer transfer error
	baEFTUM				=	2,				//	bit addressed field:					'0' to mask, '1' to enable E to F U-buffer transfer error
	baQCHM				=	1,				//	bit addressed field:					'0' to mask, '1' to enable A new block of Q-subcode data is available for reading
	baUOVWM				=	0				//	bit addressed field:					'0' to mask, '1' to enable U-bit FIFO overwrite occurs on an overwrite in the U-bit FIFO
};

enum CS8420_IRQ_MODE2_MSB {
	map_IRQ2_MODE_MSB	=	13,				//	Memory Address Pointer:					Interrupt Register 2 Mode Register MSB
	baVFIFOM1			=	5,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baREUNLOCKM1		=	4,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baDETUM1			=	3,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baEFTUM1			=	2,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baQCHM1				=	1,				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baUOVWM1			=	0				//	bit addressed field:					MSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8420_IRQ_MODE2_LSB {
	map_IRQ2_MODE_LSB	=	14,				//	Memory Address Pointer:					Interrupt Register 2 Mode Register LSB
	baVFIFOM0			=	5,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baREUNLOCKM0		=	4,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baDETUM0			=	3,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baEFTUM0			=	2,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baQCHM0				=	1,				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
	baUOVWM0			=	0				//	bit addressed field:					LSB where:	00 = Rising edge, 01 = Falling edge, 10 = level
};

enum CS8420_RX_CH_STATUS {
	map_RX_CH_STATUS	=	15,				//	Memory Address Pointer:					Receiver channel status
	baAUX				=	4,				//	bit addressed field:					Indicates the width of incoming auxiliary data field derived from AES3 channel status bits (0 to 8)
	baPPRO				=	3,				//	bit addressed field:					Channel status block format indicator
		consumer		=	0,				//		( consumer << baPPRO )				Received data is in consumer format
		professional	=	1,				//		( professional << baPPRO )			Received data is in consumer format
	baAUDIO				=	2,				//	bit addressed field:					Audio indicator
		pcmAudio		=	0,				//		( pcmAudio << baAUDIO )				Received data is linearly coded PCM data
		encodedAudio	=	1,				//		( encodedAudio << baAUDIO )			Received data is not linearly coded PCM data
	baCOPY				=	1,				//	bit addressed field:					SCMS copyright indicator
		copyrighted		=	0,				//		( copyrighted << baCOPY )			Copyright asserted
		publicDomain	=	1,				//		( publicDomain << baCOPY )			Copyright not asserted
	baORIG				=	0,				//	bit addressed field:					SCMS generation indicator as decoded from category code in 'L' bit
		notOriginal		=	0,				//		( notOriginal << baORIG )			Received data is 1st generation or higher copy
		original		=	1				//		( original << baORIG )				Received data is original
};

enum CS8420_RX_ERROR {
	map_RX_ERROR		=	16,				//	Memory Address Pointer:					Receiver Error
	baQCRC				=	6,				//	bit addressed field:					QCRC Q-subcode data CRC error updated on Q-subcode block boundaries
		qcrcNoErr		=	0,				//		( qcrcNoErr << baQCRC )				No Error
		qcrcError		=	1,				//		( qcrcError << baQCRC )				Error occurred
	baCCRC				=	5,				//	bit addressed field:					CCRC channel status block CRC error updated on channel status block boundaries
		ccrcNoErr		=	0,				//		( ccrcNoErr << baCCRC )				No Error - valid only in PROFESSIONAL mode ONLY
		ccrcError		=	1,				//		( ccrcError << baCCRC )				Error occurred - valid only in PROFESSIONAL mode ONLY
	baUNLOCK			=	4,				//	bit addressed field:					PLL lock status bit updated on channel status block boundaries
		pllLocked		=	0,				//		( pllLocked << baQCRC )				phase locked loop locked
		pllUnlocked		=	1,				//		( pllUnlocked << baQCRC )			phase locked loop not locked
	baVALID				=	3,				//	bit addressed field:					Received AES3 validity bit status updated on sub-frame boundaries
		dataValid		=	0,				//		( dataValid << baVALID )			Data is valid and is normally linear coded PCM audio
		dataInvalid		=	1,				//		( dataInvalid << baVALID )			Data is invalid OR may be valid compressed (non-PCM) audio
	baCONF				=	2,				//	bit addressed field:					Confidence bit updated on sub-frame boundaries
		confNoErr		=	0,				//		( confNoErr << baCONF )				No Error
		confError		=	1,				//		( confError << baCONF )				Confidence error indicates received data eye opening is less than 
	baBIP				=	1,				//	bit addressed field:					Bi-phase error updated on sub-frame boundaries
		bipNoErr		=	0,				//		( bipNoErr << baBIP )				No error
		bipError		=	1,				//		( bipError << baBIP )				Error in bi-phase coding
	baPARITY			=	0,				//	bit addressed field:					Parity error updated on sub-frame boundaries
		parNoErr		=	0,				//		( parNoErr << baPARITY )			No error
		parError		=	1				//		( parError << baPARITY )			Parity error
};

enum CS8420_RX_ERROR_MASK {
	map_RX_ERROR_MASK	=	17,				//	Memory Address Pointer:					Receiver Error Mask
	baQCRCM				=	6,				//	bit addressed field:				1	'0' to mask, '1' to enable QCRC Q-subcode data CRC error updated on Q-subcode block boundaries
	baCCRCM				=	5,				//	bit addressed field:				1	'0' to mask, '1' to enable CCRC channel status block CRC error updated on channel status block boundaries
	baUNLOCKM			=	4,				//	bit addressed field:				1	'0' to mask, '1' to enable PLL lock status bit updated on channel status block boundaries
	baVALIDM			=	3,				//	bit addressed field:				1	'0' to mask, '1' to enable Received AES3 validity bit status updated on sub-frame boundaries
	baCONFM				=	2,				//	bit addressed field:				1	'0' to mask, '1' to enable Confidence bit updated on sub-frame boundaries
	baBIPM				=	1,				//	bit addressed field:				1	'0' to mask, '1' to enable Bi-phase error updated on sub-frame boundaries
	baPARITYM			=	0,				//	bit addressed field:				1	'0' to mask, '1' to enable Parity error updated on sub-frame boundaries
		maskRxError		=	0,				//		( maskRxError >> ??? )				Mask the error interrupt
		enRxError		=	1				//		( enRxError >> ??? )				Enable the error interrupt
};

enum CS8420_CH_STATUS_DATA_BUF_CTRL {
	map_CH_STATUS_DATA_BUF_CTRL	=	18,		//	Memory Address Pointer:					Channel Status Data Buffer Control
	baBSEL				=	5,				//	bit addressed field:					'0' to mask, '1' to enable 
		bselChStat		=	0,				//		( bselChStat << baBSEL )			Data buffer address space contains channel status data (default)
		bselUserStat	=	1,				//		( bselUserStat << baBSEL )			Data buffer address space contains user status data
	baCBMR				=	4,				//	bit addressed field:					Control the first 5 bytes of the channel status "E" buffer
		first5DtoE		=	0,				//		( first5DtoE << baCBMR )			Allow D to E buffer transfers to overwrite the first 5 bytes of channel status data (default)
		first5ChStat	=	1,				//		( first5ChStat << baCBMR )			Prevent D to E buffer transfers from overwriting the first 5 bytes of channel status data
	baDETCI				=	3,				//	bit addressed field:					D to E C-data buffer transfer inhibit bit
		enCDataDtoE		=	0,				//		( enCDataDtoE << baDETCI )			Allow C-data D to E buffer transfers (default)
		disCDataDtoE	=	1,				//		( disCDataDtoE << baDETCI )			Inhibit C-data D to E buffer transfers
	baEFTCI				=	2,				//	bit addressed field:					E to F C-data buffer transfer inhibit bit
		enCDataEtoF		=	0,				//		( enCDataEtoF << baEFTCI )			Allow C-data E to E buffer transfers (default)
		disCDataEtoF	=	1,				//		( disCDataEtoF << baEFTCI )			Inhibit C-data E to E buffer transfers
	baCAM				=	1,				//	bit addressed field:					C-data buffer control port access mode
		onebyte			=	0,				//		( onebyte << baCAM )				One byte mode (default)
		twoByte			=	1,				//		( twoByte << baCAM )				Two byte mode
	baCHS				=	0,				//	bit addressed field:					Channel select bit
		channelA		=	0,				//		( channelA << baCHS )				Channel A info at *EMPH pin and RX channel status register (default)
		channelB		=	1				//		( channelB << baCHS )				Channel B info at *EMPH pin and RX channel status register
};

enum CS8420_USER_DATA_BUF_CTRL {
	map_USER_DATA_BUF_CTRL	=	19,			//	Memory Address Pointer:					User Data Buffer control
	baUD				=	4,				//	bit addressed field:					User data pin data direction specifier
		udataIn			=	0,				//		( udataIn << baUD )					U-pin is an input (default)
		udataOut		=	1,				//		( udataOut << baUD )				U-pin is an output
	baUBM				=	2,				//	bit addressed field:					AES3 U bit manager operating mode
		ubmTX0			=	0,				//		( ubmTX0 << baUBM )					Transmit all zeros mode (default)
		ubmBlock		=	1,				//		( ubmBlock << baUBM )				Block mode
		ubmIECmode4		=	3,				//		( ubmIECmode4 << baUBM )			IEC consumer mode 4
	baDETUI				=	1,				//	bit addressed field:					D to E U-data buffer transfer inhibit bit (valid in block mode only)
		enUDataDtoE		=	0,				//		( enUDataDtoE << baDETUI )			Allow U-data D to E buffer transfers (default)
		disUDataDtoE	=	1,				//		( disUDataDtoE << baDETUI )			Inhibit U-data D to E buffer transfers
	baEFTUI				=	0,				//	bit addressed field:					E to F U-data buffer transfer inhibit bit (valid in block mode only)
		enUDataEtoF		=	0,				//		( enUDataEtoF << baEFTUI )			Allow U-data E to F buffer transfers (default)
		disUDataEtoF	=	1				//		( disUDataEtoF << baEFTUI )			Inhibit U-data E to F buffer transfers
};

enum CS8420_Q_CHANNEL_SUBCODE {
	map_Q_CHANNEL_SUBCODE_AC	=	20,		//	Memory Address Pointer:					Q-Channel subcode byte 0 - Address & Control
	baQChAddress		=	4,				//	bit address field:						Address
	baQChControl		=	0,				//	bit addressed field:					Control
	map_Q_CHANNEL_SUBCODE_TRK	=	21,		//	Memory Address Pointer:					Q-Channel subcode byte 1
	map_Q_CHANNEL_SUBCODE_INDEX	=	22,		//	Memory Address Pointer:					Q-Channel subcode byte 2
	map_Q_CHANNEL_SUBCODE_MIN	=	23,		//	Memory Address Pointer:					Q-Channel subcode byte 3
	map_Q_CHANNEL_SUBCODE_SEC	=	24,		//	Memory Address Pointer:					Q-Channel subcode byte 4
	map_Q_CHANNEL_SUBCODE_FRAME	=	25,		//	Memory Address Pointer:					Q-Channel subcode byte 5
	map_Q_CHANNEL_SUBCODE_ZERO	=	26,		//	Memory Address Pointer:					Q-Channel subcode byte 6
	map_Q_CHANNEL_SUBCODE_ABS_MIN	= 27,	//	Memory Address Pointer:					Q-Channel subcode byte 7
	map_Q_CHANNEL_SUBCODE_ABS_SEC	= 28,	//	Memory Address Pointer:					Q-Channel subcode byte 8
	map_Q_CHANNEL_SUBCODE_ABS_FRAME	= 29	//	Memory Address Pointer:					Q-Channel subcode byte 9
};

enum CS8420_SAMPLE_RATE_RATIO {
	map_SAMPLE_RATE_RATIO	=	30,			//	Memory Address Pointer:					Sample Rate Ratio
	baInteger			=	6,				//	bit addressed field:					Integer portion of Fso / Fsi when PLL and SRC have reached lock and SRC is in use
	baFraction			=	0				//	bit addressed field:					Fraction portion of Fso / Fsi when PLL and SRC have reached lock and SRC is in use
};

enum CS8420_C_OR_U_BIT_BUFFER {				//	NOTE:  The channel / user status buffer should be read as a single 24 byte transaction with auto-increment enabled!
	map_BUFFER_0		=	32,			//	Memory Address Pointer:						Channel Status or User Status Buffer  0	(see Channel Status Block Structure below)
	map_BUFFER_1		=	33,			//	Memory Address Pointer:						Channel Status or User Status Buffer  1	(see Channel Status Block Structure below)
	map_BUFFER_2		=	34,			//	Memory Address Pointer:						Channel Status or User Status Buffer  2	(see Channel Status Block Structure below)
	map_BUFFER_3		=	35,			//	Memory Address Pointer:						Channel Status or User Status Buffer  3	(see Channel Status Block Structure below)
	map_BUFFER_4		=	36,			//	Memory Address Pointer:						Channel Status or User Status Buffer  4	(see Channel Status Block Structure below)
	map_BUFFER_5		=	37,			//	Memory Address Pointer:						Channel Status or User Status Buffer  5	(see Channel Status Block Structure below)
	map_BUFFER_6		=	38,			//	Memory Address Pointer:						Channel Status or User Status Buffer  6	(see Channel Status Block Structure below)
	map_BUFFER_7		=	39,			//	Memory Address Pointer:						Channel Status or User Status Buffer  7	(see Channel Status Block Structure below)
	map_BUFFER_8		=	40,			//	Memory Address Pointer:						Channel Status or User Status Buffer  8	(see Channel Status Block Structure below)
	map_BUFFER_9		=	41,			//	Memory Address Pointer:						Channel Status or User Status Buffer  9	(see Channel Status Block Structure below)
	map_BUFFER_10		=	42,			//	Memory Address Pointer:						Channel Status or User Status Buffer 10	(see Channel Status Block Structure below)
	map_BUFFER_11		=	43,			//	Memory Address Pointer:						Channel Status or User Status Buffer 11	(see Channel Status Block Structure below)
	map_BUFFER_12		=	44,			//	Memory Address Pointer:						Channel Status or User Status Buffer 12	(see Channel Status Block Structure below)
	map_BUFFER_13		=	45,			//	Memory Address Pointer:						Channel Status or User Status Buffer 13	(see Channel Status Block Structure below)
	map_BUFFER_14		=	46,			//	Memory Address Pointer:						Channel Status or User Status Buffer 14	(see Channel Status Block Structure below)
	map_BUFFER_15		=	47,			//	Memory Address Pointer:						Channel Status or User Status Buffer 15	(see Channel Status Block Structure below)
	map_BUFFER_16		=	48,			//	Memory Address Pointer:						Channel Status or User Status Buffer 16	(see Channel Status Block Structure below)
	map_BUFFER_17		=	49,			//	Memory Address Pointer:						Channel Status or User Status Buffer 17	(see Channel Status Block Structure below)
	map_BUFFER_18		=	50,			//	Memory Address Pointer:						Channel Status or User Status Buffer 18	(see Channel Status Block Structure below)
	map_BUFFER_19		=	51,			//	Memory Address Pointer:						Channel Status or User Status Buffer 19	(see Channel Status Block Structure below)
	map_BUFFER_20		=	52,			//	Memory Address Pointer:						Channel Status or User Status Buffer 20	(see Channel Status Block Structure below)
	map_BUFFER_21		=	53,			//	Memory Address Pointer:						Channel Status or User Status Buffer 21	(see Channel Status Block Structure below)
	map_BUFFER_22		=	54,			//	Memory Address Pointer:						Channel Status or User Status Buffer 22	(see Channel Status Block Structure below)
	map_BUFFER_23		=	55			//	Memory Address Pointer:						Channel Status or User Status Buffer 23	(see Channel Status Block Structure below)
};

enum CS8420_ID_VERSION {
	map_ID_VERSION		=	0x7F,			//	Memory Address Pointer:					I.D. and Version Register
	baID				=	4,				//	bit addressed field:					ID code for CS8420 (%0001)
	cs8420_id			=	1,				//	USE:	( cs8420_id << baID )
	cs8416_id			=	2,				//	USE:	( cs8416_id << baID )
	cs8406_id			=	14,				//	USE:	( cs8406_id << baID )
	baVersion			=	0,				//	bit addressed field:					Version of CS8420
	revisionB			=	1,				//												Revision B is coded as %0001
	revisionC			=	3				//												Revision C is coded as %0011
};

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

enum gpio{
		intEdgeSEL				=	7,		//	bit address:	R/W Enable Dual Edge
		positiveEdge			=	0,		//		0 = positive edge detect for ExtInt interrupt sources (default)
		dualEdge				=	1,		//		1 = enable both edges
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

#define	kMISC_CNTRL_1_INIT_8420				( ( swclkOMCK << baSWCLK ) | ( vbitValid << baVSET ) | ( normalSAO << baMuteSAO ) | ( muteAES3 << baMuteAES ) | ( enableDITH << baDITH ) | ( activeLowINT << baINT ) | ( outputTCBLD << baTCBLD ) )
#define	kMISC_CNTRL_1_INIT_8406				( ( swclkRMCK << baSWCLK ) | ( vbitValid << baVSET ) | ( normalSAO << baMuteSAO ) | ( muteAES3 << baMuteAES ) | ( enableDITH << baDITH ) | ( activeLowINT << baINT ) | ( outputTCBLD << baTCBLD ) )
#define	kMISC_CNTRL_2_INIT					( ( notTruncated << baTRUNC ) | ( replaceMute << baHOLD ) | ( fsi256 << baRMCKF ) | ( mmrStereo << baMMR ) | ( mmtStereo << baMMT ) | ( discrete << baMMTCS ) | ( useLeft << baMMTLR ) )
#define	kDATA_FLOW_CTRL_INIT				( ( enAutoMute << baAMLL ) | ( aes3TXNormal << baTXOFF ) | ( normalBP << baAESBP ) | ( txdSAI << baTXD ) | ( spdSrcOut << baSPD ) | ( srcdAES3 << baSRCD ) )
#define	kCLOCK_SOURCE_CTRL_INIT_STOP		( ( runSTOP << baRUN ) | ( omck256fso << baCLK ) | ( outcOmckXbaCLK << baOUTC ) | ( incRecIC << baINC ) | ( rxd256fsiAES3 << baRXD ) )
#define	kCLOCK_SOURCE_CTRL_INIT				( ( runNORMAL << baRUN ) | ( omck256fso << baCLK ) | ( outcOmckXbaCLK << baOUTC ) | ( incRecIC << baINC ) | ( rxd256fsiAES3 << baRXD ) )
#define	kSERIAL_AUDIO_INPUT_FORMAT_INIT		( ( inputSlave << baSIMS ) | ( isclk64fsi << baSISF ) | ( input24bit << baSIRES ) | ( siLeftJust << baSIJUST ) | ( siMsb2ndCk << baSIDEL ) | ( siRising << baSISPOL ) | ( siRightILRCK << baSILRPOL ) )
#define	kSERIAL_AUDIO_OUTPUT_FORMAT_INIT	( ( somsSlave << baSOMS ) | ( osclk64Fso << baSOSF ) | ( out24bit << baSORES ) | ( soLeftJust << baSOJUST ) | ( soMsb2ndCk << baSODEL ) | ( sdoutFalling << baSOSPOL ) | ( soRightOLRCK << baSOLRPOL ) )
#define	kRX_ERROR_MASK_DISABLE_RERR			( ( maskRxError << baQCRCM ) | ( maskRxError << baCCRCM ) | ( maskRxError << baUNLOCKM ) | ( maskRxError << baVALIDM ) | ( enRxError << baCONFM ) | ( enRxError << baBIPM ) | ( maskRxError << baPARITYM ) )
#define	kRX_ERROR_MASK_ENABLE_RERR			( ( maskRxError << baQCRCM ) | ( maskRxError << baCCRCM ) | ( enRxError << baUNLOCKM ) | ( maskRxError << baVALIDM ) | ( enRxError << baCONFM ) | ( enRxError << baBIPM ) | ( maskRxError << baPARITYM ) )

#define	kMASK_ALL								0x00
#define	kMASK_NONE								0xFF
#define	kCS84XX_ID_MASK							0xF0
#define kCS84XX_BIT_MASK						0x01
#define kCS84XX_TWO_BIT_MASK					0x03
#define	kMISC_CNTRL_1_INIT_8406_MASK			( ( vbitInvalid << baVSET ) | ( muteAES3 << baMuteAES ) | ( activeLowODINT << baINT ) | ( activeLowINT << baINT ) | ( outputTCBLD << baTCBLD ) )
#define	kMISC_CNTRL_2_INIT_8406_MASK			( ( mmtUseMMTLR << baMMT ) | ( useMMTLR << baMMTCS ) | ( useRight << baMMTLR ) )
#define	kDATA_FLOW_CTR_8406_MASK				( ( aes3TX0v << baTXOFF ) | ( rxpPB << baAESBP ) )
#define	kCLOCK_SOURCE_CTR_8406_MASK				( ( runNORMAL << baRUN ) | ( omck384fso << baCLK ) | ( omck512fs0 << baCLK ) )
#define	kSERIAL_AUDIO_INPUT_FORMAT_8406_MASK	kMASK_NONE
#define	kIRQ1_8406_MASK_8406_MASK				( ( kCS84XX_BIT_MASK << baTSLIP ) | ( kCS84XX_BIT_MASK << baEFTC ) )
#define	kIRQ2_8406_MASK_8406_MASK				( kCS84XX_BIT_MASK << baEFTU )
#define	kCH_STATUS_DATA_BUF_CTRL_8406_MASK		( ( bselUserStat << baBSEL ) | ( disCDataEtoF << baEFTCI ) | ( twoByte << baCAM ) )
#define	kUSER_DATA_BUF_CTRLL_8406_MASK			( ( udataOut << baUD ) | ( ubmIECmode4 << baUBM ) | ( disUDataEtoF << baEFTUI ) )


#endif
