/*
 *	PlatformInterfaceGPIO.h
 *
 *	Defines base class for GPIO support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include	"PlatformInterfaceGPIO.h"

#define super OSObject
	
class AppleOnboardAudio;
	
OSDefineMetaClassAndStructors ( PlatformInterfaceGPIO, OSObject )

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceGPIO::init ( IOService * device, AppleOnboardAudio * provider, UInt32 inDBDMADeviceIndex )
{
	bool			result;
	
	result = super::init ();
	FailIf ( !result, Exit );
	
	mProvider = provider;
Exit:
	return result;
}


//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceGPIO::free ()
{
	super::free ();
}

