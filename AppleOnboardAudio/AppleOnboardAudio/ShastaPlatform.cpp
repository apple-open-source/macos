/*
 *  ShastaPlatform.cpp
 *  
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include "ShastaPlatform.h"

#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareCommon.h"
#include "AppleOnboardAudio.h"
#include "IOKit/audio/IOAudioDevice.h"

#define super K2Platform

#pragma mark ---------------------------
#pragma mark Platform Interface Methods
#pragma mark ---------------------------

OSDefineMetaClassAndStructors(ShastaPlatform, super)

//	--------------------------------------------------------------------------------
bool ShastaPlatform::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex) {
	bool			result;
	
	debugIOLog (3,  "+ ShastaPlatform[%p]::init", this );
	result = super::init (device, provider, inDBDMADeviceIndex);
	FailIf ( !result, Exit );

Exit:
	debugIOLog (3,  "- ShastaPlatform[%p (%ld)]::init returns %d", this, mInstanceIndex, result );
	return result;
}

//	--------------------------------------------------------------------------------
void ShastaPlatform::free()
{
	debugIOLog (3, "+ ShastaPlatform::free()");

	super::free();

	debugIOLog (3, "- ShastaPlatform::free()");
}

#pragma mark ---------------------------
#pragma mark DBDMA Memory Address Acquisition Methods
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	ShastaPlatform::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IODBDMAChannelRegisters *		result = NULL;
	
	result = GetChannelRegistersVirtualAddress ( dbdmaProvider, kDMAInputIndex );
	debugIOLog (3,  "ShastaPlatform[%ld]::GetInputChannelRegistersVirtualAddress ( %p ) returns %p", mInstanceIndex, dbdmaProvider, result );
	return result;
}

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	ShastaPlatform::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IODBDMAChannelRegisters *		result = NULL;
	
	result = GetChannelRegistersVirtualAddress ( dbdmaProvider, kDMAOutputIndex );
	debugIOLog (3,  "ShastaPlatform[%ld]::GetOutputChannelRegistersVirtualAddress ( %p ) returns %p", mInstanceIndex, dbdmaProvider, result );
	return result;
}

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	ShastaPlatform::GetChannelRegistersVirtualAddress ( IOService * dbdmaProvider, UInt32 index ) {
	IOMemoryMap *				map;
	
	mIOBaseDMA[index] = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (3,  "   dbdmaProvider name is %s", dbdmaProvider->getName() );
	map = dbdmaProvider->mapDeviceMemoryWithIndex ( index );
	FailIf ( NULL == map, Exit );
	mIOBaseDMA[index] = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debugIOLog (3,  "ShastaPlatform[%ld]::mIOBaseDMA[%ld] %p is at physical %p", mInstanceIndex, index, mIOBaseDMA[index], (void*)map->getPhysicalAddress() );
	if ( NULL == mIOBaseDMA[index] ) { debugIOLog (1,  "ShastaPlatform::GetChannelRegistersVirtualAddress index %ld IODBDMAChannelRegisters NOT IN VIRTUAL SPACE", index ); }
Exit:
	return mIOBaseDMA[index];
}
