/*
 *	PlatformInterfaceDBDMA_Mapped.cpp
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include	"PlatformInterfaceDBDMA_Mapped.h"
#include	"AppleDBDMAAudio.h"

#define super PlatformInterfaceDBDMA

const UInt16 PlatformInterfaceDBDMA_Mapped::kAPPLE_IO_CONFIGURATION_SIZE	= kPlatformInterfaceSupportMappedCommon_APPLE_IO_CONFIGURATION_SIZE;
const UInt16 PlatformInterfaceDBDMA_Mapped::kI2S_IO_CONFIGURATION_SIZE		= kPlatformInterfaceSupportMappedCommon_I2S_IO_CONFIGURATION_SIZE;

const UInt32 PlatformInterfaceDBDMA_Mapped::kI2S0BaseOffset					= kPlatformInterfaceSupportMappedCommon_I2S0BaseOffset;
const UInt32 PlatformInterfaceDBDMA_Mapped::kI2S1BaseOffset					= kPlatformInterfaceSupportMappedCommon_I2S1BaseOffset;

OSDefineMetaClassAndStructors ( PlatformInterfaceDBDMA_Mapped, PlatformInterfaceDBDMA )

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceDBDMA_Mapped::init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	bool					result = FALSE;
	IOService* 				theService;
	IORegistryEntry			*macio;
	IORegistryEntry			*gpio;
	IORegistryEntry			*i2s;
	IORegistryEntry			*i2sParent;
	IOMemoryMap				*map;

	debugIOLog ( 3, "+ PlatformInterfaceDBDMA_Mapped::init ( %p, %p, %d )", device, provider, inDBDMADeviceIndex );
	
	FailIf ( NULL == provider, Exit );
	FailIf ( NULL == device, Exit );

	result = super::init ( device, provider, inDBDMADeviceIndex );
	if ( result ) 
	{
		mKeyLargoService = IOService::waitForService ( IOService::serviceMatching ( "KeyLargo" ) );
		debugIOLog ( 3, "  sound's name is %s",  ( (IORegistryEntry*)device)->getName () );
		
		i2s =  ( ( IORegistryEntry*)device)->getParentEntry ( gIODTPlane );
		FailWithAction ( 0 == i2s, result = false, Exit );
		debugIOLog ( 3, "  parent name is '%s'", i2s->getName () );

		if ( 0 == strcmp ( "i2s-a", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell0;
		}
		else if ( 0 == strcmp ( "i2s-b", i2s->getName () ) ) 
		{
			mI2SInterfaceNumber = kUseI2SCell1;
		}
		else if ( 0 == strcmp ( "i2s-c", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell2;
		}
		else if ( 0 == strcmp ( "i2s-d", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell3;
		}
		else if ( 0 == strcmp ( "i2s-e", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell4;
		}
		else if ( 0 == strcmp ( "i2s-f", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell5;
		}
		else if ( 0 == strcmp ( "i2s-g", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell6;
		}
		else if ( 0 == strcmp ( "i2s-h", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell7;
		}
		debugIOLog ( 5, "  mI2SInterfaceNumber = %d", mI2SInterfaceNumber );
		
		i2sParent = i2s->getParentEntry ( gIODTPlane );
		FailWithAction ( 0 == i2sParent, result = false, Exit );
		debugIOLog ( 3, "  parent name of '%s' is %s", i2s->getName (), i2sParent->getName () );

		macio = i2sParent->getParentEntry ( gIODTPlane );
		FailWithAction ( 0 == macio, result = false, Exit );
		debugIOLog ( 3, "  macio name is %s", macio->getName () );
		
		gpio = macio->childFromPath ( kGPIODTEntry, gIODTPlane);
		FailWithAction ( !gpio, result = false, Exit);
		debugIOLog ( 3, "  gpio name is %s", gpio->getName () );

		theService = ( OSDynamicCast ( IOService, i2s ) );
		FailWithAction ( !theService, result = false, Exit );

		map = theService->mapDeviceMemoryWithIndex ( inDBDMADeviceIndex );
		FailWithAction ( 0 == map, result = false, Exit );

		// cache the config space
		mSoundConfigSpace = (UInt8 *)map->getPhysicalAddress();

		// sets the clock base address figuring out which I2S cell we're on
		if ((((UInt32)mSoundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) 
		{
			//	[3060321]	ioBaseAddress is required by this object in order to enable the target
			//				I2S I/O Module for which this object is to service.  The I2S I/O Module
			//				enable occurs through the configuration registers which reside in the
			//				first block of ioBase.		rbm		2 Oct 2002
			mIOBaseAddress = (void *)((UInt32)mSoundConfigSpace - kI2S0BaseOffset);
			mIOBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)mSoundConfigSpace - kI2S0BaseOffset), 256);
			mI2SInterfaceNumber = kUseI2SCell0;
		}
		else if ((((UInt32)mSoundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) 
		{
			//	[3060321]	ioBaseAddress is required by this object in order to enable the target
			//				I2S I/O Module for which this object is to service.  The I2S I/O Module
			//				enable occurs through the configuration registers which reside in the
			//				first block of ioBase.		rbm		2 Oct 2002
			mIOBaseAddress = (void *)((UInt32)mSoundConfigSpace - kI2S1BaseOffset);
			mIOBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)mSoundConfigSpace - kI2S1BaseOffset), 256);
			mI2SInterfaceNumber = kUseI2SCell1;
		}
		else 
		{
			debugIOLog (3, "  AudioI2SControl::init ERROR: unable to setup ioBaseAddress and i2SInterfaceNumber");
		}
		FailIf (NULL == mIOBaseAddressMemory, Exit);

		//	[3060321]	ioConfigurationBaseAddress is required by this object in order to enable the target
		//				I2S I/O Module for which this object is to service.  The I2S I/O Module
		//				enable occurs through the configuration registers which reside in the
		//				first block of ioBase.		rbm		2 Oct 2002
		
		mIOConfigurationBaseAddress = (void *)mIOBaseAddressMemory->map()->getVirtualAddress();
		FailIf ( NULL == mIOConfigurationBaseAddress, Exit );

		//
		//	There are three sections of memory mapped I/O that are directly accessed by the Apple02Audio.  These
		//	include the GPIOs, I2S DMA Channel Registers and I2S control registers.  They fall within the memory map 
		//	as follows:
		//	~                              ~
		//	|______________________________|
		//	|                              |
		//	|         I2S Control          |
		//	|______________________________|	<-	soundConfigSpace = ioBase + i2s0BaseOffset ...OR... ioBase + i2s1BaseOffset
		//	|                              |
		//	~                              ~
		//	~                              ~
		//	|______________________________|
		//	|                              |
		//	|       I2S DMA Channel        |
		//	|______________________________|	<-	i2sDMA = ioBase + i2s0_DMA ...OR... ioBase + i2s1_DMA
		//	|                              |
		//	~                              ~
		//	~                              ~
		//	|______________________________|
		//	|            FCRs              |
		//	|            GPIO              |	<-	gpio = ioBase + gpioOffsetAddress
		//	|         ExtIntGPIO           |	<-	fcr = ioBase + fcrOffsetAddress
		//	|______________________________|	<-	ioConfigurationBaseAddress
		//	|                              |
		//	~                              ~
		//
		//	The I2S DMA Channel is mapped in by the Apple02DBDMAAudioDMAEngine.  Only the I2S control registers are 
		//	mapped in by the AudioI2SControl.  The Apple I/O Configuration Space (i.e. FCRs, GPIOs and ExtIntGPIOs)
		//	are mapped in by the subclass of Apple02Audio.  The FCRs must also be mapped in by the AudioI2SControl
		//	object as the init method must enable the I2S I/O Module for which the AudioI2SControl object is
		//	being instantiated for.
		//
		//	The physical addresses for memory mapped I/O within the KeyLargo system I/O controller are as follows:
		//	
		//
		//
		//                                       ______________________________
		//                                     /|                              |
		//                                    / |                              |
		//	                                 /  |______________________________|
		//	 ______________________________ /   |                              |
		//	|                              |    |       I2S 1 ('i2s-b')        |
		//	|                              |    |______________________________|....0xnnn11000	[0x80011000]
		//	|                              |    |                              |
		//	|                              |    |       I2S 0 ('i2s-a')        |
		//	|                              |    |______________________________|....0xnnn10000	[0x80001000]
		//	|       Device Registers       |   / 
		//	|                              |  /
		//	|                              | /   ______________________________ 
		//	|                              |/  /|                              |
		//	|______________________________|__/ |                              |
		//	|                              |    |______________________________|
		//	|                              |    |                              |
		//	|                              |    |    I2S 1 Rx DMA ('i2s-b')    |
		//	|                              |    |______________________________|....0xnnn08300	[0x80008300]
		//	|    DMA Channel Registers     |    |                              |
		//	|                              |    |    I2S 1 Tx DMA ('i2s-b')    |
		//	|                              |    |______________________________|....0xnnn08200	[0x80008200]
		//	|                              |    |                              |
		//	|______________________________|    |    I2S 0 Rx DMA ('i2s-a')    |
		//	|                              |\   |______________________________|....0xnnn08100	[0x80008100]
		//	|                              | \  |                              |
		//	|      Reserved: read "0"      |  \ |    I2S 0 Tx DMA ('i2s-a')    |
		//	|                              |   \|______________________________|....0xnnn08000	[0x80008000]
		//	|______________________________|
		//	|                              |
		//	|                              |
		//	|   Apple I/O Configuration    |
		//	|                              |
		//	|______________________________|....0xnnn00000	[0x80011000]
		//	
		//	Map the I2S configuration registers
		mIOI2SBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)mSoundConfigSpace), kI2S_IO_CONFIGURATION_SIZE);
		FailIf ( NULL == mIOI2SBaseAddressMemory, Exit );
		mI2SBaseAddress = (void *)mIOI2SBaseAddressMemory->map()->getVirtualAddress();
		FailIf (NULL == mI2SBaseAddress, Exit);
		
		debugIOLog (3, "  mI2SInterfaceNumber         = %d", mI2SInterfaceNumber);
		debugIOLog (3, "  mIOI2SBaseAddressMemory     = %p", mIOI2SBaseAddressMemory);
		debugIOLog (3, "  mI2SBaseAddress             = %p", mI2SBaseAddress);
		debugIOLog (3, "  mIOBaseAddressMemory        = %p", mIOBaseAddressMemory);
		debugIOLog (3, "  mIOConfigurationBaseAddress = %p", mIOConfigurationBaseAddress);
	}
Exit:
	debugIOLog ( 3, "- PlatformInterfaceDBDMA_Mapped::init ( %p, %p, %d ) returns %lX", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceDBDMA_Mapped::free() 
{
	debugIOLog (3, "+ PlatformInterfaceDBDMA_Mapped::free()");

 	if ( NULL != mIOBaseAddressMemory ) 
	{
		mIOBaseAddressMemory->release();
	}
	super::free();
	debugIOLog (3, "- PlatformInterfaceDBDMA_Mapped::free()");
}

#pragma mark ¥
#pragma mark ¥ Power Management
#pragma mark ¥
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceDBDMA_Mapped::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ DBDMA Memory Address Acquisition Methods
#pragma mark ¥
//	----------------------------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterfaceDBDMA_Mapped::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) 
{
	IOMemoryMap *				map;
	IOService *					parentOfParent;
	IODBDMAChannelRegisters *	ioBaseDMAInput = NULL;
	
	debugIOLog (3,  "+ PlatformInterfaceDBDMA_Mapped::GetInputChannelRegistersVirtualAddress ( %p )", dbdmaProvider );
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (3,  "  i2s-x name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debugIOLog (3,  "  parent of %s is %s", dbdmaProvider->getName(), parentOfParent->getName() );
	map = dbdmaProvider->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAInputIndex );
	FailIf ( NULL == map, Exit );
	ioBaseDMAInput = (IODBDMAChannelRegisters *) map->getVirtualAddress();
	debugIOLog (3,  "  ioBaseDMAInput virtual address %p is at physical address %p", ioBaseDMAInput, (void*)map->getPhysicalAddress() );
	if ( NULL == ioBaseDMAInput )
	{
		debugIOLog (1,  "  PlatformInterfaceDBDMA_Mapped::GetInputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" );
	}
Exit:
	debugIOLog (3,  "- PlatformInterfaceDBDMA_Mapped::GetInputChannelRegistersVirtualAddress ( %p ) returns %p", dbdmaProvider, ioBaseDMAInput );
	return ioBaseDMAInput;
}


//	----------------------------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterfaceDBDMA_Mapped::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) 
{
	IOMemoryMap *				map;
	IOService *					parentOfParent;
	
	debugIOLog (3,  "+ PlatformInterfaceDBDMA_Mapped::GetOutputChannelRegistersVirtualAddress ( %p )", dbdmaProvider );
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (3,  "  i2s-x name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debugIOLog (3,  "  parent of %s is %s", dbdmaProvider->getName(), parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	FailIf ( NULL == map, Exit );
	mIOBaseDMAOutput = (IODBDMAChannelRegisters *) map->getVirtualAddress();
	debugIOLog (3,  "  mIOBaseDMAOutput virtual address %p is at physical address %p", mIOBaseDMAOutput, (void*)map->getPhysicalAddress() );
	if ( NULL == mIOBaseDMAOutput )
	{
		debugIOLog (1,  "  PlatformInterfaceDBDMA_Mapped::GetOutputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" );
	}
Exit:
	debugIOLog (3,  "- PlatformInterfaceDBDMA_Mapped::GetOutputChannelRegistersVirtualAddress ( %p ) returns %p", dbdmaProvider, mIOBaseDMAOutput );
	return mIOBaseDMAOutput;
}


