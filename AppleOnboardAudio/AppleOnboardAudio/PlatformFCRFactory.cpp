/*
 *  PlatformFCRFactory.cpp
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include "PlatformFCRFactory.h"

#include <PlatformInterfaceFCR_PlatformFunction.h>
#include <PlatformInterfaceFCR_Mapped.h>

OSDefineMetaClassAndStructors ( PlatformFCRFactory, OSObject )

const char * PlatformFCRFactory::platformFCRMappedString			= kPlatformFCRMappedString;
const char * PlatformFCRFactory::platformFCRPlatformFunctionString	= kPlatformFCRPlatformFunctionString;

PlatformInterfaceFCR * PlatformFCRFactory::createPlatform ( const OSString* inPlatformSupportString )
{
	PlatformInterfaceFCR * thePlatformFCRSupportObject;
	
	debugIOLog ( 4, "+ PlatformFCRFactory::createPlatform ( inPlatformSupportString %p = '%s' )", inPlatformSupportString, inPlatformSupportString->getCStringNoCopy () );
	thePlatformFCRSupportObject = NULL;
	FailIf ( 0 == inPlatformSupportString, Exit );
	
	if ( inPlatformSupportString->isEqualTo ( platformFCRMappedString ) )
	{
		thePlatformFCRSupportObject = new PlatformInterfaceFCR_Mapped ();
	}
	else if ( inPlatformSupportString->isEqualTo ( platformFCRPlatformFunctionString ) )
	{
		thePlatformFCRSupportObject = new PlatformInterfaceFCR_PlatformFunction ();
	}
Exit:
	debugIOLog ( 4, "- PlatformFCRFactory::createPlatform ( %p ) returns %p", inPlatformSupportString, thePlatformFCRSupportObject );
	return thePlatformFCRSupportObject;
}
