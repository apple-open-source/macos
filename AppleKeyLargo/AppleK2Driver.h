/*
 *  AppleK2Driver.h
 *  AppleKeyLargo
 *
 *  Created by Joseph Lehrer on Mon Dec 16 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _APPLEK2DRIVER_H
#define _APPLEK2DRIVER_H

#include "KeyLargo.h"

class AppleK2Driver : public IOService
{
	OSDeclareDefaultStructors (AppleK2Driver);
private:
public:
	virtual IOService *probe (IOService *provider, SInt32 *score);
	virtual bool start (IOService *provider);

};

#endif /* _APPLEK2DRIVER_H */