/*
 *  PowerMac12_1VirtualControl.cpp
 *  AppleMacRISC4PE
 *
 *  Created by Marco Pontil on 6/28/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#include "PowerMac12_1VirtualControl.h"

/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


//#define CONTROL_DEBUG

#ifdef CONTROL_DEBUG
#ifdef CONTROL_DLOG
#undef CONTROL_DLOG
#endif
#define CONTROL_DLOG(fmt, args...)  IOLog(fmt, ## args)
#endif

#include <IOKit/IOLib.h>
#include "IOPlatformPluginDefs.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformCtrlLoop.h"
#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "PowerMac12_1VirtualControl.h"
#include "PowerMac12_1_CPUFanCtrlLoop.h"

#define super IOPlatformControl
OSDefineMetaClassAndStructors(PowerMac12_1VirtualControl, IOPlatformControl)

bool PowerMac12_1VirtualControl::init( void )
{
	// Creates a lock to prevent overalpping accesses
	mVirtualControlLock = IORecursiveLockAlloc();
    if (mVirtualControlLock == NULL)
		return false;

	return( super::init() );
}

void PowerMac12_1VirtualControl::free( void )
{
	int i;

	for ( i = 0 ; i < mActualNumberOfRegistredControlLoops ; i++)
	{
		if ( mRegistredControlLoopsArray[i].ctrlLoop )
		{
			mRegistredControlLoopsArray[i].ctrlLoop->release();
			mRegistredControlLoopsArray[i].ctrlLoop = NULL;
			mRegistredControlLoopsArray[i].hasActiveValue = false;
			mRegistredControlLoopsArray[i].currentActiveValue = 0;
		}
	}
	mActualNumberOfRegistredControlLoops = 0;

	if ( mHWControl )
	{
		mHWControl->release();
		mHWControl = NULL;
	}

	if ( mVirtualControlLock )
	{
		IORecursiveLockFree ( mVirtualControlLock );
		mVirtualControlLock = NULL;
	}

	super::free();
}

IOReturn PowerMac12_1VirtualControl::initPlatformControl( const OSDictionary *dict )
{
	const OSNumber *number;

	if ( !dict || !init() ) return(kIOReturnError);

	// Extracts the target control ID:
	if ((number = OSDynamicCast(OSNumber, dict->getObject(kPowerMac12_1TargetControlID))) != NULL)
	{
		mHWControl = platformPlugin->lookupControlByID( number );
		if ( mHWControl )
			mHWControl->retain();
		else
		{
			IOLog("=-=PowerMac12_1VirtualControl::initPlatformControl Invalid Target Control\n");
			return(kIOReturnBadArgument);
		}
	}
	else
	{
		IOLog("=-=PowerMac12_1VirtualControl::initPlatformControl Invalid Thermal Profile omits Control ID\n");
		return(kIOReturnBadArgument);
	}

	IOReturn returnValue = super::initPlatformControl(dict);

	infoDict->setObject( "output-low-bound", dict->getObject( "output-low-bound" ));
	infoDict->setObject( "rubber-band-connection", dict->getObject( "rubber-band-connection" ));

	return returnValue;
}

IOReturn PowerMac12_1VirtualControl::initPlatformControl( IOService * unknownControl, const OSDictionary * dict )
{
	const OSNumber *number;

	// Extracts the target control ID:
	if ((number = OSDynamicCast(OSNumber, dict->getObject(kPowerMac12_1TargetControlID))) != NULL)
	{
		mHWControl = platformPlugin->lookupControlByID( number );
		if ( mHWControl )
			mHWControl->retain();
		else
		{
			IOLog("=-=PowerMac12_1VirtualControl::initPlatformControl Invalid Target Control\n");
			return(kIOReturnBadArgument);
		}
	}
	else
	{
		IOLog("=-=PowerMac12_1VirtualControl::initPlatformControl Invalid Thermal Profile omits Control ID\n");
		return(kIOReturnBadArgument);
	}

	IOReturn returnValue = super::initPlatformControl( unknownControl, dict );

	infoDict->setObject( "output-low-bound", dict->getObject( "output-low-bound" ));
	infoDict->setObject( "rubber-band-connection", dict->getObject( "rubber-band-connection" ));

	return returnValue;
}

IOReturn PowerMac12_1VirtualControl::initRubberControlConnection( )
{
	OSDictionary*						rubberBandDescription;
	
	rubberBandDescription = OSDynamicCast( OSDictionary, infoDict->getObject( "rubber-band-connection" ) );
	
	// If there is a low bound descriptor good, otherwise too bad we'll do without it
	if ( rubberBandDescription )
	{
		OSNumber *tmpNumber;
	
		tmpNumber = OSDynamicCast( OSNumber, rubberBandDescription->getObject( "control-id" ) );
		if ( tmpNumber == NULL )
		{
			IOLog("=-=PowerMac12_1VirtualControl::initPlatformCtrlLoop invalid control-id\n");
			return kIOReturnError;
		}
		
		mRubberBandControl = platformPlugin->lookupControlByID( tmpNumber );
		
		if ( mRubberBandControl != NULL )
		{
			tmpNumber = OSDynamicCast( OSNumber, rubberBandDescription->getObject( "offset" ) );
			if ( tmpNumber == NULL )
			{
				IOLog("=-=PowerMac12_1VirtualControl::initPlatformCtrlLoop invalid offset\n");
				return kIOReturnError;
			}
			mControlOffset = ((SInt32) tmpNumber->unsigned32BitValue());
			
			tmpNumber = OSDynamicCast( OSNumber, rubberBandDescription->getObject( "slope" ) );
			if ( tmpNumber == NULL )
			{
				IOLog("=-=PowerMac12_1VirtualControl::initPlatformCtrlLoop invalid slope\n");
				return kIOReturnError;
			}
			mControlSlope = ((SInt32) tmpNumber->unsigned32BitValue());
		}
		else
			IOLog("=-=%s no rubber control control not found\n", super::getControlDescKey()->getCStringNoCopy());
	}
	else
		CONTROL_DLOG("=-=%s no rubber band definition\n", super::getControlDescKey()->getCStringNoCopy());
		
	return kIOReturnSuccess;
}

IOReturn PowerMac12_1VirtualControl::initControlLoopConnection( IOPlatformCtrlLoop * aCtrlLoop )
{
	OSDictionary*						lowBoundDescriptor;
	
	lowBoundDescriptor = OSDynamicCast( OSDictionary, infoDict->getObject( "output-low-bound" ) );
	
	// If there is a low bound descriptor good, otherwise too bad we'll do without it
	if ( lowBoundDescriptor )
	{
		OSNumber *tmpNumber;
	
		tmpNumber = OSDynamicCast( OSNumber, lowBoundDescriptor->getObject( "control-loop-reference" ) );
		if ( tmpNumber == NULL )
		{
			IOLog("=-=PowerMac12_1VirtualControl::initPlatformCtrlLoop invalid control-loop-reference\n");
			return kIOReturnError;
		}

		mReferenceControlLoop = OSDynamicCast( PowerMac12_1_CPUFanCtrlLoop, platformPlugin->lookupCtrlLoopByID( tmpNumber ) );
		if ( ( mReferenceControlLoop == NULL) && ( aCtrlLoop != NULL ) )
		{
			
			CONTROL_DLOG("=-=aCtrlLoop->getCtrlLoopID() tmpNumber = %ld %ld\n", aCtrlLoop->getCtrlLoopID()->unsigned16BitValue() , tmpNumber->unsigned16BitValue());
			if ( tmpNumber->isEqualTo( aCtrlLoop->getCtrlLoopID() ) )
				mReferenceControlLoop = OSDynamicCast( PowerMac12_1_CPUFanCtrlLoop, aCtrlLoop);
		}

		if ( mReferenceControlLoop != NULL )
		{
			tmpNumber = OSDynamicCast( OSNumber, lowBoundDescriptor->getObject( "offset" ) );
			if ( tmpNumber == NULL )
			{
				IOLog("=-=PowerMac12_1VirtualControl::initPlatformCtrlLoop invalid offset\n");
				return kIOReturnError;
			}

			mControlLoopOffset = (SInt32)tmpNumber->unsigned32BitValue();
			
			tmpNumber = OSDynamicCast( OSNumber, lowBoundDescriptor->getObject( "slope" ) );
			if ( tmpNumber == NULL )
			{
				IOLog("=-=PowerMac12_1VirtualControl::initPlatformCtrlLoop invalid slope\n");
				return kIOReturnError;
			}
			mControlLoopSlope = ((SInt32) tmpNumber->unsigned32BitValue());
		}
		else
			IOLog("=-=%s PowerMac12_1VirtualControl::initControlLoopConnection(%p) no control loop\n", super::getControlDescKey()->getCStringNoCopy(), aCtrlLoop);
	}
	else
		CONTROL_DLOG("=-=%s no output-low-bound definition\n", super::getControlDescKey()->getCStringNoCopy());
		
	return kIOReturnSuccess;
}

bool PowerMac12_1VirtualControl::joinedCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop )
{
	bool canJoinCtrlLoop = ( super::joinedCtrlLoop( aCtrlLoop ) && mHWControl->joinedCtrlLoop( aCtrlLoop ) );

	if ( canJoinCtrlLoop )
	{
		// The next few lines are to call
		// initControlLoopConnection( aCtrlLoop ); and 
		// initRubberControlConnection( ); 
		// These two calls need to find controls or control loops (for
		// the rubber band or for the lower bound. These calls may not
		// work in "initPlatformControl()" because it may be too early
		// to find control loops or virutal controls. So it is here,
		// at this point everything I need to find should be
		// findable.
		
		if ( mReferenceControlLoop == NULL )
		{
			initControlLoopConnection( aCtrlLoop );
		}

		if ( mRubberBandControl == NULL )
		{
			initRubberControlConnection( );
		}
		
		if ( mActualNumberOfRegistredControlLoops == kMaxSetOfControlLoops )
			return false;
			
		// Locks ONLY self contained code !!!!
		if (mVirtualControlLock != NULL)
		  IORecursiveLockLock(mVirtualControlLock);

		int i;
		bool isAlredyHere = false;
		
		for ( i = 0; i <  mActualNumberOfRegistredControlLoops ; i ++ )
		{
			isAlredyHere |= ( mRegistredControlLoopsArray[i].ctrlLoop == aCtrlLoop );
		}
		
		if ( ! isAlredyHere )
		{
			// Add the new control loop:
			mRegistredControlLoopsArray[ mActualNumberOfRegistredControlLoops ].ctrlLoop = aCtrlLoop;
			mRegistredControlLoopsArray[ mActualNumberOfRegistredControlLoops ].hasActiveValue = false;
			mRegistredControlLoopsArray[ mActualNumberOfRegistredControlLoops ].currentActiveValue = 0;
					
			mRegistredControlLoopsArray[ mActualNumberOfRegistredControlLoops ].ctrlLoop->retain();

			mActualNumberOfRegistredControlLoops++;

			CONTROL_DLOG("PowerMac12_1VirtualControl::registerControlLoop  control set %d : %d) !!\n", mActualNumberOfRegistredControlLoops , ( aCtrlLoop->getCtrlLoopID() ? aCtrlLoop->getCtrlLoopID()->unsigned16BitValue() : 0xFF ) );
		}
		
		// Locks ONLY self contained code !!!!
		if (mVirtualControlLock != NULL)
		  IORecursiveLockUnlock(mVirtualControlLock);
	}
	
	return canJoinCtrlLoop;
}

bool PowerMac12_1VirtualControl::leftCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop )
{
	bool canLeaveCtrlLoop = super::leftCtrlLoop( aCtrlLoop ) && mHWControl->leftCtrlLoop( aCtrlLoop );
	
	return canLeaveCtrlLoop;
}

bool PowerMac12_1VirtualControl::isManaged( void )
{
	return mHWControl->isManaged();
}

OSBoolean *PowerMac12_1VirtualControl::isRegistered( void )
{
	return mHWControl->isRegistered();
}

IOReturn PowerMac12_1VirtualControl::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
	if (notify) notifyCtrlLoops();

	return(kIOReturnSuccess);
}

OSNumber *PowerMac12_1VirtualControl::getVersion( void )
{
	return mHWControl->getVersion( );
}

OSString *PowerMac12_1VirtualControl::getControlType( void )
{
	return mHWControl->getControlType( );
}

UInt32 PowerMac12_1VirtualControl::getControlTypeID( void )
{
	return mHWControl->getControlTypeID( );
}

OSData *PowerMac12_1VirtualControl::getControlZone( void )
{
	return mHWControl->getControlZone( );
}

OSString *PowerMac12_1VirtualControl::getControlLocation( void )
{
	return mHWControl->getControlLocation( );
}

OSString *PowerMac12_1VirtualControl::getControlDescKey( void )
{
	return mHWControl->getControlDescKey( );
}

OSNumber *PowerMac12_1VirtualControl::getControlFlags( void )
{
	return mHWControl->getControlFlags( );
}

ControlValue PowerMac12_1VirtualControl::getValue( const OSSymbol * key, ControlValue defaultValue )
{
	ControlValue newValue = mHWControl->getValue( key, defaultValue);
	
	super::setValue(key , newValue);

	CONTROL_DLOG("%s -> %s getValue( %s, %ld) = %ld\n", super::getControlDescKey()->getCStringNoCopy(), mHWControl->getControlDescKey()->getCStringNoCopy(), key->getCStringNoCopy(), defaultValue, newValue);

	return newValue;
}

void PowerMac12_1VirtualControl::setValue( const OSSymbol * key, ControlValue newValue )
{
	mHWControl->setValue( key, newValue );
	
	CONTROL_DLOG("%s -> %s setValue( %s, %ld) \n", super::getControlDescKey()->getCStringNoCopy(), mHWControl->getControlDescKey()->getCStringNoCopy(), key->getCStringNoCopy(), newValue);
	
	super::setValue( key, newValue );
}

ControlValue PowerMac12_1VirtualControl::forceAndFetchCurrentValue( void )
{
	return( mHWControl->forceAndFetchCurrentValue( ) );
}

ControlValue PowerMac12_1VirtualControl::fetchCurrentValue( void )
{
	return( mHWControl->fetchCurrentValue( ) );
}

ControlValue PowerMac12_1VirtualControl::getTargetValue(void)
{
	ControlValue returnValue = super::getTargetValue();
	
	CONTROL_DLOG("%s getTargetValue( ) = %ld\n", super::getControlDescKey()->getCStringNoCopy(), returnValue);
	
	return returnValue;
}

void PowerMac12_1VirtualControl::setTargetValue( ControlValue target )
{
	CONTROL_DLOG("=-=PowerMac12_1VirtualControl::setTargetValue( ControlValue target ) is not ALLOWED use setTargetValue( ControlValue target, IOPlatformCtrlLoop *settingCtrlLoop, UInt32 referenceParameter) instead \n");
	mChosenTarget = target;
	privateSetControlTargetValue();
}

bool PowerMac12_1VirtualControl::sendTargetValue( ControlValue target, bool forced /* = false */)
{
	return super::sendTargetValue( target, forced ) && mHWControl->sendTargetValue( target, forced );
}

IOReturn PowerMac12_1VirtualControl::sendForceUpdate( void )
{
	return super::sendForceUpdate( ) && mHWControl->sendForceUpdate( );
}

void PowerMac12_1VirtualControl::setTargetValue( ControlValue target, IOPlatformCtrlLoop *settingCtrlLoop, UInt32 referenceParameter)
{
	if ( settingCtrlLoop == NULL )
	{
		CONTROL_DLOG("setTargetValue( %ld, %p, %ld) !!!!!!!!!!!!!!!\n", target, settingCtrlLoop, referenceParameter);
		return;
	}
	
	// If there is only one control loop do not waste time:
	if ( mActualNumberOfRegistredControlLoops == 1 )
	{
		CONTROL_DLOG("=-=*** SET TARGET VALUE     *** %s %ld, %d, %ld ONLY VALUE\n", super::getControlDescKey()->getCStringNoCopy(), target, ( settingCtrlLoop->getCtrlLoopID() ? settingCtrlLoop->getCtrlLoopID()->unsigned16BitValue() : 0xFF ), referenceParameter);

		mChosenTarget = target;
		mChosenTargetOwner = settingCtrlLoop;

		privateSetControlTargetValue();
		return;
	}
	
	// Log this settings as there are more than one to keep track of
	CONTROL_DLOG("=-=*** SET TARGET VALUE     *** %s %ld, %d, %ld\n", super::getControlDescKey()->getCStringNoCopy(), target, ( settingCtrlLoop->getCtrlLoopID() ? settingCtrlLoop->getCtrlLoopID()->unsigned16BitValue() : 0xFF ), referenceParameter);
	
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);
	
	// Find the control loop and assign the new target value, in the same loop 
	// try to find a max value and checks if everybody has registred its value.
	bool allControlLoopsPaidAVisit = true;
	ControlValue tmpMaxTarget = 0;
	UInt32 maxReferenceParameter = 0;
	IOPlatformCtrlLoop *tmpCurrentWinningLoop = NULL;
	int i;
	
	for ( i = 0 ; i < mActualNumberOfRegistredControlLoops ; i++ )
	{
		// Assign value:
		if ( mRegistredControlLoopsArray[i].ctrlLoop == settingCtrlLoop )
		{
			mRegistredControlLoopsArray[i].hasActiveValue	= true;
			mRegistredControlLoopsArray[i].currentActiveValue = target;
			mRegistredControlLoopsArray[i].referenceParameter = referenceParameter;
			
			//CONTROL_DLOG("setTargetValue( %ld, %d) value assigned at index %d / %d\n", target, ( settingCtrlLoop->getCtrlLoopID() ? settingCtrlLoop->getCtrlLoopID()->unsigned16BitValue() : 0xFF ), i, mActualNumberOfRegistredControlLoops);
		}
		
		// Now checks if if changes the max value:
		if ( mRegistredControlLoopsArray[i].hasActiveValue )
		{
			if ( maxReferenceParameter < mRegistredControlLoopsArray[i].referenceParameter )
			{
				maxReferenceParameter = mRegistredControlLoopsArray[i].referenceParameter;
				tmpMaxTarget = mRegistredControlLoopsArray[i].currentActiveValue;
				tmpCurrentWinningLoop = mRegistredControlLoopsArray[i].ctrlLoop;
			}
		}
		else
		{
			allControlLoopsPaidAVisit = false;
		}
	}

	// If we do not have a winning control loop or not al the control loops paid a visit we do not wish to continue.
	if ( ( tmpCurrentWinningLoop ) && ( allControlLoopsPaidAVisit ) )
	{
		// The new value becomes the official candidate:
		mChosenTarget = tmpMaxTarget;
		mChosenTargetOwner = tmpCurrentWinningLoop;

		if ( mChosenTargetOwner == NULL )
		{
			CONTROL_DLOG("****** %s illegal winner %p !!\n", super::getControlDescKey()->getCStringNoCopy(), mChosenTargetOwner );
		}
		else
		{
			CONTROL_DLOG("****** %s %ld of %d is the lucky winner !!\n", super::getControlDescKey()->getCStringNoCopy(), mChosenTarget, ( mChosenTargetOwner->getCtrlLoopID() ? mChosenTargetOwner->getCtrlLoopID()->unsigned16BitValue() : 0xFF ) );

			privateSetControlTargetValue();
		}
		
		// Forget that all controls paid a visit to start a new loop:
		for ( i = 0 ; i < mActualNumberOfRegistredControlLoops ; i++ )
			mRegistredControlLoopsArray[i].hasActiveValue = false;
	}
	
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockUnlock(mVirtualControlLock);

/*   Commented as it is not used so let's not waste time
	// notify control loops that the value changed outside the lock !!!
	if ( ( ctrlLoops ) && ( tmpCurrentWinningLoop ) && ( allControlLoopsPaidAVisit ) )
	{
		IOPlatformCtrlLoop * aCtrlLoop;
		int index, count;
		count = ctrlLoops->getCount();
		for (index = 0; index < count; index++)
		{
			if ((aCtrlLoop = OSDynamicCast( IOPlatformCtrlLoop, ctrlLoops->getObject(index) )) != NULL)
				aCtrlLoop->controlTargetValueWasSet( this, target );
		}
	}
*/
}

IOService *PowerMac12_1VirtualControl::getControlDriver( void )
{
	return mHWControl->getControlDriver();
}

// returns a reference to the controlled control (!?)
IOPlatformControl *PowerMac12_1VirtualControl::targetControl()
{
	return mHWControl;
}

ControlValue PowerMac12_1VirtualControl::adjustLowTargetControl(ControlValue newTarget)
{
	ControlValue returnTarget = newTarget;

	if ( mReferenceControlLoop )
	{
		// The control value stored in the plist refers to the case of a control that has 0 as min
		// fan speed. We actually need to correct it considering the real min() value.
		SInt64 referenceValue = mReferenceControlLoop->averagePower().sensValue;
		SInt32 newRefPoint = ((SInt64)referenceValue * (SInt64)mControlLoopSlope ) >> 16;
		SInt32 newMin = ( ( newRefPoint  + mControlLoopOffset ) >> 16 ) + getControlMinValue();

		if ( newMin > 0 )
		{
			returnTarget = max (newTarget, (ControlValue) newMin);

			CONTROL_DLOG("=-=*** TARGET CORRECTION    *** %s ( %d * %d) + %d + %d -> %d max (%d , %d) = %d\n",
						super::getControlDescKey()->getCStringNoCopy(),
						(SInt32)(referenceValue >> 16), (uint32_t)(mControlLoopSlope >> 16), (SInt32)(mControlLoopOffset>> 16), getControlMinValue(), newMin,
						newMin, newTarget, returnTarget);
		}
	}
	else
	{
		CONTROL_DLOG("=-=*** TARGET CORRECTION    *** %s no reference control loop\n", super::getControlDescKey()->getCStringNoCopy());
	}
	
	return returnTarget;
}	

ControlValue PowerMac12_1VirtualControl::rubberBanding(ControlValue newTarget)
{
	ControlValue returnTarget = newTarget;
	
	if ( ( mRubberBandControl ) && ( mRubberBandControl->isRegistered() ) )
	{
		ControlValue referenceValue = mRubberBandControl->getTargetValue();
		SInt32 newRefPoint = ( (SInt64)referenceValue * (SInt64)mControlSlope );
		SInt64 newPoint = newRefPoint + mControlOffset;

		if ( newPoint > 0 )
		{
			UInt32 newValue = newPoint >> 16;
			
			returnTarget = max (newTarget, newValue);
			
			CONTROL_DLOG("=-=*** TARGET RUBBERBANDING *** %s ( %ld * %ld ) + %ld -> %ld max (%ld , %ld) = %ld\n",
						super::getControlDescKey()->getCStringNoCopy(),
						referenceValue, mControlSlope >>16, mControlOffset >>16, newValue,
						newValue, newTarget, returnTarget);
		}
	}
	else
	{
		CONTROL_DLOG("=-=*** TARGET RUBBERBANDING *** %s no reference control or no registration\n", super::getControlDescKey()->getCStringNoCopy());
	}

	return returnTarget;
}	

IOReturn PowerMac12_1VirtualControl::privateSetControlTargetValue( void )
{
	// Adjust the min value if necessary:
	mChosenTarget = adjustLowTargetControl( rubberBanding( mChosenTarget ) );
	
	// Make sure it does not max out:
	mChosenTarget = min ( mChosenTarget, getControlMaxValue( ) );

	if (mHWControl->sendTargetValue( mChosenTarget ))
	{
		mHWControl->setTargetValue( mChosenTarget );
		CONTROL_DLOG("%s -> %s->%ld %ld\n", super::getControlDescKey()->getCStringNoCopy(), mHWControl->getControlDescKey()->getCStringNoCopy(), mChosenTarget, mHWControl->getCurrentValue());
	}
	else
	{
		CONTROL_DLOG("%s -> %s failed to send target value %ld\n", super::getControlDescKey()->getCStringNoCopy(),  mHWControl->getControlDescKey()->getCStringNoCopy(), mChosenTarget);
		return kIOReturnNoDevice;
	}
	
	return kIOReturnSuccess;
}