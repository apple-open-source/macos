/*
 *  AppleTopazPlugin.cpp
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Tue Oct 07 2003.
 *  Copyright (c) 2003 AppleComputer, Inc. All rights reserved.
 *
 */

#include "AppleTopazPlugin.h"
#include "CS8420_hw.h"

#define super OSObject

#pragma mark ---------------------
#pragma mark ¥ CODEC Functions
#pragma mark ---------------------

OSDefineMetaClassAndStructors ( AppleTopazPlugin, super )

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTopazPlugin::init ( OSDictionary *properties ) {
	return super::init ();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazPlugin::initPlugin ( PlatformInterface * inPlatformObject ) {
	mPlatformInterface = inPlatformObject;
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTopazPlugin::free ( void ) {
	super::free ();
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Read operations can source data from the device or from a write through
//	cache.  By specifying a 'mode' of kTOPAZ_AccessMode_FORCE_UPDATE_ALL, the device will be
//	accessed.  The size of the access may be passed in.  For most cases, the
//	size will be a single byte.  Channel status or user status may stipulate
//	a size of 24 bytes to optimize the register access.
IOReturn 	AppleTopazPlugin::CODEC_ReadRegister ( UInt8 regAddr, UInt8 * registerData, UInt32 size ) {
	IOReturn		result;
	Boolean			success;
	UInt32			index;
	UInt32			codecRegSize;

	result = kIOReturnError;
	success = false;
	FailIf ( NULL == mPlatformInterface, Exit );
	result = kIOReturnSuccess;
	if ( 1 < size ) { regAddr |= (UInt8)kMAP_AUTO_INCREMENT_ENABLE; }
	
	if ( kIOReturnSuccess == CODEC_GetRegSize ( regAddr, &codecRegSize ) ) {
		if ( 0 != size && size <= codecRegSize ) {
			//	Write through to the shadow register as a 'write through' cache would and
			//	then write the data to the hardware;
			if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) || kIOReturnSuccess == CODEC_IsStatusRegister ( regAddr ) ) {
				success = true;
				//	Performance optimization:	If the memory address pointer (MAP) has already
				//								been set then there is no need to set it again.
				//								This is important for interrupt services!!!
				if ( regAddr != mCurrentMAP ) {
					//	Must write the MAP register prior to performing the READ access
					result = setMemoryAddressPointer ( regAddr );
					FailIf ( kIOReturnSuccess != result, Exit );
				}
				//	Always read data into the cache.
				success = mPlatformInterface->readCodecRegister(kCS84xx_I2C_ADDRESS, 0, &mShadowRegs[regAddr & ~kMAP_AUTO_INCREMENT_ENABLE], size, kI2C_StandardMode);
				FailIf ( !success, Exit );
				//	Then return data from the cache.
				if ( NULL != registerData && success ) {
					for ( index = 0; index < size; index++ ) {
						registerData[index] = mShadowRegs[(regAddr & ~kMAP_AUTO_INCREMENT_ENABLE) + index];
					}
				}
			} else {
				debugIOLog (7,  "not a control or status register at register %X", regAddr );
			}
		} else {
			debugIOLog (7,  "codec register size is invalid" );
		}
	}
 
Exit:	
	if ( !success ) { result = kIOReturnError; }
	if ( kIOReturnSuccess != result ) {
		if ( NULL == registerData ) {
			debugIOLog (7, "-AppleTopazPlugin::CODEC_ReadRegister regAddr = %X registerData = %p size = %ul, returns %d", regAddr, registerData, (unsigned int)size, result);
		} else {
			debugIOLog (7, "-AppleTopazPlugin::CODEC_ReadRegister regAddr = %X registerData = %X size = %ul, returns %d", regAddr, *registerData, (unsigned int)size, result);
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	All CODEC write operations pass through this function so that a shadow
//	copy of the registers can be kept in global storage.  All CS8420 control
//	registers are one byte in length.
IOReturn 	AppleTopazPlugin::CODEC_WriteRegister ( UInt8 regAddr, UInt8 registerData ) {
	IOReturn		result;
	Boolean			updateRequired;
	bool			success = true;

	result = kIOReturnError;
	FailIf ( NULL == mPlatformInterface, Exit );
	updateRequired = false;

	debugIOLog (7,  "<>< AppleTopazPlugin::CODEC_WriteRegister ( regAddr %X, registerData %X )", regAddr, registerData );

	//	Write through to the shadow register as a 'write through' cache would and
	//	then write the data to the hardware;
	if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
		registerData &= CODEC_GetDataMask ( regAddr );
		mCurrentMAP = regAddr;
		success = mPlatformInterface->writeCodecRegister( kCS84xx_I2C_ADDRESS, regAddr, &registerData, 1, kI2C_StandardSubMode );
		FailIf ( !success, Exit );
		mShadowRegs[regAddr] = registerData;
	}
	result = kIOReturnSuccess;
	
Exit:
	if ( !success ) { result = kIOReturnError; }
	if ( kIOReturnSuccess != result && !mRecoveryInProcess) {
		debugIOLog (7,  "AppleTopazPlugin::CODEC_WriteRegister ( regAddr %X, registerData %X ) result = %X", regAddr, registerData, result );
		if ( mAudioDeviceProvider ) {
			mAudioDeviceProvider->interruptEventHandler ( kRequestCodecRecoveryStatus, (UInt32)kControlBusFatalErrorRecovery );
		}
	}
    return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt8 	AppleTopazPlugin::getMemoryAddressPointer ( void ) { 
	return mCurrentMAP; 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn 	AppleTopazPlugin::setMemoryAddressPointer ( UInt8 map ) { 
	IOReturn		result = kIOReturnError;
	
	if ( mPlatformInterface->writeCodecRegister( kCS84xx_I2C_ADDRESS, 0, &map, 1, kI2C_StandardMode) ) {
		result = kIOReturnSuccess;
		mCurrentMAP = map;
	}
	return result; 
}
