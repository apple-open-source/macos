/*
 *  PlatformI2CFactory.cpp
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include "PlatformI2CFactory.h"

#include <PlatformInterfaceI2C_PlatformFunction.h>
#include <PlatformInterfaceI2C_Mapped.h>

OSDefineMetaClassAndStructors ( PlatformI2CFactory, OSObject )

const char * PlatformI2CFactory::platformI2CMappedString			= kPlatformI2CMappedString;
const char * PlatformI2CFactory::platformI2CPlatformFunctionString	= kPlatformI2CPlatformFunctionString;

PlatformInterfaceI2C * PlatformI2CFactory::createPlatform ( const OSString* inPlatformSupportString )
{
	PlatformInterfaceI2C * thePlatformI2CSupportObject;
	
	debugIOLog ( 4, "+ PlatformI2CFactory::createPlatform ( inPlatformSupportString %p = '%s' )", inPlatformSupportString, inPlatformSupportString->getCStringNoCopy () );
	thePlatformI2CSupportObject = NULL;
	FailIf ( 0 == inPlatformSupportString, Exit );
	
	if ( inPlatformSupportString->isEqualTo ( platformI2CMappedString ) )
	{
		thePlatformI2CSupportObject = new PlatformInterfaceI2C_Mapped ();
	}
	else if ( inPlatformSupportString->isEqualTo ( platformI2CPlatformFunctionString ) )
	{
		thePlatformI2CSupportObject = new PlatformInterfaceI2C_PlatformFunction ();
	}
Exit:
	debugIOLog ( 4, "- PlatformI2CFactory::createPlatform ( %p ) returns %p", inPlatformSupportString, thePlatformI2CSupportObject );
	return thePlatformI2CSupportObject;
}
