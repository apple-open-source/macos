/*
 *  PlatformGPIOFactory.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include <IOKit/IOService.h>
#include <libkern/c++/OSString.h>
#include	"AppleOnboardAudio.h"

#include "PlatformInterfaceGPIO.h"

#ifndef __PlatformGPIOFactory
#define	__PlatformGPIOFactory

class PlatformGPIOFactory : public OSObject {

    OSDeclareDefaultStructors ( PlatformGPIOFactory );

public:	

	static PlatformInterfaceGPIO * createPlatform ( const OSString* inPlatformSupportString );

private:
	
	static const char * platformGPIOMappedString;
	static const char * platformGPIOPlatformFunctionString;
	
};

#endif
