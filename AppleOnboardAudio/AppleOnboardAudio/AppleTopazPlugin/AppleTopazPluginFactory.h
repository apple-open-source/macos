/*
 *  CS84xxFactory.h
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Wed Oct 22 2003.
 *  Copyright (c) 2003 AppleComputer, Inc. All rights reserved.
 *
 */

#include <IOKit/IOService.h>
#include <libkern/c++/OSString.h>
#include "AppleTopazPlugin.h"

#ifndef __AppleTopazPluginFactory
#define __AppleTopazPluginFactory

class AppleTopazPluginFactory : public OSObject {

    OSDeclareDefaultStructors ( AppleTopazPluginFactory );

public:	

	static AppleTopazPlugin * createTopazPlugin ( TOPAZ_CODEC_TYPES codecType );

private:
	
	
};

#endif
