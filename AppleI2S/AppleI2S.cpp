/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include "AppleI2S.h"
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>

#define super IOService

OSDefineMetaClassAndStructors(AppleI2S, IOService)

bool AppleI2S::init(OSDictionary *dict)
{
	keyLargoDrv		= 0;
	i2sBaseAddress	= 0;

	return super::init(dict);
}

void AppleI2S::free(void)
{
    super::free();
}

void AppleI2S::stop(IOService *provider)
{
	keyLargoDrv		= 0;
	i2sBaseAddress	= 0;
	
    super::stop(provider);
}


IOService *AppleI2S::probe(IOService *provider, SInt32 *score)
{
	// Nothing to do -- name match is sufficient.
	// Personality specifies probe score of 10000.
    return(this);
}

bool AppleI2S::start(IOService *nub)
{
	OSData 					*regprop;
	IOService				*parentDev;
	bool	temp;
	
	if (!super::start(nub)) 
		return(false);
			
	// Find the i2s parent (should be mac-io device, leading to keyLargo driver)
	// Traverse planes to insure compatibility with older machines if needed.
	parentDev = OSDynamicCast(IOService, nub->getParentEntry(gIODTPlane));
	if (!parentDev) return(false);

	keyLargoDrv = OSDynamicCast(IOService, parentDev->getChildEntry(gIOServicePlane));
	if (!keyLargoDrv) return(false);

	// Get i2s register base address
	if ((regprop = OSDynamicCast(OSData, nub->getProperty("reg"))) == 0)
		return(false);
	else
		i2sBaseAddress  = *(UInt32 *)regprop->getBytesNoCopy();

	// start matching on i2s sub nodes and publish ourself
	//if (nub->getProperty("preserveIODeviceTree") != 0)
	//	{
		temp = nub->callPlatformFunction("mac-io-publishChildren",0,(void*)this,
                                                        (void*)0,(void*)0,(void*)0);
	//	}
		
	publishResource("AppleI2S", this);

	return(true);
}

IOReturn AppleI2S::callPlatformFunction( const OSSymbol *functionSymbol,
                                       bool waitForFunction,
                                       void *param1, void *param2,
                                       void *param3, void *param4 ) 
{
    const char *functionName = functionSymbol->getCStringNoCopy();   // for 3203380

	IOReturn	result = kIOReturnUnsupported;
	IOService	*provider = getProvider();
	UInt32		offset;
	UInt32		regval;

	offset = (UInt32) noMask;  // Fail value for no matching string

	//Check Read cases
	if (strcmp(functionName, kI2SGetIntCtlReg) == 0)
		offset = kI2SIntCtlOffset;
		
	else if (strcmp(functionName, kI2SGetSerialFormatReg) == 0)
		offset = kI2SSerialFormatOffset;
		
	else if (strcmp(functionName, kI2SGetCodecMsgOutReg) == 0)
		offset = kI2SCodecMsgOutOffset;
		
	else if (strcmp(functionName, kI2SGetCodecMsgInReg) == 0)
		offset = kI2SCodecMsgInOffset;
		
	else if (strcmp(functionName, kI2SGetFrameCountReg) == 0)
		offset = kI2SFrameCountOffset;
		
	else if (strcmp(functionName, kI2SGetFrameMatchReg) == 0)
		offset = kI2SFrameMatchOffset;
		
	else if (strcmp(functionName, kI2SGetDataWordSizesReg) == 0)
		offset = kI2SDataWordSizesOffset;
		
	else if (strcmp(functionName, kI2SGetPeakLevelSelReg) == 0)
		offset = kI2SPeakLevelSelOffset;
		
	else if (strcmp(functionName, kI2SGetPeakLevelIn0Reg) == 0)
		offset = kI2SPeakLevelIn0Offset;
		
	else if (strcmp(functionName, kI2SGetPeakLevelIn1Reg) == 0)
		offset = kI2SPeakLevelIn1Offset;
		
	if (offset != noMask)  //found Read match
		{
			offset += (UInt32)param1;    // reg value for I2Sa or I2Sb
			offset += i2sBaseAddress;   // reg value for I2S, e.g., 0x10000

			result = keyLargoDrv->callPlatformFunction(kSafeReadRegUInt32,
				false, (void *)offset, (void *)&regval, 0, 0);

			// if the read succeeded, return
			if (result == kIOReturnSuccess)
			{
				*(UInt32 *)param2 = (UInt32)regval;
				return result;
			}
		}
			
	// Write cases
	if (strcmp(functionName, kI2SSetIntCtlReg) == 0)
		offset = kI2SIntCtlOffset;
		
	else if (strcmp(functionName, kI2SSetSerialFormatReg) == 0)
		offset = kI2SSerialFormatOffset;
		
	else if (strcmp(functionName, kI2SSetCodecMsgOutReg) == 0)
		offset = kI2SCodecMsgOutOffset;
		
	else if (strcmp(functionName, kI2SSetCodecMsgInReg) == 0)
		offset = kI2SCodecMsgInOffset;
		
	else if (strcmp(functionName, kI2SSetFrameCountReg) == 0)
		offset = kI2SFrameCountOffset;
		
	else if (strcmp(functionName, kI2SSetFrameMatchReg) == 0)
		offset = kI2SFrameMatchOffset;
		
	else if (strcmp(functionName, kI2SSetDataWordSizesReg) == 0)
		offset = kI2SDataWordSizesOffset;
		
	else if (strcmp(functionName, kI2SSetPeakLevelSelReg) == 0)
		offset = kI2SPeakLevelSelOffset;
		
	else if (strcmp(functionName, kI2SSetPeakLevelIn0Reg) == 0)
		offset = kI2SPeakLevelIn0Offset;
		
	else if (strcmp(functionName, kI2SSetPeakLevelIn1Reg) == 0)
		offset = kI2SPeakLevelIn1Offset;
		
	if (offset != noMask)  //found Write match
		{
			offset += (UInt32)param1;    // reg value for I2Sa or I2Sb
			offset += i2sBaseAddress;   // reg value for I2S, e.g., 0x10000
			regval = (UInt32)param2;

		// This call does a locked read-modify-write on key largo
		result = keyLargoDrv->callPlatformFunction(kSafeWriteRegUInt32,
				false, (void *)offset, (void *)noMask, (void *)param2, 0);
		}

	if (offset == noMask)  //no match at all...
	{
		if (provider != 0)
			result = provider->callPlatformFunction(functionSymbol, waitForFunction,
					param1, param2, param3, param4);
	}
		
	return result;
}

IOReturn AppleI2S::callPlatformFunction( const char *functionName,
                                       bool waitForFunction,
                                       void *param1, void *param2,
                                       void *param3, void *param4 )
{
    // just pass it along; avoids 3203380
    return super::callPlatformFunction(functionName, waitForFunction,
                                       param1, param2, param3, param4);
}

