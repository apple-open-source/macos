/*
 *  PlatformI2SFactory.cpp
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include "PlatformI2SFactory.h"

#include <PlatformInterfaceI2S_PlatformFunction.h>
#include <PlatformInterfaceI2S_Mapped.h>

OSDefineMetaClassAndStructors ( PlatformI2SFactory, OSObject )

const char * PlatformI2SFactory::platformI2SMappedString			= kPlatformI2SMappedString;
const char * PlatformI2SFactory::platformI2SPlatformFunctionString	= kPlatformI2SPlatformFunctionString;

PlatformInterfaceI2S * PlatformI2SFactory::createPlatform ( const OSString* inPlatformSupportString )
{
	PlatformInterfaceI2S * thePlatformI2SSupportObject;
	
	debugIOLog ( 4, "+ PlatformI2SFactory::createPlatform ( inPlatformSupportString %p = '%s' )", inPlatformSupportString, inPlatformSupportString->getCStringNoCopy () );
	thePlatformI2SSupportObject = NULL;
	FailIf ( 0 == inPlatformSupportString, Exit );
	
	if ( inPlatformSupportString->isEqualTo ( platformI2SMappedString ) )
	{
		thePlatformI2SSupportObject = new PlatformInterfaceI2S_Mapped ();
	}
	else if ( inPlatformSupportString->isEqualTo ( platformI2SPlatformFunctionString ) )
	{
		thePlatformI2SSupportObject = new PlatformInterfaceI2S_PlatformFunction ();
	}
Exit:
	debugIOLog ( 4, "- PlatformI2SFactory::createPlatform ( %p ) returns %p", inPlatformSupportString, thePlatformI2SSupportObject );
	return thePlatformI2SSupportObject;
}
