/*
 *  PlatformFactory.h
 *  AppleOnboardAudio
 *
 *  Created by Aram Lindahl on Wed Mar 12 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include <IOKit/IOService.h>
#include <libkern/c++/OSString.h>

#include "PlatformInterface.h"

class PlatformFactory : public OSObject {

    OSDeclareDefaultStructors(PlatformFactory);

public:	

	static PlatformInterface* createPlatform(const OSString* inPlatformString);

private:
	
	static const char* KeyLargoString;
	static const char* K2String;
	
};