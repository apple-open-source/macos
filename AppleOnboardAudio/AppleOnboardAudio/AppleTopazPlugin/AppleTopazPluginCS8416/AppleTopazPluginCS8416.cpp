/*
 *  AppleTopazPluginCS8416.cpp
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Tue Oct 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AppleTopazPluginCS8416.h"

#define super AppleTopazPlugin

OSDefineMetaClassAndStructors ( AppleTopazPluginCS8416, AppleTopazPlugin )

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8416::init ( OSDictionary *properties ) {
	debugIOLog (3, "± AppleTopazPluginCS8416::init ( %p )", properties );
	return super::init (properties);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8416::start ( IOService * provider ) {
	debugIOLog (3, "± AppleTopazPluginCS8416::start ( %p )", provider );
	return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8416::free ( void ) {
	debugIOLog (3, "± AppleTopazPluginCS8416::free ()" );
	super::free ();
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AppleTopazPluginCS8416::preDMAEngineInit ( void ) {
	bool			result = false;
	IOReturn		err;
	
	debugIOLog ( 6, "+ AppleToapzPluginCS8416::preDMAEngineInit ()" );
	err = CODEC_WriteRegister ( mapControl_0, kControl_0_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapControl_1, kControl_1_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapControl_2, kControl_2_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapControl_3, kControl_3_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapControl_4, kControl_4_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapSerialAudioDataFormat, kSerialAudioDataFormat_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapReceiverErrorMask, kReceiverErrorMask_DISABLE );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapInterruptMask, kInterruptMask_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapInterruptModeMSB, kInterruptModeMSB_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapInterruptModeLSB, kInterruptModeLSB_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapControl_4, kControl_4_RUN );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	result = true;
Exit:
	debugIOLog ( 6, "- AppleToapzPluginCS8416::preDMAEngineInit () returns %d", result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::initCodecRegisterCache ( void ) { 
	IOReturn		result = kIOReturnSuccess;
	IOReturn		err;
	
	for ( UInt32 regAddr = mapControl_0; regAddr <= map_ID_VERSION; regAddr++ ) {
		if ( kIOReturnSuccess == CODEC_IsStatusRegister ( regAddr ) ) {
			err = CODEC_ReadRegister ( regAddr, NULL, 1 );
			if ( kIOReturnSuccess != err && kIOReturnSuccess == result ) {
				result = err;
			}
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::setMute ( bool muteState ) {
	IOReturn		result = kIOReturnError;
	
	mShadowRegs[mapControl_1] &= ~( 1 << baMUTSAO );
	mShadowRegs[mapControl_1] |= muteState ? ( bvSDOUTmuted << baMUTSAO ) : ( bvSDOUTnotMuted << baMUTSAO ) ;
	result = CODEC_WriteRegister ( mapControl_1, mShadowRegs[mapControl_1] );
	FailIf ( kIOReturnSuccess != result, Exit );
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::performDeviceSleep ( void ) {
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3, "+ AppleTopazPluginCS8416::performDeviceSleep()");

	mShadowRegs[mapControl_4] &= ~( 1 << baRun );
	mShadowRegs[mapControl_4] |= ( bvStopped << baRun );
	result = CODEC_WriteRegister ( mapControl_4, mShadowRegs[mapControl_4] );
	FailIf ( kIOReturnSuccess != result, Exit );
Exit:
	debugIOLog (3, "- AppleTopazPluginCS8416::performDeviceSleep()");

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::performDeviceWake ( void ) {
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3,  "+ AppleTopazPluginCS8416::performDeviceWake()" );

	mShadowRegs[mapControl_4] &= ~( 1 << baRun );
	mShadowRegs[mapControl_4] |= ( bvRunning << baRun );
	result = CODEC_WriteRegister ( mapControl_4, mShadowRegs[mapControl_2] );
	FailIf ( kIOReturnSuccess != result, Exit );
Exit:
	debugIOLog (3, "- AppleTopazPluginCS8416::performDeviceWake()" );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::breakClockSelect ( UInt32 clockSource ) {
	IOReturn		result = kIOReturnError;
	UInt8			regData;

	//	Disable error interrupts
	result = CODEC_WriteRegister ( mapReceiverErrorMask, kReceiverErrorMask_DISABLE );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	//	Mute
	result = setMute ( TRUE );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	//	Stop the codec while switching clocks
	FailIf ( kIOReturnSuccess != CODEC_ReadRegister ( mapControl_4, &regData, 1 ), Exit );
	regData &= ~( 1 << baRun );
	regData |= ( bvStopped << baRun );
	result = CODEC_WriteRegister ( mapControl_4, regData );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	switch ( clockSource ) {
		case kTRANSPORT_MASTER_CLOCK: {
				debugIOLog ( 2, "breakClockSelect ( kTRANSPORT_MASTER_CLOCK )" );
				//	Set serial format to slave mode
				FailIf ( kIOReturnSuccess != CODEC_ReadRegister ( mapSerialAudioDataFormat, &regData, 1 ), Exit );
				regData &= ~( 1 << baSOMS );
				regData |= ( bvSerialOutputSlaveMode << baSOMS );
				result = CODEC_WriteRegister ( mapSerialAudioDataFormat, regData );
				FailIf ( kIOReturnSuccess != result, Exit );
			}
			break;
		case kTRANSPORT_SLAVE_CLOCK: {
				debugIOLog ( 2, "breakClockSelect ( kTRANSPORT_SLAVE_CLOCK )" );
				//	Set serial format to master mode
				FailIf ( kIOReturnSuccess != CODEC_ReadRegister ( mapSerialAudioDataFormat, &regData, 1 ), Exit );
				regData &= ~( 1 << baSOMS );
				regData |= ( bvSerialOutputMasterMode << baSOMS );
				result = CODEC_WriteRegister ( mapSerialAudioDataFormat, regData );
				FailIf ( kIOReturnSuccess != result, Exit );
			}
			break;
	}
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::makeClockSelectPreLock ( UInt32 clockSource ) {
	IOReturn		result = kIOReturnError;
	UInt8			regData;
	
	//	Clear any pending error interrupt and re-enable error interrupts after completing clock source selection
	FailIf ( kIOReturnSuccess != CODEC_ReadRegister ( mapReceiverError, &regData, 1 ), Exit );
	
	//	Enable error (i.e. RERR) interrupts if CS8416 is master
	if ( kTRANSPORT_SLAVE_CLOCK == clockSource ) {
		result = CODEC_WriteRegister ( mapReceiverErrorMask, kReceiverErrorMask_ENABLE );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	//	Restart the CODEC
	FailIf ( kIOReturnSuccess != CODEC_ReadRegister ( mapControl_4, &regData, 1 ), Exit );
	regData = regData & ~( 1 << baRun );
	regData = regData | ( bvRunning << baRun );
	result = CODEC_WriteRegister ( mapControl_4, regData );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	if ( kTRANSPORT_SLAVE_CLOCK == clockSource ) {
		//	It is necessary to restart the I2S cell here after the clocks have been
		//	established using the CS8420 as the clock source.  Ask AOA to restart
		//	the I2S cell.
		FailIf ( NULL == mAudioDeviceProvider, Exit );
		mAudioDeviceProvider->interruptEventHandler ( kRestartTransport, (UInt32)0 );
	}
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::makeClockSelectPostLock ( UInt32 clockSource ) {
	IOReturn		result = kIOReturnError;
	
	//	Restore the CODEC mute state according to 'mMuteState'
	setMute ( mMuteState );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Used only to restore the code previous run state where the 'mode' was
//	previously acquired from invoking the 'setStopMode' method.
void AppleTopazPluginCS8416::setRunMode ( UInt8 mode ) {
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3, "+ AppleTopazPluginCS8416::setRunMode( %X )", mode);

	result = CODEC_WriteRegister ( mapControl_4, mode );
	FailIf ( kIOReturnSuccess != result, Exit );
Exit:
	debugIOLog (3, "- AppleTopazPluginCS8416::setRunMode( %X )", mode);
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Returns the previous run mode as the result.  This is used for stopping
//	the codec and then the result is used to restore the codec to the
//	previous run state.
UInt8 AppleTopazPluginCS8416::setStopMode ( void ) {
	IOReturn		err = kIOReturnError;
	UInt8			result = 0;
	
	debugIOLog (3, "+ AppleTopazPluginCS8416::setStopMode()");
	result = mShadowRegs[mapControl_4];
	mShadowRegs[mapControl_4] &= ~( 1 << baRun );
	mShadowRegs[mapControl_4] |= ( bvStopped << baRun );
	err = CODEC_WriteRegister ( mapControl_4, mShadowRegs[mapControl_4] );
	FailIf ( kIOReturnSuccess != result, Exit );
Exit:
	debugIOLog (3, "- AppleTopazPluginCS8416::setStopMode() returns %X", result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This method acquires the codec error status and updates the register
//	cache.  It is normally invoked from the 'notifyHardwareEvent' method
//	in the AppleTopazAudio object in response to a codec error interrupt
//	notification from the AppleOnboardAudio object.
IOReturn	AppleTopazPluginCS8416::getCodecErrorStatus ( UInt32 * dataPtr ) {
	IOReturn		err = kIOReturnBadArgument;
	UInt8			regData;
	
	FailIf ( NULL == dataPtr, Exit );
	*dataPtr = 0;
	err = CODEC_ReadRegister ( mapReceiverError, &regData, 1 );
	FailIf ( kIOReturnSuccess != err, Exit );
	*dataPtr = (UInt32)regData;
Exit:
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This method operates on the register cache which must be
//			initialized to the hardware state previously through the
//			'getCodecErrorState' method.
bool	AppleTopazPluginCS8416::phaseLocked ( void ) {
	return ( mShadowRegs[mapReceiverError] & ( 1 << baUNLOCK )) ? FALSE : TRUE ;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This method operates on the register cache which must be
//			initialized to the hardware state previously through the
//			'getCodecErrorState' method.
bool	AppleTopazPluginCS8416::confidenceError ( void ) {
	return ( mShadowRegs[mapReceiverError] & ( 1 << baCONF )) ? TRUE : FALSE ;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This method operates on the register cache which must be
//			initialized to the hardware state previously through the
//			'getCodecErrorState' method.
bool	AppleTopazPluginCS8416::biphaseError ( void ) {
	return ( mShadowRegs[mapReceiverError] & ( 1 << baBIP )) ? TRUE : FALSE ;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTopazPluginCS8416::disableReceiverError ( void ) {
	(void)CODEC_WriteRegister ( mapReceiverErrorMask, kReceiverErrorMask_DISABLE );
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::flushControlRegisters ( void ) {
	IOReturn		result = kIOReturnSuccess;
	
	for ( UInt32 regAddr = mapControl_0; regAddr <= mapBurstPreamblePD_1; regAddr++ ) {
		if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
			result = CODEC_WriteRegister ( regAddr, mShadowRegs[regAddr] );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
Exit:
	return result; 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	The CS8416 is configured  so that the 'FSWCLK' bit is set to
//			have RMCK output driven from the OMCK signal according to the
//			SWCLK state where SWCLK is set to enable automatic switching.
//			This configuration forces the S/PDIF input on the CS8416 to
//			always run on the external clock when an external clock is 
//			available.  Under these circumstances, the AppleOnboardAudio
//			instance should make the clock switch selector a 'READ ONLY'
//			control.
//			
//			This method serves only to manage the receiver error interrupt enable.
//			
void	AppleTopazPluginCS8416::useExternalCLK ( void ) {
	IOReturn		err;
	
	err = CODEC_WriteRegister ( mapSerialAudioDataFormat, kSerialAudioDataFormat_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
Exit:
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	The CS8416 is configured  so that the 'FSWCLK' bit is set to
//			have RMCK output driven from the OMCK signal according to the
//			SWCLK state where SWCLK is set to enable automatic switching.
//			This configuration forces the S/PDIF input on the CS8416 to
//			always run on the external clock when an external clock is 
//			available.  Under these circumstances, the AppleOnboardAudio
//			instance should make the clock switch selector a 'READ ONLY'
//			control.
//			
//			This method serves only to manage the receiver error interrupt enable.
//			
void	AppleTopazPluginCS8416::useInternalCLK ( void ) {
	IOReturn		err;
	
	err = CODEC_WriteRegister ( mapSerialAudioDataFormat, kSerialAudioDataFormat_INIT | ( bvSerialOutputMasterMode << baSOMS ) );
	FailIf ( kIOReturnSuccess != err, Exit );
Exit:
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Poll the status registers to keep the register cache somewhat coherent
//	for the user client to access status data properly.
void AppleTopazPluginCS8416::poll ( void ) {
	for ( UInt8 registerAddress = mapReceiverChannelStatus; registerAddress <= mapBurstPreamblePD_1; registerAddress++ ) {
		CODEC_ReadRegister ( registerAddress, &mShadowRegs[registerAddress], 1 );
	}
	CODEC_ReadRegister ( map_ID_VERSION, &mShadowRegs[map_ID_VERSION], 1 );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::CODEC_GetRegSize ( UInt8 regAddr, UInt32 * codecRegSizePtr ) {
	IOReturn		result = kIOReturnError;
	
	if ( NULL != codecRegSizePtr ) {
		if ( kIOReturnSuccess == CODEC_IsStatusRegister ( regAddr ) || kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
			switch ( regAddr ) {
				case mapQChannelSubcode: {
						*codecRegSizePtr = ( mapQChannelSubcode_72_79 - mapQChannelSubcode_0_7 ) + 1;
					}
					break;
				case mapChannelAStatus: {
						*codecRegSizePtr = ( mapChannelAStatus_4 - mapChannelAStatus_0 ) + 1;
					}
					break;
				case mapChannelBStatus: {
						*codecRegSizePtr = ( mapChannelBStatus_4 - mapChannelBStatus_0 ) + 1;
					}
					break;
				case mapBurstPreamblePC: {
						*codecRegSizePtr = ( mapBurstPreamblePC_1 - mapBurstPreamblePC_0 ) + 1;
					}
					break;
				case mapBurstPreamblePD: {
						*codecRegSizePtr = ( mapBurstPreamblePD_1 - mapBurstPreamblePD_0 ) + 1;
					}
					break;
				default: {
						*codecRegSizePtr = 1;
					}
					break;
			}
			result = kIOReturnSuccess;
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::CODEC_IsControlRegister ( UInt8 regAddr ) {
	IOReturn		result = kIOReturnError;

	if ( regAddr <= mapInterruptModeLSB ) {
		result = kIOReturnSuccess;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::CODEC_IsStatusRegister ( UInt8 regAddr ) {
	IOReturn		result = kIOReturnError;
	
	if ( regAddr <= mapBurstPreamblePD_1 ) {
		result = kIOReturnSuccess;
	} else if ( map_ID_VERSION == regAddr ) {
		result = kIOReturnSuccess;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::getPluginState ( HardwarePluginDescriptorPtr outState ) {
	IOReturn		result = kIOReturnBadArgument;
	
	FailIf ( NULL == outState, Exit );
	outState->hardwarePluginType = kCodec_CS8416;
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
IOReturn	AppleTopazPluginCS8416::setPluginState ( HardwarePluginDescriptorPtr inState ) {
	IOReturn		result = kIOReturnBadArgument;
	
	FailIf ( NULL == inState, Exit );
	FailIf ( sizeof ( mShadowRegs ) != inState->registerCacheSize, Exit );
	result = kIOReturnSuccess;
	for ( UInt32 registerAddress = mapControl_0; ( registerAddress < map_ID_VERSION ) && ( kIOReturnSuccess == result ); registerAddress++ ) {
		if ( inState->registerCache[registerAddress] != mShadowRegs[registerAddress] ) {
			if ( kIOReturnSuccess == CODEC_IsControlRegister ( (UInt8)registerAddress ) ) {
				result = CODEC_WriteRegister ( registerAddress, inState->registerCache[registerAddress] );
			}
		}
	}
Exit:
	return result;
}

