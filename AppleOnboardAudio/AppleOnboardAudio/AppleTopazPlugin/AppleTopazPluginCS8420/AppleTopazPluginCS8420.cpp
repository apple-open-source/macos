/*
 *  AppleTopazPluginCS8420.cpp
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Tue Oct 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AppleTopazPluginCS8420.h"

#define super AppleTopazPlugin

OSDefineMetaClassAndStructors ( AppleTopazPluginCS8420, super )

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8420::init ( OSDictionary *properties ) {
	return super::init (properties);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8420::start ( IOService * provider ) {
	return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8420::free ( void ) {
	super::free ();
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8420::preDMAEngineInit ( void ) {
	bool			result = false;
	IOReturn		err;
	
	//	Place device into power down state prior to initialization
	err = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, kCS8420_CLOCK_SOURCE_CTRL_INIT_STOP );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8420_MISC_CNTRL_1, kCS8420_MISC_CNTRL_1_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8420_MISC_CNTRL_2, kCS8420_MISC_CNTRL_2_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8420_DATA_FLOW_CTRL, kCS8420_DATA_FLOW_CTRL_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8420_SERIAL_INPUT_FMT, kCS8420_SERIAL_AUDIO_INPUT_FORMAT_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8420_SERIAL_OUTPUT_FMT, kCS8420_SERIAL_AUDIO_OUTPUT_FORMAT_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );

	//	Enable receiver error (i.e. RERR) interrupts
	err = CODEC_WriteRegister ( map_CS8420_RX_ERROR_MASK, kCS8420_RX_ERROR_MASK_ENABLE_RERR );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	//	Clear any pending error interrupt
	err = CODEC_ReadRegister ( map_CS8420_RX_ERROR, NULL, 1 );
	FailIf ( kIOReturnSuccess != err, Exit );

	err = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, kCS8420_CLOCK_SOURCE_CTRL_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( map_CS8420_USER_DATA_BUF_CTRL, bvCS8420_ubmBlock << baCS8420_UBM );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	result = true;
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::initCodecRegisterCache ( void ) { 
	IOReturn		result = kIOReturnSuccess;
	IOReturn		err;
	
	for ( UInt32 loopCnt = map_CS8420_MISC_CNTRL_1; loopCnt <= map_CS8420_BUFFER_23; loopCnt++ ) {
		if ( map_CS8420_RX_ERROR != loopCnt && map_CS8420_RESERVED_1F != loopCnt ) {					//	avoid hole in register address space
			err = CODEC_ReadRegister ( loopCnt, NULL, 1 );												//	read I2C register into cache only
			if ( kIOReturnSuccess != err && kIOReturnSuccess == result ) {
				result = err;
			}
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::setMute ( bool muteState ) {
	UInt8			data;
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3, "+ AppleTopazPluginCS8420::setMute (%d)", muteState);

	data = kCS8420_MISC_CNTRL_1_INIT;
	data &= ~( 1 << baCS8420_MuteAES );
	data |= muteState ? ( bvCS8420_muteAES3 << baCS8420_MuteAES ) : ( bvCS8420_normalAES3 << baCS8420_MuteAES ) ;
	result = CODEC_WriteRegister ( map_CS8420_MISC_CNTRL_1, data );
	FailIf ( kIOReturnSuccess != result, Exit );
	mMuteState = muteState;
Exit:
	debugIOLog (3, "- AppleTopazPluginCS8420::setMute (%d) returns %X", muteState, result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::performDeviceSleep ( void ) {
	IOReturn			result;
	
	debugIOLog (3, "+ AppleTopazPluginCS8420::performDeviceSleep()");

	mShadowRegs[map_CS8420_DATA_FLOW_CTRL] &= ~( kCS84XX_BIT_MASK << baCS8420_TXOFF );
	mShadowRegs[map_CS8420_DATA_FLOW_CTRL] |= ( bvCS8420_aes3TX0v << baCS8420_TXOFF );
	result = CODEC_WriteRegister ( map_CS8420_DATA_FLOW_CTRL, mShadowRegs[map_CS8420_DATA_FLOW_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_BIT_MASK << baCS8420_RUN );
	mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] |= ( bvCS8420_runSTOP << baCS8420_RUN );
	result = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

Exit:
	debugIOLog (3, "- AppleTopazPluginCS8420::performDeviceSleep()");
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::performDeviceWake ( void ) {
	IOReturn			result;
	
	debugIOLog (3,  "+ AppleTopazPluginCS8420::performDeviceWake()" );

	flushControlRegisters ();
	
	mShadowRegs[map_CS8420_DATA_FLOW_CTRL] &= ~( kCS84XX_BIT_MASK << baCS8420_TXOFF );
	mShadowRegs[map_CS8420_DATA_FLOW_CTRL] |= ( bvCS8420_aes3TXNormal << baCS8420_TXOFF );
	result = CODEC_WriteRegister ( map_CS8420_DATA_FLOW_CTRL, mShadowRegs[map_CS8420_DATA_FLOW_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

	mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_BIT_MASK << baCS8420_RUN );
	mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] |= ( bvCS8420_runNORMAL << baCS8420_RUN );
	result = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] );
	FailIf ( kIOReturnSuccess != result, Exit );

Exit:
	debugIOLog (3,  "- AppleTopazPluginCS8420::performDeviceWake()" );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::setChannelStatus ( ChanStatusStructPtr channelStatus ) {
	UInt8				data;
	IOReturn			result = kIOReturnError;

	FailIf ( NULL == channelStatus, Exit );
	
	//	Assumes consumer mode
	data = ( ( kCopyPermited << ( 7 - kBACopyright ) ) | ( kConsumer << ( 7 -  kBAProConsumer ) ) );
	if ( channelStatus->nonAudio ) {
		data |= ( kConsumerMode_nonAudio << ( 7 - kBANonAudio ) );		//	consumer mode encoded
	} else {
		data |= ( kConsumerMode_audio << ( 7 - kBANonAudio ) );			//	consumer mode linear PCM
	}
	result = CODEC_WriteRegister ( map_CS8420_BUFFER_0, data );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	if ( channelStatus->nonAudio ) {
		result = CODEC_WriteRegister ( map_CS8420_BUFFER_1, 0 );		//	category code is not valid
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_CS8420_BUFFER_2, 0 );		//	source & channel are not valid
		FailIf ( kIOReturnSuccess != result, Exit );
			
		result = CODEC_WriteRegister ( map_CS8420_BUFFER_3, 0 );		//	not valid
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_CS8420_BUFFER_4, 0 );		//	not valid
		FailIf ( kIOReturnSuccess != result, Exit );
	} else {
		result = CODEC_WriteRegister ( map_CS8420_BUFFER_1, 0x01 );		//	category code is CD
		FailIf ( kIOReturnSuccess != result, Exit );
		
		result = CODEC_WriteRegister ( map_CS8420_BUFFER_2, 0 );		//	source & channel are not specified
		FailIf ( kIOReturnSuccess != result, Exit );
			
		switch ( channelStatus->sampleRate ) {
			case 32000:		result = CODEC_WriteRegister ( map_CS8420_BUFFER_3, cSampleFrequency_32Khz );			break;
			case 44100:		result = CODEC_WriteRegister ( map_CS8420_BUFFER_3, cSampleFrequency_44Khz );			break;
			case 48000:		result = CODEC_WriteRegister ( map_CS8420_BUFFER_3, cSampleFrequency_48Khz );			break;
			default:		result = CODEC_WriteRegister ( map_CS8420_BUFFER_3, cSampleFrequency_44Khz );			break;
		}
		FailIf ( kIOReturnSuccess != result, Exit );
		
		switch ( channelStatus->sampleDepth ) {
			case 16:		result = CODEC_WriteRegister ( map_CS8420_BUFFER_4, cWordLength_20Max_16bits );			break;
			case 24:		result = CODEC_WriteRegister ( map_CS8420_BUFFER_4, cWordLength_24Max_24bits );			break;
			default:		result = CODEC_WriteRegister ( map_CS8420_BUFFER_4, cWordLength_20Max_16bits );			break;
		}
		FailIf ( kIOReturnSuccess != result, Exit );
	}

Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::breakClockSelect ( UInt32 clockSource ) {
	UInt8				data;
	IOReturn			result;

	debugIOLog (7,  "+ AppleTopazPluginCS8420::breakClockSelect ( %d )", (unsigned int)clockSource );
	
	//	Disable error interrupts during completing clock source selection
	result = CODEC_WriteRegister ( map_CS8420_RX_ERROR_MASK, kCS8420_RX_ERROR_MASK_DISABLE_RERR );
	FailIf ( kIOReturnSuccess != result, Exit );

	//	Mute the output port
	data = mShadowRegs[map_CS8420_MISC_CNTRL_1];
	data &= ~( kCS84XX_BIT_MASK << baCS8420_MuteAES );
	data |= ( bvCS8420_muteAES3 << baCS8420_MuteAES );
	result = CODEC_WriteRegister ( map_CS8420_MISC_CNTRL_1, data );
	FailIf ( result != kIOReturnSuccess, Exit );

	//	STOP the codec while switching clocks
	data = mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL];
	data &= ~( 1 << baCS8420_RUN );
	data |= ( bvCS8420_runSTOP << baCS8420_RUN );
	result = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, data );

	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			//	Set input data source for SRC to serial audio input port
			data = mShadowRegs[map_CS8420_DATA_FLOW_CTRL];
			data &= ~( bvCS8420_spdMASK << baCS8420_SPD );
			data |= ( bvCS8420_spdSAI << baCS8420_SPD );
			result = CODEC_WriteRegister ( map_CS8420_DATA_FLOW_CTRL, data );
			FailIf ( result != kIOReturnSuccess, Exit );
			
			//	Set the input time base to the OMCK input pin
			data = mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL];
			data &= ~( kCS84XX_BIT_MASK << baCS8420_OUTC );
			data |= ( bvCS8420_outcOmckXbaCLK << baCS8420_OUTC );
			result = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, data );
			FailIf ( result != kIOReturnSuccess, Exit );

			//	Set the input port data format to slave mode
			data = mShadowRegs[map_CS8420_SERIAL_INPUT_FMT];
			data &= ~( kCS84XX_BIT_MASK << baCS8420_SIMS );
			data |= ( bvCS8420_inputSlave << baCS8420_SIMS );
			result = CODEC_WriteRegister ( map_CS8420_SERIAL_INPUT_FMT, data );
			FailIf ( result != kIOReturnSuccess, Exit );
			
			//	Set the output port data format to slave mode
			data = mShadowRegs[map_CS8420_SERIAL_OUTPUT_FMT];
			data &= ~( kCS84XX_BIT_MASK << baCS8420_SOMS );
			data |= ( bvCS8420_somsSlave << baCS8420_SOMS );
			result = CODEC_WriteRegister ( map_CS8420_SERIAL_OUTPUT_FMT, data );
			FailIf ( result != kIOReturnSuccess, Exit );
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			//	Set input data source for SRC to AES3 receiver
			data = mShadowRegs[map_CS8420_DATA_FLOW_CTRL];
			data &= ~( bvCS8420_spdMASK << baCS8420_SPD );
			data |= ( bvCS8420_spdSrcOut << baCS8420_SPD );
			result = CODEC_WriteRegister ( map_CS8420_DATA_FLOW_CTRL, data );
			FailIf ( result != kIOReturnSuccess, Exit );

			//	Set the input time base to the OMCK input pin
			data = mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL];
			data &= ~( kCS84XX_BIT_MASK << baCS8420_OUTC );
			data |= ( bvCS8420_outcRecIC << baCS8420_OUTC );
			result = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, data );
			FailIf ( result != kIOReturnSuccess, Exit );
			
			//	Set the input port data format to slave mode
			data = mShadowRegs[map_CS8420_SERIAL_INPUT_FMT];
			data &= ~( kCS84XX_BIT_MASK << baCS8420_SIMS );
			data |= ( bvCS8420_inputSlave << baCS8420_SIMS );
			result = CODEC_WriteRegister ( map_CS8420_SERIAL_INPUT_FMT, data );
			FailIf ( result != kIOReturnSuccess, Exit );
			
			//	Set the output port data format to master mode
			data = mShadowRegs[map_CS8420_SERIAL_OUTPUT_FMT];
			data &= ~( kCS84XX_BIT_MASK << baCS8420_SOMS );
			data |= ( bvCS8420_somsMaster << baCS8420_SOMS );
			result = CODEC_WriteRegister ( map_CS8420_SERIAL_OUTPUT_FMT, data );
			FailIf ( result != kIOReturnSuccess, Exit );
			break;
		default:
			result = kIOReturnBadArgument;
			break;
	}
Exit:
	debugIOLog (7,  "- AppleTopazPluginCS8420::breakClockSelect ( %d ) returns %d", (unsigned int)clockSource, (unsigned int)result );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::makeClockSelectPreLock ( UInt32 clockSource ) {
	IOReturn			result;
	UInt8				data;
	
	debugIOLog (7,  "+ AppleTopazPluginCS8420::makeClockSelect ( %d )", (unsigned int)clockSource );

	//	Clear any pending error interrupt status and re-enable error interrupts after completing clock source selection
	result = CODEC_ReadRegister ( map_CS8420_RX_ERROR, &data, 1 );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	//	Enable error (i.e. RERR) interrupts ONLY IF C28420 IS CLOCK MASTER
	if ( kTRANSPORT_SLAVE_CLOCK == clockSource ) {
		result = CODEC_WriteRegister ( map_CS8420_RX_ERROR_MASK, kCS8420_RX_ERROR_MASK_ENABLE_RERR );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK:
			data = mShadowRegs[map_CS8420_DATA_FLOW_CTRL];
			data &= ~( bvCS8420_spdMASK << baCS8420_SPD );
			data |= ( bvCS8420_spdSrcOut << baCS8420_SPD );
			result = CODEC_WriteRegister ( map_CS8420_DATA_FLOW_CTRL, data );

			data = mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL];
			data &= ~( 1 << baCS8420_OUTC );
			data |= ( bvCS8420_outcOmckXbaCLK << baCS8420_OUTC );
			result = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, data );
			break;
		case kTRANSPORT_SLAVE_CLOCK:
			data = mShadowRegs[map_CS8420_DATA_FLOW_CTRL];
			data &= ~( bvCS8420_spdMASK << baCS8420_SPD );
			data |= ( bvCS8420_spdAES3 << baCS8420_SPD );
			result = CODEC_WriteRegister ( map_CS8420_DATA_FLOW_CTRL, data );

			data = mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL];
			data &= ~( 1 << baCS8420_OUTC );
			data |= ( bvCS8420_outcRecIC << baCS8420_OUTC );
			result = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, data );
			break;
	}
	
	//	restart the codec after switching clocks
	data = mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL];
	data &= ~( 1 << baCS8420_RUN );
	data |= ( bvCS8420_runNORMAL << baCS8420_RUN );
	result = CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, data );

	if ( kTRANSPORT_SLAVE_CLOCK == clockSource ) {
		//	It is necessary to restart the I2S cell here after the clocks have been
		//	established using the CS8420 as the clock source.  Ask AOA to restart
		//	the I2S cell.
		FailIf ( NULL == mAudioDeviceProvider, Exit );
		mAudioDeviceProvider->interruptEventHandler ( kRestartTransport, (UInt32)0 );

	}
Exit:
	debugIOLog (7,  "- AppleTopazPluginCS8420::makeClockSelect ( %d ) returns %d", (unsigned int)clockSource, (unsigned int)result );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::makeClockSelectPostLock ( UInt32 clockSource ) {
	IOReturn			result;
	UInt8				data;
	
	debugIOLog (7,  "+ AppleTopazPluginCS8420::makeClockSelect ( %d )", (unsigned int)clockSource );

	//	Unmute the coded output
	data = mShadowRegs[map_CS8420_MISC_CNTRL_1];
	data &= ~( kCS84XX_BIT_MASK << baCS8420_MuteAES );
	data |= mMuteState ? ( bvCS8420_muteAES3 << baCS8420_MuteAES ) : ( bvCS8420_normalAES3 << baCS8420_MuteAES ) ;
	result = CODEC_WriteRegister ( map_CS8420_MISC_CNTRL_1, data );
	FailIf ( result != kIOReturnSuccess, Exit );
	
Exit:
	debugIOLog (7,  "- AppleTopazPluginCS8420::makeClockSelect ( %d ) returns %d", (unsigned int)clockSource, (unsigned int)result );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazPluginCS8420::setRunMode ( UInt8 mode ) {
	CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, mode );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt8 AppleTopazPluginCS8420::setStopMode ( void ) {
	UInt8			data;
	
	//	Stop the device during recovery while preserving the original run state
	data = mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL];
	CODEC_WriteRegister ( map_CS8420_CLOCK_SOURCE_CTRL, data & ~( 1 << baCS8420_RUN ) );
	return data;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This method acquires the codec error status and updates the register
//	cache.  It is normally invoked from the 'notifyHardwareEvent' method
//	in the AppleTopazAudio object in response to a codec error interrupt
//	notification from the AppleOnboardAudio object.
IOReturn	AppleTopazPluginCS8420::getCodecErrorStatus ( UInt32 * dataPtr ) {
	IOReturn		err = kIOReturnBadArgument;
	UInt8			regData;
	
	FailIf ( NULL == dataPtr, Exit );
	*dataPtr = 0;
	err = CODEC_ReadRegister ( map_CS8420_RX_ERROR, &regData, 1 );
	FailIf ( kIOReturnSuccess != err, Exit );
	*dataPtr = (UInt32)regData;
Exit:
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This method operates on the register cache which must be
//			initialized to the hardware state previously through the
//			'getCodecErrorState' method.
bool	AppleTopazPluginCS8420::phaseLocked ( void ) {
	return ( bvCS8420_pllLocked << baCS8420_UNLOCK ) == ( mShadowRegs[map_CS8420_RX_ERROR] & ( bvCS8420_pllUnlocked << baCS8420_UNLOCK ) ) ? true : false ;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This method operates on the register cache which must be
//			initialized to the hardware state previously through the
//			'getCodecErrorState' method.
bool	AppleTopazPluginCS8420::confidenceError ( void ) {
	return ( bvCS8420_confError << baCS8420_CONF ) == ( mShadowRegs[map_CS8420_RX_ERROR] & ( bvCS8420_confError << baCS8420_CONF ) ) ? true : false ;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This method operates on the register cache which must be
//			initialized to the hardware state previously through the
//			'getCodecErrorState' method.
bool	AppleTopazPluginCS8420::biphaseError ( void ) {
	return ( bvCS8420_bipError << baCS8420_BIP ) == ( mShadowRegs[map_CS8420_RX_ERROR] & ( bvCS8420_bipError << baCS8420_BIP ) ) ? true : false ;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8420::disableReceiverError ( void ) {
	CODEC_WriteRegister ( map_CS8420_RX_ERROR_MASK, kCS8420_RX_ERROR_MASK_DISABLE_RERR );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::flushControlRegisters ( void ) {
	IOReturn		result = kIOReturnSuccess;
	
	for ( UInt32 regAddr = map_CS8420_MISC_CNTRL_1; regAddr <= map_CS8420_BUFFER_23; regAddr++ ) {
		if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
			result = CODEC_WriteRegister ( regAddr, mShadowRegs[regAddr] );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
Exit:
	return result; 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8420::useExternalCLK ( void ) {
	//	If the recovered clock is derived from the I2S I/O Module LRCLK and there is an external source 
	//	then switch the recovered clock to derive from the external source.
	mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_TWO_BIT_MASK << baCS8420_RXD );
	mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] |= ( bvCS8420_rxd256fsiAES3 << baCS8420_RXD );
	CODEC_WriteRegister( map_CS8420_CLOCK_SOURCE_CTRL, mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] );
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8420::useInternalCLK ( void ) {
	//	If the recovered clock is derived from an external source and there is no external source 
	//	then switch the recovered clock to derive from the I2S I/O Module LRCLK.
	mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] &= ~( kCS84XX_TWO_BIT_MASK << baCS8420_RXD );
	mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] |= ( bvCS8420_rxd256fsiILRCLK << baCS8420_RXD );
	CODEC_WriteRegister( map_CS8420_CLOCK_SOURCE_CTRL, mShadowRegs[map_CS8420_CLOCK_SOURCE_CTRL] );
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt8	AppleTopazPluginCS8420::CODEC_GetDataMask ( UInt8 regAddr ) {
	return kMASK_NONE;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::CODEC_GetRegSize ( UInt8 regAddr, UInt32 * codecRegSizePtr ) {
	IOReturn		result;
	
	result = kIOReturnError;
	if ( NULL != codecRegSizePtr ) {
		if ( map_CS8420_BUFFER_0 == regAddr ) {
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
IOReturn	AppleTopazPluginCS8420::CODEC_IsControlRegister ( UInt8 regAddr ) {
	IOReturn	result;

	switch ( regAddr ) {
		case map_CS8420_SERIAL_OUTPUT_FMT:
		case map_CS8420_RX_ERROR_MASK:
		case map_CS8420_MISC_CNTRL_1:
		case map_CS8420_MISC_CNTRL_2:
		case map_CS8420_DATA_FLOW_CTRL:
		case map_CS8420_CLOCK_SOURCE_CTRL:
		case map_CS8420_SERIAL_INPUT_FMT:
		case map_CS8420_IRQ1_MASK:
		case map_CS8420_IRQ1_MODE_MSB:
		case map_CS8420_IRQ1_MODE_LSB:
		case map_CS8420_IRQ2_MASK:
		case map_CS8420_IRQ2_MODE_MSB:
		case map_CS8420_IRQ2_MODE_LSB:
		case map_CS8420_CH_STATUS_DATA_BUF_CTRL:
		case map_CS8420_USER_DATA_BUF_CTRL:
		case map_CS8420_BUFFER_0:
		case map_CS8420_BUFFER_1:
		case map_CS8420_BUFFER_2:
		case map_CS8420_BUFFER_3:
		case map_CS8420_BUFFER_4:
		case map_CS8420_BUFFER_5:
		case map_CS8420_BUFFER_6:
		case map_CS8420_BUFFER_7:
		case map_CS8420_BUFFER_8:
		case map_CS8420_BUFFER_9:
		case map_CS8420_BUFFER_10:
		case map_CS8420_BUFFER_11:
		case map_CS8420_BUFFER_12:
		case map_CS8420_BUFFER_13:
		case map_CS8420_BUFFER_14:
		case map_CS8420_BUFFER_15:
		case map_CS8420_BUFFER_16:
		case map_CS8420_BUFFER_17:
		case map_CS8420_BUFFER_18:
		case map_CS8420_BUFFER_19:
		case map_CS8420_BUFFER_20:
		case map_CS8420_BUFFER_21:
		case map_CS8420_BUFFER_22:
		case map_CS8420_BUFFER_23:			result = kIOReturnSuccess;			break;
		default:							result = kIOReturnError;			break;
	}

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::CODEC_IsStatusRegister ( UInt8 regAddr ) {
	IOReturn	result;
	
	result = kIOReturnError;
	
	switch ( regAddr ) {
		case map_CS8420_IRQ1_STATUS:
		case map_CS8420_IRQ2_STATUS:
		case map_CS8420_RX_CH_STATUS:
		case map_CS8420_RX_ERROR:
		case map_ID_VERSION:
		case map_CS8420_Q_CHANNEL_SUBCODE_AC:
		case map_CS8420_Q_CHANNEL_SUBCODE_TRK:
		case map_CS8420_Q_CHANNEL_SUBCODE_INDEX:
		case map_CS8420_Q_CHANNEL_SUBCODE_MIN:
		case map_CS8420_Q_CHANNEL_SUBCODE_SEC:
		case map_CS8420_Q_CHANNEL_SUBCODE_FRAME:
		case map_CS8420_Q_CHANNEL_SUBCODE_ZERO:
		case map_CS8420_Q_CHANNEL_SUBCODE_ABS_MIN:
		case map_CS8420_Q_CHANNEL_SUBCODE_ABS_SEC:
		case map_CS8420_Q_CHANNEL_SUBCODE_ABS_FRAME:
		case map_CS8420_SAMPLE_RATE_RATIO:
			result = kIOReturnSuccess;
			break;
		default:
			result = kIOReturnError;
			break;
	}
	
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8420::getPluginState ( HardwarePluginDescriptorPtr outState ) {
	IOReturn		result = kIOReturnBadArgument;
	
	FailIf ( NULL == outState, Exit );
	outState->hardwarePluginType = kCodec_CS8420;
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
IOReturn	AppleTopazPluginCS8420::setPluginState ( HardwarePluginDescriptorPtr inState ) {
	IOReturn		result = kIOReturnBadArgument;
	
	FailIf ( NULL == inState, Exit );
	FailIf ( sizeof ( mShadowRegs ) != inState->registerCacheSize, Exit );
	result = kIOReturnSuccess;
	for ( UInt32 registerAddress = map_CS8420_MISC_CNTRL_1; ( registerAddress < map_ID_VERSION ) && ( kIOReturnSuccess == result ); registerAddress++ ) {
		if ( inState->registerCache[registerAddress] != mShadowRegs[registerAddress] ) {
			if ( kIOReturnSuccess == CODEC_IsControlRegister ( (UInt8)registerAddress ) ) {
				result = CODEC_WriteRegister ( registerAddress, inState->registerCache[registerAddress] );
			}
		}
	}
Exit:
	return result;
}

