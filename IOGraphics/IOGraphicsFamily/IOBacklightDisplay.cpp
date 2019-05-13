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

#define IOFRAMEBUFFER_PRIVATE

#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPM.h>

#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/ndrvsupport/IOMacOSVideo.h>

#include "IOGraphicsKTrace.h"
#include "GMetric.hpp"

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
    virtual IOService * probe( IOService *, SInt32 * ) APPLE_KEXT_OVERRIDE;
    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual void stop( IOService * provider ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn setPowerState( unsigned long, IOService * ) APPLE_KEXT_OVERRIDE;
    virtual unsigned long maxCapabilityForDomainState( IOPMPowerFlags ) APPLE_KEXT_OVERRIDE;
    virtual unsigned long initialPowerStateForDomainState( IOPMPowerFlags ) APPLE_KEXT_OVERRIDE;
    virtual unsigned long powerStateForDomainState( IOPMPowerFlags ) APPLE_KEXT_OVERRIDE;

    // IODisplay overrides
    virtual void initPowerManagement( IOService * ) APPLE_KEXT_OVERRIDE;
    virtual bool doIntegerSet( OSDictionary * params,
                               const OSSymbol * paramName, UInt32 value ) APPLE_KEXT_OVERRIDE;
    virtual bool doUpdate( void ) APPLE_KEXT_OVERRIDE;
    virtual void makeDisplayUsable( void ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn framebufferEvent( IOFramebuffer * framebuffer,
                                       IOIndex event, void * info ) APPLE_KEXT_OVERRIDE;

private:
	bool updatePowerParam(const int where);
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

#define RECORD_METRIC(func) \
    GMETRICFUNC(func, DBG_FUNC_NONE, \
                kGMETRICS_DOMAIN_BACKLIGHT | kGMETRICS_DOMAIN_POWER)

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
    ABL_START(probe,0,0,0);
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

        FB_START(getConnectionCount,0,__LINE__,0);
        IOItemCount fbCount = framebuffer->getConnectionCount();
        FB_END(getConnectionCount,0,__LINE__,fbCount);
        for (IOItemCount idx = 0; idx < fbCount; idx++)
        {
            FB_START(getAttributeForConnection,kConnectionFlags,__LINE__,0);
            IOReturn err = framebuffer->getAttributeForConnection(idx, kConnectionFlags, &connectFlags);
            FB_END(getAttributeForConnection,err,__LINE__,connectFlags);
            if (kIOReturnSuccess != err)
                continue;
            if (0 == (kIOConnectionBuiltIn & connectFlags))
                continue;
            FB_START(getAppleSense,0,__LINE__,0);
            IOReturn error = framebuffer->getAppleSense(idx, NULL, NULL, NULL, &displayType);
            FB_END(getAppleSense,error,__LINE__,0);
            if (kIOReturnSuccess != error)
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

    ABL_END(probe,0,0,0);
    return (ret);
}

bool AppleBacklightDisplay::start( IOService * provider )
{
    ABL_START(start,0,0,0);
    IOFramebuffer * fb;
    if (!super::start(provider))
    {
        ABL_END(start,false,0,0);
        return (false);
    }

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

    ABL_END(start,true,0,0);
    return (true);
}

void AppleBacklightDisplay::stop( IOService * provider )
{
    ABL_START(stop,0,0,0);
    if (fDeferredEvents)
    {
        getConnection()->getFramebuffer()->getControllerWorkLoop()->removeEventSource(fDeferredEvents);
        fDeferredEvents->release();
        fDeferredEvents = 0;
    }

    fadeAbort();
    if (fFadeTimer)
    {
        // Don't want to be racing any notification events when we stop.  Should never be the case,
        // but doesn't hurt to protect ourselves.
        IOFramebuffer * framebuffer = (IOFramebuffer *) getConnection()->getFramebuffer();
        if (framebuffer)
            framebuffer->fbLock();

        fFadeTimer->disable();
        if (framebuffer)
            framebuffer->getControllerWorkLoop()->removeEventSource(fFadeTimer);
        fFadeTimer->release();
	    fFadeTimer = 0;

        if (framebuffer)
            framebuffer->fbUnlock();
    }

    super::stop(provider);
    ABL_END(stop,0,0,0);
    return;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//
// This method overrides the one in IODisplay to do PowerBook-only
// power management of the display.

void AppleBacklightDisplay::initPowerManagement( IOService * provider )
{
    ABL_START(initPowerManagement,0,0,0);
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

    OSObject *paramProp = copyProperty(gIODisplayParametersKey);
    displayParams = OSDynamicCast(OSDictionary, paramProp);
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
    }
    OSSafeReleaseNULL(paramProp);

    // initialize superclass variables
    PMinit();
    // attach into the power management hierarchy
    provider->joinPMtree(this);

    // register ourselves with policy-maker (us)
    registerPowerDriver(this, (IOPMPowerState *) ourPowerStates, kIODisplayNumPowerStates);

    ABL_END(initPowerManagement,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// setPowerState
//

//#define DEBGFADE(fmt, args...) do { kprintf(fmt, ## args); } while(false)          
#define DEBGFADE(fmt, args...) do {} while(false)          

IOReturn AppleBacklightDisplay::setPowerState( unsigned long powerState, IOService * whatDevice )
{
    // Single threaded by IOServicePM design
    RECORD_METRIC(DBG_IOG_SET_POWER_STATE);
    IOG_KTRACE(DBG_IOG_SET_POWER_STATE, DBG_FUNC_NONE,
               0, powerState,
               0, DBG_IOG_SOURCE_APPLEBACKLIGHTDISPLAY,
               0, 0,
               0, 0);
    ABL_START(setPowerState,powerState,0,0);

    IOReturn ret = IOPMAckImplied;
    UInt32   fromPowerState;

    if (powerState >= kIODisplayNumPowerStates)
    {
        ABL_END(setPowerState,IOPMAckImplied,__LINE__,0);
        return (IOPMAckImplied);
    }

    IOFramebuffer * framebuffer = (IOFramebuffer *) getConnection()->getFramebuffer();

    framebuffer->fbLock();

    if (isInactive() || (fCurrentPowerState == powerState))
    {
        framebuffer->fbUnlock();
        ABL_END(setPowerState,IOPMAckImplied,__LINE__,0);
        return (IOPMAckImplied);
    }
    fromPowerState = fCurrentPowerState;
    fCurrentPowerState = static_cast<UInt32>(powerState);
	if (fCurrentPowerState) fProviderPower = true;

	OSObject * obj;
	if ((!powerState) && (obj = copyProperty(kIOHibernatePreviewActiveKey, gIOServicePlane)))
	{
		obj->release();
	}
	else
	{
#if !IOG_FADE
        updatePowerParam(0);
#else /* IOG_FADE */
        SInt32         current, min, max, steps;
		uint32_t       fadeTime;
		uint32_t       dimFade;
		bool           doFadeDown, doFadeGamma, doFadeBacklight;

		fadeTime        = 0;
	    doFadeDown      = true;
	    doFadeGamma     = false;
	    doFadeBacklight = true;

		DEBGFADE("AppleBacklight: ps [%d->%ld]\n", fromPowerState, powerState);

        OSObject *paramProp = copyProperty(gIODisplayParametersKey);
        OSDictionary *displayParams = OSDynamicCast(OSDictionary, paramProp);
        bool haveRanges = displayParams
			&& getIntegerRange(displayParams, gIODisplayBrightnessFadeKey,
                    /* value */ NULL, &min, &max)
			&& getIntegerRange(displayParams, gIODisplayBrightnessKey,
                    /* value */ &current, /* min */ NULL, /* max */ NULL)
            && current;
        OSSafeReleaseNULL(paramProp);

		if (gIOGFades && haveRanges && !fFadeAbort)
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
					 doFadeBacklight = (dimFade != static_cast<uint32_t>(max));
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
			if (static_cast<uint32_t>(steps) > fFadeStateFadeDelta)
                steps = static_cast<SInt32>(fFadeStateFadeDelta);

			fFadeDown      = doFadeDown;
			fFadeGamma     = doFadeGamma;
			fFadeBacklight = doFadeBacklight;

			DEBGFADE("AppleBacklight:  %d -> %d\n", fFadeStateFadeMin, fFadeStateFadeDelta);

			fFadeState      = 0;
			if (!fFadeDown) updatePowerParam(1);

			fFadeDeadline = mach_absolute_time();
			fadeTime     /= steps;
			fFadeStateEnd = (steps - 1);
            // <rdar://problem/30194186> PanicTracer: 486 panics at com.apple.iokit.IOGraphicsFamily : AppleBacklightDisplay::fadeWork :: com.apple.iokit.IOGraphicsFamily : AppleBacklightDisplay::setPowerState
            //   Under hard to determine circumstances the fFadeStateEnd can be
            //   0. The correct fix is to not even start fading in that
            //   circumstance but that is too dangerous for a narrow bug fix.
            //   Just turn off all fading and let the engine run to completion.
            if (!fFadeStateEnd)
                fFadeBacklight = fFadeGamma = false;
		    if (framebuffer->isWakingFromHibernateGfxOn())
                fFadeState = fFadeStateEnd;
			clock_interval_to_absolutetime_interval(fadeTime, kMicrosecondScale, &fFadeInterval);
			fadeWork(fFadeTimer);
			if (fFadeDown)  ret = 20 * 1000 * 1000;
		}
		else
		{
			updatePowerParam(2);
			fFadeAbort = false;
		}
#endif
	}

    framebuffer->fbUnlock();

    ABL_END(setPowerState,ret,0,0);
    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void AppleBacklightDisplay::fadeAbort(void)
{
    // On FBcontroller workloop or called at start time
    ABL_START(fadeAbort,0,0,0);
	if (kFadeIdle == fFadeState)
    {
        ABL_END(fadeAbort,0,__LINE__,0);
        return;
    }

    OSObject *paramProp = copyProperty(gIODisplayParametersKey);
    OSDictionary *displayParams = OSDynamicCast(OSDictionary, paramProp);
    if (!displayParams) goto bail;

    // abort
    DEBGFADE("AppleBacklight: fadeAbort\n");
    doIntegerSet(displayParams, gIODisplayBrightnessFadeKey, 0);
    if (fFadeGamma)
    {
        doIntegerSet(displayParams, gIODisplayGammaScaleKey, 65536);
        doIntegerSet(displayParams, gIODisplayParametersFlushKey, 0);
    }
    if (fFadeDown)
    {
        RECORD_METRIC(DBG_IOG_ACK_POWER_STATE);
        IOG_KTRACE(DBG_IOG_ACK_POWER_STATE, DBG_FUNC_NONE,
                   0, DBG_IOG_SOURCE_IODISPLAY,
                   0, 0,
                   0, 0,
                   0, 0);
        acknowledgeSetPowerState();
    }
    fFadeState = kFadeIdle;

bail:
    OSSafeReleaseNULL(paramProp);
    ABL_END(fadeAbort,0,0,0);
}

void AppleBacklightDisplay::fadeWork(IOTimerEventSource * sender)
{
    // On FBController workloop

    ABL_START(fadeWork,0,0,0);
	SInt32 fade, gamma, point;
    
	DEBGFADE("AppleBacklight: fadeWork(fFadeStateEnd %d, fFadeState %d, %d, fFadeStateEnd %d)\n", 
			fFadeStateEnd, fFadeState & 0xffff, fFadeState >> 24, fFadeStateEnd);

	if (kFadeIdle == fFadeState)
    {
        ABL_END(fadeWork,0,__LINE__,0);
        return;
    }

    OSObject *paramProp = copyProperty(gIODisplayParametersKey);
    OSDictionary *displayParams = OSDynamicCast(OSDictionary, paramProp);
	if (!displayParams) goto bail;

    fFadeAbort = (fFadeDown && !fDisplayPMVars->displayIdle);

	if (fFadeAbort) fadeAbort();
	else if (fFadeState <= fFadeStateEnd)
    {
        if ((kIOWSAA_DeferStart != fWSAADeferState) || fFadeDown)
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
                if (fFadeDown) updatePowerParam(3);
                fFadeState = kFadePostDelay;
                clock_interval_to_absolutetime_interval(500, kMillisecondScale, &fFadeInterval);
            }
        }
	}
	else if (kFadePostDelay == fFadeState)
	{
		fFadeState = kFadeIdle;
	}

	if (kFadeIdle == fFadeState)
	{
	    DEBGFADE("AppleBacklight: fadeWork ack\n");
        if (fFadeDown)
        {
            RECORD_METRIC(DBG_IOG_ACK_POWER_STATE);
            IOG_KTRACE(DBG_IOG_ACK_POWER_STATE, DBG_FUNC_NONE,
                       0, DBG_IOG_SOURCE_IODISPLAY,
                       0, 0,
                       0, 0,
                       0, 0);
            acknowledgeSetPowerState();
        }
	}
	else
	{
		ADD_ABSOLUTETIME(&fFadeDeadline, &fFadeInterval);
		fFadeTimer->wakeAtTime(fFadeDeadline);
	}

bail:
    OSSafeReleaseNULL(paramProp);

    ABL_END(fadeWork,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void AppleBacklightDisplay::makeDisplayUsable( void )
{
    ABL_START(makeDisplayUsable,0,0,0);
    if (kIODisplayMaxPowerState == fCurrentPowerState)
        setPowerState(fCurrentPowerState, this);
    super::makeDisplayUsable();
    ABL_END(makeDisplayUsable,0,0,0);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// This simple device needs only power.  If the power domain is supplying
// power, the display can go to its highest state.  If there is no power
// it can only be in its lowest state, which is off.

unsigned long AppleBacklightDisplay::maxCapabilityForDomainState( IOPMPowerFlags domainState )
{
    ABL_START(maxCapabilityForDomainState,domainState,0,0);
    unsigned long   ret = 0;
    if (domainState & IOPMPowerOn)
        ret = (kIODisplayMaxPowerState);
    ABL_END(maxCapabilityForDomainState,ret,0,0);
    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.

unsigned long AppleBacklightDisplay::initialPowerStateForDomainState( IOPMPowerFlags domainState )
{
    ABL_START(initialPowerStateForDomainState,domainState,0,0);
    unsigned long   ret = 0;
    if (domainState & IOPMPowerOn)
        ret = (kIODisplayMaxPowerState);
    ABL_END(initialPowerStateForDomainState,ret,0,0);
    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// powerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.

unsigned long AppleBacklightDisplay::powerStateForDomainState( IOPMPowerFlags domainState )
{
    ABL_START(powerStateForDomainState,domainState,0,0);
    unsigned long   ret = 0;
    if (domainState & IOPMPowerOn)
        ret = (kIODisplayMaxPowerState);
    ABL_END(powerStateForDomainState,ret,0,0);
    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Alter the backlight brightness by user request.

bool AppleBacklightDisplay::doIntegerSet( OSDictionary * params,
                                       const OSSymbol * paramName, UInt32 value )
{
    ABL_START(doIntegerSet,value,0,0);
    if ((paramName == gIODisplayParametersCommitKey)
        && (kIODisplayMaxPowerState != fCurrentPowerState))
    {
        ABL_END(doIntegerSet,true,0,0);
        return (true);
    }

    if (paramName == gIODisplayBrightnessKey)
    {
#if 0
        fCurrentUserBrightness = value;
        getPMRootDomain()->setProperty(kIOBacklightUserBrightnessKey, fCurrentUserBrightness, 32);
#endif
        if (fPowerUsesBrightness && fClamshellSlept)
			value = 0;
    }

    bool b = super::doIntegerSet(params, paramName, value);
    ABL_END(doIntegerSet,b,0,0);
	return (b);
}

bool AppleBacklightDisplay::doUpdate( void )
{
    ABL_START(doUpdate,0,0,0);
    bool ok;

    ok = super::doUpdate();

#if 0
    OSObject *paramProp = copyProperty(gIODisplayParametersKey);
    OSDictionary *displayParams = OSDynamicCast(OSDictionary, paramProp);
    if (fDisplayPMVars && displayParams);
    {
        setParameter(displayParams, gIODisplayBrightnessKey, fCurrentUserBrightness);	
    }

    OSSafeReleaseNULL(paramProp);
#endif

    ABL_END(doUpdate,ok,0,0);
    return (ok);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleBacklightDisplay::updatePowerParam(const int where)
{
    ABL_START(updatePowerParam,0,0,0);
	SInt32 		   value, current;
	bool	       ret;

	DEBG1("B", " fProviderPower %d, fClamshellSlept %d, fCurrentPowerState %d\n", 
			fProviderPower, fClamshellSlept, fCurrentPowerState);

	if (!fProviderPower)
    {
        ABL_END(updatePowerParam,false,__LINE__,0);
        return (false);
    }
	if (!fCurrentPowerState) fProviderPower = false;

    OSObject *paramProp = copyProperty(gIODisplayParametersKey);
    OSDictionary *displayParams = OSDynamicCast(OSDictionary, paramProp);
    if (!displayParams) {
        OSSafeReleaseNULL(paramProp);
        ABL_END(updatePowerParam,false,__LINE__,0);
        return (false);
    }

	const int targetPower = fClamshellSlept ? 0 : fCurrentPowerState;
    assert(0 <= fCurrentPowerState
             && fCurrentPowerState < kIODisplayNumPowerStates);

	if (fPowerUsesBrightness)
	{
        // TODO(gvdl): AppleBacklightDisplay is used only for built in displays
        // in iMacs and laptops.  fPowerUserBrightness is only set if the
        // IODisplayParamater[dsyp(gIODisplayPowerModeKey)] is not found.
        // I have looked at j130, j145, j80 and j78 machines. They all support
        // this display parameter.  Further research will be required before we
        // can delete this code as dead, though.
		switch (targetPower)
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
		switch (targetPower)
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
        const uint64_t arg1 = GPACKUINT8T(0, /* version */ 0) // future proofing
                            | GPACKUINT8T(1, value)
                            | GPACKUINT8T(2, targetPower)
                            | GPACKUINT8T(3, where);
        IOG_KTRACE_NT(DBG_IOG_DISPLAY_POWER, DBG_FUNC_NONE, arg1, 0, 0, 0);
		ret = super::doIntegerSet(displayParams, gIODisplayPowerStateKey, value);
#if IOG_FADE2
        if (kIODisplayPowerStateOff == value) IOSleep(700);
#endif
	}

    displayParams->release();

    ABL_END(updatePowerParam,ret,0,0);
    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void AppleBacklightDisplay::_deferredEvent( OSObject * target,
        IOInterruptEventSource * evtSrc, int intCount )
{
    ABL_START(_deferredEvent,intCount,0,0);
    AppleBacklightDisplay * abd = (AppleBacklightDisplay *) target;

	abd->updatePowerParam(4);
    ABL_END(_deferredEvent,0,0,0);
}

IOReturn AppleBacklightDisplay::framebufferEvent( IOFramebuffer * framebuffer,
        IOIndex event, void * info )
{
    ABL_START(framebufferEvent,event,0,0);
    UInt8 newValue;

    switch(event)
    {
        case kIOFBNotifyDidWake:
            if (info)
            {
                fProviderPower = true;
                //		fCurrentPowerState = kIODisplayMaxPowerState;
                //		updatePowerParam();
            }
            break;
        case kIOFBNotifyClamshellChange:
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
            break;
        case kIOFBNotifyProbed:
            if (fDeferredEvents)
                fDeferredEvents->interruptOccurred(0, 0, 0);
            break;
        case kIOFBNotifyDisplayDimsChange:
            newValue = (info != NULL);
            if (newValue != fDisplayDims)
            {
                fDisplayDims = newValue;
                if (fDeferredEvents)
                    fDeferredEvents->interruptOccurred(0, 0, 0);
            }
            break;
        case kIOFBNotifyWSAAWillEnterDefer:
            DEBGFADE("AppleBacklight: disable fadeWork\n");
            /* <rdar://problem/28283410> J80G ; EVT Sleep-Wake Bright Flash */
            // If we haven't started a fade as indicated by fFadeState == kFadeIdle and the WSAA state is the deferred start state,
            // disable the fade timer, else too bad, roll on with fade.  This means we started a fade prior to the deferred enter state notification and have no choice but to complete it.
            if (fFadeTimer && (kFadeIdle == fFadeState))
                fFadeTimer->disable();
            break;
        case kIOFBNotifyWSAADidExitDefer:
            DEBGFADE("AppleBacklight: enable fadeWork\n");
            if (fFadeTimer)
                fFadeTimer->enable();
            break;
        default:
            // defer to super
            break;
    }

    IOReturn err = super::framebufferEvent( framebuffer, event, info );
    ABL_END(framebufferEvent,err,0,0);
    return (err);
}


