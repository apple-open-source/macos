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
#include <I2SSlaveOnlyTranportInterface.h>
#include <I2SOpaqueSlaveOnlyTransportInterface.h>

const char* TransportFactory::I2SString = "i2s";
const char* TransportFactory::I2SSlaveOnlyString = "i2sSlaveOnly";
const char* TransportFactory::I2SOpaqueSlaveOnlyString = "i2sOpaqueSlaveOnly";


TransportInterface* TransportFactory::createTransport ( const OSString* inTransportString )
{
	TransportInterface* theTransportObject;
	
	theTransportObject = NULL;
	
	if ( inTransportString->isEqualTo ( I2SString ) ) {
		theTransportObject = new I2STransportInterface();
	} else if ( inTransportString->isEqualTo ( I2SSlaveOnlyString ) ) {
		theTransportObject = new I2SSlaveOnlyTransportInterface();
	} else if ( inTransportString->isEqualTo ( I2SOpaqueSlaveOnlyString ) ) {
		theTransportObject = new I2SOpaqueSlaveOnlyTransportInterface();
	} 
	return theTransportObject;
}
