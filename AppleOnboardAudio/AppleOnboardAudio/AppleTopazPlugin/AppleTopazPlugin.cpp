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
void AppleTopazPlugin::initPlugin ( PlatformInterface* inPlatformObject, IOService * provider, HardwarePluginType codecID ) {
	debugIOLog ( 3, "+ AppleTopazPlugin::initPlugin ( %p, %p, 0x%0.8X )", inPlatformObject, provider, codecID );
	initPlugin ( inPlatformObject );
	mAudioDeviceProvider = (AppleOnboardAudio *)provider;
	mCodecID = codecID;
	debugIOLog ( 3, "- AppleTopazPlugin::initPlugin ( %p, %p, 0x%0.8X )", inPlatformObject, provider, codecID );
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
	UInt32			index;
	UInt32			codecRegSize;

	debugIOLog ( 6, "+ AppleTopazPlugin::CODEC_ReadRegister ( %X, %p, %d )", regAddr, registerData, size );

	result = kIOReturnError;
	FailIf ( NULL == mPlatformInterface, Exit );
	FailIf ( ( kCodec_CS8406 != mCodecID ) && ( kCodec_CS8416 != mCodecID ) && ( kCodec_CS8420 != mCodecID ), Exit );
	result = kIOReturnSuccess;
	if ( 1 < size ) { regAddr |= (UInt8)kMAP_AUTO_INCREMENT_ENABLE; }
	
	if ( kIOReturnSuccess == CODEC_GetRegSize ( regAddr, &codecRegSize ) )
	{
		if ( 0 != size && size <= codecRegSize )
		{
			//	Write through to the shadow register as a 'write through' cache would and
			//	then write the data to the hardware;
			if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) || kIOReturnSuccess == CODEC_IsStatusRegister ( regAddr ) )
			{
				//	Always read data into the cache.
				result = mPlatformInterface->readCodecRegister ( mCodecID, regAddr, &mShadowRegs[regAddr & ~kMAP_AUTO_INCREMENT_ENABLE], 1 );	//  [3648867]
				if ( kIOReturnSuccess != result )
				{
					debugIOLog ( 6, "  mPlatformInterface->readCodecRegister ( %x, %x, %p, 1 ) returns %lX", mCodecID, regAddr, &mShadowRegs[regAddr & ~kMAP_AUTO_INCREMENT_ENABLE], result );
				}
				FailIf ( kIOReturnSuccess != result, Exit );	//  [3648867]
				//	Then return data from the cache.
				if ( 0 != registerData ) {
					for ( index = 0; index < size; index++ )
					{
						registerData[index] = mShadowRegs[(regAddr & ~kMAP_AUTO_INCREMENT_ENABLE) + index];
					}
				}
			}
			else
			{
				debugIOLog (6,  "  *** not a control or status register" );
			}
		}
		else
		{
			debugIOLog (6,  "  *** codec register size is invalid" );
		}
	}
	else
	{
		debugIOLog (6,  "  *** codec register is invalid" );
	}
 
Exit:	
	debugIOLog ( 6, "- AppleTopazPlugin::CODEC_ReadRegister ( %X, %p, %d ) returns %lX", regAddr, registerData, size, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	All CODEC write operations pass through this function so that a shadow
//	copy of the registers can be kept in global storage.  All CS8420 control
//	registers are one byte in length.
IOReturn 	AppleTopazPlugin::CODEC_WriteRegister ( UInt8 regAddr, UInt8 registerData ) {
	IOReturn		result;
	Boolean			updateRequired;

	result = kIOReturnError;
	FailIf ( NULL == mPlatformInterface, Exit );
	updateRequired = false;

	//	Write through to the shadow register as a 'write through' cache would and
	//	then write the data to the hardware;
	if ( kIOReturnSuccess == CODEC_IsControlRegister ( regAddr ) ) {
		registerData &= CODEC_GetDataMask ( regAddr );
		result = mPlatformInterface->writeCodecRegister( mCodecID, regAddr, &registerData, 1 );	//  [3648867]
		FailIf ( kIOReturnSuccess != result, Exit );
		mShadowRegs[regAddr] = registerData;
	}
	
Exit:
	if ( kIOReturnSuccess != result ) {
		if ( mAudioDeviceProvider ) {
			mAudioDeviceProvider->interruptEventHandler ( kRequestCodecRecoveryStatus, (UInt32)kControlBusFatalErrorRecovery );
		}
	}
    return result;
}

