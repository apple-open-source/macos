/*
 *  CS84xxFactory.cpp
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Wed Oct 22 2003.
 *  Copyright (c) 2003 AppleComputer, Inc. All rights reserved.
 *
 */

#include "AppleTopazPluginFactory.h"

#include "AppleTopazPluginCS8406.h"
#include "AppleTopazPluginCS8416.h"
#include "AppleTopazPluginCS8420.h"

OSDefineMetaClassAndStructors ( AppleTopazPluginFactory, OSObject )

AppleTopazPlugin * AppleTopazPluginFactory::createTopazPlugin ( TOPAZ_CODEC_TYPES codecType )
{
	AppleTopazPlugin * appleTopazPlugin;
	
	switch ( codecType ) {
		case kCS8406_CODEC:		appleTopazPlugin = new AppleTopazPluginCS8406();		break;
		case kCS8420_CODEC:		appleTopazPlugin = new AppleTopazPluginCS8420();		break;
		case kCS8416_CODEC:		appleTopazPlugin = new AppleTopazPluginCS8416();		break;
		default:				appleTopazPlugin = NULL;								break;
	}
	return appleTopazPlugin;
}
