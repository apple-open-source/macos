/*
 *  PlatformFactory.cpp
 *  AppleOnboardAudio
 *
 *  Created by Aram Lindahlon Wed Mar 12 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include "PlatformFactory.h"

#include <KeyLargoPlatform.h>
#include <K2Platform.h>
#include <ShastaPlatform.h>

const char* PlatformFactory::KeyLargoString = "KeyLargo";
const char* PlatformFactory::K2String = "K2";
const char* PlatformFactory::ShastaString = "Shasta";

PlatformInterface* PlatformFactory::createPlatform(const OSString* inPlatformString)
{
	PlatformInterface* thePlatformObject;
	
	thePlatformObject = NULL;
	
	if (inPlatformString->isEqualTo(K2String)) 
	{
		thePlatformObject = new K2Platform();
	} 
	else if (inPlatformString->isEqualTo(KeyLargoString)) 
	{
		thePlatformObject = new KeyLargoPlatform();
	}
	else if (inPlatformString->isEqualTo(ShastaString)) 
	{
		thePlatformObject = new ShastaPlatform();
	}
	
	return thePlatformObject;
}
