/*
 *	PlatformInterfaceDBDMA.cpp
 *
 *	Defines base class for DBDMA support within the context of PlatformInterface derived classes.
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
 
#include	"PlatformInterfaceDBDMA.h"
	
#define super OSObject

OSDefineMetaClassAndStructors ( PlatformInterfaceDBDMA, OSObject )

class AppleOnboardAudio;
	
//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceDBDMA::init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	bool			result;
	
	result = super::init ();
	FailIf ( !result, Exit );
	
	mProvider = provider;
Exit:
	return result;
}


//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceDBDMA::free()
{
	super::free ();
}

