/*
 *	PlatformInterfaceFCR.h
 *
 *	Defines base class for FCR support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include	"PlatformInterfaceFCR.h"

#define super OSObject
	
OSDefineMetaClassAndStructors ( PlatformInterfaceFCR, OSObject )

class AppleOnboardAudio;
	
//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceFCR::init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	bool			result;
	
	result = super::init ();
	FailIf ( !result, Exit );
	
	mProvider = provider;
Exit:
	return result;
}


//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceFCR::free()
{
	super::free ();
}

