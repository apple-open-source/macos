/*
 *	PlatformInterface.cpp
 *
 *	Interface class for IO controllers
 *
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright ©  2003-2004 AppleComputer. All rights reserved.
 */

#include "PlatformInterface.h"
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareCommon.h"
#include "AppleOnboardAudio.h"

#include	"PlatformDBDMAFactory.h"
#include	"PlatformFCRFactory.h"
#include	"PlatformGPIOFactory.h"
#include	"PlatformI2CFactory.h"
#include	"PlatformI2SFactory.h"

#define super OSObject

class AppleOnboardAudio;

UInt32 PlatformInterface::sInstanceCount = 0;

OSDefineMetaClassAndStructors ( PlatformInterface, OSObject );

#pragma mark ---------------------------
#pragma mark UNIX LIKE FUNCTIONS
#pragma mark ---------------------------

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	The 'supportSelectors' argument is a bit mapped array that indicates which derived class for each
//	of the I/O modules is to be instantiated.  Each field is 4 bits wide and reserved as follows:
//
//	 ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
//	|    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
//	| 19 | 18 | 17 | 16 | 15 | 14 | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
//	|____|____|____|____|____|____|____|____|____|____|____|____|____|____|____|____|____|____|____|____|
//	  |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |____|____|____|_____ DBDMA Selector
//	  |    |    |    |    |    |    |    |    |    |    |    |    |____|____|____|_________________________	FCR Selector
//	  |    |    |    |    |    |    |    |    |____|____|____|_____________________________________________ GPIO Selector
//	  |    |    |    |    |____|____|____|_________________________________________________________________ I2C Selector
//	  |____|____|____|_____________________________________________________________________________________ I2S Selector
//
//	Where:
//
//	DBDMA Selector
//		0					RESERVED
//		1					PlatformInterfaceDBDMA_Mapped
//		2					PlatformInterfaceDBDMA_Function
//		3					PlatformInterfaceDBDMA_FunctionK2Only
//
//	FCR Selector
//		0					RESERVED
//		1					PlatformInterfaceFCR_Mapped
//		2					PlatformInterfaceFCR_Function
//
//	GPIO Selector
//		0					RESERVED
//		1					PlatformInterfaceGPIO_Mapped
//		2					PlatformInterfaceGPIO_Function
//
//	I2C Selector
//		0					RESERVED
//		1					PlatformInterfaceI2C_Mapped
//		2					PlatformInterfaceI2C_Function
//
//	I2S Selector
//		0					RESERVED
//		1					PlatformInterfaceI2S_Mapped
//		2					PlatformInterfaceI2S_Function
//
//	3 Nov 2004 - RBM
bool PlatformInterface::init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex, UInt32 supportSelectors )
{
	Boolean		result;

	debugIOLog ( 6, "+ PlatformInterface::init ( %p, %p, %d, 0x%X )", device, provider, inDBDMADeviceIndex, supportSelectors );

	result = super::init ();
	FailIf ( !result, Exit );
	
	FailIf ( 0 == provider, Exit );
	mProvider = provider;

	mSupportSelectors = supportSelectors;
	
	PlatformInterface::sInstanceCount++;
	
	mInstanceIndex = PlatformInterface::sInstanceCount;
	
	mComboInAssociation = kGPIO_Selector_NotAssociated;		//	[3453799]
	mComboOutAssociation = kGPIO_Selector_NotAssociated;	//	[3453799]
	
	for ( UInt32 index=0; index < kNumberOfActionSelectors; index++ )
	{
		mComboStateMachine[index] = kComboStateMachine_handle_jack_insert;
	}

	switch ( KPlatformSupport_bitAddress_mask & ( supportSelectors >> kPlatformSupportDBDMA_bitAddress ) )
	{
		case kPlatformSupport_MAPPED:		platformInterfaceDBDMA = PlatformDBDMAFactory::createPlatform ( OSString::withCString ( kPlatformDBDMAMappedString ) );					break;
		case kPlatformSupport_PLATFORM:		platformInterfaceDBDMA = PlatformDBDMAFactory::createPlatform ( OSString::withCString ( kPlatformDBDMAPlatformFunctionString ) );		break;
		case kPlatformSupportDBDMA_K2:		platformInterfaceDBDMA = PlatformDBDMAFactory::createPlatform ( OSString::withCString ( kPlatformDBDMAPlatformFunctionK2String ) );		break;
	}
	FailWithAction ( 0 == platformInterfaceDBDMA, result = FALSE, Exit );
	result = platformInterfaceDBDMA->init ( device, provider, inDBDMADeviceIndex );
	FailIf ( !result, Exit );
	
	switch ( KPlatformSupport_bitAddress_mask & ( supportSelectors >> kPlatformSupportFCR_bitAddress ) )
	{
		case kPlatformSupport_MAPPED:	platformInterfaceFCR = PlatformFCRFactory::createPlatform ( OSString::withCString ( kPlatformFCRMappedString ) );				break;
		case kPlatformSupport_PLATFORM:	platformInterfaceFCR = PlatformFCRFactory::createPlatform ( OSString::withCString ( kPlatformFCRPlatformFunctionString ) );		break;
	}
	FailWithAction ( 0 == platformInterfaceFCR, result = FALSE, Exit );
	result = platformInterfaceFCR->init ( device, provider, inDBDMADeviceIndex );
	if ( !result )
	FailIf ( !result, Exit );
	
	switch ( KPlatformSupport_bitAddress_mask & ( supportSelectors >> kPlatformSupportGPIO_bitAddress ) )
	{
		case kPlatformSupport_MAPPED:	platformInterfaceGPIO = PlatformGPIOFactory::createPlatform ( OSString::withCString ( kPlatformGPIOMappedString ) );			break;
		case kPlatformSupport_PLATFORM:	platformInterfaceGPIO = PlatformGPIOFactory::createPlatform ( OSString::withCString ( kPlatformGPIOPlatformFunctionString ) );	break;
	}
	FailWithAction ( 0 == platformInterfaceGPIO, result = FALSE, Exit );
	result = platformInterfaceGPIO->init ( device, provider, inDBDMADeviceIndex );
	FailIf ( !result, Exit );
	
	switch ( KPlatformSupport_bitAddress_mask & ( supportSelectors >> kPlatformSupportI2C_bitAddress ) )
	{
		case kPlatformSupport_MAPPED:	platformInterfaceI2C = PlatformI2CFactory::createPlatform ( OSString::withCString ( kPlatformI2CMappedString ) );				break;
		case kPlatformSupport_PLATFORM:	platformInterfaceI2C = PlatformI2CFactory::createPlatform ( OSString::withCString ( kPlatformI2CPlatformFunctionString ) );		break;
	}
	FailWithAction ( 0 == platformInterfaceI2C, result = FALSE, Exit );
	result = platformInterfaceI2C->init ( device, provider, inDBDMADeviceIndex );
	FailIf ( !result, Exit );
	
	switch ( KPlatformSupport_bitAddress_mask & ( supportSelectors >> kPlatformSupportI2S_bitAddress ) )
	{
		case kPlatformSupport_MAPPED:	platformInterfaceI2S = PlatformI2SFactory::createPlatform ( OSString::withCString ( kPlatformI2SMappedString ) );				break;
		case kPlatformSupport_PLATFORM:	platformInterfaceI2S = PlatformI2SFactory::createPlatform ( OSString::withCString ( kPlatformI2SPlatformFunctionString ) );		break;
	}
	FailWithAction ( 0 == platformInterfaceI2S, result = FALSE, Exit );
	result = platformInterfaceI2S->init ( device, provider, inDBDMADeviceIndex );
	FailIf ( !result, Exit );
	
Exit:
	if ( !result )
	{
		if ( 0 != platformInterfaceDBDMA )
		{
			CLEAN_RELEASE ( platformInterfaceDBDMA );
			platformInterfaceDBDMA = 0;
		}
		if ( 0 != platformInterfaceFCR )
		{
			CLEAN_RELEASE ( platformInterfaceFCR );
			platformInterfaceFCR = 0;
		}
		if ( 0 != platformInterfaceGPIO )
		{
			CLEAN_RELEASE ( platformInterfaceGPIO );
			platformInterfaceGPIO = 0;
		}
		if ( 0 != platformInterfaceI2C )
		{
			CLEAN_RELEASE ( platformInterfaceI2C );
			platformInterfaceI2C = 0;
		}
		if ( 0 != platformInterfaceI2S )
		{
			CLEAN_RELEASE ( platformInterfaceI2S );
			platformInterfaceI2S = 0;
		}
	}
	debugIOLog ( 6, "- PlatformInterface::init ( %p, %p, %d, 0x%X ) returns %d", device, provider, inDBDMADeviceIndex, supportSelectors, result );
	return result;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::free ( void )
{
	
	debugIOLog ( 3, "+ PlatformInterface::free" );
	
	unregisterNonDetectInterrupts ();
	unregisterDetectInterrupts ();

	if ( 0 != platformInterfaceDBDMA )
	{
		CLEAN_RELEASE ( platformInterfaceDBDMA );
		platformInterfaceDBDMA = 0;
	}
	if ( 0 != platformInterfaceFCR )
	{
		CLEAN_RELEASE ( platformInterfaceFCR );
		platformInterfaceFCR = 0;
	}
	if ( 0 != platformInterfaceGPIO )
	{
		CLEAN_RELEASE ( platformInterfaceGPIO );
		platformInterfaceGPIO = 0;
	}
	if ( 0 != platformInterfaceI2C )
	{
		CLEAN_RELEASE ( platformInterfaceI2C );
		platformInterfaceI2C = 0;
	}
	if ( 0 != platformInterfaceI2S )
	{
		CLEAN_RELEASE ( platformInterfaceI2S );
		platformInterfaceI2S = 0;
	}

	if ( 0 != mRegisterDetectInterruptsThread ) {
		thread_call_free ( mRegisterDetectInterruptsThread );
	}

	if ( 0 != mRegisterNonDetectInterruptsThread ) {
		thread_call_free ( mRegisterNonDetectInterruptsThread );
	}

	super::free ();
	
	debugIOLog ( 3, "- PlatformInterface::free" );
	return;
}

#pragma mark ---------------------------
#pragma mark POWER MANAGEMENT SUPPORT
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	debugIOLog ( 6, "+ PlatformInterface::performPowerStateChange ( %p, %ld, %ld )", device, currentPowerState, pendingPowerState );
	switch ( pendingPowerState )
	{
		case kIOAudioDeviceSleep:
			//	Sleep must disable/unregister all non-detect and detect interrupts.
			if ( 0 != platformInterfaceGPIO )
			{
				if ( platformInterfaceGPIO->needsUnregisterInterruptsOnSleep () )
				{
					if ( kIOAudioDeviceActive == currentPowerState )
					{
						unregisterNonDetectInterrupts ();
						unregisterDetectInterrupts ();
					}
					else if ( kIOAudioDeviceIdle == currentPowerState )
					{
						unregisterDetectInterrupts ();
					}
				}
			}
			if ( 0 != platformInterfaceDBDMA )
			{
				platformInterfaceDBDMA->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceFCR )
			{
				platformInterfaceFCR->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceGPIO )
			{
				platformInterfaceGPIO->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceI2C )
			{
				platformInterfaceI2C->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceI2S )
			{
				platformInterfaceI2S->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			break;
		case kIOAudioDeviceIdle:
			//	Idle needs to disable/unregister any non-detect interrupt such as codec error interrupts
			//	or codec interrupts.  Detect interrupts, such as headphone detect must remain enabled.
			if ( kIOAudioDeviceActive == currentPowerState )
			{
				if ( 0 != platformInterfaceGPIO )
				{
					if ( platformInterfaceGPIO->needsUnregisterInterruptsOnSleep () )
					{
						unregisterNonDetectInterrupts ();
					}
				}
			}
			if ( 0 != platformInterfaceDBDMA )
			{
				platformInterfaceDBDMA->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceFCR )
			{
				platformInterfaceFCR->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceGPIO )
			{
				platformInterfaceGPIO->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceI2C )
			{
				platformInterfaceI2C->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceI2S )
			{
				platformInterfaceI2S->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			if ( kIOAudioDeviceSleep == currentPowerState )
			{
				if ( 0 != platformInterfaceGPIO )
				{
					if ( platformInterfaceGPIO->needsUnregisterInterruptsOnSleep () )
					{
						registerDetectInterrupts ( (IOService*)this );
					}
				}
			}
			break;
		case kIOAudioDeviceActive:
			//	Active must enable/register all interrupt sources.  This includes non-detect and detect interrupts.
			if ( 0 != platformInterfaceDBDMA )
			{
				platformInterfaceDBDMA->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceFCR )
			{
				platformInterfaceFCR->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceGPIO )
			{
				platformInterfaceGPIO->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceI2C )
			{
				platformInterfaceI2C->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			
			if ( 0 != platformInterfaceI2S )
			{
				platformInterfaceI2S->performPowerStateChange ( device, currentPowerState, pendingPowerState );
			}
			if ( kIOAudioDeviceSleep == currentPowerState )
			{
				if ( 0 != platformInterfaceGPIO )
				{
					if ( platformInterfaceGPIO->needsRegisterInterruptsOnWake () )
					{
						registerDetectInterrupts ( (IOService*)this );
						registerNonDetectInterrupts ( (IOService*)this );
					}
				}
			} else if ( kIOAudioDeviceIdle == currentPowerState )
			{
				if ( 0 != platformInterfaceGPIO )
				{
					if ( platformInterfaceGPIO->needsRegisterInterruptsOnWake () )
					{
						registerNonDetectInterrupts ( (IOService*)this );
					}
		}
	}
	if ( 0 != platformInterfaceGPIO )
	{
		if ( platformInterfaceGPIO->needsCheckDetectStatusOnWake () && !platformInterfaceGPIO->needsRegisterInterruptsOnWake () )
		{
			checkDetectStatus ( device );
		}
	}
			break;
	}
	debugIOLog ( 6, "- PlatformInterface::performPowerStateChange ( %p, %ld, %ld ) returns 0x%lX", device, currentPowerState, pendingPowerState, result );
	return result;
}


#pragma mark ---------------------------
#pragma mark INTERRUPT SUPPORT
#pragma mark ---------------------------

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::checkDetectStatus ( IOService * device )
{
#if 0
	GPIOSelector		theAnalogJack;
#endif
	
	debugIOLog ( 3, "+ PlatformInterface[%ld]::checkDetectStatus", mInstanceIndex );

#if 0
	theAnalogJack = getComboOutAssociation();
	if ( kGPIO_Unknown != getComboOut () && kGPIO_Selector_NotAssociated != theAnalogJack )
	{
		switch ( mComboStateMachine[theAnalogJack] )
		{
			case kComboStateMachine_handle_jack_insert:
				mComboStateMachine[theAnalogJack] = kComboStateMachine_handle_plastic_change;
				break;
			case kComboStateMachine_handle_metal_change:
				mComboStateMachine[theAnalogJack] = kComboStateMachine_handle_jack_insert;
				break;
			case kComboStateMachine_handle_plastic_change:
				mComboStateMachine[theAnalogJack] = kComboStateMachine_handle_jack_insert;
				break;
		}
	}

	theAnalogJack = getComboInAssociation();
	if ( kGPIO_Unknown != getComboIn () && kGPIO_Selector_NotAssociated != theAnalogJack )
	{
		switch ( mComboStateMachine[theAnalogJack] )
		{
			case kComboStateMachine_handle_jack_insert:
				mComboStateMachine[theAnalogJack] = kComboStateMachine_handle_plastic_change;
				break;
			case kComboStateMachine_handle_metal_change:
				mComboStateMachine[theAnalogJack] = kComboStateMachine_handle_jack_insert;
				break;
			case kComboStateMachine_handle_plastic_change:
				mComboStateMachine[theAnalogJack] = kComboStateMachine_handle_jack_insert;
				break;
		}
	}
#endif
	if ( mGpioMessageFlag & ( 1 << gpioMessage_HeadphoneDetect_bitAddress ) )  {
		headphoneDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_SpeakerDetect_bitAddress ) )  {
		speakerDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_LineInDetect_bitAddress ) )  {
		lineInDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_LineOutDetect_bitAddress ) )  {
		lineOutDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_DigitalInDetect_bitAddress ) )  {
		digitalInDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_DigitalOutDetect_bitAddress ) )  {
		digitalOutDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	debugIOLog ( 3, "- PlatformInterface[%ld]::checkDetectStatus", mInstanceIndex );
}


//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::registerInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source )
{
	IOReturn			result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::registerInterruptHandler ( IOService * theDevice %p, void * interruptHandler %p, PlatformInterruptSource source %d )", theDevice, interruptHandler, source );
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->registerInterruptHandler ( theDevice, interruptHandler, source );
	}
	debugIOLog ( 6, "- PlatformInterface::registerInterruptHandler ( IOService * theDevice %p, void * interruptHandler %p, PlatformInterruptSource source %d ) returns 0x%lX", theDevice, interruptHandler, source, result );
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	PlatformInterface::registerDetectInterrupts ( IOService * device )
{
	bool		result = false;
	
	debugIOLog ( 3, "+ PlatformInterface[%ld]::registerDetectInterrupts ( IOService * device %p ) ", mInstanceIndex, device );

	FailIf ( 0 == device, Exit );

#if 1
	threadedMemberRegisterDetectInterrupts ( device );
	result = TRUE;
#else
	if ( 0 == mRegisterDetectInterruptsThread )
	{
		mRegisterDetectInterruptsThread = thread_call_allocate ( (thread_call_func_t ) PlatformInterface::threadedRegisterDetectInterrupts, ( thread_call_param_t ) this );
	}

	if ( 0 != mRegisterDetectInterruptsThread )
	{
		thread_call_enter1 ( mRegisterDetectInterruptsThread, ( thread_call_param_t ) device );
		result = true;
	}
#endif

Exit:
	debugIOLog ( 3, "- PlatformInterface[%ld]::registerDetectInterrupts ( IOService * device %p ) ", mInstanceIndex, device );
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::threadedRegisterDetectInterrupts ( PlatformInterface *self, IOService * device )
{
	return self->threadedMemberRegisterDetectInterrupts ( device );
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::threadedMemberRegisterDetectInterrupts ( IOService * device )
{
	IOReturn	err;
	
	debugIOLog ( 3, "+ PlatformInterface::threadedMemberRegisterDetectInterrupts ( IOService * device %p )", device );
	
	FailIf ( 0 == device, Exit );
	FailIf ( 0 == mProvider, Exit );
	
	if ( !mDetectInterruptsHaveBeenRegistered ) 
	{
		if ( kGPIO_Unknown != getComboInJackTypeConnected () )  
		{
			err = registerInterruptHandler ( device, ( void* ) comboInDetectInterruptHandler, kComboInDetectInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_ComboInJackType_bitAddress );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_ComboInJackType_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterDetectInterrupts kGPIO_Unknown == getComboInJackTypeConnected( ) " );
		}
		
		if ( kGPIO_Unknown != getComboOutJackTypeConnected () )  {
			debugIOLog ( 3, "  Attempting to register comboOutDetectInterruptHandler..." );
			err = registerInterruptHandler ( device, ( void* ) comboOutDetectInterruptHandler, kComboOutDetectInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_ComboOutJackType_bitAddress );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_ComboOutJackType_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterDetectInterrupts kGPIO_Unknown == getComboOutJackTypeConnected( ) " );
		}
		
		if ( kGPIO_Unknown != getHeadphoneConnected () )
		{
			debugIOLog ( 3, "  err = registerInterruptHandler ( device %p, headphoneDetectInterruptHandler %p, kHeadphoneDetectInterrupt %d", device, headphoneDetectInterruptHandler, kHeadphoneDetectInterrupt );
			err = registerInterruptHandler ( device, ( void* ) headphoneDetectInterruptHandler, kHeadphoneDetectInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_HeadphoneDetect_bitAddress );
				debugIOLog ( 3, "  headphoneDetectInterruptHandler has been registered!!!" );
			}
			else
			{
				debugIOLog ( 3, "  headphoneDetectInterruptHandler failed registration!!!" );
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_HeadphoneDetect_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterDetectInterrupts kGPIO_Unknown == getHeadphoneConnected( ) " );
		}
		
		if ( kGPIO_Unknown != getSpeakerConnected () )
		{
			err = registerInterruptHandler ( device, ( void* ) speakerDetectInterruptHandler, kSpeakerDetectInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_SpeakerDetect_bitAddress );
				debugIOLog ( 3, "  speakerDetectInterruptHandler has been registered!!!" );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_SpeakerDetect_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterDetectInterrupts kGPIO_Unknown == getSpeakerConnected( ) " );
		}

		if ( kGPIO_Unknown != getLineInConnected () )
		{
			err = registerInterruptHandler ( device, ( void* ) lineInDetectInterruptHandler, kLineInputDetectInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_LineInDetect_bitAddress );
				debugIOLog ( 3, "  lineInDetectInterruptHandler has been registered!!!" );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_LineInDetect_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterDetectInterrupts kGPIO_Unknown == getLineInConnected( ) " );
		}

		if ( kGPIO_Unknown != getLineOutConnected () )
		{
			err = registerInterruptHandler ( device, ( void* ) lineOutDetectInterruptHandler, kLineOutputDetectInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_LineOutDetect_bitAddress );
				debugIOLog ( 3, "  lineOutDetectInterruptHandler has been registered!!!" );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_LineOutDetect_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterDetectInterrupts kGPIO_Unknown == getLineOutConnected( ) " );
		}
		
		if ( kGPIO_Unknown != getDigitalInConnected ()  && kGPIO_Unknown == getComboInJackTypeConnected () )
		{
			//	IMPORTANT:	Only supported if the digital line input connector is not a combo connector
			//				that is already being supported by the line input handler.
			err = registerInterruptHandler ( device, ( void* ) digitalInDetectInterruptHandler, kDigitalInDetectInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_DigitalInDetect_bitAddress );
				debugIOLog ( 3, "  digitalInDetectInterruptHandler has been registered!!!" );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_DigitalInDetect_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterDetectInterrupts kGPIO_Unknown == getDigitalInConnected( ) " );
		}
		
		if ( kGPIO_Unknown != getDigitalOutConnected ()  && kGPIO_Unknown == getComboOutJackTypeConnected () )
		{
			//	IMPORTANT:	Only supported if the digital line output connector is not a combo connector
			//				that is already being supported by the headphone handler.
			err = registerInterruptHandler ( device, ( void* ) digitalOutDetectInterruptHandler, kDigitalOutDetectInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_DigitalOutDetect_bitAddress );
				debugIOLog ( 3, "  digitalOutDetectInterruptHandler has been registered!!!" );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_DigitalOutDetect_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterDetectInterrupts kGPIO_Unknown == getDigitalOutConnected( ) " );
		}

		mDetectInterruptsHaveBeenRegistered = TRUE;
		checkDetectStatus ( device );
	}

Exit:
	debugIOLog ( 3, "- PlatformInterface::threadedMemberRegisterDetectInterrupts ( IOService * device %p )", device );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	PlatformInterface::registerNonDetectInterrupts ( IOService * device )
{
	bool		result = false;
	
	debugIOLog ( 3, "+ PlatformInterface[%ld]::registerNonDetectInterrupts ( IOService * device %p ) ", mInstanceIndex, device );

	FailIf ( 0 == device, Exit );

#if 1
	threadedMemberRegisterNonDetectInterrupts ( device );
	result = TRUE;
#else
	if ( 0 == mRegisterNonDetectInterruptsThread )
	{
		mRegisterNonDetectInterruptsThread = thread_call_allocate ( (thread_call_func_t ) PlatformInterface::threadedRegisterNonDetectInterrupts, ( thread_call_param_t ) this );
	}

	if ( 0 != mRegisterNonDetectInterruptsThread )
	{
		thread_call_enter1 ( mRegisterNonDetectInterruptsThread, ( thread_call_param_t ) device );
		result = true;
	}
#endif

Exit:
	debugIOLog ( 3, "- PlatformInterface[%ld]::registerNonDetectInterrupts ( IOService * device %p ) ", mInstanceIndex, device );
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::threadedRegisterNonDetectInterrupts ( PlatformInterface *self, IOService * device )
{
	return self->threadedMemberRegisterNonDetectInterrupts ( device );
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::threadedMemberRegisterNonDetectInterrupts ( IOService * device )
{
	IOReturn	err;
	
	debugIOLog ( 3, "+ PlatformInterface::threadedMemberRegisterNonDetectInterrupts ( IOService * device %p ), mNonDetectInterruptsHaveBeenRegistered = %d", device, mNonDetectInterruptsHaveBeenRegistered );
	
	FailIf ( 0 == device, Exit );
	FailIf ( 0 == mProvider, Exit );
	
	if ( !mNonDetectInterruptsHaveBeenRegistered ) 
	{
		if ( kGPIO_Unknown != getCodecInterrupt( ) )
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterNonDetectInterrupts kGPIO_Unknown != getCodecInterrupt( ) " );
			err = registerInterruptHandler ( device, ( void* ) codecInterruptHandler, kCodecInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_CodecInterrupt_bitAddress );
				codecInterruptHandler ( device, NULL, 0, 0 );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_CodecInterrupt_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterNonDetectInterrupts kGPIO_Unknown == getCodecInterrupt( ) " );
		}

		if ( kGPIO_Unknown != getCodecErrorInterrupt( ) )
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterNonDetectInterrupts kGPIO_Unknown != getCodecErrorInterrupt( ) " );
			err = registerInterruptHandler ( device, ( void* ) codecErrorInterruptHandler, kCodecErrorInterrupt );
			if ( kIOReturnSuccess == err )
			{
				mGpioMessageFlag |= ( 1 << gpioMessage_CodecErrorInterrupt_bitAddress );
				debugIOLog ( 3, "  codecErrorInterruptHandler has been registered!!!" );
				codecErrorInterruptHandler ( device, NULL, 0, 0 );
			}
			else
			{
				mGpioMessageFlag &= ( ~( 1 << gpioMessage_CodecErrorInterrupt_bitAddress ) );
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog ( 3, "  PlatformInterface::threadedMemberRegisterNonDetectInterrupts kGPIO_Unknown == getCodecErrorInterrupt( ) " );
		}

		mNonDetectInterruptsHaveBeenRegistered = TRUE;
	}

Exit:
	debugIOLog ( 3, "- PlatformInterface::threadedMemberRegisterNonDetectInterrupts ( IOService * device %p ), mNonDetectInterruptsHaveBeenRegistered = %d", device, mNonDetectInterruptsHaveBeenRegistered );
	return;
}


//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::unregisterInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source )
{
	IOReturn			result = kIOReturnError;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->unregisterInterruptHandler ( theDevice, interruptHandler, source );
	}
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::unregisterDetectInterrupts ( void )
{
	debugIOLog ( 6, "+ PlatformInterface::unregisterDetectInterrupts ()" );
	if ( mDetectInterruptsHaveBeenRegistered )  {
		if ( kGPIO_Unknown != getComboInJackTypeConnected () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)comboInDetectInterruptHandler, kComboInDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getComboOutJackTypeConnected () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)comboOutDetectInterruptHandler, kComboOutDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getHeadphoneConnected () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)headphoneDetectInterruptHandler, kHeadphoneDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getSpeakerConnected () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)speakerDetectInterruptHandler, kSpeakerDetectInterrupt );
		}
		if ( kGPIO_Unknown != getLineInConnected () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)lineInDetectInterruptHandler, kLineInputDetectInterrupt );
		}
		if ( kGPIO_Unknown != getLineOutConnected () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)lineOutDetectInterruptHandler, kLineOutputDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getDigitalInConnected ()  && kGPIO_Unknown == getComboInJackTypeConnected () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)digitalInDetectInterruptHandler, kDigitalInDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getDigitalOutConnected ()  && kGPIO_Unknown == getComboOutJackTypeConnected () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)digitalOutDetectInterruptHandler, kDigitalOutDetectInterrupt );
		}
		
		mDetectInterruptsHaveBeenRegistered = FALSE;
	}
	debugIOLog ( 6, "- PlatformInterface::unregisterDetectInterrupts ()" );
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::unregisterNonDetectInterrupts ( void )
{
	debugIOLog ( 6, "+ PlatformInterface::unregisterNonDetectInterrupts ()" );
	if ( mNonDetectInterruptsHaveBeenRegistered )  {
		if ( kGPIO_Unknown != getCodecInterrupt () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)codecInterruptHandler, kCodecInterrupt );
		}
		
		if ( kGPIO_Unknown != getCodecErrorInterrupt () )
		{
			unregisterInterruptHandler ( (IOService*)this, (void*)codecErrorInterruptHandler, kCodecErrorInterrupt );
		}

		mNonDetectInterruptsHaveBeenRegistered = FALSE;
	}
	debugIOLog ( 6, "- PlatformInterface::unregisterNonDetectInterrupts ()" );
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::setWorkLoop ( IOWorkLoop* inWorkLoop )
{
	debugIOLog ( 5, "+ PlatformInterface::setWorkLoop ( %p )", inWorkLoop );
	FailIf ( 0 == inWorkLoop, Exit );
	
	mWorkLoop = inWorkLoop;
	
	if ( 0 != platformInterfaceDBDMA )
	{
		platformInterfaceDBDMA->setWorkLoop ( inWorkLoop );
	}
	if ( 0 != platformInterfaceFCR )
	{
		platformInterfaceFCR->setWorkLoop ( inWorkLoop );
	}
	if ( 0 != platformInterfaceGPIO )
	{
		platformInterfaceGPIO->setWorkLoop ( inWorkLoop );
	}
	if ( 0 != platformInterfaceI2C )
	{
		platformInterfaceI2C->setWorkLoop ( inWorkLoop );
	}
	if ( 0 != platformInterfaceI2S )
	{
		platformInterfaceI2S->setWorkLoop ( inWorkLoop );
	}
Exit:
	debugIOLog ( 5, "- PlatformInterface::setWorkLoop ( %p )", inWorkLoop );
	return;
}					



#pragma mark ---------------------------
#pragma mark INTERRUPT HANDLERS
#pragma mark ---------------------------

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::codecErrorInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *		cg;
	
	debugIOLog ( 6, "+ codecErrorInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kCodecErrorInterruptStatus, (void*) platformInterface->getCodecErrorInterrupt () , (void*) 0 );
	}
Exit:
	debugIOLog ( 6, "- codecErrorInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::codecInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *		cg;

	debugIOLog ( 6, "+ codecInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kCodecInterruptStatus, (void*) platformInterface->getCodecInterrupt () , (void*) 0 );
	}
Exit:
	debugIOLog ( 6, "- codecInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3517297]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::comboInDetectInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	
	debugIOLog ( 6, "+ comboInDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );

	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		IOSleep ( 10 );
		if ( kGPIO_Selector_LineInDetect == platformInterface->getComboInAssociation () )
		{
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface,
													  platformInterface->getLineInConnected () , 
													  platformInterface->getComboInJackTypeConnected () , 
													  kGPIO_Selector_LineInDetect 
													 );
		}
		else
		{
			FailMessage ( TRUE );
		}
	}
Exit:
	debugIOLog ( 6, "- comboInDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3517297]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::comboOutDetectInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	
	debugIOLog ( 6, "+ comboOutDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );

	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		IOSleep ( 10 );
		if ( kGPIO_Selector_HeadphoneDetect == platformInterface->getComboOutAssociation () )
		{
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  platformInterface->getHeadphoneConnected () , 
													  platformInterface->getComboOutJackTypeConnected () , 
													  kGPIO_Selector_HeadphoneDetect 
													 );
		}
		else if ( kGPIO_Selector_LineOutDetect == platformInterface->getComboOutAssociation () )
		{
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  platformInterface->getLineOutConnected () , 
													  platformInterface->getComboOutJackTypeConnected () , 
													  kGPIO_Selector_LineOutDetect 
													 );
		}
		else
		{
			FailMessage ( TRUE );
		}
	}
Exit:
	debugIOLog ( 6, "- comboOutDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::digitalInDetectInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	
	debugIOLog ( 6, "+ digitalInDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );

	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		if ( kGPIO_Connected == platformInterface->getDigitalInConnected () )
		{
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalInInsertStatus, (void*) platformInterface->getDigitalInConnected () , (void*) 0 );
		}
		else
		{
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalInRemoveStatus, (void*) platformInterface->getDigitalInConnected () , (void*) 0 );
		}
	}
Exit:
	debugIOLog ( 6, "- digitalInDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::digitalOutDetectInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *		cg;
	
	debugIOLog ( 6, "+ digitalOutDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalOutStatus, (void*) platformInterface->getDigitalOutConnected () , (void*) 0 );
	}
Exit:
	debugIOLog ( 6, "- digitalOutDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::headphoneDetectInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;

	debugIOLog ( 6, "+ PlatformInterface::headphoneDetectInterruptHandler ( OSObject * owner %p, IOInterruptEventSource * source %p, UInt32 count %ld, void * arg4 %p ) ", owner, source, count, arg4 );
	
	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		IOSleep ( 10 );
		theAnalogJackState = platformInterface->getHeadphoneConnected ();
		if ( kGPIO_Selector_HeadphoneDetect != platformInterface->getComboOutAssociation () )
		{
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kHeadphoneStatus, (void*) theAnalogJackState, (void*) 0 );
		}
		else
		{
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  theAnalogJackState, 
													  platformInterface->getComboOutJackTypeConnected (), 
													  kGPIO_Selector_HeadphoneDetect 
													 );
		}
	}
Exit:
	debugIOLog ( 6, "- PlatformInterface::headphoneDetectInterruptHandler ( OSObject * owner %p, IOInterruptEventSource * source %p, UInt32 count %ld, void * arg4 %p ) ", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::lineInDetectInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;
	
	debugIOLog ( 6, "+ lineInDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );

	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		IOSleep ( 10 );
		theAnalogJackState = platformInterface->getLineInConnected ();
		if ( kGPIO_Selector_LineInDetect != platformInterface->getComboInAssociation () )
		{
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineInStatus, (void*) theAnalogJackState, (void*) 0 );
		}
		else
		{
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  theAnalogJackState, 
													  platformInterface->getComboInJackTypeConnected () , 
													  kGPIO_Selector_LineInDetect 
													 );
		}
	}
Exit:
	debugIOLog ( 6, "- lineInDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::lineOutDetectInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;
	
	debugIOLog ( 5, "+ PlatformInterface::lineOutDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );

	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		IOSleep ( 10 );
		theAnalogJackState = platformInterface->getLineOutConnected ();
		if ( kGPIO_Selector_LineOutDetect != platformInterface->getComboOutAssociation () )  {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineOutStatus, (void*) theAnalogJackState, (void*) 0 );
		}
		else
		{
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  theAnalogJackState, 
													  platformInterface->getComboOutJackTypeConnected () , 
													  kGPIO_Selector_LineOutDetect 
													 );
		}
	}
Exit:

	debugIOLog ( 5, "- PlatformInterface::lineOutDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::poll ( void )
{
	if ( 0 != platformInterfaceGPIO ) 
	{
		platformInterfaceGPIO->poll ();
	}
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Radar 3517297
//	Arguments:		detectState		Indicates the state of the analog detect ( for example, the Line Output
//									detect ) .  Values may be either kGPIO_Connected or kGPIO_Disconnected.
//
//					typeSenseState	Indicates the state of the metal / plastic sense.  Values may be either
//									kGPIO_TypeIsAnalog or kGPIO_TypeIsDigital.
//
//					analogJackType	Indicates the type of analog jack that is serving as a combo jack.  May
//									include kGPIO_HeadphoneDetect, kGPIO_LineInDetect, kGPIO_LineOutDetect,
//									kGPIO_SpeakerDetect.  Note that the 'analogJackType' indicates whether
//									an input jack or output jack is in use and is used to determine whether
//									a digital input or digital output message is to be posted when the
//									'typeSenseState' indicates kGPIO_TypeIsDigital.
//
void PlatformInterface::RunComboStateMachine ( IOCommandGate * cg, PlatformInterface * platformInterface, UInt32 detectState, UInt32 typeSenseState, UInt32 analogJackType )
{
	
	FailIf ( 0 == cg, Exit );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );

	debugIOLog ( 5, "+ RunComboStateMachine ( %p, %p, %ld, %ld, %ld ) , power state = %d", cg, platformInterface, detectState, typeSenseState, analogJackType, platformInterface->mProvider->getPowerState () );

	if ( platformInterface->mProvider->getPowerState ()  == kIOAudioDeviceSleep )  goto Exit;		// don't advance the state machine if we are asleep ( we'll run this code on wake ) 

	switch ( mComboStateMachine[analogJackType] )
	{
		case kComboStateMachine_handle_jack_insert:
			//	When no jack is inserted then the only events that can occur is insertion of metal or plastic jacks.
			if ( kGPIO_Connected == detectState )
			{
				if ( kGPIO_TypeIsAnalog == typeSenseState )
				{
					switch ( analogJackType )
					{
						case kGPIO_Selector_LineInDetect:
							debugIOLog ( 5, "  RunComboStateMachine 'Handle Jack Insert' posting ANALOG INSERT of 'Line Input' jack" );
							cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineInStatus, (void*) kGPIO_Connected, (void*) 0 );
							break;
						case kGPIO_Selector_LineOutDetect:
							debugIOLog ( 5, "  RunComboStateMachine 'Handle Jack Insert' posting ANALOG INSERT of 'Line Output' jack" );
							cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineOutStatus, (void*) kGPIO_Connected, (void*) 0 );
							break;
						case kGPIO_Selector_HeadphoneDetect:
							debugIOLog ( 5, "  RunComboStateMachine 'Handle Jack Insert' posting ANALOG INSERT of 'Line Headphone' jack" );
							cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kHeadphoneStatus, (void*) kGPIO_Connected, (void*) 0 );
							break;
					}
					mComboStateMachine[analogJackType] = kComboStateMachine_handle_metal_change;
				} 
				else 
				{
					debugIOLog ( 5, "kGPIO_TypeIsAnalog != typeSenseState" );
					if ( kGPIO_Selector_LineInDetect == analogJackType )
					{
						debugIOLog ( 5, "  RunComboStateMachine 'Handle Jack Insert' posting DIGITAL INPUT INSERT" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalInInsertStatus, (void*) kGPIO_Connected, (void*) 0 );
					} 
					else 
					{
						debugIOLog ( 5, "  RunComboStateMachine 'Handle Jack Insert' posting DIGITAL OUTPUT INSERT" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalOutStatus, (void*) kGPIO_Connected, (void*) 0 );
					}
					mComboStateMachine[analogJackType] = kComboStateMachine_handle_plastic_change;
				}
			}
			break;
		case kComboStateMachine_handle_metal_change:
			//	[3564007]
			switch ( analogJackType )
			{
				case kGPIO_Selector_LineInDetect:
					debugIOLog ( 5, "  RunComboStateMachine 'Handle Metal Change' posting ANALOG EXTRACT of 'Line Input' jack" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineInStatus, (void*) kGPIO_Disconnected, (void*) 0 );
					break;
				case kGPIO_Selector_LineOutDetect:
					debugIOLog ( 5, "  RunComboStateMachine 'Handle Metal Change' posting ANALOG EXTRACT of 'Line Output' jack" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineOutStatus, (void*) kGPIO_Disconnected, (void*) 0 );
					break;
				case kGPIO_Selector_HeadphoneDetect:
					debugIOLog ( 5, "  RunComboStateMachine 'Handle Metal Change' posting ANALOG EXTRACT of 'Line Headphone' jack" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kHeadphoneStatus, (void*) kGPIO_Disconnected, (void*) 0 );
					break;
			}
			if ( kGPIO_Disconnected == detectState )
			{
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_jack_insert;
			}
			else if ( kGPIO_TypeIsAnalog == typeSenseState )
			{
				switch ( analogJackType )
				{
					case kGPIO_Selector_LineInDetect:
						debugIOLog ( 5, "  RunComboStateMachine 'Handle Metal Change' posting ANALOG INSERT of 'Line Input' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineInStatus, (void*) kGPIO_Connected, (void*) 0 );
						break;
					case kGPIO_Selector_LineOutDetect:
						debugIOLog ( 5, "  RunComboStateMachine 'Handle Metal Change' posting ANALOG INSERT of 'Line Output' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineOutStatus, (void*) kGPIO_Connected, (void*) 0 );
						break;
					case kGPIO_Selector_HeadphoneDetect:
						debugIOLog ( 5, "  RunComboStateMachine 'Handle Metal Change' posting ANALOG INSERT of 'Line Headphone' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kHeadphoneStatus, (void*) kGPIO_Connected, (void*) 0 );
						break;
				}
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_metal_change;
			}
			else
			{
				if ( kGPIO_Selector_LineInDetect == analogJackType )
				{
					debugIOLog ( 5, "  RunComboStateMachine 'Handle Metal Change' posting DIGITAL INPUT INSERT" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalInInsertStatus, (void*) kGPIO_Connected, (void*) 0 );
				}
				else
				{
					debugIOLog ( 5, "  RunComboStateMachine 'Handle Metal Change' posting DIGITAL OUTPUT INSERT" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalOutStatus, (void*) kGPIO_Connected, (void*) 0 );
				}
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_plastic_change;
			}
			break;
		case kComboStateMachine_handle_plastic_change:
			//	When plastic is inserted then the only events that can occur is that the plastic can be extracted or
			//	the jack type can change to metal.  In either case, digital jack extraction must be posted.
			if ( kGPIO_Selector_LineInDetect == analogJackType )	//	Check is to determine INPUT v.s. OUTPUT ( there is only one kind of input connector ) 
			{
				debugIOLog ( 5, "  RunComboStateMachine 'Handle Plastic Change' posting DIGITAL INPUT EXTRACT" );
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalInRemoveStatus, (void*) kGPIO_Disconnected, (void*) 0 );
			}
			else
			{
				debugIOLog ( 5, "  RunComboStateMachine 'Handle Plastic Change' posting DIGITAL OUTPUT EXTRACT" );
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalOutStatus, (void*) kGPIO_Disconnected, (void*) 0 );
			}
			if ( kGPIO_Disconnected == detectState )
			{
				//	Handle digital jack extraction
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_jack_insert;
			}
			else if ( kGPIO_TypeIsAnalog == typeSenseState )
			{
				//	Handle direct transition from plastic digital jack to metal analog jack
				debugIOLog ( 5, "  RunComboStateMachine 'Handle Plastic Change' posting change to DIGITAL INPUT INSERT" );
				switch ( analogJackType )
				{
					case kGPIO_Selector_LineInDetect:
						debugIOLog ( 5, "  RunComboStateMachine 'Handle Plastic Change' posting ANALOG INSERT of 'Line Input' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineInStatus, (void*) kGPIO_Connected, (void*) 0 );
						break;
					case kGPIO_Selector_LineOutDetect:
						debugIOLog ( 5, "  RunComboStateMachine 'Handle Plastic Change' posting ANALOG INSERT of 'Line Output' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kLineOutStatus, (void*) kGPIO_Connected, (void*) 0 );
						break;
					case kGPIO_Selector_HeadphoneDetect:
						debugIOLog ( 5, "  RunComboStateMachine 'Handle Plastic Change' posting ANALOG INSERT of 'Line Headphone' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kHeadphoneStatus, (void*) kGPIO_Connected, (void*) 0 );
						break;
				}
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_metal_change;
			}
			else
			{
				if ( kGPIO_Selector_LineInDetect == analogJackType )
				{
					debugIOLog ( 5, "  RunComboStateMachine 'Handle Plastic Change' posting DIGITAL INPUT INSERT" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalInInsertStatus, (void*) kGPIO_Connected, (void*) 0 );
				}
				else
				{
					debugIOLog ( 5, "  RunComboStateMachine 'Handle Plastic Change' posting DIGITAL OUTPUT INSERT" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kDigitalOutStatus, (void*) kGPIO_Connected, (void*) 0 );
				}
			}
			break;
	}
	
	debugIOLog ( 5, "- RunComboStateMachine ( %p, %p, %ld, %ld, %ld ) ", cg, platformInterface, detectState, typeSenseState, analogJackType );
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::speakerDetectInterruptHandler ( OSObject * owner, IOInterruptEventSource * source, UInt32 count, void * arg4 )
{
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;
	
	debugIOLog ( 6, "+ speakerDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );

	platformInterface = OSDynamicCast ( PlatformInterface, owner );
	FailIf ( 0 == platformInterface, Exit );
	FailIf ( 0 == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		IOSleep ( 10 );
		theAnalogJackState = platformInterface->getSpeakerConnected ();
		if ( kGPIO_Selector_SpeakerDetect != platformInterface->getComboOutAssociation () )
		{
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*) kExtSpeakersStatus, (void*) theAnalogJackState, (void*) 0 );
		}
		else
		{
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  theAnalogJackState, 
													  platformInterface->getComboOutJackTypeConnected () , 
													  kGPIO_Selector_SpeakerDetect 
													 );
		}
	}
	
Exit:
	debugIOLog ( 6, "- speakerDetectInterruptHandler ( %p, %p, %ld, %p ) ", owner, source, count, arg4 );
	return;
}


#pragma mark ---------------------------
#pragma mark Codec Methods	
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::readCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ) {
	IOReturn		result = kIOReturnError;
	
	if ( platformInterfaceI2C )
	{
		result = platformInterfaceI2C->readCodecRegister ( codecRef, subAddress, data, dataLength );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::writeCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ) {
	IOReturn		result = kIOReturnError;
	
	if ( platformInterfaceI2C )
	{
		result = platformInterfaceI2C->WriteCodecRegister ( codecRef, subAddress, data, dataLength );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}


//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getSavedMAP ( UInt32 codecRef )
{
	UInt32			result = 0xFFFFFFFF;
	
	if ( platformInterfaceI2C )
	{
		result = platformInterfaceI2C->getSavedMAP ( codecRef );
	}
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setMAP ( UInt32 codecRef, UInt8 subAddress )
{
	IOReturn		result = kIOReturnError;
	
	if ( platformInterfaceI2C )
	{
		result = platformInterfaceI2C->setMAP ( codecRef, subAddress );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

#pragma mark ---------------------------
#pragma mark GPIO Support	
#pragma mark ---------------------------

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn PlatformInterface::disableInterrupt ( IOService * device, PlatformInterruptSource source )
{
	IOReturn		result = kIOReturnError;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->disableInterrupt ( device, source );
	}
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3514762]
void PlatformInterface::enableAmplifierMuteRelease ( void )
{
	debugIOLog ( 3, "+ PlatformInterface::enableAmplifierMuteRelease ()" );
	FailIf ( 0 == platformInterfaceGPIO, Exit )
	platformInterfaceGPIO->enableAmplifierMuteRelease ();
Exit:
	debugIOLog ( 3, "- PlatformInterface::enableAmplifierMuteRelease ()" );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn PlatformInterface::enableInterrupt ( IOService * device, PlatformInterruptSource source )
{
	IOReturn		result = kIOReturnError;
	
	if ( platformInterfaceGPIO )
	{

		result = platformInterfaceGPIO->enableInterrupt ( device, source );
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getClockMux ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getClockMux ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getCodecErrorInterrupt ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getCodecErrorInterrupt ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getCodecInterrupt ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getCodecInterrupt ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getCodecReset ( CODEC_RESET target )
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getCodecReset ( target );
	}
	return result;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
GpioAttributes PlatformInterface::getComboIn ( void )
{
	return mComboInJackState;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
GPIOSelector PlatformInterface::getComboInAssociation ( void )
{
	return mComboInAssociation;
}

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getComboInJackTypeConnected ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getComboInJackTypeConnected ();
	}
	return result;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
GpioAttributes PlatformInterface::getComboOut ( void )
{
	return mComboOutJackState;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
GPIOSelector PlatformInterface::getComboOutAssociation ( void )
{
	debugIOLog ( 6, " ± PlatformInterface::getComboOutAssociation () returns %d", mComboOutAssociation );
	return mComboOutAssociation;
}

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getComboOutJackTypeConnected ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getComboOutJackTypeConnected ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getDigitalInConnected ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getDigitalInConnected ( getComboInAssociation () );
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getDigitalOutConnected ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	debugIOLog ( 6, "+ PlatformInterface::getDigitalOutConnected ()" );
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getDigitalOutConnected ( getComboOutAssociation () );
	}
	debugIOLog ( 6, "- PlatformInterface::getDigitalOutConnected () returns %d", result );
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getHeadphoneConnected ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getHeadphoneConnected ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getHeadphoneMuteState ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getHeadphoneMuteState ();
	}
	return result;
}
	

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getInputDataMux ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getInputDataMux ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getInternalSpeakerID ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getInternalSpeakerID ();
	}
	return result;
}
	
	
//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getLineInConnected ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getLineInConnected ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getLineOutConnected ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getLineOutConnected ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getLineOutMuteState ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getLineOutMuteState ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes	PlatformInterface::getSpeakerConnected ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getSpeakerConnected ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterface::getSpeakerMuteState ()
{
	GpioAttributes			result = kGPIO_Unknown;
	
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->getSpeakerMuteState ();
	}
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
void PlatformInterface::setAssociateComboInTo ( GPIOSelector theDetectInterruptGpio )
{
	if ( ( kGPIO_Selector_LineInDetect == theDetectInterruptGpio )  || ( kGPIO_Selector_ExternalMicDetect == theDetectInterruptGpio ) )
	{
		mComboInAssociation = theDetectInterruptGpio;
	}
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
void PlatformInterface::setAssociateComboOutTo ( GPIOSelector theDetectInterruptGpio )
{
	debugIOLog ( 6, "+ PlatformInterface::setAssociateComboOutTo ( %d )", theDetectInterruptGpio );
	switch ( theDetectInterruptGpio )
	{
		case kGPIO_Selector_LineOutDetect:
			debugIOLog ( 6, "  combo out associated to kGPIO_Selector_LineOutDetect" );
			mComboOutAssociation = theDetectInterruptGpio;
			break;
		case kGPIO_Selector_HeadphoneDetect:
			debugIOLog ( 6, "  combo out associated to kGPIO_Selector_HeadphoneDetect" );
			mComboOutAssociation = theDetectInterruptGpio;
			break;
		case kGPIO_Selector_SpeakerDetect:
			debugIOLog ( 6, "  combo out associated to kGPIO_Selector_SpeakerDetect" );
			mComboOutAssociation = theDetectInterruptGpio;
			break;
		default:
			break;
	}
	debugIOLog ( 6, "- PlatformInterface::setAssociateComboOutTo ( %d )", theDetectInterruptGpio );
}


//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setClockMux ( GpioAttributes muxState )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setClockMux ( %d )", muxState );
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->setClockMux ( muxState );
	}
	debugIOLog ( 6, "- PlatformInterface::setClockMux ( %d ) returns 0x%X", muxState, result );
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setCodecReset ( CODEC_RESET target, GpioAttributes reset )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setCodecReset ( %d, %d )", target, reset );
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->setCodecReset ( target, reset );
	}
	debugIOLog ( 6, "- PlatformInterface::setCodecReset ( %d, %d ) returns 0x%X", target, reset, result );
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
void PlatformInterface::setComboIn ( GpioAttributes jackState )
{
	if ( ( kGPIO_TypeIsDigital == jackState )  || ( kGPIO_TypeIsAnalog == jackState ) )
	{
		mComboInJackState = jackState;
	}
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
void PlatformInterface::setComboOut ( GpioAttributes jackState )
{
	if ( ( kGPIO_TypeIsDigital == jackState )  || ( kGPIO_TypeIsAnalog == jackState ) )
	{
		debugIOLog ( 5, "combo out type set to %d", jackState );
		mComboOutJackState = jackState;
	}
}



//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setHeadphoneMuteState ( GpioAttributes muteState )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setHeadphoneMuteState ( %d )", muteState );
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->setHeadphoneMuteState ( muteState );
	}
	debugIOLog ( 6, "- PlatformInterface::setHeadphoneMuteState ( %d ) returns 0x%X", muteState, result );
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setInputDataMux ( GpioAttributes muxState )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setInputDataMux ( %d )", muxState );
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->setInputDataMux ( muxState );
	}
	debugIOLog ( 6, "- PlatformInterface::setInputDataMux ( %d ) returns 0x%X", muxState, result );
	return result;
}
	
	
//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setLineOutMuteState ( GpioAttributes muteState )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setLineOutMuteState ( %d )", muteState );
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->setLineOutMuteState ( muteState );
	}
	debugIOLog ( 6, "- PlatformInterface::setLineOutMuteState ( %d ) returns 0x%X", muteState, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setSpeakerMuteState ( GpioAttributes muteState )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setSpeakerMuteState ( %d )", muteState );
	if ( platformInterfaceGPIO )
	{
		result = platformInterfaceGPIO->setSpeakerMuteState ( muteState );
	}
	debugIOLog ( 6, "- PlatformInterface::setSpeakerMuteState ( %d ) returns 0x%X", muteState, result );
	return result;
}


#pragma mark ---------------------------
#pragma mark I2S - FCR
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
bool PlatformInterface::getI2SCellEnable ()
{
	bool		result = FALSE;
	
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->getI2SCellEnable ();
	}
	return result;
}


//	--------------------------------------------------------------------------------
bool PlatformInterface::getI2SClockEnable ()
{
	bool		result = FALSE;
	
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->getI2SClockEnable ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
bool PlatformInterface::getI2SEnable ()
{
	bool		result = FALSE;
	
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->getI2SEnable ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
bool PlatformInterface::getI2SSWReset ()
{
	bool		result = FALSE;
	
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->getI2SSWReset ();
	}
	return result;
}
	

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn PlatformInterface::releaseI2SClockSource ( I2SClockFrequency inFrequency )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::releaseI2SClockSource ( %d )", inFrequency );
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->releaseI2SClockSource ( inFrequency );
	}
	debugIOLog ( 6, "- PlatformInterface::releaseI2SClockSource ( %d ) returns 0x%X", inFrequency, result );
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn PlatformInterface::requestI2SClockSource ( I2SClockFrequency inFrequency )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::requestI2SClockSource ( %d )", inFrequency );
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->requestI2SClockSource ( inFrequency );
	}
	debugIOLog ( 6, "- PlatformInterface::requestI2SClockSource ( %d ) returns 0x%X", inFrequency, result );
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SCellEnable ( bool enable )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SCellEnable ( %d )", enable );
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->setI2SCellEnable ( enable );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SCellEnable ( %d ) returns 0x%X", enable, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SClockEnable ( bool enable )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SClockEnable ( %d )", enable );
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->setI2SClockEnable ( enable );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SClockEnable ( %d ) returns 0x%X", enable, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SEnable ( bool enable )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SEnable ( %d )", enable );
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->setI2SEnable ( enable );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SEnable ( %d ) returns 0x%X", enable, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SSWReset ( bool enable )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SSWReset ( %d )", enable );
	if ( platformInterfaceFCR )
	{
		result = platformInterfaceFCR->setI2SSWReset ( enable );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SSWReset ( %d ) returns 0x%X", enable, result );
	return result;
}


#pragma mark ---------------------------
#pragma mark  I2S
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getDataWordSizes ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getDataWordSizes ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getFrameCount ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getFrameCount ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getI2SIOM_CodecMsgIn ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getI2SIOM_CodecMsgIn ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getI2SIOM_CodecMsgOut ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getI2SIOM_CodecMsgOut ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getI2SIOM_FrameMatch ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getI2SIOM_FrameMatch ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getI2SIOM_PeakLevelIn0 ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getI2SIOM_PeakLevelIn0 ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getI2SIOM_PeakLevelIn1 ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getI2SIOM_PeakLevelIn1 ();
	}
	return result;
}

	
//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getI2SIOM_PeakLevelSel ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getI2SIOM_PeakLevelSel ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getI2SIOMIntControl ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getI2SIOMIntControl ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getPeakLevel ( UInt32 channelTarget )
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getPeakLevel ( channelTarget );
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterface::getSerialFormatRegister ()
{
	UInt32			result = 0;
	
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->getSerialFormatRegister ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setDataWordSizes ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setDataWordSizes ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setDataWordSizes ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setDataWordSizes ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setFrameCount ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setFrameCount ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setFrameCount ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setFrameCount ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SIOM_CodecMsgIn ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SIOM_CodecMsgIn ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setI2SIOM_CodecMsgIn ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SIOM_CodecMsgIn ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SIOM_CodecMsgOut ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SIOM_CodecMsgOut ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setI2SIOM_CodecMsgOut ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SIOM_CodecMsgOut ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SIOM_FrameMatch ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SIOM_FrameMatch ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setI2SIOM_FrameMatch ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SIOM_FrameMatch ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SIOM_PeakLevelIn0 ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SIOM_PeakLevelIn0 ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setI2SIOM_PeakLevelIn0 ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SIOM_PeakLevelIn0 ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SIOM_PeakLevelIn1 ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SIOM_PeakLevelIn1 ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setI2SIOM_PeakLevelIn1 ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SIOM_PeakLevelIn1 ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SIOM_PeakLevelSel ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SIOM_PeakLevelSel ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setI2SIOM_PeakLevelSel ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SIOM_PeakLevelSel ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setI2SIOMIntControl ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setI2SIOMIntControl ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setI2SIOMIntControl ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setI2SIOMIntControl ( 0x%X ) returns 0x%X", value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setPeakLevel ( UInt32 channelTarget, UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setPeakLevel ( %d, 0x%X )", channelTarget, value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setPeakLevel ( channelTarget, value );
	}
	debugIOLog ( 6, "- PlatformInterface::setPeakLevel ( %d, 0x%X ) returns 0x%X", channelTarget, value, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterface::setSerialFormatRegister ( UInt32 value )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterface::setSerialFormatRegister ( 0x%X )", value );
	if ( platformInterfaceI2S )
	{
		result = platformInterfaceI2S->setSerialFormatRegister ( value );
	}
	debugIOLog ( 6, "- PlatformInterface::setSerialFormatRegister ( 0x%X ) returns 0x%X", value, result );
	return result;
}


#pragma mark ---------------------------
#pragma mark DBDMA
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterface::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider )
{
	IODBDMAChannelRegisters *		result = 0;
	
	if ( platformInterfaceDBDMA )
	{
		result = platformInterfaceDBDMA->GetInputChannelRegistersVirtualAddress ( dbdmaProvider );
	}
	return result;
}


//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	PlatformInterface::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider )
{
	IODBDMAChannelRegisters *		result = 0;
	
	if ( platformInterfaceDBDMA )
	{
		result = platformInterfaceDBDMA->GetOutputChannelRegistersVirtualAddress ( dbdmaProvider );
	}
	return result;
}

#pragma mark ---------------------------
#pragma mark ¥ USER CLIENT SUPPORT
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn	PlatformInterface::getPlatformState ( PlatformStateStructPtr outState ) {
	IOReturn			result = kIOReturnBadArgument;
	
	FailIf ( NULL == outState, Exit );
	outState->platformType = kPlatformInterfaceType_Shasta;
	
	for ( UInt32 index = 0; index < sizeof ( PlatformStateStruct ); index++ ) {
		((UInt8*)outState)[0] = 0;
	}
	
	if ( platformInterfaceDBDMA ) {
	}
	
	if ( platformInterfaceFCR ) {
		outState->fcr.i2sEnable = platformInterfaceFCR->getI2SEnable ();
		outState->fcr.i2sClockEnable = platformInterfaceFCR->getI2SClockEnable ();
		outState->fcr.i2sReset = 0;
		outState->fcr.i2sCellEnable = platformInterfaceFCR->getI2SCellEnable ();
		outState->fcr.clock18mHzEnable = 1;
		outState->fcr.clock45mHzEnable = 1;
		outState->fcr.clock49mHzEnable = 1;
		outState->fcr.pll45mHzShutdown = 0;
		outState->fcr.pll49mHzShutdown = 0;
	}
	
	if ( platformInterfaceGPIO ) {
		outState->gpio.gpio_AnalogCodecReset = platformInterfaceGPIO->getCodecReset ( kCODEC_RESET_Analog );
		outState->gpio.gpio_ClockMux = platformInterfaceGPIO->getClockMux ();
		outState->gpio.gpio_CodecInterrupt = platformInterfaceGPIO->getCodecInterrupt ();
		outState->gpio.gpio_CodecErrorInterrupt = platformInterfaceGPIO->getCodecErrorInterrupt ();
		outState->gpio.gpio_ComboInJackType = platformInterfaceGPIO->getComboInJackTypeConnected ();
		outState->gpio.gpio_ComboOutJackType = platformInterfaceGPIO->getComboOutJackTypeConnected ();
		outState->gpio.gpio_DigitalCodecReset = platformInterfaceGPIO->getCodecReset ( kCODEC_RESET_Digital );
		outState->gpio.gpio_DigitalInDetect = getDigitalInConnected ();
		outState->gpio.gpio_DigitalOutDetect = getDigitalOutConnected ();
		outState->gpio.gpio_HeadphoneDetect = platformInterfaceGPIO->getHeadphoneConnected ();
		outState->gpio.gpio_HeadphoneMute = platformInterfaceGPIO->getHeadphoneMuteState ();
		outState->gpio.gpio_InputDataMux = platformInterfaceGPIO->getInputDataMux ();
		outState->gpio.gpio_InternalSpeakerID = platformInterfaceGPIO->getInternalSpeakerID ();
		outState->gpio.gpio_LineInDetect = platformInterfaceGPIO->getLineInConnected ();
		outState->gpio.gpio_LineOutDetect = platformInterfaceGPIO->getLineOutConnected ();
		outState->gpio.gpio_LineOutMute = platformInterfaceGPIO->getLineOutMuteState ();
		outState->gpio.gpio_SpeakerDetect = platformInterfaceGPIO->getSpeakerConnected ();
		outState->gpio.gpio_SpeakerMute = platformInterfaceGPIO->getSpeakerMuteState ();
		outState->gpio.gpio_ComboInAssociation = getComboInAssociation ();
		outState->gpio.gpio_ComboOutAssociation = getComboOutAssociation ();
		
		outState->gpio.reserved_20 = kGPIO_Unknown;
		outState->gpio.reserved_21 = kGPIO_Unknown;
		outState->gpio.reserved_22 = kGPIO_Unknown;
		outState->gpio.reserved_23 = kGPIO_Unknown;
		outState->gpio.reserved_24 = kGPIO_Unknown;
		outState->gpio.reserved_25 = kGPIO_Unknown;
		outState->gpio.reserved_26 = kGPIO_Unknown;
		outState->gpio.reserved_27 = kGPIO_Unknown;
		outState->gpio.reserved_28 = kGPIO_Unknown;
		outState->gpio.reserved_29 = kGPIO_Unknown;
		outState->gpio.reserved_30 = kGPIO_Unknown;
		outState->gpio.gpioMessageFlags = mGpioMessageFlag;
	} else {
		outState->gpio.gpio_AnalogCodecReset = kGPIO_Unknown;
		outState->gpio.gpio_ClockMux = kGPIO_Unknown;
		outState->gpio.gpio_CodecInterrupt = kGPIO_Unknown;
		outState->gpio.gpio_CodecErrorInterrupt = kGPIO_Unknown;
		outState->gpio.gpio_ComboInJackType = kGPIO_Unknown;
		outState->gpio.gpio_ComboOutJackType = kGPIO_Unknown;
		outState->gpio.gpio_DigitalCodecReset = kGPIO_Unknown;
		outState->gpio.gpio_DigitalOutDetect = kGPIO_Unknown;
		outState->gpio.gpio_HeadphoneDetect = kGPIO_Unknown;
		outState->gpio.gpio_HeadphoneMute = kGPIO_Unknown;
		outState->gpio.gpio_InputDataMux = kGPIO_Unknown;
		outState->gpio.gpio_InternalSpeakerID = kGPIO_Unknown;
		outState->gpio.gpio_LineInDetect = kGPIO_Unknown;
		outState->gpio.gpio_LineOutDetect = kGPIO_Unknown;
		outState->gpio.gpio_LineOutMute = kGPIO_Unknown;
		outState->gpio.gpio_SpeakerDetect = kGPIO_Unknown;
		outState->gpio.gpio_SpeakerMute = kGPIO_Unknown;
		outState->gpio.gpio_ComboInAssociation = kGPIO_Selector_NotAssociated;
		outState->gpio.gpio_ComboOutAssociation = kGPIO_Selector_NotAssociated;
		outState->gpio.reserved_20 = kGPIO_Unknown;
		outState->gpio.reserved_21 = kGPIO_Unknown;
		outState->gpio.reserved_22 = kGPIO_Unknown;
		outState->gpio.reserved_23 = kGPIO_Unknown;
		outState->gpio.reserved_24 = kGPIO_Unknown;
		outState->gpio.reserved_25 = kGPIO_Unknown;
		outState->gpio.reserved_26 = kGPIO_Unknown;
		outState->gpio.reserved_27 = kGPIO_Unknown;
		outState->gpio.reserved_28 = kGPIO_Unknown;
		outState->gpio.reserved_29 = kGPIO_Unknown;
		outState->gpio.reserved_30 = kGPIO_Unknown;
		outState->gpio.gpioMessageFlags = mGpioMessageFlag;
	}
	
	if ( platformInterfaceI2C ) {
	}
	
	if ( platformInterfaceI2S ) {
		outState->i2s.intCtrl = platformInterfaceI2S->getI2SIOMIntControl ();
		outState->i2s.serialFmt = platformInterfaceI2S->getSerialFormatRegister ();
		outState->i2s.codecMsgOut = platformInterfaceI2S->getI2SIOM_CodecMsgOut ();
		outState->i2s.codecMsgIn = platformInterfaceI2S->getI2SIOM_CodecMsgIn ();
		outState->i2s.frameCount = platformInterfaceI2S->getFrameCount ();
		outState->i2s.frameCountToMatch = platformInterfaceI2S->getI2SIOM_FrameMatch ();
		outState->i2s.dataWordSizes = platformInterfaceI2S->getDataWordSizes ();
		outState->i2s.peakLevelSfSel = platformInterfaceI2S->getI2SIOM_PeakLevelSel ();
		outState->i2s.peakLevelIn0 = platformInterfaceI2S->getPeakLevel ( kStreamFrontLeft );
		outState->i2s.peakLevelIn1 = platformInterfaceI2S->getPeakLevel ( kStreamFrontRight );
		outState->i2s.newPeakLevelIn0 = platformInterfaceI2S->getI2SIOM_PeakLevelIn0 ();
		outState->i2s.newPeakLevelIn1 = platformInterfaceI2S->getI2SIOM_PeakLevelIn1 ();
	}
	result = kIOReturnSuccess;
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	PlatformInterface::setPlatformState ( PlatformStateStructPtr inState ) {
	IOReturn			result = kIOReturnBadArgument;
	
	FailIf ( NULL == inState, Exit );
	if ( platformInterfaceDBDMA ) {
	}
	
	if ( platformInterfaceFCR ) {
		if ( inState->fcr.i2sEnable != platformInterfaceFCR->getI2SEnable () ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setI2SEnable ( %lX )", inState->fcr.i2sEnable );
			result = platformInterfaceFCR->setI2SEnable ( inState->fcr.i2sEnable );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( inState->fcr.i2sClockEnable != platformInterfaceFCR->getI2SClockEnable () ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setI2SClockEnable ( %lX )", inState->fcr.i2sClockEnable );
			result = platformInterfaceFCR->setI2SClockEnable ( inState->fcr.i2sClockEnable );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( inState->fcr.i2sCellEnable != platformInterfaceFCR->getI2SCellEnable () ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setI2SCellEnable ( %lX )", inState->fcr.i2sCellEnable );
			result = platformInterfaceFCR->setI2SCellEnable ( inState->fcr.i2sCellEnable );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
	
	if ( platformInterfaceGPIO ) {
		if ( kGPIO_Unknown != inState->gpio.gpio_AnalogCodecReset ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setCodecReset ( kCODEC_RESET_Analog, %X )", inState->gpio.gpio_AnalogCodecReset );
			result = platformInterfaceGPIO->setCodecReset ( kCODEC_RESET_Analog, inState->gpio.gpio_AnalogCodecReset );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kGPIO_Unknown != inState->gpio.gpio_ClockMux ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setClockMux ( %X )", inState->gpio.gpio_ClockMux );
			result = platformInterfaceGPIO->setClockMux ( inState->gpio.gpio_ClockMux );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kGPIO_Unknown != inState->gpio.gpio_DigitalCodecReset ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setCodecReset ( kCODEC_RESET_Digital, %X )", inState->gpio.gpio_DigitalCodecReset );
			result = platformInterfaceGPIO->setCodecReset ( kCODEC_RESET_Digital, inState->gpio.gpio_DigitalCodecReset );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kGPIO_Unknown != inState->gpio.gpio_HeadphoneMute ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setHeadphoneMuteState ( %X )", inState->gpio.gpio_HeadphoneMute );
			result = platformInterfaceGPIO->setHeadphoneMuteState ( inState->gpio.gpio_HeadphoneMute );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kGPIO_Unknown != inState->gpio.gpio_InputDataMux ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setInputDataMux ( %X )", inState->gpio.gpio_InputDataMux );
			result = platformInterfaceGPIO->setInputDataMux ( inState->gpio.gpio_InputDataMux );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kGPIO_Unknown != inState->gpio.gpio_LineOutMute ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setLineOutMuteState ( %X )", inState->gpio.gpio_LineOutMute );
			result = platformInterfaceGPIO->setLineOutMuteState ( inState->gpio.gpio_LineOutMute );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kGPIO_Unknown != inState->gpio.gpio_SpeakerMute ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setSpeakerMuteState ( %X )", inState->gpio.gpio_SpeakerMute );
			result = platformInterfaceGPIO->setSpeakerMuteState ( inState->gpio.gpio_SpeakerMute );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
	
	if ( platformInterfaceI2C ) {
	}
	
	if ( platformInterfaceI2S ) {
		if ( inState->i2s.intCtrl != platformInterfaceI2S->getI2SIOMIntControl () ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setI2SIOMIntControl ( %lX )", inState->i2s.intCtrl );
			result = platformInterfaceI2S->setI2SIOMIntControl ( inState->i2s.intCtrl );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( inState->i2s.serialFmt != platformInterfaceI2S->getSerialFormatRegister () ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setSerialFormatRegister ( %lX )", inState->i2s.serialFmt );
			result = platformInterfaceI2S->setSerialFormatRegister ( inState->i2s.serialFmt );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( ( inState->i2s.frameCount != platformInterfaceI2S->getFrameCount () ) && ( 0 == inState->i2s.frameCount ) ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setFrameCount ( %lX )", inState->i2s.frameCount );
			result = platformInterfaceI2S->setFrameCount ( inState->i2s.frameCount );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( inState->i2s.dataWordSizes != platformInterfaceI2S->getDataWordSizes () ) {
			debugIOLog (3,  "ShastaPlatform::setPlatformState setDataWordSizes ( %lX )", inState->i2s.dataWordSizes );
			result = platformInterfaceI2S->setDataWordSizes ( inState->i2s.dataWordSizes );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
Exit:
	return result;
}

