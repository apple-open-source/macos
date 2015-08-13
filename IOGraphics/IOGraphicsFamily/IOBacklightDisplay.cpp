/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOTimerEventSource.h>

#define IOFRAMEBUFFER_PRIVATE
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/ndrvsupport/IOMacOSVideo.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPM.h>

/*
    We further divide the actual display panel brightness levels into four
    IOKit power states which we export to our superclass.
    
    In the lowest state, the display is off.  This state consists of only one
    of the brightness levels, the lowest. In the next state it is in the dimmest
    usable state. The highest state consists of all the brightness levels.
    
    The display has no state or configuration or programming that would be
    saved/restored over power state changes, and the driver does not register
    with the superclass as an interested driver.
    
    This driver doesn't have much to do. It changes between the four power state
    brightnesses on command from the superclass, and it notices which brightness
    level the user has set.
    
    The only smart thing it does is keep track of which of the brightness levels
    the user has selected, and it never exceeds that on command from the display
    device object.
*/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IODisplay

OSDefineMetaClassAndStructors(IOBacklightDisplay, IODisplay)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSMetaClassDefineReservedUnused(IOBacklightDisplay, 0);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 1);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 2);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 3);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 4);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 5);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 6);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 7);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 8);
OSMetaClassDefineReservedUnused(IOBacklightDisplay, 9);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleBacklightDisplay : public IOBacklightDisplay
{
    OSDeclareDefaultStructors(AppleBacklightDisplay)

protected:
    // User preferred brightness level
//    SInt32      fCurrentUserBrightness;
    UInt32      fCurrentPowerState;
    SInt32      fMinBrightness;
    SInt32      fMaxBrightness;

    IOInterruptEventSource * fDeferredEvents;

    IOTimerEventSource *     fFadeTimer;
	AbsoluteTime		     fFadeDeadline;
	AbsoluteTime		     fFadeInterval;
	uint32_t				 fFadeState;
	uint32_t				 fFadeStateEnd;
	uint32_t				 fFadeStateFadeMin;
	uint32_t				 fFadeStateFadeMax;
	uint32_t				 fFadeStateFadeDelta;
    uint8_t                  fClamshellSlept;
	uint8_t					 fDisplayDims;
	uint8_t					 fProviderPower;
	uint8_t					 fFadeBacklight;
	uint8_t					 fFadeGamma;
	uint8_t					 fFadeDown;
	uint8_t					 fFadeAbort;

public:

    // IOService overrides
    virtual IOService * probe( IOService *, SInt32 * );
    virtual bool start( IOService * provider );
    virtual void stop( IOService * provider );

    virtual IOReturn setPowerState( unsigned long, IOService * );
    virtual unsigned long maxCapabilityForDomainState( IOPMPowerFlags );
    virtual unsigned long initialPowerStateForDomainState( IOPMPowerFlags );
    virtual unsigned long powerStateForDomainState( IOPMPowerFlags );

    // IODisplay overrides
    virtual void initPowerManagement( IOService * );
    virtual bool doIntegerSet( OSDictionary * params,
                               const OSSymbol * paramName, UInt32 value );
    virtual bool doUpdate( void );
    virtual void makeDisplayUsable( void );
    virtual IOReturn framebufferEvent( IOFramebuffer * framebuffer,
                                       IOIndex event, void * info );

private:
	bool updatePowerParam(void);
    void handlePMSettingCallback(const OSSymbol *, OSObject *, uintptr_t);
    static void _deferredEvent( OSObject * target,
                                IOInterruptEventSource * evtSrc, int intCount );
    void fadeAbort(void);
    void fadeWork(IOTimerEventSource * sender);
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOBacklightDisplay

OSDefineMetaClassAndStructors(AppleBacklightDisplay, IOBacklightDisplay)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//#define kIOBacklightUserBrightnessKey   "IOBacklightUserBrightness"
#define fPowerUsesBrightness	fMaxBrightness

#define IOG_FADE 1

enum 
{
    // fFadeState
    kFadeIdle        = (1 << 24),
    kFadePostDelay   = (2 << 24),
    kFadeUpdatePower = (3 << 24),

	kFadeDimLevel    = ((uint32_t) (0.75f * 1024)),
	kFadeMidLevel    = ((uint32_t) (0.50f * 1024)),
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// probe
//

IOService * AppleBacklightDisplay::probe( IOService * provider, SInt32 * score )
{
    IOFramebuffer *     framebuffer;
    IOService *         ret = 0;
    UInt32              displayType;
    uintptr_t           connectFlags;

    do
    {
        if (!gIOFBHaveBacklight)
            continue;

        if (!super::probe(provider, score))
            continue;

        framebuffer = (IOFramebuffer *) getConnection()->getFramebuffer();

        for (IOItemCount idx = 0; idx < framebuffer->getConnectionCount(); idx++)
        {
            if (kIOReturnSuccess != framebuffer->getAttributeForConnection(idx,
                    kConnectionFlags, &connectFlags))
                continue;
            if (0 == (kIOConnectionBuiltIn & connectFlags))
                continue;
            if (kIOReturnSuccess != framebuffer->getAppleSense(idx, NULL, NULL, NULL, &displayType))
                continue;
            if ((kPanelTFTConnect != displayType)
                    && (kGenericLCD != displayType)
                    && (kPanelFSTNConnect != displayType))
                continue;

            ret = this;         // yes, we will control the panel
            break;
        }
    }
    while (false);

    return (ret);
}

bool AppleBacklightDisplay::start( IOService * provider )
{
    IOFramebuffer * fb;
    if (!super::start(provider)) return (false);

	fClamshellSlept = gIOFBCurrentClamshellState;
    fb = getConnection()->getFramebuffer();

    fDeferredEvents = IOInterruptEventSource::interruptEventSource(this, _deferredEvent);
    if (fDeferredEvents) fb->getControllerWorkLoop()->addEventSource(fDeferredEvents);

	fFadeTimer = IOTimerEventSource::timerEventSource(this, 
								OSMemberFunctionCast(IOTimerEventSource::Action, this, 
													&AppleBacklightDisplay::fadeWork));
    if (fFadeTimer) fb->getControllerWorkLoop()->addEventSource(fFadeTimer);

	fFadeState = kFadeIdle;

    fb->setProperty(kIOFBBuiltInKey, this, 0);

    return (true);
}

void AppleBacklightDisplay::stop( IOService * provider )
{
    if (fDeferredEvents)
    {
        getConnection()->getFramebuffer()->getControllerWorkLoop()->removeEventSource(fDeferredEvents);
        fDeferredEvents->release();
        fDeferredEvents = 0;
    }

    fadeAbort();
    if (fFadeTimer)
    {
    	fFadeTimer->disable();
        getConnection()->getFramebuffer()->getControllerWorkLoop()->removeEventSource(fFadeTimer);
        fFadeTimer->release();
	    fFadeTimer = 0;
    }

    return (super::stop(provider));
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//
// This method overrides the one in IODisplay to do PowerBook-only
// power management of the display.

void AppleBacklightDisplay::initPowerManagement( IOService * provider )
{
    OSDictionary * displayParams;    
	OSObject *     obj;
    OSNumber *     num;

    static const IOPMPowerState ourPowerStates[kIODisplayNumPowerStates] = {
        // version,
        //   capabilityFlags,      outputPowerCharacter, inputPowerRequirement,
        { 1, 0,                                     0, 0,           0,0,0,0,0,0,0,0 },
        { 1, 0,                                     0, 0,           0,0,0,0,0,0,0,0 },
        { 1, IOPMDeviceUsable,                      0, kIOPMPowerOn, 0,0,0,0,0,0,0,0 },
        { 1, IOPMDeviceUsable | IOPMMaxPerformance, 0, kIOPMPowerOn, 0,0,0,0,0,0,0,0 }
        // staticPower, unbudgetedPower, powerToAttain, timeToAttain, settleUpTime,
        // timeToLower, settleDownTime, powerDomainBudget
    };

    // Check initial state of "DisplaySleepUsesDim"
    obj = getPMRootDomain()->copyPMSetting(
                    const_cast<OSSymbol *>(gIOFBPMSettingDisplaySleepUsesDimKey));
    fDisplayDims = (!(num = OSDynamicCast(OSNumber, obj)) || (num->unsigned32BitValue()));
    if (obj) obj->release();

	fCurrentPowerState = -1U;

    displayParams = OSDynamicCast(OSDictionary, copyProperty(gIODisplayParametersKey));
    if (displayParams)
    {
		SInt32 value, min, max;

        if (getIntegerRange(displayParams, gIODisplayPowerStateKey,
                             &value, &min, &max))
        {
        }
		else
		{
			fMinBrightness = 0;
			fMaxBrightness = 255;
		}
#if IOG_FADE
		OSDictionary * newParams;
		newParams = OSDictionary::withDictionary(displayParams);
		addParameter(newParams, gIODisplayFadeTime1Key, 0, 10000);
		setParameter(newParams, gIODisplayFadeTime1Key, 500);
		addParameter(newParams, gIODisplayFadeTime2Key, 0, 10000);
		setParameter(newParams, gIODisplayFadeTime2Key, 4000);
		addParameter(newParams, gIODisplayFadeTime3Key, 0, 10000);
		setParameter(newParams, gIODisplayFadeTime3Key, 500);
		addParameter(newParams, gIODisplayFadeStyleKey, 0, 10);
		setParameter(newParams, gIODisplayFadeStyleKey, 0);
		setProperty(gIODisplayParametersKey, newParams);
		newParams->release();
#endif
        displayParams->release();
    }

    // initialize superclass variables
    PMinit();
    // attach into the power management hierarchy
    provider->joinPMtree(this);

    // register ourselves with policy-maker (us)
    registerPowerDriver(this, (IOPMPowerState *) ourPowerStates, kIODisplayNumPowerStates);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// setPowerState
//

//#define DEBGFADE(fmt, args...) do { kprintf(fmt, ## args); } while(false)          
#define DEBGFADE(fmt, args...) do {} while(false)          

IOReturn AppleBacklightDisplay::setPowerState( unsigned long powerState, IOService * whatDevice )
{
    IOReturn ret = IOPMAckImplied;
    UInt32   fromPowerState;

    if (powerState >= kIODisplayNumPowerStates)
        return (IOPMAckImplied);

    IOFramebuffer * framebuffer = (IOFramebuffer *) getConnection()->getFramebuffer();

    framebuffer->fbLock();

    if (isInactive() || (fCurrentPowerState == powerState))
    {
        framebuffer->fbUnlock();
        return (IOPMAckImplied);
    }
    fromPowerState = fCurrentPowerState;
    fCurrentPowerState = powerState;
	if (fCurrentPowerState) fProviderPower = true;

	OSObject * obj;
	if ((!powerState) && (obj = copyProperty(kIOHibernatePreviewActiveKey, gIOServicePlane)))
	{
		obj->release();
	}
	else
	{
#if IOG_FADE
        SInt32         current, min, max, steps;
        OSDictionary * displayParams;
		uint32_t       fadeTime;
		uint32_t       dimFade;
		bool           doFadeDown, doFadeGamma, doFadeBacklight;

        displayParams = OSDynamicCast(OSDictionary, copyProperty(gIODisplayParametersKey));
		fadeTime        = 0;
	    doFadeDown      = true;
	    doFadeGamma     = false;
	    doFadeBacklight = true;

		DEBGFADE("AppleBacklight: ps [%d->%ld]\n", fromPowerState, powerState);

		if (gIOGFades
			&& displayParams 
			&& (getIntegerRange(displayParams, gIODisplayBrightnessFadeKey, NULL, &min, &max))
			&& (getIntegerRange(displayParams, gIODisplayBrightnessKey, &current, NULL, NULL))
			&& current
			&& !fFadeAbort)
		{
			if (current < ((kFadeMidLevel * max) / 1024)) dimFade = max;
			else                                          dimFade = (kFadeDimLevel * max) / 1024;

			if (-1U == fromPowerState) { /* boot */ }
			else if ((powerState > fromPowerState) && (1 >= fromPowerState))
			{
				// fade up from off
				fadeTime    = gIODisplayFadeTime3*1000;
				doFadeGamma = true;
				doFadeDown  = false;
			}
			if ((3 == powerState) && (1 >= fromPowerState))
			{
				// fade up from off
				fadeTime   = gIODisplayFadeTime3*1000;
				doFadeDown = false;
			}
			if ((3 == powerState) && (2 == fromPowerState))
			{
				// fade up from dim
				fadeTime   = gIODisplayFadeTime3*1000;
				max        = dimFade;
				doFadeDown = false;
			}
			else if ((0 == powerState) && (3 == fromPowerState))
			{
				// user initiated -> off
				fadeTime    = gIODisplayFadeTime1*1000;
				doFadeGamma = true;
			}
			else if (1 != gIODisplayFadeStyle)
			{
				 if ((2 == powerState) && (3 == fromPowerState))
				 {
					  if (fDisplayDims)
					  {
						 // idle initiated -> dim
						 fadeTime = gIODisplayFadeTime1*1000;
						 max      = dimFade;
					  }
				 }
				 if ((1 == powerState) && (2 == fromPowerState))
				 {
					 // idle initiated -> off
					 fadeTime    = gIODisplayFadeTime1*1000;
					 doFadeBacklight = (dimFade != max);
					 if (doFadeBacklight) current = max - dimFade;
					 doFadeGamma = true;
				 }
			}
			else if (1 == gIODisplayFadeStyle)
			{
				 if ((2 == powerState) && (3 == fromPowerState))
				 {
					 // idle initiated
					 fCurrentPowerState = 1; 
					 fadeTime           = gIODisplayFadeTime2*1000;
					 doFadeGamma        = true;
				 };
			}
			DEBGFADE("AppleBacklight: fadeTime %d style %d abort %d\n", fadeTime, gIODisplayFadeStyle, fFadeAbort);
		}

		if (fadeTime)
		{
			steps = fadeTime / 16667;

			DEBGFADE("AppleBacklight: p %d -> %ld, c %d -> m %d\n", fromPowerState, powerState, current, max);

			if (current > max)   current = max;

			fFadeStateFadeMin   = max - current;
			fFadeStateFadeMax   = max;
			fFadeStateFadeDelta = current;
			if (steps > fFadeStateFadeDelta) steps = fFadeStateFadeDelta;

			fFadeDown      = doFadeDown;
			fFadeGamma     = doFadeGamma;
			fFadeBacklight = doFadeBacklight;

			DEBGFADE("AppleBacklight:  %d -> %d\n", fFadeStateFadeMin, fFadeStateFadeDelta);

			fFadeState      = 0;
			if (!fFadeDown) updatePowerParam();

			fFadeDeadline = mach_absolute_time();
			fadeTime     /= steps;
			fFadeStateEnd = (steps - 1);
		    if (framebuffer->isWakingFromHibernateGfxOn()) fFadeState = fFadeStateEnd;
			clock_interval_to_absolutetime_interval(fadeTime, kMicrosecondScale, &fFadeInterval);
			fadeWork(fFadeTimer);
			if (fFadeDown)  ret = 20 * 1000 * 1000;
		}
		else
		{
			updatePowerParam();
			fFadeAbort = false;
		}
#else
        updatePowerParam();
#endif
	}

	if (!fCurrentPowerState) fProviderPower = false;

    framebuffer->fbUnlock();

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void AppleBacklightDisplay::fadeAbort(void)
{
    OSDictionary * displayParams;

	if (kFadeIdle == fFadeState) return;
	displayParams = OSDynamicCast(OSDictionary, copyProperty(gIODisplayParametersKey));
	if (!displayParams) return;

	// abort
	DEBGFADE("AppleBacklight: fadeAbort\n");
	doIntegerSet(displayParams, gIODisplayBrightnessFadeKey, 0);
	if (fFadeGamma)
	{
		doIntegerSet(displayParams, gIODisplayGammaScaleKey, 65536);
		doIntegerSet(displayParams, gIODisplayParametersFlushKey, 0);
	}
	displayParams->release();
	if (fFadeDown)  acknowledgeSetPowerState();
	fFadeState = kFadeIdle;
}

void AppleBacklightDisplay::fadeWork(IOTimerEventSource * sender)
{
    OSDictionary * displayParams;
	SInt32 fade, gamma, point;
    
	DEBGFADE("AppleBacklight: fadeWork(fFadeStateEnd %d, fFadeState %d, %d, fFadeStateEnd %d)\n", 
			fFadeStateEnd, fFadeState & 0xffff, fFadeState >> 24, fFadeStateEnd);

	if (kFadeIdle == fFadeState) return;
	displayParams = OSDynamicCast(OSDictionary, copyProperty(gIODisplayParametersKey));
	if (!displayParams) return;

    fFadeAbort = (fFadeDown && !fDisplayPMVars->displayIdle);

	if (fFadeAbort) fadeAbort();
	else if (fFadeState <= fFadeStateEnd)
    {
		point = fFadeState;
		if (!fFadeDown) point = (fFadeStateEnd - point);
		if (fFadeBacklight)
		{
			if (!fFadeDown && !point) fade = 0;
			else if (fFadeStateFadeMax > 0x8000)
			{
				fade = fFadeDown ? fFadeStateFadeMax : 0;
				fFadeBacklight = false;
			}
			else
			{
				fade = ((point * fFadeStateFadeDelta) / fFadeStateEnd);
				fade = fFadeStateFadeMin + fade;
			}
			DEBGFADE("AppleBacklight: backlight: %d\n", fade);
			doIntegerSet(displayParams, gIODisplayBrightnessFadeKey, fade);
		}
		if (fFadeGamma)
		{
			gamma = 65536 - ((point * 65536) / fFadeStateEnd);
			DEBGFADE("AppleBacklight: gamma: %d\n", gamma);
			doIntegerSet(displayParams, gIODisplayGammaScaleKey, gamma);
			doIntegerSet(displayParams, gIODisplayParametersFlushKey, 0);
		}

		fFadeState++;
		if (fFadeState > fFadeStateEnd) 
		{
			if (fFadeDown) updatePowerParam();
			fFadeState = kFadePostDelay;
			clock_interval_to_absolutetime_interval(500, kMillisecondScale, &fFadeInterval);
		}
	}
	else if (kFadePostDelay == fFadeState)
	{
		fFadeState = kFadeIdle;
	}

	if (kFadeIdle == fFadeState)
	{
	    DEBGFADE("AppleBacklight: fadeWork ack\n");
	    if (fFadeDown)  acknowledgeSetPowerState();
	}
	else
	{
		ADD_ABSOLUTETIME(&fFadeDeadline, &fFadeInterval);
		fFadeTimer->wakeAtTime(fFadeDeadline);
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void AppleBacklightDisplay::makeDisplayUsable( void )
{
    if (kIODisplayMaxPowerState == fCurrentPowerState)
        setPowerState(fCurrentPowerState, this);
    super::makeDisplayUsable();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// This simple device needs only power.  If the power domain is supplying
// power, the display can go to its highest state.  If there is no power
// it can only be in its lowest state, which is off.

unsigned long AppleBacklightDisplay::maxCapabilityForDomainState( IOPMPowerFlags domainState )
{
    if (domainState & IOPMPowerOn)
        return (kIODisplayMaxPowerState);
    else
        return (0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.

unsigned long AppleBacklightDisplay::initialPowerStateForDomainState( IOPMPowerFlags domainState )
{
    if (domainState & IOPMPowerOn)
        return (kIODisplayMaxPowerState);
    else
        return (0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// powerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.

unsigned long AppleBacklightDisplay::powerStateForDomainState( IOPMPowerFlags domainState )
{
    if (domainState & IOPMPowerOn)
        return (kIODisplayMaxPowerState);
    else
        return (0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Alter the backlight brightness by user request.

bool AppleBacklightDisplay::doIntegerSet( OSDictionary * params,
                                       const OSSymbol * paramName, UInt32 value )
{
    if ((paramName == gIODisplayParametersCommitKey) 
        && (kIODisplayMaxPowerState != fCurrentPowerState))
        return (true);

    if (paramName == gIODisplayBrightnessKey)
    {
#if 0
        fCurrentUserBrightness = value;
        getPMRootDomain()->setProperty(kIOBacklightUserBrightnessKey, fCurrentUserBrightness, 32);
#endif
        if (fPowerUsesBrightness && fClamshellSlept)
			value = 0;
    }

	return (super::doIntegerSet(params, paramName, value));
}

bool AppleBacklightDisplay::doUpdate( void )
{
    bool ok;

    ok = super::doUpdate();

#if 0
    OSDictionary * displayParams;
    if (fDisplayPMVars
        && (displayParams = OSDynamicCast(OSDictionary, copyProperty(gIODisplayParametersKey))))
    {
        setParameter(displayParams, gIODisplayBrightnessKey, fCurrentUserBrightness);	
        displayParams->release();
    }
#endif

    return (ok);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleBacklightDisplay::updatePowerParam(void)
{
    OSDictionary * displayParams;
	SInt32 		   value, current;
	bool	       ret;

#if DEBUG
    IOLog("brightness[%d,%d] %d\n", (int) fCurrentPowerState, (int) gIOFBLastClamshellState, (int) value);
    if (!getControllerWorkLoop()->inGate())
        OSReportWithBacktrace("AppleBacklightDisplay::updatePowerParam !inGate\n");
#endif

	DEBG1("B", " fProviderPower %d, fClamshellSlept %d, fCurrentPowerState %d\n", 
			fProviderPower, fClamshellSlept, fCurrentPowerState);

	if (!fProviderPower) return (false);

    displayParams = OSDynamicCast(OSDictionary, copyProperty(gIODisplayParametersKey));
    if (!displayParams) return (false);

	value = fClamshellSlept ? 0 : fCurrentPowerState;

	if (fPowerUsesBrightness)
	{
		switch (value)
		{
			case 0:
				value = 0;
				break;
			case 1:
				value = fMinBrightness;
				break;
			case 2:
				value = fDisplayDims ? (fMinBrightness + 1) : fMaxBrightness;
				break;
			case 3:
				value = fMaxBrightness;
				break;
		}
        if (getIntegerRange(displayParams, gIODisplayBrightnessKey, &current, NULL, NULL)
		  && (value > current))
			value = current;
		//if(gIOFBSystemPower)
		if (value <= fMinBrightness)
			value = 0;
		ret = super::doIntegerSet(displayParams, gIODisplayBrightnessKey, value);
	}
	else
	{
		switch (value)
		{
			case 0:
				value = kIODisplayPowerStateOff;
				break;
			case 1:
				value = kIODisplayPowerStateOff;
				break;
			case 2:
				value = (fDisplayDims && (kFadeIdle == fFadeState)) ? kIODisplayPowerStateMinUsable : kIODisplayPowerStateOn;
				break;
			case 3:
				value = kIODisplayPowerStateOn;
				break;
		}
		DEBG1("B", " dsyp %d\n", value);
		ret = super::doIntegerSet(displayParams, gIODisplayPowerStateKey, value);
#if IOG_FADE2
        if (kIODisplayPowerStateOff == value) IOSleep(700);
#endif
	}

    displayParams->release();

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void AppleBacklightDisplay::_deferredEvent( OSObject * target,
        IOInterruptEventSource * evtSrc, int intCount )
{
    AppleBacklightDisplay * self = (AppleBacklightDisplay *) target;

	self->updatePowerParam();
}

IOReturn AppleBacklightDisplay::framebufferEvent( IOFramebuffer * framebuffer,
        IOIndex event, void * info )
{
    if ((kIOFBNotifyDidWake == event) && (info))
    {
	    fProviderPower = true;
//		fCurrentPowerState = kIODisplayMaxPowerState;
//		updatePowerParam();
    }
    else if (kIOFBNotifyClamshellChange == event)
    {
        if (kOSBooleanTrue == info)
        {
#if LCM_HWSLEEP
            fConnection->getFramebuffer()->changePowerStateTo(0);
#endif
            fClamshellSlept = true;
        }
        else if (fClamshellSlept)
        {
            fClamshellSlept = false;
        }
        // may be in the right power state already, but wrong brightness because
        // of the clamshell state at setPowerState time.
        if (fDeferredEvents)
            fDeferredEvents->interruptOccurred(0, 0, 0);
    }
    else if (kIOFBNotifyProbed == event)
    {
        if (fDeferredEvents)
            fDeferredEvents->interruptOccurred(0, 0, 0);
    }
    else if (kIOFBNotifyDisplayDimsChange == event)
    {
		UInt8 newValue = (info != NULL);
		if (newValue != fDisplayDims)
		{
			fDisplayDims = newValue;
			if (fDeferredEvents)
				fDeferredEvents->interruptOccurred(0, 0, 0);
		}
    }

    return (super::framebufferEvent( framebuffer, event, info ));
}


