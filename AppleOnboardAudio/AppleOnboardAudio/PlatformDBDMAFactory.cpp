/*
 *  PlatformDBDMAFactory.cpp
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include "PlatformDBDMAFactory.h"

#include <PlatformInterfaceDBDMA_Mapped.h>
#include <PlatformInterfaceDBDMA_PlatformFunction.h>
#include <PlatformInterfaceDBDMA_PlatformFunction_K2.h>

OSDefineMetaClassAndStructors ( PlatformDBDMAFactory, OSObject )

const char * PlatformDBDMAFactory::platformDBDMAMappedString					= kPlatformDBDMAMappedString;
const char * PlatformDBDMAFactory::platformDBDMAPlatformFunctionString			= kPlatformDBDMAPlatformFunctionString;
const char * PlatformDBDMAFactory::platformDBDMAPlatformFunctionK2String		= kPlatformDBDMAPlatformFunctionK2String;

PlatformInterfaceDBDMA * PlatformDBDMAFactory::createPlatform ( const OSString * inPlatformSupportString )
{
	PlatformInterfaceDBDMA * thePlatformDBDMASupportObject;
	
	debugIOLog ( 4, "+ PlatformDBDMAFactory::createPlatform ( inPlatformSupportString %p = '%s' )", inPlatformSupportString, inPlatformSupportString->getCStringNoCopy () );
	thePlatformDBDMASupportObject = NULL;
	FailIf ( 0 == inPlatformSupportString, Exit );
	
	if ( inPlatformSupportString->isEqualTo ( platformDBDMAMappedString ) )
	{
		thePlatformDBDMASupportObject = new PlatformInterfaceDBDMA_Mapped ();
	}
	else if ( inPlatformSupportString->isEqualTo ( platformDBDMAPlatformFunctionString ) )
	{
		thePlatformDBDMASupportObject = new PlatformInterfaceDBDMA_PlatformFunction ();
	}
	else if ( inPlatformSupportString->isEqualTo ( platformDBDMAPlatformFunctionK2String ) )
	{
		thePlatformDBDMASupportObject = new PlatformInterfaceDBDMA_PlatformFunctionK2 ();
	}
Exit:
	debugIOLog ( 4, "- PlatformDBDMAFactory::createPlatform ( %p ) returns %p", inPlatformSupportString, thePlatformDBDMASupportObject );
	return thePlatformDBDMASupportObject;
}
