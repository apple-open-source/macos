/*
 *  AppleDallasDriver.h
 *  AppleDallasDriver
 *
 *  Created by Keith Cox on Tue Jul 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
#include <IOKit/IOService.h>

class AppleDallasDriver : public IOService
{
OSDeclareDefaultStructors(AppleDallasDriver)
public:
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);
	virtual bool readROM(UInt8 *bROM, UInt8 *bEEPROM, UInt8 *bAppReg);
	virtual bool getSpeakerID (UInt8 *bROM, UInt8 *bEEPROM, UInt8 *bAppReg);

protected:
    IODeviceMemory  *gpioRegMem;
};