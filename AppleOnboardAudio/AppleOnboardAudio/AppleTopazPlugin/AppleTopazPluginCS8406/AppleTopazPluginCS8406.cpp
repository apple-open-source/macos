/*
 *  AppleTopazPluginCS8406.cpp
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Tue Oct 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AppleTopazPluginCS8406.h"

#define super AppleTopazPlugin

static UInt8	cs8406_regs[] = {   0x01, 0x02, 0x03, 0x04, 0x05, 0x07, 0x08, 0x09, 
									0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x12, 0x13, 0x20,
									0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
									0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
									0x31, 0x32, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 };

OSDefineMetaClassAndStructors ( AppleTopazPluginCS8406, AppleTopazPlugin )

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8406::init ( OSDictionary *properties ) {
	mChanStatusStruct.sampleRate	= 44100;						//  [3666183]   
	mChanStatusStruct.sampleDepth   = 16;							//  [3666183]   
	mChanStatusStruct.nonAudio		= kConsumerMode_audio;			//  [3666183]   
	mChanStatusStruct.consumerMode  = kConsumer;					//  [3666183]   
	return super::init (properties);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8406::start ( IOService * provider ) {
	return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8406::free ( void ) {
	super::free ();
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8406::preDMAEngineInit ( void ) {
	bool			result = false;
	IOReturn		err;
	
	//	Place device into power down state prior to initialization
	err = CODEC_WriteRegister ( map_CS8406_CLOCK_SOURCE_CTRL, kCS8406_CLOCK_SOURCE_CTRL_INIT_STOP );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8406_MISC_CNTRL_1, kCS8406_MISC_CNTRL_1_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8406_MISC_CNTRL_2, kCS8406_MISC_CNTRL_2_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8406_DATA_FLOW_CTRL, kCS8406_DATA_FLOW_CTRL_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8406_SERIAL_INPUT_FMT, kCS8406_SERIAL_AUDIO_INPUT_FORMAT_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8406_CLOCK_SOURCE_CTRL, kCS8406_CLOCK_SOURCE_CTRL_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8406_USER_DATA_BUF_CTRL, bvCS8406_ubmBlock << baCS8406_UBM );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	result = true;
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::initCodecRegisterCache ( void ) { 
	IOReturn		result = kIOReturnSuccess;
	IOReturn		err;
	
	for ( UInt32 cs8406_regs_table_index = 0; cs8406_regs_table_index <= sizeof ( cs8406_regs ); cs8406_regs_table_index++ ) {
		err = CODEC_ReadRegister ( cs8406_regs[cs8406_regs_table_index], NULL, 1 );												//	read I2C register into cache only
		if ( kIOReturnSuccess != err && kIOReturnSuccess == result ) {
			result = err;
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::setMute ( bool muteState ) {
	UInt8			data;
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3, "+ AppleTopazPluginCS8406::setMute (%d)", muteState);

	data = kCS8406_MISC_CNTRL_1_INIT;
	data &= ~( 1 << baCS8406_MuteAES );
	data |= muteState ? ( bvCS8406_muteAES3 << baCS8406_MuteAES ) : ( bvCS8406_normalAES3 << baCS8406_MuteAES ) ;
	result = CODEC_WriteRegister ( map_CS8406_MISC_CNTRL_1, data );
	FailIf ( kIOReturnSuccess != result, Exit );
	mMuteState = muteState;
Exit:
	debugIOLog (3, "- AppleTopazPluginCS8406::setMute (%d) returns %X", muteState, result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::performDeviceSleep ( void ) {
	IOReturn			result;
	
	debugIOLog (3, "+ AppleTopazPluginCS8406::performDeviceSleep()");

	mShadowRegs[map_CS8406_DATA_FLOW_CTRL] &= ~( kCS84XX_BIT_MASK << baCS8406_TXOFF );
	mShadowRegs[map_CS8406_DATA_FLOW_CTRL] |= ( bvCS8406_aes3TX0v << baCS8406_TXOFF );
	result = CODEC_WriteRegister ( map_CS8406_DATA_FLOW_CTRL, mShadowRegs[map_CS8406_DATA_FLOW_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_BIT_MASK << baCS8406_RUN );
	mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL] |= ( bvCS8406_runSTOP << baCS8406_RUN );
	result = CODEC_WriteRegister ( map_CS8406_CLOCK_SOURCE_CTRL, mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

Exit:
	debugIOLog (3, "- AppleTopazPluginCS8406::performDeviceSleep()");
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::performDeviceWake ( void ) {
	IOReturn			result;
	
	debugIOLog (3,  "+ AppleTopazPluginCS8406::performDeviceWake()" );

	flushControlRegisters ();
	
	mShadowRegs[map_CS8406_DATA_FLOW_CTRL] &= ~( kCS84XX_BIT_MASK << baCS8406_TXOFF );
	mShadowRegs[map_CS8406_DATA_FLOW_CTRL] |= ( bvCS8406_aes3TXNormal << baCS8406_TXOFF );
	result = CODEC_WriteRegister ( map_CS8406_DATA_FLOW_CTRL, mShadowRegs[map_CS8406_DATA_FLOW_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

	mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_BIT_MASK << baCS8406_RUN );
	mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL] |= ( bvCS8406_runNORMAL << baCS8406_RUN );
	result = CODEC_WriteRegister ( map_CS8406_CLOCK_SOURCE_CTRL, mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

	setChannelStatus ( &mChanStatusStruct );		//  [3666183]   Flush channel status buffer

Exit:
	debugIOLog (3,  "- AppleTopazPluginCS8406::performDeviceWake()" );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::setChannelStatus ( ChanStatusStructPtr channelStatus ) {
	UInt8				data;
	IOReturn			result = kIOReturnError;

	FailIf ( NULL == channelStatus, Exit );
	
	if ( 0 != channelStatus->sampleRate ) {
		mChanStatusStruct.sampleRate	= channelStatus->sampleRate;		//  [3666183]   
	}
	if ( 0 != channelStatus->sampleDepth ) {
		mChanStatusStruct.sampleDepth   = channelStatus->sampleDepth;		//  [3666183]   
	}
	mChanStatusStruct.nonAudio = channelStatus->nonAudio;					//  [3666183]   
	mChanStatusStruct.consumerMode = channelStatus->consumerMode;			//  [3666183]   
	
	//	Assumes consumer mode
	data = ( ( kCopyPermited << ( 7 - kBACopyright ) ) | ( kConsumer << ( 7 -  kBAProConsumer ) ) );
	if ( mChanStatusStruct.nonAudio ) {
		data |= ( kConsumerMode_nonAudio << ( 7 - kBANonAudio ) );								//	consumer mode encoded
	} else {
		data |= ( kConsumerMode_audio << ( 7 - kBANonAudio ) );									//	consumer mode linear PCM
	}
	result = CODEC_WriteRegister ( map_CS8406_BUFFER_0, data );									//  [0É7]   consumer/format/copyright/pre-emphasis/
	FailIf ( kIOReturnSuccess != result, Exit );
	
	if ( mChanStatusStruct.nonAudio ) {
		result = CODEC_WriteRegister ( map_CS8406_BUFFER_1, kIEC60958_CategoryCode_DVD );		//	[8É15]  category code is not valid
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_CS8406_BUFFER_2, 0 );								//	[16É23] source & channel are not valid
		FailIf ( kIOReturnSuccess != result, Exit );
			
		result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_NotIndicated );	//	[24É31] sample frequency not indicated
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_CS8406_BUFFER_4, cWordLength_20Max_16bits );			//	[32É39] word length & original sample frequency
		FailIf ( kIOReturnSuccess != result, Exit );
	} else {
		result = CODEC_WriteRegister ( map_CS8406_BUFFER_1, kIEC60958_CategoryCode_CD );		//	[8É15]  category code is CD
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_CS8406_BUFFER_2, 0 );								//	[16É23] source & channel are not specified
		FailIf ( kIOReturnSuccess != result, Exit );
			
		switch ( mChanStatusStruct.sampleRate ) {												//	[24É31] sample frequency
			case 24000:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_24Khz );			break;
			case 32000:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_32Khz );			break;
			case 44100:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_44Khz );			break;
			case 48000:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_48Khz );			break;
			case 88200:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_88Khz );			break;
			case 96000:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_96Khz );			break;
			case 176400:	result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_176Khz );			break;
			case 192000:	result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_192Khz );			break;
			default:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_3, cSampleFrequency_44Khz );			break;
		}
		FailIf ( kIOReturnSuccess != result, Exit );
		
		switch ( mChanStatusStruct.sampleDepth ) {												//	[32É39] word length & original sample frequency
			case 16:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_4, cWordLength_20Max_16bits );			break;
			case 24:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_4, cWordLength_24Max_24bits );			break;
			default:		result = CODEC_WriteRegister ( map_CS8406_BUFFER_4, cWordLength_20Max_16bits );			break;
		}
		FailIf ( kIOReturnSuccess != result, Exit );
	}

Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::breakClockSelect ( UInt32 clockSource ) {
	UInt8				data;
	IOReturn			result;

	debugIOLog (7,  "+ AppleTopazPluginCS8406::breakClockSelect ( %d )", (unsigned int)clockSource );
	
	//	Mute the output port
	data = mShadowRegs[map_CS8406_MISC_CNTRL_1];
	data &= ~( kCS84XX_BIT_MASK << baCS8406_MuteAES );
	data |= ( bvCS8406_muteAES3 << baCS8406_MuteAES );
	result = CODEC_WriteRegister ( map_CS8406_MISC_CNTRL_1, data );
	FailIf ( result != kIOReturnSuccess, Exit );

	//	STOP the codec while switching clocks
	data = mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL];
	data &= ~( 1 << baCS8406_RUN );
	data |= ( bvCS8406_runSTOP << baCS8406_RUN );
	result = CODEC_WriteRegister ( map_CS8406_CLOCK_SOURCE_CTRL, data );

	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			//	Set the input port data format to slave mode
			data = mShadowRegs[map_CS8406_SERIAL_INPUT_FMT];
			data &= ~( kCS84XX_BIT_MASK << baCS8406_SIMS );
			data |= ( bvCS8406_inputSlave << baCS8406_SIMS );
			result = CODEC_WriteRegister ( map_CS8406_SERIAL_INPUT_FMT, data );
			FailIf ( result != kIOReturnSuccess, Exit );
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			//	Set the input port data format to slave mode
			data = mShadowRegs[map_CS8406_SERIAL_INPUT_FMT];
			data &= ~( kCS84XX_BIT_MASK << baCS8406_SIMS );
			data |= ( bvCS8406_inputSlave << baCS8406_SIMS );
			result = CODEC_WriteRegister ( map_CS8406_SERIAL_INPUT_FMT, data );
			FailIf ( result != kIOReturnSuccess, Exit );
			break;
		default:
			result = kIOReturnBadArgument;
			break;
	}
Exit:
	debugIOLog (7,  "- AppleTopazPluginCS8406::breakClockSelect ( %d ) returns %d", (unsigned int)clockSource, (unsigned int)result );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::makeClockSelectPreLock ( UInt32 clockSource ) {
	IOReturn			result;
	UInt8				data;
	
	debugIOLog (7,  "+ AppleTopazPluginCS8406::makeClockSelect ( %d )", (unsigned int)clockSource );

	//	restart the codec after switching clocks
	data = mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL];
	data &= ~( 1 << baCS8406_RUN );
	data |= ( bvCS8406_runNORMAL << baCS8406_RUN );
	result = CODEC_WriteRegister ( map_CS8406_CLOCK_SOURCE_CTRL, data );

	setChannelStatus ( &mChanStatusStruct );		//  [3666183]   Flush channel status buffer
	
	debugIOLog (7,  "- AppleTopazPluginCS8406::makeClockSelect ( %d ) returns %d", (unsigned int)clockSource, (unsigned int)result );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::makeClockSelectPostLock ( UInt32 clockSource ) {
	IOReturn			result;
	UInt8				data;
	
	debugIOLog (7,  "+ AppleTopazPluginCS8406::makeClockSelect ( %d )", (unsigned int)clockSource );

	//	Unmute the coded output
	data = mShadowRegs[map_CS8406_MISC_CNTRL_1];
	data &= ~( kCS84XX_BIT_MASK << baCS8406_MuteAES );
	data |= mMuteState ? ( bvCS8406_muteAES3 << baCS8406_MuteAES ) : ( bvCS8406_normalAES3 << baCS8406_MuteAES ) ;
	result = CODEC_WriteRegister ( map_CS8406_MISC_CNTRL_1, data );
	FailIf ( result != kIOReturnSuccess, Exit );
	
Exit:
	debugIOLog (7,  "- AppleTopazPluginCS8406::makeClockSelect ( %d ) returns %d", (unsigned int)clockSource, (unsigned int)result );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazPluginCS8406::setRunMode ( UInt8 mode ) {
	CODEC_WriteRegister ( map_CS8406_CLOCK_SOURCE_CTRL, mode );
	
	if ( ( mode & ( 1 << baCS8406_RUN ) ) == ( 1 << baCS8406_RUN ) ) {		//  [3666183]   
		setChannelStatus ( &mChanStatusStruct );							//  [3666183]   Flush channel status buffer
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt8 AppleTopazPluginCS8406::setStopMode ( void ) {
	UInt8			data;
	
	//	Stop the device during recovery while preserving the original run state
	data = mShadowRegs[map_CS8406_CLOCK_SOURCE_CTRL];
	CODEC_WriteRegister ( map_CS8406_CLOCK_SOURCE_CTRL, data & ~( 1 << baCS8406_RUN ) );
	return data;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8406::disableReceiverError ( void ) {
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::flushControlRegisters ( void ) {
	IOReturn		result = kIOReturnSuccess;
	
	for ( UInt32 index = 0; index < sizeof ( cs8406_regs ); index++ ) {
		if ( kIOReturnSuccess == CODEC_IsControlRegister ( cs8406_regs[index] ) ) {
			result = CODEC_WriteRegister ( cs8406_regs[index], mShadowRegs[cs8406_regs[index]] );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
	
Exit:
	return result; 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8406::useExternalCLK ( void ) {
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8406::useInternalCLK ( void ) {
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt8	AppleTopazPluginCS8406::CODEC_GetDataMask ( UInt8 regAddr ) {
	UInt8		mask;
	
	mask = kMASK_NONE;
	switch ( regAddr ) {
		case map_CS8406_MISC_CNTRL_1:				mask = kMISC_CNTRL_1_INIT_8406_MASK;			break;
		case map_CS8406_MISC_CNTRL_2:				mask = kCS8406_MISC_CNTRL_2_INIT_MASK;			break;
		case map_CS8406_DATA_FLOW_CTRL:				mask = kCS8406_DATA_FLOW_CTR_MASK;				break;
		case map_CS8406_CLOCK_SOURCE_CTRL:			mask = kCS8406_CLOCK_SOURCE_CTR_MASK;			break;
		case map_CS8406_SERIAL_INPUT_FMT:			mask = kCS8406_SERIAL_AUDIO_INPUT_FORMAT_MASK;	break;
		case map_CS8406_IRQ1_MASK:					mask = kCS8406_IRQ1_8406_MASK_MASK;				break;
		case map_CS8406_IRQ1_MODE_MSB:				mask = kCS8406_IRQ1_8406_MASK_MASK;				break;
		case map_CS8406_IRQ1_MODE_LSB:				mask = kCS8406_IRQ1_8406_MASK_MASK;				break;
		case map_CS8406_IRQ2_MASK:					mask = kCS8406_IRQ2_8406_MASK_MASK;				break;
		case map_CS8406_IRQ2_MODE_MSB:				mask = kCS8406_IRQ2_8406_MASK_MASK;				break;
		case map_CS8406_IRQ2_MODE_LSB:				mask = kCS8406_IRQ2_8406_MASK_MASK;				break;
		case map_CS8406_CH_STATUS_DATA_BUF_CTRL:	mask = kCS8406_CH_STATUS_DATA_BUF_CTRL_MASK;	break;
		case map_CS8406_USER_DATA_BUF_CTRL:			mask = kCS8406_USER_DATA_BUF_CTRLL_MASK;		break;
		default:
			if ( ( map_CS8406_BUFFER_0 <= regAddr ) && ( regAddr <= map_CS8406_BUFFER_23 ) ) {
				mask = kMASK_NONE;
			} else {
				mask = kMASK_ALL;
			}
			break;
	}
	return mask;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::CODEC_GetRegSize ( UInt8 regAddr, UInt32 * codecRegSizePtr ) {
	IOReturn		result;
	
	result = kIOReturnError;
	if ( NULL != codecRegSizePtr ) {
		if ( map_CS8406_BUFFER_0 == regAddr ) {
			* codecRegSizePtr = 24;
			result = kIOReturnSuccess;
		} else if ( CODEC_IsControlRegister( (char)regAddr ) || CODEC_IsStatusRegister( (char)regAddr ) ) {
			* codecRegSizePtr = 1;
			result = kIOReturnSuccess;
		} else {
			* codecRegSizePtr = 0;
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::CODEC_IsControlRegister ( UInt8 regAddr ) {
	IOReturn	result;

	switch ( regAddr ) {
		case map_CS8406_MISC_CNTRL_1:
		case map_CS8406_MISC_CNTRL_2:
		case map_CS8406_DATA_FLOW_CTRL:
		case map_CS8406_CLOCK_SOURCE_CTRL:
		case map_CS8406_SERIAL_INPUT_FMT:
		case map_CS8406_IRQ1_MASK:
		case map_CS8406_IRQ1_MODE_MSB:
		case map_CS8406_IRQ1_MODE_LSB:
		case map_CS8406_IRQ2_MASK:
		case map_CS8406_IRQ2_MODE_MSB:
		case map_CS8406_IRQ2_MODE_LSB:
		case map_CS8406_CH_STATUS_DATA_BUF_CTRL:
		case map_CS8406_USER_DATA_BUF_CTRL:
		case map_CS8406_BUFFER_0:
		case map_CS8406_BUFFER_1:
		case map_CS8406_BUFFER_2:
		case map_CS8406_BUFFER_3:
		case map_CS8406_BUFFER_4:
		case map_CS8406_BUFFER_5:
		case map_CS8406_BUFFER_6:
		case map_CS8406_BUFFER_7:
		case map_CS8406_BUFFER_8:
		case map_CS8406_BUFFER_9:
		case map_CS8406_BUFFER_10:
		case map_CS8406_BUFFER_11:
		case map_CS8406_BUFFER_12:
		case map_CS8406_BUFFER_13:
		case map_CS8406_BUFFER_14:
		case map_CS8406_BUFFER_15:
		case map_CS8406_BUFFER_16:
		case map_CS8406_BUFFER_17:
		case map_CS8406_BUFFER_18:
		case map_CS8406_BUFFER_19:
		case map_CS8406_BUFFER_20:
		case map_CS8406_BUFFER_21:
		case map_CS8406_BUFFER_22:
		case map_CS8406_BUFFER_23:			result = kIOReturnSuccess;			break;
		default:							result = kIOReturnError;			break;
	}

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::CODEC_IsStatusRegister ( UInt8 regAddr ) {
	IOReturn	result;
	
	result = kIOReturnError;
	
	switch ( regAddr ) {
		case map_CS8406_IRQ1_STATUS:
		case map_CS8406_IRQ2_STATUS:
		case map_ID_VERSION:
			result = kIOReturnSuccess;
			break;
		default:
			result = kIOReturnError;
			break;
	}
	
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::getPluginState ( HardwarePluginDescriptorPtr outState ) {
	IOReturn		result = kIOReturnBadArgument;
	
	FailIf ( NULL == outState, Exit );
	outState->hardwarePluginType = kCodec_CS8406;
	outState->registerCacheSize = sizeof ( mShadowRegs );
	for ( UInt32 registerAddress = 0; registerAddress < outState->registerCacheSize; registerAddress++ ) {
		outState->registerCache[registerAddress] = mShadowRegs[registerAddress];
	}
	outState->recoveryRequest = 0;
	result = kIOReturnSuccess;
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8406::setPluginState ( HardwarePluginDescriptorPtr inState ) {
	IOReturn		result = kIOReturnBadArgument;
	
	FailIf ( NULL == inState, Exit );
	FailIf ( sizeof ( mShadowRegs ) != inState->registerCacheSize, Exit );
	result = kIOReturnSuccess;
	for ( UInt32 registerAddress = map_CS8406_MISC_CNTRL_1; ( registerAddress < map_ID_VERSION ) && ( kIOReturnSuccess == result ); registerAddress++ ) {
		if ( inState->registerCache[registerAddress] != mShadowRegs[registerAddress] ) {
			if ( kIOReturnSuccess == CODEC_IsControlRegister ( (UInt8)registerAddress ) ) {
				result = CODEC_WriteRegister ( registerAddress, inState->registerCache[registerAddress] );
			}
		}
	}
Exit:
	return result;
}

