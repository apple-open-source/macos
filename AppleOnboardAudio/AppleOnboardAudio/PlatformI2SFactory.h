/*
 *  PlatformI2SFactory.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include <IOKit/IOService.h>
#include <libkern/c++/OSString.h>
#include	"AppleOnboardAudio.h"

#include "PlatformInterfaceI2S.h"

#ifndef __PlatformI2SFactory
#define	__PlatformI2SFactory

class PlatformI2SFactory : public OSObject {

    OSDeclareDefaultStructors ( PlatformI2SFactory );

public:	

	static PlatformInterfaceI2S * createPlatform ( const OSString* inPlatformSupportString );

private:
	
	static const char * platformI2SMappedString;
	static const char * platformI2SPlatformFunctionString;
	
};

#endif
