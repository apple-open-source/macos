/*
 *	PlatformInterfaceDBDMA_PlatformFunction.cpp
 *
 *	
 *
 *  Created by AudioSW Team on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterfaceDBDMA_PlatformFunction.h"

#define super PlatformInterfaceDBDMA

OSDefineMetaClassAndStructors ( PlatformInterfaceDBDMA_PlatformFunction, PlatformInterfaceDBDMA )

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceDBDMA_PlatformFunction::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex ) 
{
	IORegistryEntry			*sound;
	bool					result;
	OSData					*osdata;
	IORegistryEntry			*i2s;
	IORegistryEntry			*macIO;
	
	debugIOLog ( 3, "+ PlatformInterfaceDBDMA_PlatformFunction::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d )", device, provider, inDBDMADeviceIndex );
	result = super::init (device, provider, inDBDMADeviceIndex);
	FailIf ( !result, Exit );

	sound = device;
	FailWithAction ( !sound, result = false, Exit );

	mI2S = sound->getParentEntry (gIODTPlane);
	FailWithAction (!mI2S, result = false, Exit);

	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "AAPL,phandle" ) );
	mI2SPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog ( 3,  "  mI2SPHandle 0x%lX", mI2SPHandle );
	
	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "reg" ) );
	mI2SOffset = *((UInt32*)osdata->getBytesNoCopy());
		
	i2s = mI2S->getParentEntry (gIODTPlane);
	FailWithAction ( !i2s, result = false, Exit );

	macIO = i2s->getParentEntry ( gIODTPlane );
	FailWithAction ( !macIO, result = false, Exit );
	debugIOLog ( 3, "  path = '...:%s:%s:%s:%s:'", macIO->getName (), i2s->getName (), mI2S->getName (), sound->getName () );
	
	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "AAPL,phandle" ) );
	mMacIOPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog ( 3,  "  mMacIOPHandle %lX", mMacIOPHandle );

	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "reg" ) );
	mMacIOOffset = *((UInt32*)osdata->getBytesNoCopy());


	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "compatible" ) );
	FailIf ( 0 == osdata, Exit );
	if ( osdata->isEqualTo ( kParentOfParentCompatible32bitSysIO, strlen ( kParentOfParentCompatible32bitSysIO ) ) )
	{
		debugIOLog ( 3,  "  about to waitForService on mSystemIOControllerService %p for %s", mSystemIOControllerService, kParentOfParentCompatible32bitSysIO );
		mSystemIOControllerService = IOService::waitForService ( IOService::serviceMatching ( "AppleKeyLargo" ) );
	}
	else if ( osdata->isEqualTo ( kParentOfParentCompatible64bitSysIO, strlen ( kParentOfParentCompatible64bitSysIO ) ) )
	{
		debugIOLog ( 3,  "  about to waitForService on mSystemIOControllerService %p for %s", mSystemIOControllerService, kParentOfParentCompatible64bitSysIO );
		mSystemIOControllerService = IOService::waitForService ( IOService::serviceMatching ( "AppleK2" ) );
	}
	else
	{
		FailIf ( TRUE, Exit );
	}
	debugIOLog ( 3,  "  mSystemIOControllerService %p", mSystemIOControllerService );
	
Exit:
	debugIOLog ( 3, "- PlatformInterfaceDBDMA_PlatformFunction::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d ) returns %lX", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceDBDMA_PlatformFunction::free() 
{
}

#pragma mark ¥
#pragma mark ¥ Power Management
#pragma mark ¥
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceDBDMA_PlatformFunction::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ DBDMA Memory Address Acquisition Methods
#pragma mark ¥
//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterfaceDBDMA_PlatformFunction::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) 
{
	IODBDMAChannelRegisters *		result = NULL;
	
	result = GetChannelRegistersVirtualAddress ( dbdmaProvider, kDMAInputIndex );
	debugIOLog ( 3,  "± PlatformInterfaceDBDMA_PlatformFunction::GetInputChannelRegistersVirtualAddress ( %p ) returns %p", dbdmaProvider, result );
	return result;
}

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterfaceDBDMA_PlatformFunction::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) 
{
	IODBDMAChannelRegisters *		result = NULL;
	
	result = GetChannelRegistersVirtualAddress ( dbdmaProvider, kDMAOutputIndex );
	debugIOLog ( 3,  "± PlatformInterfaceDBDMA_PlatformFunction::GetOutputChannelRegistersVirtualAddress ( %p ) returns %p", dbdmaProvider, result );
	return result;
}

#pragma mark ¥
#pragma mark ¥ Utility Methods
#pragma mark ¥
//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterfaceDBDMA_PlatformFunction::GetChannelRegistersVirtualAddress ( IOService * dbdmaProvider, UInt32 index ) 
{
	IOMemoryMap *				map;
	
	mIOBaseDMA[index] = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog ( 3,  "  PlatformInterfaceDBDMA_PlatformFunction dbdmaProvider name is %s", dbdmaProvider->getName() );
	map = dbdmaProvider->mapDeviceMemoryWithIndex ( index );
	FailIf ( NULL == map, Exit );
	mIOBaseDMA[index] = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debugIOLog ( 3,  "  PlatformInterfaceDBDMA_PlatformFunction::mIOBaseDMA[%ld] %p is at physical %p", index, mIOBaseDMA[index], (void*)map->getPhysicalAddress() );
	if ( NULL == mIOBaseDMA[index] ) 
	{ 
		debugIOLog (1,  "± PlatformInterfaceDBDMA_PlatformFunction::GetChannelRegistersVirtualAddress index %ld IODBDMAChannelRegisters NOT IN VIRTUAL SPACE", index ); 
	}
Exit:
	return mIOBaseDMA[index];
}
