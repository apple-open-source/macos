/*
 *	PlatformInterfaceDBDMA_PlatformFunctionK2.cpp
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterfaceDBDMA_PlatformFunction_K2.h"

#define super PlatformInterfaceDBDMA

OSDefineMetaClassAndStructors ( PlatformInterfaceDBDMA_PlatformFunctionK2, PlatformInterfaceDBDMA )

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceDBDMA_PlatformFunctionK2::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex ) 
{
	IORegistryEntry			*sound;
	bool					result;
	OSData					*osdata;
	IORegistryEntry			*i2S;
	IORegistryEntry			*macIO;
	
	debugIOLog (3,  "+ K2Platform::init ( %p, %p, %d )", device, provider, inDBDMADeviceIndex );
	result = super::init (device, provider, inDBDMADeviceIndex);
	FailIf ( !result, Exit );

	sound = device;
	FailWithAction (!sound, result = false, Exit);
	debugIOLog (3, "  K2 - sound's name is %s", sound->getName ());

	mI2S = sound->getParentEntry (gIODTPlane);
	FailWithAction (!mI2S, result = false, Exit);
	debugIOLog (3, "  K2Platform - i2s' name is %s", mI2S->getName ());

	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "AAPL,phandle" ) );
	mI2SPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog (3,  "  mI2SPHandle 0x%lX", mI2SPHandle );
	
	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "reg" ) );
	mI2SOffset = *((UInt32*)osdata->getBytesNoCopy());
		
	i2S = mI2S->getParentEntry (gIODTPlane);
	FailWithAction (!i2S, result = false, Exit);
	debugIOLog (3, "  mI2S - parent name is %s", i2S->getName ());

	macIO = i2S->getParentEntry (gIODTPlane);
	FailWithAction (!macIO, result = false, Exit);
	debugIOLog (3, "  i2S - parent name is %s", macIO->getName ());
	
	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "AAPL,phandle" ) );
	mMacIOPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog (3,  "  mMacIOPHandle %lX", mMacIOPHandle );

	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "reg" ) );
	mMacIOOffset = *((UInt32*)osdata->getBytesNoCopy());


	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "compatible" ) );
	FailIf ( 0 == osdata, Exit );
	if ( osdata->isEqualTo ( kParentOfParentCompatible32bitSysIO, strlen ( kParentOfParentCompatible32bitSysIO ) ) )
	{
		debugIOLog ( 3,  "  about to waitForService on mSystemIOControllerService %p for %s", mSystemIOControllerService, kParentOfParentCompatible32bitSysIO );
		mSystemIOControllerService = IOService::waitForService ( IOService::serviceMatching ( "AppleKeyLargo" ) );
	}
	if ( osdata->isEqualTo ( kParentOfParentCompatible64bitSysIO, strlen ( kParentOfParentCompatible64bitSysIO ) ) )
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

	debugIOLog (3,  "- K2Platform::init ( %p, %p, %d ) returns %d", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceDBDMA_PlatformFunctionK2::free() 
{
}

#pragma mark ¥
#pragma mark ¥ Power Management
#pragma mark ¥
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceDBDMA_PlatformFunctionK2::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ DBDMA Memory Address Acquisition Methods
#pragma mark ¥
//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterfaceDBDMA_PlatformFunctionK2::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) 
{
	IOMemoryMap *			map;
	IOService *				parentOfParent;
	IOPhysicalAddress		ioPhysAddr;
	IOMemoryDescriptor *	theDescriptor;
	IOByteCount				length = 256;
	
	mIOBaseDMA[kDMAInputIndex] = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	
	debugIOLog (7,  "  PlatformInterfaceDBDMA_PlatformFunctionK2::GetInputChannelRegistersVirtualAddress i2s-a name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	
	debugIOLog (7,  "   parentOfParent name is %s", parentOfParent->getName() );
//	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	map = parentOfParent->mapDeviceMemoryWithIndex ( 1 );
	FailIf ( NULL == map, Exit );
	
	ioPhysAddr = map->getPhysicalSegment( kIODMAInputOffset, &length );
	FailIf ( NULL == ioPhysAddr, Exit );
	
	theDescriptor = IOMemoryDescriptor::withPhysicalAddress ( ioPhysAddr, 256, kIODirectionOutIn );
	FailIf ( NULL == theDescriptor, Exit );
	
	map = theDescriptor->map ();
	FailIf ( NULL == map, Exit );
	
	mIOBaseDMA[kDMAInputIndex] = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debugIOLog (7,  "  mIOBaseDMA[kDMAInputIndex] %p", mIOBaseDMA[kDMAInputIndex] );
	if ( NULL == mIOBaseDMA[kDMAInputIndex] ) 
	{
		debugIOLog (1,  "  PlatformInterfaceDBDMA_PlatformFunctionK2::GetInputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" );
	}
Exit:
	return mIOBaseDMA[kDMAInputIndex];
}

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterfaceDBDMA_PlatformFunctionK2::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) 
{
	IOMemoryMap *				map;
	IOService *				parentOfParent;

	mIOBaseDMA[kDMAOutputIndex] = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (7,  "  PlatformInterfaceDBDMA_PlatformFunctionK2::GetOutputChannelRegistersVirtualAddress i2s-a name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	
	debugIOLog (7,  "   parentOfParent name is %s", parentOfParent->getName() );
//	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	map = parentOfParent->mapDeviceMemoryWithIndex ( 1 );
	FailIf ( NULL == map, Exit );
	
	mIOBaseDMA[kDMAOutputIndex] = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debugIOLog (7,  "  mIOBaseDMA[kDMAOutputIndex] %p is at physical %p", mIOBaseDMA[kDMAOutputIndex], (void*)map->getPhysicalAddress() );
	if ( NULL == mIOBaseDMA[kDMAOutputIndex] ) 
	{
		debugIOLog (1,  "  PlatformInterfaceDBDMA_PlatformFunctionK2::GetOutputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" ); 
	}
Exit:
	return mIOBaseDMA[kDMAOutputIndex];
}

