/*
 *	PlatformInterfaceI2C.h
 *
 *	Defines base class for I2C support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
 
#include	"PlatformInterfaceI2C.h"
	
#define super OSObject

OSDefineMetaClassAndStructors ( PlatformInterfaceI2C, OSObject )

class AppleOnboardAudio;
	
//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceI2C::init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	bool			result;
	
	result = super::init ();
	FailIf ( !result, Exit );
	
	mProvider = provider;
Exit:
	return result;
}


//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceI2C::free()
{
	super::free ();
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2C::getSavedMAP ( UInt32 codecRef )
{
	UInt32			result = 0xFFFFFFFF;
	
	FailIf ( kCodec_Unknown == codecRef, Exit );
	FailIf ( kCodec_NumberOfTypes <= codecRef, Exit );
	
	result = mMAPArray[codecRef];
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceI2C::saveMAP ( UInt32 codecRef, UInt32 map )
{
	FailIf ( kCodec_Unknown == codecRef, Exit );
	FailIf ( kCodec_NumberOfTypes <= codecRef, Exit );
	
	mMAPArray[codecRef] = map;
Exit:
	return;
}


