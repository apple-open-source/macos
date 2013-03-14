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
#include <libkern/c++/OSContainers.h>
#include <libkern/OSByteOrder.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IODeviceTreeSupport.h>

#define IOFRAMEBUFFER_PRIVATE
#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/i2c/IOI2CInterface.h>
#if defined(__ppc__) && !defined(OSTYPES_K64_REV)
#include <IOKit/i2c/PPCI2CInterface.h>
#endif
#include <IOKit/acpi/IOACPIPlatformExpert.h>

#include "IOFramebufferUserClient.h"
#include "IODisplayWrangler.h"
#include "IOFramebufferReallyPrivate.h"
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>

#include <string.h>
#include <IOKit/assert.h>
#include <sys/kdebug.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef VERSION_MAJOR
#error no VERSION_MAJOR
#endif

#define SINGLE_THREAD	0
#define TIME_LOGS		RLOG1
#define TIME_CURSOR		RLOG1
#define ASYNC_GAMMA		1

#define GAMMA_ADJ		0

#define DOANIO        0
#define VRAM_SAVE     1
#define VRAM_COMPRESS 1

enum { kIOFBVRAMCompressSpeed = 0 };
enum { kVBLThrottleTimeMS     = 5000 };
enum { kInitFBTimeoutNS = 1000000000ULL };

enum { k2xDPI = (150*10) };

//#define AUTO_COLOR_MODE		kIODisplayColorModeRGB
#define AUTO_COLOR_MODE		kIODisplayColorModeYCbCr444


#if defined(__i386__) || defined(__x86_64__)
enum { kIOFBMapCacheMode = kIOMapWriteCombineCache }; 
#else
enum { kIOFBMapCacheMode = kIOMapInhibitCache };
#endif

#if VRAM_COMPRESS
#include "bmcompress.h"
#endif

#if DOANIO
#include <sys/uio.h>
#include <sys/conf.h>
#endif

enum {
    kIOFBClamshellProbeDelayMS = 1*1000
};
enum {
    kIOFBClamshellEnableDelayMS = 5*1000
};

enum
{
	// all seconds:
	kSystemWillSleepTimeout = 90,   // 90
	kServerAckTimeout       = 25,	// 25
	kPowerStateTimeout      = 45,
	kDarkWokeTimeout        = 5,
};

enum
{
	kIOFBEventCaptureSetting     = 0x00000001,
    kIOFBEventDisplayDimsSetting = 0x00000002,
	kIOFBEventReadClamshell	 	 = 0x00000004,
    kIOFBEventResetClamshell	 = 0x00000008,
    kIOFBEventEnableClamshell    = 0x00000010,
    kIOFBEventProbeAll			 = 0x00000020,
    kIOFBEventDisplaysPowerState = 0x00000040
};

enum
{
    fg    = 1,
    bg    = 2,
    fgOff = 3,
    bgOff = 4,
};

#if RLOG1
static const char * processConnectChangeModeNames[] =
	{ "", "fg", "bg", "fgOff", "bgOff" };
#endif

#define CHAR(c)    ((c) ? ((char) (c)) : '0')
#define FEAT(f)    CHAR(f>>24), CHAR(f>>16), CHAR(f>>8), CHAR(f>>0)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static class IOGraphicsWorkLoop * gIOFBSystemWorkLoop;
static struct IOFBController *    gIOFBAllControllers;
static struct IOFBController *    gIOFBLastController;

static OSArray *            gAllFramebuffers;
static OSArray *            gRunawayFramebuffers;
static IOWorkLoop *         gIOFBHIDWorkLoop;
static IOTimerEventSource * gIOFBDelayedPrefsEvent;
static IOTimerEventSource * gIOFBServerAckTimer;
static IOTimerEventSource * gIOFBDarkWakeTimer;
static IONotifier *         gIOFBRootNotifier;
static IONotifier *         gIOFBClamshellNotify;
static IONotifier *         gIOFBGCNotifier;
static IOInterruptEventSource * gIOFBWorkES;
static volatile UInt32      gIOFBGlobalEvents;
static IORegistryEntry *    gChosenEntry;
static IOService *          gIOFBSystemPowerAckTo;
static void *               gIOFBSystemPowerAckRef;
static IOService *          gIOFBSystemPowerMuxAckTo;
static uint32_t             gIOFBSystemPowerMuxAckRef;
static UInt32               gIOFBLastMuxMessage = kIOMessageSystemHasPoweredOn;
static bool					gIOFBSwitching;
bool                        gIOFBSystemPower = true;
bool						gIOFBSystemDark;
static bool					gIOFBWSState = true;
static bool					gIOFBPostWakeNeeded;
static bool					gIOFBProbeCaptured;
static uint32_t             gIOFBCaptureState;
bool                        gIOGraphicsSystemPower = true;
static thread_call_t        gIOFBClamshellCallout;
static SInt32               gIOFBDisplayCount;
static SInt32               gIOFBBacklightDisplayCount;
static IOOptionBits         gIOFBClamshellState;
static IOFramebuffer *      gIOFBConsoleFramebuffer;
static bool                 gIOFBDesktopModeAllowed = true;
IOOptionBits                gIOFBCurrentClamshellState;
static IOOptionBits         gIOFBLastClamshellState;
int32_t                     gIOFBHaveBacklight = -1;
static IOOptionBits         gIOFBLastReadClamshellState;
const OSSymbol *            gIOFBGetSensorValueKey;
const OSSymbol *            gIOFramebufferKey;
const OSSymbol *            gIOFBRotateKey;
const OSSymbol *            gIOFBStartupModeTimingKey;
const OSSymbol *            gIOFBPMSettingDisplaySleepUsesDimKey;
const OSSymbol *            gIOFBConfigKey;
const OSSymbol *            gIOFBModesKey;
const OSSymbol *            gIOFBModeIDKey;
const OSSymbol *            gIOFBModeDMKey;
static OSDictionary *       gIOFBPrefs;
static OSDictionary *       gIOFBPrefsParameters;
static OSDictionary *       gIOFBIgnoreParameters;
static OSSerializer *       gIOFBPrefsSerializer;
static IOService *          gIOGraphicsControl;
static OSObject *           gIOResourcesAppleClamshellState;
static AbsoluteTime         gIOFBNextProbeAllTime;
static AbsoluteTime         gIOFBMaxVBLDelta;
OSData *                    gIOFBZero32Data;
OSData *                    gIOFBOne32Data;
uint32_t             		gIOFBGrayValue = kIOFBBootGrayValue;
OSData *                    gIOFBGray32Data;
static const OSSymbol *     gIOGraphicsPrefsVersionKey;
static OSNumber *           gIOGraphicsPrefsVersionValue;
static uint8_t				gIOFBLidOpenMode;
static uint8_t				gIOFBVBLThrottle;
static uint8_t				gIOFBVBLDrift;
uint32_t					gIOGDebugFlags;
uint32_t					gIOGNotifyTO;

#define kIOFBGetSensorValueKey  "getSensorValue"

enum { kIOFBDefaultScalerUnderscan = 0*kIOFBScalerUnderscan };

// console clut
extern UInt8 appleClut8[256 * 3];

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum { kIOFBControllerMaxFBs = 32 };

struct IOFBController
{
    IOFBController *            nextController;
    IOFramebuffer  *            fbs[kIOFBControllerMaxFBs + 1];
    class IOGraphicsWorkLoop *  wl;
	IOInterruptEventSource *    workES;
    IOService      *            device;
    const char     *            name;
	AbsoluteTime				initTime;

	thread_t					powerThread;

	uint32_t					vendorID;
    uint32_t                    maxFB;
    uint32_t                    onlineMask;

    int32_t                     connectChange;
    int32_t                     lastForceRetrain;
    int32_t                     lastMessagedChange;
    int32_t                     lastFinishedChange;
	int32_t						postWakeChange;

    uint8_t                     mute;

    uint8_t                     wsWait;

	uint8_t						needsWork;
	uint8_t						didWork;
	uint8_t						asyncWork;
	uint8_t						pendingMuxPowerChange;

	uint32_t					state;
	uint32_t					aliasID;
};

struct IOFBInterruptRegister
{
    IOFBInterruptProc           handler;
    OSObject *                  target;
    void *                      ref;
    UInt32						state;
};
enum
{
	kIOFBMCCSInterruptRegister = 0,
	kIOFBNumInterruptRegister  = 1
};

struct IOFramebufferPrivate
{
    IOFBController *            controller;
    IODisplay *					display;
    IOFramebuffer *				nextMirror;
	uint32_t					displayOptions;
    uint32_t					controllerIndex;
    IOGSize                     maxWaitCursorSize;
    UInt32                      numCursorFrames;
    uint32_t                    cursorBytesPerPixel;
    UInt8 *                     cursorFlags;
    volatile unsigned char **   cursorImages;
    volatile unsigned char **   cursorMasks;
    IOMemoryDescriptor *        saveBitsMD[kIOPreviewImageCount];

	IOGBounds					screenBounds[2];			// phys & virtual bounds

    class IOFramebufferParameterHandler * paramHandler;

    void *              		vblInterrupt;
    void *              		connectInterrupt;
	IOTimerEventSource *        vblUpdateTimer;
	IOTimerEventSource *        deferredCLUTSetTimerEvent;
	IOInterruptEventSource *    deferredVBLDisableEvent;
    uint64_t					actualVBLCount;
	OSObject *                  displayAttributes;

	IOFBInterruptRegister		interruptRegisters[kIOFBNumInterruptRegister];
	
    OSArray *                   cursorAttributes;
    IOFBCursorControlAttribute  cursorControl;
    IOInterruptEventSource *    cursorThread;
    IOOptionBits                cursorToDo;
    UInt32                      framePending;
    SInt32                      xPending;
    SInt32                      yPending;
    IOGPoint                    cursorHotSpotAdjust[2];
    IOGPoint                    lastHotSpot;
    void *                      waitVBLEvent;

    IOByteCount                 gammaHeaderSize;
    UInt32                      desiredGammaDataWidth;
    UInt32                      desiredGammaDataCount;

    IOInterruptEventSource *    deferredCLUTSetEvent;
    IOInterruptEventSource *    deferredSpeedChangeEvent;
    IOTimerEventSource *        delayedConnectInterrupt;
    UInt32                      delayedConnectTime;

    IOTimerEventSource *        dpInterruptES;
    void *                      dpInterruptRef;
    UInt32                      dpInterrupDelayTime;

	const OSSymbol *			displayPrefKey;

    IOByteCount                 gammaDataLen;
    UInt8 *                     gammaData;
    UInt32                      gammaChannelCount;
    UInt32                      gammaDataCount;
    UInt32                      gammaDataWidth;

    IOByteCount                 rawGammaDataLen;
    UInt8 *                     rawGammaData;
    UInt32                      rawGammaChannelCount;
    UInt32                      rawGammaDataCount;
    UInt32                      rawGammaDataWidth;

    IOByteCount                 clutDataLen;
    UInt8 *                     clutData;
    UInt32                      clutIndex;
    UInt32                      clutOptions;

    uint32_t                    framebufferWidth;
    uint32_t                    framebufferHeight;
    uint32_t                    consoleDepth;
    uint32_t                    saveLength;
    void *                      saveFramebuffer;
	UInt8						saveGammaPending;

	UInt8						vblThrottle;
	UInt8						vblEnabled;
    UInt8                       gammaNeedSet;
    UInt8                       gammaScaleChange;
    UInt8                       scaledMode;
    UInt8                       visiblePending;
    UInt8                       testingCursor;
    UInt8                       index;
	UInt8						gammaSet;
    UInt8                       cursorSlept;
    UInt8                       cursorPanning;
    UInt8                       pendingSpeedChange;

    UInt8                       lli2c;
    UInt8                       cursorClutDependent;
    UInt8                       allowSpeedChanges;
    UInt8                       dimDisable;

    UInt8                       enableScalerUnderscan;
    UInt8                       userSetTransform;
    UInt8                       closed;
    UInt8                       online;
	UInt8						displaysOnline;
    UInt8                       lastNotifyOnline;
    UInt8						modeUpdate;
    UInt8                       dpInterrupts;
    SInt8                       dpSupported;
    UInt8                       dpDongle;
    UInt8                       dpDongleSinkCount;
    UInt8                       dpBusID;
	UInt8						colorModesAllowed;
	UInt8						needsInit;
	uint32_t					uiScale;

	uint32_t					colorModesSupported;

    UInt64                      transform;
    UInt64                      selectedTransform;
    UInt32                      reducedSpeed;
    IOService *                 temperatureSensor;
    IOI2CBusTiming              defaultI2CTiming;

    uintptr_t                   gammaScale[3];

    IOPixelInformation          pixelInfo;
	IOTimingInformation 		timingInfo;
    IODisplayModeID             offlineMode;
    IODisplayModeID             aliasMode;
    IODisplayModeID             matchedMode;
    IODisplayModeID             setupMode;
    IOIndex                     currentDepth;

    int32_t                     lastProcessedChange;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define GetShmem(instance)      ((StdFBShmem_t *)(instance->priv))

#define KICK_CURSOR(thread)     \
            thread->interruptOccurred(0, 0, 0);

#define CLEARSEMA(shmem, inst)                          \
        if( inst->__private->cursorToDo ) {             \
            KICK_CURSOR(inst->__private->cursorThread); \
        }                                               \
        OSSpinLockUnlock(&shmem->cursorSema)

#define SETSEMA(shmem)          \
        if (!OSSpinLockTry(&shmem->cursorSema)) return;
#define TOUCHBOUNDS(one, two) \
        (((one.minx < two.maxx) && (two.minx < one.maxx)) && \
        ((one.miny < two.maxy) && (two.miny < one.maxy)))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOGraphicsWorkLoop : public IOWorkLoop
{
    OSDeclareDefaultStructors(IOGraphicsWorkLoop)
public:
	typedef void GateFunction(IOWorkLoop * wl, OSObject * obj, void * reference, bool gate);

	IOLock *        gateMutex;
	thread_t		gateThread;
	uint32_t		gateCount;

	IOOptionBits   options;
	GateFunction * func;
	OSObject *     obj;
	void *         reference;

    static IOGraphicsWorkLoop * workLoop(IOOptionBits options = 0, 
  										 GateFunction * func = NULL, 
  										 OSObject * obj = NULL, void * reference = NULL);
	virtual bool init();
	virtual void free();

	virtual void signalWorkAvailable() { IOWorkLoop::signalWorkAvailable(); }
    virtual bool inGate() const;
    virtual void closeGate();
    virtual void openGate();
    virtual bool tryCloseGate();
    virtual int  sleepGate(void *event, UInt32 interuptibleType);
    virtual int  sleepGate(void *event, AbsoluteTime deadline, UInt32 interuptibleType);
    virtual void wakeupGate(void *event, bool oneThread);
};

OSDefineMetaClassAndStructors(IOGraphicsWorkLoop, IOWorkLoop)

__private_extern__ "C" kern_return_t IOGraphicsFamilyModuleStart(kmod_info_t *ki, void *data)
{
	gIOFBSystemWorkLoop = IOGraphicsWorkLoop::workLoop(0, NULL, NULL, NULL);
	return (kIOReturnSuccess);
}

IOGraphicsWorkLoop * IOGraphicsWorkLoop::workLoop(IOOptionBits options, 
  										 GateFunction * func, OSObject * obj, void * reference)
{
    IOGraphicsWorkLoop *me = new IOGraphicsWorkLoop;

    if (!me)
		return (NULL);

	me->options   = options;
	me->func      = func;
	me->obj       = obj;
	me->reference = reference;
	if (!me->init())
	{
		me->release();
		me = NULL;
	}

    return (me);
}

bool IOGraphicsWorkLoop::init()
{
	bool ok;

	gateMutex = IOLockAlloc();
    if (!gateMutex)	return (false);

	ok = IOWorkLoop::init();

	if (gateLock)
	{
	    IORecursiveLockFree(gateLock);
	    gateLock = NULL;
	}

	return (ok);
}

void IOGraphicsWorkLoop::free()
{
	if (gateMutex && !workThread)
    {
		IOLockFree(gateMutex);
		gateMutex = NULL;
	}
	IOWorkLoop::free();
}

bool IOGraphicsWorkLoop::inGate() const
{
    return (gateThread == IOThreadSelf());
}

void IOGraphicsWorkLoop::closeGate()
{
	if (gateThread == IOThreadSelf())
	{
		if (!gateCount) panic("gateCount");
		gateCount++;
	}
	else
	{
#if 0
			AbsoluteTime startTime, endTime;
			uint64_t nsec;
			AbsoluteTime_to_scalar(&startTime) = mach_absolute_time();
#endif
        IOLockLock(gateMutex);
#if 0
			AbsoluteTime_to_scalar(&endTime) = mach_absolute_time();
			SUB_ABSOLUTETIME(&endTime, &startTime);
			absolutetime_to_nanoseconds(endTime, &nsec);
			nsec /= 1000000ULL;
			if (nsec >= 50)
				OSReportWithBacktrace("wsloow %qd ms\n", nsec);
#endif
        assert (gateThread == 0);
        assert (gateCount == 0);
		if (gateThread) panic("gateThread");
		if (gateCount)  panic("gateCount");

        gateThread = IOThreadSelf();
        gateCount  = 1;
		if (func) (*func)(this, obj, reference, true);
	}
}

void IOGraphicsWorkLoop::openGate()
{
    assert (gateThread == IOThreadSelf());
    if (gateThread != IOThreadSelf()) panic("gateThread");
    if (1 == gateCount)
    {
		if (func) (*func)(this, obj, reference, false);
		if (gateThread != IOThreadSelf()) panic("gateThread");
		if (gateCount != 1)  panic("gateCount");
        gateThread = NULL;
        gateCount = 0;
        IOLockUnlock(gateMutex);
        return;
    }
	gateCount--;
}

bool IOGraphicsWorkLoop::tryCloseGate()
{
	bool gotit = true;

	if (gateThread == IOThreadSelf()) gateCount++;
	else
	{
        if (!IOLockTryLock(gateMutex)) gotit = false;
        else
        {
			assert (gateThread == 0);
			assert (gateCount == 0);
			if (gateThread) panic("gateThread");
			if (gateCount)  panic("gateCount");
			gateThread = IOThreadSelf();
			gateCount  = 1;
			if (func) (*func)(this, obj, reference, true);
		}
	}
	return (gotit);
}

int IOGraphicsWorkLoop::sleepGate(void *event, UInt32 interuptibleType)
{
	int      result;
	uint32_t count;

    assert(gateThread == IOThreadSelf());
    if (gateThread != IOThreadSelf()) panic("gateThread");

	count     = gateCount;
	gateCount = 0;
	if (func) (*func)(this, obj, reference, false);

	gateThread = NULL;
	result = IOLockSleep(gateMutex, event, interuptibleType);
	
	assert (gateThread == 0);
	assert (gateCount == 0);
	if (gateThread) panic("gateThread");
	if (gateCount)  panic("gateCount");
	gateThread = IOThreadSelf();
	gateCount  = count;

	if (func) (*func)(this, obj, reference, true);
    return (result);
}

int IOGraphicsWorkLoop::sleepGate(void *event, AbsoluteTime deadline, UInt32 interuptibleType)
{
	int      result;
	uint32_t count;

    assert(gateThread == IOThreadSelf());

	count     = gateCount;
	gateCount = 0;
	if (func) (*func)(this, obj, reference, false);

	gateThread = NULL;
	result = IOLockSleepDeadline(gateMutex, event, deadline, interuptibleType);
	
	assert (gateThread == 0);
	assert (gateCount == 0);
	gateThread = IOThreadSelf();
	gateCount  = count;

	if (func) (*func)(this, obj, reference, true);

    return (result);
}

void IOGraphicsWorkLoop::wakeupGate(void *event, bool oneThread)
{
	return (IOLockWakeup(gateMutex, event, oneThread));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define SYSLOCK()         \
    gIOFBSystemWorkLoop->closeGate()
#define SYSUNLOCK()       \
    gIOFBSystemWorkLoop->openGate()

#define FCLOCK(fc)        \
    fc->wl->closeGate()
#define FCUNLOCK(fc)      \
    fc->wl->openGate()

#define FBWL(fb)          \
    fb->__private->controller->wl

#define _FBLOCK(fb)        	\
    FBWL(fb)->closeGate()

#define FBUNLOCK(fb)       \
    FBWL(fb)->openGate()	\

#if TIME_LOGS

static void TIMELOCK(IOGraphicsWorkLoop * wl, const char * name, const char * fn)
{
    AbsoluteTime startTime, endTime;
    uint64_t nsec;

	AbsoluteTime_to_scalar(&startTime) = mach_absolute_time();

    wl->closeGate();

    AbsoluteTime_to_scalar(&endTime) = mach_absolute_time();
    SUB_ABSOLUTETIME(&endTime, &startTime);
    absolutetime_to_nanoseconds(endTime, &nsec);
    nsec /= 1000000ULL;
    if (nsec >= 5)
        IOLog("%s: %s: sloow %qd ms\n", name, fn, nsec);
}
#define FBLOCK(fb) TIMELOCK(FBWL(fb), fb->thisName, __FUNCTION__)


#define TIMESTART()									\
{													\
    AbsoluteTime startTime, endTime;				\
    uint64_t nsec;									\
	AbsoluteTime_to_scalar(&startTime) = mach_absolute_time();

#define TIMEEND(name, fmt, args...)								\
    AbsoluteTime_to_scalar(&endTime) = mach_absolute_time();    \
    absolutetime_to_nanoseconds(endTime, &nsec);                  \
    IOLog("%08d [%s]: ", (uint32_t) (nsec / 1000000ULL), name); \
    SUB_ABSOLUTETIME(&endTime, &startTime);			\
    absolutetime_to_nanoseconds(endTime, &nsec);	\
    nsec /= 1000000ULL;								\
    IOLog(fmt, ## args , nsec);						\
}

#else	/* !TIME_LOGS */

#define FBLOCK(fb) 					_FBLOCK(fb)
#define TIMELOCK(wl, name, fn)		wl->closeGate()
#define TIMESTART()
#define TIMEEND(name, fmt, args...)

#endif	

#if TIME_CURSOR

#define CURSORLOCK(fb) 													\
    if (!cursorBlitProc)												\
        return;															\
    AbsoluteTime startTime, endTime;									\
    uint64_t nsec;														\
	AbsoluteTime_to_scalar(&startTime) = mach_absolute_time();			\
																		\
    StdFBShmem_t *shmem = GetShmem(fb);									\
    bool __checkTime = (2 == fb->getPowerState());						\
    SETSEMA(shmem);														\
    FBLOCK(this);														\
																		\
    AbsoluteTime_to_scalar(&endTime) = mach_absolute_time();			\
    SUB_ABSOLUTETIME(&endTime, &startTime);								\
    absolutetime_to_nanoseconds(endTime, &nsec);						\
    nsec /= 1000000ULL;													\
    if (__checkTime && (nsec >= 20))										\
        IOLog("%s: %s: cursor lock stall %qd ms\n", fb->thisName, __FUNCTION__, nsec);

#else	/* TIME_CURSOR */

#define CURSORLOCK(fb) 													\
    if (!cursorBlitProc)												\
        return;															\
    StdFBShmem_t *shmem = GetShmem(fb);									\
    SETSEMA(shmem);														\
    FBLOCK(this);														\

#endif	/* TIME_CURSOR */

#define CURSORUNLOCK(fb)	\
    FBUNLOCK(fb);			\
    CLEARSEMA(shmem, fb);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define extEntry(allowOffline)  		_extEntry(false, allowOffline, __FUNCTION__)
#define extExit(result)  				_extExit (false, result, __FUNCTION__)
#define extEntrySys(allowOffline)  		_extEntry(true,  allowOffline, __FUNCTION__)
#define extExitSys(result)  			_extExit (true,  result, __FUNCTION__)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferParameterHandler : public IODisplayParameterHandler
{
    OSDeclareDefaultStructors(IOFramebufferParameterHandler)

    OSDictionary *      fDisplayParams;
    IOFramebuffer *     fFramebuffer;
    IODisplay *         fDisplay;

public:
    static IOFramebufferParameterHandler * withFramebuffer( IOFramebuffer * framebuffer );
    virtual void free();

    virtual bool setDisplay( IODisplay * display );
    virtual bool doIntegerSet( OSDictionary * params,
                               const OSSymbol * paramName, UInt32 value );
    virtual bool doDataSet( const OSSymbol * paramName, OSData * value );
    virtual bool doUpdate( void );

    void displayModeChange( void );
};

class IOFramebufferSensor : public IOService
{
    OSDeclareDefaultStructors(IOFramebufferSensor)

    IOFramebuffer *     fFramebuffer;

public:
    static IOFramebufferSensor * withFramebuffer( IOFramebuffer * framebuffer );
    virtual void free();

    virtual IOReturn callPlatformFunction( const OSSymbol * functionName,
                                           bool waitForFunction,
                                           void *param1, void *param2,
                                           void *param3, void *param4 );

    virtual IOReturn callPlatformFunction( const char * functionName,
                                           bool waitForFunction,
                                           void *param1, void *param2,
                                           void *param3, void *param4 );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferI2CInterface : public IOI2CInterface
{
    OSDeclareDefaultStructors(IOFramebufferI2CInterface)

    IOFramebuffer *     fFramebuffer;
    SInt32              fBusID;
    UInt32              fSupportedTypes;
    UInt32              fSupportedCommFlags;
    
public:
    virtual bool start( IOService * provider );
    virtual IOReturn startIO( IOI2CRequest * request );

    static IOFramebufferI2CInterface * withFramebuffer( IOFramebuffer * framebuffer, 
                                                        OSDictionary * info );
    static IOReturn create( IOFramebuffer * framebuffer );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOGraphicsDevice

OSDefineMetaClass( IOFramebuffer, IOGraphicsDevice )
OSDefineAbstractStructors( IOFramebuffer, IOGraphicsDevice )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::fbLock( void )
{
	FBLOCK(this);
}

void IOFramebuffer::fbUnlock( void )
{
    FBUNLOCK(this);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Cursor rendering
 */

#include "IOCursorBlits.h"

inline void IOFramebuffer::StdFBDisplayCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *shmem;
    IOGBounds saveRect;
    volatile unsigned char *vramPtr;    /* screen data pointer */
    unsigned int cursStart;
    unsigned int cursorWidth;
    int width;
    int height;

    shmem = GetShmem(inst);
    saveRect = shmem->cursorRect;
    /* Clip saveRect vertical within screen bounds */
    if (saveRect.miny < shmem->screenBounds.miny)
        saveRect.miny = shmem->screenBounds.miny;
    if (saveRect.maxy > shmem->screenBounds.maxy)
        saveRect.maxy = shmem->screenBounds.maxy;
    if (saveRect.minx < shmem->screenBounds.minx)
        saveRect.minx = shmem->screenBounds.minx;
    if (saveRect.maxx > shmem->screenBounds.maxx)
        saveRect.maxx = shmem->screenBounds.maxx;
    shmem->saveRect = saveRect; /* Remember save rect for RemoveCursor */

    vramPtr = inst->frameBuffer +
              (inst->rowBytes * (saveRect.miny - shmem->screenBounds.miny)) +
              (inst->bytesPerPixel * (saveRect.minx - shmem->screenBounds.minx));

    width = saveRect.maxx - saveRect.minx;
    height = saveRect.maxy - saveRect.miny;
    cursorWidth = shmem->cursorSize[0 != shmem->frame].width;

    cursStart = (saveRect.miny - shmem->cursorRect.miny) * cursorWidth +
                (saveRect.minx - shmem->cursorRect.minx);

    if (inst->cursorBlitProc)
        inst->cursorBlitProc( inst,
                              (void *) shmem,
                              vramPtr,
                              cursStart,
                              inst->totalWidth - width,   /* vramRow */
                              cursorWidth - width,      /* cursRow */
                              width,
                              height);
}

// Description: RemoveCursor erases the cursor by replacing the background
//              image that was saved by the previous call to DisplayCursor.
//              If the frame buffer is cacheable, flush at the end of the
//              drawing operation.

inline void IOFramebuffer::StdFBRemoveCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *shmem;
    volatile unsigned char *vramPtr;    /* screen data pointer */
    unsigned int vramRow;
    int width;
    int height;

    shmem = GetShmem(inst);

    /* Clip saveRect vertical within screen bounds */
    if ((shmem->saveRect.miny < shmem->screenBounds.miny)
     || (shmem->saveRect.maxy > shmem->screenBounds.maxy)
     || (shmem->saveRect.minx < shmem->screenBounds.minx)
     || (shmem->saveRect.maxx > shmem->screenBounds.maxx))
	{
		IOLog("%s: bad crsr saverect (%d, %d), (%d, %d) - (%d, %d), (%d, %d)\n",
					inst->thisName,
					shmem->saveRect.minx, shmem->saveRect.miny,
					shmem->saveRect.maxx, shmem->saveRect.maxy,
					shmem->screenBounds.minx, shmem->screenBounds.miny,
					shmem->screenBounds.maxx, shmem->screenBounds.maxy);
		return;
	}

    vramRow = inst->totalWidth; /* Scanline width in pixels */

    vramPtr = inst->frameBuffer +
              (inst->rowBytes * (shmem->saveRect.miny - shmem->screenBounds.miny))
              + (inst->bytesPerPixel *
                 (shmem->saveRect.minx - shmem->screenBounds.minx));

    width = shmem->saveRect.maxx - shmem->saveRect.minx;
    height = shmem->saveRect.maxy - shmem->saveRect.miny;
    vramRow -= width;

    if (inst->cursorRemoveProc)
        inst->cursorRemoveProc( inst, (void *)shmem,
                                vramPtr, vramRow, width, height);
}

inline void IOFramebuffer::RemoveCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *      shmem = GetShmem(inst);

    if (!inst->pagingState)
        return;

    if (shmem->hardwareCursorActive)
    {
        IOGPoint *              hs;

        hs = &shmem->hotSpot[0 != shmem->frame];
        inst->_setCursorState(
            shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
            shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, false );
    }
    else
        StdFBRemoveCursor(inst);
}

inline void IOFramebuffer::DisplayCursor( IOFramebuffer * inst )
{
    IOGPoint     *      hs;
    StdFBShmem_t *      shmem = GetShmem(inst);
    SInt32              x, y;

    if (!inst->pagingState)
        return;

    hs = &shmem->hotSpot[0 != shmem->frame];
    x  = shmem->cursorLoc.x - hs->x;
    y  = shmem->cursorLoc.y - hs->y;

    if (shmem->hardwareCursorActive)
        inst->_setCursorState( x - shmem->screenBounds.minx,
                               y - shmem->screenBounds.miny, true );
    else
    {
        shmem->cursorRect.maxx = (shmem->cursorRect.minx = x)
                                 + shmem->cursorSize[0 != shmem->frame].width;
        shmem->cursorRect.maxy = (shmem->cursorRect.miny = y)
                                 + shmem->cursorSize[0 != shmem->frame].height;
        StdFBDisplayCursor(inst);
        shmem->oldCursorRect = shmem->cursorRect;
    }
}

inline void IOFramebuffer::SysHideCursor( IOFramebuffer * inst )
{
    if (!GetShmem(inst)->cursorShow++)
        RemoveCursor(inst);
}

inline void IOFramebuffer::SysShowCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *shmem;

    shmem = GetShmem(inst);

    if (shmem->cursorShow)
        if (!--(shmem->cursorShow))
            DisplayCursor(inst);
}

inline void IOFramebuffer::CheckShield( IOFramebuffer * inst )
{
    IOGPoint *          hs;
    int                 intersect;
    IOGBounds           tempRect;
    StdFBShmem_t *      shmem = GetShmem(inst);

    /* Calculate temp cursorRect */
    hs = &shmem->hotSpot[0 != shmem->frame];
    tempRect.maxx = (tempRect.minx = (shmem->cursorLoc).x - hs->x)
                    + shmem->cursorSize[0 != shmem->frame].width;
    tempRect.maxy = (tempRect.miny = (shmem->cursorLoc).y - hs->y)
                    + shmem->cursorSize[0 != shmem->frame].height;

    intersect = TOUCHBOUNDS(tempRect, shmem->shieldRect);
    if (intersect != shmem->shielded)
        (shmem->shielded = intersect) ?
        SysHideCursor(inst) : SysShowCursor(inst);
}

#include "AppleLogo.h"
#include "AppleLogo2x.h"

/**************************************************************
 LZSS.C -- A Data Compression Program
***************************************************************
    4/6/1989 Haruhiko Okumura
    Use, distribute, and modify this program freely.
    Please send me your improved versions.
        PC-VAN      SCIENCE
        NIFTY-Serve PAF01022
        CompuServe  74050,1022

**************************************************************/
#define N         4096  /* size of ring buffer - must be power of 2 */
#define F         18    /* upper limit for match_length */
#define THRESHOLD 2     /* encode string into position and length
                           if match_length is greater than this */

static void IOFramebufferBootInitFB(IOVirtualAddress frameBuffer, 
			uint32_t framebufferWidth, uint32_t framebufferHeight, 
			uint32_t totalWidth, uint32_t consoleDepth, uint8_t logo)
{
	const uint8_t  * src;
	uint32_t *       out;
	uint32_t         gray, data, x, y, ox, oy, sx, lw, lh;

	if (32 == consoleDepth)
		gray = 0xbfbfbf;
	else if (30 == consoleDepth)
#define g10 ((0xbf << 2) | (0xbf >> 6))
		gray = (g10 | (g10 << 10 ) | (g10 << 20));
	else
		return;

    src = (2 == logo) ? &gAppleLogo2XPacked[0] : &gAppleLogoPacked[0];
	lw  = (2 == logo) ? kAppleLogo2XWidth      : kAppleLogoWidth;
	lh  = (2 == logo) ? kAppleLogo2XHeight     : kAppleLogoHeight;

	ox = (framebufferWidth - lw) / 2;
	oy = (framebufferHeight - lh) / 2;

    out = (uint32_t *) frameBuffer;

    /* ring buffer of size N, with extra F-1 bytes to aid string comparison */
    uint8_t text_buf[N + F - 1];
    int32_t  i, j, k, r;
    uint32_t flags;

    for (i = 0; i < N - F; i++) text_buf[i] = ' ';
    r = N - F;
    flags = 0;
    j = 0;
    k = INT32_MAX;

	for (y = 0; y < framebufferHeight; y++)
	{
		if ((!logo) || (y < oy) || (y >= (oy + lh)))
		{
			for (x = 0; x < framebufferWidth; x++) out[x] = gray;	
		}
		else
		{
			for (x = 0; x < ox; x++) out[x] = gray;
			for (sx = 0; sx < lw; sx++)
			{
				if (k > j) {
					if (((flags >>= 1) & 0x100) == 0) {
						data = *src++;
						flags = data | 0xFF00;  /* uses higher byte cleverly */
					}   					    /* to count eight */
					if (flags & 1) data = *src++;
					else {
						i = *src++;
						j = *src++;
						i |= ((j & 0xF0) << 4);
						j  =  (j & 0x0F) + THRESHOLD;
						k = 0;
					}
				}
				if (k <= j) {
					data = text_buf[(i + k) & (N - 1)];
					k++;
				}
				text_buf[r++] = data;
				r &= (N - 1);

				if (32 == consoleDepth)
					data |= (data << 8) | (data << 16);
				else if (30 == consoleDepth)
					data |= (data << 10) | (data << 20);
				out[x++] = data;
			}
			for (; x < framebufferWidth; x++) out[x] = gray;
		}
		out += totalWidth;
	}
}

void IOFramebuffer::setupCursor(void)
{
    StdFBShmem_t *              shmem = GetShmem(this);
	IOPixelInformation *        info  = &__private->pixelInfo;
    volatile unsigned char *    bits;
    IOByteCount                 cursorImageBytes, waitCursorImageBytes;

    rowBytes = info->bytesPerRow;
    totalWidth = (rowBytes * 8) / info->bitsPerPixel;
    bytesPerPixel = info->bitsPerPixel / 8;
    if (bytesPerPixel > 4)
        __private->cursorBytesPerPixel = 4;
    else
        __private->cursorBytesPerPixel = bytesPerPixel;
    frameBuffer = (volatile unsigned char *) vramMap->getVirtualAddress();
    __private->framebufferWidth  = info->activeWidth;
    __private->framebufferHeight = info->activeHeight;
    if (info->bitsPerComponent > 8)
        __private->consoleDepth = info->componentCount * info->bitsPerComponent;
    else
        __private->consoleDepth = info->bitsPerPixel;

    if (shmem)
    {
		DEBG1(thisName, " setupCursor online %d, (%d, %d) - (%d, %d)\n", 
                __private->online,
                shmem->screenBounds.minx, shmem->screenBounds.miny,
                shmem->screenBounds.maxx, shmem->screenBounds.maxy);
        if (__private->online &&
                ((shmem->screenBounds.maxx == shmem->screenBounds.minx)
                || (shmem->screenBounds.maxy == shmem->screenBounds.miny)))
        {
            // a default if no one calls IOFBSetBounds()
            shmem->screenBounds.minx = 0;
            shmem->screenBounds.miny = 0;
            shmem->screenBounds.maxx = info->activeWidth;
            shmem->screenBounds.maxy = info->activeHeight;
			__private->screenBounds[0] = shmem->screenBounds;
			__private->screenBounds[1] = shmem->screenBounds;
        }
		__private->actualVBLCount = 0;

        shmem->cursorSize[0] = maxCursorSize;
        shmem->cursorSize[1] = __private->maxWaitCursorSize;
        shmem->cursorSize[2] = __private->maxWaitCursorSize;
        shmem->cursorSize[3] = __private->maxWaitCursorSize;

        cursorImageBytes = maxCursorSize.width * maxCursorSize.height
                           * __private->cursorBytesPerPixel;
        waitCursorImageBytes = __private->maxWaitCursorSize.width * __private->maxWaitCursorSize.height
                               * __private->cursorBytesPerPixel;
        bits = shmem->cursor;

        for (UInt32 i = 0; i < __private->numCursorFrames; i++)
        {
            __private->cursorFlags[i] = kIOFBCursorImageNew;
            __private->cursorImages[i] = bits;
            bits += i ? waitCursorImageBytes : cursorImageBytes;
        }
        if (info->bitsPerPixel <= 8)
        {
            for (UInt32 i = 0; i < __private->numCursorFrames; i++)
            {
                __private->cursorMasks[i] = bits;
                bits += i ? waitCursorImageBytes : cursorImageBytes;
            }
        }
        cursorSave = bits;
    }

    switch (info->bitsPerPixel)
    {
        case 8:
            if (colorConvert.t._bm256To38SampleTable
                    && colorConvert.t._bm38To256SampleTable)
            {
                cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor8P;
                cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor8;
            }
            break;
        case 16:
            if (colorConvert.t._bm34To35SampleTable
                    && colorConvert.t._bm35To34SampleTable)
            {
                cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor555;
                cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor16;
            }
            break;
        case 32:
        case 64:
            if (colorConvert.t._bm256To38SampleTable
                    && colorConvert.t._bm38To256SampleTable)
            {
                if (10 == info->bitsPerComponent)
                    cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor30Axxx;
                else
                    cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor32Axxx;
                cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor32;
            }
            break;
        default:
            DEBG1(thisName, " can't do cursor at depth %d\n",
                  (uint32_t) info->bitsPerPixel);
            cursorBlitProc = (CursorBlitProc) NULL;
            cursorRemoveProc = (CursorRemoveProc) NULL;
            break;
    }
}

void IOFramebuffer::stopCursor( void )
{
    cursorBlitProc = (CursorBlitProc) NULL;
    cursorRemoveProc = (CursorRemoveProc) NULL;
}

IOReturn IOFramebuffer::extCreateSharedCursor(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst         = (IOFramebuffer *) target;
    int             version      = args->scalarInput[0];
    int             maxWidth     = args->scalarInput[1];
    int             maxWaitWidth = args->scalarInput[2];

    IOReturn err;

    if ((err = inst->extEntry(true)))
        return (err);

	err = inst->createSharedCursor( version, maxWidth, maxWaitWidth );

    inst->extExit(err);

    return (err);
}

bool IOFramebuffer::deepFramebuffer(IOPixelInformation * pixelInfo)
{
    if ((pixelInfo->pixelType == kIORGBSignedFloatingPointPixels)
    || (pixelInfo->bitsPerComponent > 8))
        return (true);
    else
        return (false);
}

bool IOFramebuffer::validFramebuffer(IOPixelInformation * pixelInfo)
{
    if (pixelInfo->pixelType == kIORGBSignedDirectPixels)
        return (false);

    if (deepFramebuffer(pixelInfo))
        return (true);

    return (true);
}

IOIndex IOFramebuffer::closestDepth(IODisplayModeID mode, IOPixelInformation * matchInfo)
{
	IOReturn err;
	IOIndex  depth;
	IOPixelInformation pixelInfo;

	depth = 0; 
	for (depth = 0;; depth++)
	{
		err = getPixelInformation(mode, depth, kIOFBSystemAperture, &pixelInfo);
		if (kIOReturnSuccess != err)
		{
			depth = 0;
			break;
		}
		if ((pixelInfo.bitsPerPixel == matchInfo->bitsPerPixel)
		  && (pixelInfo.pixelType == matchInfo->pixelType)
		  && (pixelInfo.componentCount == matchInfo->componentCount)
		  && (pixelInfo.bitsPerComponent == matchInfo->bitsPerComponent))
			break;
	}
	return (depth);
}

IOReturn IOFramebuffer::extGetPixelInformation(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer *      inst        = (IOFramebuffer *) target;
    IODisplayModeID      displayMode = args->scalarInput[0];
    IOIndex              depth       = args->scalarInput[1];
    IOPixelAperture      aperture    = args->scalarInput[2];
    IOPixelInformation * pixelInfo   = (IOPixelInformation *) args->structureOutput;

    IOReturn err;

    if ((err = inst->extEntry(false)))
        return (err);

	err = inst->getPixelInformation(displayMode, depth, aperture, pixelInfo);

    inst->extExit(err);

    return (err);
}

IOReturn IOFramebuffer::extGetCurrentDisplayMode(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst = (IOFramebuffer *) target;
    IODisplayModeID displayMode;
    IOIndex         depth;
    
    IOReturn err;
    
    if ((err = inst->extEntry(false)))
        return (err);

    if (kIODisplayModeIDInvalid != inst->__private->aliasMode)
    {
        displayMode = inst->__private->aliasMode;
        depth       = inst->__private->currentDepth;
    }
    else
    {
        err = inst->getCurrentDisplayMode(&displayMode, &depth);
    }

    inst->extExit(err);
    
    args->scalarOutput[0] = displayMode;
    args->scalarOutput[1] = depth;
    
    return (err);
}

IOReturn IOFramebuffer::extSetStartupDisplayMode(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst        = (IOFramebuffer *) target;
    IODisplayModeID displayMode = args->scalarInput[0];
    IOIndex         depth       = args->scalarInput[1];

    IOReturn err;

    if ((err = inst->extEntry(false)))
        return (err);

	err = inst->setStartupDisplayMode( displayMode, depth );
    
	if (inst->__private->online && (kIODetailedTimingValid & inst->__private->timingInfo.flags))
	{
		OSData * data = OSData::withBytes(
			&inst->__private->timingInfo, sizeof(inst->__private->timingInfo));
		if (data)
		{
			inst->setPreference(NULL, gIOFBStartupModeTimingKey, data);
			data->release();
		}
	}

    inst->extExit(err);
    
    return (err);
}

#if 0
static void IOFBLogGamma(
    uint32_t channelCount, uint32_t srcDataCount,
    uint32_t dataWidth, const void * data)
{
    uint32_t c, i;
	uint16_t * words;
	words = ((uint16_t *)data);
    kprintf("/*    0      1      2      3      4      5      6      7      8      9      A      B      C      D      E      F */");
	for (c = 0; c < channelCount; c++)
	{
		for (i = 0; i < srcDataCount; i++)
		{
			if( 0 == (i & 15))
				kprintf("\n    ");
			kprintf("0x%04x,", words[i]);
		}
		words += i;
		kprintf("\n\n\n");
	}
}
#endif

enum { kIOFBGammaPointCountMax = 256 };
enum { kIOFBGammaDesiredError  = 127 };

struct IOFBLineSeg
{
	// [start end]
    uint16_t start;
    uint16_t end;
    uint16_t dist;
    uint16_t split;
};
typedef struct IOFBLineSeg IOFBLineSeg;

static void 
IOFBSegDist(const uint16_t data[], IOFBLineSeg * seg)
{
	uint16_t start;
	uint16_t end;
	uint16_t idx;
	uint16_t interp;
	int16_t  dist;

	seg->dist  = 0;
	start = seg->start;
	end   = seg->end;
	for (idx = start + 1; idx < end; idx++)
	{
		interp = data[start] + ((data[end] - data[start]) * (idx - start)) / (end - start);
		dist = data[idx] - interp;
		if (dist < 0) dist = -dist;
		if (dist > seg->dist)
		{
			seg->dist = dist;
			seg->split = idx;
		}
	}
}

static void 
IOFBSegInit(const uint16_t data[], IOFBLineSeg * seg, uint16_t start, uint16_t end)
{
	seg->start = start;
	seg->end   = end;
	IOFBSegDist(data, seg);
}

static void
IOFBSimplifySegs(const uint16_t data[], 
				uint16_t desiredError, uint16_t * maxError, 
				IOFBLineSeg * segs, uint16_t count, uint16_t maxCount,
				IOFBBootGamma * bootGamma)
{
	IOFBGamma * channelGamma;
	uint16_t idx;
	uint16_t furthest;

	while (count < maxCount)
	{
		furthest = 0;
		for (idx = 1; idx < count; idx++)
		{
			if (segs[idx].dist > segs[furthest].dist) furthest = idx;
		}
		if (segs[furthest].dist <= desiredError) break;
		bcopy(&segs[furthest+1], &segs[furthest+2], (count - furthest - 1) * sizeof(IOFBLineSeg));
		count++;
		IOFBSegInit(data, &segs[furthest+1], segs[furthest].split, segs[furthest].end);
		segs[furthest].end = segs[furthest].split;
		IOFBSegDist(data, &segs[furthest]);
	}

	*maxError = 0;
	channelGamma = &bootGamma->gamma.red;
	for (idx = 0; idx < count; idx++)
	{
		if (segs[idx].dist > *maxError) 
		{
			*maxError = segs[idx].dist;
			//IOLog("max error seg 0x%x, 0x%x count %d, target %d\n", segs[idx].start, segs[idx].end, count, maxCount);
		}
		if (!(segs[idx].start & 255))
		{
			if (segs[idx].start) 
			{ 
				channelGamma = (typeof(channelGamma)) &channelGamma->points[channelGamma->pointCount];
			}
			channelGamma->pointCount = 0;
			continue;
		}
		channelGamma->points[channelGamma->pointCount].in =
							(segs[idx].start & 0xFF) | ((segs[idx].start & 0xFF) << 8);
		channelGamma->points[channelGamma->pointCount].out = data[segs[idx].start];
		channelGamma->pointCount++;
	}
}

static bool 
IOFBCompressGamma(
	IOFBBootGamma * bootGamma,
    uint16_t channelCount, uint16_t srcDataCount,
    uint16_t dataWidth, const void * _data,
    uint16_t desiredError, uint16_t maxCount,
    uint16_t * maxError)
{
	IOFBGamma *      channelGamma;
	const uint16_t * data = (typeof(data)) _data;
	IOFBLineSeg *    segs;
	uint16_t         idx;

	if ((3 != channelCount) || (16 != dataWidth)) return (false);

	maxCount += 3;
	segs = IONew(IOFBLineSeg, maxCount);
	if (!segs) return (false);

	data = (typeof(data)) _data;

	IOFBSegInit(data, &segs[0],   0, 255);
	IOFBSegInit(data, &segs[1], 256, 511);
	IOFBSegInit(data, &segs[2], 512, 767);

	IOFBSimplifySegs(data, desiredError, maxError,
					segs, 3, maxCount,
					bootGamma);

	IODelete(segs, IOFBLineSeg, maxCount);

	channelGamma = &bootGamma->gamma.red;
	for (idx = 0; idx < channelCount; idx++)
	{
		channelGamma = (typeof(channelGamma)) &channelGamma->points[channelGamma->pointCount];
	}
	bootGamma->length = ((uintptr_t) channelGamma) - ((uintptr_t) bootGamma);

	return (true);
}


static void
IOFBDecompressGamma(const IOFBBootGamma * bootGamma, uint16_t * data)
{
	const IOFBGamma * channelGamma;
	uint16_t channel, idx, seg;
	uint16_t startIn, startOut;
	uint16_t endIn, endOut;

	channelGamma = &bootGamma->gamma.red;
	for (channel = 0; channel < 3; channel++)
	{
		seg = 0;
		startIn = 0;
		startOut = 0x0000;
		endIn = 0;
		endOut = 0;
		for (idx = 0; idx < 256; idx++)
		{
			if ((idx == endIn) && (idx != 255))
			{
				startIn = endIn;
				startOut = endOut;
				if (seg < channelGamma->pointCount)
				{
					endIn = channelGamma->points[seg].in;
					endOut = channelGamma->points[seg].out;
					seg++;
				}
				else
				{
					endIn = 255;
					endOut = 0xFFFF;
				}
			}
			data[channel * 256 + idx] = startOut + ((endOut - startOut) * (idx - startIn)) / (endIn - startIn);
		}
		channelGamma = (typeof(channelGamma)) &channelGamma->points[channelGamma->pointCount];
	}
}

void IOFramebuffer::saveGammaTables(void)
{
	IORegistryEntry * options;
	const OSSymbol *  sym;
	IOFBBootGamma *   bootGamma;
	IOFramebuffer *   fb;
	OSNumber *        num;
	OSData *          data;
	uint32_t          maxCount;
	uint16_t          maxError;

	options = IORegistryEntry::fromPath("/options", gIODTPlane);
	if (!options) return;

	maxCount = (gIOFBBacklightDisplayCount + gIOFBDisplayCount);
	if (!maxCount) return;
	maxCount = ((0x700 - (maxCount * sizeof(IOFBBootGamma))) / sizeof(IOFBGammaPoint) / maxCount);
	if (maxCount > kIOFBGammaPointCountMax) maxCount = kIOFBGammaPointCountMax;

	bootGamma = (typeof(bootGamma)) IOMalloc(sizeof(IOFBBootGamma) 
					+ maxCount * sizeof(IOFBGammaPoint));
	if (!bootGamma) return;

	data = OSData::withCapacity(512);
	if (!data) return;

	for (UInt32 index = 0;
		 (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
		 index++)
	{
		FBLOCK(fb);
		if (fb->__private->display) do
		{
			bootGamma->vendor  = 0;
			bootGamma->product = 0;
			bootGamma->serial  = 0;
			bootGamma->length  = 0;
			bootGamma->resvA   = 0;
			bootGamma->resvB   = 0;

			if ((num = OSDynamicCast(OSNumber, fb->__private->display->getProperty(kDisplayVendorID))))
				bootGamma->vendor  = num->unsigned32BitValue();
			if ((num = OSDynamicCast(OSNumber, fb->__private->display->getProperty(kDisplayProductID))))
				bootGamma->product = num->unsigned32BitValue();
			if ((num = OSDynamicCast(OSNumber, fb->__private->display->getProperty(kDisplaySerialNumber))))
				bootGamma->serial  = num->unsigned32BitValue();
#if 0
        	IOFBLogGamma(fb->__private->rawGammaChannelCount, 
        				 fb->__private->rawGammaDataCount, 
        				 fb->__private->rawGammaDataWidth, 
        				 fb->__private->rawGammaData);
#endif
        	if (IOFBCompressGamma(bootGamma, 
        							fb->__private->rawGammaChannelCount, 
        							fb->__private->rawGammaDataCount, 
        							fb->__private->rawGammaDataWidth, 
        							fb->__private->rawGammaData,
        						    kIOFBGammaDesiredError, maxCount, 
        						    &maxError))
			{
				DEBG1(fb->thisName, " compressed gamma %d max error 0x%04x\n", bootGamma->length, maxError);
				if (bootGamma->gamma.red.pointCount)
				{
					data->appendBytes(bootGamma, bootGamma->length);
				}
			}
		}
		while (false);
		FBUNLOCK(fb);
	}
	IOFree(bootGamma, sizeof(IOFBBootGamma) 
					+ maxCount * sizeof(IOFBGammaPoint));
	sym = OSSymbol::withCStringNoCopy(kIOFBBootGammaKey);
	if (sym && data->getLength())
	{
		options->setProperty(sym, data);
		sym->release();
	}
	data->release();
	options->release();
}

IOReturn IOFramebuffer::extSetGammaTable(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst          = (IOFramebuffer *) target;
    UInt32          channelCount  = args->scalarInput[0];
    UInt32          dataCount     = args->scalarInput[1];
    UInt32          dataWidth     = args->scalarInput[2];
    IOReturn        err;
    IOByteCount     dataLen;

    if ((err = inst->extEntry(true)))
        return (err);

    dataLen  = (dataWidth + 7) / 8;
    dataLen *= dataCount * channelCount;

    if (dataLen != inst->__private->rawGammaDataLen)
    {
        if (inst->__private->rawGammaDataLen)
            IODelete(inst->__private->rawGammaData, UInt8, inst->__private->rawGammaDataLen);
        inst->__private->rawGammaData = IONew(UInt8, dataLen);
        inst->__private->rawGammaDataLen = dataLen;
    }

    if (!inst->__private->rawGammaData)
        err = kIOReturnNoMemory;
    else
    {
        inst->__private->rawGammaChannelCount = channelCount;
        inst->__private->rawGammaDataCount    = dataCount;
        inst->__private->rawGammaDataWidth    = dataWidth;

        if (args->structureInputDescriptor)
        {
            if (dataLen != args->structureInputDescriptor->getLength())
                err = kIOReturnBadArgument;
            else
            {
                err = args->structureInputDescriptor->prepare(kIODirectionOut);
                if ((kIOReturnSuccess == err)
                 && (dataLen != args->structureInputDescriptor->readBytes(
                                0, inst->__private->rawGammaData, dataLen)))
                {
                    err = kIOReturnVMError;
                }
                args->structureInputDescriptor->complete();
            }
        }
        else
        {
            if (dataLen == args->structureInputSize)
                bcopy(args->structureInput, inst->__private->rawGammaData, dataLen);
            else
                err = kIOReturnBadArgument;
        }
        if (kIOReturnSuccess == err)
        {
#if 0
        	IOFBLogGamma(inst->__private->rawGammaChannelCount, 
        				 inst->__private->rawGammaDataCount, 
        				 inst->__private->rawGammaDataWidth, 
        				 inst->__private->rawGammaData);
#endif
            err = inst->updateGammaTable(channelCount, dataCount, dataWidth, inst->__private->rawGammaData);
        }
    }
#if 0
DEBG1(inst->thisName, " extSetGammaTable(%x) online %d %ld %ld data %x\n", 
	err, inst->__private->online, 
	channelCount, dataCount,
	*((uint32_t *) inst->__private->rawGammaData));
#endif

	if (kIOFBBootGrayValue == gIOFBGrayValue) gIOFBGrayValue = kIOFBGrayValue;
	inst->__private->gammaSet = 1;

    inst->extExit(err);

    return (err);
}

IOReturn IOFramebuffer::updateGammaTable(
    UInt32 channelCount, UInt32 srcDataCount,
    UInt32 dataWidth, const void * data,
    bool immediate)
{
    IOReturn    err = kIOReturnBadArgument;
    IOByteCount dataLen;
    UInt16 *    channelData;
    UInt32      dataCount;
    UInt32      tryWidth;
    UInt8 *     table = NULL;
    bool        needAlloc;
    bool        gammaHaveScale = ((1 << 16) != __private->gammaScale[0])
                                || ((1 << 16) != __private->gammaScale[1])
                                || ((1 << 16) != __private->gammaScale[2]);
	const uint32_t * adjustParams = NULL;
	const uint32_t * adjustNext   = NULL;
	uint32_t         gammaThresh;
	uint32_t         gammaAdjust;

	if (GAMMA_ADJ && gIOGraphicsControl && (__private->desiredGammaDataWidth <= 8))
	{
		static const uint32_t _params[]  = { 138, 3, 256, 4 }; 
		adjustParams = &_params[0];
	}
	else
	{
		gammaThresh = -1U;
		gammaAdjust = 0;
	}
    do
    {
        if (!__private->online)
        {
			DEBG1(thisName, " offline updateGammaTable\n");
			err = kIOReturnOffline;
            break;
		}
        if (dataWidth != 16)
            break;

        if (!__private->desiredGammaDataWidth)
        {
            __private->desiredGammaDataWidth = dataWidth;
            __private->desiredGammaDataCount = srcDataCount;
        }
    
        dataCount = __private->desiredGammaDataCount;
        dataLen   = (__private->desiredGammaDataWidth + 7) / 8;
        dataLen  *= dataCount * channelCount;
        dataLen  += __private->gammaHeaderSize;

        needAlloc = (0 == __private->gammaDataLen);
        if (!needAlloc)
        {
            table = __private->gammaData;
            if (__private->gammaDataLen != dataLen)
            {
                IODelete(table, UInt8, __private->gammaDataLen);
                __private->gammaData = NULL;
                needAlloc = true;
            }
            __private->gammaDataLen = 0;
            __private->gammaNeedSet = false;
        }

        if (needAlloc)
        {
            table = IONew(UInt8, dataLen);
            if (!table)
            {
                err = kIOReturnNoMemory;
                continue;
            }
            __private->gammaData = table;
        }

        __private->gammaChannelCount = channelCount;
        __private->gammaDataCount    = dataCount;
    
        table += __private->gammaHeaderSize;
   
        tryWidth = __private->desiredGammaDataWidth;

#if 0
        OSData * ddata = OSData::withBytesNoCopy((void *)data, srcDataCount*channelCount*sizeof(UInt16));
        setProperty("GammaIn", ddata);
        ddata->release();
        ddata = OSData::withBytesNoCopy(table, dataLen - __private->gammaHeaderSize);
        setProperty("GammaOut", ddata);
        ddata->release();
#endif

        if ((__private->desiredGammaDataCount == dataCount)
          && (tryWidth == dataWidth)
          && !gammaHaveScale
          && data
          && !adjustParams)
            bcopy(data, table, dataLen - __private->gammaHeaderSize);
        else
        {
            uint32_t pin, pt5, in, out, channel, idx, maxSrc, maxDst, interpCount;
            int64_t value, value2;
    
            pin = (1 << tryWidth) - 1;
            pt5 = 0; //(1 << (tryWidth - 1));               // truncate not round
            if (gammaHaveScale)
                dataWidth += 16;

            channelData = (UInt16 *) data;
            maxSrc = (srcDataCount - 1);
            maxDst = (__private->desiredGammaDataCount - 1);
            if ((srcDataCount < __private->desiredGammaDataCount)
             && (0 == (__private->desiredGammaDataCount % srcDataCount)))
                interpCount = __private->desiredGammaDataCount / srcDataCount;
            else
                interpCount = 0;

            for (out = 0, channel = 0; channel < channelCount; channel++)
            {
				if (adjustParams)
				{
					gammaThresh = 0;
					adjustNext = adjustParams;
				}
                for (idx = 0; idx <= maxDst; idx++)
                {
					if (idx >= gammaThresh)
					{
						gammaThresh = *adjustNext++;
						gammaAdjust = *adjustNext++;
					}
					if (channelData)
					{
						in = ((idx * maxSrc) + (idx ? (idx - 1) : 0)) / maxDst;
						value = (channelData[in] /*+ pt5*/);
						if (interpCount && (in < maxSrc))
						{
							value2 = (channelData[in+1] /*+ pt5*/);
							value += ((value2 - value) * (idx % interpCount) + (interpCount - 1)) / interpCount;
						}
					}
					else
					    value = (idx * ((1 << dataWidth) - 1)) / maxDst; 
                    if (gammaHaveScale)
                        value = ((value * __private->gammaScale[channel]) + (1 << 15));
                    value = (value >> (dataWidth - tryWidth));
					if (value)
						value += gammaAdjust;
                    if (value > pin)
                        value = pin;

                    if (tryWidth <= 8)
                        ((UInt8 *) table)[out] = (value & 0xff);
                    else
                        ((UInt16 *) table)[out] = value;
                    out++;
                }
                if (channelData) channelData += srcDataCount;
            }
        }
        __private->gammaDataWidth = tryWidth;
        __private->gammaDataLen   = dataLen;
    
        if (ASYNC_GAMMA && !immediate)
        {
			if (__private->vblThrottle && __private->deferredCLUTSetTimerEvent)
			{
				AbsoluteTime deadline;
				getTimeOfVBL(&deadline, 1);
				__private->deferredCLUTSetTimerEvent->wakeAtTime(deadline);
				__private->gammaNeedSet = true;
				err = kIOReturnSuccess;
			}
			else if (__private->deferredCLUTSetEvent)
			{
				__private->gammaNeedSet = true;
				err = kIOReturnSuccess;
			}
		}
		if (!__private->gammaNeedSet)
        {
            err = setGammaTable( __private->gammaChannelCount, __private->gammaDataCount,
                                 __private->gammaDataWidth, __private->gammaData );
            updateCursorForCLUTSet();
        }
    }
    while (false);

    return (err);
}


IOReturn IOFramebuffer::extSetCLUTWithEntries(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst    = (IOFramebuffer *) target;
    UInt32          index   = args->scalarInput[0];
    IOOptionBits    options = args->scalarInput[1];
    IOColorEntry *  colors  = (IOColorEntry *) args->structureInput;
    IOByteCount     dataLen = args->structureInputSize;

    IOReturn    err;
    UInt8 *     table;
    bool        needAlloc;

    if ((err = inst->extEntry(false)))
        return (err);

    err = kIOReturnBadArgument;
    if (inst->__private->deferredCLUTSetEvent)
    {
        do
        {
            needAlloc = (0 == inst->__private->clutDataLen);
            if (!needAlloc)
            {
                if (index || (inst->__private->clutDataLen != dataLen))
                {
                    inst->checkDeferredCLUTSet();
                    needAlloc = true;
                }
                inst->__private->clutDataLen = 0;
            }
    
            if (needAlloc)
            {
                table = IONew(UInt8, dataLen);
                if (!table)
                {
                    err = kIOReturnNoMemory;
                    continue;
                }
                inst->__private->clutData = table;
            }
            else
                table = inst->__private->clutData;
        
            inst->__private->clutIndex   = index;
            inst->__private->clutOptions = options;
            inst->__private->clutDataLen = dataLen;
    
            bcopy(colors, table, dataLen);

			if (inst->__private->vblThrottle && inst->__private->deferredCLUTSetTimerEvent)
			{
				AbsoluteTime deadline;
				inst->getTimeOfVBL(&deadline, 1);
				inst->__private->deferredCLUTSetTimerEvent->wakeAtTime(deadline);
			}

            err = kIOReturnSuccess;
        }
        while (false);
    }
    else
    {
        err = inst->setCLUTWithEntries( colors, index,
                                  dataLen / sizeof( IOColorEntry), options );
        inst->updateCursorForCLUTSet();
    }

    if (inst == gIOFBConsoleFramebuffer)
    {
        UInt32 count = index + dataLen / sizeof(IOColorEntry);
        if (count > 256)
            count = 256;
        for (; index < count; index++)
        {
            appleClut8[index * 3 + 0] = colors[index].red   >> 8;
            appleClut8[index * 3 + 1] = colors[index].green >> 8;
            appleClut8[index * 3 + 2] = colors[index].blue  >> 8;
        }
    }

    inst->extExit(err);

    return (err);
}

void IOFramebuffer::updateCursorForCLUTSet( void )
{
    if (__private->cursorClutDependent)
    {
        StdFBShmem_t *shmem = GetShmem(this);

        SETSEMA(shmem);
        if ((kIOFBHardwareCursorActive == shmem->hardwareCursorActive) 
         && !shmem->cursorShow)
        {
            RemoveCursor(this);
            setCursorImage( (void *) shmem->frame );
            DisplayCursor(this);
        }
        CLEARSEMA(shmem, this);
    }
}

void IOFramebuffer::deferredCLUTSetInterrupt( OSObject * owner,
                                              IOInterruptEventSource * evtSrc, int intCount )
{
    IOFramebuffer * self = (IOFramebuffer *) owner;

    if (self->__private->waitVBLEvent)
    {
        FBWL(self)->wakeupGate(self->__private->waitVBLEvent, true);
        self->__private->waitVBLEvent = 0;
    }

    self->checkDeferredCLUTSet();
}

void IOFramebuffer::deferredCLUTSetTimer(OSObject * owner, IOTimerEventSource * source)
{
    IOFramebuffer * self = (IOFramebuffer *) owner;
    self->checkDeferredCLUTSet();
}

void IOFramebuffer::checkDeferredCLUTSet( void )
{
    IOReturn    ret;
    bool        gammaNeedSet = __private->gammaNeedSet;
    IOByteCount clutLen      = __private->clutDataLen;

    if( !gammaNeedSet && !clutLen)
        return;

    __private->gammaNeedSet = false;
    __private->clutDataLen  = 0;

    if (gammaNeedSet)
    {
        ret = setGammaTable( __private->gammaChannelCount, __private->gammaDataCount, 
                             __private->gammaDataWidth, __private->gammaData );
    }

    if (clutLen)
    {
        ret = setCLUTWithEntries( (IOColorEntry *) __private->clutData, __private->clutIndex,
                                  clutLen / sizeof( IOColorEntry), __private->clutOptions );

        IODelete(__private->clutData, UInt8, clutLen);
    }

    updateCursorForCLUTSet();
}

IOReturn IOFramebuffer::createSharedCursor(
    int version, int maxWidth, int maxWaitWidth )
{
    StdFBShmem_t *      shmem;
    UInt32              shmemVersion;
    IOByteCount         size, maxImageSize, maxWaitImageSize;
    UInt32              numCursorFrames;

    DEBG(thisName, " vers = %08x, %d x %d\n",
         version, maxWidth, maxWaitWidth);

    shmemVersion = version & kIOFBShmemVersionMask;

    if (shmemVersion == kIOFBTenPtTwoShmemVersion)
    {
        numCursorFrames = (kIOFBShmemCursorNumFramesMask & version) >> kIOFBShmemCursorNumFramesShift;

        setProperty(kIOFBWaitCursorFramesKey, (numCursorFrames - 1), 32);
        setProperty(kIOFBWaitCursorPeriodKey, 33333333, 32);    /* 30 fps */
    }
    else if (shmemVersion == kIOFBTenPtOneShmemVersion)
    {
        numCursorFrames = 4;
    }
    else
        return (kIOReturnUnsupported);

    shmemClientVersion = shmemVersion;

    if (__private->cursorFlags)
    {
        IODelete( __private->cursorFlags, UInt8, __private->numCursorFrames );
        __private->cursorFlags = 0;
    }
    if (__private->cursorImages)
    {
        IODelete( __private->cursorImages, volatile unsigned char *, __private->numCursorFrames );
        __private->cursorImages = 0;
    }
    if (__private->cursorMasks)
    {
        IODelete( __private->cursorMasks, volatile unsigned char *, __private->numCursorFrames );
        __private->cursorMasks = 0;
    }
    __private->numCursorFrames = numCursorFrames;
    __private->cursorFlags     = IONew( UInt8, numCursorFrames );
    __private->cursorImages    = IONew( volatile unsigned char *, numCursorFrames );
    __private->cursorMasks     = IONew( volatile unsigned char *, numCursorFrames );

    if (!__private->cursorFlags || !__private->cursorImages || !__private->cursorMasks)
        return (kIOReturnNoMemory);

    maxImageSize = (maxWidth * maxWidth * kIOFBMaxCursorDepth) / 8;
    maxWaitImageSize = (maxWaitWidth * maxWaitWidth * kIOFBMaxCursorDepth) / 8;

    size = sizeof( StdFBShmem_t)
           + maxImageSize
           + max(maxImageSize, maxWaitImageSize)
           + ((numCursorFrames - 1) * maxWaitImageSize);

    if (!sharedCursor || (size != sharedCursor->getLength()))
    {
        IOBufferMemoryDescriptor * newDesc;

        priv = 0;
        newDesc = IOBufferMemoryDescriptor::withOptions(
                      kIODirectionNone | kIOMemoryKernelUserShared, size );
        if (!newDesc)
            return (kIOReturnNoMemory);

        if (sharedCursor)
            sharedCursor->release();
        sharedCursor = newDesc;
    }
    shmem = (StdFBShmem_t *) sharedCursor->getBytesNoCopy();
    priv = shmem;

    // Init shared memory area
    bzero( shmem, size );
	shmem->version = shmemClientVersion;
    shmem->structSize = size;
    shmem->cursorShow = 1;
    shmem->hardwareCursorCapable = haveHWCursor;
    for (UInt32 i = 0; i < numCursorFrames; i++)
        __private->cursorFlags[i] = kIOFBCursorImageNew;

    maxCursorSize.width = maxWidth;
    maxCursorSize.height = maxWidth;
    __private->maxWaitCursorSize.width = maxWaitWidth;
    __private->maxWaitCursorSize.height = maxWaitWidth;

    doSetup( false );

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::setBoundingRect( IOGBounds * bounds )
{
    return (kIOReturnUnsupported);
}

/**
 ** IOUserClient methods
 **/

IOReturn IOFramebuffer::newUserClient(  task_t          owningTask,
                                        void *          security_id,
                                        UInt32          type,
                                        IOUserClient ** handler )

{
    IOReturn            err = kIOReturnSuccess;
    IOUserClient *      newConnect = 0;
    IOUserClient *      theConnect = 0;

    switch (type)
    {
        case kIOFBServerConnectType:
            if (serverConnect)
                err = kIOReturnExclusiveAccess;
            else
            {
                if ((this == gIOFBConsoleFramebuffer) || !gIOFBConsoleFramebuffer)
                    getPlatform()->setConsoleInfo( 0, kPEReleaseScreen);

                err = open();
                if (kIOReturnSuccess == err)
                    newConnect = IOFramebufferUserClient::withTask(owningTask);
            }
            break;

        case kIOFBSharedConnectType:
            if (sharedConnect)
            {
                theConnect = sharedConnect;
                theConnect->retain();
            }
            else if (serverConnect)
            {
                newConnect = IOFramebufferSharedUserClient::withTask(owningTask);
                if (newConnect)
                    newConnect->retain();
            }
            else
                err = kIOReturnNotOpen;
            break;

        default:
            err = kIOReturnBadArgument;
    }

    if (newConnect)
    {
        if ((false == newConnect->attach(this))
                || (false == newConnect->start(this)))
        {
            newConnect->detach( this );
            newConnect->release();
        }
        else
            theConnect = newConnect;
    }

    *handler = theConnect;
    return (err);
}

IOReturn IOFramebuffer::extGetDisplayModeCount(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst  = (IOFramebuffer *) target;
    uint64_t *      count = &args->scalarOutput[0];

    IOReturn err;

    if ((err = inst->extEntry(false)))
        return (err);

	*count = inst->dead ? 0 : inst->getDisplayModeCount();
    
    inst->extExit(err);

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extGetDisplayModes(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer *   inst     = (IOFramebuffer *) target;
    IODisplayModeID * allModes = (IODisplayModeID *) args->structureOutput;
    uint32_t *        size     = &args->structureOutputSize;

    IOReturn    err;
    IOByteCount outSize;

    if ((err = inst->extEntry(false)))
        return (err);

	outSize = inst->getDisplayModeCount() * sizeof( IODisplayModeID);
	if (*size >= outSize)
	{
		*size = outSize;
		err = inst->getDisplayModes( allModes );
	}
	else
		err = kIOReturnBadArgument;

    inst->extExit(err);

    return (err);
}

IOReturn IOFramebuffer::extGetVRAMMapOffset(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst   = (IOFramebuffer *) target;
    // IOPixelAperture aperture = args->scalarInput[0];
    uint64_t *      offset = &args->scalarOutput[0];

    IOReturn err;
    
    if ((err = inst->extEntry(false)))
        return (err);
    
    *offset = inst->vramMapOffset;
    
    inst->extExit(err);

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::_extEntry(bool system, bool allowOffline, const char * where)
{
    IOReturn             err = kIOReturnSuccess;

	if (system)
	{
		TIMELOCK(gIOFBSystemWorkLoop, thisName, where);
	}

    TIMELOCK(FBWL(this), thisName, where);

    while (!pagingState && !gIOFBSystemPowerAckTo)
    {
//		IODisplayWrangler::activityChange(this);
		if (system)
		{
			FBUNLOCK(this);
			err = gIOFBSystemWorkLoop->sleepGate(&serverConnect, false);
			FBLOCK(this);
		}
		else
			err = FBWL(this)->sleepGate(&serverConnect, false);

        if (kIOReturnSuccess != err)
            break;
    }

	if ((kIOReturnSuccess == err) && !__private->online && !allowOffline) err = kIOReturnOffline;

	if (kIOReturnSuccess != err) DEBG1(thisName, " %s: err 0x%x\n", where, err);

	if (kIOReturnSuccess != err)
		_extExit(system, err, where);

    return (err);
}

void IOFramebuffer::_extExit(bool system, IOReturn result, const char * where)
{
	FBUNLOCK(this);
	if (system)
		SYSUNLOCK();
}

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::extSetBounds(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst   = (IOFramebuffer *) target;
    IOGBounds *     bounds = (IOGBounds *) args->structureInput;
    uint32_t		inSize = args->structureInputSize;
    uint32_t        virtIdx = 0;
    SInt16			ox, oy;

    IOReturn       err;
    StdFBShmem_t * shmem;

	if (inSize < sizeof(IOGBounds))
	    return (kIOReturnBadArgument);
	if (inSize >= (2 * sizeof(IOGBounds)))
		virtIdx = 1;

	DEBG1(inst->thisName, " (%d, %d), (%d, %d)\n",
                bounds->minx, bounds->miny,
                bounds->maxx, bounds->maxy);

    if (true &&
        (1 == (bounds->maxx - bounds->minx))
        && (1 == (bounds->maxy - bounds->miny)))
    {
        shmem = GetShmem(inst);
        if (shmem)
        {
			ox = shmem->screenBounds.minx;
			oy = shmem->screenBounds.miny;

            shmem->screenBounds = *bounds;
			inst->__private->screenBounds[0] = bounds[0];
			inst->__private->screenBounds[1] = bounds[virtIdx];

			ox = bounds->minx - ox;
			oy = bounds->miny - oy;
			shmem->saveRect.minx += ox;
			shmem->saveRect.maxx += ox;
			shmem->saveRect.miny += oy;
			shmem->saveRect.maxy += oy;
    	}

        return (kIOReturnSuccess);
    }
    
    if ((err = inst->extEntry(true)))
        return (err);

	shmem = GetShmem(inst);
    if (shmem)
    {
    	ox = shmem->screenBounds.minx;
    	oy = shmem->screenBounds.miny;
    	
        if ((kIOFBHardwareCursorActive == shmem->hardwareCursorActive) && inst->__private->online)
        {
            IOReturn   err;
            IOGPoint * hs;
            hs = &shmem->hotSpot[0 != shmem->frame];
            err = inst->_setCursorState(
                      shmem->cursorLoc.x - hs->x - ox,
                      shmem->cursorLoc.y - hs->y - oy, false );
        }

        shmem->screenBounds = *bounds;
		inst->__private->screenBounds[0] = bounds[0];
		inst->__private->screenBounds[1] = bounds[virtIdx];

		ox = bounds->minx - ox;
		oy = bounds->miny - oy;
		shmem->saveRect.minx += ox;
		shmem->saveRect.maxx += ox;
		shmem->saveRect.miny += oy;
		shmem->saveRect.maxy += oy;
    }

    inst->extExit(err);

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extValidateDetailedTiming(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst           = (IOFramebuffer *) target;
    void *          description    = const_cast<void *>(args->structureInput);
    void *          outDescription = args->structureOutput;
    uint32_t        inSize         = args->structureInputSize;
    uint32_t *      outSize        = &args->structureOutputSize;

    IOReturn    err;

    if (*outSize != inSize)
        return (kIOReturnBadArgument);

    if ((err = inst->extEntry(false)))
        return (err);

	err = inst->validateDetailedTiming( description, inSize );

    if (kIOReturnSuccess == err)
        bcopy( description, outDescription, inSize );

    inst->extExit(err);

    return (err);
}


IOReturn IOFramebuffer::extSetColorConvertTable(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst   = (IOFramebuffer *) target;
    UInt32          select = args->scalarInput[0];
    UInt8 *         data   = (UInt8 *) args->structureInput;
    IOByteCount     length = args->structureInputSize;

    static const IOByteCount checkLength[] = {
                16 * sizeof( UInt8),
                32 * sizeof( UInt8),
                256 * sizeof( UInt32),
                5 * 256 * sizeof( UInt8) };

    IOReturn            err;
    UInt8 *             table;

    if (select > 3)
        return (kIOReturnBadArgument);

    if (length != checkLength[select])
        return (kIOReturnBadArgument);

    if ((err = inst->extEntry(true)))
        return (err);
    
    do
    {
        err = kIOReturnNoMemory;
        table = inst->colorConvert.tables[select];
        if (0 == table)
        {
            table = (UInt8 *) IOMalloc( length );
            inst->colorConvert.tables[select] = table;
        }
        if (!table)
            continue;

        bcopy( data, table, length );
        if (select == 3)
            inst->white = data[data[255] + data[511] + data[767] + 1024];

        if ((NULL == inst->cursorBlitProc)
                && inst->__private->online
                && inst->colorConvert.tables[0] && inst->colorConvert.tables[1]
                && inst->colorConvert.tables[2] && inst->colorConvert.tables[3]
                && inst->vramMap
                && (inst->__private->pixelInfo.activeWidth >= 128))
            inst->setupCursor();

        err = kIOReturnSuccess;
    }
    while (false);

    inst->extExit(err);

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFramebuffer::requestTerminate( IOService * provider, IOOptionBits options )
{
    SYSLOCK();

    if (opened)
    {
        if (!gRunawayFramebuffers)
            gRunawayFramebuffers = OSArray::withCapacity(4);
        if (gRunawayFramebuffers)
            gRunawayFramebuffers->setObject(this);
    }

    SYSUNLOCK();

    return (false);
}

void IOFramebuffer::stop( IOService * provider )
{

    if (opened)
    {
        SYSLOCK();

        dead = true;

        setAttribute( kIOSystemPowerAttribute, kIOMessageSystemWillPowerOff );

        SYSUNLOCK();

        temporaryPowerClampOn();        // only to clear out kIOPMPreventSystemSleep
        PMstop();
        initialized = false;

        connectChangeInterrupt(this, 0);
    }

    return (super::stop(provider));
}

void IOFramebuffer::free()
{
    if (vblSemaphore)
        semaphore_destroy(kernel_task, vblSemaphore);
    if (__private)
    {
        IODelete( __private, IOFramebufferPrivate, 1 );
        __private = 0;
    }
    super::free();
}

IOService * IOFramebuffer::probe( IOService * provider, SInt32 * score )
{
    IOFramebuffer * replace = 0;

    if (gRunawayFramebuffers)
    {
        SYSLOCK();

        replace = (IOFramebuffer *) gRunawayFramebuffers->getObject(0);
        gRunawayFramebuffers->removeObject(0);

        SYSUNLOCK();
    }

    return (replace ? replace : this);
}

void IOFramebuffer::initialize()
{
}

bool IOFramebuffer::start( IOService * provider )
{
    bool runaway;

    if (!super::start(provider))
        return (false);

    runaway = (__private != 0);

    if (!__private)
    {
        __private = IONew( IOFramebufferPrivate, 1 );
        if (!__private)
            return (false);
        bzero( __private, sizeof(IOFramebufferPrivate) );
        __private->lastNotifyOnline = 0xdd;

        userAccessRanges = OSArray::withCapacity( 1 );
        if (!userAccessRanges)
            return (false);
    
        serverMsg = IOMalloc( sizeof (mach_msg_header_t) );
        if (!serverMsg)
            return (false);
        bzero( serverMsg, sizeof (mach_msg_header_t));
    }
    if (!thisName)
    	thisName = "IOFB?";

    // initialize superclass power management variables
    PMinit();
    // attach into the power management hierarchy
    setProperty("IOPMStrictTreeOrder", kOSBooleanTrue);
    provider->joinPMtree(this);

    if (runaway)
    {
        IOReturn err;

        SYSLOCK();
        err = enableController();
        if (kIOReturnSuccess == err)
        {
            opened = true;
            dead = false;
            isUsable = true;
            connectChangeInterrupt(this, 0);
        }
        SYSUNLOCK();
    }
    else
        registerService();

    return (true);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//
// BEGIN:       Implementation of the evScreen protocol
//

void IOFramebuffer::deferredMoveCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *      shmem = GetShmem(inst);
    IOReturn            err = kIOReturnSuccess;

    if (shmem->hardwareCursorActive)
    {
        if (shmem->cursorObscured)
        {
            shmem->cursorObscured = 0;
            if (shmem->cursorShow)
                --shmem->cursorShow;
        }
        if (shmem->hardwareCursorShields && shmem->shieldFlag)
            CheckShield(inst);
        if (!shmem->cursorShow)
        {
            IOGPoint * hs;
            hs = &shmem->hotSpot[0 != shmem->frame];
            err = inst->_setCursorState(
                        shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
                        shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, true );
#if 0
			// debug
			shmem->cursorRect.minx = shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx;
			shmem->cursorRect.miny = shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny;
			shmem->cursorRect.maxx = 0;
			shmem->cursorRect.maxy = 0;
#endif
        }
    }
    else
    {
        if (!shmem->cursorShow++)
            RemoveCursor(inst);
        if (shmem->cursorObscured)
        {
            shmem->cursorObscured = 0;
            if (shmem->cursorShow)
                --shmem->cursorShow;
        }
        if (shmem->shieldFlag)
            CheckShield(inst);
        if (shmem->cursorShow)
            if (!--shmem->cursorShow)
                DisplayCursor(inst);

        inst->flushCursor();

        if (inst->__private->cursorPanning)
        {
            IOGPoint * hs;
            hs = &shmem->hotSpot[0 != shmem->frame];
            err = inst->setCursorState(
                      shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
                      shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, false );
        }
    }
}

void IOFramebuffer::cursorWork( OSObject * p0, IOInterruptEventSource * evtSrc, int intCount )
{
    IOFramebuffer *             inst = (IOFramebuffer *) p0;
    StdFBShmem_t *              shmem = GetShmem(inst);
    struct IOFramebufferPrivate * __private = inst->__private;
    IOFBCursorControlAttribute  * cursorControl = &__private->cursorControl;
    IOReturn                    ret;
    IOHardwareCursorDescriptor  desc;

    IOOptionBits todo = inst->__private->cursorToDo;

    while (todo)
    {
        if (2 & todo)
        {
            desc.majorVersion   = kHardwareCursorDescriptorMajorVersion;
            desc.minorVersion   = kHardwareCursorDescriptorMinorVersion;
            desc.height         = shmem->cursorSize[0 != __private->framePending].height;
            desc.width          = shmem->cursorSize[0 != __private->framePending].width;
            desc.bitDepth       = inst->__private->cursorBytesPerPixel * 8;
            desc.maskBitDepth   = 0;
            desc.colorEncodings = 0;
            desc.flags          = 0;
            desc.supportedSpecialEncodings = kTransparentEncodedPixel;

            ret = (*cursorControl->callouts->setCursorImage) (
                      cursorControl->self, cursorControl->ref,
                      &desc, (void *) __private->framePending );
        }
        if (1 & todo)
            ret = (*cursorControl->callouts->setCursorState) (
                      cursorControl->self, cursorControl->ref,
                      __private->xPending, __private->yPending, __private->visiblePending );

        todo = __private->cursorToDo & ~todo;
        __private->cursorToDo = todo;
    }
}

IOOptionBits IOFramebuffer::_setCursorImage( UInt32 frame )
{
    StdFBShmem_t * shmem = GetShmem(this);
    IOGPoint *     hs;
    IOOptionBits   flags;
    bool           animation = (((int) frame) != shmem->frame);

    hs = &shmem->hotSpot[0 != frame];
    if (false && ((hs->x != __private->lastHotSpot.x) || (hs->y != __private->lastHotSpot.y)))
    {
        __private->lastHotSpot = *hs;
        if (GetShmem(this) && __private->deferredCLUTSetEvent && shmem->vblCount && !__private->cursorSlept && !suspended)
        {
            if (false)
            {
                __private->waitVBLEvent = hs;
                FBWL(this)->sleepGate(hs, THREAD_UNINT);
            }
            else if (CMP_ABSOLUTETIME(&shmem->vblDelta, &gIOFBMaxVBLDelta) < 0)
            {
                AbsoluteTime deadline = shmem->vblTime;
                ADD_ABSOLUTETIME(&deadline, &shmem->vblDelta);
                clock_delay_until(deadline);
            }
        }
    }

    if (!animation && ((kIOFBHardwareCursorActive == shmem->hardwareCursorActive) || !shmem->cursorShow))
	{
        RemoveCursor(this);
    }
    flags = (kIOReturnSuccess == setCursorImage( (void *) frame ))
            ? kIOFBHardwareCursorActive : 0;
    if (!animation && !shmem->cursorShow)
	{
        DisplayCursor(this);
	}
    if (!flags && __private->cursorThread && (__private->cursorBytesPerPixel >= 2))
    {
        __private->framePending = frame;
        __private->cursorToDo |= 2;
        flags = kIOFBHardwareCursorActive | kIOFBHardwareCursorInVRAM;
    }

    return (flags);
}

IOReturn IOFramebuffer::_setCursorState( SInt32 x, SInt32 y, bool visible )
{
    StdFBShmem_t *shmem = GetShmem(this);
    IOReturn ret = kIOReturnUnsupported;

    x -= __private->cursorHotSpotAdjust[0].x;
    y -= __private->cursorHotSpotAdjust[0].y;

    if (kIOFBHardwareCursorActive == shmem->hardwareCursorActive)
    {
        ret = setCursorState( x, y, visible );
    }
    else if (__private->cursorThread)
    {
        __private->cursorToDo |= 1;
        __private->xPending = x;
        __private->yPending = y;
        __private->visiblePending = visible;
    }

    return (ret);
}

void IOFramebuffer::transformLocation(StdFBShmem_t * shmem,
                                        IOGPoint * cursorLoc, IOGPoint * transformLoc)
{
    SInt32 x, y;

    x = cursorLoc->x - shmem->screenBounds.minx;
    y = cursorLoc->y - shmem->screenBounds.miny;

    if (__private->transform & kIOFBSwapAxes)
    {
        SInt32 t = x;
        x = y;
        y = t;
    }
    if (__private->transform & kIOFBInvertX)
        x = __private->framebufferWidth - x - 1;
    if (__private->transform & kIOFBInvertY)
        y = __private->framebufferHeight - y - 1;

    transformLoc->x = x + shmem->screenBounds.minx;
    transformLoc->y = y + shmem->screenBounds.miny;
}

void IOFramebuffer::moveCursor( IOGPoint * cursorLoc, int frame )
{
    UInt32 hwCursorActive;

	nextCursorLoc = *cursorLoc;
    nextCursorFrame = frame;

	CURSORLOCK(this);

    if (frame != shmem->frame)
    {
        if (__private->cursorFlags[frame] && pagingState)
        {
            hwCursorActive = _setCursorImage( frame );
            __private->cursorFlags[frame] = hwCursorActive ? kIOFBCursorHWCapable : 0;
        }
        else
            hwCursorActive = 0;

        shmem->frame = frame;
        if (shmem->hardwareCursorActive != hwCursorActive)
        {
            SysHideCursor( this );
            shmem->hardwareCursorActive = hwCursorActive;
            if (shmem->shieldFlag
                    && ((0 == hwCursorActive) || (shmem->hardwareCursorShields)))
                CheckShield(this);
            SysShowCursor( this );
        }
    }

	if (kIOFBRotateFlags & __private->transform)
		transformLocation(shmem, &nextCursorLoc, &shmem->cursorLoc);
	else
		shmem->cursorLoc = nextCursorLoc;
	shmem->frame = frame;
	deferredMoveCursor( this );

	CURSORUNLOCK(this);
}

void IOFramebuffer::hideCursor( void )
{
	CURSORLOCK(this);

    SysHideCursor(this);

	CURSORUNLOCK(this);
}

void IOFramebuffer::showCursor( IOGPoint * cursorLoc, int frame )
{
    UInt32 hwCursorActive;

	CURSORLOCK(this);

    if (frame != shmem->frame)
    {
        if (__private->cursorFlags[frame])
        {
            hwCursorActive = _setCursorImage( frame );
            __private->cursorFlags[frame] = hwCursorActive ? kIOFBCursorHWCapable : 0;
        }
        else
            hwCursorActive = 0;
        shmem->frame = frame;
        shmem->hardwareCursorActive = hwCursorActive;
    }

    shmem->cursorLoc = *cursorLoc;
    if (shmem->shieldFlag
            && ((0 == shmem->hardwareCursorActive) || (shmem->hardwareCursorShields)))
        CheckShield(this);

    SysShowCursor(this);

	CURSORUNLOCK(this);
}

void IOFramebuffer::updateVBL(OSObject * owner, IOTimerEventSource * sender)
{
    IOFramebuffer * inst = (IOFramebuffer *) owner;

	if (inst->__private->vblInterrupt
			&& inst->__private->vblThrottle 
			&& inst->__private->displaysOnline
			&& inst->pagingState)
	{
		inst->setInterruptState(inst->__private->vblInterrupt, kEnabledInterruptState);
		inst->__private->vblEnabled = true;
	}
}

void IOFramebuffer::deferredVBLDisable(OSObject * owner,
                                       IOInterruptEventSource * evtSrc, int intCount)
{
    IOFramebuffer * inst = (IOFramebuffer *) owner;

	if (inst->__private->vblInterrupt && inst->__private->vblThrottle)
	{
		inst->setInterruptState(inst->__private->vblInterrupt, kDisabledInterruptState);
	}
}

bool IOFramebuffer::getTimeOfVBL(AbsoluteTime * deadline, uint32_t frames)
{
	uint64_t last, now;
	uint64_t delta;

    StdFBShmem_t * shmem = GetShmem(this);
    if (!shmem)
        return (false);

	do
	{
		last = AbsoluteTime_to_scalar(&shmem->vblTime);
		now = mach_absolute_time();
	}
	while (last != AbsoluteTime_to_scalar(&shmem->vblTime));

	delta = AbsoluteTime_to_scalar(&shmem->vblDelta);
	if (delta)
	{
		now += frames * delta - ((now - last) % delta);
	}
	AbsoluteTime_to_scalar(deadline) = now;

	return (true);
}

void IOFramebuffer::handleVBL(IOFramebuffer * inst, void * ref)
{
    StdFBShmem_t * shmem = GetShmem(inst);
    AbsoluteTime   now;
    uint64_t       _now, calculatedDelta, drift;

    if (!shmem) return;
	inst->__private->actualVBLCount++;
	if (!inst->__private->vblEnabled) return;

    _now = mach_absolute_time();
    AbsoluteTime_to_scalar(&now) = _now;
	AbsoluteTime delta = now;
	SUB_ABSOLUTETIME(&delta, &shmem->vblTime);
	calculatedDelta = AbsoluteTime_to_scalar(&shmem->vblDelta);

	if (calculatedDelta && inst->__private->vblThrottle)
	{
		// round up to deal with scheduling noise.
		uint64_t timeCount = ((AbsoluteTime_to_scalar(&delta) * 2 / calculatedDelta) + 1) / 2;
		shmem->vblCount += timeCount;
		timeCount = (timeCount > inst->__private->actualVBLCount)
					? (timeCount - inst->__private->actualVBLCount)
					: (inst->__private->actualVBLCount - timeCount);
		if (gIOFBVBLDrift && (timeCount <= 1))
		{
			drift = (AbsoluteTime_to_scalar(&shmem->vblTime) 
					 + inst->__private->actualVBLCount * calculatedDelta);
			drift = (drift < _now) ? (_now - drift) : (drift - _now);
			shmem->vblDrift = drift;
			shmem->vblDeltaMeasured = (AbsoluteTime_to_scalar(&delta) / inst->__private->actualVBLCount);
//   		shmem->vblDeltaMeasured = IOMappedRead32(0xb0070000);
		}
	}
	else
	{
		shmem->vblCount++;
		shmem->vblDelta = delta;
	}
    shmem->vblTime  = now;
	inst->__private->actualVBLCount = 0;

    KERNEL_DEBUG(0xc000030 | DBG_FUNC_NONE,
                 (uint32_t)(AbsoluteTime_to_scalar(&shmem->vblDelta) >> 32),
                 (uint32_t)(AbsoluteTime_to_scalar(&shmem->vblDelta)), 0, 0, 0);

    if (inst->vblSemaphore)
        semaphore_signal_all(inst->vblSemaphore);

	if (inst->__private->vblThrottle)
	{
		inst->__private->vblEnabled = false;
        if (!gIOFBVBLDrift) inst->__private->deferredVBLDisableEvent->interruptOccurred(0, 0, 0);
		inst->__private->vblUpdateTimer->setTimeoutMS(kVBLThrottleTimeMS);
	}
	else if (inst->__private->deferredCLUTSetEvent 
           && (inst->__private->gammaNeedSet || inst->__private->clutDataLen || inst->__private->waitVBLEvent))
	{
        inst->__private->deferredCLUTSetEvent->interruptOccurred(0, 0, 0);
	}
}

void IOFramebuffer::resetCursor( void )
{
    StdFBShmem_t *      shmem;
    int                 frame;

    shmem = GetShmem(this);
    //    hwCursorLoaded = false;
    if (!shmem)
        return;

    shmem->hardwareCursorActive = 0;
    frame = shmem->frame;
    shmem->frame = frame ^ 1;
    showCursor( &shmem->cursorLoc, frame );
}

void IOFramebuffer::getVBLTime( AbsoluteTime * time, AbsoluteTime * delta )
{
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if (shmem)
    {
		getTimeOfVBL(time, 0);
        *delta = shmem->vblDelta;
    }
    else
    {
        AbsoluteTime_to_scalar(time) = 0;
        AbsoluteTime_to_scalar(delta) = 0;
    }
}

void IOFramebuffer::getBoundingRect( IOGBounds ** bounds )
{
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if (NULL == shmem)
        *bounds = NULL;
    else
        *bounds = &__private->screenBounds[0];
}

//
// END:         Implementation of the evScreen protocol
//

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::getNotificationSemaphore(
    IOSelect interruptType, semaphore_t * semaphore )
{
    kern_return_t       kr;
    semaphore_t         sema;

    if (interruptType != kIOFBVBLInterruptType)
        return (kIOReturnUnsupported);

    if (!haveVBLService)
        return (kIOReturnNoResources);

    if (MACH_PORT_NULL == vblSemaphore)
    {
        kr = semaphore_create(kernel_task, &sema, SYNC_POLICY_FIFO, 0);
        if (kr == KERN_SUCCESS)
            vblSemaphore = sema;
    }
    else
        kr = KERN_SUCCESS;

    if (kr == KERN_SUCCESS)
        *semaphore = vblSemaphore;

    return (kr);
}

IOReturn IOFramebuffer::extSetCursorVisible(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst    = (IOFramebuffer *) target;
    bool            visible = args->scalarInput[0];

    IOReturn            err;
    IOGPoint *          hs;
    StdFBShmem_t *      shmem;

    if ((err = inst->extEntry(true)))
        return (err);

    shmem = GetShmem(inst);
    if (shmem->hardwareCursorActive && inst->__private->online)
    {
        hs = &shmem->hotSpot[0 != shmem->frame];
        err = inst->_setCursorState(
                  shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
                  shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny,
                  visible );

        if (inst->__private->cursorToDo)
            KICK_CURSOR(inst->__private->cursorThread);
    }
    else
        err = kIOReturnBadArgument;

    inst->extExit(err);

    return (err);
}

IOReturn IOFramebuffer::extSetCursorPosition(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
//    IOFramebuffer * inst = (IOFramebuffer *) target;
//    SInt32          x    = args->scalarInput[0];
//    SInt32          y    = args->scalarInput[1];

    return (kIOReturnUnsupported);
}

void IOFramebuffer::transformCursor( StdFBShmem_t * shmem, IOIndex frame )
{
    void *                    buf;
    unsigned int * out;
    volatile unsigned int   * cursPtr32 = 0;
    volatile unsigned short * cursPtr16 = 0;
    SInt32 x, y, dw, dh, sx, sy, sw, sh;

    if (__private->cursorBytesPerPixel == 4)
    {
        cursPtr32 = (volatile unsigned int *) __private->cursorImages[frame];
        cursPtr16 = 0;
    }
    else if (__private->cursorBytesPerPixel == 2)
    {
        cursPtr32 = 0;
        cursPtr16 = (volatile unsigned short *) __private->cursorImages[frame];
    }

    sw = shmem->cursorSize[0 != frame].width;
    sh = shmem->cursorSize[0 != frame].height;

    if (kIOFBSwapAxes & __private->transform)
    {
        dw = sh;
        dh = sw;
    }
    else
    {
        dw = sw;
        dh = sh;
    }

    buf = IOMalloc(dw * dh * __private->cursorBytesPerPixel);
    out = (unsigned int *) buf;

    for (y = 0; y < dh; y++)
    {
        for (x = 0; x < dw; x++)
        {
            if (kIOFBSwapAxes & __private->transform)
            {
                sx = y;
                sy = x;
            }
            else
            {
                sx = x;
                sy = y;
            }
            if (__private->transform & kIOFBInvertY)
                sx = sw - sx - 1;
            if (__private->transform & kIOFBInvertX)
                sy = sh - sy - 1;

            if (cursPtr32)
                *out++ = cursPtr32[sx + sy * sw];
            else
                STOREINC(out, cursPtr16[sx + sy * sw], UInt16)
        }
    }

    bcopy(buf, (void *) __private->cursorImages[frame], dw * dh * __private->cursorBytesPerPixel);
    IOFree(buf, dw * dh * __private->cursorBytesPerPixel);

    shmem->cursorSize[0 != frame].width  = dw;
    shmem->cursorSize[0 != frame].height = dh;

    if (kIOFBSwapAxes & __private->transform)
    {
        x = shmem->hotSpot[0 != frame].y;
        y = shmem->hotSpot[0 != frame].x;
    }
    else
    {
        x = shmem->hotSpot[0 != frame].x;
        y = shmem->hotSpot[0 != frame].y;
    }
    if (__private->transform & kIOFBInvertX)
        x = dw - x - 1;
    if (__private->transform & kIOFBInvertY)
        y = dh - y - 1;
    shmem->hotSpot[0 != frame].x = x;
    shmem->hotSpot[0 != frame].y = y;
}

IOReturn IOFramebuffer::extSetNewCursor(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst    = (IOFramebuffer *) target;
    IOIndex         cursor  = args->scalarInput[0];
    IOIndex         frame   = args->scalarInput[1];
    IOOptionBits    options = args->scalarInput[2];

    StdFBShmem_t *      shmem;
    IOReturn            err;
    UInt32              hwCursorActive;
        
    if ((err = inst->extEntry(false)))
        return (err);

    shmem = GetShmem(inst);
    // assumes called with cursorSema held
    if (cursor || options || (((UInt32) frame) >= inst->__private->numCursorFrames))
        err = kIOReturnBadArgument;
    else
    {
        if (kIOFBRotateFlags & inst->__private->transform)
            inst->transformCursor(shmem, frame);

        if ((shmem->cursorSize[0 != frame].width > inst->maxCursorSize.width)
                || (shmem->cursorSize[0 != frame].height > inst->maxCursorSize.height))
            err = kIOReturnBadArgument;

        else if (inst->haveHWCursor)
        {
            if (frame == shmem->frame)
            {
                hwCursorActive = inst->_setCursorImage( frame );
                shmem->hardwareCursorActive = hwCursorActive;
                inst->__private->cursorFlags[frame] = hwCursorActive ? kIOFBCursorHWCapable : 0;
            }
            else
            {
                inst->__private->cursorFlags[frame] = kIOFBCursorImageNew;
            }
            err = kIOReturnSuccess;             // I guess
        }
        else
            err = kIOReturnUnsupported;
    }
    if (inst->__private->cursorToDo)
        KICK_CURSOR(inst->__private->cursorThread);

    inst->extExit(err);

    return (err);
}

bool IOFramebuffer::convertCursorImage( void * cursorImage,
                                        IOHardwareCursorDescriptor * hwDesc,
                                        IOHardwareCursorInfo * hwCursorInfo )
{
    StdFBShmem_t *              shmem = GetShmem(this);
    UInt8 *                     dataOut = hwCursorInfo->hardwareCursorData;
    IOColorEntry *              clut = hwCursorInfo->colorMap;
    UInt32                      maxColors = hwDesc->numColors;
    int                         frame = (uintptr_t) cursorImage;

    volatile unsigned short *   cursPtr16;
    volatile unsigned int *     cursPtr32;
    SInt32                      x, lastx, y, lasty;
    UInt32                      width, height, lineBytes = 0;
    UInt32                      index, numColors = 0;
    UInt32                      alpha, red, green, blue;
    UInt16                      s16;
    UInt32                      s32;
    UInt32                      pixel = 0;
    UInt32                      data = 0;
    UInt32                      bits = 0;
    bool                        ok = true;
    bool                        isDirect;

    if (__private->testingCursor)
    {
        IOHardwareCursorDescriptor copy;

        if ((hwDesc->numColors == 0) && (hwDesc->bitDepth > 8) && (hwDesc->bitDepth < 32))
        {
            copy = *hwDesc;
            hwDesc = &copy;
            copy.bitDepth = 32;
        }

        OSData * data = OSData::withBytes( hwDesc, sizeof(IOHardwareCursorDescriptor) );
        if (data)
        {
            __private->cursorAttributes->setObject( data );
            data->release();
        }

        return (false);
    }
    else if (!hwCursorInfo || !hwCursorInfo->hardwareCursorData)
        return (false);

    assert( frame < __private->numCursorFrames );

    if (__private->cursorBytesPerPixel == 4)
    {
        cursPtr32 = (volatile unsigned int *) __private->cursorImages[frame];
        cursPtr16 = 0;
    }
    else if (__private->cursorBytesPerPixel == 2)
    {
        cursPtr32 = 0;
        cursPtr16 = (volatile unsigned short *) __private->cursorImages[frame];
    }
    else
        return (false);

    x = shmem->cursorSize[0 != frame].width;
    y = shmem->cursorSize[0 != frame].height;
    if ((x > (SInt32) hwDesc->width) || (y > (SInt32) hwDesc->height))
        return (false);
    isDirect = (hwDesc->bitDepth > 8);
    if (isDirect && (hwDesc->bitDepth != 32) && (hwDesc->bitDepth != 16))
        return (false);

    width  = hwDesc->width;
    height = hwDesc->height;

    // matrox workaround - 2979661
    if ((maxColors > 1) && (&clut[1] == (IOColorEntry *) hwCursorInfo))
        width = height = 16;
    // --

    SInt32 adjX = 4 - shmem->hotSpot[0 != frame].x;
    SInt32 adjY = 4 - shmem->hotSpot[0 != frame].y;
    if ((adjX < 0) || ((UInt32)(x + adjX) > width))
        adjX = 0;
    else
        x += adjX;
    if ((adjY < 0) || ((UInt32)(y + adjY) > height))
        adjY = 0;

    __private->cursorHotSpotAdjust[0 != frame].x = adjX;
    __private->cursorHotSpotAdjust[0 != frame].y = adjY;

    while ((width >> 1) >= (UInt32) x)
        width >>= 1;
    while ((UInt32)(height >> 1) >= (UInt32)(y + adjY))
        height >>= 1;

    hwCursorInfo->cursorWidth  = width;
    hwCursorInfo->cursorHeight = height;
    hwCursorInfo->cursorHotSpotX = shmem->hotSpot[0 != frame].x 
                                 + __private->cursorHotSpotAdjust[0 != frame].x;
    hwCursorInfo->cursorHotSpotY = shmem->hotSpot[0 != frame].y 
                                 + __private->cursorHotSpotAdjust[0 != frame].y;
    lastx = x - width - 1;

    if (isDirect && adjY)
    {
        lineBytes = width * (hwDesc->bitDepth >> 3);
        // top lines
        bzero_nc( dataOut, adjY * lineBytes );
        dataOut += adjY * lineBytes;
        // bottom lines
        adjY    = height - shmem->cursorSize[0 != frame].height - adjY;
        lasty   = -1;
    }
    else
    {
        y += adjY;
        lasty = y - height - 1;
    }

    while (ok && (--y != lasty))
    {
        x = shmem->cursorSize[0 != frame].width + adjX;
        while (ok && (--x != lastx))
        {
            if ((x < 0)
                    || (y < 0)
                    || (x >= shmem->cursorSize[0 != frame].width)
                    || (y >= shmem->cursorSize[0 != frame].height))
                alpha = red = green = blue = 0;

            else if (cursPtr32)
            {
                s32 = *(cursPtr32++);
                alpha = (s32 >> 24) & 0xff;
                red = (s32 >> 16) & 0xff;
                green = (s32 >> 8) & 0xff;
                blue = (s32) & 0xff;
            }
            else
            {
#define RMASK16 0xF000
#define GMASK16 0x0F00
#define BMASK16 0x00F0
#define AMASK16 0x000F
                s16 = *(cursPtr16++);
                alpha = s16 & AMASK16;
                alpha |= (alpha << 4);
                red = (s16 & RMASK16) >> 8;
                red |= (red >> 4);
                green = (s16 & GMASK16) >> 4;
                green |= (green >> 4);
                blue = s16 & BMASK16;
                blue |= (blue >> 4);
            }

            if (isDirect)
            {
                if (alpha == 0)
                {
                    if (0xff == (red & green & blue))
                    {
                        /* Transparent white area.  Invert dst. */
                        if (kInvertingEncodedPixel
                                & hwDesc->supportedSpecialEncodings)
                            pixel = hwDesc->specialEncodings[kInvertingEncoding];
                        else
                            ok = false;
                    }
                    else
                        pixel = 0;

                    if (hwDesc->bitDepth == 32)
                        STOREINC(dataOut, pixel, UInt32)
                    else
                        STOREINC(dataOut, pixel, UInt16)
                }
                else
                {
                    if (0xff != alpha)
                    {
                        red   = 0xff * red   / alpha;
                        green = 0xff * green / alpha;
                        blue  = 0xff * blue  / alpha;
                    }
                    if (hwDesc->bitDepth == 32)
                    {
                        pixel =   (alpha << 24)
                                  | ((red   & 0xff) << 16)
                                  | ((green & 0xff) << 8)
                                  | (blue   & 0xff);

                        STOREINC(dataOut, pixel, UInt32)
                    }
                    else
                    {
                        pixel =   ((alpha & 0xf0) << 8)
                                  | ((red   & 0xf0) << 4)
                                  | ((green & 0xf0) << 0)
                                  | ((blue  & 0xf0) >> 4);

                        STOREINC(dataOut, pixel, UInt16)
                    }
                }
            }
            else
            {
                /* Indexed pixels */

                if (alpha == 0)
                {
                    if (0 == (red | green | blue))
                    {
                        /* Transparent black area.  Leave dst as is. */
                        if (kTransparentEncodedPixel
                                & hwDesc->supportedSpecialEncodings)
                            pixel = hwDesc->specialEncodings[kTransparentEncoding];
                        else
                            ok = false;
                    }
                    else if (0xff == (red & green & blue))
                    {
                        /* Transparent white area.  Invert dst. */
                        if (kInvertingEncodedPixel
                                & hwDesc->supportedSpecialEncodings)
                            pixel = hwDesc->specialEncodings[kInvertingEncoding];
                        else
                            ok = false;
                    }
                    else
                        ok = false;
                }
                else if (alpha == 0xff)
                {
                    red   |= (red << 8);
                    green |= (green << 8);
                    blue  |= (blue << 8);

                    /* Opaque cursor pixel.  Mark it. */
                    for (index = 0; index < numColors; index++)
                    {
                        if ((red   == clut[index].red)
                                && (green == clut[index].green)
                                && (blue  == clut[index].blue))
                        {
                            pixel = clut[index].index;
                            break;
                        }
                    }
                    if (index == numColors)
                    {
                        ok = (numColors < maxColors);
                        if (ok)
                        {
                            pixel = hwDesc->colorEncodings[numColors++];
                            clut[index].red   = red;
                            clut[index].green = green;
                            clut[index].blue  = blue;
                            clut[index].index = pixel;
                        }
                    }
                }
                else
                {
                    /* Alpha is not 0 or 1.0.  Sover the cursor. */
                    ok = false;
                    break;
                }
                data <<= hwDesc->bitDepth;
                data |= pixel;
                bits += hwDesc->bitDepth;
                if (0 == (bits & 31))
                {
                    OSWriteBigInt32(dataOut, 0, data);
                    dataOut += sizeof(UInt32);
                }
            }
        } /* x */
    } /* y */

    if (ok && isDirect && adjY)
    {
        // bottom lines
        bzero_nc( dataOut, adjY * lineBytes );
        dataOut += adjY * lineBytes;
    }

    __private->cursorClutDependent = (ok && !isDirect);

#if 0
    if (ok)
    {
        static UInt32 lastWidth;
        static UInt32 lastHeight;

        if ((width != lastWidth) || (height != lastHeight))
        {
            lastWidth = width;
            lastHeight = height;
            IOLog("[%d,%d]", width, height);
        }

        if (((UInt32)(dataOut - hwCursorInfo->hardwareCursorData)
                != ((hwCursorInfo->cursorHeight * hwCursorInfo->cursorWidth * hwDesc->bitDepth) >> 3)))
            IOLog("dataOut %p, %p @ %d\n", dataOut, hwCursorInfo->hardwareCursorData, hwDesc->bitDepth );
    }
#endif

    return (ok);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Apple standard 8-bit CLUT

#if 0
UInt8 appleClut8[256 * 3] =
{
    // 00
    0xFF,0xFF,0xFF, 0xFF,0xFF,0xCC,     0xFF,0xFF,0x99, 0xFF,0xFF,0x66,
    0xFF,0xFF,0x33, 0xFF,0xFF,0x00,     0xFF,0xCC,0xFF, 0xFF,0xCC,0xCC,
    0xFF,0xCC,0x99, 0xFF,0xCC,0x66,     0xFF,0xCC,0x33, 0xFF,0xCC,0x00,
    0xFF,0x99,0xFF, 0xFF,0x99,0xCC,     0xFF,0x99,0x99, 0xFF,0x99,0x66,
    // 10
    0xFF,0x99,0x33, 0xFF,0x99,0x00,     0xFF,0x66,0xFF, 0xFF,0x66,0xCC,
    0xFF,0x66,0x99, 0xFF,0x66,0x66,     0xFF,0x66,0x33, 0xFF,0x66,0x00,
    0xFF,0x33,0xFF, 0xFF,0x33,0xCC,     0xFF,0x33,0x99, 0xFF,0x33,0x66,
    0xFF,0x33,0x33, 0xFF,0x33,0x00,     0xFF,0x00,0xFF, 0xFF,0x00,0xCC,
    // 20
    0xFF,0x00,0x99, 0xFF,0x00,0x66,     0xFF,0x00,0x33, 0xFF,0x00,0x00,
    0xCC,0xFF,0xFF, 0xCC,0xFF,0xCC,     0xCC,0xFF,0x99, 0xCC,0xFF,0x66,
    0xCC,0xFF,0x33, 0xCC,0xFF,0x00,     0xCC,0xCC,0xFF, 0xCC,0xCC,0xCC,
    0xCC,0xCC,0x99, 0xCC,0xCC,0x66,     0xCC,0xCC,0x33, 0xCC,0xCC,0x00,
    // 30
    0xCC,0x99,0xFF, 0xCC,0x99,0xCC,     0xCC,0x99,0x99, 0xCC,0x99,0x66,
    0xCC,0x99,0x33, 0xCC,0x99,0x00,     0xCC,0x66,0xFF, 0xCC,0x66,0xCC,
    0xCC,0x66,0x99, 0xCC,0x66,0x66,     0xCC,0x66,0x33, 0xCC,0x66,0x00,
    0xCC,0x33,0xFF, 0xCC,0x33,0xCC,     0xCC,0x33,0x99, 0xCC,0x33,0x66,
    // 40
    0xCC,0x33,0x33, 0xCC,0x33,0x00,     0xCC,0x00,0xFF, 0xCC,0x00,0xCC,
    0xCC,0x00,0x99, 0xCC,0x00,0x66,     0xCC,0x00,0x33, 0xCC,0x00,0x00,
    0x99,0xFF,0xFF, 0x99,0xFF,0xCC,     0x99,0xFF,0x99, 0x99,0xFF,0x66,
    0x99,0xFF,0x33, 0x99,0xFF,0x00,     0x99,0xCC,0xFF, 0x99,0xCC,0xCC,
    // 50
    0x99,0xCC,0x99, 0x99,0xCC,0x66,     0x99,0xCC,0x33, 0x99,0xCC,0x00,
    0x99,0x99,0xFF, 0x99,0x99,0xCC,     0x99,0x99,0x99, 0x99,0x99,0x66,
    0x99,0x99,0x33, 0x99,0x99,0x00,     0x99,0x66,0xFF, 0x99,0x66,0xCC,
    0x99,0x66,0x99, 0x99,0x66,0x66,     0x99,0x66,0x33, 0x99,0x66,0x00,
    // 60
    0x99,0x33,0xFF, 0x99,0x33,0xCC,     0x99,0x33,0x99, 0x99,0x33,0x66,
    0x99,0x33,0x33, 0x99,0x33,0x00,     0x99,0x00,0xFF, 0x99,0x00,0xCC,
    0x99,0x00,0x99, 0x99,0x00,0x66,     0x99,0x00,0x33, 0x99,0x00,0x00,
    0x66,0xFF,0xFF, 0x66,0xFF,0xCC,     0x66,0xFF,0x99, 0x66,0xFF,0x66,
    // 70
    0x66,0xFF,0x33, 0x66,0xFF,0x00,     0x66,0xCC,0xFF, 0x66,0xCC,0xCC,
    0x66,0xCC,0x99, 0x66,0xCC,0x66,     0x66,0xCC,0x33, 0x66,0xCC,0x00,
    0x66,0x99,0xFF, 0x66,0x99,0xCC,     0x66,0x99,0x99, 0x66,0x99,0x66,
    0x66,0x99,0x33, 0x66,0x99,0x00,     0x66,0x66,0xFF, 0x66,0x66,0xCC,
    // 80
    0x66,0x66,0x99, 0x66,0x66,0x66,     0x66,0x66,0x33, 0x66,0x66,0x00,
    0x66,0x33,0xFF, 0x66,0x33,0xCC,     0x66,0x33,0x99, 0x66,0x33,0x66,
    0x66,0x33,0x33, 0x66,0x33,0x00,     0x66,0x00,0xFF, 0x66,0x00,0xCC,
    0x66,0x00,0x99, 0x66,0x00,0x66,     0x66,0x00,0x33, 0x66,0x00,0x00,
    // 90
    0x33,0xFF,0xFF, 0x33,0xFF,0xCC,     0x33,0xFF,0x99, 0x33,0xFF,0x66,
    0x33,0xFF,0x33, 0x33,0xFF,0x00,     0x33,0xCC,0xFF, 0x33,0xCC,0xCC,
    0x33,0xCC,0x99, 0x33,0xCC,0x66,     0x33,0xCC,0x33, 0x33,0xCC,0x00,
    0x33,0x99,0xFF, 0x33,0x99,0xCC,     0x33,0x99,0x99, 0x33,0x99,0x66,
    // a0
    0x33,0x99,0x33, 0x33,0x99,0x00,     0x33,0x66,0xFF, 0x33,0x66,0xCC,
    0x33,0x66,0x99, 0x33,0x66,0x66,     0x33,0x66,0x33, 0x33,0x66,0x00,
    0x33,0x33,0xFF, 0x33,0x33,0xCC,     0x33,0x33,0x99, 0x33,0x33,0x66,
    0x33,0x33,0x33, 0x33,0x33,0x00,     0x33,0x00,0xFF, 0x33,0x00,0xCC,
    // b0
    0x33,0x00,0x99, 0x33,0x00,0x66,     0x33,0x00,0x33, 0x33,0x00,0x00,
    0x00,0xFF,0xFF, 0x00,0xFF,0xCC,     0x00,0xFF,0x99, 0x00,0xFF,0x66,
    0x00,0xFF,0x33, 0x00,0xFF,0x00,     0x00,0xCC,0xFF, 0x00,0xCC,0xCC,
    0x00,0xCC,0x99, 0x00,0xCC,0x66,     0x00,0xCC,0x33, 0x00,0xCC,0x00,
    // c0
    0x00,0x99,0xFF, 0x00,0x99,0xCC,     0x00,0x99,0x99, 0x00,0x99,0x66,
    0x00,0x99,0x33, 0x00,0x99,0x00,     0x00,0x66,0xFF, 0x00,0x66,0xCC,
    0x00,0x66,0x99, 0x00,0x66,0x66,     0x00,0x66,0x33, 0x00,0x66,0x00,
    0x00,0x33,0xFF, 0x00,0x33,0xCC,     0x00,0x33,0x99, 0x00,0x33,0x66,
    // d0
    0x00,0x33,0x33, 0x00,0x33,0x00,     0x00,0x00,0xFF, 0x00,0x00,0xCC,
    0x00,0x00,0x99, 0x00,0x00,0x66,     0x00,0x00,0x33, 0xEE,0x00,0x00,
    0xDD,0x00,0x00, 0xBB,0x00,0x00,     0xAA,0x00,0x00, 0x88,0x00,0x00,
    0x77,0x00,0x00, 0x55,0x00,0x00,     0x44,0x00,0x00, 0x22,0x00,0x00,
    // e0
    0x11,0x00,0x00, 0x00,0xEE,0x00,     0x00,0xDD,0x00, 0x00,0xBB,0x00,
    0x00,0xAA,0x00, 0x00,0x88,0x00,     0x00,0x77,0x00, 0x00,0x55,0x00,
    0x00,0x44,0x00, 0x00,0x22,0x00,     0x00,0x11,0x00, 0x00,0x00,0xEE,
    0x00,0x00,0xDD, 0x00,0x00,0xBB,     0x00,0x00,0xAA, 0x00,0x00,0x88,
    // f0
    0x00,0x00,0x77, 0x00,0x00,0x55,     0x00,0x00,0x44, 0x00,0x00,0x22,
    0x00,0x00,0x11, 0xEE,0xEE,0xEE,     0xDD,0xDD,0xDD, 0xBB,0xBB,0xBB,
    0xAA,0xAA,0xAA, 0x88,0x88,0x88,     0x77,0x77,0x77, 0x55,0x55,0x55,
    0x44,0x44,0x44, 0x22,0x22,0x22,     0x11,0x11,0x11, 0x00,0x00,0x00
};
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifdef __ppc__
extern
#endif
    int killprint;
extern "C"
{
    int kmputc( int c );
}

#if DOANIO

#warning ** DO AN IO **

static unsigned long doaniobuf[256];

static void doanio( void )
{
    struct uio uio;
    struct iovec iovec;
    int err;
    dev_t device = makedev( 14, 0 );

    iovec.iov_base = (char *) &doaniobuf[0];
    iovec.iov_len  = 1024;

    uio.uio_iov = &iovec;
    uio.uio_iovcnt = 1;
    uio.uio_rw = UIO_READ;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_offset = 0;
    uio.uio_resid = 1024;

    DEBG("", "\n");
    err = ((*cdevsw[major(device)].d_read)(device, &uio, 0));
    DEBG("", " done(%08lx)\n", doaniobuf[0]);
}

#endif

IOReturn IOFramebuffer::deliverDisplayModeDidChangeNotification( void )
{
    IOReturn
    ret = deliverFramebufferNotification( kIOFBNotifyDisplayModeDidChange );

    if (__private->lastNotifyOnline != __private->online)
    {
        __private->lastNotifyOnline = __private->online;
        deliverFramebufferNotification( kIOFBNotifyOnlineChange, (void *) __private->online);
    }

    return (ret);
}

void IOFramebuffer::saveFramebuffer(void)
{
#if VRAM_SAVE
	// vram content is being lost
	uintptr_t           value;
	if (true
			&& !dead
			&& !__private->saveLength
			&& __private->online
			&& __private->framebufferHeight
			&& __private->framebufferWidth
			&& rowBytes
			&& (kIOReturnSuccess == getAttribute(kIOVRAMSaveAttribute, &value)) 
			&& value)
	{
		vm_size_t sLen;
		sLen = __private->framebufferHeight * rowBytes;

		/*
		* dLen should account for possible growth. (e.g. run-length encoding noise)
		*       Add 5 bytes for header,
		*       12% for RLE growth
		*       2 bytes per line for line spans,
		*       1 additional escape code byte for trailing pixel in each line
		*/
#if VRAM_COMPRESS
		vm_size_t dLen;
		uint32_t  idx;

		dLen = 5 + sLen + ((sLen + 7) >> 3) + (__private->framebufferHeight * 3) + rowBytes;

		for (idx = 0;
			 (idx < kIOPreviewImageCount) && __private->saveBitsMD[idx];
			 idx++)	{}

		dLen *= (idx + 1);
		DEBG1(thisName, " dLen 0x%x\n", (int) dLen);

		dLen = round_page(dLen);
		__private->saveLength = dLen;
#else
		__private->saveLength = round_page(sLen);
#endif
		__private->saveFramebuffer = IOMallocPageable( __private->saveLength, page_size );
		if (!__private->saveFramebuffer)
			__private->saveLength = 0;
		else
		{
#if VRAM_COMPRESS
			UInt8 *       bits[kIOPreviewImageCount] = { 0 };
			IOByteCount   bitsLen = 0;
			IOMemoryMap * map[kIOPreviewImageCount] = { 0 };

			for (idx = 0;
				 (idx < kIOPreviewImageCount) && __private->saveBitsMD[idx];
				 idx++)
			{
				map[idx] = __private->saveBitsMD[idx]->map(kIOMapReadOnly);
				if (map[idx])
				{
					bits[idx] = (UInt8 *) map[idx]->getVirtualAddress();
					if (!idx) bitsLen = map[idx]->getLength();
					else if (bitsLen != map[idx]->getLength())
					{
						bits[idx] = NULL;
						map[idx]->release();
						map[idx] = 0;
					}
				}
				__private->saveBitsMD[idx]->release();
				__private->saveBitsMD[idx] = 0;
			}

			if (!bits[0])
			{
				IOLog("%s: no save bits\n", thisName);
				dLen = 0;
			}
			else if ((((__private->framebufferHeight - 1) * rowBytes)
				 + __private->framebufferWidth * bytesPerPixel) > bitsLen)
			{
				IOLog("%s: bad pixel parameters %d x %d x %d > 0x%x\n", thisName,
				 (int) __private->framebufferWidth, (int) __private->framebufferHeight, (int) rowBytes, (int) bitsLen);
				dLen = 0;
			}
			else
			{
				uint8_t * gammaData = __private->gammaData;
				if (gammaData) gammaData += __private->gammaHeaderSize;
				dLen = CompressData( bits, kIOPreviewImageCount, bytesPerPixel,
									 __private->framebufferWidth, __private->framebufferHeight, rowBytes,
									 (UInt8 *) __private->saveFramebuffer, __private->saveLength,
									 __private->gammaChannelCount, __private->gammaDataCount, 
									__private->gammaDataWidth, gammaData);
			}
			DEBG1(thisName, " compressed to %d%%\n", (int) ((dLen * 100) / sLen));

			for (idx = 0; (idx < kIOPreviewImageCount); idx++) 
			{
				if (map[idx]) map[idx]->release();
			}

			dLen = round_page( dLen );
			if (__private->saveLength > dLen)
			{
				IOFreePageable( (void *) (((uintptr_t) __private->saveFramebuffer) + dLen),
								__private->saveLength - dLen );
				__private->saveLength = dLen;
			}
#else
			bcopy_nc( (void *) frameBuffer, __private->saveFramebuffer, sLen );
#endif
			if (__private->saveLength)
			{
#if RLOG
				kern_return_t kr = 
#endif
				vm_map_wire( IOPageableMapForAddress( (vm_address_t) __private->saveFramebuffer ),
							 (vm_address_t) __private->saveFramebuffer,
							 ((vm_address_t) __private->saveFramebuffer) + __private->saveLength,
							 VM_PROT_READ | VM_PROT_WRITE, FALSE );
				DEBG(thisName, " vm_map_wire(%x)\n", kr);

				if (this == gIOFBConsoleFramebuffer)
				{
					IOMemoryDescriptor *
					previewBuffer = IOMemoryDescriptor::withAddress(
										__private->saveFramebuffer, 
										__private->saveLength, 
										kIODirectionInOut);
					if (previewBuffer)
					{
						getPMRootDomain()->setProperty(kIOHibernatePreviewBufferKey, previewBuffer);
						previewBuffer->release();

						OSNumber * num;
						if (false 
						 && (num = OSDynamicCast(OSNumber, getPMRootDomain()->getProperty(kIOHibernateModeKey))) 
						 && (kIOHibernateModeOn == ((kIOHibernateModeSleep | kIOHibernateModeOn) & num->unsigned32BitValue()))
						 && !gIOFBCurrentClamshellState)
						{
							// hibernate enabled, will power off, clamshell open - do preview
							UInt32 flags = 0;
							getProvider()->setProperty(kIOHibernatePreviewActiveKey, &flags, sizeof(flags));
							PreviewDecompressData(&__private->saveFramebuffer, 0, (void *)frameBuffer, 
													__private->framebufferWidth, __private->framebufferHeight,
													bytesPerPixel, rowBytes);
						}
					}
				}
			}
		}
	}
#endif /* VRAM_SAVE */
}

IOReturn IOFramebuffer::restoreFramebuffer(IOIndex event)
{
	IOReturn ret = kIOReturnNotReady;

#if VRAM_SAVE
	// restore vram content
	if (__private->saveLength)
	{
		thread_t saveThread = NULL;

		if (kIOFBNotifyVRAMReady == event)
		{
			IOMemoryDescriptor * vram;
			if ((vram = getVRAMRange()))
			{
				vram->redirect( kernel_task, false );
				vram->release();
			}
		}
		else
		{
			saveThread = __private->controller->powerThread;
			__private->controller->powerThread = current_thread();
			setAttribute(kIOFBSpeedAttribute, kIOFBVRAMCompressSpeed);
		}

#if VRAM_COMPRESS
		uint32_t image = (kOSBooleanTrue == 
			IORegistryEntry::getRegistryRoot()->getProperty(kIOConsoleLockedKey));
		if (image >= kIOPreviewImageCount) image = 0;
		DecompressData((UInt8 *) __private->saveFramebuffer, image, (UInt8 *) frameBuffer,
						0, 0, __private->framebufferWidth, __private->framebufferHeight, rowBytes);
#else
		bcopy_nc( __private->saveFramebuffer, (void *) frameBuffer, __private->saveLength );
#endif
		DEBG1(thisName, " screen drawn\n");
		__private->saveGammaPending = true;

		if (kIOFBNotifyVRAMReady != event)
		{
			setAttribute(kIOFBSpeedAttribute, __private->reducedSpeed);
			__private->controller->powerThread = saveThread;
		}

		ret = kIOReturnSuccess;
	}

	if ((kIOFBNotifyVRAMReady != event) && __private->saveGammaPending)
	{
		if (__private->gammaDataLen && __private->gammaData && !__private->scaledMode)
		{
			DEBG1(thisName, " set gamma\n");
			setGammaTable( __private->gammaChannelCount, __private->gammaDataCount, 
							__private->gammaDataWidth, __private->gammaData );
		}
		__private->saveGammaPending = false;
	}
#endif /* VRAM_SAVE */

    return (ret);
}

IOReturn IOFramebuffer::handleEvent( IOIndex event, void * info )
{
    IOReturn ret = kIOReturnSuccess;
	bool     sendEvent = true;

    DEBG1(thisName, "(%d, %d)\n", (uint32_t) event, 
    		current_thread() != __private->controller->powerThread);

    if (current_thread() != __private->controller->powerThread)
    {
		sendEvent = false;
    }
    else switch (event)
    {
        case kIOFBNotifyWillSleep:
            if (!info)
				break;

            if (this == gIOFBConsoleFramebuffer)
            {
//              getPlatform()->setConsoleInfo( 0, kPEDisableScreen);
                killprint = 1;
            }
			// notification sent early at system sleep
			sendEvent = false;
			ret = kIOReturnSuccess;
            break;

        case kIOFBNotifyDidWake:
            if (!info)
				break;

			restoreFramebuffer(event);

            if (this == gIOFBConsoleFramebuffer)
            {
//              getPlatform()->setConsoleInfo( 0, kPEEnableScreen);
                killprint = 0;
                kmputc( 033 );
                kmputc( 'c' );
            }

            sleepConnectCheck = true;

#if DOANIO
            doanio();
#endif
			pagingState = true;
		    __private->actualVBLCount = 0;
			updateVBL(this, NULL);
			ret = deliverFramebufferNotification(kIOFBNotifyDidWake, (void *) true);
			ret = deliverFramebufferNotification(kIOFBNotifyDidPowerOn, (void *) false);
			info = (void *) false;
			break;

        case kIOFBNotifyDidPowerOn:
			pagingState = true;
			if (sleepConnectCheck)
				sendEvent = false;
			ret = kIOReturnSuccess;
            break;

        case kIOFBNotifyVRAMReady:
			ret = restoreFramebuffer(event);
			sendEvent = false;
            break;
    }

	if (sendEvent)
		ret = deliverFramebufferNotification(event, info);

    return (ret);
}

IOReturn IOFramebuffer::notifyServer( UInt8 state )
{
    mach_msg_header_t * msgh = (mach_msg_header_t *) serverMsg;
    IOReturn            err = kIOReturnSuccess;

    if (serverNotified != state)
    {
		if (!gChosenEntry) gChosenEntry = IORegistryEntry::fromPath("/chosen", gIODTPlane);
		if (gChosenEntry && !state) gChosenEntry->removeProperty(kIOScreenLockStateKey);
		if (state)
		{
			if (this == gIOFBConsoleFramebuffer)
			{
				getPMRootDomain()->removeProperty(kIOHibernatePreviewBufferKey);
				getProvider()->removeProperty(kIOHibernatePreviewActiveKey);
			}
			if (__private->saveFramebuffer)
			{
				IOFreePageable( __private->saveFramebuffer, __private->saveLength );
				__private->saveFramebuffer = 0;
				__private->saveLength      = 0;
			}
		}

        serverNotified = state;

        DEBG1(thisName, "(%p, wait %d, %d->%d)\n", msgh->msgh_remote_port,
             __private->controller->wsWait, serverState, serverNotified);

        msgh->msgh_id = state;
        if ((MACH_PORT_NULL == msgh->msgh_remote_port)
                || (KERN_SUCCESS != mach_msg_send_from_kernel(msgh, msgh->msgh_size)))
        {
			// failed to send
            serverState = serverNotified;
        }
        else
        {
			// start ack timeout
			gIOFBServerAckTimer->setTimeoutMS(kServerAckTimeout * 1000);
        }
    }

    if (serverNotified)
	{
		// wakeup is not gate specific
    	FBWL(this)->wakeupGate(&serverConnect, true);
	}

    return (err);
}

bool IOFramebuffer::getIsUsable( void )
{
    return (dead || (0 != isUsable));
}

IOReturn IOFramebuffer::postWake(void)
{
    IOReturn  ret;
    uintptr_t value;
    bool      probeDP;

	DEBG1(thisName, " post wake from sleep %d\n", sleepConnectCheck);

    probeDP = (__private->dpDongle && !sleepConnectCheck);

    ret = getAttributeForConnection(0, kConnectionPostWake, &value);

    if (captured)
		gIOFBProbeCaptured = true;
	else
    {
        getAttributeForConnection(0, kConnectionChanged, 0);
        if (probeDP)
        {
#if RLOG
            IOReturn probeErr = 
#endif
            setAttributeForConnection(0, kConnectionProbe, kIOFBUserRequestProbe);
            DEBG(thisName, " dp probe wake result %x\n", probeErr);
        }
    }

	__private->controller->postWakeChange = __private->controller->connectChange;
//	gIOFBLastReadClamshellState = gIOFBCurrentClamshellState;

	resetClamshell();
	sleepConnectCheck = false;

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::pmSettingsChange(OSObject * target, const OSSymbol * type,
                                         OSObject * val, uintptr_t refcon)
{
    if (type == gIOFBPMSettingDisplaySleepUsesDimKey)
    {
        OSBitOrAtomic(kIOFBEventDisplayDimsSetting, &gIOFBGlobalEvents);
		startThread(false);
    }
    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::startAsync(IOFBController * controller, uint32_t asyncWork)
{
	if (!gIOFBSystemWorkLoop->inGate()) panic("!sys");
	if (!controller->wl->inGate())      panic("!gate\n");
	if (controller->asyncWork) 			panic("asyncWork");
	if (controller->didWork) 			panic("didWork");

	controller->asyncWork = asyncWork;
	controller->workES->interruptOccurred(0, 0, 0);
}

// system WL

enum
{
    kIOFBDidWork 			= 0x00000001,
    kIOFBWorking  			= 0x00000002,
    kIOFBPaging   			= 0x00000004,
    kIOFBWsWait   			= 0x00000008,
    kIOFBDimmed   			= 0x00000010,
    kIOFBServerSlept 		= 0x00000020,	// any fb ws notified asleep
    kIOFBServerUp           = 0x00000040,	// any fb ws state awake
    kIOFBServerDown			= 0x00000080,	// any fb ws state asleep
    kIOFBCaptured 			= 0x00000100,
    kIOFBDimDisable 		= 0x00000200,
    kIOFBDisplaysChanging 	= 0x00001000
};

enum
{
	kWorkStateChange = 0x00000001,
	kWorkPower       = 0x00000002,
	kWorkSuspend     = 0x00000004,
};

void IOFramebuffer::systemWork(OSObject * owner,
                               IOInterruptEventSource * evtSrc, int intCount)
{
    UInt32 events;
    uint32_t	        index;
    IOFBController *    controller;
    IOFramebuffer *     fb;
    uint32_t            allState;
    
	allState = 0;

	controller = gIOFBAllControllers;
	if (controller) do
	{
		allState |= controllerState(controller);
		controller = controller->nextController;
	}
	while (controller != gIOFBAllControllers);

    DEBG1("S1", " state 0x%x\n", allState);

	controller = gIOFBAllControllers;
	if (controller) do
	{
		if (controller->needsWork)
		{
			uint32_t state = allState;
			DEBG1(controller->name, " working %x\n", state);

			FCLOCK(controller);
			if (!controller->didWork)
			{
				state |= checkPowerWork(controller, state);
			
				if (kIOFBServerSlept & state)
				{
					if ((controller->lastFinishedChange != controller->connectChange)
							 && !controller->fbs[0]->messaged
							 && gIOFBSwitching)
					{
						IODisplayWrangler::activityChange(controller->fbs[0]);
					}
				}
				else
				{
					state |= checkConnectionWork(controller, state);
					controller->needsWork = false;
				}
			}
			FCUNLOCK(controller);
		}
		controller = controller->nextController;
	}
	while (controller != gIOFBAllControllers);

    DEBG1("S2", " state 0x%x\n", allState);

	if (kIOFBWorking & allState)
		return;

	if (!(kIOFBWsWait & allState))
	{
		bool wsState = ((gIOFBSystemPower || gIOFBSwitching) && !(kIOFBDimmed & allState));
		if (wsState != gIOFBWSState)
		{
			if (wsState && (kIOFBServerUp & allState))
			{
				DEBG1("S", " notifyServer wait ack\n");
			}
			else
			{
				DEBG1("S", " notifyServer(%d)\n", wsState);
				gIOFBWSState = wsState;
				if (wsState)
					gIOFBPostWakeNeeded = true;
				for (index = 0;
						(fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
						index++)
				{
					FBLOCK(fb);
					fb->notifyServer(wsState);
					FBUNLOCK(fb);
				}
			}
		}
	}

	if (!(kIOFBServerUp & allState) && gIOFBSystemPowerAckTo
		&& !(kIOFBDisplaysChanging & allState)
		&& !gIOFBSwitching) 
	{
		uintptr_t notiArg;
		// tell accelerators to disable, then protect HW
		for(notiArg = 0; notiArg < 2; notiArg++)
		{
			for (index = 0;
				 (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
				 index++)
			{
				FBLOCK(fb);
				fb->deliverFramebufferNotification(kIOFBNotifyWillSleep, (void *) notiArg);
				if (notiArg)
				{
					fb->saveFramebuffer();
					fb->pagingState = false;
				}
				FBUNLOCK(fb);
			}
		}
		
		DEBG("S", " allowPowerChange(%p)\n", gIOFBSystemPowerAckRef);
		
		IOService * ackTo  = gIOFBSystemPowerAckTo;
		void *      ackRef = gIOFBSystemPowerAckRef;
		gIOFBSystemPowerAckTo = 0;
	   
		ackTo->allowPowerChange( (uintptr_t) ackRef );
		
		DEBG("S", " did allowPowerChange()\n");
	}

	if (!(kIOFBServerDown & allState) && gIOFBPostWakeNeeded)
	{
		gIOFBPostWakeNeeded = false;
		for (index = 0;
			 (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
			 index++)
		{
			FBLOCK(fb);
			fb->postWake();
			FBUNLOCK(fb);
		}
	}

	events = gIOFBGlobalEvents;

	if (kIOFBEventCaptureSetting & events)
	{
		bool wasCaptured, wasDimDisable;
		bool newCaptured, newDimDisable;

		OSBitAndAtomic(~kIOFBEventCaptureSetting, &gIOFBGlobalEvents);

		wasCaptured   = (0 != (kIOFBCaptured & allState));
		wasDimDisable = (0 != (kIOFBDimDisable & allState));
		newCaptured   = (0 != (kIOCaptureDisableDisplayChange  & gIOFBCaptureState));
		newDimDisable = (0 != (kIOCaptureDisableDisplayDimming & gIOFBCaptureState));

		for (index = 0;
			 (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
			 index++)
		{
			FBLOCK(fb);
			fb->setCaptured(newCaptured);
			fb->setDimDisable(newDimDisable);
			FBUNLOCK(fb);
		}
		if (wasCaptured != newCaptured) gIOFBGrayValue = newCaptured ? 0 : kIOFBGrayValue;

		if (newDimDisable != wasDimDisable)
		{
			getPMRootDomain()->setAggressiveness(kIOFBCaptureAggressiveness, newDimDisable);
		}

		if (gIOFBProbeCaptured && wasCaptured && !newCaptured)
		{
			gIOFBProbeCaptured = false;
			OSBitOrAtomic(kIOFBEventProbeAll, &gIOFBGlobalEvents);
			startThread(false);
			DEBG1("S", " capt probe all\n");
		}
	}

	if (kIOFBEventDisplayDimsSetting & events)
	{
		OSNumber *  num;
		uintptr_t   value = true;
		OSObject *  obj;
		
		OSBitAndAtomic(~kIOFBEventDisplayDimsSetting, &gIOFBGlobalEvents);
		obj = getPMRootDomain()->copyPMSetting(const_cast<OSSymbol *>
											   (gIOFBPMSettingDisplaySleepUsesDimKey));
		if ((num = OSDynamicCast(OSNumber, obj)))
			value = num->unsigned32BitValue();
		if (obj)
			obj->release();
		
		if (num) for (uint32_t index = 0;
			 (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
			 index++)
		{
			FBLOCK(fb);
			fb->deliverFramebufferNotification(kIOFBNotifyDisplayDimsChange, (void *) value);
			FBUNLOCK(fb);
		}
	}

	if ((kIOFBEventReadClamshell & events) 
		&& !gIOFBSwitching
		&& !(kIOFBWsWait & allState))
	{
		OSObject * clamshellProperty;

		OSBitAndAtomic(~kIOFBEventReadClamshell, &gIOFBGlobalEvents);
		clamshellProperty = gIOResourcesAppleClamshellState;
		if (clamshellProperty)
		{
//			gIOFBLastClamshellState = 
			gIOFBCurrentClamshellState = (kOSBooleanTrue == clamshellProperty);
			DEBG1("S", " clamshell read %d\n", (int) gIOFBCurrentClamshellState);
		
			for (uint32_t index = 0;
					(fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
					index++)
			{
				FBLOCK(fb);
				fb->deliverFramebufferNotification(kIOFBNotifyClamshellChange, clamshellProperty);
				FBUNLOCK(fb);
			}

			bool desktopMode;
			if (gIOFBLidOpenMode)
				desktopMode = gIOFBDesktopModeAllowed && (gIOFBDisplayCount > 0);
			else
				desktopMode = gIOFBDesktopModeAllowed && (gIOFBBacklightDisplayCount <= 0);

			if (desktopMode)
			{
				// lid change, desktop mode
				DEBG1("S", " desktop will reprobe\n");
				resetClamshell();
			}
		}
	}

	if ((kIOFBEventResetClamshell & events) && gIOFBSystemPower 
		&& !(kIOFBWsWait & allState)
		&& !(kIOFBDisplaysChanging & allState)) 
	{
		OSBitAndAtomic(~kIOFBEventResetClamshell, &gIOFBGlobalEvents);

		if ((gIOFBLidOpenMode && (gIOFBCurrentClamshellState != gIOFBLastReadClamshellState))
			// clamshell changed since last probe
		 || (!gIOFBLidOpenMode && (gIOFBBacklightDisplayCount && gIOFBLastReadClamshellState && !gIOFBCurrentClamshellState)))
			// clamshell was closed during last probe, now open => reprobe
		{
			DEBG1("S", " clamshell caused reprobe\n");
			events |= kIOFBEventProbeAll;
			OSBitOrAtomic(kIOFBEventProbeAll, &gIOFBGlobalEvents);
		}
		else
		{
			AbsoluteTime deadline;
			clock_interval_to_deadline(kIOFBClamshellEnableDelayMS, kMillisecondScale, &deadline );
			thread_call_enter1_delayed(gIOFBClamshellCallout,
										(thread_call_param_t) kIOFBEventEnableClamshell, deadline );
		}
	}

	if ((kIOFBEventEnableClamshell & events) && gIOFBSystemPower 
		&& !(kIOFBWsWait & allState)
		&& !(kIOFBDisplaysChanging & allState)) 
	{
		UInt32      change;
		bool        desktopMode;

		OSBitAndAtomic(~kIOFBEventEnableClamshell, &gIOFBGlobalEvents);

		if (gIOFBLidOpenMode)
			desktopMode = gIOFBDesktopModeAllowed && (gIOFBDisplayCount > 0);
		else
			desktopMode = gIOFBDesktopModeAllowed && (gIOFBBacklightDisplayCount <= 0);

		change = kIOPMEnableClamshell | kIOPMSetDesktopMode | (desktopMode ? kIOPMSetValue : 0);
		if (change != gIOFBClamshellState)
		{
			gIOFBClamshellState = change;
			DEBG1("S", " clamshell ena desktopMode %d\n", desktopMode);
			getPMRootDomain()->receivePowerNotification(change);
		}
	}

	if (kIOFBEventDisplaysPowerState & events)
	{
		unsigned long state = IODisplayWrangler::getDisplaysPowerState();

		if (gIOFBSwitching && (state < kIODisplayNumPowerStates))
		{
			state = kIODisplayNumPowerStates;
		}
		else
		{
			OSBitAndAtomic(~kIOFBEventDisplaysPowerState, &gIOFBGlobalEvents);
		}
		DEBG1("S", " displays pstate %ld\n", state);

		for (uint32_t index = 0;
				(fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
				index++)
		{
			FBLOCK(fb);
			if (fb->__private->display)
				fb->__private->display->setDisplayPowerState(state);
			FBUNLOCK(fb);
		}
	}

	if ((kIOFBEventProbeAll & events) 
		&& gIOFBSystemPower 
		&& (kIOMessageSystemHasPoweredOn == gIOFBLastMuxMessage)
		&& !(kIOFBWsWait & allState)
		&& !(kIOFBCaptured & allState)
		&& !(kIOFBDisplaysChanging & allState)) 
	{
		OSBitAndAtomic(~kIOFBEventProbeAll, &gIOFBGlobalEvents);

		DEBG1("S", " probeAll clam %d -> %d\n",
				(int) gIOFBLastClamshellState, (int) gIOFBCurrentClamshellState);

		gIOFBLastClamshellState = gIOFBCurrentClamshellState;
		gIOFBLastReadClamshellState = gIOFBCurrentClamshellState;

		probeAll(kIOFBUserRequestProbe);
		for (uint32_t index = 0;
				(fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
				index++)
		{
			FBLOCK(fb);
			fb->deliverFramebufferNotification(kIOFBNotifyProbed, NULL);
			FBUNLOCK(fb);
		}
		resetClamshell();
	}
}

void IOFramebuffer::serverAckTimeout(OSObject * owner, IOTimerEventSource * sender)
{
    uint32_t	        index;
    IOFramebuffer *     fb;

	for (index = 0;
			(fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
			index++)
	{
		FBLOCK(fb);
		if (fb->serverState != fb->serverNotified)
		{
			DEBG1(fb->thisName, " (%d->%d) server ack timeout\n",
				 fb->serverState, fb->serverNotified);
			fb->serverAcknowledgeNotification();
		}
		FBUNLOCK(fb);
	}
}

IOFBController * IOFramebuffer::aliasController(IOFBController * controller)
{
	IOFBController * result = controller;
	do
	{
		result = result->nextController;
		if (result->aliasID == controller->aliasID)
			break;
	}
	while (result != controller);

	if (result == controller)
		result = controller->nextController;

	return (result);
}

uint32_t IOFramebuffer::controllerState(IOFBController * controller)
{
	IOFramebuffer * fb;
    uint32_t idx;
    uint32_t state;

	state = controller->state;

	if (controller->didWork || controller->connectChange)
	{
		FCLOCK(controller);

		if (controller->connectChange)
			controller->needsWork = true;

		if (controller->didWork & ~kWorkStateChange)
		{
			controller->asyncWork = false;

			for (idx = 0; (fb = controller->fbs[idx]); idx++)
			{
				if (kWorkPower & controller->didWork)
				{
					if (fb->pendingPowerState)
					{
						fb->deliverFramebufferNotification( kIOFBNotifyDidWake,    (void *) true);
						fb->deliverFramebufferNotification( kIOFBNotifyDidPowerOn, (void *) false);
//   					fb->deliverFramebufferNotification( kIOFBNotifyDidWake,    (void *) false);
					}
					else
						fb->deliverFramebufferNotification( kIOFBNotifyDidPowerOff, (void *) false);
				}
				if (kWorkSuspend & controller->didWork)
				{
					fb->suspend(false);
				}
			}
	
			if (kWorkSuspend & controller->didWork)
			{
				if (controller->onlineMask) 				// bgOn
				{
					IOFBController * oldController;
		
					oldController = aliasController(controller);
					FCLOCK(oldController);
					if (oldController->lastFinishedChange == oldController->connectChange)
					{
						DEBG1(controller->name, " mute exit\n");
					}
					FCUNLOCK(oldController);
					messageConnectionChange(controller);
				}
			}
		}

		state = 0;
		if (controller->wsWait)
			state |= kIOFBWsWait;

		for (idx = 0; (fb = controller->fbs[idx]); idx++)
		{
			if (fb->pagingState)
				state |= kIOFBPaging;
			if (fb->messaged)
				state |= kIOFBDisplaysChanging;
			if (!fb->getIsUsable())
				state |= kIOFBDimmed;
			if (!fb->serverNotified)
				state |= kIOFBServerSlept;
			if (fb->serverState)
				state |= kIOFBServerUp;
			else
				state |= kIOFBServerDown;
			if (fb->captured)
				state |= kIOFBCaptured;
			if (fb->getDimDisable())
				state |= kIOFBDimDisable;
		}
	
		controller->state = state;
		controller->didWork = false;
		state |= kIOFBDidWork;

		FCUNLOCK(controller);
	}

	DEBG1(controller->name, " state %x\n", state);

	return (state);
}

void IOFramebuffer::startThread(bool highPri)
{
    gIOFBWorkES->interruptOccurred(0, 0, 0);
}

// controller WL

void IOFramebuffer::startControllerThread(IOFBController * controller)
{
	controller->needsWork = true;

	controller->didWork |= kWorkStateChange;

	startThread(false);
}

void IOFramebuffer::controllerDidWork(IOFBController * controller, IOOptionBits work)
{
	if (controller->asyncWork && !work) OSReportWithBacktrace("asyncWork+did");

	controller->didWork |= work;

	startThread(false);
}

void IOFramebuffer::controllerAsyncWork(OSObject * owner,
                                        IOInterruptEventSource * evtSrc, int intCount)
{
	IOFramebuffer * fb = (IOFramebuffer *) owner;
	IOFBController * controller = fb->__private->controller;
    uint32_t idx;
    uint32_t asyncWork;

	asyncWork = controller->asyncWork;
	controller->asyncWork = true;

	DEBG1(controller->name, " (%d)\n", asyncWork);

	if (kWorkPower & asyncWork)
	{
		uint32_t newState = controller->fbs[0]->pendingPowerState;
		DEBG1(controller->name, " async kIODriverPowerAttribute(%d, %d)\n", 
				newState, controller->fbs[0]->pendingPowerState);
		for (idx = 0; (fb = controller->fbs[idx]); idx++)
			fb->setAttribute(kIODriverPowerAttribute, newState);
		controllerDidWork(controller, kWorkPower);
	}
	else if (kWorkSuspend & asyncWork)
	{
		for (idx = 0; (fb = controller->fbs[idx]); idx++)
			fb->setAttributeForConnection(0, kConnectionPanelTimingDisable, false);

		processConnectChange(controller, bg);
		if (controller->onlineMask)
			matchController(controller);
		else
		{
			DEBG1(controller->name, " bg offline\n");
			controller->lastFinishedChange = controller->connectChange;
		}

		for (idx = 0; (fb = controller->fbs[idx]); idx++)
			fb->setAttributeForConnection(0, kConnectionPanelTimingDisable, true);

		controllerDidWork(controller, kWorkSuspend);
	}
}

IOOptionBits IOFramebuffer::checkPowerWork(IOFBController * controller, IOOptionBits state)
{
	IOFramebuffer *  fb;
    uint32_t idx;

	if (controller->pendingMuxPowerChange)
	{
		uint32_t newState = controller->fbs[0]->pendingPowerState;
		DEBG1(controller->name, " async checkPowerWork(%d)\n", newState);
		if (!newState)
		{
			for (idx = 0; (fb = controller->fbs[idx]); idx++)
			{
//					fb->deliverFramebufferNotification(kIOFBNotifyWillSleep,    (void *) false);
				fb->deliverFramebufferNotification(kIOFBNotifyWillPowerOff, (void *) false);
				fb->deliverFramebufferNotification(kIOFBNotifyWillSleep,    (void *) true);
			}
		}
		controller->pendingMuxPowerChange = false;
		startAsync(controller, kWorkPower);
	}
	else for (idx = 0; (fb = controller->fbs[idx]); idx++)
	{
		state |= fb->checkPowerWork(state);
	}

    return (state);
}

IOOptionBits IOFramebuffer::checkPowerWork(IOOptionBits state)
{
    IOOptionBits ourState = kIOFBPaging;
	IOService *  device;
    OSData *     stateData = 0;

	if (pendingPowerChange)
	{
		uintptr_t newState = pendingPowerState;

		DEBG1(thisName, " pendingPowerState(%ld)\n", newState);

    	__private->controller->powerThread = current_thread();

		device = __private->controller->device;
		if (device 
		  && (stateData = OSDynamicCast(OSData, getPMRootDomain()->getProperty(kIOHibernateStateKey))))
		{
			if (kIOHibernateStateWakingFromHibernate == ((uint32_t *) stateData->getBytesNoCopy())[0])
			{
				OSNumber *
				num = OSDynamicCast(OSNumber, getPMRootDomain()->getProperty(kIOHibernateOptionsKey));
				uint32_t options = 0;
				if (num) options = num->unsigned32BitValue();
				DEBG1(thisName, " kIOHibernateOptionsKey %p (0x%x)\n", num, options);
				if (kIOHibernateOptionDarkWake & options)
				{
					stateData = 0;
				}
				else
				{
					device->setProperty(kIOHibernateStateKey, stateData);
					device->setProperty(kIOHibernateEFIGfxStatusKey,
										getPMRootDomain()->getProperty(kIOHibernateGfxStatusKey));
					if (newState == 1) newState = 2;
				}
			}
		}

		if ((newState == 1) 
			&& (kIODisplayOptionDimDisable & __private->displayOptions) 
			&& !gIOFBSystemDark)
		{
			newState = 2;
		}

		setAttribute(kIOPowerStateAttribute, newState);
		DEBG1(thisName, " did kIOPowerStateAttribute(%ld)\n", newState);
		if (stateData)
		{
			device->setProperty(kIOHibernateStateKey, gIOFBZero32Data);
			device->removeProperty(kIOHibernateEFIGfxStatusKey);
		}

    	__private->controller->powerThread = NULL;

		OSObject * obj;
		if ((this == gIOFBConsoleFramebuffer) 
			&& (obj = copyProperty(kIOHibernatePreviewActiveKey, gIOServicePlane)))
		{
			getPMRootDomain()->setProperty(kIOHibernatePreviewActiveKey, obj);
			obj->release();
		}

		DEBG(thisName, " acknowledgeSetPowerState\n");
		pendingPowerChange = false;
		acknowledgeSetPowerState();
	}

    if (__private->allowSpeedChanges && __private->pendingSpeedChange)
    {
        __private->pendingSpeedChange = false;
    	__private->controller->powerThread = current_thread();
        setAttribute(kIOFBSpeedAttribute, __private->reducedSpeed);
    	__private->controller->powerThread = NULL;
    }

    return (ourState);
}

IOOptionBits IOFramebuffer::checkConnectionWork( IOFBController * controller, IOOptionBits state )
{
    IOFramebuffer * fb;
    uint32_t idx;

    DEBG1(controller->name, " count(%d, msg %d, fin %d, proc %d, wake %d), capt(%d) wsWait(%d)\n",
          controller->connectChange,
          controller->lastMessagedChange, controller->lastFinishedChange,
          controller->fbs[0]->__private->lastProcessedChange,
          controller->postWakeChange,
          (0 != (kIOFBCaptured & state)), controller->wsWait);

	if (kIOFBCaptured & state)
	{
		if (controller->connectChange != controller->lastForceRetrain)
        {
            controller->lastForceRetrain = controller->connectChange;
            for (idx = 0; (fb = controller->fbs[idx]); idx++)
            {
                if (!fb->__private->dpSupported)
				{
                    IOReturn err;
					uintptr_t value[16];
                    err = fb->getAttributeForConnection(0, kConnectionHandleDisplayPortEvent, &value[0]);
					fb->__private->dpSupported = (kIOReturnSuccess == err) ? true : -1;
                    DEBG1(fb->thisName, " dpSupported(%d)\n", fb->__private->dpSupported);
				}

                if (fb->__private->dpSupported > 0)
                {
                    IOReturn err;
                    uintptr_t sel = kIODPEventForceRetrain;
                    err = fb->setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
                    DEBG1(fb->thisName, " kIODPEventForceRetrain(0x%x)\n", err);
                }
            }
        }
		return (state);
	}

	if (gIOGraphicsControl 
	 && (controller->fbs[0]->__private->lastProcessedChange != controller->connectChange)
	 && !controller->asyncWork)
	{
		bool needAsync = false;
		if ((!controller->onlineMask)
			&& (controller->lastFinishedChange == controller->connectChange))
		{
			// bgOff
			needAsync = true;
		}
		else if ((!controller->onlineMask)
				&& (!controller->mute)
				&& (controller->connectChange != controller->postWakeChange))
		{
			// bgOn
			IOFBController * oldController;
			IOFramebuffer  * oldfb;
			oldController = aliasController(controller);
			FCLOCK(oldController);

			DEBG1(controller->name, " copy from %s\n", oldController->name);

			for (idx = 0; (fb = controller->fbs[idx]) && (oldfb = oldController->fbs[idx]); idx++)
			{
				DEBG1(oldfb->thisName, " copy %d x %d\n", 
					(int) oldfb->__private->pixelInfo.activeWidth,
					(int) oldfb->__private->pixelInfo.activeHeight);

				fb->__private->pixelInfo    = oldfb->__private->pixelInfo;
				fb->__private->timingInfo   = oldfb->__private->timingInfo;
				fb->__private->uiScale      = oldfb->__private->uiScale;
				fb->__private->matchedMode  = oldfb->__private->setupMode;

				if (oldfb->__private->displayAttributes) oldfb->__private->displayAttributes->retain();
				if (fb->__private->displayAttributes) fb->__private->displayAttributes->release();
				fb->__private->displayAttributes =    oldfb->__private->displayAttributes;
	
				if (oldfb->__private->rawGammaDataLen != fb->__private->rawGammaDataLen)
				{
					if (fb->__private->rawGammaDataLen)
						IODelete(fb->__private->rawGammaData, UInt8, fb->__private->rawGammaDataLen);
					fb->__private->rawGammaData = IONew(UInt8, oldfb->__private->rawGammaDataLen);
				}
				if (!fb->__private->rawGammaData)
				{
					fb->__private->rawGammaDataLen = 0;
					break;
				}
				fb->__private->rawGammaDataLen      = oldfb->__private->rawGammaDataLen;
				fb->__private->rawGammaChannelCount = oldfb->__private->rawGammaChannelCount;
				fb->__private->rawGammaDataCount    = oldfb->__private->rawGammaDataCount;
				fb->__private->rawGammaDataWidth    = oldfb->__private->rawGammaDataWidth;
		
				bcopy(oldfb->__private->rawGammaData, fb->__private->rawGammaData, 
						fb->__private->rawGammaDataLen);

			}
			FCUNLOCK(oldController);
			needAsync = true;
		}

		if (needAsync)
		{
			for (idx = 0; (fb = controller->fbs[idx]); idx++)
				fb->suspend(true);
			startAsync(controller, kWorkSuspend);
			state |= kIOFBDisplaysChanging;
		}
	}

	if (!controller->asyncWork)
	{
		messageConnectionChange(controller);
	}

	return (state);
}

void IOFramebuffer::messageConnectionChange( IOFBController * controller )
{
	IOReturn 		err;
	bool     		discard = false;
    IOFramebuffer * fb;
    uint32_t 		idx;
	
	if ((controller->lastMessagedChange != controller->connectChange)
		 && !controller->fbs[0]->messaged
		 && !controller->wsWait)
	{
		controller->lastMessagedChange = controller->connectChange;
		if (gIOGraphicsControl)
		{
			err = gIOGraphicsControl->message(kIOFBMessageConnectChange, controller->fbs[0], NULL);
			if (kIOReturnOffline == err)
			{
				DEBG1(controller->name, " AGC discard\n");
				discard = true;
			}
		}
		if (discard)
		{
			controller->lastFinishedChange  = controller->lastMessagedChange;
			for (idx = 0; (fb = controller->fbs[idx]); idx++)
				fb->__private->lastProcessedChange = controller->lastMessagedChange;
		}
		else
		{
			DEBG1(controller->name, " messaged\n");
			controller->fbs[0]->messaged = true;
			controller->fbs[0]->messageClients(kIOMessageServiceIsSuspended, (void *) true);
		}
    }
}


IOReturn IOFramebuffer::extEndConnectionChange(void)
{
    IOFBController * controller = __private->controller;

	DEBG1(controller->name, " WS done msg %d, onl %x, count(%d, msg %d, fin %d, proc %d, wake %d)\n", 
		  controller->fbs[0]->messaged, __private->controller->onlineMask,
          controller->connectChange,
          controller->lastMessagedChange, controller->lastFinishedChange,
          controller->fbs[0]->__private->lastProcessedChange,
          controller->postWakeChange);

    if (!controller->fbs[0]->messaged)
        return (kIOReturnSuccess);

	controller->fbs[0]->messaged = false;
	__private->controller->lastFinishedChange = __private->controller->lastMessagedChange;

	if (gIOGraphicsControl)
	{
		IOReturn err;
		err = gIOGraphicsControl->message(kIOFBMessageEndConnectChange, controller->fbs[0], NULL);
	}

	resetClamshell();
	startControllerThread(controller);

	return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::processConnectChange(IOFBController * controller, IOOptionBits mode)
{
    IOFramebuffer * fb;
    uint32_t idx;

    for (idx = 0; (fb = controller->fbs[idx]); idx++)
    {
        fb->processConnectChange(mode);
    }
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extProcessConnectionChange(void)
{
    IOFBController * controller = __private->controller;
    IOReturn         err = kIOReturnSuccess;
	IOOptionBits     mode = 0;

	TIMELOCK(gIOFBSystemWorkLoop, thisName, __FUNCTION__);

	if ((err = extEntry(true)))
	{
		SYSUNLOCK();
		return (err);
	}

    if (controller->fbs[0]->messaged) 
	{
		if (controller->mute)
			mode = fgOff;
		else if (__private->lastProcessedChange != __private->controller->connectChange)
			mode = fg;
		else if (kIODisplayModeIDInvalid != __private->aliasMode)
		{
			IOFBController * oldController;
			IOFramebuffer  * oldfb;
			bool			 rematch = false;

			oldController = aliasController(controller);
			FCLOCK(oldController);
			oldfb = oldController->fbs[__private->controllerIndex];
			if (oldfb)
			{
				DEBG1(thisName, " check copy from %s: 0x%x==0x%x?\n", oldfb->thisName,
						(int) __private->matchedMode, (int) oldfb->__private->setupMode);
				rematch = (__private->matchedMode != oldfb->__private->setupMode);
				if (rematch)
				{
					DEBG1(thisName, " rematch using %d x %d\n", 
						(int) oldfb->__private->pixelInfo.activeWidth,
						(int) oldfb->__private->pixelInfo.activeHeight);
					__private->pixelInfo    = oldfb->__private->pixelInfo;
					__private->timingInfo   = oldfb->__private->timingInfo;
					__private->uiScale      = oldfb->__private->uiScale;
					__private->matchedMode  = oldfb->__private->setupMode;
				}
			}
			FCUNLOCK(oldController);
			if (rematch)
			{
				suspend(true);
				matchFramebuffer();
				suspend(false);
			}
		}
	}
	extExit(err);

    if (mode)
    {
		temporaryPowerClampOn();
		IODisplayWrangler::destroyDisplayConnects(this);
   
		err = extEntry(true);
		if (kIOReturnSuccess == err)
		{
			err = processConnectChange(mode);
			extExit(err);
		}
	}

	SYSUNLOCK();

    return (err);
}


IOReturn IOFramebuffer::processConnectChange(IOOptionBits mode)
{
    IOReturn  err;
    uintptr_t unused;
    bool      nowOnline;
    
    DEBG1(thisName, " (%d==%s) curr %d\n", 
    		(uint32_t) mode, processConnectChangeModeNames[mode], __private->lastProcessedChange);

    if (fgOff == mode)
    {
		displaysOnline(false);
        __private->online = false;
        return (kIOReturnSuccess);
    }
    if (__private->lastProcessedChange == __private->controller->connectChange)
        return (kIOReturnSuccess);
    
    if (fg == mode) suspend(true);
    
	{
		// connect change vars here
		__private->enableScalerUnderscan = false;
		__private->colorModesSupported   = 0;
	}
    
    TIMESTART();
    err = getAttributeForConnection(0, kConnectionChanged, &unused);
    TIMEEND(thisName, "kConnectionChanged time: %qd ms\n");
    
    __private->lastProcessedChange = __private->controller->connectChange;
    extSetMirrorOne(0, 0);

    nowOnline = updateOnline();
    if (false && nowOnline)
    {
        DEBG1(thisName, " bgOff forced\n");
        nowOnline = false;
    }

    displaysOnline(false);
    __private->online = nowOnline;
    __private->transform = __private->selectedTransform;
    setProperty(kIOFBTransformKey, __private->transform, 64);
    if (nowOnline)
        displaysOnline(true);
    
    if (nowOnline)
    {
        if (__private->cursorAttributes && !__private->cursorAttributes->getCount())
        {
            __private->cursorAttributes->release();
            __private->cursorAttributes = 0;
        }
        
        if (!__private->cursorAttributes)
        {
            if ((__private->cursorAttributes = OSArray::withCapacity(2)))
            {
                __private->testingCursor = true;
                setCursorImage( (void *) 0 );
                __private->testingCursor = false;
                
                setProperty( kIOFBCursorInfoKey, __private->cursorAttributes );
            }
        }
    }
	else if (kIODisplayModeIDInvalid != __private->offlineMode)
    {
		IOPixelInformation pixelInfo;

		pixelInfo.bitsPerPixel     = 32;
		pixelInfo.pixelType        = kIORGBDirectPixels;
		pixelInfo.componentCount   = 3;
		pixelInfo.bitsPerComponent = 8;
      	__private->currentDepth    = closestDepth(__private->offlineMode, &pixelInfo);

		err = setDisplayMode(__private->offlineMode, __private->currentDepth);
		DEBG1(thisName, " offline setDisplayMode(0x%x, %d) err %x msg %d susp %d\n",
				(int32_t) __private->offlineMode, (int32_t) __private->currentDepth,
				err, messaged, suspended);

		if (fg == mode) suspend(false);
	}

    err = kIOReturnSuccess;
    return (err);
}

bool IOFramebuffer::updateOnline(void)
{
    IOReturn                    err;
    uintptr_t					connectEnabled;
	bool						nowOnline;
    IOTimingInformation         info;
    IODisplayModeID *           modeIDs;
    IOItemCount                 modeCount, modeNum;

    TIMESTART();
    err = getAttributeForConnection(0, kConnectionCheckEnable, &connectEnabled);
    TIMEEND(thisName, "kConnectionCheckEnable == %ld, time: %qd ms\n", connectEnabled);

    nowOnline = (!dead && ((kIOReturnSuccess != err) || connectEnabled));

    __private->gammaScale[0] = __private->gammaScale[1] = __private->gammaScale[2] = (1 << 16);
	__private->aliasMode         = kIODisplayModeIDInvalid;
	__private->offlineMode       = kIODisplayModeIDInvalid;

	if (!nowOnline) do
	{
		TIMESTART();
		modeCount = getDisplayModeCount();
		modeIDs = IONew(IODisplayModeID, modeCount);
		if (!modeIDs)
			break;
		err = getDisplayModes(modeIDs);
		if (kIOReturnSuccess == err)
		{
			for (modeNum = 0; modeNum < modeCount; modeNum++)
			{
				info.flags = 0;
				err = getTimingInfoForDisplayMode(modeIDs[modeNum], &info);
				if (kIOReturnSuccess != err)
					continue;
				if (kIOTimingIDApple_0x0_0hz_Offline != info.appleTimingID)
					continue;
				__private->offlineMode = modeIDs[modeNum];
				break;
			}
		}
		IODelete(modeIDs, IODisplayModeID, modeCount);
		TIMEEND(thisName, "offline mode %x time: %qd ms\n", (int32_t) __private->offlineMode);
	}
	while (false);

    return (nowOnline);
}

void IOFramebuffer::displaysOnline(bool nowOnline)
{
	if (nowOnline == __private->displaysOnline)
		return;

	if (nowOnline)
    {
		TIMESTART();
        IODisplayWrangler::makeDisplayConnects(this);
		TIMEEND(thisName, "makeDisplayConnects time: %qd ms\n");
	}
	else
	{
		if (__private->displayPrefKey)
		{
			__private->displayPrefKey->release();
			__private->displayPrefKey = NULL;
		}
	
		TIMESTART();
		if (__private->paramHandler)
			__private->paramHandler->setDisplay(0);
		TIMEEND(thisName, "setDisplay time: %qd ms\n");

		TIMESTART();
		stopCursor();
//		temporaryPowerClampOn();
		//        FCUNLOCK(controller);
//		IODisplayWrangler::destroyDisplayConnects(this);
		//        FCLOCK(controller);
		TIMEEND(thisName, "destroyDisplayConnects time: %qd ms\n");
	}
	__private->displaysOnline = nowOnline;

	__private->actualVBLCount = 0;
    StdFBShmem_t * shmem = GetShmem(this);
	if (shmem)
	{
		shmem->vblDrift         = 0;
		shmem->vblDeltaMeasured = 0;
	}
	if (nowOnline)
	{
		__private->controller->onlineMask |=  (1 << __private->controllerIndex);
		updateVBL(this, NULL);
	}
	else	
	{
		__private->controller->onlineMask &= ~(1 << __private->controllerIndex);
	}
}

IOReturn IOFramebuffer::matchFramebuffer(void)
{
	IOFBDisplayModeDescription  modeInfo;
	IOReturn		err;
	IODisplayModeID mode;
	IOIndex         depth = 0;
	OSData *		data;
	OSArray *		array;
	
	modeInfo.timingInfo = __private->timingInfo;
	mode = kIODisplayModeIDInvalid;
	if (kIODetailedTimingValid & modeInfo.timingInfo.flags) do
	{
		modeInfo.timingInfo.detailedInfo.v2.detailedTimingModeID
				= (kIODisplayModeIDReservedBase | kIODisplayModeIDAliasBase);
		modeInfo.timingInfo.detailedInfo.v2.minPixelClock
				= modeInfo.timingInfo.detailedInfo.v2.pixelClock;
		modeInfo.timingInfo.detailedInfo.v2.maxPixelClock
				= modeInfo.timingInfo.detailedInfo.v2.pixelClock;

		err = validateDetailedTiming(&modeInfo, sizeof(modeInfo));
		if (kIOReturnSuccess != err)
		{
			DEBG1(thisName, " validateDetailedTiming(%x)\n", err);
			break;
		}	
		data = OSData::withBytes(&modeInfo.timingInfo.detailedInfo.v2,
										  sizeof(modeInfo.timingInfo.detailedInfo.v2));
		array = OSArray::withObjects((const OSObject**) &data, 1, 1);
		data->release();
		err = setDetailedTimings(array);
		array->release();
		if (kIOReturnSuccess != err)
		{
			DEBG1(thisName, " setDetailedTimings(%x)\n", err);
			break;
		}	
		mode = kIODisplayModeIDReservedBase | kIODisplayModeIDAliasBase;
	}
	while (false);
	if (kIODisplayModeIDInvalid != mode)
	{
		setDisplayAttributes(__private->displayAttributes);

		depth = closestDepth(mode, &__private->pixelInfo);
		TIMESTART();
		err = setDisplayMode(mode, depth);
		TIMEEND(thisName, "matching setDisplayMode(0x%x, %d) err %x time: %qd ms\n", 
				(int32_t) mode, (int32_t) depth, err);

		if (__private->rawGammaData)
		{
			TIMESTART();
			updateGammaTable(__private->rawGammaChannelCount, 
									__private->rawGammaDataCount, 
									__private->rawGammaDataWidth, 
									__private->rawGammaData,
									true);
			TIMEEND(thisName, "match updateGammaTable time: %qd ms\n");
		}
	}
	__private->aliasMode    = mode;
	__private->currentDepth = depth;
    return (err);
}

IOReturn IOFramebuffer::matchController(IOFBController * controller)
{
    IOFramebuffer * fb;
    uint32_t idx;

    for (idx = 0; (fb = controller->fbs[idx]); idx++)
    {
	   	if (!fb->__private->online)
			continue;
		fb->matchFramebuffer();
	}
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::setPowerState( unsigned long powerStateOrdinal,
                                       IOService * whichDevice )
{
	IOReturn ret = kIOPMAckImplied;

    DEBG1(thisName, " (%ld) mute %d, now %d\n",
    	 powerStateOrdinal, __private->controller->mute, (int)getPowerState());

	if (gIOFBSystemPowerAckTo && !powerStateOrdinal)
		IOLog("graphics notify timeout (%d, %d)", serverState, serverNotified);

    FBLOCK(this);

	pendingPowerState = powerStateOrdinal;
	pendingPowerChange = true;

	startControllerThread(__private->controller);
    ret = kPowerStateTimeout * 1000 * 1000;

    FBUNLOCK(this);

    return (ret);
}

IOReturn IOFramebuffer::powerStateWillChangeTo( IOPMPowerFlags flags,
        unsigned long state, IOService * whatDevice )
{
    DEBG1(thisName, " (%08lx)\n", flags);

	FBLOCK(this);
    if (isUsable && !(IOPMDeviceUsable & flags))
    {
		isUsable = false;
		controllerDidWork(__private->controller, kWorkStateChange);
	}
	FBUNLOCK(this);

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::powerStateDidChangeTo( IOPMPowerFlags flags,
        unsigned long, IOService* whatDevice )
{
    DEBG1(thisName, " (%08lx)\n", flags);

	FBLOCK(this);
    if ((IOPMDeviceUsable & flags) && !isUsable)
    {
		isUsable = true;
		controllerDidWork(__private->controller, kWorkStateChange);
	}
	FBUNLOCK(this);

    return (kIOReturnSuccess);
}

void IOFramebuffer::updateDisplaysPowerState(void)
{
	OSBitOrAtomic(kIOFBEventDisplaysPowerState, &gIOFBGlobalEvents);
	startThread(false);
}

void IOFramebuffer::delayedEvent(thread_call_param_t p0, thread_call_param_t p1)
{
	OSBitOrAtomic((uintptr_t) p1, &gIOFBGlobalEvents);
	startThread(false);
}

void IOFramebuffer::resetClamshell(void)
{
	AbsoluteTime deadline;
	clock_interval_to_deadline(kIOFBClamshellProbeDelayMS, kMillisecondScale, &deadline );
	thread_call_enter1_delayed(gIOFBClamshellCallout,
								(thread_call_param_t) kIOFBEventResetClamshell, deadline );
}

void IOFramebuffer::displayOnline(IODisplay * display, SInt32 delta, uint32_t options)
{
	if (delta <= 0)
	{
		options = __private->displayOptions;
	}
	if (kIODisplayOptionBacklight & options)
		OSAddAtomic(delta, &gIOFBBacklightDisplayCount);
	else
		OSAddAtomic(delta, &gIOFBDisplayCount);

	if (delta < 0)
	{
//		if (display != __private->display) panic("(display != __private->display)");
		__private->display        = NULL;
		__private->displayOptions = 0;
		if (kIODisplayOptionBacklight & options)
		{
			OSBitAndAtomic(~(kIOFBEventResetClamshell | kIOFBEventEnableClamshell),
							&gIOFBGlobalEvents);
			gIOFBClamshellState = kIOPMDisableClamshell;
			getPMRootDomain()->receivePowerNotification(kIOPMDisableClamshell);
			DEBG1("S", " clamshell disable\n");
		}
	}
	else
	{
//		if (__private->display) panic("(__private->display)");
		__private->display        = display;
		__private->displayOptions = options;
	}
}

IOOptionBits IOFramebuffer::clamshellState( void )
{
    return (gIOFBClamshellState);
}

IOReturn IOFramebuffer::agcMessage( void * target, void * refCon,
        UInt32 messageType, IOService * service,
        void * messageArgument, vm_size_t argSize )
{
    IOReturn ret = kIOReturnSuccess;
	enum
	{
		gMux_Message      = 'gMUX',
		gMux_WillSwitch   = 0,
		gMux_DidSwitch    = 1,
		gMux_DidNotSwitch = 2,
	};

	IOService * ackTo  = NULL;
	uint32_t    ackRef = NULL;
	uintptr_t   param  = (uintptr_t) messageArgument;

    if (gMux_Message != messageType)
		return (ret);

	DEBG1("AGC", " %p\n", messageArgument);

	SYSLOCK();
	if (gMux_WillSwitch == param)
	{
		gIOFBSwitching = true;
	}
	else if (gIOFBSwitching && ((gMux_DidSwitch == param) || (gMux_DidNotSwitch == param)))
	{
		gIOFBSwitching = false;

		ackTo  = gIOFBSystemPowerMuxAckTo;
		ackRef = gIOFBSystemPowerMuxAckRef;
		gIOFBSystemPowerMuxAckTo = 0;

		startThread(false);
	}

	SYSUNLOCK();

	if (ackTo)
	{
		DEBG("S", " mux allowPowerChange()\n");
		ackTo->allowPowerChange(ackRef);
	}

    return (ret);
}

IOReturn IOFramebuffer::muxPowerMessage(UInt32 messageType)
{
    IOReturn ret = kIOReturnSuccess;

    DEBG1("PWR", " muxPowerMessage(%x->%x)\n", (int) gIOFBLastMuxMessage, (int) messageType);

	if (messageType == gIOFBLastMuxMessage)
		return (kIOReturnSuccess);

	if ((kIOMessageSystemHasPoweredOn == messageType) 
		&& (kIOMessageSystemWillPowerOn != gIOFBLastMuxMessage))
	{
		DEBG1("PWR", " muxPowerMessage(%x)\n", (int) kIOMessageSystemWillPowerOn);
		if (gIOGraphicsControl) (void) gIOGraphicsControl->message(kIOMessageSystemWillPowerOn, NULL, NULL);
	}

	gIOFBLastMuxMessage = messageType;
	DEBG1("PWR", " muxPowerMessage(%x)\n", (int) messageType);
	if (gIOGraphicsControl) ret = gIOGraphicsControl->message(messageType, NULL, NULL);

	return (ret);
}

void IOFramebuffer::didDarkWake(OSObject * owner, IOTimerEventSource * sender)
{
	if (!gIOFBSystemPower)
	{
		SYSUNLOCK();
		muxPowerMessage(kIOMessageDeviceHasPoweredOn);
		SYSLOCK();
	}
}

IOReturn IOFramebuffer::systemPowerChange( void * target, void * refCon,
        UInt32 messageType, IOService * service,
        void * messageArgument, vm_size_t argSize )
{
    IOReturn ret = kIOReturnSuccess;

    DEBG1("PWR", "(%08x)\n", (uint32_t) messageType);

	if (gIOGraphicsControl) switch (messageType)
	{
#if VERSION_MAJOR >= 11
        case kIOMessageSystemCapabilityChange:
#endif
        case kIOMessageSystemWillSleep:
        case kIOMessageSystemWillPowerOn:
        case kIOMessageSystemHasPoweredOn:
			break;

		default:
			ret = gIOGraphicsControl->message(messageType, service, messageArgument);
			break;
	}

    switch (messageType)
    {
#if VERSION_MAJOR < 11
#warning sl
#else
        case kIOMessageSystemCapabilityChange:
		{
			IOPMSystemCapabilityChangeParameters * params = (typeof params) messageArgument;
	
			// root domain won't overlap capability changes with pstate changes

			DEBG1("DARK", " %s%s 0x%x->0x%x\n", 
			params->changeFlags & kIOPMSystemCapabilityWillChange ? "will" : "",
			params->changeFlags & kIOPMSystemCapabilityDidChange ? "did" : "",
			params->fromCapabilities,
			params->toCapabilities);

			if ((params->changeFlags & kIOPMSystemCapabilityWillChange) &&
				(params->fromCapabilities & kIOPMSystemCapabilityGraphics) &&
				((params->toCapabilities & kIOPMSystemCapabilityGraphics) == 0))
			{
				ret = muxPowerMessage(kIOMessageSystemWillSleep);
				if (kIOReturnNotReady == ret)
				{
					SYSLOCK();
					gIOFBSwitching = true;
					gIOFBSystemPowerMuxAckRef = params->notifyRef;
					gIOFBSystemPowerMuxAckTo  = service;
					SYSUNLOCK();
					params->maxWaitForReply = gIOGNotifyTO * 1000 * 1000;
				}
			}
			else if ((params->changeFlags & kIOPMSystemCapabilityWillChange) &&
				((params->fromCapabilities & kIOPMSystemCapabilityGraphics) == 0) &&
				(params->toCapabilities & kIOPMSystemCapabilityGraphics))
			{

		if (kIOMessageSystemHasPoweredOn != gIOFBLastMuxMessage)
				muxPowerMessage(kIOMessageSystemWillPowerOn);
			}
			else if ((params->changeFlags & kIOPMSystemCapabilityDidChange) &&
				((params->fromCapabilities & kIOPMSystemCapabilityGraphics) == 0) &&
				(params->toCapabilities & kIOPMSystemCapabilityGraphics))
			{
				muxPowerMessage(kIOMessageSystemHasPoweredOn);
			}

			else if ((params->changeFlags & kIOPMSystemCapabilityDidChange) &&
				((params->fromCapabilities & kIOPMSystemCapabilityCPU) == 0) &&
				(params->toCapabilities & kIOPMSystemCapabilityCPU))
			{
				muxPowerMessage(kIOMessageSystemHasPoweredOn);
			}
			else if ((params->changeFlags & kIOPMSystemCapabilityWillChange) &&
				((params->fromCapabilities & kIOPMSystemCapabilityCPU) == 0) &&
				(params->toCapabilities & kIOPMSystemCapabilityCPU))
			{
				muxPowerMessage(kIOMessageSystemWillPowerOn);
				SYSLOCK();
				gIOFBSystemPower       = true;
				gIOGraphicsSystemPower = true;
				gIOFBSystemDark        = true;
				SYSUNLOCK();
			}
			else if ((params->changeFlags & kIOPMSystemCapabilityWillChange) &&
				(params->fromCapabilities & kIOPMSystemCapabilityCPU) &&
				((params->toCapabilities & kIOPMSystemCapabilityCPU) == 0))
			{

if (gIOFBSystemPower)
{
				ret = muxPowerMessage(kIOMessageSystemWillSleep);
	
				SYSLOCK();

//				gIOFBClamshellState = kIOPMDisableClamshell;
//				getPMRootDomain()->receivePowerNotification(kIOPMDisableClamshell);
			
				gIOFBSystemPower       = false;
				gIOFBSystemPowerAckRef = (void *) params->notifyRef;
				gIOFBSystemPowerAckTo  = service;
#if 0
				gIOFBSwitching = (kIOReturnNotReady == ret);
				if (gIOFBSwitching)
				{
					DEBG1("PWR", " agc not ready\n");
				}
#endif
				startThread(false);
				gIOGraphicsSystemPower = false;
			
				SYSUNLOCK();
			
				// We will ack within gIOGNotifyTO seconds
				params->maxWaitForReply = gIOGNotifyTO * 1000 * 1000;
				ret                    = kIOReturnSuccess;
}
			}

#if 0
			else if ((params->changeFlags & kIOPMSystemCapabilityWillChange) &&
				((params->fromCapabilities & kIOPMSystemCapabilityCPU) == 0) &&
				(params->toCapabilities & kIOPMSystemCapabilityCPU))
			{
				gIOFBDarkWakeTimer->setTimeoutMS(kDarkWokeTimeout * 1000);
			}
			else if ((params->changeFlags & kIOPMSystemCapabilityWillChange) &&
				(params->fromCapabilities & kIOPMSystemCapabilityCPU) &&
				((params->toCapabilities & kIOPMSystemCapabilityCPU) == 0))
			{
				ret = muxPowerMessage(kIOMessageSystemWillSleep);
			}
#endif
			ret = kIOReturnSuccess;
			break;
		}
#endif

        case kIOMessageSystemWillSleep:
		{
			IOPowerStateChangeNotification * params = (typeof params) messageArgument;

			ret = muxPowerMessage(kIOMessageSystemWillSleep);

			SYSLOCK();
 			gIOFBClamshellState = kIOPMDisableClamshell;
			getPMRootDomain()->receivePowerNotification(kIOPMDisableClamshell);

			gIOFBSystemPower       = false;
			gIOFBSystemPowerAckRef = params->powerRef;
			gIOFBSystemPowerAckTo  = service;
			gIOFBSwitching = (kIOReturnNotReady == ret);
			if (gIOFBSwitching)
			{
				DEBG1("PWR", " agc not ready\n");
			}
			startThread(false);
			gIOGraphicsSystemPower = false;
			SYSUNLOCK();

			// We will ack within gIOGNotifyTO seconds
			params->returnValue    = gIOGNotifyTO * 1000 * 1000;
            ret                    = kIOReturnSuccess;
            break;
		}

        case kIOMessageSystemWillPowerOn:
		{
			IOPowerStateChangeNotification * params = (typeof params) messageArgument;

			gIOFBSystemDark        = false;

if (!gIOFBSystemPower)
{
			muxPowerMessage(kIOMessageSystemWillPowerOn);

			SYSLOCK();
		
			readClamshellState();
			OSBitAndAtomic(~(kIOFBEventResetClamshell | kIOFBEventEnableClamshell),
							&gIOFBGlobalEvents);
			gIOFBSystemPower       = true;
			gIOGraphicsSystemPower = true;
		
			SYSUNLOCK();
}
            params->returnValue    = 0;
            ret                    = kIOReturnSuccess;
            break;
		}

        case kIOMessageSystemHasPoweredOn:
		{
			IOPowerStateChangeNotification * params = (typeof params) messageArgument;

			muxPowerMessage(kIOMessageSystemHasPoweredOn);

            params->returnValue = 0;
            ret                 = kIOReturnSuccess;
            break;
		}

        case kIOMessageSystemWillRestart:
        case kIOMessageSystemWillPowerOff:
        case kIOMessageSystemPagingOff:
		{
			IOPowerStateChangeNotification * params = (typeof params) messageArgument;

            SYSLOCK();
            if (gAllFramebuffers)
            {
                IOFramebuffer * fb;
                for (UInt32 index = 0;
                        (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
                        index++)
                {
                    FBLOCK(fb);
                    fb->deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
					if (kIOMessageSystemPagingOff != messageType)
					{
						fb->setAttribute( kIOSystemPowerAttribute, messageType );
						fb->__private->closed = true;
					}
                    FBUNLOCK(fb);
                }
				if (kIOMessageSystemPagingOff == messageType) saveGammaTables();
            }
            SYSUNLOCK();

            params->returnValue = 0;
            ret                 = kIOReturnSuccess;
            break;
		}

        default:
            ret = kIOReturnUnsupported;
            break;
    }

    return (ret);
}

void IOFramebuffer::deferredSpeedChangeEvent( OSObject * owner,
                                              IOInterruptEventSource * evtSrc, int intCount )
{
    IOFramebuffer * self = (IOFramebuffer *) owner;

    if (self->__private->pendingSpeedChange)
    {
        self->__private->pendingSpeedChange = false;
    	self->__private->controller->powerThread = current_thread();
        self->setAttribute(kIOFBSpeedAttribute, self->__private->reducedSpeed);
    	self->__private->controller->powerThread = NULL;

    }
}

IOReturn IOFramebuffer::setAggressiveness( unsigned long type, unsigned long newLevel )
{
    UInt32 reducedSpeed = newLevel;

    if (__private
     && (type == (unsigned long) kIOFBLowPowerAggressiveness)
//     && (reducedSpeed != __private->reducedSpeed)
	)
    {
        __private->reducedSpeed       = reducedSpeed;
        __private->pendingSpeedChange = true;
        if (__private->allowSpeedChanges && __private->deferredSpeedChangeEvent)
		{
			FBLOCK(this);
			startControllerThread(__private->controller);
			FBUNLOCK(this);
//            __private->deferredSpeedChangeEvent->interruptOccurred(0, 0, 0);
		}
    }

    super::setAggressiveness(type, newLevel);

    return (kIOReturnSuccess);
}

IOReturn 
IOFramebuffer::getAggressiveness( unsigned long type, unsigned long * currentLevel )
{
    IOReturn ret;

    if (gIOFBSystemWorkLoop && (type == (unsigned long) kIOFBLowPowerAggressiveness))
    {
        SYSLOCK();
        *currentLevel = __private->reducedSpeed;
        SYSUNLOCK();
        ret = kIOReturnSuccess;
    }
    else
        ret = super::getAggressiveness(type, currentLevel);

    return (ret);
}

void
IOFramebuffer::serverAcknowledgeNotification(void)
{
    bool  nowOn;

    nowOn = (serverNotified && (serverState != serverNotified));
    serverState = serverNotified;

    if (serverState && __private->cursorSlept)
    {
        resetCursor();
        __private->cursorSlept = false;
    }
    else if (!serverState && !__private->cursorSlept)
    {
        hideCursor();
        __private->cursorSlept = true;
    }

    controllerDidWork(__private->controller, kWorkStateChange);
}

IOReturn
IOFramebuffer::extAcknowledgeNotification(
            OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst = (IOFramebuffer *) target;
    IOReturn err;
	uint32_t idx;
	uint32_t countIn;

    if ((err = inst->extEntry(true)))
        return (err);

	countIn = args->scalarInputCount / 2;
	for (idx = 0; (idx < kIOPreviewImageCount); idx++)
	{
		mach_vm_address_t address;
		mach_vm_size_t    length;
		if (inst->__private->saveBitsMD[idx])
		{
			inst->__private->saveBitsMD[idx]->release();
			inst->__private->saveBitsMD[idx] = 0;
		}
		if ((idx < countIn)
			&& (address = args->scalarInput[idx]) 
			&& (length  = args->scalarInput[countIn + idx]))
		{
			inst->__private->saveBitsMD[idx] = IOMemoryDescriptor::withAddressRange(
												address, length,
												kIODirectionOut | kIOMemoryPersistent, current_task());
			DEBG1(inst->thisName, " (%d->%d) save bits [0x%llx, 0x%llx] md %p\n",
				 inst->serverState, inst->serverNotified,
				 address, length,
				 inst->__private->saveBitsMD[idx]);
		}
	}

	inst->serverAcknowledgeNotification();

    inst->extExit(err);

    return (err);
}

IOReturn IOFramebuffer::extRegisterNotificationPort(
    mach_port_t         port,
    UInt32              type,
    UInt32              refCon )
{
    mach_msg_header_t * msgh;
    UInt8               currentState;

    FBLOCK(this);

    msgh = (mach_msg_header_t *) serverMsg;
    bzero( msgh, sizeof(mach_msg_header_t) );

    msgh->msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msgh->msgh_size        = sizeof(mach_msg_header_t);
    msgh->msgh_remote_port = port;

    msgh->msgh_id = 0x87654321;
    mach_msg_send_from_kernel( msgh, msgh->msgh_size );

    currentState     = serverNotified;
    serverNotified   = true;                    // server assumes so at startup
    notifyServer( currentState );

    FBUNLOCK(this);

    return (kIOReturnSuccess);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::writePrefs( OSObject * owner, IOTimerEventSource * sender )
{
    IOFramebuffer * self = (IOFramebuffer *) owner;
    DEBG(self->thisName, "\n");
    self->messageClients( kIOMessageServicePropertyChange, (void *) 'pref' );
}

void IOFramebuffer::connectChangeInterrupt( IOFramebuffer * inst, void * delay )
{
    OSIncrementAtomic( &inst->__private->controller->connectChange);
    
    DEBG1(inst->thisName, "(%d)\n", inst->__private->controller->connectChange);

	startThread(false);
}

static bool 
IOFramebufferLockedSerialize(void * target, void * ref, OSSerialize * s) 
{
    bool ok;
    SYSLOCK();
    ok = ((OSObject *) target)->serialize(s);
    SYSUNLOCK();
    return (ok);
}                                     

IOReturn IOFramebuffer::open( void )
{
    IOReturn            err = kIOReturnSuccess;
    uintptr_t           value;
    IOFramebuffer *     next;
    OSNumber *          depIDProp;
    OSNumber *          num;
    OSData *            data;
    OSObject *          obj;
    bool                newController;
    bool                openAllDependents;

    do
    {
        if (gIOFBSystemWorkLoop)
            SYSLOCK();

        if (opened)
            continue;

        if (dead)
        {
            err = kIOReturnNotOpen;
            continue;
        }
        if (!gAllFramebuffers)
        {
            gAllFramebuffers = OSArray::withCapacity(1);

			if (PE_parse_boot_argn("iog", &gIOGDebugFlags, sizeof(gIOGDebugFlags)))
			{
				DEBG1("IOGraphics", " flags 0x%x\n", gIOGDebugFlags);
				gIOFBLidOpenMode = (0 != (kIOGDbgLidOpen     & gIOGDebugFlags));
				gIOFBVBLThrottle = (0 != (kIOGDbgVBLThrottle & gIOGDebugFlags));
				gIOFBVBLDrift    = (0 != (kIOGDbgVBLDrift & gIOGDebugFlags));
			}
			else
			{
				gIOFBLidOpenMode = (version_major >= 11);
				gIOFBVBLThrottle = (version_major >= 11);
			}
			if (!PE_parse_boot_argn("iognotifyto", &gIOGNotifyTO, sizeof(gIOGNotifyTO)) 
				|| !gIOGNotifyTO)
			{
				gIOGNotifyTO = kSystemWillSleepTimeout;
			}
			DEBG1("IOGraphics", " notify timeout %ds\n", gIOGNotifyTO);

            gIOFBDesktopModeAllowed = true;

            OSIterator * iter = getMatchingServices(serviceMatching("AppleGraphicsControl"));
            if (iter)
            {
                gIOGraphicsControl = OSDynamicCast(IOService, iter->getNextObject());
                iter->release();
				if (gIOGraphicsControl)
				{
					gIOFBGCNotifier = gIOGraphicsControl->registerInterest(
                                    			gIOGeneralInterest, &agcMessage, 0, 0 );
				}
            }

            iter = getMatchingServices(serviceMatching("IOAccelerator"));
            if (iter)
            {
                IOService * accel;
                while ((accel = OSDynamicCast(IOService, iter->getNextObject())))
                    accel->requestProbe(kIOFBUserRequestProbe);

                iter->release();
            }

            if (gIOFBPrefs)
                gIOFBPrefsSerializer = OSSerializer::forTarget(gIOFBPrefs, 
                                                                &IOFramebufferLockedSerialize, 0);

            clock_interval_to_absolutetime_interval(20, kMillisecondScale, &gIOFBMaxVBLDelta);
        }

        if (!gIOFramebufferKey)
            gIOFramebufferKey = OSSymbol::withCStringNoCopy("IOFramebuffer");
        if (!gIOFBGetSensorValueKey)
            gIOFBGetSensorValueKey = OSSymbol::withCStringNoCopy(kIOFBGetSensorValueKey);
        if (!gIOFBRotateKey)
            gIOFBRotateKey = OSSymbol::withCStringNoCopy(kIOFBRotatePrefsKey);
        if (!gIOFBStartupModeTimingKey)
            gIOFBStartupModeTimingKey = OSSymbol::withCStringNoCopy(kIOFBStartupTimingPrefsKey);
        if (!gIOFBPMSettingDisplaySleepUsesDimKey)
            gIOFBPMSettingDisplaySleepUsesDimKey = OSSymbol::withCString(kIOPMSettingDisplaySleepUsesDimKey);

        if (!gIOFBConfigKey)
            gIOFBConfigKey = OSSymbol::withCString(kIOFBConfigKey);
        if (!gIOFBModesKey)
            gIOFBModesKey = OSSymbol::withCString(kIOFBModesKey);
        if (!gIOFBModeIDKey)
            gIOFBModeIDKey = OSSymbol::withCString(kIOFBModeIDKey);
        if (!gIOFBModeDMKey)
            gIOFBModeDMKey = OSSymbol::withCString(kIOFBModeDMKey);

        if (!gIOFramebufferKey || !gIOFBGetSensorValueKey || !gIOFBRotateKey || !gIOFBPMSettingDisplaySleepUsesDimKey)
            continue;
        
        if (!gAllFramebuffers)
            continue;
        if (!gIOFBRootNotifier)
            gIOFBRootNotifier = getPMRootDomain()->registerInterest(
                                    gIOPriorityPowerStateInterest, &systemPowerChange, 0, 0 );
        if (!gIOFBRootNotifier)
            continue;
        if (!gIOFBClamshellCallout)
            gIOFBClamshellCallout = thread_call_allocate(&delayedEvent, (thread_call_param_t) 0);
        static uint32_t zero = 0;
        if (!gIOFBZero32Data)
            gIOFBZero32Data = OSData::withBytesNoCopy(&zero, sizeof(zero));
        static uint32_t one = 1;
        if (!gIOFBOne32Data)
            gIOFBOne32Data = OSData::withBytesNoCopy(&one, sizeof(one));
        if (!gIOFBGray32Data)
            gIOFBGray32Data = OSData::withBytesNoCopy(&gIOFBGrayValue, sizeof(gIOFBGrayValue));

		if (!gIOGraphicsPrefsVersionKey)
			gIOGraphicsPrefsVersionKey = OSSymbol::withCStringNoCopy(kIOGraphicsPrefsVersionKey);
		if (!gIOGraphicsPrefsVersionValue)
			gIOGraphicsPrefsVersionValue = OSNumber::withNumber(kIOGraphicsPrefsCurrentVersion, 32);

        if (!gIOFBHIDWorkLoop)
        {
            OSIterator * iter = getMatchingServices( nameMatching("IOHIDSystem") );
            if (iter)
            {
                IOService * hidsystem;
                if ((hidsystem = OSDynamicCast(IOService, iter->getNextObject())))
                {
                    gIOFBHIDWorkLoop = hidsystem->getWorkLoop();
                    if (gIOFBHIDWorkLoop)
                        gIOFBHIDWorkLoop->retain();
                }
                iter->release();
            }
        }
        if (!gIOFBHIDWorkLoop)
            continue;
        if (!gIOFBSystemWorkLoop)
        {
			gIOFBSystemWorkLoop = IOGraphicsWorkLoop::workLoop(0, NULL, NULL, NULL);
			if (!gIOFBSystemWorkLoop)
				continue;
			SYSLOCK();
		}
        
        if (!gIOFBWorkES)
        {
            // system work
            gIOFBWorkES = IOInterruptEventSource::interruptEventSource(gIOFBSystemWorkLoop, &systemWork);
            if (gIOFBWorkES)
                gIOFBSystemWorkLoop->addEventSource(gIOFBWorkES);
        }

        if (!gIOFBDelayedPrefsEvent)
        {
            gIOFBDelayedPrefsEvent = IOTimerEventSource::timerEventSource(
                                                    this, &writePrefs);
            if (gIOFBDelayedPrefsEvent)
                gIOFBSystemWorkLoop->addEventSource(gIOFBDelayedPrefsEvent);
        }
        if (!gIOFBServerAckTimer)
        {
            gIOFBServerAckTimer = IOTimerEventSource::timerEventSource(
                                                    this, &serverAckTimeout);
            if (gIOFBServerAckTimer)
                gIOFBSystemWorkLoop->addEventSource(gIOFBServerAckTimer);
        }
        if (!gIOFBDarkWakeTimer)
        {
            gIOFBDarkWakeTimer = IOTimerEventSource::timerEventSource(
                                                    this, &didDarkWake);
            if (gIOFBDarkWakeTimer)
                gIOFBSystemWorkLoop->addEventSource(gIOFBDarkWakeTimer);
        }

        if (!IODisplayWrangler::serverStart())
            continue;

        if (!gIOFBClamshellNotify)
            gIOFBClamshellNotify = addMatchingNotification( gIOPublishNotification,
                                                       resourceMatching(kAppleClamshellStateKey),
                                                       &clamshellHandler, NULL, 0, 10000 );
		readClamshellState();

        if (-1 == gIOFBHaveBacklight) do
        {
            OSDictionary * matching      = nameMatching("backlight");
            OSIterator *   iter          = NULL;
            bool           haveBacklight = false;
            if (matching)
            {
                iter = getMatchingServices(matching);
                matching->release();
            }
            if (iter)
            {
                haveBacklight = (0 != iter->getNextObject());
                iter->release();
            }
            gIOFBHaveBacklight = haveBacklight;

            if (!haveBacklight)
                continue;

            const OSSymbol * settingsArray[2];
            getPMRootDomain()->publishFeature("DisplayDims");   
            // Register to manage the "DisplaySleepUsesDim" setting
            settingsArray[0] = gIOFBPMSettingDisplaySleepUsesDimKey;
            settingsArray[1] = NULL;
            OSObject * pmSettingNotificationHandle;
            getPMRootDomain()->registerPMSettingController(settingsArray, 
                                                            &pmSettingsChange,
                                                            NULL,
                                                            (uintptr_t) NULL,
                                                            &pmSettingNotificationHandle);
        }
        while (false);

        serverNotified   = true;
        serverState      = true;
        
        newController = (!__private->controller);
        if (newController)
        {
            __private->controller = IONew(IOFBController, 1);
            if (!__private->controller)
                panic("IOFBController");
            bzero(__private->controller, sizeof(IOFBController));
            IOService * service = this;
            while ((service = service->getProvider()) && !OSDynamicCast(IOPCIDevice, service))
            	{}
            if (service)
            {
            	__private->controller->device = service;
                __private->controller->name = service->getName();
				if ((data = OSDynamicCast(OSData, service->getProperty("vendor-id"))))
					__private->controller->vendorID = ((uint32_t *) data->getBytesNoCopy())[0];
			}
            else
                __private->controller->name = "FB??";
#if SINGLE_THREAD
            __private->controller->wl = gIOFBSystemWorkLoop;
#else
            __private->controller->wl = IOGraphicsWorkLoop::workLoop(0, 
												NULL, NULL, __private->controller);
#endif
            if (!__private->controller->wl)
                panic("controller->wl");
            __private->controller->workES = IOInterruptEventSource::interruptEventSource(
												this, &controllerAsyncWork);
            if (!__private->controller->workES)
                panic("controller->workES");
			__private->controller->wl->addEventSource(__private->controller->workES);
			__private->controller->didWork = true;
		}

		FBLOCK(this);

		uint32_t depIdx;
        depIDProp = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIDKey));
        openAllDependents = depIDProp && !nextDependent;
        num = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIndexKey));
		depIdx = num ? num->unsigned32BitValue() : 0;
		if (depIdx >= kIOFBControllerMaxFBs)
			panic("%s: bad " kIOFBDependentIndexKey "\n", getName());
		else
		{
			__private->controller->fbs[depIdx] = this;
			if (depIdx >= __private->controller->maxFB)
				__private->controller->maxFB = depIdx;
			__private->controllerIndex = depIdx;
		}

		__private->controller->wsWait  |= (1 << __private->controllerIndex);
		isUsable = true;

		size_t len = strlen(__private->controller->name) + 3;
		char * logName = IONew(char, len);
		if (logName)
			snprintf(logName, len, "%s-%c", __private->controller->name, 'A' + depIdx);
		thisName = logName;

        if (depIDProp)
        {
            do
            {
                OSDictionary * matching;
                OSDictionary * propMatch;
                OSIterator   * iter;
                
                matching = serviceMatching(gIOFramebufferKey);
                if (!matching)
                    continue;
                propMatch = OSDictionary::withCapacity(1);
                if (!propMatch)
                    continue;
                propMatch->setObject( kIOFBDependentIDKey, depIDProp );
                matching->setObject( gIOPropertyMatchKey, propMatch );
                propMatch->release();
                iter = getMatchingServices( matching );
                matching->release();
                if (iter)
                {
                    IOFramebuffer * first = 0;
                    IOFramebuffer * last = 0;
                    
                    while ((next = (IOFramebuffer *) iter->getNextObject()))
                    {
                        next->__private->controller = __private->controller;
                        if (!first)
                            first = next;
                        else if (last)
                            last->setNextDependent( next );
                        last = next;
                    }
                    if (first && last && (first != last))
                        last->setNextDependent( first );
                    iter->release();
                }
            }
            while (false);
        }

        // tell the console if it's on this display, it's going away
        if (isConsoleDevice() /*&& !gIOFBConsoleFramebuffer*/)
            gIOFBConsoleFramebuffer = this;
        if ((this == gIOFBConsoleFramebuffer) || !gIOFBConsoleFramebuffer)
            getPlatform()->setConsoleInfo( 0, kPEDisableScreen);

        deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );

        IOService * provider = getProvider();
        provider->setProperty("AAPL,gray-value", gIOFBGray32Data);
        provider->setProperty("AAPL,gray-page", gIOFBOne32Data);
        data = OSData::withBytesNoCopy(&__private->uiScale, sizeof(__private->uiScale));
		if (data)
		{
			setProperty(kIOFBUIScaleKey, data);
			data->release();
		}

		if (!AbsoluteTime_to_scalar(&__private->controller->initTime))
			AbsoluteTime_to_scalar(&__private->controller->initTime) = mach_absolute_time();

        err = enableController();

        if (kIOReturnSuccess != err)
        {
            dead = true;
            if (nextDependent)
            {
                nextDependent->setNextDependent( NULL );
                nextDependent = NULL;
            }
            deliverDisplayModeDidChangeNotification();
			FBUNLOCK(this);
            continue;
        }

        if (newController)
        {
            if (gIOFBLastController)
                gIOFBLastController->nextController = __private->controller;
			else
                gIOFBAllControllers = __private->controller;
			__private->controller->nextController = gIOFBAllControllers;
			gIOFBLastController = __private->controller;

			if ((obj = copyProperty("AAPL,display-alias", gIOServicePlane)))
			{
				if ((data = OSDynamicCast(OSData, obj)))
					__private->controller->aliasID = (0x80000000 | ((uint32_t *) data->getBytesNoCopy())[0]);
				obj->release();
			}
        }
        thisIndex = gAllFramebuffers->getCount();
        gAllFramebuffers->setObject(this);
        
        setProperty(kIOFramebufferOpenGLIndexKey, thisIndex, 64);

        DEBG(thisName, " this %p \"%s\" \"%s\"\n", this, getName(), getProvider()->getName());

        DEBG1(thisName, " singleth %d, this %p controller %p\n", 
              SINGLE_THREAD, this, __private->controller);

        DEBG1(thisName, " init time now %lld start %lld\n", 
              mach_absolute_time(), AbsoluteTime_to_scalar(&__private->controller->initTime));
        
        pagingState = true;

		obj = copyProperty("graphic-options", gIOServicePlane);
		if (obj)
		{
			data = OSDynamicCast(OSData, obj);
			uint32_t gOpts = ((UInt32 *) data->getBytesNoCopy())[0];
			__private->colorModesAllowed = (data && (0 != (kIOGPlatformYCbCr & gOpts)));
			obj->release();
		}

        if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBTransformKey))))
            __private->selectedTransform = num->unsigned64BitValue();
        __private->selectedTransform |= kIOFBDefaultScalerUnderscan;

        // vbl events
        err = registerForInterruptType( kIOFBVBLInterruptType,
                                        (IOFBInterruptProc) &handleVBL,
                                        this, priv, &__private->vblInterrupt );
        haveVBLService = (err == kIOReturnSuccess );
        __private->vblEnabled = haveVBLService;
        if (haveVBLService)
        {
            __private->deferredVBLDisableEvent = IOInterruptEventSource::interruptEventSource(
															this, &deferredVBLDisable);
			if (__private->deferredVBLDisableEvent)
				__private->controller->wl->addEventSource(__private->deferredVBLDisableEvent);
			__private->vblUpdateTimer = IOTimerEventSource::timerEventSource(this, &updateVBL);
			if (__private->vblUpdateTimer)
				__private->controller->wl->addEventSource(__private->vblUpdateTimer);
		}

        if (haveVBLService
         && (kIOReturnSuccess == getAttribute( kIODeferCLUTSetAttribute, &value ))
         && value)
        {
            __private->deferredCLUTSetEvent = IOInterruptEventSource::interruptEventSource(
															this, &deferredCLUTSetInterrupt);
            if (__private->deferredCLUTSetEvent)
                __private->controller->wl->addEventSource(__private->deferredCLUTSetEvent);

            __private->deferredCLUTSetTimerEvent = IOTimerEventSource::timerEventSource(
            												this, &deferredCLUTSetTimer);
            if (__private->deferredCLUTSetTimerEvent)
                __private->controller->wl->addEventSource(__private->deferredCLUTSetTimerEvent);

            if (__private->deferredCLUTSetEvent || __private->deferredCLUTSetTimerEvent)
                setProperty(kIOFBCLUTDeferKey, kOSBooleanTrue);
        }

        // connect events
		obj = copyProperty(kIOFBConnectInterruptDelayKey, gIOServicePlane);
        if (obj)
        {
            OSData * data;
            if ((data = OSDynamicCast(OSData, obj)))
                __private->delayedConnectTime = *((UInt32 *) data->getBytesNoCopy());
            obj->release();
        }

        err = registerForInterruptType( kIOFBConnectInterruptType,
                                        (IOFBInterruptProc) &connectChangeInterrupt,
                                        this, (void *) __private->delayedConnectTime,
                                        &__private->connectInterrupt );
        // dp events
        __private->dpInterrupDelayTime = 2;
        __private->dpInterruptES = IOTimerEventSource::timerEventSource(this, &dpInterrupt);
        if (__private->dpInterruptES)
            __private->controller->wl->addEventSource(__private->dpInterruptES);
    
        err = registerForInterruptType( kIOFBDisplayPortInterruptType,
                                       (IOFBInterruptProc) &dpInterruptProc,
                                       this, (void *) __private->dpInterrupDelayTime,
                                       &__private->dpInterruptRef );
        __private->dpInterrupts = (kIOReturnSuccess == err);
        __private->dpSupported  = __private->dpInterrupts;
        //

        err = getAttribute( kIOHardwareCursorAttribute, &value );
        haveHWCursor = ((err == kIOReturnSuccess) && (0 != (kIOFBHWCursorSupported & value)));

        clock_interval_to_deadline(40, kMicrosecondScale, 
                                    &__private->defaultI2CTiming.bitTimeout);
        clock_interval_to_deadline(40, kMicrosecondScale, 
                                    &__private->defaultI2CTiming.byteTimeout);
        clock_interval_to_deadline(40, kMicrosecondScale, 
                                    &__private->defaultI2CTiming.acknowledgeTimeout);
        clock_interval_to_deadline(40, kMicrosecondScale, 
                                    &__private->defaultI2CTiming.startTimeout);

        __private->lli2c = (kIOReturnSuccess == getAttributeForConnection(
                                        0, kConnectionSupportsLLDDCSense, 
                                        (uintptr_t *) &__private->defaultI2CTiming));

        if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaWidthKey))))
            __private->desiredGammaDataWidth = num->unsigned32BitValue();
        if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaCountKey))))
            __private->desiredGammaDataCount = num->unsigned32BitValue();
        if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaHeaderSizeKey))))
            __private->gammaHeaderSize = num->unsigned32BitValue();

        __private->deferredSpeedChangeEvent = IOInterruptEventSource::interruptEventSource(
                                                                    this, deferredSpeedChangeEvent);
        if (__private->deferredSpeedChangeEvent)
            __private->controller->wl->addEventSource(__private->deferredSpeedChangeEvent);
        
        if (openAllDependents)
        {
            do
            {
                OSDictionary  * matching;
                OSDictionary  * propMatch;
                OSIterator    * iter;

                matching = serviceMatching(gIOFramebufferKey);
                if (!matching)
                    continue;
                propMatch = OSDictionary::withCapacity(1);
                if (!propMatch)
                    continue;
                propMatch->setObject( kIOFBDependentIDKey, depIDProp );
                matching->setObject( gIOPropertyMatchKey, propMatch );
                propMatch->release();
                iter = getMatchingServices( matching );
                matching->release();
                if (iter)
                {
                    IOFramebuffer * first = 0;
                    IOFramebuffer * last = 0;

                    while ((next = (IOFramebuffer *) iter->getNextObject()))
                    {
                        next->__private->controller = __private->controller;
                        if (!first)
                            first = next;
                        else if (last)
                            last->setNextDependent( next );
                        last = next;
                    }
                    if (first && last && (first != last))
                        last->setNextDependent( first );
                    iter->release();
                }
            }
            while (false);
        }

        opened = true;

        bool nowOnline;
        nowOnline = updateOnline();

        if (!gIOFBConsoleFramebuffer && nowOnline)
            gIOFBConsoleFramebuffer = this;

        __private->paramHandler = IOFramebufferParameterHandler::withFramebuffer(this);
        if (__private->paramHandler)
            setProperty(gIODisplayParametersKey, __private->paramHandler);

        IOFramebufferI2CInterface::create( this );

        __private->online = nowOnline;
        if (nowOnline)
        	displaysOnline(nowOnline);
        else
            IODisplayUpdateNVRAM(this, 0);

        __private->transform = __private->selectedTransform;
        setProperty(kIOFBTransformKey, __private->transform, 64);

        if (openAllDependents)
        {
            next = this;
            while ((next = next->getNextDependent()) && (next != this))
            {
                next->open();
            }
        }

        if (nowOnline)
        {
            deliverDisplayModeDidChangeNotification();
            err = kIOReturnSuccess;
        }
        else
        {
        	findConsole();
            deliverDisplayModeDidChangeNotification();
            dpUpdateConnect();
        }

        if (openAllDependents)
        {
            next = this;
            do
            {
                next->postOpen();
            }
            while ((next = next->getNextDependent()) && (next != this));
        }

		FBUNLOCK(this);
    }
    while (false);

	FBLOCK(this);
    __private->allowSpeedChanges = true;
    if (__private->pendingSpeedChange)
    {
        __private->pendingSpeedChange = false;
    	__private->controller->powerThread = current_thread();
        setAttribute(kIOFBSpeedAttribute, __private->reducedSpeed);
    	__private->controller->powerThread = NULL;
    }
	FBUNLOCK(this);
    
    if (gIOFBSystemWorkLoop)
        SYSUNLOCK();

    return (err);
}

void IOFramebuffer::setTransform( UInt64 newTransform, bool generateChange )
{
    newTransform |= (kIOFBScalerUnderscan & __private->selectedTransform);

    if (newTransform != __private->selectedTransform)
    {
        __private->userSetTransform = generateChange;
        __private->selectedTransform = newTransform;
        if (generateChange)
            connectChangeInterrupt(this, 0);
        else
        {
            __private->transform = newTransform;
            setProperty(kIOFBTransformKey, newTransform, 64);
        }
    }
}

UInt64 IOFramebuffer::getTransform( void )
{
    return (__private->transform);
}

IOReturn IOFramebuffer::selectTransform( UInt64 transform, bool generateChange )
{
    IOFramebuffer * next;
    next = this;
//    do
    {
        next->setTransform(transform, generateChange);
    }
//    while ((next = next->getNextDependent()) && (next != this));

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::probeAll( IOOptionBits options )
{
    IOReturn err = kIOReturnSuccess;

    do
    {
        unsigned int    index;
        IOFramebuffer * fb;

        if (gIOGraphicsControl)
        {
            err = gIOGraphicsControl->requestProbe(options);
			break;
        }
        for (index = 0;
                (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
                index++)
        {
			FBLOCK(fb);
            if (!fb->captured)
			{
				IOReturn
				thisErr = fb->setAttributeForConnection(0, kConnectionProbe, options);
				if (kIOReturnSuccess == err)
					err = thisErr;
			}
			FBUNLOCK(fb);
        }
    }
    while (false);

    return (err);
}

IOReturn IOFramebuffer::requestProbe( IOOptionBits options )
{
    IOReturn err;

    if (!gIOFBSystemWorkLoop || gIOFBSystemWorkLoop->inGate())
        return (kIOReturnNotReady);

    if ((err = extEntry(true)))
        return (err);

#if 0
    if (!__private->online)
    {
		inst->extExit(err);
        return (kIOReturnSuccess);
    }
#endif
    if (kIOFBSetTransform & options)
    {
        options >>= 16;
        selectTransform(options, true);
    }
    else
    {
        if (captured)
        {
            err = kIOReturnBusy;
        }
        else
        {
            AbsoluteTime now;
            AbsoluteTime_to_scalar(&now) = mach_absolute_time();
            if (CMP_ABSOLUTETIME(&now, &gIOFBNextProbeAllTime) >= 0) 
            {
				OSBitOrAtomic(kIOFBEventProbeAll, &gIOFBGlobalEvents);
				startThread(false);
                clock_interval_to_deadline(10, kSecondScale, &gIOFBNextProbeAllTime);
            }
        }
    }

    extExit(err);

    return (kIOReturnSuccess);
}

void IOFramebuffer::initFB(void)
{
	AbsoluteTime now;
    uint64_t nsec;

	if (!__private->online)
	{
		__private->needsInit = false;
		return;
	}
	do
	{
		IOReturn             err;
		IODisplayModeID      mode;
		IOIndex              depth;
		IOPixelInformation	 pixelInfo;
		IOMemoryDescriptor * fbRange;
		uint32_t 			 totalWidth, consoleDepth;
		uint8_t				 logo;
		bool				 timeout = false;

		err = getCurrentDisplayMode(&mode, &depth);
		if (kIOReturnSuccess != err)
			break;
		err = getPixelInformation(mode, depth, kIOFBSystemAperture, &pixelInfo);
		if (kIOReturnSuccess != err)
			break;
		if (pixelInfo.activeWidth < 128)
			break;
	
		if (!vramMap)
		{
			fbRange = getApertureRange(kIOFBSystemAperture);
			if (!fbRange)
				break;
			vramMap = fbRange->map(kIOFBMapCacheMode);
			fbRange->release();
			if (!vramMap)
				break;
		}
		if (pixelInfo.bitsPerComponent > 8)
			consoleDepth = pixelInfo.componentCount * pixelInfo.bitsPerComponent;
		else
			consoleDepth = pixelInfo.bitsPerPixel;
		totalWidth = (pixelInfo.bytesPerRow * 8) / pixelInfo.bitsPerPixel;

		if (false && __private->needsInit == 1)
		{
			AbsoluteTime_to_scalar(&now) = mach_absolute_time();
			SUB_ABSOLUTETIME(&now, &__private->controller->initTime);
			absolutetime_to_nanoseconds(now, &nsec);
			timeout = (nsec > kInitFBTimeoutNS);
			if (timeout) DEBG1(thisName, " init timeout\n");
		}
		// timeout = false;
		logo = ((!timeout) 
			&& (NULL != getProperty("AAPL,boot-display", gIOServicePlane))
			&& (__private->needsInit != 3));
		// logo = 1;
		if (logo)
		{
			PE_Video consoleInfo;
            IOService::getPlatform()->getConsoleInfo(&consoleInfo);
			if ((consoleInfo.v_width == pixelInfo.activeWidth)
				&& (consoleInfo.v_height == pixelInfo.activeHeight))
			{
				if (2 == consoleInfo.v_scale) logo = 2;
			}
			else logo = 0;
		}
		DEBG1(thisName, " initFB: needsInit %d logo %d\n", 
				__private->needsInit, logo);
		IOFramebufferBootInitFB(
			vramMap->getVirtualAddress(), 
			pixelInfo.activeWidth, pixelInfo.activeHeight, 
			totalWidth, consoleDepth, 
			logo);
		DEBG1(thisName, " initFB: done\n");
		if (logo) updateGammaTable(3, 256, 16, NULL, false);
		__private->needsInit = false;
		setProperty(kIOFBNeedsRefreshKey, (0 == logo));
	}
	while (false);
}

IOReturn IOFramebuffer::postOpen( void )
{
	__private->needsInit = true;
	__private->needsInit += (kOSBooleanFalse == getProperty(kIOFBNeedsRefreshKey));
	setProperty(kIOFBNeedsRefreshKey, true);
	initFB();

    if (__private->cursorAttributes)
    {
        __private->cursorAttributes->release();
        __private->cursorAttributes = 0;
    }

    __private->cursorAttributes = OSArray::withCapacity(2);
    if (!__private->cursorAttributes)
        return (kIOReturnNoMemory);

    __private->testingCursor = true;

    setCursorImage( (void *) 0 );

    if (__private->cursorThread)
    {
        IOHardwareCursorDescriptor desc;

        desc.majorVersion       = kHardwareCursorDescriptorMajorVersion;
        desc.minorVersion       = kHardwareCursorDescriptorMinorVersion;
        desc.height             = 256;
        desc.width              = 256;
        desc.bitDepth           = 32;
        desc.maskBitDepth       = 0;
        desc.colorEncodings     = 0;
        desc.flags              = 0;
        desc.supportedSpecialEncodings = kTransparentEncodedPixel;

        (*__private->cursorControl.callouts->setCursorImage) (
            __private->cursorControl.self, __private->cursorControl.ref,
            &desc, (void *) 0 );

        if (__private->controller->wl)
            __private->controller->wl->addEventSource(__private->cursorThread);
    }

    __private->testingCursor = false;

    setProperty( kIOFBCursorInfoKey, __private->cursorAttributes );

    IOService * sensor = 0;
    uintptr_t value[16];

//#define kTempAttribute        kConnectionWSSB
#define kTempAttribute  'thrm'

    if (!__private->temperatureSensor
     && (kIOReturnSuccess == getAttributeForConnection(0, kTempAttribute, &value[0])))
    do
    {
        UInt32     data;
        OSNumber * num;

        num = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIDKey));
        if (num && num->unsigned32BitValue())
            continue;

        sensor = new IOService;
        if (!sensor)
            continue;
        if (!sensor->init())
            continue;

#define kTempSensorName         "temp-sensor"
        sensor->setName(kTempSensorName);
        sensor->setProperty("name", (void *) kTempSensorName, strlen(kTempSensorName) + 1);
        sensor->setProperty("compatible", (void *) kTempSensorName, strlen(kTempSensorName) + 1);
        sensor->setProperty("device_type", (void *) kTempSensorName, strlen(kTempSensorName) + 1);
        sensor->setProperty("type", "temperature");
        sensor->setProperty("location", "GPU");
        data = 0xff000002;
        sensor->setProperty("zone", &data, sizeof(data));
        data = 0x00000001;
        sensor->setProperty("version", &data, sizeof(data));

        OSData * prop;
        IOService * device;
        data = 0x12345678;
        if ((device = getProvider())
         && (prop = OSDynamicCast(OSData, device->getProperty("AAPL,phandle"))))
            data = (*((UInt32 *) prop->getBytesNoCopy())) << 8;
        sensor->setProperty("sensor-id", &data, sizeof(data));

        if (!sensor->attach(this))
            continue;

        sensor->registerService();
        __private->temperatureSensor = sensor;
    }
    while (false);

    if (sensor)
        sensor->release();

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::callPlatformFunction( const OSSymbol * functionName,
                                                    bool waitForFunction,
                                                    void *p1, void *p2,
                                                    void *p3, void *p4 )
{
    uintptr_t   value[16];
    IOReturn ret;

    if (functionName != gIOFBGetSensorValueKey)
        return (super::callPlatformFunction(functionName, waitForFunction, p1, p2, p3, p4));

    FBLOCK(this);
    ret = getAttributeForConnection(0, kTempAttribute, &value[0]);
    FBUNLOCK(this);

    if (kIOReturnSuccess == ret)
        *((UInt32 *)p2) = ((value[0] & 0xffff) << 16);

    return (ret);
}

IOWorkLoop * IOFramebuffer::getControllerWorkLoop() const
{
	return (__private->controller->wl);
}

IOWorkLoop * IOFramebuffer::getGraphicsSystemWorkLoop() const
{
    return (gIOFBSystemWorkLoop);
}

IOWorkLoop * IOFramebuffer::getWorkLoop() const
{
	if (__private && __private->controller)
	return (__private->controller->wl);
	else
		return (NULL);
}

void IOFramebuffer::setCaptured( bool isCaptured )
{
    bool wasCaptured = captured;

    captured = isCaptured;

	DEBG1(thisName, " captured %d -> %d\n", wasCaptured, captured);

    if (wasCaptured != isCaptured)
    {
        if (isCaptured)
            setProperty(kIOFBCapturedKey, kOSBooleanTrue);
        else
            removeProperty(kIOFBCapturedKey);
		deliverFramebufferNotification(kIOFBNotifyCaptureChange, (void *) isCaptured);

		startControllerThread(__private->controller);
    }
}

void IOFramebuffer::setDimDisable( bool dimDisable )
{
    __private->dimDisable = dimDisable;
}

bool IOFramebuffer::getDimDisable( void )
{
    return (__private->dimDisable);
}

void IOFramebuffer::setNextDependent( IOFramebuffer * dependent )
{
    nextDependent = dependent;
}

IOFramebuffer * IOFramebuffer::getNextDependent( void )
{
    return (nextDependent);
}

void IOFramebuffer::close( void )       // called by the user client when
{                                       // the window server exits
    mach_msg_header_t * msgh;
    unsigned int        idx;

    if ((this == gIOFBConsoleFramebuffer) && getPowerState())
        getPlatform()->setConsoleInfo( 0, kPEAcquireScreen);

    msgh = (mach_msg_header_t *) serverMsg;
    if (msgh)
        msgh->msgh_remote_port = MACH_PORT_NULL;

	__private->controller->wsWait |= (1 << __private->controllerIndex);
    serverConnect = 0;
    captured = false;
	setProperty(kIOFBNeedsRefreshKey, true);

    if (gRunawayFramebuffers)
    {
        SYSLOCK();

        idx = gAllFramebuffers->getNextIndexOfObject( this, 0 );
        if (idx != (unsigned int) -1)
            gAllFramebuffers->removeObject(idx);
    
        idx = gRunawayFramebuffers->getNextIndexOfObject( this, 0 );
        if (idx != (unsigned int) -1)
            gRunawayFramebuffers->removeObject(idx);
    
        SYSUNLOCK();

        if (idx != (unsigned int) -1)
        {
            terminate();
        }
    }
}

IODeviceMemory * IOFramebuffer::getVRAMRange( void )
{
    return (getApertureRange(kIOFBSystemAperture));
}

IOReturn IOFramebuffer::setUserRanges( void )
{
#if RLOG
    // print ranges
    uint32_t              i, numRanges;
    IOMemoryDescriptor *    mem;
    numRanges = userAccessRanges->getCount();
    DEBG(thisName, " ranges num:%d\n", numRanges);
    for (i = 0; i < numRanges; i++)
    {
        mem = (IOMemoryDescriptor *) userAccessRanges->getObject( i );
        if (0 == mem)
            continue;
        DEBG(thisName, " start:%llx size:%lx\n",
             mem->getPhysicalSegment(0, 0, kIOMemoryMapperNone), (long) mem->getLength() );
    }
#endif

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::setBackingFramebuffer(const IOPixelInformation * info,
					      uint32_t bufferCount,
					      void * mappedAddress[])
{
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::switchBackingFramebuffer(uint32_t bufferIndex)
{
    return (kIOReturnSuccess);
}

void IOFramebuffer::findConsole(void)
{
    PE_Video newConsole;
    IOFramebuffer * look;
    IOFramebuffer * fb = NULL;
    uintptr_t       value;

    for (uint32_t index = 0;
            (look = (IOFramebuffer *) gAllFramebuffers->getObject(index));
            index++)
    {
        if (!look->__private 
            || !look->__private->framebufferWidth 
            || !look->__private->framebufferHeight
            || !look->__private->online 
            || !look->__private->controller
            || !look->__private->controller->device
            || !look->__private->consoleDepth 
            || (look->__private->consoleDepth > 32))
            continue;

		if ((kIOReturnSuccess == look->getAttribute(kIOVRAMSaveAttribute, &value)) 
			&& !value)
			continue;

        fb = look;
        if ((look == gIOFBConsoleFramebuffer) || !gIOFBConsoleFramebuffer)
            break;
    }

    if (fb)
    {
		DEBG1(fb->thisName, " console set 0x%x000 %d x %d\n",
							pmap_find_phys(kernel_pmap, (addr64_t) fb->frameBuffer),
							fb->__private->framebufferWidth,
							fb->__private->framebufferHeight);
        bzero(&newConsole, sizeof(newConsole));
        newConsole.v_baseAddr   = (unsigned long) fb->frameBuffer;
        newConsole.v_rowBytes   = fb->rowBytes;
        newConsole.v_width      = fb->__private->framebufferWidth;
        newConsole.v_height     = fb->__private->framebufferHeight;
        newConsole.v_depth      = fb->__private->consoleDepth;
        newConsole.v_scale      = fb->__private->uiScale;
        newConsole.v_display    = 1;  // graphics mode for i386
        //      strcpy( consoleInfo->v_pixelFormat, "PPPPPPPP");
        getPlatform()->setConsoleInfo( &newConsole, kPEReleaseScreen );
        getPlatform()->setConsoleInfo( &newConsole, kPEEnableScreen );
        gIOFBConsoleFramebuffer = fb;
        DEBG1(fb->thisName, " now console\n");
    }
}

IOReturn IOFramebuffer::setupForCurrentConfig( void )
{
	TIMESTART();
	if (__private->paramHandler)
		__private->paramHandler->displayModeChange();
	TIMEEND(thisName, "paramHandler->displayModeChange time: %qd ms\n");

    return (doSetup(true));
}

OSData * IOFramebuffer::getConfigMode(IODisplayModeID mode, const OSSymbol * sym)
{
	OSDictionary * dict;
	OSArray * array;
	OSNumber * num;
	unsigned int idx;
	
	dict = OSDynamicCast(OSDictionary, getProperty(gIOFBConfigKey));
	if (!dict) return (0);
	array = OSDynamicCast(OSArray, dict->getObject(gIOFBModesKey));
	if (!array) return (0);
	for (idx = 0; (dict = OSDynamicCast(OSDictionary, array->getObject(idx))); idx++)
	{
		if (!(num = OSDynamicCast(OSNumber, dict->getObject(gIOFBModeIDKey)))) continue;
		if (num->unsigned32BitValue() == (UInt32) mode) break;
	}
	if (!dict) return (0);
	return (OSDynamicCast(OSData, dict->getObject(sym)));
}

IOReturn IOFramebuffer::doSetup( bool full )
{
    StdFBShmem_t *      		shmem = GetShmem(this);
    IOReturn                    err;
    IODisplayModeID             mode;
    IOIndex                     depth;
    IOMemoryDescriptor *        mem;
    IOMemoryDescriptor *        fbRange;
	OSData *				    data;
    IOPhysicalAddress64         base;
    uintptr_t                   value;
    bool                        haveFB = __private->online;

	bzero(&__private->pixelInfo, sizeof(__private->pixelInfo));
	if (haveFB)
	{
		err = getAttribute( kIOHardwareCursorAttribute, &value );
		__private->cursorPanning = ((err == kIOReturnSuccess) && (0 != (kIOFBCursorPans & value)));
	
		err = getCurrentDisplayMode( &mode, &depth );
		if (kIOReturnSuccess == err)
			err = getPixelInformation( mode, depth, kIOFBSystemAperture, &__private->pixelInfo );
		if (kIOReturnSuccess != err)
			bzero(&__private->pixelInfo, sizeof(__private->pixelInfo));
		if (__private->pixelInfo.activeWidth < 128)
			haveFB = false;
		else if ((data = getConfigMode(mode, gIOFBModeDMKey)))
		{
			IODisplayModeInformation * info = (typeof(info)) data->getBytesNoCopy();
			if (info->imageWidth)
			{
				if ((__private->pixelInfo.activeWidth >= 2048)
				 && (__private->pixelInfo.activeHeight >= 1280)
				 && (((254 * __private->pixelInfo.activeWidth) / info->imageWidth) > k2xDPI))
					 __private->uiScale  = 2;
				else __private->uiScale  = 1;
			}
			else __private->uiScale      = 0;
		}
	}

    __private->timingInfo.flags = kIODetailedTimingValid;
    if (haveFB 
     && (kIOReturnSuccess == getTimingInfoForDisplayMode(mode, &__private->timingInfo)))
    {
        if (kIODetailedTimingValid & __private->timingInfo.flags)
        {
        	uint64_t count = ((uint64_t)(__private->timingInfo.detailedInfo.v2.horizontalActive
									+ __private->timingInfo.detailedInfo.v2.horizontalBlanking));
			count *= ((uint64_t)(__private->timingInfo.detailedInfo.v2.verticalActive
								   + __private->timingInfo.detailedInfo.v2.verticalBlanking));
			if (kIOInterlacedCEATiming & __private->timingInfo.detailedInfo.v2.signalConfig)
				count >>= 1;

			uint64_t clock  = __private->timingInfo.detailedInfo.v2.pixelClock;
			uint64_t actual = __private->timingInfo.detailedInfo.v2.minPixelClock;

			DEBG1(thisName, " minPixelClock %qd maxPixelClock %qd\n", 
							__private->timingInfo.detailedInfo.v2.maxPixelClock,
							__private->timingInfo.detailedInfo.v2.minPixelClock);

			if (actual != __private->timingInfo.detailedInfo.v2.maxPixelClock)
				actual = 0;
			bool throttleEnable = (gIOFBVBLThrottle && clock && actual && shmem);
			DEBG1(thisName, " vblthrottle(%d) clk %qd act %qd\n", throttleEnable, clock, actual);
			if (throttleEnable)
				clock = actual;
            setProperty(kIOFBCurrentPixelClockKey, clock, 64);
            setProperty(kIOFBCurrentPixelCountKey, count, 64);
            if (shmem && throttleEnable)
            {
				mach_timebase_info_data_t timebaseInfo;
				clock_timebase_info(&timebaseInfo);
				AbsoluteTime_to_scalar(&shmem->vblDelta) 
						= (count * kSecondScale * timebaseInfo.numer / clock / timebaseInfo.denom);
			}
			__private->vblThrottle = throttleEnable;
			__private->setupMode = mode;
        }
        else
        {
            removeProperty(kIOFBCurrentPixelClockKey);
            removeProperty(kIOFBCurrentPixelCountKey);
			if (shmem && __private->vblThrottle)
				AbsoluteTime_to_scalar(&shmem->vblDelta) = 0;
        }

        __private->scaledMode = false;
        if (kIOScalingInfoValid & __private->timingInfo.flags)
        {
            __private->scaledMode =
            (__private->timingInfo.detailedInfo.v2.scalerFlags
             || __private->timingInfo.detailedInfo.v2.horizontalScaledInset
             || __private->timingInfo.detailedInfo.v2.verticalScaledInset
             || (__private->timingInfo.detailedInfo.v2.horizontalScaled 
                    && (__private->timingInfo.detailedInfo.v2.horizontalScaled != __private->timingInfo.detailedInfo.v2.horizontalActive))
             || (__private->timingInfo.detailedInfo.v2.verticalScaled 
                    && (__private->timingInfo.detailedInfo.v2.verticalScaled != __private->timingInfo.detailedInfo.v2.verticalActive)));
        }
    }
    else
		__private->timingInfo.flags = 0;

    if (full && (fbRange = getApertureRange( kIOFBSystemAperture )))
    {
        userAccessRanges->removeObject( kIOFBSystemAperture );
        userAccessRanges->setObject( kIOFBSystemAperture, fbRange );
        err = setUserRanges();

        base = fbRange->getPhysicalSegment(0, 0, kIOMemoryMapperNone);
        if ((mem = getVRAMRange()))
        {
            vramMapOffset = base - mem->getPhysicalSegment(0, 0, kIOMemoryMapperNone);
            if (vramMapOffset > mem->getLength())
                vramMapOffset &= (mem->getLength() - 1);
            setProperty( kIOFBMemorySizeKey, mem->getLength(), 32 );
            mem->release();
        }

        IOMemoryMap * oldMap = vramMap;
        vramMap = fbRange->map( kIOFBMapCacheMode );
		if (oldMap)
			oldMap->release();
        assert( vramMap );
        if (vramMap)
        {
            base = vramMap->getVirtualAddress();
            frameBuffer = (volatile unsigned char *) base;
        }

        DEBG1(thisName, " using (%dx%d,%d bpp)\n",
             (uint32_t) __private->pixelInfo.activeWidth, (uint32_t) __private->pixelInfo.activeHeight, 
             (uint32_t) __private->pixelInfo.bitsPerPixel );

		if (fbRange)
			fbRange->release();
    }

    if (full)
    {
        deliverDisplayModeDidChangeNotification();

        dpUpdateConnect();
    }
    
DEBG1(thisName, " doSetup vram %d, fb %d\n", vramMap != NULL, haveFB);
    if (vramMap && haveFB)
    {
		if (__private->needsInit) initFB();
        setupCursor();
    }
    else
    {
        cursorBlitProc   = (CursorBlitProc)   NULL;
        cursorRemoveProc = (CursorRemoveProc) NULL;
        __private->framebufferWidth  = 0;
        __private->framebufferHeight = 0;
    }

    // reset console
    if (haveFB || !gIOFBConsoleFramebuffer)
        findConsole();

    return (kIOReturnSuccess);
}

bool IOFramebuffer::suspend(bool now)
{
    if (now == suspended)
        return (true);

    if (now)
    {
		stopCursor();
		checkDeferredCLUTSet();
		if (this == gIOFBConsoleFramebuffer)
		{
			getPlatform()->setConsoleInfo( 0, kPEDisableScreen);
			gIOFBConsoleFramebuffer = 0;
		}
		deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
        suspended = true;
    }
    else
    {
		TIMESTART();
		setupForCurrentConfig();
		TIMEEND(thisName, "exit suspend setupForCurrentConfig time: %qd ms\n");

        suspended = false;
        __private->modeUpdate = false;
    }

    return (false);
}

IOReturn IOFramebuffer::extSetDisplayMode(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst        = (IOFramebuffer *) target;
    IODisplayModeID displayMode = args->scalarInput[0];
    IOIndex         depth       = args->scalarInput[1];
    IOReturn        err;

	err = inst->doSetDisplayMode(displayMode, depth);

	return (err);
}

IOReturn IOFramebuffer::doSetDisplayMode(
		IODisplayModeID displayMode, IOIndex depth)
{
    IOReturn        err;

    DEBG1(thisName, " extSetDisplayMode(0x%x, %d) susp %d online %d\n", 
    		(int32_t) displayMode, (uint32_t) depth, suspended,
            __private->online);

    if (kIODisplayModeIDAliasBase & displayMode)
    {
    	// && (depth == __private->currentDepth))

		if ((err = extEntry(false)))
			return (err);

        DEBG(thisName, " nop set mode\n");
		__private->aliasMode = displayMode & ~kIODisplayModeIDAliasBase;
		__private->modeUpdate = false;
		extExit(err);

        return (kIOReturnSuccess);
    }

    if ((err = extEntrySys(false)))
        return (err);

   	if ((kIODisplayModeIDCurrent == displayMode) 
   		&& __private->modeUpdate)
	{
        getCurrentDisplayMode(&displayMode, &depth);
	}

	suspend(true);

   	if (kIODisplayModeIDCurrent != displayMode)
	{
		TIMESTART();
		err = setDisplayMode( displayMode, depth );
		TIMEEND(thisName, "setDisplayMode time: %qd ms\n");
		__private->aliasMode    = kIODisplayModeIDInvalid;
		__private->currentDepth = depth;
	}

    suspend(false);

	extExitSys(err);

    return (err);
}

IOReturn IOFramebuffer::checkMirrorSafe( UInt32 value, IOFramebuffer * other )
{
    IOReturn        err = kIOReturnSuccess;
    IOFramebuffer * next = this;

    while ((next = next->getNextDependent()) && (next != this))
    {
		DEBG1(next->thisName, " transform 0x%llx \n", __private->transform);
        if (~kIOFBScalerUnderscan & (__private->transform ^ next->getTransform()))
        {
            err = kIOReturnUnsupported;
            break;
        }
    }

    return (err);
}

IOReturn IOFramebuffer::extSetMirrorOne(uint32_t value, IOFramebuffer * other)
{
    IOReturn    err;
    IOFramebuffer * next;
    uintptr_t   data[2];
    bool        was;

	if (value && __private->nextMirror)   return (kIOReturnBusy);
	if (value && !other)                  return (kIOReturnBadArgument);
	if (!value && !__private->nextMirror) return (kIOReturnSuccess);
	if (!value && __private->nextMirror)
	{
		next = this;
		do
		{
			next->suspend(true);
			data[0] = value;
			data[1] = (uintptr_t) next->__private->nextMirror;
			DEBG1(next->thisName, " kIOMirrorAttribute(0)\n"); 
			err = next->setAttribute(kIOMirrorAttribute, (uintptr_t) &data);
			DEBG1(next->thisName, " kIOMirrorAttribute(%d) ret 0x%x\n", value, err); 
		}
		while ((next = next->__private->nextMirror) && (next != this));

		next = this;
		do
		{
			IOFramebuffer * prev;
			prev = next;
			next = next->__private->nextMirror;
			prev->__private->nextMirror = 0;
			prev->suspend(false);
		}
		while (next && (next != this));

		return (kIOReturnSuccess);
	}

	was = suspend(true);
	data[0] = value;
	data[1] = (uintptr_t) other;
	err = setAttribute(kIOMirrorAttribute, (uintptr_t) &data);
	DEBG1(thisName, " kIOMirrorAttribute(%d) ret 0x%x\n", value, err); 
	if (kIOReturnSuccess != err)
	{
		if (!was) suspend(false);
	}
	else
	{
		__private->nextMirror = other;
		if (other->__private->nextMirror)
		{
			next = other;
			do next->suspend(false);
			while ((next = next->__private->nextMirror) && (next != other));
		}
	}
	return (err);
}

IOReturn IOFramebuffer::extSetAttribute(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst      = (IOFramebuffer *) target;
    IOSelect        attribute = args->scalarInput[0];
    uint32_t        value     = args->scalarInput[1];
    IOFramebuffer * other     = (IOFramebuffer *) reference;

    IOReturn    err;

    if ((err = inst->extEntry(false)))
        return (err);
    
    switch (attribute)
    {
        case kIOMirrorAttribute:

            DEBG1(inst->thisName, " kIOMirrorAttribute(%d) susp(%d), curr(%d)\n", 
                    value, inst->suspended, (inst->__private->nextMirror != 0));
            value = (value != 0);
            if (value)
            {
                err = inst->checkMirrorSafe(value, other);
                if (kIOReturnSuccess != err)
                    break;
            }
			err = inst->extSetMirrorOne(value, other);
            break;

        default:
            err = inst->setAttribute( attribute, value );
            break;
    }

	inst->extExit(err);

    return (err);
}

IOReturn IOFramebuffer::extGetAttribute(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst      = (IOFramebuffer *) target;
    IOSelect        attribute = args->scalarInput[0];
    uint64_t *      value     = &args->scalarOutput[0];
    IOFramebuffer * other     = (IOFramebuffer *) reference;

    IOReturn    err = kIOReturnSuccess;
    
	*value = 0;

    switch (attribute)
    {
        case kIOFBProcessConnectChangeAttribute:
            err = inst->extProcessConnectionChange();
            break;

        case kIOFBEndConnectChangeAttribute:
			if ((err = inst->extEntrySys(true)))
				return (err);
            err = inst->extEndConnectionChange();
			inst->extExitSys(err);
            break;

        case kIOFBWSStartAttribute:
			if ((err = inst->extEntrySys(true)))
				return (err);
        	if (inst->__private->controller->wsWait)
        	{
				DEBG1(inst->thisName, " kIOFBWSStartAttribute wsWait %d\n", inst->__private->controller->wsWait);
				inst->__private->controller->wsWait &= ~(1 << inst->__private->controllerIndex);
				if (!inst->__private->controller->wsWait)
				{
				DEBG1(inst->thisName, " wsWait done remsg %d\n", inst->messaged);
				if (inst->messaged)
				{
					inst->messageClients(kIOMessageServiceIsSuspended, (void *) true);
				}
				resetClamshell();
				startControllerThread(inst->__private->controller);
			}
			}
			inst->extExitSys(err);
            break;

        default:

			if ((err = inst->extEntry(false)))
				return (err);

			uintptr_t result = (uintptr_t) other;
			err = inst->getAttribute( attribute, &result );
			*value = (UInt32) result;

			inst->extExit(err);
            break;
    }


    return (err);
}

IOReturn IOFramebuffer::extGetInformationForDisplayMode(
        OSObject * target, void * reference, IOExternalMethodArguments * args)
{
    IOFramebuffer * inst   = (IOFramebuffer *) target;
    IODisplayModeID mode   = args->scalarInput[0];
    void *          info   = args->structureOutput;
    IOByteCount     length = args->structureOutputSize;

    UInt32                       flags = 0;
    IOReturn                     err;
    bool                         getTiming;
    IOFBDisplayModeDescription * out = (IOFBDisplayModeDescription *) info;

    if (length < sizeof(IODisplayModeInformation))
        return (kIOReturnBadArgument);

    if ((err = inst->extEntry(false)))
        return (err);

    err = inst->getInformationForDisplayMode( mode, &out->info );
    if (kIOReturnSuccess == err)
    {
        err = IODisplayWrangler::getFlagsForDisplayMode( inst, mode, &flags);
        if (kIOReturnSuccess == err)
        {
            out->info.flags &= ~kDisplayModeSafetyFlags;
            out->info.flags |= flags;
        }
        getTiming = (length >= sizeof(IOFBDisplayModeDescription));
        out->timingInfo.flags = getTiming ? kIODetailedTimingValid : 0;
        if (kIOReturnSuccess != inst->getTimingInfoForDisplayMode(mode, &out->timingInfo))
        {
            out->timingInfo.flags &= ~kIODetailedTimingValid;
            out->timingInfo.appleTimingID = 0;
        }
    }

	inst->extExit(err);

    return (err);
}

IOReturn IOFramebuffer::setDisplayAttributes(OSObject * obj)
{
    IOReturn       r, ret = kIOReturnSuccess;
    OSData *       data;
    OSDictionary * dict;
	uint32_t *     attributes;
	uint32_t       idx, max;
	uint32_t       attr, attrValue, value, mask;
	uint32_t       controllerDepths, ditherMask = 0;
	uintptr_t      lvalue[16];
	bool           found = false;
	bool           skip = false;
	bool           updatesMode = false;

	if (!obj) return (kIOReturnSuccess);

	obj->retain();
	if (__private->displayAttributes) __private->displayAttributes->release();
	__private->displayAttributes = obj;

	if (__private->display) __private->display->setProperty(kIODisplayAttributesKey, obj);

	dict = OSDynamicCast(OSDictionary, obj);
	data = dict ? OSDynamicCast(OSData, dict->getObject(kIODisplayAttributesKey))
			    : OSDynamicCast(OSData, obj);
		
	if (!data) return (kIOReturnSuccess);

	attributes = (uint32_t *) data->getBytesNoCopy();
	max        = data->getLength() / sizeof(attributes[0]);

	for (idx = 0; idx < max; idx += 2)
	{
		attr      = attributes[idx];
		attrValue = attributes[idx + 1];

		if (kConnectionVendorTag == attr)
		{
			if (found)
				break;
			skip = (attrValue && (attrValue != __private->controller->vendorID));
			found = !skip;
			continue;
		}
		if (skip)
			continue;

		DEBG1(thisName, " (0x%08x '%c%c%c%c')(%x)\n", attr, FEAT(attr), attrValue);

		updatesMode = false;
		switch (attr)
		{
		  case kConnectionColorModesSupported:
			attrValue |= kIODisplayColorModeAuto;
			attrValue |= kIODisplayColorModeRGBLimited;
			__private->colorModesSupported = attrValue;
			break;

		  case kConnectionColorDepthsSupported:

			r = getAttributeForConnection(0, kConnectionControllerDepthsSupported, &lvalue[0]);
			if (kIOReturnSuccess != r)
			{
				lvalue[0] =  kIODisplayRGBColorComponentBits6
						| kIODisplayRGBColorComponentBits8;
				if (10 == __private->desiredGammaDataWidth)
					lvalue[0] |= kIODisplayRGBColorComponentBits10;
			}
				
			controllerDepths = lvalue[0];

#define COLRMASK(n)   ((n ## 16) | (n ## 14) | (n ## 12) | (n ## 10) | (n ## 8) | (n ## 6))
#define HIGHBIT(v)    (1 << (31 - __builtin_clz(v)))

			value = controllerDepths & attrValue;
			if (!value)
				value = kIODisplayRGBColorComponentBits8;

			mask = COLRMASK(kIODisplayRGBColorComponentBits);
			if (value & mask)
			{
				value = (value & ~mask) | HIGHBIT(value & mask);
				if (!(value & HIGHBIT(controllerDepths & mask)))
					ditherMask |= (kIODisplayDitherAll << kIODisplayDitherRGBShift);
			}

			mask = COLRMASK(kIODisplayYCbCr444ColorComponentBits);
			if (value & mask)
			{
				value = (value & ~mask) | HIGHBIT(value & mask);
				if (!(value & HIGHBIT(controllerDepths & mask)))
					ditherMask |= (kIODisplayDitherAll << kIODisplayDitherYCbCr444Shift);
			}

			mask = COLRMASK(kIODisplayYCbCr422ColorComponentBits);
			if (value & mask)
			{
				value = (value & ~mask) | HIGHBIT(value & mask);
				if (!(value & HIGHBIT(controllerDepths & mask)))
					ditherMask |= (kIODisplayDitherAll << kIODisplayDitherYCbCr422Shift);
			}

			// pass thru kConnectionColorDepthsSupported
			DEBG1(thisName, " (0x%08x '%c%c%c%c')(%x)\n", attr, FEAT(attr), attrValue);
			r = setAttributeForConnection(0, attr, attrValue);
			if (kIOReturnSuccess != r)
				ret = r;

			attr = kConnectionControllerColorDepth;
			attrValue = value;
			updatesMode = true;
			break;

		  case kConnectionControllerDitherControl:

			attrValue &= ditherMask;
			updatesMode = true;
			break;
		}

		DEBG1(thisName, " (0x%08x '%c%c%c%c')(%x)\n", attr, FEAT(attr), attrValue);

		if (updatesMode)
		{
			r = getAttributeForConnection(0, attr, &lvalue[0]);
			if (kIOReturnSuccess != r)
				continue;
			if (lvalue[0] == attrValue)
				continue;
		}
		r = setAttributeForConnection(0, attr, attrValue);
		if (kIOReturnSuccess == r)
			__private->modeUpdate |= updatesMode;
		else
			ret = r;
	}

	return (ret);
}

IOReturn IOFramebuffer::extSetProperties( OSDictionary * props )
{
    OSDictionary * dict;
    OSArray *      array;
    IOReturn       err = kIOReturnUnsupported;

    if ((err = extEntry(true)))
        return (err);

    err = kIOReturnUnsupported;
    if ((dict = OSDynamicCast(OSDictionary, props->getObject(gIOFBConfigKey)))) do
    {
        setProperty( gIOFBConfigKey, dict );
        if (!__private->online)
        {
            err = kIOReturnSuccess;
            break;
        }
        if (dict->getObject("IOFBScalerUnderscan"))
        {
            __private->enableScalerUnderscan = true;
        }
        if ((array = OSDynamicCast(OSArray,
                                   dict->getObject(kIOFBDetailedTimingsKey))))
            err = setDetailedTimings( array );
        else
            err = kIOReturnSuccess;
    }
    while (false);

	setDisplayAttributes(props->getObject(kIODisplayAttributesKey));
    
	extExit(err);

    return (err);
}

//// Controller attributes

IOReturn IOFramebuffer::setAttribute( IOSelect attribute, uintptr_t value )
{
    IOReturn        ret;

    switch (attribute)
    {
        case kIOCapturedAttribute:
        {
            DEBG(thisName, " kIOCapturedAttribute(%ld)\n", value);
			if (value != gIOFBCaptureState)
			{
				gIOFBCaptureState = value;
				OSBitOrAtomic(kIOFBEventCaptureSetting, &gIOFBGlobalEvents);
				controllerDidWork(__private->controller, kWorkStateChange);
			}
            ret = kIOReturnSuccess;
            break;
        }
        
        case kIOCursorControlAttribute:
            {
                IOFBCursorControlAttribute * crsrControl;

                crsrControl = (IOFBCursorControlAttribute *) value;

                if (__private->cursorThread)
                {
                    __private->cursorThread->release();
                    __private->cursorThread = 0;
                }

                if (crsrControl && crsrControl->callouts)
                {
                    __private->cursorControl = *((IOFBCursorControlAttribute *) value);
                    __private->cursorThread = IOInterruptEventSource::interruptEventSource(this, &cursorWork);
                    if (__private->controller->wl && __private->cursorThread)
                        __private->controller->wl->addEventSource(__private->cursorThread);
                }
                ret = kIOReturnSuccess;
                break;
            }

		case kIOPowerStateAttribute:
            ret = setAttribute(kIOPowerAttribute, value);
            break;
            
        case kIOPowerAttribute:
            ret = setAttributeExt(attribute, value);
            break;
            
        default:
            ret = kIOReturnUnsupported;
            break;
    }

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if __ppc__
static IOReturn
ApplePMUSendMiscCommand( UInt32 command,
                            IOByteCount sendLength, UInt8 * sendBuffer,
                            IOByteCount * readLength, UInt8 * readBuffer )
{
    struct SendMiscCommandParameterBlock
    {
        int command;
        IOByteCount sLength;
        UInt8 *sBuffer;
        IOByteCount *rLength;
        UInt8 *rBuffer;
    };
    IOReturn ret = kIOReturnError;
    static IOService * pmu;

    // See if ApplePMU exists
    if (!pmu)
    {
        OSIterator * iter;
        iter = IOService::getMatchingServices(IOService::serviceMatching("ApplePMU"));
        if (iter)
        {
            pmu = (IOService *) iter->getNextObject();
            iter->release();
        }
    }

    SendMiscCommandParameterBlock params = { command, sendLength, sendBuffer,
                                            readLength, readBuffer };
    if (pmu)
        ret = pmu->callPlatformFunction( "sendMiscCommand", true,
                                            (void*)&params, NULL, NULL, NULL );
    return (ret);
}
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::readClamshellState()
{
    // zero -> lid open

#if __ppc__

#define readExtSwitches 0xDC

    UInt8 bootEnvIntData[32];
    IOByteCount iLen = sizeof(UInt8);

    ret = ApplePMUSendMiscCommand(readExtSwitches, 0, NULL, &iLen, &bootEnvIntData[0]);
    if (kIOReturnSuccess == ret)
    {
		gIOFBLastClamshellState = bootEnvIntData[0];
    }

#elif defined(__i386__) || defined(__x86_64__)
    static IOACPIPlatformDevice * lidDevice;
    UInt32 lidState;

    if (!lidDevice)
    {
        OSIterator * iter;
        IOService *  service;

        iter = IOService::getMatchingServices(IOService::nameMatching("PNP0C0D"));
        if (iter)
        {                       
            service = (IOService *)iter->getNextObject();
            if (service->metaCast("IOACPIPlatformDevice"))
            {
                lidDevice = (IOACPIPlatformDevice *) service;
                lidDevice->retain();
            }
            iter->release();
        }
    }

    if (lidDevice)
    {
    	IOReturn ret;
        ret = lidDevice->evaluateInteger("_LID", &lidState);
        if (kIOReturnSuccess == ret)
            gIOFBLastClamshellState = (lidState == 0);
    }
#endif

    DEBG1("S", " %d\n", (int) gIOFBLastClamshellState);
}

extern "C" IOReturn
IOGetHardwareClamshellState( IOOptionBits * result )
{
    // zero -> lid open
	if (gIOFBDesktopModeAllowed)
		*result = gIOFBLastClamshellState;
	else
		*result = 0;

	gIOFBLastReadClamshellState = gIOFBLastClamshellState;

    DEBG1("S", " %d\n", (int) *result);

    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFramebuffer::clamshellHandler(void * target, void * ref,
                                       IOService * resourceService, IONotifier * notifier)
{
    gIOResourcesAppleClamshellState = resourceService->getProperty(kAppleClamshellStateKey);
    resourceService->removeProperty(kAppleClamshellStateKey);
	OSBitOrAtomic(kIOFBEventReadClamshell, &gIOFBGlobalEvents);
	startThread(false);
	return (true);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::getAttribute( IOSelect attribute, uintptr_t * value )
{
    IOReturn ret = kIOReturnUnsupported;

    switch (attribute)
    {
      case kIOClamshellStateAttribute:
        IOOptionBits result;
        ret = IOGetHardwareClamshellState( &result );
        *value = result;
        break;

      default:
        break;
    }

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFramebuffer::setNumber( OSDictionary * dict, const char * key,
                               UInt32 value )
{
    OSNumber *  num;
    bool        ok;

    num = OSNumber::withNumber( value, 32 );
    if (!num)
        return (false);

    ok = dict->setObject( key, num );
    num->release();

    return (ok);
}

bool IOFramebuffer::serializeInfo( OSSerialize * s )
{
    IOReturn                    err;
    IODisplayModeInformation    info;
    IOPixelInformation          pixelInfo;
    IODisplayModeID *           modeIDs;
    IOItemCount                 modeCount, modeNum, aperture;
    IOIndex                     depthNum;
    OSDictionary *              infoDict;
    OSDictionary *              modeDict;
    OSDictionary *              pixelDict;
    char                        keyBuf[12];
    bool                        ok = true;

    modeCount = getDisplayModeCount();
    modeIDs = IONew( IODisplayModeID, modeCount );
    if (!modeIDs)
        return (false);

    err = getDisplayModes( modeIDs );
    if (err)
        return (false);

    infoDict = OSDictionary::withCapacity( 10 );
    if (!infoDict)
        return (false);

    for (modeNum = 0; modeNum < modeCount; modeNum++)
    {
        err = getInformationForDisplayMode( modeIDs[modeNum], &info );
        if (err)
            continue;

        modeDict = OSDictionary::withCapacity( 10 );
        if (!modeDict)
            break;

        ok = setNumber( modeDict, kIOFBWidthKey,
                        info.nominalWidth )
             && setNumber( modeDict, kIOFBHeightKey,
                           info.nominalHeight )
             && setNumber( modeDict, kIOFBRefreshRateKey,
                           info.refreshRate )
             && setNumber( modeDict, kIOFBFlagsKey,
                           info.flags );
        if (!ok)
            break;

        for (depthNum = 0; depthNum < info.maxDepthIndex; depthNum++)
        {
            for (aperture = 0; ; aperture++)
            {
                err = getPixelInformation( modeIDs[modeNum], depthNum,
                                           aperture, &pixelInfo );
                if (err)
                    break;

                pixelDict = OSDictionary::withCapacity( 10 );
                if (!pixelDict)
                    continue;

                ok = setNumber( pixelDict, kIOFBBytesPerRowKey,
                                pixelInfo.bytesPerRow )
                     && setNumber( pixelDict, kIOFBBytesPerPlaneKey,
                                   pixelInfo.bytesPerPlane )
                     && setNumber( pixelDict, kIOFBBitsPerPixelKey,
                                   pixelInfo.bitsPerPixel )
                     && setNumber( pixelDict, kIOFBComponentCountKey,
                                   pixelInfo.componentCount )
                     && setNumber( pixelDict, kIOFBBitsPerComponentKey,
                                   pixelInfo.bitsPerComponent )
                     && setNumber( pixelDict, kIOFBFlagsKey,
                                   pixelInfo.flags )
                     && setNumber( pixelDict, kIOFBWidthKey,
                                   pixelInfo.activeWidth )
                     && setNumber( pixelDict, kIOFBHeightKey,
                                   pixelInfo.activeHeight );
                if (!ok)
                    break;

                snprintf(keyBuf, sizeof(keyBuf), "%x", (int) (depthNum + (aperture << 16)));
                modeDict->setObject( keyBuf, pixelDict );
                pixelDict->release();
            }
        }

        snprintf(keyBuf, sizeof(keyBuf), "%x", (int) modeIDs[modeNum]);
        infoDict->setObject( keyBuf, modeDict );
        modeDict->release();
    }

    IODelete( modeIDs, IODisplayModeID, modeCount );

    ok &= infoDict->serialize( s );
    infoDict->release();

    return (ok);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(_IOFramebufferNotifier, IONotifier)
#define LOCKNOTIFY()
#define UNLOCKNOTIFY()

void _IOFramebufferNotifier::remove()
{
    LOCKNOTIFY();

    if (whence)
    {
        whence->removeObject( (OSObject *) this );
        whence = 0;
    }

    fEnable = false;

    UNLOCKNOTIFY();

    release();
}

bool _IOFramebufferNotifier::disable()
{
    bool        ret;

    LOCKNOTIFY();
    ret = fEnable;
    fEnable = false;
    UNLOCKNOTIFY();

    return (ret);
}

void _IOFramebufferNotifier::enable( bool was )
{
    LOCKNOTIFY();
    fEnable = was;
    UNLOCKNOTIFY();
}

IONotifier * IOFramebuffer::addFramebufferNotification(
    IOFramebufferNotificationHandler handler,
    OSObject * self, void * ref)
{
    _IOFramebufferNotifier *    notify = 0;

    notify = new _IOFramebufferNotifier;
    if (notify && !notify->init())
    {
        notify->release();
        notify = 0;
    }

    if (notify)
    {
        notify->handler = handler;
        notify->self = self;
        notify->ref = ref;
        notify->fEnable = true;

		if (__private && __private->controller)
			FBLOCK(this);
		else
			SYSLOCK();
        if (0 == fbNotifications)
            fbNotifications = OSSet::withCapacity(1);

        notify->whence = fbNotifications;
        if (fbNotifications)
            fbNotifications->setObject( notify );
		if (__private && __private->controller)
			FBUNLOCK(this);
		else
			SYSUNLOCK();
    }

    return (notify);
}

IOReturn IOFramebuffer::deliverFramebufferNotification(
    IOIndex event, void * info )
{
    OSIterator *                iter;
    _IOFramebufferNotifier *    notify;
    IOReturn                    ret = kIOReturnSuccess;
    IOReturn                    r;

#if RLOG1
    const char * name = NULL;
    switch (event)
    {
        case kIOFBNotifyDisplayModeWillChange:
            name = "kIOFBNotifyDisplayModeWillChange";
            break;
        case kIOFBNotifyDisplayModeDidChange:
            name = "kIOFBNotifyDisplayModeDidChange";
            break;
        case kIOFBNotifyWillSleep:
            name = "kIOFBNotifyWillSleep";
            break;
        case kIOFBNotifyDidWake:
            name = "kIOFBNotifyDidWake";
            break;
        case kIOFBNotifyDidPowerOff:
            name = "kIOFBNotifyDidPowerOff";
            break;
        case kIOFBNotifyWillPowerOn:
            name = "kIOFBNotifyWillPowerOn";
            break;
        case kIOFBNotifyWillPowerOff:
            name = "kIOFBNotifyWillPowerOff";
            break;
        case kIOFBNotifyDidPowerOn:
            name = "kIOFBNotifyDidPowerOn";
            break;
#if 0
        case kIOFBNotifyWillChangeSpeed:
            name = "kIOFBNotifyWillChangeSpeed";
            break;
        case kIOFBNotifyDidChangeSpeed:
            name = "kIOFBNotifyDidChangeSpeed";
            break;
        case kIOFBNotifyDisplayDimsChange:
            name = "kIOFBNotifyDisplayDimsChange";
            break;
#endif
        case kIOFBNotifyClamshellChange:
            name = "kIOFBNotifyClamshellChange";
            break;
        case kIOFBNotifyCaptureChange:
            name = "kIOFBNotifyCaptureChange";
            break;
        case kIOFBNotifyOnlineChange:
            name = "kIOFBNotifyOnlineChange";
            break;
    }
#endif
    
    LOCKNOTIFY();

#if RLOG1
    AbsoluteTime startTime, endTime;
    uint64_t nsec;
	AbsoluteTime_to_scalar(&startTime) = mach_absolute_time();
#endif

    iter = OSCollectionIterator::withCollection( fbNotifications );

    if (iter)
    {
        while ((notify = (_IOFramebufferNotifier *) iter->getNextObject()))
        {
            if (notify->fEnable)
            {
                r = (*notify->handler)( notify->self, notify->ref, this,
                                        event, info );
                if (kIOReturnSuccess != r)
                    ret = r;
            }
        }
        iter->release();
    }

#if RLOG1
	AbsoluteTime_to_scalar(&endTime) = mach_absolute_time();
    SUB_ABSOLUTETIME(&endTime, &startTime);
    absolutetime_to_nanoseconds(endTime, &nsec);

	bool notGated = (!gIOFBSystemWorkLoop->inGate() || !FBWL(this)->inGate());

	if (name || notGated)
	{
    	DEBG1(thisName, " %s(%s(%d), %p) %qd ms\n", 
                    notGated ? "not gated " : "",
                    name ? name : "", (uint32_t) event, info,
                    nsec / 1000000ULL);
	}
#endif
    
    UNLOCKNOTIFY();

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Some stubs

IOReturn IOFramebuffer::enableController ( void )
{
    return (kIOReturnSuccess);
}

bool IOFramebuffer::isConsoleDevice( void )
{
    return (false);
}

// Set display mode and depth
IOReturn IOFramebuffer::setDisplayMode( IODisplayModeID /* displayMode */,
                                        IOIndex /* depth */ )
{
    return (kIOReturnUnsupported);
}

// For pages
IOReturn IOFramebuffer::setApertureEnable(
    IOPixelAperture /* aperture */, IOOptionBits /* enable */ )
{
    return (kIOReturnUnsupported);
}

// Display mode and depth for startup
IOReturn IOFramebuffer::setStartupDisplayMode(
    IODisplayModeID /* displayMode */, IOIndex /* depth */ )
{
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::getStartupDisplayMode(
    IODisplayModeID * /* displayMode */, IOIndex * /* depth */ )
{
    return (kIOReturnUnsupported);
}

//// CLUTs

IOReturn IOFramebuffer::setCLUTWithEntries(
    IOColorEntry * /* colors */, UInt32 /* index */,
    UInt32 /* numEntries */, IOOptionBits /* options */ )
{
    return (kIOReturnUnsupported);
}

//// Gamma

IOReturn IOFramebuffer::setGammaTable( UInt32 /* channelCount */,
                                       UInt32 /* dataCount */, UInt32 /* dataWidth */, void * /* data */ )
{
    return (kIOReturnUnsupported);
}


//// Display mode timing information

IOReturn IOFramebuffer::getTimingInfoForDisplayMode(
    IODisplayModeID /* displayMode */,
    IOTimingInformation * /* info */ )
{
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::validateDetailedTiming(
    void * description, IOByteCount descripSize )
{
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setDetailedTimings( OSArray * array )
{
    return (kIOReturnUnsupported);
}

//// Connections

IOItemCount IOFramebuffer::getConnectionCount( void )
{
    return (1);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::setAttributeExt( IOSelect attribute, uintptr_t value )
{
	IOReturn err;

	FBLOCK(this);

	switch (attribute)
	{
        case kIOPowerAttribute:

            DEBG1(thisName, " mux power change %d->%ld, gated %d, thread %d\n", 
            	pendingPowerState, value,
                gIOFBSystemWorkLoop->inGate(), gIOFBSystemWorkLoop->onThread());

            if (value != pendingPowerState)
            {
                pendingPowerState = value;
				if (!__private->controllerIndex)
				{
					__private->controller->pendingMuxPowerChange = true;
					startControllerThread(__private->controller);
				}
            }
            err = kIOReturnSuccess;
			break;

		default:
			err = setAttribute(attribute, value);
			break;
	}

	FBUNLOCK(this);
	return (err);
}

IOReturn IOFramebuffer::getAttributeExt( IOSelect attribute, uintptr_t * value )
{
	IOReturn err;

	FBLOCK(this);
	err = getAttribute(attribute, value);
	switch (attribute)
	{
		case kIOMirrorAttribute:
			if (kIOReturnSuccess != err) *value = 0;
			if (__private->nextMirror) *value |= kIOMirrorIsMirrored;
			err = kIOReturnSuccess;
			break;

		default:
			break;
	}
	FBUNLOCK(this);
	return (err);
}

IOReturn IOFramebuffer::setAttributeForConnectionExt( IOIndex connectIndex,
           IOSelect attribute, uintptr_t value )
{
	IOReturn err;

	if (opened)
		FBLOCK(this);
	else
		return (kIOReturnNotReady);

	if ('\0igr' == attribute)
	{

		DEBG1(thisName, " 0igr ->0x%lx, gated %d, thread %d\n", value,
                gIOFBSystemWorkLoop->inGate(), gIOFBSystemWorkLoop->onThread());

		__private->controller->mute = (0 != (value & (1 << 31)));
	}

	err = setAttributeForConnection(connectIndex, attribute, value);

	if (opened)
		FBUNLOCK(this);

	return (err);
}

IOReturn IOFramebuffer::getAttributeForConnectionExt( IOIndex connectIndex,
        IOSelect attribute, uintptr_t * value )
{
	IOReturn err;

	FBLOCK(this);
	err = getAttributeForConnection(connectIndex, attribute, value);
	FBUNLOCK(this);

	return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::getAttributeForConnectionParam( IOIndex connectIndex,
        IOSelect attribute, uintptr_t * value )
{
	IOReturn err;

	if (!__private->colorModesAllowed)
	{
		if (kConnectionColorMode == attribute)
			return (kIOReturnUnsupported);
		if (kConnectionColorModesSupported == attribute) 
			return (kIOReturnUnsupported);
	}

	err = getAttributeForConnection(connectIndex, attribute, value);

	return (err);
}

IOReturn IOFramebuffer::setAttributeForConnectionParam( IOIndex connectIndex,
           IOSelect attribute, uintptr_t value )
{
	IOReturn err;

	err = setAttributeForConnection(connectIndex, attribute, value);

	return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::setAttributeForConnection( IOIndex connectIndex,
        IOSelect attribute, uintptr_t info )
{
    IOReturn err;

    switch( attribute )
    {
        case kConnectionRedGammaScale:
            if (info != __private->gammaScale[0])
            {
                __private->gammaScale[0] = info;
                __private->gammaScaleChange = true;
            }
            err = kIOReturnSuccess;
            break;

        case kConnectionGreenGammaScale:
            if (info != __private->gammaScale[1])
            {
                __private->gammaScale[1] = info;
                __private->gammaScaleChange = true;
            }
            err = kIOReturnSuccess;
            break;

        case kConnectionBlueGammaScale:
            if (info != __private->gammaScale[2])
            {
                __private->gammaScale[2] = info;
                __private->gammaScaleChange = true;
            }
            err = kIOReturnSuccess;
            break;

        case kConnectionOverscan:

            UInt64 newTransform;
            DEBG(thisName, " set oscn %ld, ena %d\n", info, __private->enableScalerUnderscan);
            if (info) 
                newTransform = __private->selectedTransform & ~kIOFBScalerUnderscan;
            else
                newTransform = __private->selectedTransform | kIOFBScalerUnderscan;
            if (__private->enableScalerUnderscan)
            {
                if (newTransform != __private->selectedTransform)
                {
                    __private->selectedTransform = newTransform;
                    if (!suspended)
                        connectChangeInterrupt(this, 0);
                    else
                    {
                        __private->transform = newTransform;
                        setProperty(kIOFBTransformKey, newTransform, 64);
                    }
                }
                err = kIOReturnSuccess;
                break;
            }

            /* fall thru */

        default:
            err = kIOReturnUnsupported;
            break;
    }

    return( err );
}

IOReturn IOFramebuffer::flushParameters(void)
{
    if (__private->gammaScaleChange)
    {
        __private->gammaScaleChange = false;
        updateGammaTable(__private->rawGammaChannelCount, __private->rawGammaDataCount, 
                         __private->rawGammaDataWidth, __private->rawGammaData);
    }
    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::getAttributeForConnection( IOIndex connectIndex,
        IOSelect attribute, uintptr_t * value )
{
    IOReturn err;
    uintptr_t result;

    switch( attribute )
    {
        case kConnectionDisplayParameterCount:
            result = 3;		// 3 gamma scales
            if (__private->enableScalerUnderscan)
                result++;
            *value = result;
            err = kIOReturnSuccess;
            break;

        case kConnectionDisplayParameters:
			result = 0;
            value[result++] = kConnectionRedGammaScale;
            value[result++] = kConnectionGreenGammaScale;
            value[result++] = kConnectionBlueGammaScale;
            if (__private->enableScalerUnderscan)
                value[result++] = kConnectionOverscan;
            err = kIOReturnSuccess;
            break;

        case kConnectionOverscan:
            if (__private->enableScalerUnderscan)
            {
                value[0] = (0 == (kIOFBScalerUnderscan & __private->selectedTransform));
                DEBG(thisName, " oscn %ld (%qx)\n", value[0], __private->selectedTransform);
                value[1] = 0;
                value[2] = 1;
                err = kIOReturnSuccess;
            }
            else
                err = kIOReturnUnsupported;
            break;

        case kConnectionRedGammaScale:
            value[0] = __private->gammaScale[0];
            value[1] = 0;
            value[2] = (1 << 16);
            err = kIOReturnSuccess;
            break;

        case kConnectionGreenGammaScale:
            value[0] = __private->gammaScale[1];
            value[1] = 0;
            value[2] = (1 << 16);
            err = kIOReturnSuccess;
            break;

        case kConnectionBlueGammaScale:
            value[0] = __private->gammaScale[2];
            value[1] = 0;
            value[2] = (1 << 16);
            err = kIOReturnSuccess;
            break;

        case kConnectionCheckEnable:
            err = getAttributeForConnection(connectIndex, kConnectionEnable, value);
            break;

        case kConnectionSupportsHLDDCSense:
            if (__private->lli2c)
            {
                err = kIOReturnSuccess;
                break;
            }
            // fall thru
        default:
            err = kIOReturnUnsupported;
            break;
    }

    return( err );
}

//// HW Cursors

IOReturn IOFramebuffer::setCursorImage( void * cursorImage )
{
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setCursorState( SInt32 x, SInt32 y, bool visible )
{
    return (kIOReturnUnsupported);
}

void IOFramebuffer::flushCursor( void )
{}

//// Interrupts

IOReturn IOFramebuffer::registerForInterruptType( IOSelect interruptType,
        IOFBInterruptProc proc, OSObject * target, void * ref,
        void ** interruptRef )
{
    if ((interruptType != kIOFBMCCSInterruptType) || !__private->dpInterrupts)
		return (kIOReturnNoResources);

	interruptType = kIOFBMCCSInterruptRegister;
	__private->interruptRegisters[interruptType].handler = proc;
	__private->interruptRegisters[interruptType].target  = target;
	__private->interruptRegisters[interruptType].ref     = ref;
	__private->interruptRegisters[interruptType].state   = true;
	*interruptRef = &__private->interruptRegisters[interruptType];

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::unregisterInterrupt(void * interruptRef)
{
	uintptr_t index;
    IOReturn  err;

	index = ((IOFBInterruptRegister *) interruptRef) - &__private->interruptRegisters[0];
    if (interruptRef == &__private->interruptRegisters[index])
    {
        __private->interruptRegisters[index].handler = 0;
        err = kIOReturnSuccess;
    }
    else
    {
        err = kIOReturnUnsupported;
    }

    return (err);
}

IOReturn IOFramebuffer::setInterruptState(void * interruptRef, UInt32 state)
{
	uintptr_t index;
    IOReturn  err;

	index = ((IOFBInterruptRegister *) interruptRef) - &__private->interruptRegisters[0];
    if (interruptRef == &__private->interruptRegisters[index])
    {
        __private->interruptRegisters[index].state = state;
        err = kIOReturnSuccess;
    }
    else
    {
        err = kIOReturnUnsupported;
    }

    return (err);
}

// Apple sensing

IOReturn IOFramebuffer::getAppleSense(
    IOIndex  /* connectIndex */,
    UInt32 * /* senseType */,
    UInt32 * /* primary */,
    UInt32 * /* extended */,
    UInt32 * /* displayType */ )
{
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::connectFlags( IOIndex /* connectIndex */,
                                      IODisplayModeID /* displayMode */, IOOptionBits * /* flags */ )
{
    return (kIOReturnUnsupported);
}

//// IOLowLevelDDCSense

void IOFramebuffer::setDDCClock( IOIndex /* connectIndex */, UInt32 /* value */ )
{}

void IOFramebuffer::setDDCData( IOIndex /* connectIndex */, UInt32 /* value */ )
{}

bool IOFramebuffer::readDDCClock( IOIndex /* connectIndex */ )
{
    return (false);
}

bool IOFramebuffer::readDDCData( IOIndex /* connectIndex */ )
{
    return (false);
}

IOReturn IOFramebuffer::enableDDCRaster( bool /* enable */ )
{
    return (kIOReturnUnsupported);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//// IOHighLevelDDCSense

enum { kDDCBlockSize = 128 };

bool IOFramebuffer::hasDDCConnect( IOIndex connectIndex )
{
    return (__private->lli2c);
}

IOReturn IOFramebuffer::getDDCBlock( IOIndex bus, UInt32 blockNumber,
                                        IOSelect blockType, IOOptionBits options,
                                        UInt8 * data, IOByteCount * length )
{
    UInt8               startAddress;
    IOReturn            err;
    UInt32              badsums, timeouts;
    IOI2CBusTiming *    timing = &__private->defaultI2CTiming;

    if (!__private->lli2c)
        return (kIOReturnUnsupported);
    
    // Assume that we have already attempted to stop DDC1
    
    // Read the requested block (Block 1 is at 0x0, each additional block is at 0x80 offset)
    startAddress = kDDCBlockSize * (blockNumber - 1);
    if (length)
        *length = kDDCBlockSize;
    
    // Attempt to read the DDC data
    //  1.      If the error is a timeout, then it will attempt one more time.  If it gets another timeout, then
    //          it will return a timeout error to the caller.
    //
    //  2.  If the error is a bad checksum error, it will attempt to read the block up to 2 more times.
    //          If it still gets an error, then it will return a bad checksum error to the caller.  
    i2cSend9Stops(bus, timing);
    badsums = timeouts = 0;
    do
    {
        err = readDDCBlock(bus, timing, 0xa0, startAddress, data);
        if (kIOReturnSuccess == err)
            break;
        IOLog("readDDCBlock returned error\n");
        i2cSend9Stops(bus, timing);

        // We got an error.   Determine what kind
        if (kIOReturnNotResponding == err)
        {
            IOLog("timeout\n");
            timeouts++;
        }
        else if (kIOReturnUnformattedMedia == err)
        {
            IOLog("bad sum\n");
            badsums++;
        }
        else
            break;
    }
    while ((timeouts < 2) && (badsums < 4));

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      doI2CRequest(), 
//

IOReturn IOFramebuffer::doI2CRequest( UInt32 bus, IOI2CBusTiming * timing, IOI2CRequest * request )
{
    IOReturn err = kIOReturnError;      // Assume failure

    if (!__private->lli2c)
        return (kIOReturnUnsupported);

    if (!timing)
        timing = &__private->defaultI2CTiming;
    
    if (request->sendTransactionType == kIOI2CSimpleTransactionType)
    {
        if ( request->sendAddress & 0x01 )
        {
            // Read Transaction
            //
            err = i2cReadData(bus, timing, request->sendAddress, request->sendBytes, (UInt8 *) request->sendBuffer);
        }
        else
        {
            // Read Transaction
            //
            err = i2cWriteData(bus, timing, request->sendAddress, request->sendBytes, (UInt8 *) request->sendBuffer);
        }
    }

    // Now, let's check to see if there is a csReplyType
    //
    if (request->replyTransactionType == kIOI2CDDCciReplyTransactionType )
    {
        err = i2cReadDDCciData(bus, timing, request->replyAddress, request->replyBytes, (UInt8 *) request->replyBuffer);
    }
    else if (request->replyTransactionType == kIOI2CSimpleTransactionType )
    {
        err = i2cReadData(bus, timing, request->replyAddress, request->replyBytes, (UInt8 *) request->replyBuffer);
    }

    request->result = err;
    if (request->completion)
        (*request->completion)(request);

    err = kIOReturnSuccess;

    return (err);
}

/*
    with thanks to:
    File:       GraphicsCoreUtils.c
    Written by: Sean Williams, Kevin Williams, Fernando Urbina
*/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      stopDDC1SendCommand(), 
//
//      The VESA spec for DDC says a display in DDC1 will transition from DDC1 to DDC2 when a valid DDC2
//      command is received.
//      DDC1 constantly spews data on the data line if syncs are active...bad for macintosh sensing
//      DDC2 only sends data when requested.
//      The VESA spec illustrates the manner to do this.
//      Read the first byte of data, send a Nack and a Stop.
//      This routine does that.
//
//      There is a delay of two vertical clock periods where the clock line is forced low. The 
//      NEC XE15 monitor has a controller that sometimes pulls the clockline low and never releases it.
//      This is bad, DDC will fail, and the monitor sensing algorithim will think a mono 1152x870 display
//      is attached. This isn't part of the spec but it fixes the NEC XE15.
//

IOReturn IOFramebuffer::stopDDC1SendCommand(IOIndex bus, IOI2CBusTiming * timing)
{
    UInt8       data;
    IOReturn    err = kIOReturnSuccess; 
    UInt8       address;

    // keep clock line low for 2 vclocks....keeps NEC XE15 from locking clock line low
    // 640x480@67hz has a veritcal frequency of 15 ms
    // 640x480@60hz has a vertical frequency of 16.7 ms
    // Lower the clock line for 34 milliseconds

    setDDCClock(bus, kIODDCLow);
    IOSleep( 34 );

    address = 0;
    err = i2cWrite(bus, timing, 0xa0, 1, &address);

    if (kIOReturnSuccess == err)
    {                   
        i2cStart(bus, timing);
        
        i2cSendByte(bus, timing, 0xa1 );
        
        err = i2cWaitForAck(bus, timing);
        if (kIOReturnSuccess == err)
            err = i2cReadByte(bus, timing, &data);
    }
    
    i2cSendNack(bus, timing);
    i2cStop(bus, timing);

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cReadData()
//
//      The parameters are described as follows:
//
//                      -> deviceAddress        device's I2C address
//                      -> count                # of bytes to read
//                      <- buffer               buffer for the data

IOReturn IOFramebuffer::i2cReadData(IOIndex bus, IOI2CBusTiming * timing,
                                    UInt8 deviceAddress, UInt8 count, UInt8 * buffer)
{
    IOReturn    err = kIOReturnError;
    UInt32      attempts = 10;
    
    while ((kIOReturnSuccess != err) && (attempts-- > 0))
    {
        // Attempt to read the I2C data
        i2cSend9Stops(bus, timing);
        err = i2cRead(bus, timing, deviceAddress, count, buffer);
    }
    
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cWriteData()
//
//      The parameters are described as follows:
//
//                      -> deviceAddress        device's I2C address
//                      -> count                # of bytes to read
//                      -> buffer               buffer for the data

IOReturn IOFramebuffer::i2cWriteData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 * buffer)
{
    IOReturn    err = kIOReturnError;
    UInt32      attempts = 10;
    
    while ((kIOReturnSuccess != err) && (attempts-- > 0))
    {
        // Attempt to write the I2C data
        i2cSend9Stops(bus, timing);
        err = i2cWrite(bus, timing, deviceAddress, count, buffer);
    }
    
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      WaitForDDCDataLine()
//
//      Watch the DDC data line and see if it is toggling. If the data line is toggling, it means
//      1) DDC display is connected
//      2) DDC controller in the display is ready to receive commands.
//
//                      -> waitTime             max duration that the DDC data line should be watched

void IOFramebuffer::waitForDDCDataLine(IOIndex bus, IOI2CBusTiming * timing, UInt32 waitTime)
{
    AbsoluteTime        now, expirationTime;
    UInt32              dataLine;

    setDDCData(bus, kIODDCTristate);            // make sure data line is tristated

    // Set up the timeout timer...watch DDC data line for waitTime, see if it changes
    clock_interval_to_deadline(waitTime, kMillisecondScale, &expirationTime);
                            
    dataLine = readDDCData(bus);                // read present state of dataline
    
    while (true)
    {
        if (dataLine != readDDCData(bus))
            break;
        
        AbsoluteTime_to_scalar(&now) = mach_absolute_time();
        if (CMP_ABSOLUTETIME(&now, &expirationTime) > 0)
            break;
    }
}
        
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      readDDCBlock()
//      Read one block of DDC data
//
//      The parameters are described as follows:
//
//                      -> deviceAddress        device's I2C address
//                      -> startAddress         start address to get data from
//                      <- data                 a block of EDID data

IOReturn IOFramebuffer::readDDCBlock(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 startAddress, UInt8 * data)
{
    IOReturn    err;
    UInt32      i;
    UInt8       sum = 0;
    
    // First, send the address/data as a write
    err = i2cWrite(bus, timing, deviceAddress, 1, &startAddress);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
    
    // Now, read the I2C data
    err = i2cRead(bus, timing, deviceAddress, kDDCBlockSize, data);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
    
    for (i = 0; i < kDDCBlockSize; i++)
        sum += data[i];

    if (sum)
        err = kIOReturnUnformattedMedia;
    
ErrorExit:

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cStart()
//      Start a I2C transaction

void IOFramebuffer::i2cStart(IOIndex bus, IOI2CBusTiming * timing)
{
    // Generates a Start condition:
    
    // Set DATA and CLK high and enabled
    setDDCData(bus, kIODDCHigh);
    setDDCClock(bus, kIODDCHigh);

    IODelay( 100 );
    
    // Bring DATA low
    setDDCData(bus, kIODDCLow);
    IODelay( 100 );
    
    // Bring CLK low
    setDDCClock(bus, kIODDCLow);
    IODelay( 100 );
}       

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cStop()

void IOFramebuffer::i2cStop(IOIndex bus, IOI2CBusTiming * timing)
{
    // Generate a low to high transition on DATA
    // while SCL is high
    
    // Bring DATA and CLK low
    IODelay( 200 );
    setDDCData(bus, kIODDCLow);
    setDDCClock(bus, kIODDCLow);

    IODelay( 100 );
    
    // Bring CLK High
    setDDCClock(bus, kIODDCHigh);
    IODelay( 200 );
    
    // Bring DATA High
    setDDCData(bus, kIODDCHigh);
    IODelay( 100 );

    // Release Bus

    setDDCData(bus, kIODDCTristate);
    setDDCClock(bus, kIODDCTristate);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cSendAck()
//      Send an ACK to acknowledge we received the data

void IOFramebuffer::i2cSendAck(IOIndex bus, IOI2CBusTiming * timing)
{
    // Here, we have to make sure that the CLK is low while
    // we bring DATA low and pulse CLK
    setDDCClock(bus, kIODDCLow);

    // This routine will release the bus by
    // tristating the CLK and DATA lines
    IODelay(20);

    // should we wait for the SDA to be high before going on???

    IODelay( 40 );

    // Bring SDA low
    setDDCData(bus, kIODDCLow);
    IODelay( 100 );
    
    // pulse the CLK
    setDDCClock(bus, kIODDCHigh);
    IODelay( 200 );
    setDDCClock(bus, kIODDCLow);
    IODelay( 40 );
    
    // Release SDA,
    setDDCData(bus, kIODDCTristate);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cSendNack()
//      Send an ACK to acknowledge we received the data

void IOFramebuffer::i2cSendNack(IOIndex bus, IOI2CBusTiming * timing)
{
    // Here, we have to make sure that the CLK is low while
    // we bring DATA high and pulse CLK
    setDDCClock(bus, kIODDCLow);

    // This routine will release the bus by
    // tristating the CLK and DATA lines
    IODelay( 20 );
    // should we wait for the SDA to be high before going on???

    IODelay( 40 );

    // Bring SDA high
    setDDCData(bus, kIODDCHigh);
    IODelay( 100 );
    
    // pulse the CLK
    setDDCClock(bus, kIODDCHigh);
    IODelay( 200 );
    setDDCClock(bus, kIODDCLow);
    IODelay( 40 );
    
    // Release SDA,
    setDDCData(bus, kIODDCTristate);
    IODelay( 100 );
    IODelay( 100 );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cWaitForAck()
//      This routine will poll the SDA line looking for a LOW value and when it finds it, it will pulse
//      the CLK.

IOReturn IOFramebuffer::i2cWaitForAck(IOIndex bus, IOI2CBusTiming * timing)
{
    AbsoluteTime        now, expirationTime;
    IOReturn            err = kIOReturnSuccess;
    
    IODelay( 40 );
    
    // Set up a watchdog timer that will time us out, in case we never see the SDA LOW.
    clock_interval_to_deadline(1, kMillisecondScale, &expirationTime);

    while ((0 != readDDCData(bus)) && (kIOReturnSuccess == err))
    {
        AbsoluteTime_to_scalar(&now) = mach_absolute_time();
        if (CMP_ABSOLUTETIME(&now, &expirationTime) > 0)
            err = kIOReturnNotResponding;                               // Timed Out
    }
    
    // OK, now pulse the clock (SDA is not enabled), the CLK
    // should be low here.
    IODelay( 40 );
    setDDCClock(bus, kIODDCHigh);
    IODelay( 200 );
    setDDCClock(bus, kIODDCLow);
    IODelay( 40 );

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cSendByte()
//      Send a byte of data
//

void IOFramebuffer::i2cSendByte(IOIndex bus, IOI2CBusTiming * timing, UInt8 data)
{
    UInt8       valueToSend;
    int         i;
    
    // CLK should be low when entering this routine
    // and will be low when exiting
    
    for ( i = 0 ; i < 8; i++ )
    {
        // Wait a bit
        IODelay( 100 );

        // Get the bit
        valueToSend = ( data >> (7 - i)) & 0x01;

        // Send it out
        setDDCData(bus, valueToSend);
        
        // Wait for 40 us and then pulse the clock
        
        IODelay( 40 );
        // Raise the CLK line
        setDDCClock(bus, kIODDCHigh);

        IODelay( 200 );
        // Lower the clock line
        setDDCClock(bus, kIODDCLow);
        
        // Wait a bit
        IODelay( 40 );
    }
    
    // Tristate the DATA while keeping CLK low
    setDDCData(bus, kIODDCTristate);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cReadByte()
//      Read a byte of data

IOReturn IOFramebuffer::i2cReadByte(IOIndex bus, IOI2CBusTiming * timing, UInt8 *data)
{
    AbsoluteTime        now, expirationTime;
    IOReturn            err = kIOReturnSuccess;
    UInt32              i;
    UInt32              value;

    // Make sure that DATA is Tristated and that Clock is low
    setDDCClock(bus, kIODDCLow);
    setDDCData(bus, kIODDCTristate);
    
    for (i = 0 ; (kIOReturnSuccess == err) && (i < 8); i++)
    {
        // Wait for 1 msec and then pulse the clock
        IODelay( 100 );
        // Release the CLK line
        setDDCClock(bus, kIODDCTristate);
        
        // Wait for a slow device by reading the SCL line until it is high
        // (A slow device will keep it low).  The DDC spec suggests a timeout
        // of 2ms here.
        // Setup for a timeout
        clock_interval_to_deadline(2, kMillisecondScale, &expirationTime);

        while ((0 == readDDCClock(bus)) && (kIOReturnSuccess == err))
        {
            AbsoluteTime_to_scalar(&now) = mach_absolute_time();
            if (CMP_ABSOLUTETIME(&now, &expirationTime) > 0)
                err = kIOReturnNotResponding;                   // Timed Out
        }

        // Read the data
        value = readDDCData(bus);
        *data |= (value << (7-i));
        
        //we keep clock high for when sending bits....so do same here. Ensures display sees clock.
        // reach 100% success rate with NEC XE15
        
        IODelay( 200 );
        // Lower the clock line
        setDDCClock(bus, kIODDCLow);
        IODelay( 40 );
    }
    
    return (err);
}               

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cWaitForBus()
//      Tristate DDC Clk and DDC Data lines

IOReturn IOFramebuffer::i2cWaitForBus(IOIndex bus, IOI2CBusTiming * timing)
{
    // should we wait for the bus here?
    
    setDDCClock(bus, kIODDCTristate);
    setDDCData(bus, kIODDCTristate);
    IODelay( 200 );
    
    return kIOReturnSuccess;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cReadDDCciData()
//
//      The parameters are described as follows:
//
//                      -> deviceAddress                                device's I2C address
//                      -> count                                                # of bytes to read
//                      <- buffer                                               buffer for the data
//

IOReturn IOFramebuffer::i2cReadDDCciData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 *buffer)
{
    // This is a funky call that encodes the length of the transaction in the response. 
    // According to the VESA DDC/ci spec, the low 7 bits of second byte returned by the display 
    // will contain the length of the message less the checksum.  The card should then attempt to read 
    // that length plus the checksum but should not exceed "count" bytes.  
    // If the size exceeds "count", then the buffer should be filled with "count" bytes and the
    // transaction should be completed without copying more bytes into the buffer.
    
    IOReturn    err = kIOReturnSuccess;
    UInt32      i;
    UInt8       readLength;
    UInt32      bufferSize;
    Boolean     reportShortRead = false;
    
    // Assume that the bufferSize == count
    bufferSize = count;
    
    err = i2cWaitForBus(bus, timing);
    if( kIOReturnSuccess != err ) goto ErrorExit;
            
    i2cStart(bus, timing);
    
    i2cSendByte(bus, timing, deviceAddress | 0x01 );
            
    err = i2cWaitForAck(bus, timing);
    if( kIOReturnSuccess != err ) goto ErrorExit;
            
    for ( i = 0; i < bufferSize; i++ )
    {
        err = i2cReadByte(bus, timing, &buffer[i] );
        if( kIOReturnSuccess != err ) goto ErrorExit;
        
        i2cSendAck(bus, timing);
        
        if ( i == 1 )
        {
            // We have read the 2nd byte, so adjust the
            // bufferSize accordingly
            //
            readLength = buffer[i] & 0x07;
            if ( (readLength + 1) <= count )
            {
                // The read amount is less than our bufferSize, so
                // adjust that size
                //
                bufferSize = (readLength + 1);
            }
            else
            {
                // The amount to read  > than our bufferSize
                // so only read up to our buffer size and remember to
                // report that we didn't read all the data
                //
                reportShortRead = true;
            }
        }
    }

    
ErrorExit:
    i2cSendNack(bus, timing);
    i2cStop(bus, timing);
                    
    if ( reportShortRead )
            err = kIOReturnOverrun;
            
    return (err);

}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cRead()
//      Read a bunch of data via I2C
//
//      The parameters are described as follows:
//
//                      -> deviceAddress                device's I2C address
//                      -> numberOfBytes                number of bytes to read
//                      <- data                         the requested number of bytes of data

IOReturn IOFramebuffer::i2cRead(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 numberOfBytes, UInt8 * data)
{
    IOReturn    err = kIOReturnSuccess;
    int         i;
    
    err = i2cWaitForBus(bus, timing);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
            
    i2cStart(bus, timing);
    
    i2cSendByte(bus, timing, deviceAddress | 0x01 );
            
    err = i2cWaitForAck(bus, timing);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
            
    for (i = 0; i < numberOfBytes; i++)
    {
        data[i] = 0;
        err = i2cReadByte(bus, timing, &data[i] );
        if (kIOReturnSuccess != err)
            break;
        if (i != (numberOfBytes - 1))
            i2cSendAck(bus, timing);
    }
    
ErrorExit:
    i2cSendNack(bus, timing);
    i2cStop(bus, timing);
                    
    return (err);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      i2cWrite()
//      Write a bunch of data via I2C
//
//      The parameters are described as follows:
//
//                      -> deviceAddress                device's I2C address
//                      -> numberOfBytes                number of bytes to write
//                      -> data                         the number of bytes of data

IOReturn IOFramebuffer::i2cWrite(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 numberOfBytes, UInt8 * data)
{
    IOReturn    err = kIOReturnSuccess;
    UInt32      i;
    
    err = i2cWaitForBus(bus, timing);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
            
    i2cStart(bus, timing);
    
    i2cSendByte(bus, timing, deviceAddress);
            
    err = i2cWaitForAck(bus, timing);
    if (kIOReturnSuccess != err)
        goto ErrorExit;
            
    for (i = 0; i < numberOfBytes; i++)
    {
        i2cSendByte(bus, timing, data[i] );
        err = i2cWaitForAck(bus, timing);
        if (kIOReturnSuccess != err)
            break;
    }

                    
ErrorExit:

    if (kIOReturnSuccess != err)
        i2cStop(bus, timing);
            
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//
//      i2cSend9Stops()
//
//      Assume we are reading the DDC data, and display misses a few clocks, we send ack, display misses ack
//      The display might still be holding down the data line. Whenever we get an error, send nine stops.
//      If the display is waiting for a clock before going to the next bit, the stop will be interpreted
//      as a clock. It will go onto the next bit. Whenever it has finished writing the eigth bit, the
//      next stop will look like a stop....the display will release the bus.
//      8 bits, 9 stops. The display should see at least one stop....

void IOFramebuffer::i2cSend9Stops(IOIndex bus, IOI2CBusTiming * timing)
{
    for (UInt32 i = 0; i < 9; i++)
        i2cStop(bus, timing);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::setPreferences( IOService * props, OSDictionary * prefs )
{
    if (!gIOFBPrefs)
    {
        prefs->retain();
        gIOFBPrefs = prefs;
        gIOFBPrefsParameters = OSDynamicCast(OSDictionary,
                                        props->getProperty(kIOGraphicsPrefsParametersKey));
        gIOFBIgnoreParameters = OSDynamicCast(OSDictionary,
                                        props->getProperty(kIOGraphicsIgnoreParametersKey));

    }
    return (kIOReturnSuccess);
}

OSObject * IOFramebuffer::copyPreferences( void )
{
    if (gIOFBPrefsSerializer)
        gIOFBPrefsSerializer->retain();
    return (gIOFBPrefsSerializer);
}

OSObject * IOFramebuffer::copyPreference( IODisplay * display, const OSSymbol * key )
{
    OSDictionary *   dict;
    OSObject *       value = 0;

    if (!gIOFBPrefs)
        return (value);

	if (!__private->displayPrefKey && display)
		__private->displayPrefKey = (const OSSymbol *) display->copyProperty(kIODisplayPrefKeyKey);
    if (!__private->displayPrefKey)
        return (value);

    if ((dict = OSDynamicCast(OSDictionary, gIOFBPrefs->getObject(__private->displayPrefKey))))
    {
        value = dict->getObject(key);
        if (value)
            value->retain();
    }

    return (value);
}

bool IOFramebuffer::getIntegerPreference( IODisplay * display, const OSSymbol * key, UInt32 * value )
{
    bool found = false;
    OSObject *
    pref = copyPreference(display, key);
    if (pref)
    {
        OSNumber * num;
        if ((num = OSDynamicCast(OSNumber, pref)))
        {
            value[0] = num->unsigned32BitValue();
            DEBG1(thisName, " found(%s) %s = %d\n", __private->displayPrefKey->getCStringNoCopy(), 
            										key->getCStringNoCopy(), (uint32_t) value[0]);
            found = true;
        }
        pref->release();
    }
    return (found);
}

bool IOFramebuffer::setPreference( IODisplay * display, const OSSymbol * key, OSObject * value )
{
    OSDictionary *   dict;
    OSObject *       oldValue = 0;
    bool             madeChanges = false;

    if (!gIOFBPrefs)
        return (false);

	if (!__private->displayPrefKey && display)
		__private->displayPrefKey = (const OSSymbol *) display->copyProperty(kIODisplayPrefKeyKey);

	if (!__private->displayPrefKey)
		return (false);

    dict = OSDynamicCast(OSDictionary, gIOFBPrefs->getObject(__private->displayPrefKey));
    if (!dict)
    {
        dict = OSDictionary::withCapacity(4);
        if (dict)
        {
            gIOFBPrefs->setObject(__private->displayPrefKey, dict);
            dict->release();
            madeChanges = true;
        }
    }
    else if (key)
        oldValue = dict->getObject(key);

	if (dict 
	 && !gIOGraphicsPrefsVersionValue->isEqualTo(dict->getObject(gIOGraphicsPrefsVersionKey)))
	{
		dict->setObject(gIOGraphicsPrefsVersionKey, gIOGraphicsPrefsVersionValue);
		madeChanges = true;
	}
        
    if (key && dict)
    {
        if (!oldValue || (!oldValue->isEqualTo(value)))
        {
            dict->setObject(key, value);
            madeChanges = true;
        }
    }

    if (madeChanges)
    {
        DEBG(thisName, " sched prefs\n");
        gIOFBDelayedPrefsEvent->setTimeoutMS((UInt32) 2000);
    }

    return (true);
}

bool IOFramebuffer::setIntegerPreference( IODisplay * display, const OSSymbol * key, UInt32 value )
{
    bool ok = false;
    OSNumber *
    num = OSNumber::withNumber(value, 32);
    if (num)
    {
        ok = setPreference(display, key, num);
        num->release();
    }
    DEBG("", " %s = %d\n", key->getCStringNoCopy(), (uint32_t) value);
    return (ok);
}

void IOFramebuffer::getTransformPrefs( IODisplay * display )
{
	OSObject * obj;
    UInt32     value;

	if ((obj = copyPreference(display, gIOFBStartupModeTimingKey)))
		setProperty(gIOFBStartupModeTimingKey, obj);

    if (getIntegerPreference(display, gIODisplayOverscanKey, &value))
    {
        if (value) 
             __private->selectedTransform  = __private->selectedTransform & ~kIOFBScalerUnderscan;
        else
            __private->selectedTransform = __private->selectedTransform | kIOFBScalerUnderscan;
    }

    if (__private->userSetTransform)
    {
        __private->userSetTransform = false;
        setIntegerPreference(display, gIOFBRotateKey, __private->selectedTransform & ~kIOFBScalerUnderscan);
    }
    else if (getIntegerPreference(display, gIOFBRotateKey, &value))
        selectTransform(value, false);
    else if (__private->transform & ~kIOFBScalerUnderscan)
        selectTransform(0, false);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IODisplayParameterHandler

OSDefineMetaClassAndStructors(IOFramebufferParameterHandler, IODisplayParameterHandler)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOFramebufferParameterHandler * IOFramebufferParameterHandler::withFramebuffer( IOFramebuffer * framebuffer )
{
    IOFramebufferParameterHandler * handler;
    uintptr_t                       count = 0;

    if ((kIOReturnSuccess != framebuffer->getAttributeForConnection(
                                    0, kConnectionDisplayParameterCount, &count)))
        return (0);

    handler = new IOFramebufferParameterHandler;
    if (handler && !handler->init())
    {
        handler->release();
        handler = 0;
    }
    if (handler)
        handler->fFramebuffer = framebuffer;

    return (handler);
}

void IOFramebufferParameterHandler::free()
{
    if (fDisplayParams)
        fDisplayParams->release();

    super::free();
}

bool IOFramebufferParameterHandler::setDisplay( IODisplay * display )
{
    fDisplay = display;

    fFramebuffer->setPreference(display, 0, 0);         // register display

    if (!display)
        return (false);

    fFramebuffer->getTransformPrefs(display);

    return (true);
}

void IOFramebufferParameterHandler::displayModeChange( void )
{
    IODisplay *      display = fDisplay;
    IOReturn         ret;
    uintptr_t        count = 0;
    UInt32           str[2];
    const OSSymbol * sym;
    uintptr_t        value[16];
    uintptr_t *      attributes;
    OSDictionary *   allParams;
    OSDictionary *   newDict = 0;
    OSDictionary *   oldParams;
    const OSSymbol * key;
    OSIterator *     iter;

    if (!display)
        return;

    allParams = OSDynamicCast(OSDictionary, display->copyProperty(gIODisplayParametersKey));
    if (allParams)
    {
        newDict = OSDictionary::withDictionary(allParams);
        allParams->release();
    }

    ret = fFramebuffer->getAttributeForConnectionParam(
                            0, kConnectionDisplayParameterCount, &count);
    if (kIOReturnSuccess != ret)
        count = 0;

    DEBG(fFramebuffer->thisName, " (%x) count %ld\n", ret, count);

    oldParams = fDisplayParams;
    do
    {
        if (count)
            fDisplayParams = OSDictionary::withCapacity(count);
        else
            fDisplayParams = 0;
        if (!fDisplayParams)
            continue;

        attributes = IONew(uintptr_t, count);
        if (!attributes)
            continue;
    
        if (kIOReturnSuccess != fFramebuffer->getAttributeForConnection(
                                        0, kConnectionDisplayParameters, attributes))
            continue;
    
        str[1] = 0;
        for (uint32_t i = 0; i < count; i++)
        {
            DEBG1(fFramebuffer->thisName, " [%d] 0x%08lx '%c%c%c%c'\n", i, attributes[i], FEAT(attributes[i]));

            if (attributes[i] < 0x00ffffff)
                continue;

            OSWriteBigInt32(str, 0, attributes[i]);
            sym = OSSymbol::withCString((const char *) str);
            if (!sym)
                continue;

			if ((!gIOFBIgnoreParameters || !gIOFBIgnoreParameters->getObject(sym))
			 && (kIOReturnSuccess == fFramebuffer->getAttributeForConnectionParam(0, attributes[i], &value[0])))
            {
            	OSObject * obj;

				DEBG1(fFramebuffer->thisName, " [%d] drvr  %s = %ld, (%ld - %ld)\n", i, (const char *) str, value[0], value[1], value[2]);

                if (gIOFBPrefsParameters && (obj = gIOFBPrefsParameters->getObject(sym)))
                {
					OSNumber * prefDefault = NULL;
                    UInt32     pref;

                    if ((fFramebuffer->getIntegerPreference(display, sym, &pref))
					 || (prefDefault = OSDynamicCast(OSNumber, obj)))
					{
						if (prefDefault)
							pref = prefDefault->unsigned32BitValue();
						if (pref < value[1])
							pref = value[1];
						if (pref > value[2])
							pref = value[2];
						value[0] = pref;
					}
                }
				if (kConnectionColorMode == attributes[i])
				{
					IODisplay::addParameter(fDisplayParams, gIODisplaySelectedColorModeKey, 0, kIODisplayColorModeRGBLimited);
					IODisplay::setParameter(fDisplayParams, gIODisplaySelectedColorModeKey, kIODisplayColorModeRGB);
				}
                IODisplay::addParameter(fDisplayParams, sym, value[1], value[2]);
				IODisplay::setParameter(fDisplayParams, sym, value[0]);
            }
            DEBG1(fFramebuffer->thisName, " [%d] added %s = %ld, (%ld - %ld)\n", i, (const char *) str, value[0], value[1], value[2]);
            sym->release();
        }
    
        IODelete(attributes, uintptr_t, count);
    }
    while (false);

    if (oldParams)
    {
        if (newDict)
        {
            iter = OSCollectionIterator::withCollection(oldParams);
            if (iter)
            {
                while ((key = (const OSSymbol *) iter->getNextObject()))
                {
                    if (!fDisplayParams || !fDisplayParams->getObject(key))
                        newDict->removeObject(key);
                }
                iter->release();
            }
        }
        oldParams->release();
    }

    if (newDict)
    {
        if (fDisplayParams)
            newDict->merge(fDisplayParams);
        display->setProperty(gIODisplayParametersKey, newDict);
        newDict->release();
    }
    else if (fDisplayParams)
        display->setProperty(gIODisplayParametersKey, fDisplayParams);
}

bool IOFramebufferParameterHandler::doIntegerSet( OSDictionary * params,
                               const OSSymbol * paramName, UInt32 value )
{
    UInt32      attribute;
    bool        ok;
    SInt32      min, max;

    fFramebuffer->fbLock();

    if (fDisplayParams && fDisplayParams->getObject(paramName))
    {
        if (fDisplay 
        	&& gIOFBPrefsParameters
        	&& gIOFBPrefsParameters->getObject(paramName)
			&& (params = IODisplay::getIntegerRange(fDisplayParams, paramName, 0, &min, &max))
        	&& (min != max))
		{
            fFramebuffer->setIntegerPreference(fDisplay, paramName, value);
		}

        attribute = OSReadBigInt32(paramName->getCStringNoCopy(), 0);

		if (kConnectionColorMode == attribute)
		{
			if ((kIODisplayColorModeAuto == value)
				&& (AUTO_COLOR_MODE != kIODisplayColorModeRGB) 
				&& fFramebuffer->__private->colorModesAllowed
				&& (AUTO_COLOR_MODE & fFramebuffer->__private->colorModesSupported))
			{
				value = AUTO_COLOR_MODE;
			}
			else
			{
				value = kIODisplayColorModeRGB;
			}
			IODisplay::setParameter(fDisplayParams, gIODisplaySelectedColorModeKey, value);
		}
    
        ok = (kIOReturnSuccess == fFramebuffer->setAttributeForConnectionParam(
                                        0, attribute, value));

		DEBG1(fFramebuffer->thisName, "(%d) %s = %d\n", ok, paramName->getCStringNoCopy(), (int) value);
    }
    else
        ok = false;

    if (gIODisplayParametersFlushKey == paramName)
        fFramebuffer->flushParameters();

    fFramebuffer->fbUnlock();

    return (ok);
}

bool IOFramebufferParameterHandler::doDataSet( const OSSymbol * paramName, OSData * value )
{
    return (false);
}

bool IOFramebufferParameterHandler::doUpdate( void )
{
    bool ok = true;

    return (ok);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::dpInterruptProc(OSObject * target, void * ref)
{
    IOFramebuffer * self = (IOFramebuffer *) target;
    uintptr_t       delay = (uintptr_t) ref;

    if (delay && self->__private->dpInterruptES)
        self->__private->dpInterruptES->setTimeoutMS(delay);
}

void IOFramebuffer::dpInterrupt(OSObject * owner, IOTimerEventSource * sender)
{
    IOFramebuffer * self = (IOFramebuffer *) owner;
    self->dpProcessInterrupt();
}

void IOFramebuffer::dpProcessInterrupt(void)
{
    IOReturn     err;
    IOI2CRequest request;
    UInt8        data[6];
    uintptr_t    bits, sel;

    if (__private->closed || !pagingState)
        return;

    sel = kIODPEventStart;
    err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);

    bzero(&data[0], sizeof(data));
    do
    {
        bzero( &request, sizeof(request) );
        
        request.commFlags               = 0;
        request.sendAddress             = 0;
        request.sendTransactionType     = kIOI2CNoTransactionType;
        request.sendBuffer              = NULL;
        request.sendBytes               = 0;
        
        request.replyAddress            = kDPRegisterLinkStatus;
        request.replyTransactionType    = kIOI2CDisplayPortNativeTransactionType;
        request.replyBuffer             = (vm_address_t) &data[0];
        request.replyBytes              = sizeof(data);
        
        err = doI2CRequest(__private->dpBusID, 0, &request);
        if( (kIOReturnSuccess != err) || (kIOReturnSuccess != request.result))
            break;

        bits = data[1];
        if (!bits)
          break;
        DEBG1(thisName, "dp events: 0x%02lx\n", bits);

        if (kDPIRQRemoteControlCommandPending & bits)
        {
            sel = kIODPEventRemoteControlCommandPending;
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
        }
        if (kDPIRQAutomatedTestRequest & bits)
        {
            sel = kIODPEventAutomatedTestRequest;
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
        }
        if (kDPIRQContentProtection & bits)
        {
            sel = kIODPEventContentProtection;
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
        }
        if (kDPIRQMCCS & bits)
        {
            sel = kIODPEventMCCS;
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);

			IOFBInterruptProc proc;
			if ((proc = __private->interruptRegisters[kIOFBMCCSInterruptRegister].handler)
				&& __private->interruptRegisters[kIOFBMCCSInterruptRegister].state)
			{
				(*proc)(__private->interruptRegisters[kIOFBMCCSInterruptRegister].target,
						__private->interruptRegisters[kIOFBMCCSInterruptRegister].ref);
			}
        }
        if (kDPIRQSinkSpecific & bits)
        {
            sel = kIODPEventSinkSpecific;
            err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);
        }

        request.sendAddress             = kDPRegisterServiceIRQ;
        request.sendTransactionType     = kIOI2CDisplayPortNativeTransactionType;
        request.sendBuffer              = (vm_address_t) &data[1];
        request.sendBytes               = sizeof(data[1]);
        
        request.replyAddress            = kDPRegisterLinkStatus;
        request.replyTransactionType    = kIOI2CDisplayPortNativeTransactionType;
        request.replyBuffer             = (vm_address_t) &data[0];
        request.replyBytes              = sizeof(data);
        
        err = doI2CRequest(__private->dpBusID, 0, &request);
        if( (kIOReturnSuccess != err) || (kIOReturnSuccess != request.result))
            break;

        if (data[1] == bits)
        {
            DEBG("dp events not cleared: 0x%02x\n", data[1]);
            break;
        }
    }
    while (false);

    sel = kIODPEventIdle;
    err = setAttributeForConnection(0, kConnectionHandleDisplayPortEvent, (uintptr_t) &sel);

    DEBG(thisName, "dp sinkCount %d\n", (kDPLinkStatusSinkCountMask & data[0]));

    if (__private->dpDongle)
    {
        UInt8 sinkCount = (kDPLinkStatusSinkCountMask & data[0]);
        if (sinkCount != __private->dpDongleSinkCount) do
        {
            __private->dpDongleSinkCount = sinkCount;
            if (captured)
                continue;
			OSBitOrAtomic(kIOFBEventProbeAll, &gIOFBGlobalEvents);
			startThread(false);
            DEBG(thisName, "dp dongle hpd probeDP\n");
        }
        while (false);
    }
    
    return;
}

void IOFramebuffer::dpUpdateConnect(void)
{
    OSObject * obj;
    OSData *   data;

    __private->dpDongle          = false;
    if (getProvider()->getProperty(kIOFBDPDeviceIDKey)
     && (obj = getProvider()->copyProperty(kIOFBDPDeviceTypeKey)))
    {
        data = OSDynamicCast(OSData, obj);
        __private->dpDongle =
            (data && data->isEqualTo(kIOFBDPDeviceTypeDongleKey, strlen(kIOFBDPDeviceTypeDongleKey)));
        obj->release();

        if (__private->dpDongle)
        {
            IOReturn     err;
            IOI2CRequest request;
            UInt8        data[6];

            bzero(&data[0], sizeof(data));
            do
            {
                bzero( &request, sizeof(request) );
                
                request.commFlags               = 0;
                request.sendAddress             = 0;
                request.sendTransactionType     = kIOI2CNoTransactionType;
                request.sendBuffer              = NULL;
                request.sendBytes               = 0;
                
                request.replyAddress            = kDPRegisterLinkStatus;
                request.replyTransactionType    = kIOI2CDisplayPortNativeTransactionType;
                request.replyBuffer             = (vm_address_t) &data[0];
                request.replyBytes              = sizeof(data);
                
                err = doI2CRequest(__private->dpBusID, 0, &request);
                if( (kIOReturnSuccess != err) || (kIOReturnSuccess != request.result))
                    break;

                __private->dpDongleSinkCount = (kDPLinkStatusSinkCountMask & data[0]);
            }
            while (false);
        }
    }
    DEBG(thisName, "dp dongle %d, sinks %d\n", __private->dpDongle, __private->dpDongleSinkCount);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOI2CInterface

OSDefineMetaClassAndStructors(IOFramebufferI2CInterface, IOI2CInterface)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOFramebufferI2CInterface * IOFramebufferI2CInterface::withFramebuffer(
    IOFramebuffer * framebuffer, OSDictionary * info )
{
    IOFramebufferI2CInterface * interface;

    interface = new IOFramebufferI2CInterface;
    info = OSDictionary::withDictionary(info);

    if (interface && info)
    {
        interface->fFramebuffer = framebuffer;
        if (!interface->init(info)
                || !interface->attach(framebuffer)
                || !interface->start(framebuffer)
           )
        {
            interface->detach( framebuffer );
            interface->release();
            interface = 0;
        }
    }
    if (info)
        info->release();

    return (interface);
}

bool IOFramebufferI2CInterface::start( IOService * provider )
{
    bool       ok = false;
    OSNumber * num;

    if (!super::start(provider))
        return (false);

    do
    {
        num = OSDynamicCast(OSNumber, getProperty(kIOI2CInterfaceIDKey));
        if (!num)
            break;
        fBusID = num->unsigned32BitValue();

        num = OSDynamicCast(OSNumber, getProperty(kIOI2CBusTypeKey));
        if (!num)
            setProperty(kIOI2CBusTypeKey, (UInt64) kIOI2CBusTypeI2C, 32);

        num = OSDynamicCast(OSNumber, getProperty(kIOI2CTransactionTypesKey));
        if (num)
            fSupportedTypes = num->unsigned32BitValue();
        else
        {
            fSupportedTypes = ((1 << kIOI2CNoTransactionType)
                             | (1 << kIOI2CSimpleTransactionType)
                             | (1 << kIOI2CDDCciReplyTransactionType)
                             | (1 << kIOI2CCombinedTransactionType));
            setProperty(kIOI2CTransactionTypesKey, (UInt64) fSupportedTypes, 32);
        }

        num = OSDynamicCast(OSNumber, getProperty(kIOI2CSupportedCommFlagsKey));
        if (num)
            fSupportedCommFlags = num->unsigned32BitValue();
        else
        {
            fSupportedCommFlags = kIOI2CUseSubAddressCommFlag;
            setProperty(kIOI2CSupportedCommFlagsKey, (UInt64) fSupportedCommFlags, 32);
        }

        UInt64 id = (((UInt64) (uintptr_t) fFramebuffer) << 32) | fBusID;
        registerI2C(id);

        ok = true;
    }
    while (false);

    return (ok);
}

IOReturn IOFramebufferI2CInterface::startIO( IOI2CRequest * request )
{
    IOReturn            err;

    fFramebuffer->fbLock();

    do
    {
        if (0 == ((1 << request->sendTransactionType) & fSupportedTypes))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }
        if (0 == ((1 << request->replyTransactionType) & fSupportedTypes))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }
        if (request->commFlags != (request->commFlags & fSupportedCommFlags))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }

        err = fFramebuffer->doI2CRequest( fBusID, 0, request );
    }
    while (false);

    if (kIOReturnSuccess != err)
    {
        request->result = err;
        if (request->completion)
            (*request->completion)(request);

        err = kIOReturnSuccess;
    }

    fFramebuffer->fbUnlock();

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if defined(__ppc__) && !defined(OSTYPES_K64_REV)

class AppleOnboardI2CInterface : public IOI2CInterface
{
    OSDeclareDefaultStructors(AppleOnboardI2CInterface)

    class PPCI2CInterface * fInterface;
    SInt32                  fPort;

public:
    virtual bool start( IOService * provider );
    virtual IOReturn startIO( IOI2CRequest * request );

    static AppleOnboardI2CInterface * withInterface( PPCI2CInterface * interface, SInt32 port );
};

#undef super
#define super IOI2CInterface

OSDefineMetaClassAndStructors(AppleOnboardI2CInterface, IOI2CInterface)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

AppleOnboardI2CInterface * AppleOnboardI2CInterface::withInterface(
    PPCI2CInterface * onboardInterface, SInt32 port )
{
    AppleOnboardI2CInterface * interface;
    UInt64 id = (((UInt64) (UInt32) onboardInterface) << 32) | port;

    interface = new AppleOnboardI2CInterface;
    if (interface)
    {
        interface->fInterface = onboardInterface;
        interface->fPort = port;
        if (!interface->init()
                || !interface->attach(onboardInterface)
                || !interface->start(onboardInterface)
           )
        {
            interface->detach( onboardInterface );
            interface->release();
            interface = 0;
        }
        else
            interface->registerI2C(id);
    }
    return (interface);
}

bool AppleOnboardI2CInterface::start( IOService * provider )
{
    if (!super::start(provider))
        return (false);

    setProperty(kIOI2CBusTypeKey,
                (UInt64) kIOI2CBusTypeI2C, 32);
    setProperty(kIOI2CTransactionTypesKey,
                (UInt64) ((1 << kIOI2CNoTransactionType)
                          | (1 << kIOI2CSimpleTransactionType)
                          | (1 << kIOI2CDDCciReplyTransactionType)
                          | (1 << kIOI2CCombinedTransactionType)), 32);
    setProperty(kIOI2CSupportedCommFlagsKey,
                (UInt64) kIOI2CUseSubAddressCommFlag, 32);

    return (true);
}

IOReturn AppleOnboardI2CInterface::startIO( IOI2CRequest * request )
{
    IOReturn err = kIOReturnSuccess;

    do
    {
        // Open the interface and sets it in the wanted mode:

        fInterface->openI2CBus(fPort);

        // the i2c driver does not support well read in interrupt mode
        // so it is better to "go polling" (read does not timeout on errors
        // in interrupt mode).
        fInterface->setPollingMode(true);

        if (request->sendBytes && (kIOI2CNoTransactionType != request->sendTransactionType))
        {
            if (kIOI2CCombinedTransactionType == request->sendTransactionType)
                fInterface->setCombinedMode();
            else if (kIOI2CUseSubAddressCommFlag & request->commFlags)
                fInterface->setStandardSubMode();
            else
                fInterface->setStandardMode();

            if (!fInterface->writeI2CBus(request->sendAddress >> 1, request->sendSubAddress,
                                         (UInt8 *) request->sendBuffer, request->sendBytes))
                err = kIOReturnNotWritable;
        }

        if (request->replyBytes && (kIOI2CNoTransactionType != request->replyTransactionType))
        {
            if (kIOI2CCombinedTransactionType == request->replyTransactionType)
                fInterface->setCombinedMode();
            else if (kIOI2CUseSubAddressCommFlag & request->commFlags)
                fInterface->setStandardSubMode();
            else
                fInterface->setStandardMode();

            if (!fInterface->readI2CBus(request->replyAddress >> 1, request->replySubAddress,
                                        (UInt8 *) request->replyBuffer, request->replyBytes))
                err = kIOReturnNotReadable;
        }

        fInterface->closeI2CBus();
    }
    while (false);

    request->result = err;

    err = kIOReturnSuccess;

    return (err);
}

#endif  /* __ppc__ && !defined(OSTYPES_K64_REV) */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebufferI2CInterface::create( IOFramebuffer * framebuffer )
{
    IOReturn                    err = kIOReturnSuccess;
    IOI2CInterface *            interface;
    UInt32                      idx;
    OSArray *                   busArray;
    OSArray *                   interfaceIDArray;
    OSDictionary *              dict;
    OSObject *                  num;
    bool                        ok = true;

    interfaceIDArray = OSArray::withCapacity(1);
    if (!interfaceIDArray)
        return (kIOReturnNoMemory);

    busArray = OSDynamicCast(OSArray, framebuffer->getProperty(kIOFBI2CInterfaceInfoKey));
    do
    {
        if (!busArray)
            continue;
        for (idx = 0; (dict = OSDynamicCast(OSDictionary, busArray->getObject(idx))); idx++)
        {
            interface = IOFramebufferI2CInterface::withFramebuffer(framebuffer, dict);
            if (!interface)
                break;
            num = interface->getProperty(kIOI2CInterfaceIDKey);
            if (num)
                interfaceIDArray->setObject(num);
            else
                break;
        }

        ok = (idx == busArray->getCount());
    }
    while (false);

#if defined(__ppc__) && !defined(OSTYPES_K64_REV)

    OSData * data = OSDynamicCast( OSData, framebuffer->getProvider()->getProperty("iic-address"));
    if (data && (!framebuffer->getProperty(kIOFBDependentIDKey))
            && (0x8c == *((UInt32 *) data->getBytesNoCopy())) /*iMac*/)
    {
        do
        {
            PPCI2CInterface * onboardInterface =
                (PPCI2CInterface*) getResourceService()->getProperty("PPCI2CInterface.i2c-uni-n");
            if (!onboardInterface)
                continue;

            interface = AppleOnboardI2CInterface::withInterface( onboardInterface, 1 );
            if (!interface)
                break;
            num = interface->getProperty(kIOI2CInterfaceIDKey);
            if (num)
                interfaceIDArray->setObject(num);
            else
                break;
        }
        while (false);
    }
#endif /* defined(__ppc__) && !defined(OSTYPES_K64_REV) */

    if (ok && interfaceIDArray->getCount())
        framebuffer->setProperty(kIOFBI2CInterfaceIDsKey, interfaceIDArray);

    interfaceIDArray->release();

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSMetaClassDefineReservedUsed(IOFramebuffer, 0);

OSMetaClassDefineReservedUnused(IOFramebuffer, 1);
OSMetaClassDefineReservedUnused(IOFramebuffer, 2);
OSMetaClassDefineReservedUnused(IOFramebuffer, 3);
OSMetaClassDefineReservedUnused(IOFramebuffer, 4);
OSMetaClassDefineReservedUnused(IOFramebuffer, 5);
OSMetaClassDefineReservedUnused(IOFramebuffer, 6);
OSMetaClassDefineReservedUnused(IOFramebuffer, 7);
OSMetaClassDefineReservedUnused(IOFramebuffer, 8);
OSMetaClassDefineReservedUnused(IOFramebuffer, 9);
OSMetaClassDefineReservedUnused(IOFramebuffer, 10);
OSMetaClassDefineReservedUnused(IOFramebuffer, 11);
OSMetaClassDefineReservedUnused(IOFramebuffer, 12);
OSMetaClassDefineReservedUnused(IOFramebuffer, 13);
OSMetaClassDefineReservedUnused(IOFramebuffer, 14);
OSMetaClassDefineReservedUnused(IOFramebuffer, 15);
OSMetaClassDefineReservedUnused(IOFramebuffer, 16);
OSMetaClassDefineReservedUnused(IOFramebuffer, 17);
OSMetaClassDefineReservedUnused(IOFramebuffer, 18);
OSMetaClassDefineReservedUnused(IOFramebuffer, 19);
OSMetaClassDefineReservedUnused(IOFramebuffer, 20);
OSMetaClassDefineReservedUnused(IOFramebuffer, 21);
OSMetaClassDefineReservedUnused(IOFramebuffer, 22);
OSMetaClassDefineReservedUnused(IOFramebuffer, 23);
OSMetaClassDefineReservedUnused(IOFramebuffer, 24);
OSMetaClassDefineReservedUnused(IOFramebuffer, 25);
OSMetaClassDefineReservedUnused(IOFramebuffer, 26);
OSMetaClassDefineReservedUnused(IOFramebuffer, 27);
OSMetaClassDefineReservedUnused(IOFramebuffer, 28);
OSMetaClassDefineReservedUnused(IOFramebuffer, 29);
OSMetaClassDefineReservedUnused(IOFramebuffer, 30);
OSMetaClassDefineReservedUnused(IOFramebuffer, 31);


