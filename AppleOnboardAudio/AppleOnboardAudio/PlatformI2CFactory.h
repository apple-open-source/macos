/*
 *  PlatformI2CFactory.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include <IOKit/IOService.h>
#include <libkern/c++/OSString.h>
#include	"AppleOnboardAudio.h"

#include "PlatformInterfaceI2C.h"

#ifndef __PlatformI2CFactory
#define	__PlatformI2CFactory

class PlatformI2CFactory : public OSObject {

    OSDeclareDefaultStructors ( PlatformI2CFactory );

public:	

	static PlatformInterfaceI2C * createPlatform ( const OSString* inPlatformSupportString );

private:
	
	static const char * platformI2CMappedString;
	static const char * platformI2CPlatformFunctionString;
	
};

#endif
