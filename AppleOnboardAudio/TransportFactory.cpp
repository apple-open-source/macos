/*
 *  TransportFactory.cpp
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne on Wed Mar 17 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include "TransportFactory.h"

#include <I2STransportInterface.h>

const char* TransportFactory::I2SString = "i2s";

TransportInterface* TransportFactory::createTransport ( const OSString* inTransportString )
{
	TransportInterface* theTransportObject;
	
	theTransportObject = NULL;
	
	if ( inTransportString->isEqualTo ( I2SString ) ) 
	{
		theTransportObject = new I2STransportInterface();
	} 
	return theTransportObject;
}
