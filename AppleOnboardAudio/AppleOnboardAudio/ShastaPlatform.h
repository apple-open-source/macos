/*
 *  ShastaPlatform.h
 *  
 *
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 *
 */
#include "K2Platform.h"

#ifndef _SHASTA_PLATFORM_H
#define _SHASTA_PLATFORM_H

class ShastaPlatform : public K2Platform {

    OSDeclareDefaultStructors(ShastaPlatform);

public:

	virtual bool				init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex);
	virtual	void				free();
	//
	// DBDMA Memory Address Acquisition Methods
	//
	virtual	IODBDMAChannelRegisters *	GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );
	virtual	IODBDMAChannelRegisters *	GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider );

	virtual PlatformInterfaceObjectType	getPlatformInterfaceType () { return kPlatformInterfaceType_Shasta; }
protected:
	IODBDMAChannelRegisters *			GetChannelRegistersVirtualAddress ( IOService * dbdmaProvider, UInt32 index );
};

#endif

