/*
 *  PlatformFCRFactory.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include <IOKit/IOService.h>
#include <libkern/c++/OSString.h>
#include	"AppleOnboardAudio.h"

#include "PlatformInterfaceFCR.h"

#ifndef __PlatformFCRFactory
#define	__PlatformFCRFactory

class PlatformFCRFactory : public OSObject {

    OSDeclareDefaultStructors ( PlatformFCRFactory );

public:	

	static PlatformInterfaceFCR * createPlatform ( const OSString* inPlatformSupportString );

private:
	
	static const char * platformFCRMappedString;
	static const char * platformFCRPlatformFunctionString;
	
};

#endif
