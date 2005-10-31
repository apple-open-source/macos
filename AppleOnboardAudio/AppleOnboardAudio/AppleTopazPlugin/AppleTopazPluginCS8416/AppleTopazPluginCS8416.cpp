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
bool	AppleTopazPluginCS8416::preDMAEngineInit ( UInt32 autoClockSelectionIsBeingUsed ) {
	bool			result = false;
    UInt8           regVal;
	IOReturn		err;
    
    mDelayPollAfterWakeCounter = kPollsToDelayReportingAfterWake;  // make sure we let things stabilize at start-up before posting clock lock/unlock status
    mOMCK_RMCK_RatioHasBeenCached = false;
    mAutoClockSelectionIsBeingUsed = autoClockSelectionIsBeingUsed;
    
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
	
    if ( mAutoClockSelectionIsBeingUsed ) {
        regVal = kReceiverErrorMask_ENABLE | ( bvInterruptEnabled << baINVALID );
    }
    else {
        regVal = kReceiverErrorMask_ENABLE;
    }
    
	err = CODEC_WriteRegister ( mapReceiverErrorMask, regVal );
	FailIf ( kIOReturnSuccess != err, Exit );
    
    if ( mAutoClockSelectionIsBeingUsed ) {
        regVal = kInterruptMask_INIT | ( bvInterruptEnabled << baRERR );
    }
    else {
        regVal = kInterruptMask_INIT;
    }
    
	err = CODEC_WriteRegister ( mapInterruptMask, regVal );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapInterruptModeMSB, kInterruptModeMSB_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapInterruptModeLSB, kInterruptModeLSB_INIT );
	FailIf ( kIOReturnSuccess != err, Exit );
	
	err = CODEC_WriteRegister ( mapControl_4, kControl_4_RUN );
	FailIf ( kIOReturnSuccess != err, Exit );
	
    // [4176686]
    // Store receiver channel status to detect changes in bit depth across calls to 
    // notifyHardwareEvent.
    err = CODEC_ReadRegister ( mapReceiverChannelStatus, &mCachedReceiverChannelStatus, 1 );
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
IOReturn	AppleTopazPluginCS8416::performDeviceSleep ( void ) {
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3, "+ AppleTopazPluginCS8416::performDeviceSleep()");

	mTopazCS8416isSLEEP = TRUE;																	//  [3678605]
	mDelayPollAfterWakeCounter = kPollsToDelayReportingAfterWake;								//  [3678605]
    mOMCK_RMCK_RatioHasBeenCached = false;
	
	//  [3678605]   begin {
	//  Preserve the state of the RATIO register when sleeping so that
	//  a change in ratio upon wake can be interpretted as a loss of
	//  LOCK.
	result = CODEC_ReadRegister ( mapControl_4, &mShadowRegs[mapControl_4], 1 );
	FailIf ( kIOReturnSuccess != result, Exit );
	debugIOLog ( 4, "  mShadowRegs[mapControl_4] = 0x%0.2X", mShadowRegs[mapControl_4] );

	result = CODEC_ReadRegister ( mapOMCK_RMCK_Ratio, &mShadowRegs[mapOMCK_RMCK_Ratio], 1 );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	mRatioEnteringSleep = mShadowRegs[mapOMCK_RMCK_Ratio];
	debugIOLog ( 4, "  mRatioEnteringSleep 0x%0.2X", mRatioEnteringSleep );
	//  }   end		[3678605]

	
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

	mDelayPollAfterWakeCounter = kPollsToDelayReportingAfterWake;								//  [3678605]
    mOMCK_RMCK_RatioHasBeenCached = false;
    
	mShadowRegs[mapControl_4] &= ~( 1 << baRun );
	result = CODEC_WriteRegister ( mapControl_4, mShadowRegs[mapControl_4] );					//  [3678605]
	FailIf ( kIOReturnSuccess != result, Exit );

	mShadowRegs[mapControl_4] |= ( bvRunning << baRun );
	result = CODEC_WriteRegister ( mapControl_4, mShadowRegs[mapControl_4] );
	FailIf ( kIOReturnSuccess != result, Exit );

	result = flushControlRegisters ();															//  [3674345]   [3678605]
	FailIf ( kIOReturnSuccess != result, Exit );

	mTopazCS8416isSLEEP = FALSE;																//  [3678605]
Exit:
	debugIOLog (3, "- AppleTopazPluginCS8416::performDeviceWake()" );

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::breakClockSelect ( UInt32 clockSource ) {
	IOReturn		result = kIOReturnError;
	UInt8			regData;

	debugIOLog ( 5, "+ AppleTopazPluginCS8416::breakClockSelect ( 0x%0.8X )", clockSource );
	
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
	debugIOLog ( 5, "- AppleTopazPluginCS8416::breakClockSelect ( 0x%0.8X ) returns 0x%0.8X", clockSource, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::makeClockSelectPreLock ( UInt32 clockSource ) {
	IOReturn		result = kIOReturnError;
	UInt8			regData;
	
	debugIOLog ( 5, "+ AppleTopazPluginCS8416::makeClockSelectPreLock ( 0x%0.8X )", clockSource );
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

	result = CODEC_WriteRegister ( mapControl_4, regData );										//  [3674345]
	FailIf ( kIOReturnSuccess != result, Exit );

	regData = regData | ( bvRunning << baRun );
	result = CODEC_WriteRegister ( mapControl_4, regData );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	result = flushControlRegisters ();															//  [3674345]
	FailIf ( kIOReturnSuccess != result, Exit );

	if ( kTRANSPORT_SLAVE_CLOCK == clockSource ) {
		//	It is necessary to restart the I2S cell here after the clocks have been
		//	established using the CS8420 as the clock source.  Ask AOA to restart
		//	the I2S cell.
		FailIf ( NULL == mAudioDeviceProvider, Exit );
		mAudioDeviceProvider->interruptEventHandler ( kRestartTransport, (UInt32)0 );
	}

Exit:
	debugIOLog ( 5, "- AppleTopazPluginCS8416::makeClockSelectPreLock ( 0x%0.8X ) returns 0x%0.8X", clockSource, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::makeClockSelectPostLock ( UInt32 clockSource ) {
	IOReturn result = kIOReturnSuccess;
	
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
	FailIf ( kIOReturnSuccess != err, Exit );
Exit:
	debugIOLog (3, "- AppleTopazPluginCS8416::setStopMode() returns %X", result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  Never disable the receiver errors on the CS8416.  The CS8416 is an input
//  only device with no hardware sample rate conversion so the I2S module 
//  always operates in slave mode.  Receiver errors are used to broadcast
//  status to other AppleOnboardAudio instances and are not used in the context
//  of the AppleOnboardAudio instance owning this plugin module.
void	AppleTopazPluginCS8416::disableReceiverError ( void ) {
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
	if ( kReceiverErrorMask_ENABLE == ( mShadowRegs[mapReceiverErrorMask] & kReceiverErrorMask_ENABLE ) ) {
		result = CODEC_WriteRegister ( mapReceiverErrorMask, kReceiverErrorMask_DISABLE );			//  [3678605]
		FailIf ( kIOReturnSuccess != result, Exit );

		result = CODEC_WriteRegister ( mapReceiverErrorMask, kReceiverErrorMask_ENABLE );			//  [3674345]
		FailIf ( kIOReturnSuccess != result, Exit );
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
	UInt8			data = kSerialAudioDataFormat_INIT | ( bvSerialOutputMasterMode << baSOMS );
	
	debugIOLog ( 5, "+ AppleTopazPluginCS8416::useExternalCLK () about to write 0x%X to mapSerialAudioDataFormat", data );
	err = CODEC_WriteRegister ( mapSerialAudioDataFormat, data );
	FailIf ( kIOReturnSuccess != err, Exit );
Exit:
	debugIOLog ( 5, "- AppleTopazPluginCS8416::useExternalCLK ()" );
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
	UInt8			data = kSerialAudioDataFormat_INIT | ( bvSerialOutputSlaveMode << baSOMS );
	
	debugIOLog ( 5, "+ AppleTopazPluginCS8416::useInternalCLK () about to write 0x%X to mapSerialAudioDataFormat", data );
	err = CODEC_WriteRegister ( mapSerialAudioDataFormat, data );
	FailIf ( kIOReturnSuccess != err, Exit );
Exit:
	debugIOLog ( 5, "- AppleTopazPluginCS8416::useInternalCLK ()" );
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Poll the status registers to keep the register cache somewhat coherent
//	for the user client to access status data properly.  Polling avoids 
//  accessing any interrupt register that is processed in the 'notifyHardwareEvent'
//  method to avoid potential corruption of the interrupt status.
void AppleTopazPluginCS8416::poll ( void ) {
	for ( UInt8 registerAddress = mapControl_0; registerAddress <= mapBurstPreamblePD_1; registerAddress++ ) {
		if ( ( kIOReturnSuccess == CODEC_IsControlRegister ( registerAddress ) ) || ( kIOReturnSuccess == CODEC_IsStatusRegister ( registerAddress ) ) ) {
			if ( mapReceiverError != registerAddress ) {
				CODEC_ReadRegister ( registerAddress, &mShadowRegs[registerAddress], 1 );
			}
		}
	}
	CODEC_ReadRegister ( map_ID_VERSION, &mShadowRegs[map_ID_VERSION], 1 );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  This method is invoked from the 'codecErrorInterruptHandler' residing in the
//  platform interface object.  The 'codecErrorInterruptHandler' may be invoked
//  through GPIO hardware interrupt dispatch services or through timer polled
//  services.  Section 7.1.1 of the CS8416 DS578PP4 Data Sheet indicates that the
//  codec interrupt status bits are "sticky" and require two transactions to
//  validate the current interrupt status and clear the interrupt.
void AppleTopazPluginCS8416::notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue ) {
	IOReturn		error = kIOReturnError;
    UInt32          autoClockLockState;                                                                         //  [4189050]
    UInt32          maxSlewRatio;                                                                               //  [4189050]
    UInt32          minSlewRatio;                                                                               //  [4189050]
    UInt32          ratio;                                                                                      //  [4073140]
    UInt8           receiverChannelStatus;                                                                      //  [4176686]
    bool            validRatioDetected;                                                                         //  [4073140]
    bool            lastRatioWasValid;                                                                          //  [4189050]
    bool            ratioStepDetected;                                                                          //  [4189050]
    bool            bitDepthChangeDetected;                                                                     //  [4176686]
    bool            dataIsValid;                                                                                //  [4244167, 4267209]
    
	switch ( statusSelector ) {
		case kCodecErrorInterruptStatus:	debugIOLog ( 4, "+ AppleTopazPluginCS8416::notifyHardwareEvent ( %d = kCodecErrorInterruptStatus, %d ), mDelayPollAfterWakeCounter %ld", statusSelector, newValue, mDelayPollAfterWakeCounter );	break;
		case kCodecInterruptStatus:			debugIOLog ( 5, "+ AppleTopazPluginCS8416::notifyHardwareEvent ( %d = kCodecInterruptStatus, %d )", statusSelector, newValue );			break;
		default:							debugIOLog ( 5, "+ AppleTopazPluginCS8416::notifyHardwareEvent ( %d, %d )", statusSelector, newValue );									break;
	}
	
	if ( !mTopazCS8416isSLEEP ) {																				//  [3678605]
		switch ( statusSelector ) {
			case kCodecErrorInterruptStatus:                
				if ( ( 0 == mDelayPollAfterWakeCounter ) && mOMCK_RMCK_RatioHasBeenCached )  // assumes decrementDelayPollAfterWakeCounter() is always called prior to notifyHardwareEvent()
				{																								//  [3678605]
                    // [4073140] - validate OMCK/RMCK ratio register
                    error = CODEC_ReadRegister ( mapOMCK_RMCK_Ratio, &mShadowRegs[mapOMCK_RMCK_Ratio], 1 );
                    FailIf ( kIOReturnSuccess != error, Exit );
                    
                    ratio = (UInt32) mShadowRegs[mapOMCK_RMCK_Ratio];
                    validRatioDetected = ( kCS84XX_OMCK_RMCK_RATIO_LOCKED_MIN <= ratio ) && ( kCS84XX_OMCK_RMCK_RATIO_LOCKED_MAX >= ratio );
                    lastRatioWasValid = ( kCS84XX_OMCK_RMCK_RATIO_LOCKED_MIN <= mCached_OMCK_RMCK_Ratio ) && ( kCS84XX_OMCK_RMCK_RATIO_LOCKED_MAX >= mCached_OMCK_RMCK_Ratio );
                    
                    // [4189050] - check for a step change in the ratio (ratio and max/min slew ratios must be wider than 8 bits to prevent overflow here)
                    maxSlewRatio = mCached_OMCK_RMCK_Ratio + kMAX_OMCK_RMCK_RATIO_STEP;
                    minSlewRatio = mCached_OMCK_RMCK_Ratio - kMAX_OMCK_RMCK_RATIO_STEP;
                    
                    ratioStepDetected = ( ratio > maxSlewRatio ) || ( ratio < minSlewRatio );
                    
                    // [4176686]
                    // Force posting of an unlock message if we detect a change in bit depth by reading the 
                    // receiver channel status and comparing it to our cached value and noting that the two differ.
                    error = CODEC_ReadRegister ( mapReceiverChannelStatus, &mShadowRegs[mapReceiverChannelStatus], 1 );
                    FailIf ( kIOReturnSuccess != error, Exit );
                    
                    receiverChannelStatus = mShadowRegs[mapReceiverChannelStatus];
                    bitDepthChangeDetected = receiverChannelStatus != mCachedReceiverChannelStatus;
                    
                    // [4189050]
                    // Post unlock and a pending relock message if...
                    //      - a ratio step was detected and at least one of the step endpoints corresponds to a valid ratio
                    //      - a bit depth change was detected and the ratio is valid
                    if ( ratioStepDetected && ( validRatioDetected || lastRatioWasValid ) ) {
                        debugIOLog ( 4, "  AppleTopazPluginCS8416::notifyHardwareEvent about to post kClockUnLockStatus due to step in sampling rate ..." );
                        mCodecHasLocked = FALSE;
                        autoClockLockState = kAutoClockLockStatePendingRelock;
                    }
                    else if ( validRatioDetected ) {
                        if ( bitDepthChangeDetected ) {
                            debugIOLog ( 4, "  AppleTopazPluginCS8416::notifyHardwareEvent about to post kClockUnLockStatus due to change in bit depth ..." );
                            mCodecHasLocked = FALSE;
                            autoClockLockState = kAutoClockLockStatePendingRelock;
                        }
                        else {
                            // [4244167, 4267209]
                            // It could be the case that the CS8416 PLL locks to "noise", thinking that it has external clock, when in fact, there is no
                            // external clock present.  When auto locking to external clock, we don't want this to happen, as we'll try to run on an
                            // invalid external clock.  So, we must further check to make sure that the data we're receiving is valid.
                            if ( mAutoClockSelectionIsBeingUsed ) {
                                UInt8 tempRegData;
                                
                                // clear the "sticky" error bits
                                error = CODEC_ReadRegister ( mapReceiverError, &tempRegData, 1 );
                                FailIf ( kIOReturnSuccess != error, Exit );
                                
                                // check to see if a "V" receiver error, i.e. AES3 validity error, has occurred
                                error = CODEC_ReadRegister ( mapReceiverError, &mShadowRegs[mapReceiverError], 1 );
                                FailIf ( kIOReturnSuccess != error, Exit );
                                
                                dataIsValid = ( 0 == ( ( 1 << baINVALID ) & mShadowRegs[mapReceiverError] ) );
                                
                                if ( dataIsValid ) {
                                    mCodecHasLocked = TRUE;
                                    autoClockLockState = kAutoClockLockStateNormal;
                                }
                                else {
                                    debugIOLog ( 4, "  AppleTopazPluginCS8416::notifyHardwareEvent about to post kClockUnLockStatus due to invalid AES3 data ..." );
                                    mCodecHasLocked = FALSE;
                                    autoClockLockState = kAutoClockLockStateNormal;
                                }
                            }
                            else {
                                mCodecHasLocked = TRUE;
                                autoClockLockState = kAutoClockLockStateNormal;
                            }
                        }
                    }
                    else {
                        debugIOLog ( 4, "  AppleTopazPluginCS8416::notifyHardwareEvent about to post kClockUnLockStatus due to stable but invalid clock ratio ..." );
                        mCodecHasLocked = FALSE;
                        autoClockLockState = kAutoClockLockStateNormal;
                    }
                    
                    // [4189050] - By caching the ratio on every poll, we can differentiate between smooth slewing of the sampling rate and steps
                    mCached_OMCK_RMCK_Ratio = ratio;
                    
                    // [4176686]
                    mCachedReceiverChannelStatus = receiverChannelStatus;
                    
                    if (TRUE == mCodecHasLocked) {
                        debugIOLog ( 4, "  AppleTopazPluginCS8416::notifyHardwareEvent posts kClockLockStatus, OMCK/RMCK ratio = 0x%0.2X, receiver channel status = 0x%0.2X, auto clock lock state = %lu", ratio, receiverChannelStatus, autoClockLockState );
                        mAudioDeviceProvider->interruptEventHandler ( kClockLockStatus, autoClockLockState );
                    }
                    else {
                        debugIOLog ( 4, "  AppleTopazPluginCS8416::notifyHardwareEvent posts kClockUnLockStatus, OMCK/RMCK ratio = 0x%0.2X, receiver channel status = 0x%0.2X, auto clock lock state = %lu", ratio, receiverChannelStatus, autoClockLockState );
                        mAudioDeviceProvider->interruptEventHandler ( kClockUnLockStatus, autoClockLockState );
                    }
				}
                else if ( ( 0 == mDelayPollAfterWakeCounter ) && ( false == mOMCK_RMCK_RatioHasBeenCached ) )  // implies kPollsToDelayReportingAfterWake + 1 polls before we start reporting clock lock status
                {   // [4189050]
                    // Load ratio into cache for comparison upon next poll
                    error = CODEC_ReadRegister ( mapOMCK_RMCK_Ratio, &mShadowRegs[mapOMCK_RMCK_Ratio], 1 );
                    FailIf ( kIOReturnSuccess != error, Exit );
                    
                    mCached_OMCK_RMCK_Ratio = (UInt32) mShadowRegs[mapOMCK_RMCK_Ratio];
                    mOMCK_RMCK_RatioHasBeenCached = true;
                }
				break;
			case kCodecInterruptStatus:
				break;
		}
	}
	
Exit:
	switch ( statusSelector ) {
		case kCodecErrorInterruptStatus:	debugIOLog ( 4, "- AppleTopazPluginCS8416::notifyHardwareEvent ( %d = kCodecErrorInterruptStatus, %d )", statusSelector, newValue );	break;
		case kCodecInterruptStatus:			debugIOLog ( 5, "- AppleTopazPluginCS8416::notifyHardwareEvent ( %d = kCodecInterruptStatus, %d )", statusSelector, newValue );			break;
		default:							debugIOLog ( 5, "- AppleTopazPluginCS8416::notifyHardwareEvent ( %d, %d )", statusSelector, newValue );									break;
	}
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazPluginCS8416::decrementDelayPollAfterWakeCounter() {
    debugIOLog(5, "+ AppleTopazPluginCS8416::decrementDelayPollAfterWakeCounter()");
    
    if ((FALSE == mTopazCS8416isSLEEP) && (mDelayPollAfterWakeCounter > 0)) {
        mDelayPollAfterWakeCounter--;
    }
    
    debugIOLog(5, "- AppleTopazPluginCS8416::decrementDelayPollAfterWakeCounter()");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	AppleTopazPluginCS8416::getClockLock ( void ) {
	UInt32  result = ( ( mShadowRegs[mapReceiverError] & ( 1 << baUNLOCK ) ) == ( 1 << baUNLOCK ) ) ? FALSE : TRUE ;
	debugIOLog ( 5, "  ##### AppleTopazPluginCS8416::getClockLock() returns %d", result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTopazPluginCS8416::CODEC_GetRegSize ( UInt8 regAddr, UInt32 * codecRegSizePtr ) {
	IOReturn		result = kIOReturnError;
	
	if ( NULL != codecRegSizePtr ) {
		if ( kIOReturnSuccess == CODEC_IsStatusRegister ( regAddr ) || kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
			switch ( regAddr ) {
				case mapQChannelSubcode:	*codecRegSizePtr = ( mapQChannelSubcode_72_79 - mapQChannelSubcode_0_7 ) + 1;   break;
				case mapChannelAStatus:		*codecRegSizePtr = ( mapChannelAStatus_4 - mapChannelAStatus_0 ) + 1;			break;
				case mapChannelBStatus:		*codecRegSizePtr = ( mapChannelBStatus_4 - mapChannelBStatus_0 ) + 1;			break;
				case mapBurstPreamblePC:	*codecRegSizePtr = ( mapBurstPreamblePC_1 - mapBurstPreamblePC_0 ) + 1;			break;
				case mapBurstPreamblePD:	*codecRegSizePtr = ( mapBurstPreamblePD_1 - mapBurstPreamblePD_0 ) + 1;			break;
				default:					*codecRegSizePtr = 1;															break;
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	AppleTopazPluginCS8416::getClockLockTerminalCount () {
	UInt32			result = kClockLockFilterCountSeed;
	
	if ( mCodecHasLocked ) {
		result = kClockUnlockFilterCountSeed;
	}
	return result;
}


