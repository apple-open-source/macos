/*
 *  TransportFactory.h
 *  AppleOnboardAudio
 *
 *  Created by Ray Montagne on Wed Mar 17 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include <IOKit/IOService.h>
#include <libkern/c++/OSString.h>

#include "TransportInterface.h"

class TransportFactory : public OSObject {

    OSDeclareDefaultStructors(TransportFactory);

public:	

	static TransportInterface* createTransport(const OSString* inTransportString);

private:
	
	static const char* I2SString;
	static const char* I2SSlaveOnlyString;
	
};
