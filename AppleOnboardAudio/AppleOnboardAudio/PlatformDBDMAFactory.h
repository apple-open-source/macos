/*
 *  PlatformDBDMAFactory.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne Mon 20 Sept 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#include	<IOKit/IOService.h>
#include	<libkern/c++/OSString.h>
#include	"AppleOnboardAudio.h"

#include "PlatformInterfaceDBDMA.h"

#ifndef __PlatformDBDMAFactory
#define	__PlatformDBDMAFactory

class PlatformDBDMAFactory : public OSObject {

    OSDeclareDefaultStructors ( PlatformDBDMAFactory );

public:	

	static PlatformInterfaceDBDMA * createPlatform ( const OSString* inPlatformSupportString );

private:
	
	static const char * platformDBDMAMappedString;
	static const char * platformDBDMAPlatformFunctionString;
	static const char * platformDBDMAPlatformFunctionK2String;
	
};

#endif