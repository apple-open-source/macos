/*
 *  PlatformGPIOFactory.cpp
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include "PlatformGPIOFactory.h"

#include <PlatformInterfaceGPIO_PlatformFunction.h>
#include <PlatformInterfaceGPIO_Mapped.h>

OSDefineMetaClassAndStructors ( PlatformGPIOFactory, OSObject )

const char * PlatformGPIOFactory::platformGPIOMappedString				= "GPIO_Mapped";
const char * PlatformGPIOFactory::platformGPIOPlatformFunctionString	= "GPIO_PlatformFunction";

PlatformInterfaceGPIO * PlatformGPIOFactory::createPlatform ( const OSString* inPlatformSupportString )
{
	PlatformInterfaceGPIO * thePlatformGPIOSupportObject;
	
	debugIOLog ( 4, "+ PlatformGPIOFactory::createPlatform ( inPlatformSupportString %p = '%s' )", inPlatformSupportString, inPlatformSupportString->getCStringNoCopy () );
	thePlatformGPIOSupportObject = NULL;
	FailIf ( 0 == inPlatformSupportString, Exit );
	
	if ( inPlatformSupportString->isEqualTo ( platformGPIOMappedString ) )
	{
		thePlatformGPIOSupportObject = new PlatformInterfaceGPIO_Mapped ();
	}
	else if ( inPlatformSupportString->isEqualTo ( platformGPIOPlatformFunctionString ) )
	{
		thePlatformGPIOSupportObject = new PlatformInterfaceGPIO_PlatformFunction ();
	}
	debugIOLog ( 4, "- PlatformGPIOFactory::createPlatform ( %p ) returns %p", inPlatformSupportString, thePlatformGPIOSupportObject );
Exit:
	return thePlatformGPIOSupportObject;
}
