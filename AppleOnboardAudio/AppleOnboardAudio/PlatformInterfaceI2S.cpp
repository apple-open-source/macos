/*
 *	PlatformInterfaceI2S.h
 *
 *	Defines base class for I2S support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
 
#include	"PlatformInterfaceI2S.h"
	
#define super OSObject

class AppleOnboardAudio;
	
OSDefineMetaClassAndStructors ( PlatformInterfaceI2S, OSObject )

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceI2S::init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	bool			result;
	
	result = super::init ();
	FailIf ( !result, Exit );
	
	mProvider = provider;
Exit:
	return result;
}


//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceI2S::free()
{
	super::free ();
}

