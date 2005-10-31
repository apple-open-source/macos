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
//#define GROUPVIRTUALCONTROL_DEBUG
//#define GROUPVIRTUALCONTROL_DYNAMICS

#ifdef CONTROL_DEBUG
#define CONTROL_DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define CONTROL_DLOG(fmt, args...)
#endif

#ifdef GROUPVIRTUALCONTROL_DEBUG
#define GROUPVIRTUALCONTROL_DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define GROUPVIRTUALCONTROL_DLOG(fmt, args...)
#endif

#ifdef GROUPVIRTUALCONTROL_DYNAMICS
#define GROUPVIRTUALCONTROL_DYNAMICLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define GROUPVIRTUALCONTROL_DYNAMICLOG(fmt, args...)
#endif

#include "IOGroupVirtualControl.h"
#include "IOPlatformPIDCtrlLoop.h"
#include "PowerMac11_2_ThermalProfile.h"
#include <kern/clock.h>
#include <kern/thread_call.h>

void controlUpdateHandle( thread_call_param_t		param0, thread_call_param_t		param1)
{
	IOGroupVirtualControl *vControl = OSDynamicCast( IOGroupVirtualControl, (OSObject*)param0 );

	if ( vControl )
	{
		GROUPVIRTUALCONTROL_DLOG("controlUpdateHandle\n");
	
		vControl->UpdateControlsWithCurrentTargetValue();
	}
}

#define super OSObject
OSDefineMetaClassAndStructors(IOGroupVirtualControl, OSObject)

bool IOGroupVirtualControl::init( void )
{
	// Creates a lock to prevent overalpping accesses
	mVirtualControlLock = IORecursiveLockAlloc();
    if (mVirtualControlLock == NULL)
		return false;

	// Creates the thread that is reposoble for writing the controls:
	mTimedThreadCallToUpdateControls = thread_call_allocate( controlUpdateHandle, (thread_call_param_t)this);
	if ( mTimedThreadCallToUpdateControls == NULL )
		return false;

	// Sets the limiting value to its default:
	mLimitingLowerDelta = kMaxNegativeFanSpeed;

	return( super::init() );
}

void IOGroupVirtualControl::free( void )
{
	int i;
	
	for ( i = 0 ; i < mActualNumberOfIntakeFans ; i++)
	{
		if ( intakeFans[i] )
		{
			intakeFans[i]->release();
			intakeFans[i] = NULL;
		}
	}
	mActualNumberOfIntakeFans = 0;

	for ( i = 0 ; i < mActualNumberOfExaustFans ; i++)
	{
		if ( exaustFans[i] )
		{
			exaustFans[i]->release();
			exaustFans[i] = NULL;
		}
	}
	mActualNumberOfExaustFans = 0;

	for ( i = 0 ; i < mActualNumberOfPumps ; i++)
	{
		if ( pumps[i] )
		{
			pumps[i]->release();
			pumps[i] = NULL;
		}
	}
	mActualNumberOfPumps = 0;
	
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

	if ( mTimedThreadCallToUpdateControls )
	{
		thread_call_cancel( mTimedThreadCallToUpdateControls );
		thread_call_free( mTimedThreadCallToUpdateControls );
		mTimedThreadCallToUpdateControls = NULL;
	}
	
	if ( mVirtualControlLock )
	{
		IORecursiveLockFree ( mVirtualControlLock );
		mVirtualControlLock = NULL;
	}

	super::free();
}

UInt16 IOGroupVirtualControl::numberOfPumpsInThisSystem()
{
	UInt16 pumpCount = 0;

	if (  IOService::fromPath ("/smu/@4a00/@7", gIODTPlane) != NULL )
	{
		pumpCount++;
	}

	if (  IOService::fromPath ("/smu/@4a00/@6", gIODTPlane) != NULL )
	{
		pumpCount++;
	}
	
	GROUPVIRTUALCONTROL_DLOG("IOGroupVirtualControl::numberOfPumpsInThisSystem returns %d\n", pumpCount);
	
	return pumpCount;
}


// Scaling / Transformations
ControlValue IOGroupVirtualControl::setIntakeFanValueFromControlValue(ControlValue newValue, ControlValue maxValue, IOPlatformControl *control)
{
	// the intake value is 97% of the control value
	UInt32 newControlValue = ( newValue * 97 ) / 100;

	if ( newControlValue < control->getControlMinValue() )
		newControlValue = control->getControlMinValue();

	if ( newControlValue > control->getControlMaxValue() )
		newControlValue = control->getControlMaxValue();

	if (control->sendTargetValue( newControlValue ))
	{
		control->setTargetValue( newControlValue );
		GROUPVIRTUALCONTROL_DLOG("IOGroupVirtualControl::setIntakeFanValueFromControlValue(%ld) %s->%ld %ld\n", newValue, control->getControlDescKey()->getCStringNoCopy(), newControlValue, control->getCurrentValue());
	}
	else
	{
		CONTROL_DLOG("IOGroupVirtualControl::setIntakeFanValueFromControlValue(%ld) %s failed to send intake target value %ld\n", newControlValue,  control->getControlDescKey()->getCStringNoCopy(), newValue);
	}
	
	return (ControlValue)newControlValue;
}


ControlValue IOGroupVirtualControl::setExaustFanValueFromControlValue(ControlValue newValue, ControlValue maxValue, IOPlatformControl *control)
{
	// the exaust value is 100% of the control value
	UInt32 newControlValue = newValue;

	if ( newControlValue < control->getControlMinValue() )
		newControlValue = control->getControlMinValue();

	if ( newControlValue > control->getControlMaxValue() )
		newControlValue = control->getControlMaxValue();

	if (control->sendTargetValue( newControlValue ))
	{
		control->setTargetValue( newControlValue );
		GROUPVIRTUALCONTROL_DLOG("IOGroupVirtualControl::setExaustFanValueFromControlValue(%ld) %s->%ld %ld\n", newValue, control->getControlDescKey()->getCStringNoCopy(), newControlValue, control->getCurrentValue());
	}
	else
	{
		CONTROL_DLOG("IOGroupVirtualControl::setExaustFanValueFromControlValue(%ld) %s failed to send intake target value %ld\n", newControlValue,  control->getControlDescKey()->getCStringNoCopy(), newValue);
	}
	

	return (ControlValue)newControlValue;
}

ControlValue IOGroupVirtualControl::setPumpValueFromControlValue(ControlValue newValue, ControlValue maxValue, IOPlatformControl *control)
{
	// the pump value is 100% of the control value scaled in the same way (so if the fan is going 30% so should the pump)
	UInt32 newControlValue = ( newValue * control->getControlMaxValue() ) / maxValue;

	if ( newControlValue < control->getControlMinValue() )
		newControlValue = control->getControlMinValue();

	if ( newControlValue > control->getControlMaxValue() )
		newControlValue = control->getControlMaxValue();

	if (control->sendTargetValue( newControlValue ))
	{
		control->setTargetValue( newControlValue );
		GROUPVIRTUALCONTROL_DLOG("IOGroupVirtualControl::setPumpValueFromControlValue(%ld) %s->%ld %ld\n", newValue, control->getControlDescKey()->getCStringNoCopy(), newControlValue, control->getCurrentValue());
	}
	else
	{
		CONTROL_DLOG("IOGroupVirtualControl::setPumpValueFromControlValue(%ld) %s failed to send intake target value %ld\n", newControlValue,  control->getControlDescKey()->getCStringNoCopy(), newValue);
	}
	

	return (ControlValue)newControlValue;
}

void IOGroupVirtualControl::programSetWithValue(ControlValue newTargetValue)
{
	GROUPVIRTUALCONTROL_DYNAMICLOG("******  programSetWithValue(%ld)\n", newTargetValue);
	int i;	
	ControlValue maxControlValue = exaustFans[0]->getControlMaxValue();
		
	for ( i = 0 ; i < mActualNumberOfIntakeFans ; i++)
	{
		if ( intakeFans[i] )
		{
			setIntakeFanValueFromControlValue(newTargetValue, maxControlValue, intakeFans[i] );
		}
	}

	for ( i = 0 ; i < mActualNumberOfExaustFans ; i++)
	{
		if ( exaustFans[i] )
		{
			setExaustFanValueFromControlValue(newTargetValue, maxControlValue , exaustFans[i] );
		}
	}

	for ( i = 0 ; i < mActualNumberOfPumps ; i++)
	{
		if ( pumps[i] )
		{
			setPumpValueFromControlValue(newTargetValue, maxControlValue , pumps[i] );
		}
	}
}

/*! @function registerControlLoop
	@abstract registers a control Loop with this virtual control. */

IOReturn IOGroupVirtualControl::registerControlLoop( IOPlatformCtrlLoop *newControlLoop)
{
	if ( newControlLoop == NULL )
		return kIOReturnBadArgument;

	if ( mActualNumberOfRegistredControlLoops == kMaxSetOfControlLoops )
		return kIOReturnNoResources;
		
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);

	int i;
	bool isAlredyHere = false;
	
	for ( i = 0; i <  mActualNumberOfRegistredControlLoops ; i ++ )
	{
		isAlredyHere |= ( mRegistredControlLoopsArray[i].ctrlLoop == newControlLoop );
	}
	
	if ( ! isAlredyHere )
	{
		// Add the new control loop:
		mRegistredControlLoopsArray[ mActualNumberOfRegistredControlLoops ].ctrlLoop = newControlLoop;
		mRegistredControlLoopsArray[ mActualNumberOfRegistredControlLoops ].hasActiveValue = false;
		mRegistredControlLoopsArray[ mActualNumberOfRegistredControlLoops ].currentActiveValue = 0;
				
		mRegistredControlLoopsArray[ mActualNumberOfRegistredControlLoops ].ctrlLoop->retain();

		mActualNumberOfRegistredControlLoops++;

		GROUPVIRTUALCONTROL_DLOG("IOGroupVirtualControl::registerControlLoop  control set %d : %d) !!\n", mActualNumberOfRegistredControlLoops ,
			( newControlLoop->getCtrlLoopID() ? newControlLoop->getCtrlLoopID()->unsigned16BitValue() : 0xFF ) );
	}
	
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockUnlock(mVirtualControlLock);

	return kIOReturnSuccess;
}

/*! @function addControlSet
	@abstract Adds a set of controls (intake/exaust/pump) to the existing set. */

IOReturn IOGroupVirtualControl::addIntakeFan( IOPlatformControl *intakeFan )
{
	if ( intakeFan == NULL )
		return kIOReturnBadArgument;

	if ( mActualNumberOfIntakeFans == kMaxSetOfControls )
		return kIOReturnNoResources;

	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);

	bool isAlredyHere = false;
	int i;
	for ( i = 0 ; i < mActualNumberOfIntakeFans ; i++ )
		isAlredyHere |= ( intakeFans[i] == intakeFan );

	if ( ! isAlredyHere )
	{
		intakeFans[mActualNumberOfIntakeFans] = intakeFan;
		intakeFans[mActualNumberOfIntakeFans]->retain();
		mActualNumberOfIntakeFans++;
	}
	
	// Locks ONLY self contained code !!!!
	if (mVirtualControlLock != NULL)
		IORecursiveLockUnlock(mVirtualControlLock);
			
	return kIOReturnSuccess;
}

IOReturn IOGroupVirtualControl::addExaustFan( IOPlatformControl *exaustFan )
{
	if ( exaustFan == NULL )
		return kIOReturnBadArgument;

	if ( mActualNumberOfExaustFans == kMaxSetOfControls )
		return kIOReturnNoResources;
		
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);

	// If we already have this do not add it:
	bool isAlredyHere = false;
	int i;
	for ( i = 0 ; i < mActualNumberOfExaustFans ; i++ )
		isAlredyHere |= ( exaustFans[i] == exaustFan );

	if ( ! isAlredyHere )
	{
		exaustFans[mActualNumberOfExaustFans] = exaustFan;
		exaustFans[mActualNumberOfExaustFans]->retain();
		mActualNumberOfExaustFans++;
	}
	
	// Locks ONLY self contained code !!!!
	if (mVirtualControlLock != NULL)
		IORecursiveLockUnlock(mVirtualControlLock);

	return kIOReturnSuccess;
}

IOReturn IOGroupVirtualControl::addPump( IOPlatformControl *pump )
{
	if ( pump == NULL )
		return kIOReturnBadArgument;

	if ( mActualNumberOfPumps == kMaxSetOfControls )
		return kIOReturnNoResources;

	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);

	bool isAlredyHere = false;
	int i;
	for ( i = 0 ; i < mActualNumberOfPumps ; i++ )
		isAlredyHere |= ( pumps[i] == pump );

	if ( ! isAlredyHere )
	{
		pumps[mActualNumberOfPumps] = pump;
		pumps[mActualNumberOfPumps]->retain();
		mActualNumberOfPumps++;
	}
	
	// Locks ONLY self contained code !!!!
	if (mVirtualControlLock != NULL)
		IORecursiveLockUnlock(mVirtualControlLock);

	return kIOReturnSuccess;
}	

/*! @function setDecresingLimitingFactor
	@abstract sets a new value for the limiting factor of how the fans can decrease */
IOReturn  IOGroupVirtualControl::setDecresingLimitingFactor( UInt32 newValue )
{
	mLimitingLowerDelta = newValue;
	return( kIOReturnSuccess );		// this gets rid of a compile warning.  Should this function be a void instead of an IOReturn? -- bg
}
	
/*! @function isGroupManaged
	@abstract Boolean to tell whether the platform plugin is managing these controls.  Some controls are only represented so that they can report errors/failures. */
bool IOGroupVirtualControl::isGroupManaged( void )
{
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);
	  
	bool allControlsManaged = true;
	int i;

	for ( i = 0 ; i < mActualNumberOfIntakeFans ; i++)
	{
		if ( intakeFans[i] )
		{
			allControlsManaged &= intakeFans[i]->isManaged();
		}
	}

	for ( i = 0 ; i < mActualNumberOfExaustFans ; i++)
	{
		if ( exaustFans[i] )
		{
			allControlsManaged &= exaustFans[i]->isManaged();
		}
	}

	int numberOfManagedPumps = 0;

	for ( i = 0 ; i < mActualNumberOfPumps ; i++)
	{
		if ( ( pumps[i] ) && ( pumps[i]->isManaged() ) )
		{
			numberOfManagedPumps++;
		}
	}
	
	allControlsManaged &= ( numberOfManagedPumps >= numberOfPumpsInThisSystem() );



	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockUnlock(mVirtualControlLock);
	  
	return allControlsManaged;
}


/*! @function isRegistered
	@abstract Tells whether there is an IOService * associated with these controls. */
OSBoolean * IOGroupVirtualControl::isGroupRegistered( void )
{
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);

	bool allControlsRegistred = true;
	int i;

	for ( i = 0 ; i < mActualNumberOfIntakeFans ; i++)
	{
		if ( intakeFans[i] )
		{
			allControlsRegistred &= ( intakeFans[i]->isRegistered() == kOSBooleanTrue );
			GROUPVIRTUALCONTROL_DYNAMICLOG("isGroupRegistered %s->%d\n", intakeFans[i]->getControlDescKey()->getCStringNoCopy(), ( intakeFans[i]->isRegistered() == kOSBooleanTrue ) );
		}
	}

	for ( i = 0 ; i < mActualNumberOfExaustFans ; i++)
	{
		if ( exaustFans[i] )
		{
			allControlsRegistred &= ( exaustFans[i]->isRegistered() == kOSBooleanTrue );
			GROUPVIRTUALCONTROL_DYNAMICLOG("isGroupRegistered %s->%d\n", exaustFans[i]->getControlDescKey()->getCStringNoCopy(), ( exaustFans[i]->isRegistered() == kOSBooleanTrue ) );
		}
	}

	// for pumps it is a different story since this may have one or two pumps
	int numberOfRegistedPumps = 0;
	
	for ( i = 0 ; i < mActualNumberOfPumps ; i++)
	{
		if ( ( pumps[i] ) && ( pumps[i]->isRegistered() == kOSBooleanTrue ) )
		{
			numberOfRegistedPumps++;
			GROUPVIRTUALCONTROL_DYNAMICLOG("isGroupRegistered %s->%d\n", pumps[i]->getControlDescKey()->getCStringNoCopy(), ( pumps[i]->isRegistered() == kOSBooleanTrue ) );
		}
	}
	
	allControlsRegistred &= ( numberOfRegistedPumps >= numberOfPumpsInThisSystem() );

	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockUnlock(mVirtualControlLock);

	return ( allControlsRegistred ? kOSBooleanTrue : kOSBooleanFalse );
}

// target-value
ControlValue IOGroupVirtualControl::getTargetValue( IOPlatformCtrlLoop **setter)
{
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);

	ControlValue returnMe;
	
	if ( mChosenTargetOwner != NULL )
	{
		if ( setter != NULL )
		{
			*setter = mChosenTargetOwner;
		}
		
		returnMe = mChosenTarget;
	}
	else
	{
		returnMe = 0;
	}
	
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockUnlock(mVirtualControlLock);

	return returnMe;
}

void IOGroupVirtualControl::setTargetValue( ControlValue target, IOPlatformCtrlLoop *settingCtrlLoop, SInt32 referenceParameter)
{
	if ( settingCtrlLoop == NULL )
	{
		GROUPVIRTUALCONTROL_DLOG("setTargetValue( %ld, %p, %ld) !!!!!!!!!!!!!!!\n", target, settingCtrlLoop, referenceParameter);
		return;
	}

	GROUPVIRTUALCONTROL_DLOG("setTargetValue( %ld, %d, %ld)\n", target, ( settingCtrlLoop->getCtrlLoopID() ? settingCtrlLoop->getCtrlLoopID()->unsigned16BitValue() : 0xFF ), referenceParameter);
	
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);
	
	// Find the control loop and assign the new target value, in the same loop 
	// try to find a max value and checks if everybody has registred its value.
	bool allControlLoopsPaidAVisit = true;
	ControlValue tmpMaxTarget = 0;
	SInt32 maxReferenceParameter = -10000;
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
			
			GROUPVIRTUALCONTROL_DLOG("setTargetValue( %ld, %d, %ld) value assigned at index %d / %d\n",
				target, ( settingCtrlLoop->getCtrlLoopID() ? settingCtrlLoop->getCtrlLoopID()->unsigned16BitValue() : 0xFF ), referenceParameter, i, mActualNumberOfRegistredControlLoops);
		}
	
		// Now checks if if changes the max value:
		if ( mRegistredControlLoopsArray[i].hasActiveValue )
		{
			GROUPVIRTUALCONTROL_DLOG("%ld < %ld ? %p = %p\n", maxReferenceParameter, mRegistredControlLoopsArray[i].referenceParameter, tmpCurrentWinningLoop, mRegistredControlLoopsArray[i].ctrlLoop);
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
		//IOLog("MarkC-Log2: %ld,", tmpMaxTarget);

		// Finds the direction of the dan change:
		if ( tmpMaxTarget < mChosenTarget )
		{
			// We are slowing down by:
			UInt32 deltaSpeed = mChosenTarget - tmpMaxTarget;
			
			// Clamp the change:
			if ( deltaSpeed > mLimitingLowerDelta )
			{
				GROUPVIRTUALCONTROL_DYNAMICLOG("****** setTargetValue Dfan = -%ld, is too big so %ld becomes %ld\n", deltaSpeed , tmpMaxTarget, mChosenTarget - mLimitingLowerDelta);
			
				tmpMaxTarget = mChosenTarget - mLimitingLowerDelta;
			}
		}
	
		//IOLog(/* "MarkC-Log2:" */ "%ld (%ld)\n", tmpMaxTarget, maxReferenceParameter);
	
		// The new value becomes the official candidate:
		mChosenTarget = tmpMaxTarget;
		mChosenTargetOwner = tmpCurrentWinningLoop;

		if ( mChosenTargetOwner == NULL )
		{
			GROUPVIRTUALCONTROL_DYNAMICLOG("****** UpdateControlsWithCurrentTargetValue illegal winner %p !!\n", mChosenTargetOwner );
		}
		else
		{
			GROUPVIRTUALCONTROL_DYNAMICLOG("****** UpdateControlsWithCurrentTargetValue %ld of %d is the lucky winner !!\n", mChosenTarget, ( mChosenTargetOwner->getCtrlLoopID() ? mChosenTargetOwner->getCtrlLoopID()->unsigned16BitValue() : 0xFF ) );
			
			// Keep track of the max value and update
			if ( mTimedThreadCallToUpdateControls != NULL )
			{
				thread_call_enter( mTimedThreadCallToUpdateControls );
			}
			//UpdateControlsWithCurrentTargetValue();
		}
		
		// Forget that all controls paid a visit to start a new loop:
		for ( i = 0 ; i < mActualNumberOfRegistredControlLoops ; i++ )
			mRegistredControlLoopsArray[i].hasActiveValue = false;
	}
	
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockUnlock(mVirtualControlLock);
}

void IOGroupVirtualControl::UpdateControlsWithCurrentTargetValue()
{
	// Locks ONLY self contained code !!!!
    if (mVirtualControlLock != NULL)
      IORecursiveLockLock(mVirtualControlLock);

 	if 	( mChosenTargetOwner != 0 )
	{
		programSetWithValue ( mChosenTarget );
	}

	if (mVirtualControlLock != NULL)
		IORecursiveLockUnlock(mVirtualControlLock);
}

