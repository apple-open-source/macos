/*
 *	PlatformInterface.cpp
 *
 *	Interface class for IO controllers
 *
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 */

#include "PlatformInterface.h"
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareCommon.h"
#include "AppleOnboardAudio.h"

#define super OSObject

class AppleOnboardAudio;

UInt32 PlatformInterface::sInstanceCount = 0;

OSDefineMetaClassAndStructors(PlatformInterface, OSObject);

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool PlatformInterface::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex) {
	Boolean result;

	result = super::init ();
	FailIf ( !result, Exit );
	
	FailIf ( NULL == provider, Exit );
	mProvider = provider;

	PlatformInterface::sInstanceCount++;
	
	mInstanceIndex = PlatformInterface::sInstanceCount;
	
	mComboInAssociation = kGPIO_Selector_NotAssociated;		//	[3453799]
	mComboOutAssociation = kGPIO_Selector_NotAssociated;	//	[3453799]
	
	for ( UInt32 index=0; index < kNumberOfActionSelectors; index++ ) { mComboStateMachine[index] = kComboStateMachine_handle_jack_insert; }
	mEnableAmplifierMuteRelease = FALSE;					//	[3514762]

Exit:
	return result;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::free ( void ) {
	
	debugIOLog (3,  "+ PlatformInterface::free" );
	
	unregisterInterrupts();

	super::free ();
	
	debugIOLog (3,  "- PlatformInterface::free" );
	return;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::threadedRegisterInterrupts (PlatformInterface *self, IOService * device) {
	return self->threadedMemberRegisterInterrupts (device);
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::threadedMemberRegisterInterrupts (IOService * device) {
	IOReturn	err;
	
	FailIf (NULL == device, Exit );
	FailIf ( NULL == mProvider, Exit );
	
	if ( !mInterruptsHaveBeenRegistered ) {
		if ( kGPIO_Unknown != getComboInJackTypeConnected() ) {
			err = registerInterruptHandler ( device, (void*)comboInDetectInterruptHandler, kComboInDetectInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_ComboInJackType_bitAddress );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_ComboInJackType_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getComboInJackTypeConnected()");
		}
		
		if ( kGPIO_Unknown != getComboOutJackTypeConnected() ) {
			debugIOLog (3,  "  Attempting to register comboOutDetectInterruptHandler..." );
			err = registerInterruptHandler ( device, (void*)comboOutDetectInterruptHandler, kComboOutDetectInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_ComboOutJackType_bitAddress );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_ComboOutJackType_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getComboOutJackTypeConnected()");
		}
		
		if ( kGPIO_Unknown != getHeadphoneConnected() ) {
			err = registerInterruptHandler ( device, (void*)headphoneDetectInterruptHandler, kHeadphoneDetectInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_HeadphoneDetect_bitAddress );
				debugIOLog (3,  "  headphoneDetectInterruptHandler has been registered!!!" );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_HeadphoneDetect_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getHeadphoneConnected()");
		}
		
		if ( kGPIO_Unknown != getSpeakerConnected() ) {
			err = registerInterruptHandler ( device, (void*)speakerDetectInterruptHandler, kSpeakerDetectInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_SpeakerDetect_bitAddress );
				debugIOLog (3,  "  speakerDetectInterruptHandler has been registered!!!" );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_SpeakerDetect_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getSpeakerConnected()");
		}

		if ( kGPIO_Unknown != getLineInConnected() ) {
			err = registerInterruptHandler ( device, (void*)lineInDetectInterruptHandler, kLineInputDetectInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_LineInDetect_bitAddress );
				debugIOLog (3,  "  lineInDetectInterruptHandler has been registered!!!" );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_LineInDetect_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getLineInConnected()");
		}

		if ( kGPIO_Unknown != getLineOutConnected() ) {
			err = registerInterruptHandler ( device, (void*)lineOutDetectInterruptHandler, kLineOutputDetectInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_LineOutDetect_bitAddress );
				debugIOLog (3,  "  lineOutDetectInterruptHandler has been registered!!!" );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_LineOutDetect_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getLineOutConnected()");
		}
		
		if ( kGPIO_Unknown != getDigitalInConnected() && kGPIO_Unknown == getComboInJackTypeConnected() ) {
			//	IMPORTANT:	Only supported if the digital line input connector is not a combo connector
			//				that is already being supported by the line input handler.
			err = registerInterruptHandler ( device, (void*)digitalInDetectInterruptHandler, kDigitalInDetectInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_DigitalInDetect_bitAddress );
				debugIOLog (3,  "  digitalInDetectInterruptHandler has been registered!!!" );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_DigitalInDetect_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getDigitalInConnected()");
		}
		
		if ( kGPIO_Unknown != getDigitalOutConnected() && kGPIO_Unknown == getComboOutJackTypeConnected() ) {
			//	IMPORTANT:	Only supported if the digital line output connector is not a combo connector
			//				that is already being supported by the headphone handler.
			err = registerInterruptHandler ( device, (void*)digitalOutDetectInterruptHandler, kDigitalOutDetectInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_DigitalOutDetect_bitAddress );
				debugIOLog (3,  "  digitalOutDetectInterruptHandler has been registered!!!" );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_DigitalOutDetect_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getDigitalOutConnected()");
		}

		if ( kGPIO_Unknown != getCodecInterrupt() ) {
			err = registerInterruptHandler ( device, (void*)codecInterruptHandler, kCodecInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_CodecInterrupt_bitAddress );
				codecInterruptHandler ( device, NULL, 0, 0 );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_CodecInterrupt_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getCodecInterrupt()");
		}

		if ( kGPIO_Unknown != getCodecErrorInterrupt() ) {
			err = registerInterruptHandler ( device, (void*)codecErrorInterruptHandler, kCodecErrorInterrupt );
			if ( kIOReturnSuccess == err ) {
				mGpioMessageFlag |= ( 1 << gpioMessage_CodecErrorInterrupt_bitAddress );
				debugIOLog (3,  "  codecErrorInterruptHandler has been registered!!!" );
				codecErrorInterruptHandler ( device, NULL, 0, 0 );
			} else {
				mGpioMessageFlag &= (~( 1 << gpioMessage_CodecErrorInterrupt_bitAddress ));
				FailMessage ( TRUE );
			}
		}
		else
		{
			debugIOLog (3, "  PlatformInterface::registerInterrupts kGPIO_Unknown == getCodecErrorInterrupt()");
		}

		mInterruptsHaveBeenRegistered = TRUE;
		checkDetectStatus ( device );
	}

Exit:
	return;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::checkDetectStatus ( IOService * device ) {
	debugIOLog (3, "± PlatformInterface::checkDetectStatus");

	if ( mGpioMessageFlag & ( 1 << gpioMessage_HeadphoneDetect_bitAddress ) ) {
		headphoneDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_SpeakerDetect_bitAddress ) ) {
		speakerDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_LineInDetect_bitAddress ) ) {
		lineInDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_LineOutDetect_bitAddress ) ) {
		lineOutDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_DigitalInDetect_bitAddress ) ) {
		digitalInDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	if ( mGpioMessageFlag & ( 1 << gpioMessage_DigitalOutDetect_bitAddress ) ) {
		digitalOutDetectInterruptHandler ( device, NULL, 0, 0 );
	}
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	PlatformInterface::registerInterrupts ( IOService * device ) {
	bool		result = false;
	
	debugIOLog (3,  "+ PlatformInterface[%ld]::registerInterrupts ( %p )", mInstanceIndex, device );

	FailIf (NULL == device, Exit);

	if (NULL == mRegisterInterruptsThread) {
		mRegisterInterruptsThread = thread_call_allocate ((thread_call_func_t)PlatformInterface::threadedRegisterInterrupts, (thread_call_param_t)this);
	}

	if (NULL != mRegisterInterruptsThread) {
		thread_call_enter1 (mRegisterInterruptsThread, (thread_call_param_t)device);
		result = true;
	}

Exit:
	debugIOLog (3,  "- PlatformInterface[%ld]::registerInterrupts ( %p )", mInstanceIndex, device );
	return result;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::unregisterInterrupts ( void ) {

	if ( mInterruptsHaveBeenRegistered ) {
		if ( kGPIO_Unknown != getComboInJackTypeConnected() ) {
			unregisterInterruptHandler ( NULL, NULL, kComboInDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getComboOutJackTypeConnected() ) {
			unregisterInterruptHandler ( NULL, NULL, kComboOutDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getHeadphoneConnected() ) {
			unregisterInterruptHandler ( NULL, NULL, kHeadphoneDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getSpeakerConnected() ) {
			unregisterInterruptHandler ( NULL, NULL, kSpeakerDetectInterrupt );
		}
	#if 0	
		if ( kGPIO_Unknown != getLineInConnected() ) {
			unregisterInterruptHandler ( NULL, NULL, kLineInputDetectInterrupt );
		}
	#endif	
		if ( kGPIO_Unknown != getLineOutConnected() ) {
			unregisterInterruptHandler ( NULL, NULL, kLineOutputDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getDigitalInConnected() && kGPIO_Unknown == getComboInJackTypeConnected() ) {
			unregisterInterruptHandler ( NULL, NULL, kDigitalInDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getDigitalOutConnected() && kGPIO_Unknown == getComboOutJackTypeConnected() ) {
			unregisterInterruptHandler ( NULL, NULL, kDigitalOutDetectInterrupt );
		}
		
		if ( kGPIO_Unknown != getCodecInterrupt() ) {
			unregisterInterruptHandler ( NULL, NULL, kCodecInterrupt );
		}
		
		if ( kGPIO_Unknown != getCodecErrorInterrupt() ) {
			unregisterInterruptHandler ( NULL, NULL, kCodecErrorInterrupt );
		}

		mInterruptsHaveBeenRegistered = FALSE;
	}
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
GPIOSelector PlatformInterface::getComboInAssociation ( void ) {
	return mComboInAssociation;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
void PlatformInterface::setAssociateComboInTo ( GPIOSelector theDetectInterruptGpio ) {
	if (( kGPIO_Selector_LineInDetect == theDetectInterruptGpio ) || ( kGPIO_Selector_ExternalMicDetect == theDetectInterruptGpio )) {
		mComboInAssociation = theDetectInterruptGpio;
	}
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
GPIOSelector PlatformInterface::getComboOutAssociation ( void ) {
	return mComboOutAssociation;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
void PlatformInterface::setAssociateComboOutTo ( GPIOSelector theDetectInterruptGpio ) {
	if (( kGPIO_Selector_LineOutDetect == theDetectInterruptGpio ) || ( kGPIO_Selector_HeadphoneDetect == theDetectInterruptGpio ) || ( kGPIO_Selector_SpeakerDetect == theDetectInterruptGpio )) {
		mComboOutAssociation = theDetectInterruptGpio;
	}
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
GpioAttributes PlatformInterface::getComboIn ( void ) {
	return mComboInJackState;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
void PlatformInterface::setComboIn ( GpioAttributes jackState ) {
	if (( kGPIO_TypeIsDigital == jackState ) || ( kGPIO_TypeIsAnalog == jackState )) {
		mComboInJackState = jackState;
	}
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
GpioAttributes PlatformInterface::getComboOut ( void ) {
	return mComboOutJackState;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]
void PlatformInterface::setComboOut ( GpioAttributes jackState ) {
	if (( kGPIO_TypeIsDigital == jackState ) || ( kGPIO_TypeIsAnalog == jackState )) {
		debugIOLog (5,  "combo out type set to %d", jackState );
		mComboOutJackState = jackState;
	}
}



//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3517297]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::comboInDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	
	debugIOLog (5,  "comboInDetectInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		IOSleep ( 10 );
		if ( kGPIO_Selector_LineInDetect == platformInterface->getComboInAssociation () ) {
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface,
													  platformInterface->getLineInConnected (), 
													  platformInterface->getComboInJackTypeConnected (), 
													  kGPIO_Selector_LineInDetect 
													);
		} else {
			FailMessage ( TRUE );
		}
	}
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3517297]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::comboOutDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	
	debugIOLog (5,  "comboOutDetectInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		IOSleep ( 10 );
		if ( kGPIO_Selector_HeadphoneDetect == platformInterface->getComboOutAssociation () ) {
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  platformInterface->getHeadphoneConnected (), 
													  platformInterface->getComboOutJackTypeConnected (), 
													  kGPIO_Selector_HeadphoneDetect 
													);
		} else if ( kGPIO_Selector_LineOutDetect == platformInterface->getComboOutAssociation () ) {
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  platformInterface->getLineOutConnected (), 
													  platformInterface->getComboOutJackTypeConnected (), 
													  kGPIO_Selector_LineOutDetect 
													);
		} else {
			FailMessage ( TRUE );
		}
	}
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::headphoneDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;

	debugIOLog (5,  "± PlatformInterface::headphoneDetectInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		IOSleep ( 10 );
		theAnalogJackState = platformInterface->getHeadphoneConnected ();
		if ( kGPIO_Selector_HeadphoneDetect != platformInterface->getComboOutAssociation () ) {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kHeadphoneStatus, (void *)theAnalogJackState, (void *)0 );
		} else {
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  theAnalogJackState, 
													  platformInterface->getComboOutJackTypeConnected (), 
													  kGPIO_Selector_HeadphoneDetect 
													);
		}
	}
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::speakerDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;
	
	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		IOSleep ( 10 );
		theAnalogJackState = platformInterface->getSpeakerConnected ();
		if ( kGPIO_Selector_SpeakerDetect != platformInterface->getComboOutAssociation () ) {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kExtSpeakersStatus, (void *)theAnalogJackState, (void *)0 );
		} else {
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  theAnalogJackState, 
													  platformInterface->getComboOutJackTypeConnected (), 
													  kGPIO_Selector_SpeakerDetect 
													);
		}
	}
	
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::lineInDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;
	
	debugIOLog (5,  "lineInDetectInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		IOSleep ( 10 );
		theAnalogJackState = platformInterface->getLineInConnected ();
		if ( kGPIO_Selector_LineInDetect != platformInterface->getComboInAssociation () ) {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kLineInStatus, (void *)theAnalogJackState, (void *)0 );
		} else {
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  theAnalogJackState, 
													  platformInterface->getComboInJackTypeConnected (), 
													  kGPIO_Selector_LineInDetect 
													);
		}
	}
Exit:
	return;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::lineOutDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;
	
	debugIOLog (5,  "+ PlatformInterface::lineOutDetectInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		IOSleep ( 10 );
		theAnalogJackState = platformInterface->getLineOutConnected ();
		if ( kGPIO_Selector_LineOutDetect != platformInterface->getComboOutAssociation () ) {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kLineOutStatus, (void *)theAnalogJackState, (void *)0 );
		} else {
			platformInterface->RunComboStateMachine ( cg, 
													  platformInterface, 
													  theAnalogJackState, 
													  platformInterface->getComboOutJackTypeConnected (), 
													  kGPIO_Selector_LineOutDetect 
													);
		}
	}
Exit:

	debugIOLog (5,  "- PlatformInterface::lineOutDetectInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::digitalInDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *		cg;
	
	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		if ( kGPIO_Connected == platformInterface->getDigitalInConnected () ) {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalInInsertStatus, (void *)platformInterface->getDigitalInConnected (), (void *)0 );
		} else {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalInRemoveStatus, (void *)platformInterface->getDigitalInConnected (), (void *)0 );
		}
	}
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::digitalOutDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *		cg;
	
	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)platformInterface->getDigitalOutConnected (), (void *)0 );
	}
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::codecInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *		cg;

	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kCodecInterruptStatus, (void *)platformInterface->getCodecInterrupt (), (void *)0 );
	}
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PlatformInterface::codecErrorInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *		cg;
	
	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kCodecErrorInterruptStatus, (void *)platformInterface->getCodecErrorInterrupt (), (void *)0 );
	}
Exit:
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Radar 3517297
//	Arguments:		detectState		Indicates the state of the analog detect (for example, the Line Output
//									detect).  Values may be either kGPIO_Connected or kGPIO_Disconnected.
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
void PlatformInterface::RunComboStateMachine ( IOCommandGate * cg, PlatformInterface * platformInterface, UInt32 detectState, UInt32 typeSenseState, UInt32 analogJackType ) {
	
	debugIOLog (5,  "+ RunComboStateMachine ( %p, %p, %ld, %ld, %ld ), power state = %d", cg, platformInterface, detectState, typeSenseState, analogJackType, platformInterface->mProvider->getPowerState () );

	FailIf ( NULL == cg, Exit );
	FailIf ( NULL == platformInterface, Exit );
	FailIf ( NULL == platformInterface->mProvider, Exit );

	if (platformInterface->mProvider->getPowerState () == kIOAudioDeviceSleep) goto Exit;		// don't advance the state machine if we are asleep (we'll run this code on wake)

	switch ( mComboStateMachine[analogJackType] ) {
		case kComboStateMachine_handle_jack_insert:
			//	When no jack is inserted then the only events that can occur is insertion of metal or plastic jacks.
			if ( kGPIO_Connected == detectState ) {
				if ( kGPIO_TypeIsAnalog == typeSenseState ) {
					switch ( analogJackType ) {
						case kGPIO_Selector_LineInDetect:
							debugIOLog (5,  "  RunComboStateMachine 'Handle Jack Insert' posting ANALOG INSERT of 'Line Input' jack" );
							cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kLineInStatus, (void *)kGPIO_Connected, (void *)0 );
							break;
						case kGPIO_Selector_LineOutDetect:
							debugIOLog (5,  "  RunComboStateMachine 'Handle Jack Insert' posting ANALOG INSERT of 'Line Output' jack" );
							cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kLineOutStatus, (void *)kGPIO_Connected, (void *)0 );
							break;
						case kGPIO_Selector_HeadphoneDetect:
							debugIOLog (5,  "  RunComboStateMachine 'Handle Jack Insert' posting ANALOG INSERT of 'Line Headphone' jack" );
							cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kHeadphoneStatus, (void *)kGPIO_Connected, (void *)0 );
							break;
					}
					mComboStateMachine[analogJackType] = kComboStateMachine_handle_metal_change;
				} else {
					debugIOLog (5,  "kGPIO_TypeIsAnalog != typeSenseState");
					if ( testIsInputJack( analogJackType ) ) {
						debugIOLog (5,  "  RunComboStateMachine 'Handle Jack Insert' posting DIGITAL INPUT INSERT" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalInInsertStatus, (void *)kGPIO_Connected, (void *)0 );
					} else {
						debugIOLog (5,  "  RunComboStateMachine 'Handle Jack Insert' posting DIGITAL OUTPUT INSERT" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)kGPIO_Connected, (void *)0 );
					}
					mComboStateMachine[analogJackType] = kComboStateMachine_handle_plastic_change;
				}
			}
			break;
		case kComboStateMachine_handle_metal_change:
			//	[3564007]
			switch ( analogJackType ) {
				case kGPIO_Selector_LineInDetect:
					debugIOLog (5,  "  RunComboStateMachine 'Handle Metal Change' posting ANALOG EXTRACT of 'Line Input' jack" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kLineInStatus, (void *)kGPIO_Disconnected, (void *)0 );
					break;
				case kGPIO_Selector_LineOutDetect:
					debugIOLog (5,  "  RunComboStateMachine 'Handle Metal Change' posting ANALOG EXTRACT of 'Line Output' jack" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kLineOutStatus, (void *)kGPIO_Disconnected, (void *)0 );
					break;
				case kGPIO_Selector_HeadphoneDetect:
					debugIOLog (5,  "  RunComboStateMachine 'Handle Metal Change' posting ANALOG EXTRACT of 'Line Headphone' jack" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kHeadphoneStatus, (void *)kGPIO_Disconnected, (void *)0 );
					break;
			}
			if ( kGPIO_Disconnected == detectState ) {
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_jack_insert;
			} else if ( kGPIO_TypeIsAnalog == typeSenseState ) {
				switch ( analogJackType ) {
					case kGPIO_Selector_LineInDetect:
						debugIOLog (5,  "  RunComboStateMachine 'Handle Metal Change' posting ANALOG INSERT of 'Line Input' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kLineInStatus, (void *)kGPIO_Connected, (void *)0 );
						break;
					case kGPIO_Selector_LineOutDetect:
						debugIOLog (5,  "  RunComboStateMachine 'Handle Metal Change' posting ANALOG INSERT of 'Line Output' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kLineOutStatus, (void *)kGPIO_Connected, (void *)0 );
						break;
					case kGPIO_Selector_HeadphoneDetect:
						debugIOLog (5,  "  RunComboStateMachine 'Handle Metal Change' posting ANALOG INSERT of 'Line Headphone' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kHeadphoneStatus, (void *)kGPIO_Connected, (void *)0 );
						break;
				}
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_metal_change;
			} else {
				if ( testIsInputJack( analogJackType ) ) {
					debugIOLog (5,  "  RunComboStateMachine 'Handle Metal Change' posting DIGITAL INPUT INSERT" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalInInsertStatus, (void *)kGPIO_Connected, (void *)0 );
				} else {
					debugIOLog (5,  "  RunComboStateMachine 'Handle Metal Change' posting DIGITAL OUTPUT INSERT" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)kGPIO_Connected, (void *)0 );
				}
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_plastic_change;
			}
			break;
		case kComboStateMachine_handle_plastic_change:
			//	When plastic is inserted then the only events that can occur is that the plastic can be extracted or
			//	the jack type can change to metal.  In either case, digital jack extraction must be posted.
			if ( testIsInputJack( analogJackType ) ) {	//	Check is to determine INPUT v.s. OUTPUT (there is only one kind of input connector)
				debugIOLog (5,  "  RunComboStateMachine 'Handle Plastic Change' posting DIGITAL INPUT EXTRACT" );
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalInRemoveStatus, (void *)kGPIO_Disconnected, (void *)0 );
			} else {
				debugIOLog (5,  "  RunComboStateMachine 'Handle Plastic Change' posting DIGITAL OUTPUT EXTRACT" );
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)kGPIO_Disconnected, (void *)0 );
			}
			if ( kGPIO_Disconnected == detectState ) {
				//	Handle digital jack extraction
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_jack_insert;
			} else if ( kGPIO_TypeIsAnalog == typeSenseState ) {
				//	Handle direct transition from plastic digital jack to metal analog jack
				debugIOLog (5,  "  RunComboStateMachine 'Handle Plastic Change' posting change to DIGITAL INPUT INSERT" );
				switch ( analogJackType ) {
					case kGPIO_Selector_LineInDetect:
						debugIOLog (5,  "  RunComboStateMachine 'Handle Plastic Change' posting ANALOG INSERT of 'Line Input' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kLineInStatus, (void *)kGPIO_Connected, (void *)0 );
						break;
					case kGPIO_Selector_LineOutDetect:
						debugIOLog (5,  "  RunComboStateMachine 'Handle Plastic Change' posting ANALOG INSERT of 'Line Output' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kLineOutStatus, (void *)kGPIO_Connected, (void *)0 );
						break;
					case kGPIO_Selector_HeadphoneDetect:
						debugIOLog (5,  "  RunComboStateMachine 'Handle Plastic Change' posting ANALOG INSERT of 'Line Headphone' jack" );
						cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void*)kHeadphoneStatus, (void *)kGPIO_Connected, (void *)0 );
						break;
				}
				mComboStateMachine[analogJackType] = kComboStateMachine_handle_metal_change;
			} else {
				if ( testIsInputJack( analogJackType ) ) {
					debugIOLog (5,  "  RunComboStateMachine 'Handle Plastic Change' posting DIGITAL INPUT INSERT" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalInInsertStatus, (void *)kGPIO_Connected, (void *)0 );
				} else {
					debugIOLog (5,  "  RunComboStateMachine 'Handle Plastic Change' posting DIGITAL OUTPUT INSERT" );
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)kGPIO_Connected, (void *)0 );
				}
			}
			break;
	}
	
Exit:
	debugIOLog (5,  "- RunComboStateMachine ( %p, %p, %ld, %ld, %ld )", cg, platformInterface, detectState, typeSenseState, analogJackType );
	return;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'TRUE' if the analog jack type is an input jack.	[3564007]
bool PlatformInterface::testIsInputJack ( UInt32 analogJackType ) {
	return ( kGPIO_Selector_LineInDetect == analogJackType );
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3514762]
void PlatformInterface::enableAmplifierMuteRelease ( void ) {
	debugIOLog (3, "enabling amplifier mutes");
	mEnableAmplifierMuteRelease = TRUE;
}



