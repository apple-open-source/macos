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

Exit:
	return result;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::free ( void ) {
	
	debugIOLog (3,  "+ PlatformInterface::free" );
	
	unregisterInterrupts();

	return super::free ();
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	PlatformInterface::registerInterrupts ( IOService * device ) {
	IOReturn	err;
	bool		result = false;
	
	debugIOLog (3,  "+ PlatformInterface[%ld]::registerInterrupts ( %p )", mInstanceIndex, device );

	FailIf (NULL == device, Exit );
	FailIf ( NULL == mProvider, Exit );

	if ( kGPIO_Unknown != getHeadphoneConnected() ) {
		err = registerInterruptHandler ( device, (void*)headphoneDetectInterruptHandler, kHeadphoneDetectInterrupt );
		FailIf ( kIOReturnSuccess != err, Exit );
		debugIOLog (3,  "headphoneDetectInterruptHandler has been registered!!!" );
		headphoneDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	else
	{
		debugIOLog (3, "PlatformInterface::registerInterrupts kGPIO_Unknown == getHeadphoneConnected()");
	}
	
	if ( kGPIO_Unknown != getSpeakerConnected() ) {
		err = registerInterruptHandler ( device, (void*)speakerDetectInterruptHandler, kSpeakerDetectInterrupt );
		FailIf ( kIOReturnSuccess != err, Exit );
		debugIOLog (3,  "speakerDetectInterruptHandler has been registered!!!" );
		speakerDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	else
	{
		debugIOLog (3, "PlatformInterface::registerInterrupts kGPIO_Unknown == getSpeakerConnected()");
	}
	
	if ( kGPIO_Unknown != getLineInConnected() ) {
		err = registerInterruptHandler ( device, (void*)lineInDetectInterruptHandler, kLineInputDetectInterrupt );
		FailIf ( kIOReturnSuccess != err, Exit );
		debugIOLog (3,  "lineInDetectInterruptHandler has been registered!!!" );
		lineInDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	else
	{
		debugIOLog (3, "PlatformInterface::registerInterrupts kGPIO_Unknown == getLineInConnected()");
	}
	
	if ( kGPIO_Unknown != getLineOutConnected() ) {
		err = registerInterruptHandler ( device, (void*)lineOutDetectInterruptHandler, kLineOutputDetectInterrupt );
		FailIf ( kIOReturnSuccess != err, Exit );
		debugIOLog (3,  "lineOutDetectInterruptHandler has been registered!!!" );
		lineOutDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	else
	{
		debugIOLog (3, "PlatformInterface::registerInterrupts kGPIO_Unknown == getLineOutConnected()");
	}
	
	if ( kGPIO_Unknown != getDigitalInConnected() && kGPIO_Unknown == getComboInJackTypeConnected() ) {
		//	IMPORTANT:	Only supported if the digital line input connector is not a combo connector
		//				that is already being supported by the line input handler.
		err = registerInterruptHandler ( device, (void*)digitalInDetectInterruptHandler, kDigitalInDetectInterrupt );
		FailIf ( kIOReturnSuccess != err, Exit );
		debugIOLog (3,  "digitalInDetectInterruptHandler has been registered!!!" );
		digitalInDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	else
	{
		debugIOLog (3, "PlatformInterface::registerInterrupts kGPIO_Unknown == getDigitalInConnected()");
	}
	
	if ( kGPIO_Unknown != getDigitalOutConnected() && kGPIO_Unknown == getComboOutJackTypeConnected() ) {
		//	IMPORTANT:	Only supported if the digital line output connector is not a combo connector
		//				that is already being supported by the headphone handler.
		err = registerInterruptHandler ( device, (void*)digitalOutDetectInterruptHandler, kDigitalOutDetectInterrupt );
		FailIf ( kIOReturnSuccess != err, Exit );
		debugIOLog (3,  "digitalOutDetectInterruptHandler has been registered!!!" );
		digitalOutDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	else
	{
		debugIOLog (3, "PlatformInterface::registerInterrupts kGPIO_Unknown == getDigitalOutConnected()");
	}

	if ( kGPIO_Unknown != getCodecInterrupt() ) {
		err = registerInterruptHandler ( device, (void*)codecInterruptHandler, kCodecInterrupt );
		FailIf ( kIOReturnSuccess != err, Exit );
		debugIOLog (3,  "codecInterruptHandler has been registered!!!" );
		codecInterruptHandler ( device, NULL, 0, 0 );
	}
	else
	{
		debugIOLog (3, "PlatformInterface::registerInterrupts kGPIO_Unknown == getCodecInterrupt()");
	}
	
	if ( kGPIO_Unknown != getCodecErrorInterrupt() ) {
		err = registerInterruptHandler ( device, (void*)codecErrorInterruptHandler, kCodecErrorInterrupt );
		FailIf ( kIOReturnSuccess != err, Exit );
		debugIOLog (3,  "codecErrorInterruptHandler has been registered!!!" );
		codecErrorInterruptHandler ( device, NULL, 0, 0 );
	}
	else
	{
		debugIOLog (3, "PlatformInterface::registerInterrupts kGPIO_Unknown == getCodecErrorInterrupt()");
	}
	
	result = true;
Exit:
	debugIOLog (3,  "- PlatformInterface[%ld]::registerInterrupts ( %p )", mInstanceIndex, device );
	return result;
}


//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	PlatformInterface::unregisterInterrupts ( void ) {
	if ( kGPIO_Unknown != getHeadphoneConnected() ) {
		unregisterInterruptHandler ( NULL, NULL, kHeadphoneDetectInterrupt );
	}
	
	if ( kGPIO_Unknown != getSpeakerConnected() ) {
		unregisterInterruptHandler ( NULL, NULL, kSpeakerDetectInterrupt );
	}
	
	if ( kGPIO_Unknown != getLineInConnected() ) {
		unregisterInterruptHandler ( NULL, NULL, kLineInputDetectInterrupt );
	}
	
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
		debugIOLog (3,  "combo out associated with %d", theDetectInterruptGpio);
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
//	[3453799]	This method now tests the combo-in association and if the combo-in is associated
//				with this interrupt source then the headphone jack state will only be reported
//				if an analog combo jack insertion / removal occurs.  Otherwise the digital out
//				jack state is reported.
void PlatformInterface::headphoneDetectInterruptHandler ( OSObject *owner, IOInterruptEventSource *source, UInt32 count, void * arg4 ) {
	PlatformInterface * 	platformInterface;
	IOCommandGate *			cg;
	GpioAttributes			theAnalogJackState;
	
	debugIOLog (5,  "headphoneDetectInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		theAnalogJackState = platformInterface->getHeadphoneConnected ();
		if ( kGPIO_Selector_HeadphoneDetect != platformInterface->getComboOutAssociation () ) {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kHeadphoneStatus, (void *)theAnalogJackState, (void *)0 );
		} else {
			//	Always update the combo jack history state on jack insertion ONLY!  Then the
			//	combo jack history is used to indicate either digital or analog jack insertion.
			//	This is required for proper reporting of jack removal relative to digital or analog.
			if ( kGPIO_Connected == theAnalogJackState ) {
				platformInterface->setComboOut ( platformInterface->getComboOutJackTypeConnected () );
			}
			if ( kGPIO_TypeIsDigital == platformInterface->mComboOutJackState ) {
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)theAnalogJackState, (void *)0 );
			} else {
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kHeadphoneStatus, (void *)theAnalogJackState, (void *)0 );
			}
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
		theAnalogJackState = platformInterface->getSpeakerConnected ();
		if ( kGPIO_Selector_SpeakerDetect != platformInterface->getComboOutAssociation () ) {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kExtSpeakersStatus, (void *)theAnalogJackState, (void *)0 );
		} else {
			//	Always update the combo jack history state on jack insertion ONLY!  Then the
			//	combo jack history is used to indicate either digital or analog jack insertion.
			//	This is required for proper reporting of jack removal relative to digital or analog.
			if ( kGPIO_Connected == theAnalogJackState ) {
				platformInterface->setComboOut ( platformInterface->getComboOutJackTypeConnected () );
			}
			if ( kGPIO_TypeIsDigital == platformInterface->mComboOutJackState ) {
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)theAnalogJackState, (void *)0 );
			} else {
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kExtSpeakersStatus, (void *)theAnalogJackState, (void *)0 );
			}
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
		theAnalogJackState = platformInterface->getLineInConnected ();
		if ( kGPIO_Selector_LineInDetect != platformInterface->getComboInAssociation () ) {
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kLineInStatus, (void *)theAnalogJackState, (void *)0 );
		} else {
			//	Always update the combo jack history state on jack insertion ONLY!  Then the
			//	combo jack history is used to indicate either digital or analog jack insertion.
			//	This is required for proper reporting of jack removal relative to digital or analog.
			if ( kGPIO_Connected == theAnalogJackState ) {
				platformInterface->setComboIn ( platformInterface->getComboInJackTypeConnected () );
			}
			if ( kGPIO_TypeIsDigital == platformInterface->mComboInJackState ) {
				if ( kGPIO_Connected == theAnalogJackState ) {
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalInInsertStatus, (void *)theAnalogJackState, (void *)0 );
				} else {
					cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalInRemoveStatus, (void *)theAnalogJackState, (void *)0 );
				}
			} else {
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kLineInStatus, (void *)theAnalogJackState, (void *)0 );
			}
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
	
	debugIOLog (5,  "PlatformInterface::lineOutDetectInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

	platformInterface = OSDynamicCast (PlatformInterface, owner );
	FailIf (NULL == platformInterface, Exit);
	FailIf ( NULL == platformInterface->mProvider, Exit );
	cg = platformInterface->mProvider->getCommandGate ();
	if ( NULL != cg ) {
		theAnalogJackState = platformInterface->getLineOutConnected (true);
		debugIOLog (5,  "lineOutDetectInterruptHandler: analog jack state = %d", theAnalogJackState );
		if ( kGPIO_Selector_LineOutDetect != platformInterface->getComboOutAssociation () ) {
			debugIOLog (5,  "lineOutDetectInterruptHandler: no combo jack association" );
			cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kLineOutStatus, (void *)theAnalogJackState, (void *)0 );
		} else {
			//	Always update the combo jack history state on jack insertion ONLY!  Then the
			//	combo jack history is used to indicate either digital or analog jack insertion.
			//	This is required for proper reporting of jack removal relative to digital or analog.
			debugIOLog (5,  "lineOutDetectInterruptHandler: line out has a combo jack association" );
			if ( kGPIO_Connected == theAnalogJackState ) {
				platformInterface->setComboOut ( platformInterface->getComboOutJackTypeConnected () );
			}
			if ( kGPIO_TypeIsDigital == platformInterface->mComboOutJackState ) {
				debugIOLog (5,  "lineOutDetectInterruptHandler: combo jack type = digital" );
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)theAnalogJackState, (void *)0 );
			} else {
				debugIOLog (5,  "lineOutDetectInterruptHandler: combo jack type = analog" );
				cg->runAction ( platformInterface->mProvider->interruptEventHandlerAction, (void *)kLineOutStatus, (void *)theAnalogJackState, (void *)0 );
			}
		}
	}
Exit:
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

	debugIOLog (5,  "codecInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

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
	
	debugIOLog (5,  "codecErrorInterruptHandler ( %p, %p, %ld, %p )", owner, source, count, arg4 );

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
void PlatformInterface::LogDBDMAChannelRegisters ( void ) {
	return;
}


