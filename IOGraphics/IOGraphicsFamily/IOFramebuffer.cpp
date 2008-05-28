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

#define IOFRAMEBUFFER_PRIVATE
#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/i2c/IOI2CInterface.h>
#include <IOKit/i2c/PPCI2CInterface.h>
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

#define ACCEL_OFF_ON_DIM	false

#define LID_OPEN_PROBE		0

#define DOANIO	      0
#define VRAM_SAVE     1
#define VRAM_COMPRESS 1

#ifdef __i386__
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
    kIOFBClamshellProbeDelayMS = 2000
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static OSArray *	    gAllFramebuffers;
static OSArray *	    gRunawayFramebuffers;
static class IOFBGate *	    gIOFBGate;
static thread_call_t	    gIOFBSleepCallout;
static IOWorkLoop *	    gIOFBWorkLoop;
static IOTimerEventSource * gIOFBDelayedPrefsEvent;
static IONotifier *	    gIOFBRootNotifier;
static IONotifier *	    gIOFBClamshellNotify;
static IOTimerEventSource * gIOFBClamshellProbeTES;
static IOService *	    gIOFBSystemPowerAckTo;
static UInt32		    gIOFBSystemPowerAckRef;
bool			    gIOFBSystemPower = true;
bool			    gIOGraphicsSystemPower = true;
static bool		    gIOFBSleepThread;
static thread_call_t	    gIOFBClamshellCallout;
static SInt32		    gIOFBClamshellEnable;
static IOOptionBits	    gIOFBClamshellState;
static SInt32		    gIOFBSuspendCount;
static IOFramebuffer *      gIOFBConsoleFramebuffer;
bool			    gIOFBDesktopModeAllowed = true;
IOOptionBits		    gIOFBLastClamshellState;
static IOOptionBits	    gIOFBLastReadClamshellState;
const OSSymbol *	    gIOFBGetSensorValueKey;
const OSSymbol *	    gIOFramebufferKey;
const OSSymbol *	    gIOFBRotateKey;
static bool		    gIOFBDimDisable;
OSDictionary *   	    gIOFBPrefs;
OSDictionary *   	    gIOFBPrefsParameters;
OSSerializer * 		    gIOFBPrefsSerializer;
IOService *                 gIOGraphicsControl;
AbsoluteTime		    gIOFBNextProbeAllTime;
AbsoluteTime		    gIOFBMaxVBLDelta;

#define	kIOFBGetSensorValueKey	"getSensorValue"

enum { kIOFBDefaultScalerUnderscan = 0*kIOFBScalerUnderscan };

// console clut
extern UInt8 appleClut8[256 * 3];

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct IOFramebufferPrivate
{
    IOGSize			maxWaitCursorSize;
    UInt32			numCursorFrames;
    UInt8 *			cursorFlags;
    volatile unsigned char **	cursorImages;
    volatile unsigned char **	cursorMasks;

    class IOFramebufferParameterHandler * paramHandler;

    OSArray *			cursorAttributes;
    IOFBCursorControlAttribute	cursorControl;
    IOInterruptEventSource *	cursorThread;
    IOOptionBits 		cursorToDo;
    UInt32			framePending;
    SInt32			xPending;
    SInt32			yPending;
    IOGPoint			cursorHotSpotAdjust[2];
    IOGPoint			lastHotSpot;
    void *			waitVBLEvent;

    IOByteCount			gammaHeaderSize;
    UInt32			desiredGammaDataWidth;
    UInt32			desiredGammaDataCount;


    IOInterruptEventSource *	deferredCLUTSetEvent;
    IOInterruptEventSource *	deferredSpeedChangeEvent;
    IOTimerEventSource *	delayedConnectInterrupt;
    UInt32			delayedConnectTime;
    SInt32			connectChangeAtSleep;

    IOByteCount			gammaDataLen;
    UInt8 *			gammaData;
    UInt32			gammaChannelCount;
    UInt32			gammaDataCount;
    UInt32			gammaDataWidth;
    IOByteCount			clutDataLen;
    UInt8 *			clutData;
    UInt32			clutIndex;
    UInt32			clutOptions;

    UInt32			framebufferWidth;
    UInt32			framebufferHeight;
    UInt32			saveLength;
    void *			saveFramebuffer;

    UInt8			gammaNeedSet;
    UInt8			scaledMode;
    UInt8			visiblePending;
    UInt8			testingCursor;
    UInt8			disabledForConnectChange;
    UInt8			index;

    UInt8			cursorSlept;
    UInt8			cursorPanning;
    UInt8			mirrorState;
    UInt8			pendingSpeedChange;

    UInt8			lli2c;
    UInt8			cursorClutDependent;
    UInt8			allowSpeedChanges;
    UInt8			dimDisable;

    UInt8			enableScalerUnderscan;
    UInt8			userSetTransform;
    UInt8			online;
    UInt8			lastNotifyOnline;
    UInt8			offlinePoweredOff;
    UInt8			pad[3];

    IODisplayModeID		offlineDisplayMode;
    IOIndex			offlineDisplayDepth;

    UInt64			transform;
    UInt64			selectedTransform;
    UInt32			reducedSpeed;
    IOService *			temperatureSensor;
    IOI2CBusTiming		defaultI2CTiming;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define	GetShmem(instance)	((StdFBShmem_t *)(instance->priv))

#define KICK_CURSOR(thread)	\
            thread->interruptOccurred(0, 0, 0);

#define CLEARSEMA(shmem, inst)				\
        if( inst->__private->cursorToDo ) {		\
            KICK_CURSOR(inst->__private->cursorThread);	\
        }						\
        ev_unlock(&shmem->cursorSema)

#define SETSEMA(shmem)		\
        if (!ev_try_lock(&shmem->cursorSema)) return;
#define TOUCHBOUNDS(one, two) \
        (((one.minx < two.maxx) && (two.minx < one.maxx)) && \
        ((one.miny < two.maxy) && (two.miny < one.maxy)))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFBGate : public IOCommandGate
{
    OSDeclareDefaultStructors(IOFBGate)
public:
    static IOFBGate *gate(IOService *owner);
    inline void closeGate()
    {
        IOCommandGate::closeGate();
    };
    inline void openGate()
    {
        IOCommandGate::openGate();
    };
    inline bool tryCloseGate()
    {
	return IOCommandGate::tryCloseGate();
    }
    inline int sleepGate(void *event, UInt32 interuptibleType)
    {
	return IOCommandGate::sleepGate(event, interuptibleType);
    }
    inline void wakeupGate(void *event, bool oneThread)
    {
	return IOCommandGate::wakeupGate(event, oneThread);
    }
    inline bool inGate()
    {
	return workLoop->inGate();
    }
};

#define super IOCommandGate
OSDefineMetaClassAndStructors(IOFBGate, IOCommandGate)

IOFBGate * IOFBGate::gate(IOService *inOwner)
{
    IOFBGate *me = new IOFBGate;

    if (me && !me->init(inOwner, 0))
    {
        me->free();
        return (0);
    }

    return (me);
}

#define FBLOCK()	\
    gIOFBGate->closeGate();
#define FBUNLOCK()	\
    gIOFBGate->openGate();


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferParameterHandler : public IODisplayParameterHandler
{
    OSDeclareDefaultStructors(IOFramebufferParameterHandler)

    OSDictionary *	fDisplayParams;
    IOFramebuffer * 	fFramebuffer;
    IODisplay *		fDisplay;

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

    IOFramebuffer * 	fFramebuffer;

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

    IOFramebuffer *	fFramebuffer;
    SInt32		fBusID;
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

/*
 * Cursor rendering
 */

#include "IOCursorBlits.h"

inline void IOFramebuffer::StdFBDisplayCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *shmem;
    IOGBounds saveRect;
    volatile unsigned char *vramPtr;	/* screen data pointer */
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
                              cursorWidth - width,	/* cursRow */
                              width,
                              height);
}

// Description:	RemoveCursor erases the cursor by replacing the background
//		image that was saved by the previous call to DisplayCursor.
//		If the frame buffer is cacheable, flush at the end of the
//		drawing operation.

inline void IOFramebuffer::StdFBRemoveCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *shmem;
    volatile unsigned char *vramPtr;	/* screen data pointer */
    unsigned int vramRow;
    int width;
    int height;

    shmem = GetShmem(inst);

    vramRow = inst->totalWidth;	/* Scanline width in pixels */

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
    StdFBShmem_t *	shmem = GetShmem(inst);

    if (!inst->pagingState)
	return;

    if (shmem->hardwareCursorActive)
    {
        IOGPoint *		hs;

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
    IOGPoint 	 *	hs;
    StdFBShmem_t *	shmem = GetShmem(inst);
    SInt32		x, y;

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
    IOGPoint *		hs;
    int 		intersect;
    IOGBounds 		tempRect;
    StdFBShmem_t *	shmem = GetShmem(inst);

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

/**
 ** external methods
 **/

void IOFramebuffer::setupCursor( IOPixelInformation * info )
{
    StdFBShmem_t *		shmem	= GetShmem(this);
    volatile unsigned char *	bits;
    IOByteCount			cursorImageBytes, waitCursorImageBytes;

    rowBytes = info->bytesPerRow;
    totalWidth = (rowBytes * 8) / info->bitsPerPixel;
    bytesPerPixel = info->bitsPerPixel / 8;
    frameBuffer = (volatile unsigned char *) vramMap->getVirtualAddress();
    __private->framebufferWidth  = info->activeWidth;
    __private->framebufferHeight = info->activeHeight;

    if (shmem)
    {
        if ((shmem->screenBounds.maxx == shmem->screenBounds.minx)
                || (shmem->screenBounds.maxy == shmem->screenBounds.miny))
        {
            // a default if no one calls IOFBSetBounds()
            shmem->screenBounds.minx = 0;
            shmem->screenBounds.miny = 0;
            shmem->screenBounds.maxx = info->activeWidth;
            shmem->screenBounds.maxy = info->activeHeight;
        }

        shmem->cursorSize[0] = maxCursorSize;
        shmem->cursorSize[1] = __private->maxWaitCursorSize;
        shmem->cursorSize[2] = __private->maxWaitCursorSize;
        shmem->cursorSize[3] = __private->maxWaitCursorSize;

        cursorImageBytes = maxCursorSize.width * maxCursorSize.height
                           * bytesPerPixel;
        waitCursorImageBytes = __private->maxWaitCursorSize.width * __private->maxWaitCursorSize.height
                               * bytesPerPixel;
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
            if (colorConvert.t._bm256To38SampleTable
                    && colorConvert.t._bm38To256SampleTable)
            {
                cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor32Axxx;
                cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor32;
            }
            break;
        default:
            IOLog("%s: can't do cursor at depth %ld\n",
                  getName(), info->bitsPerPixel);
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
    int version, int maxWidth, int maxWaitWidth )
{
    IOReturn err;

    if ((err = extEntry()))
	return (err);

#if 0
    if (__private->offlinePoweredOff)
	err = kIOReturnOffline;
    else
#endif
	err = createSharedCursor( version, maxWidth, maxWidth );

    FBUNLOCK();

    return (err);
}

bool IOFramebuffer::deepFramebuffer( 
    IOPixelInformation * pixelInfo )
{
    if ((pixelInfo->pixelType == kIORGBSignedFloatingPointPixels)
    || (pixelInfo->bitsPerComponent > 8))
	return (true);
    else
	return (false);
}

bool IOFramebuffer::validFramebuffer(
    IOPixelInformation * pixelInfo )
{
    if (pixelInfo->pixelType == kIORGBSignedDirectPixels)
	return (false);

    if (deepFramebuffer(pixelInfo))
	return (true);

    return (true);
}

IOReturn IOFramebuffer::extGetPixelInformation(
    IODisplayModeID displayMode, IOIndex depth,
    IOPixelAperture aperture, IOPixelInformation * pixelInfo )
{
    IOReturn err;

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
	bzero(pixelInfo, sizeof(IOPixelInformation));
	pixelInfo->bytesPerRow = 32;
	pixelInfo->bytesPerPlane = 0;
	pixelInfo->flags = 0;
	pixelInfo->activeWidth = 1;
	pixelInfo->activeHeight = 1;
	strcpy( pixelInfo->pixelFormat, "--------RRRRRRRRGGGGGGGGBBBBBBBB");
	pixelInfo->pixelType = kIORGBDirectPixels;
	pixelInfo->componentMasks[0] = 0x00ff0000;
	pixelInfo->componentMasks[1] = 0x0000ff00;
	pixelInfo->componentMasks[2] = 0x000000ff;
	pixelInfo->bitsPerPixel = 32;
	pixelInfo->componentCount = 3;
	pixelInfo->bitsPerComponent = 8;
    }
    else
	err = getPixelInformation(  displayMode, depth, aperture, pixelInfo );

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extGetCurrentDisplayMode(
    IODisplayModeID * displayMode, IOIndex * depth )
{
    IOReturn err;

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
	*displayMode = __private->offlineDisplayMode;
	*depth       = __private->offlineDisplayDepth;
    }
    else
	err = getCurrentDisplayMode( displayMode, depth );

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extSetStartupDisplayMode(
    IODisplayModeID displayMode, IOIndex depth )
{
    IOReturn err;

    if ((err = extEntry()))
	return (err);

    if (!__private->offlinePoweredOff)
	err = setStartupDisplayMode( displayMode, depth );

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extSetGammaTable(
    UInt32 channelCount, UInt32 dataCount,
    UInt32 dataWidth, void * data )
{
    IOReturn	err = kIOReturnBadArgument;
    UInt32	expandCount = 0;
    IOByteCount	dataLen;
    UInt32      tryWidth;
    UInt8 *	table = 0;
    bool	needAlloc;

    if ((err = extEntry()))
	return (err);

    do
    {
	if (__private->offlinePoweredOff)
	    break;

	if (!__private->desiredGammaDataWidth)
	{
	    __private->desiredGammaDataWidth = dataWidth;
	    __private->desiredGammaDataCount = dataCount;
	}
    
	if (dataWidth < __private->desiredGammaDataWidth)
	    continue;
    
	if (dataCount < __private->desiredGammaDataCount)
	{
	    expandCount = __private->desiredGammaDataCount / dataCount;
	    if ((expandCount * dataCount) == __private->desiredGammaDataCount)
	    {
		dataCount = __private->desiredGammaDataCount;
		expandCount--;
	    }
	    else
		expandCount = 0;
	}
	dataLen  = (dataWidth + 7) / 8;
	dataLen *= dataCount * channelCount;
	dataLen += __private->gammaHeaderSize;

	needAlloc = (0 == __private->gammaDataLen);
	if (!needAlloc)
	{
	    table = __private->gammaData;
	    if (__private->gammaDataLen != dataLen)
	    {
		IODelete(table, UInt8, __private->gammaDataLen);
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
	    __private->gammaData     = table;
	}
    
	__private->gammaChannelCount = channelCount;
	__private->gammaDataCount    = dataCount;
    
	table += __private->gammaHeaderSize;
    
	tryWidth = __private->desiredGammaDataWidth;

	if (!expandCount && (tryWidth == dataWidth))
	    bcopy(data, table, dataLen - __private->gammaHeaderSize);
	else
	{
	    UInt32 pin, pt5, value;
    
	    pin = (1 << tryWidth) - 1;
	    pt5 = 0;	// truncate not round

	    for (UInt32 in = 0, out = 0; out < (dataCount * channelCount);)
	    {
		value = (((UInt16 *) data)[in++] + pt5) >> (dataWidth - tryWidth);
		if (value > pin)
		    value = pin;
		for (UInt32 i = 0; i <= expandCount; i++)
		{
		    if (tryWidth <= 8)
			((UInt8 *) table)[out++] = (value & 0xff);
		    else
			((UInt16 *) table)[out++] = value;
		}
	    }
	}
	__private->gammaDataWidth = tryWidth;
	__private->gammaDataLen   = dataLen;
    
	if (__private->deferredCLUTSetEvent)
	{
	    __private->gammaNeedSet = true;
	    err = kIOReturnSuccess;
	}
	else
	{
	    err = setGammaTable( __private->gammaChannelCount, __private->gammaDataCount, 
				 __private->gammaDataWidth, __private->gammaData );
    
	    updateCursorForCLUTSet();
	}
    }
    while (false);

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extSetCLUTWithEntries( UInt32 index, IOOptionBits options,
        IOColorEntry * colors, IOByteCount dataLen )
{
    IOReturn	err;
    UInt8 *	table;
    bool	needAlloc;

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
	FBUNLOCK();
	return (err);
    }

    err = kIOReturnBadArgument;
    if (__private->deferredCLUTSetEvent)
    {
	do
	{
	    needAlloc = (0 == __private->clutDataLen);
	    if (!needAlloc)
	    {
		if (index || (__private->clutDataLen != dataLen))
		{
		    checkDeferredCLUTSet();
		    needAlloc = true;
		}
		__private->clutDataLen = 0;
	    }
    
	    if (needAlloc)
	    {
		table = IONew(UInt8, dataLen);
		if (!table)
		{
		    err = kIOReturnNoMemory;
		    continue;
		}
		__private->clutData = table;
	    }
	    else
		table = __private->clutData;
	
	    __private->clutIndex   = index;
	    __private->clutOptions = options;
	    __private->clutDataLen = dataLen;
    
	    bcopy(colors, table, dataLen);

	    err = kIOReturnSuccess;
	}
	while (false);
    }
    else
    {
	err = setCLUTWithEntries( colors, index,
				  dataLen / sizeof( IOColorEntry), options );
	updateCursorForCLUTSet();
    }

    if (this == gIOFBConsoleFramebuffer)
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

    FBUNLOCK();

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
    IOFramebuffer *	self = (IOFramebuffer *) owner;

    if (self->__private->waitVBLEvent)
    {
	gIOFBGate->commandWakeup(self->__private->waitVBLEvent, true);
	self->__private->waitVBLEvent = 0;
    }

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
    StdFBShmem_t *	shmem;
    UInt32		shmemVersion;
    IOByteCount		size, maxImageSize, maxWaitImageSize;
    UInt32		numCursorFrames;

    DEBG(thisIndex, " vers = %08x, %d x %d\n",
         version, maxWidth, maxWaitWidth);

    shmemVersion = version & kIOFBShmemVersionMask;

    if (shmemVersion == kIOFBTenPtTwoShmemVersion)
    {
        numCursorFrames = (kIOFBShmemCursorNumFramesMask & version) >> kIOFBShmemCursorNumFramesShift;

        setProperty(kIOFBWaitCursorFramesKey, (numCursorFrames - 1), 32);
        setProperty(kIOFBWaitCursorPeriodKey, 33333333, 32);	/* 30 fps */
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
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if (NULL == shmem)
        return (kIOReturnUnsupported);

    shmem->screenBounds = *bounds;

    return (kIOReturnSuccess);
}

/**
 ** IOUserClient methods
 **/

IOReturn IOFramebuffer::newUserClient(  task_t		owningTask,
                                        void * 		security_id,
                                        UInt32  	type,
                                        IOUserClient **	handler )

{
    IOReturn		err = kIOReturnSuccess;
    IOUserClient *	newConnect = 0;
    IOUserClient *	theConnect = 0;

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

IOReturn IOFramebuffer::extGetDisplayModeCount( IOItemCount * count )
{
    IOReturn err;

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
	*count = 1;
    else
	*count = dead ? 0 : getDisplayModeCount();

    FBUNLOCK();

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extGetDisplayModes( IODisplayModeID * allModes, IOByteCount * size )
{
    IOReturn    err;
    IOByteCount outSize;

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
	outSize = 1 * sizeof(IODisplayModeID);

	if (*size >= outSize)
	{
	    *size = outSize;
	    *allModes = __private->offlineDisplayMode;
	}
	else
	    err = kIOReturnBadArgument;
    }
    else
    {
	outSize = getDisplayModeCount() * sizeof( IODisplayModeID);

	if (*size >= outSize)
	{
	    *size = outSize;
	    err = getDisplayModes( allModes );
	}
	else
	    err = kIOReturnBadArgument;
    }

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extGetVRAMMapOffset( IOPixelAperture /* aperture */,
        IOByteCount * offset )
{
    IOReturn err;

    if ((err = extEntry()))
	return (err);

    *offset = vramMapOffset;

    FBUNLOCK();

    return (kIOReturnSuccess);
}

void IOFramebuffer::wakeServerState(UInt8 state)
{
    if (state)
	gIOFBGate->wakeupGate(&serverConnect, true);
}

IOReturn IOFramebuffer::extEntry(void)
{
    IOReturn err = kIOReturnSuccess;

    FBLOCK();

    while (!pagingState)
    {
	err = gIOFBGate->sleepGate(&serverConnect, false);
	if (kIOReturnSuccess != err)
	{
	    FBUNLOCK();
	    break;
	}
    }

    return (err);
}

IOReturn IOFramebuffer::extSetBounds( IOGBounds * bounds )
{
    IOReturn       err;
    StdFBShmem_t * shmem;

    if ((err = extEntry()))
	return (err);

    shmem = GetShmem(this);
    if (shmem)
    {
        if ((kIOFBHardwareCursorActive == shmem->hardwareCursorActive) && !__private->offlinePoweredOff)
	{
	    IOReturn   err;
            IOGPoint * hs;
            hs = &shmem->hotSpot[0 != shmem->frame];
            err = _setCursorState(
                      shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
                      shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, false );
	}

        shmem->screenBounds = *bounds;
    }

    FBUNLOCK();

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extValidateDetailedTiming(
    void * description, void * outDescription,
    IOByteCount inSize, IOByteCount * outSize )
{
    IOReturn	err;

    if (*outSize != inSize)
        return (kIOReturnBadArgument);

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
	err = kIOReturnOffline;
    else
	err = validateDetailedTiming( description, inSize );

    if (kIOReturnSuccess == err)
        bcopy( description, outDescription, inSize );

    FBUNLOCK();

    return (err);
}


IOReturn IOFramebuffer::extSetColorConvertTable( UInt32 select,
        UInt8 * data, IOByteCount length )
{
    static const IOByteCount checkLength[] = {
                16 * sizeof( UInt8),
                32 * sizeof( UInt8),
                256 * sizeof( UInt32),
                5 * 256 * sizeof( UInt8) };

    IOReturn		err;
    UInt8 *		table;
    IODisplayModeID	mode;
    IOIndex		depth;
    IOPixelInformation	info;

    if (select > 3)
        return (kIOReturnBadArgument);

    if (length != checkLength[select])
        return (kIOReturnBadArgument);

    if ((err = extEntry()))
	return (err);

    do
    {
	if (__private->offlinePoweredOff)
	    break;

        err = kIOReturnNoMemory;
        table = colorConvert.tables[select];
        if (0 == table)
        {
            table = (UInt8 *) IOMalloc( length );
            colorConvert.tables[select] = table;
        }
        if (!table)
            continue;

        bcopy( data, table, length );
        if (select == 3)
            white = data[data[255] + data[511] + data[767] + 1024];

        if ((NULL == cursorBlitProc)
                && colorConvert.tables[0] && colorConvert.tables[1]
                && colorConvert.tables[2] && colorConvert.tables[3]
                && vramMap
                && (kIOReturnSuccess == getCurrentDisplayMode(&mode, &depth))
                && (kIOReturnSuccess == getPixelInformation(mode, depth, kIOFBSystemAperture, &info)))
            setupCursor( &info );

        err = kIOReturnSuccess;
    }
    while (false);

    FBUNLOCK();

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFramebuffer::requestTerminate( IOService * provider, IOOptionBits options )
{
    FBLOCK();

    if (opened)
    {
	if (!gRunawayFramebuffers)
	    gRunawayFramebuffers = OSArray::withCapacity(4);
	if (gRunawayFramebuffers)
	    gRunawayFramebuffers->setObject(this);
    }

    FBUNLOCK();

    return (false);
}

void IOFramebuffer::stop( IOService * provider )
{

    if (opened)
    {
	FBLOCK();

	dead = true;

	setAttribute( kIOSystemPowerAttribute, kIOMessageSystemWillPowerOff );

	FBUNLOCK();

	temporaryPowerClampOn();	// only to clear out kIOPMPreventSystemSleep
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
        FBLOCK();

	replace = (IOFramebuffer *) gRunawayFramebuffers->getObject(0);
	gRunawayFramebuffers->removeObject(0);

	FBUNLOCK();
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

    // initialize superclass power management variables
    PMinit();
    // attach into the power management hierarchy
    setProperty("IOPMStrictTreeOrder", kOSBooleanTrue);
    provider->joinPMtree(this);

    if (runaway)
    {
	IOReturn err;

	FBLOCK();
	err = enableController();
	if (kIOReturnSuccess == err)
	{
	    opened = true;
	    dead = false;
	    isUsable = true;
            configPending = serverState ? 0 : 1;

	    connectChangeInterrupt(this, 0);
	}
	FBUNLOCK();
    }
    else
	registerService();

    return (true);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//
// BEGIN:	Implementation of the evScreen protocol
//

void IOFramebuffer::hideCursor( void )
{
    StdFBShmem_t *shmem = GetShmem(this);

    SETSEMA(shmem);
    SysHideCursor(this);
    CLEARSEMA(shmem, this);
}

void IOFramebuffer::deferredMoveCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *	shmem = GetShmem(inst);
    IOReturn		err = kIOReturnSuccess;

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
    inst->needCursorService = (kIOReturnBusy == err);
}

void IOFramebuffer::cursorWork( OSObject * p0, IOInterruptEventSource * evtSrc, int intCount )
{
    IOFramebuffer *		inst = (IOFramebuffer *) p0;
    StdFBShmem_t *		shmem = GetShmem(inst);
    struct IOFramebufferPrivate * __private = inst->__private;
    IOFBCursorControlAttribute	* cursorControl = &__private->cursorControl;
    IOReturn			ret;
    IOHardwareCursorDescriptor	desc;

    IOOptionBits todo = inst->__private->cursorToDo;

    while (todo)
    {
        if (2 & todo)
        {
            desc.majorVersion 	= kHardwareCursorDescriptorMajorVersion;
            desc.minorVersion 	= kHardwareCursorDescriptorMinorVersion;
            desc.height 	= shmem->cursorSize[0 != __private->framePending].height;
            desc.width 		= shmem->cursorSize[0 != __private->framePending].width;
            desc.bitDepth 	= inst->bytesPerPixel * 8;
            desc.maskBitDepth 	= 0;
            desc.colorEncodings = 0;
            desc.flags 		= 0;
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
    IOGPoint *	   hs;
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
		gIOFBGate->sleepGate(hs, THREAD_UNINT);
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
	RemoveCursor(this);
    flags = (kIOReturnSuccess == setCursorImage( (void *) frame ))
            ? kIOFBHardwareCursorActive : 0;
    if (!animation && !shmem->cursorShow)
	DisplayCursor(this);

    if (!flags && __private->cursorThread && (bytesPerPixel >= 2))
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
        ret = setCursorState( x, y, visible );
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
    nextCursorLoc = *cursorLoc;
    nextCursorFrame = frame;
    needCursorService  = true;
    UInt32 hwCursorActive;

    StdFBShmem_t *shmem = GetShmem(this);

    SETSEMA(shmem);

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

    if ((kIOFBHardwareCursorActive == 
	    (shmem->hardwareCursorActive & (kIOFBHardwareCursorActive | kIOFBHardwareCursorInVRAM))) 
    || !haveVBLService)
    {
	if (kIOFBRotateFlags & __private->transform)
	    transformLocation(shmem, &nextCursorLoc, &shmem->cursorLoc);
	else
	    shmem->cursorLoc = nextCursorLoc;
        shmem->frame = frame;
        deferredMoveCursor( this );
    }

    CLEARSEMA(shmem, this);
}

void IOFramebuffer::handleVBL( IOFramebuffer * inst, void * ref )
{
    StdFBShmem_t *	shmem = GetShmem(inst);
    AbsoluteTime	now;
    AbsoluteTime	delta;

    if (!shmem)
        return ;

    shmem->vblCount++;
    clock_get_uptime( &now );
    delta = now;
    SUB_ABSOLUTETIME( &delta, &shmem->vblTime );
    shmem->vblDelta = delta;
    shmem->vblTime  = now;

    KERNEL_DEBUG(0xc000030 | DBG_FUNC_NONE,
                 (uint32_t)(AbsoluteTime_to_scalar(&shmem->vblDelta) >> 32),
                 (uint32_t)(AbsoluteTime_to_scalar(&shmem->vblDelta)), 0, 0, 0);

    if (inst->vblSemaphore)
        semaphore_signal_all(inst->vblSemaphore);

    if (inst->__private->deferredCLUTSetEvent 
     && (inst->__private->gammaNeedSet || inst->__private->clutDataLen || inst->__private->waitVBLEvent))
	inst->__private->deferredCLUTSetEvent->interruptOccurred(0, 0, 0);

    SETSEMA(shmem);

    if (inst->needCursorService)
    {
	if (kIOFBRotateFlags & inst->__private->transform)
	    inst->transformLocation(shmem, &inst->nextCursorLoc, &shmem->cursorLoc);
	else
	    shmem->cursorLoc = inst->nextCursorLoc;
        shmem->frame = inst->nextCursorFrame;
        deferredMoveCursor( inst );
    }

    CLEARSEMA(shmem, inst);
}

void IOFramebuffer::showCursor( IOGPoint * cursorLoc, int frame )
{
    StdFBShmem_t *shmem;
    UInt32 hwCursorActive;

    shmem = GetShmem(this);
    SETSEMA(shmem);

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
    CLEARSEMA(shmem, this);
}

void IOFramebuffer::resetCursor( void )
{
    StdFBShmem_t *	shmem;
    int			frame;

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
        *time = shmem->vblTime;
        *delta = shmem->vblDelta;
    }
    else
        AbsoluteTime_to_scalar(&time) = 0;
}

void IOFramebuffer::getBoundingRect( IOGBounds ** bounds )
{
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if (NULL == shmem)
        *bounds = NULL;
    else
        *bounds = &shmem->screenBounds;
}

//
// END:		Implementation of the evScreen protocol
//

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebuffer::getNotificationSemaphore(
    IOSelect interruptType, semaphore_t * semaphore )
{
    kern_return_t	kr;
    semaphore_t		sema;

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

IOReturn IOFramebuffer::extSetCursorVisible( bool visible )
{
    IOReturn		err;
    IOGPoint *		hs;
    StdFBShmem_t *	shmem;

    if ((err = extEntry()))
	return (err);

    shmem = GetShmem(this);
    if (shmem->hardwareCursorActive && !__private->offlinePoweredOff)
    {
        hs = &shmem->hotSpot[0 != shmem->frame];
        err = _setCursorState(
                  shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
                  shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny,
                  visible );

        if (__private->cursorToDo)
            KICK_CURSOR(__private->cursorThread);
    }
    else
        err = kIOReturnBadArgument;

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extSetCursorPosition( SInt32 x, SInt32 y )
{
    return (kIOReturnUnsupported);
}

void IOFramebuffer::transformCursor( StdFBShmem_t * shmem, IOIndex frame )
{
    void *		      buf;
    unsigned int * out;
    volatile unsigned int   * cursPtr32 = 0;
    volatile unsigned short * cursPtr16 = 0;
    SInt32 x, y, dw, dh, sx, sy, sw, sh;

    if (bytesPerPixel == 4)
    {
        cursPtr32 = (volatile unsigned int *) __private->cursorImages[frame];
        cursPtr16 = 0;
    }
    else if (bytesPerPixel == 2)
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

    buf = IOMalloc(dw * dh * bytesPerPixel);
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

    bcopy(buf, (void *) __private->cursorImages[frame], dw * dh * bytesPerPixel);
    IOFree(buf, dw * dh * bytesPerPixel);

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

IOReturn IOFramebuffer::extSetNewCursor( void * cursor, IOIndex frame,
        IOOptionBits options )
{
    StdFBShmem_t *	shmem;
    IOReturn		err;
    UInt32		hwCursorActive;

    if ((err = extEntry()))
	return (err);

    shmem = GetShmem(this);
    // assumes called with cursorSema held
    if (cursor || options || (((UInt32) frame) >= __private->numCursorFrames) 
	|| __private->offlinePoweredOff)
        err = kIOReturnBadArgument;
    else
    {
	if (kIOFBRotateFlags & __private->transform)
	    transformCursor(shmem, frame);

        if ((shmem->cursorSize[0 != frame].width > maxCursorSize.width)
                || (shmem->cursorSize[0 != frame].height > maxCursorSize.height))
            err = kIOReturnBadArgument;

        else if (haveHWCursor)
        {
            if (frame == shmem->frame)
            {
                hwCursorActive = _setCursorImage( frame );
                shmem->hardwareCursorActive = hwCursorActive;
                __private->cursorFlags[frame] = hwCursorActive ? kIOFBCursorHWCapable : 0;
            }
            else
            {
                __private->cursorFlags[frame] = kIOFBCursorImageNew;
            }
            err = kIOReturnSuccess;		// I guess
        }
        else
            err = kIOReturnUnsupported;
    }
    if (__private->cursorToDo)
        KICK_CURSOR(__private->cursorThread);

    FBUNLOCK();

    return (err);
}

bool IOFramebuffer::convertCursorImage( void * cursorImage,
                                        IOHardwareCursorDescriptor * hwDesc,
                                        IOHardwareCursorInfo * hwCursorInfo )
{
    StdFBShmem_t *		shmem = GetShmem(this);
    UInt8 *			dataOut = hwCursorInfo->hardwareCursorData;
    IOColorEntry *		clut = hwCursorInfo->colorMap;
    UInt32			maxColors = hwDesc->numColors;
    int				frame = (int) cursorImage;

    volatile unsigned short *	cursPtr16;
    volatile unsigned int *	cursPtr32;
    SInt32 			x, lastx, y, lasty;
    UInt32			width, height, lineBytes = 0;
    UInt32			index, numColors = 0;
    UInt32			alpha, red, green, blue;
    UInt16			s16;
    UInt32			s32;
    UInt32			pixel = 0;
    UInt32			data = 0;
    UInt32			bits = 0;
    bool			ok = true;
    bool			isDirect;

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

    if (bytesPerPixel == 4)
    {
        cursPtr32 = (volatile unsigned int *) __private->cursorImages[frame];
        cursPtr16 = 0;
    }
    else if (bytesPerPixel == 2)
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
#define RMASK16	0xF000
#define GMASK16	0x0F00
#define BMASK16	0x00F0
#define AMASK16	0x000F
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
    0xFF,0xFF,0xFF, 0xFF,0xFF,0xCC,	0xFF,0xFF,0x99,	0xFF,0xFF,0x66,
    0xFF,0xFF,0x33, 0xFF,0xFF,0x00,	0xFF,0xCC,0xFF,	0xFF,0xCC,0xCC,
    0xFF,0xCC,0x99, 0xFF,0xCC,0x66,	0xFF,0xCC,0x33,	0xFF,0xCC,0x00,
    0xFF,0x99,0xFF, 0xFF,0x99,0xCC,	0xFF,0x99,0x99,	0xFF,0x99,0x66,
    // 10
    0xFF,0x99,0x33, 0xFF,0x99,0x00,	0xFF,0x66,0xFF,	0xFF,0x66,0xCC,
    0xFF,0x66,0x99, 0xFF,0x66,0x66,	0xFF,0x66,0x33,	0xFF,0x66,0x00,
    0xFF,0x33,0xFF, 0xFF,0x33,0xCC,	0xFF,0x33,0x99,	0xFF,0x33,0x66,
    0xFF,0x33,0x33, 0xFF,0x33,0x00,	0xFF,0x00,0xFF,	0xFF,0x00,0xCC,
    // 20
    0xFF,0x00,0x99, 0xFF,0x00,0x66,	0xFF,0x00,0x33,	0xFF,0x00,0x00,
    0xCC,0xFF,0xFF, 0xCC,0xFF,0xCC,	0xCC,0xFF,0x99,	0xCC,0xFF,0x66,
    0xCC,0xFF,0x33, 0xCC,0xFF,0x00,	0xCC,0xCC,0xFF,	0xCC,0xCC,0xCC,
    0xCC,0xCC,0x99, 0xCC,0xCC,0x66,	0xCC,0xCC,0x33,	0xCC,0xCC,0x00,
    // 30
    0xCC,0x99,0xFF, 0xCC,0x99,0xCC,	0xCC,0x99,0x99,	0xCC,0x99,0x66,
    0xCC,0x99,0x33, 0xCC,0x99,0x00,	0xCC,0x66,0xFF,	0xCC,0x66,0xCC,
    0xCC,0x66,0x99, 0xCC,0x66,0x66,	0xCC,0x66,0x33,	0xCC,0x66,0x00,
    0xCC,0x33,0xFF, 0xCC,0x33,0xCC,	0xCC,0x33,0x99,	0xCC,0x33,0x66,
    // 40
    0xCC,0x33,0x33, 0xCC,0x33,0x00,	0xCC,0x00,0xFF,	0xCC,0x00,0xCC,
    0xCC,0x00,0x99, 0xCC,0x00,0x66,	0xCC,0x00,0x33,	0xCC,0x00,0x00,
    0x99,0xFF,0xFF, 0x99,0xFF,0xCC,	0x99,0xFF,0x99,	0x99,0xFF,0x66,
    0x99,0xFF,0x33, 0x99,0xFF,0x00,	0x99,0xCC,0xFF,	0x99,0xCC,0xCC,
    // 50
    0x99,0xCC,0x99, 0x99,0xCC,0x66,	0x99,0xCC,0x33,	0x99,0xCC,0x00,
    0x99,0x99,0xFF, 0x99,0x99,0xCC,	0x99,0x99,0x99,	0x99,0x99,0x66,
    0x99,0x99,0x33, 0x99,0x99,0x00,	0x99,0x66,0xFF,	0x99,0x66,0xCC,
    0x99,0x66,0x99, 0x99,0x66,0x66,	0x99,0x66,0x33,	0x99,0x66,0x00,
    // 60
    0x99,0x33,0xFF, 0x99,0x33,0xCC,	0x99,0x33,0x99,	0x99,0x33,0x66,
    0x99,0x33,0x33, 0x99,0x33,0x00,	0x99,0x00,0xFF,	0x99,0x00,0xCC,
    0x99,0x00,0x99, 0x99,0x00,0x66,	0x99,0x00,0x33,	0x99,0x00,0x00,
    0x66,0xFF,0xFF, 0x66,0xFF,0xCC,	0x66,0xFF,0x99,	0x66,0xFF,0x66,
    // 70
    0x66,0xFF,0x33, 0x66,0xFF,0x00,	0x66,0xCC,0xFF,	0x66,0xCC,0xCC,
    0x66,0xCC,0x99, 0x66,0xCC,0x66,	0x66,0xCC,0x33,	0x66,0xCC,0x00,
    0x66,0x99,0xFF, 0x66,0x99,0xCC,	0x66,0x99,0x99,	0x66,0x99,0x66,
    0x66,0x99,0x33, 0x66,0x99,0x00,	0x66,0x66,0xFF,	0x66,0x66,0xCC,
    // 80
    0x66,0x66,0x99, 0x66,0x66,0x66,	0x66,0x66,0x33,	0x66,0x66,0x00,
    0x66,0x33,0xFF, 0x66,0x33,0xCC,	0x66,0x33,0x99,	0x66,0x33,0x66,
    0x66,0x33,0x33, 0x66,0x33,0x00,	0x66,0x00,0xFF,	0x66,0x00,0xCC,
    0x66,0x00,0x99, 0x66,0x00,0x66,	0x66,0x00,0x33,	0x66,0x00,0x00,
    // 90
    0x33,0xFF,0xFF, 0x33,0xFF,0xCC,	0x33,0xFF,0x99,	0x33,0xFF,0x66,
    0x33,0xFF,0x33, 0x33,0xFF,0x00,	0x33,0xCC,0xFF,	0x33,0xCC,0xCC,
    0x33,0xCC,0x99, 0x33,0xCC,0x66,	0x33,0xCC,0x33,	0x33,0xCC,0x00,
    0x33,0x99,0xFF, 0x33,0x99,0xCC,	0x33,0x99,0x99,	0x33,0x99,0x66,
    // a0
    0x33,0x99,0x33, 0x33,0x99,0x00,	0x33,0x66,0xFF,	0x33,0x66,0xCC,
    0x33,0x66,0x99, 0x33,0x66,0x66,	0x33,0x66,0x33,	0x33,0x66,0x00,
    0x33,0x33,0xFF, 0x33,0x33,0xCC,	0x33,0x33,0x99,	0x33,0x33,0x66,
    0x33,0x33,0x33, 0x33,0x33,0x00,	0x33,0x00,0xFF,	0x33,0x00,0xCC,
    // b0
    0x33,0x00,0x99, 0x33,0x00,0x66,	0x33,0x00,0x33,	0x33,0x00,0x00,
    0x00,0xFF,0xFF, 0x00,0xFF,0xCC,	0x00,0xFF,0x99,	0x00,0xFF,0x66,
    0x00,0xFF,0x33, 0x00,0xFF,0x00,	0x00,0xCC,0xFF,	0x00,0xCC,0xCC,
    0x00,0xCC,0x99, 0x00,0xCC,0x66,	0x00,0xCC,0x33,	0x00,0xCC,0x00,
    // c0
    0x00,0x99,0xFF, 0x00,0x99,0xCC,	0x00,0x99,0x99,	0x00,0x99,0x66,
    0x00,0x99,0x33, 0x00,0x99,0x00,	0x00,0x66,0xFF,	0x00,0x66,0xCC,
    0x00,0x66,0x99, 0x00,0x66,0x66,	0x00,0x66,0x33,	0x00,0x66,0x00,
    0x00,0x33,0xFF, 0x00,0x33,0xCC,	0x00,0x33,0x99,	0x00,0x33,0x66,
    // d0
    0x00,0x33,0x33, 0x00,0x33,0x00,	0x00,0x00,0xFF,	0x00,0x00,0xCC,
    0x00,0x00,0x99, 0x00,0x00,0x66,	0x00,0x00,0x33,	0xEE,0x00,0x00,
    0xDD,0x00,0x00, 0xBB,0x00,0x00,	0xAA,0x00,0x00,	0x88,0x00,0x00,
    0x77,0x00,0x00, 0x55,0x00,0x00,	0x44,0x00,0x00,	0x22,0x00,0x00,
    // e0
    0x11,0x00,0x00, 0x00,0xEE,0x00,	0x00,0xDD,0x00,	0x00,0xBB,0x00,
    0x00,0xAA,0x00, 0x00,0x88,0x00,	0x00,0x77,0x00,	0x00,0x55,0x00,
    0x00,0x44,0x00, 0x00,0x22,0x00,	0x00,0x11,0x00,	0x00,0x00,0xEE,
    0x00,0x00,0xDD, 0x00,0x00,0xBB,	0x00,0x00,0xAA,	0x00,0x00,0x88,
    // f0
    0x00,0x00,0x77, 0x00,0x00,0x55,	0x00,0x00,0x44,	0x00,0x00,0x22,
    0x00,0x00,0x11, 0xEE,0xEE,0xEE,	0xDD,0xDD,0xDD,	0xBB,0xBB,0xBB,
    0xAA,0xAA,0xAA, 0x88,0x88,0x88,	0x77,0x77,0x77,	0x55,0x55,0x55,
    0x44,0x44,0x44, 0x22,0x22,0x22,	0x11,0x11,0x11,	0x00,0x00,0x00
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

    DEBG(0, "\n");
    err = ((*cdevsw[major(device)].d_read)(device, &uio, 0));
    DEBG(0, " done(%08lx)\n", doaniobuf[0]);
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

IOReturn IOFramebuffer::handleEvent( IOIndex event, void * info )
{
    IOReturn ret;

    DEBG(thisIndex, "(%ld, %d)\n", event, pagingState);

    switch (event)
    {
        case kIOFBNotifyWillPowerOff:
            if (this == gIOFBConsoleFramebuffer)
            {
//		getPlatform()->setConsoleInfo( 0, kPEDisableScreen);
                killprint = 1;
            }

            if (ACCEL_OFF_ON_DIM && pagingState)
            {
                pagingState = false;
                deliverFramebufferNotification( kIOFBNotifyWillSleep, info );
            }

            ret = deliverFramebufferNotification( event, info );
            configPending = true;

	    __private->offlinePoweredOff = (0 != gIOGraphicsControl);
	    if (__private->offlinePoweredOff 
	      && (kIOReturnSuccess != 
			getCurrentDisplayMode(&__private->offlineDisplayMode, &__private->offlineDisplayDepth)))
	    {
		__private->offlineDisplayMode  = 0;
		__private->offlineDisplayDepth = 0;
	    }
            break;

        case kIOFBNotifyDidPowerOff:
	    __private->connectChangeAtSleep = connectChange;
            ret = deliverFramebufferNotification( event, info );
	    break;

        case kIOFBNotifyWillPowerOn:
	    __private->offlinePoweredOff = false;
            ret = deliverFramebufferNotification( event, info );
            break;
            
        case kIOFBNotifyDidPowerOn:

            if (this == gIOFBConsoleFramebuffer)
            {
//		getPlatform()->setConsoleInfo( 0, kPEEnableScreen);
                killprint = 0;
                kmputc( 033 );
                kmputc( 'c' );
            }
            ret = deliverFramebufferNotification( event, info );

            if (!pagingState && gIOFBSystemPower)
            {
                pagingState = true;

                if (__private->connectChangeAtSleep != connectChange)
                {
                    if (!__private->disabledForConnectChange)
                    {
                        __private->disabledForConnectChange = true;
                        deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
                    }
                }

                deliverFramebufferNotification( kIOFBNotifyDidWake, info );
            }

#if DOANIO
            doanio();
#endif

            configPending = false;
//          checkConnectionChange();
            break;

#if DOANIO
        case kIOFBNotifyWillSleep:
            if (!info)
            {
                doanio();
            }
            // else its the redirection notify
            ret = deliverFramebufferNotification( event, info );
            break;
#endif

#if VRAM_SAVE
        case kIOFBNotifyDidWake:
            if (info)
            {
                // restore vram content
                if (__private->saveLength)
                {
                    if (!suspended
                    && ((this != gIOFBConsoleFramebuffer)
                            || (kOSBooleanTrue != getPMRootDomain()->getProperty(kIOHibernatePreviewBufferKey))))
                    {
#if VRAM_COMPRESS
                        DecompressData( (UInt8 *) __private->saveFramebuffer, (UInt8 *) frameBuffer,
                                        0, 0, __private->framebufferWidth, __private->framebufferHeight, rowBytes);
#else
                        bcopy_nc( __private->saveFramebuffer, (void *) frameBuffer, __private->saveLength );
#endif
                        DEBG(thisIndex, " screen drawn\n");

			if (__private->gammaDataLen && __private->gammaData && !__private->scaledMode)
			    setGammaTable( __private->gammaChannelCount, __private->gammaDataCount, 
					   __private->gammaDataWidth, __private->gammaData );
                    }

                    if (this == gIOFBConsoleFramebuffer)
		    {
                        getPMRootDomain()->removeProperty(kIOHibernatePreviewBufferKey);
			getProvider()->removeProperty(kIOHibernatePreviewActiveKey);
		    }
                    IOFreePageable( __private->saveFramebuffer, __private->saveLength );
                    __private->saveFramebuffer = 0;
                    __private->saveLength      = 0;
                }
            }
            ret = deliverFramebufferNotification( event, info );
            break;
#endif /* VRAM_SAVE */

        default:
            ret = deliverFramebufferNotification( event, info );
            break;
    }

    return (ret);
}

IOReturn IOFramebuffer::notifyServer( UInt8 state )
{
    mach_msg_header_t * msgh = (mach_msg_header_t *) serverMsg;
    IOReturn		err = kIOReturnSuccess;

    if (serverNotified != state)
    {
        serverNotified = state;

        DEBG(thisIndex, "(%p, %d->%d, %d)\n", msgh->msgh_remote_port,
             serverState, serverNotified, serverPendingAck);

        msgh->msgh_id = state;
        if ((MACH_PORT_NULL == msgh->msgh_remote_port)
                || (KERN_SUCCESS != mach_msg_send_from_kernel(msgh, msgh->msgh_size)))
        {
            serverState = serverNotified;
        }
    }
    wakeServerState(serverNotified);

    return (err);
}

bool IOFramebuffer::getIsUsable( void )
{
    return (dead || (0 != isUsable));
}

IOReturn IOFramebuffer::postWake( IOOptionBits state )
{
    IOReturn ret;
    UInt32   value;

    sleepConnectCheck = false;

    configPending = false;

    ret = getAttributeForConnection(0, kConnectionPostWake, &value);

    if (!captured)
	getAttributeForConnection(0, kConnectionChanged, 0);

    return (ret);
}

void IOFramebuffer::notifyServerAll( UInt8 state )
{
    unsigned int	index;
    IOFramebuffer *	fb;
    bool		doNotify = true;

    FBLOCK();

    if (state)
    {
        doNotify = gIOFBSystemPower;
        for (index = 0;
                doNotify && (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
                index++)
        {
            doNotify = fb->getIsUsable();
        }
        if (doNotify)
            IOSleep(20);
    }

    if (doNotify)
    {
        if (state)
	{
            for (index = 0;
                    (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
                    index++)
            {
                fb->postWake( state );
            }
	}
        for (index = 0;
                (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
                index++)
        {
            fb->notifyServer( state );
        }
    }

    FBUNLOCK();
}

enum {
    kIOFBDidWork = 0x00000001,
    kIOFBPaging  = 0x00000002,
};

void IOFramebuffer::sleepWork( void * arg )
{
    unsigned int	index;
    IOFramebuffer *	fb;
    IOOptionBits	allState;

    DEBG(0, "\n");

    FBLOCK();

    do
    {
        for (index = 0, allState = 0;
                (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
                index++)
        {
	    allState |= fb->checkPowerWork();
        }

        if ((0 == (kIOFBPaging & allState)) && gIOFBSystemPowerAckTo)
        {
            DEBG(0, " allowPowerChange(%ld)\n", gIOFBSystemPowerAckRef);

            IOService * ackTo = gIOFBSystemPowerAckTo;
            UInt32      ackRef = gIOFBSystemPowerAckRef;
            gIOFBSystemPowerAckTo = 0;
            FBUNLOCK();

            ackTo->allowPowerChange( ackRef );

            DEBG(0, " did allowPowerChange()\n");

            FBLOCK();
            allState |= kIOFBDidWork;
        }
    }
    while (kIOFBDidWork & allState);

    gIOFBSleepThread = false;

    FBUNLOCK();
}

IOOptionBits IOFramebuffer::checkPowerWork( void )
{
    UInt32	 newState;
    IOOptionBits ourState = kIOFBPaging;

    DEBG(thisIndex, "(%d, %d)\n",
         gIOFBSystemPower, pendingPowerChange);

    if (!gIOFBSystemPower)
    {
        notifyServer( false );

        if (!serverState)
        {
            bool doSave = (0 != gIOFBSystemPowerAckTo);

            if (pagingState)
            {
                pagingState = false;
                FBUNLOCK();

                handleEvent( kIOFBNotifyWillSleep );

                FBLOCK();
                ourState |= kIOFBDidWork;
            }

#if VRAM_SAVE
            // vram content is being lost
            UInt32		value;
            if (doSave
                    && !dead
                    && !__private->saveLength
                    && (kIOReturnSuccess == getAttribute(kIOVRAMSaveAttribute, &value)) 
		    && value)
            {
                vm_size_t sLen;
                sLen = __private->framebufferHeight * rowBytes;

                /*
                * dLen should account for possible growth. (e.g. run-length encoding noise)
                * 	Add 5 bytes for header,
                * 	12% for RLE growth
                * 	2 bytes per line for line spans,
                * 	1 additional escape code byte for trailing pixel in each line
                */
#if VRAM_COMPRESS
                vm_size_t dLen;

                dLen = 5 + sLen + ((sLen + 7) >> 3) + (__private->framebufferHeight * 3) + rowBytes;
                dLen = round_page_32(dLen);
                __private->saveLength = dLen;
#else
                __private->saveLength = round_page_32(sLen);
#endif
                __private->saveFramebuffer = IOMallocPageable( __private->saveLength, page_size );

                if (__private->saveFramebuffer)
                {
#if VRAM_COMPRESS
                    DEBG(thisIndex, " compressing\n");
                    dLen = CompressData( (UInt8 *) frameBuffer, bytesPerPixel,
                                         __private->framebufferWidth, __private->framebufferHeight, rowBytes,
                                         (UInt8 *) __private->saveFramebuffer, __private->saveLength );

                    DEBG(thisIndex, " compressed to %d%%\n", (dLen * 100) / sLen);

                    dLen = round_page_32( dLen );
                    if (__private->saveLength > dLen)
                    {
                        IOFreePageable( (void *) (((UInt32) __private->saveFramebuffer) + dLen),
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
                        DEBG(thisIndex, " vm_map_wire(%x)\n", kr);

                        if (this == gIOFBConsoleFramebuffer)
                        {
#if VERSION_MAJOR >= 9
			    IOMemoryDescriptor *
			    previewBuffer = IOMemoryDescriptor::withAddress(
						__private->saveFramebuffer, 
						__private->saveLength, 
						kIODirectionInOut);
#else
                            OSData *
			    previewBuffer = OSData::withBytesNoCopy(
                                    __private->saveFramebuffer, __private->saveLength);
#endif
                            if (previewBuffer)
                            {
                                getPMRootDomain()->setProperty(kIOHibernatePreviewBufferKey, previewBuffer);
                                previewBuffer->release();

				OSNumber * num;
				if ((num = OSDynamicCast(OSNumber, getPMRootDomain()->getProperty(kIOHibernateModeKey))) 
				 && (kIOHibernateModeOn == ((kIOHibernateModeSleep | kIOHibernateModeOn) & num->unsigned32BitValue()))
				 && !gIOFBLastClamshellState)
				{
				    // hibernate enabled, will power off, clamshell open - do preview
				    UInt32 flags = 0;
				    getProvider()->setProperty(kIOHibernatePreviewActiveKey, &flags, sizeof(flags));
				    PreviewDecompressData(__private->saveFramebuffer, (void *)frameBuffer, 
							    __private->framebufferWidth, __private->framebufferHeight,
							    bytesPerPixel, rowBytes);
				}
                            }
                        }
                    }
                }
                else
                    __private->saveLength = 0;
            }
#endif /* VRAM_SAVE */
            ourState &= ~kIOFBPaging;
        }
    }

    if (pendingPowerChange)
    {
        pendingPowerChange = false;
        newState = pendingPowerState;

        //        FBUNLOCK();

        DEBG(thisIndex, " kIOPowerAttribute(%ld)\n", newState);

        setAttribute( kIOPowerAttribute, newState );

        FBUNLOCK();

	OSObject * obj;
	if ((this == gIOFBConsoleFramebuffer) 
	 && (obj = copyProperty(kIOHibernatePreviewActiveKey, gIOServicePlane)))
	{
	    getPMRootDomain()->setProperty(kIOHibernatePreviewActiveKey, obj);
	    obj->release();
	}

        DEBG(thisIndex, " acknowledgeSetPowerState\n");
        acknowledgeSetPowerState();

        FBLOCK();
        ourState |= kIOFBDidWork;
    }

    if (__private->pendingSpeedChange)
    {
        __private->pendingSpeedChange = false;
	setAttribute(kIOFBSpeedAttribute, __private->reducedSpeed);
    }

    return (ourState);
}

void IOFramebuffer::startThread(bool highPri)
{
    if (!gIOFBSleepThread)
    {
        gIOFBSleepThread = true;

	if (highPri)
	    thread_call_enter1(gIOFBSleepCallout, (thread_call_param_t) 0);
	else
	    IOCreateThread(&sleepWork, 0);
    }
}

IOReturn IOFramebuffer::setPowerState( unsigned long powerStateOrdinal,
                                       IOService * whichDevice )
{
    bool now;

    DEBG(thisIndex, " (%ld)\n", powerStateOrdinal);

    FBLOCK();

    pendingPowerState = powerStateOrdinal;

    now = (0xffffffff == gAllFramebuffers->getNextIndexOfObject( this, 0 ));

    if (!now)
    {
        pendingPowerChange = true;
        startThread(!gIOFBSystemPower || sleepConnectCheck);
    }

//    if (now)        setAttribute( kIOPowerAttribute, powerStateOrdinal );

    FBUNLOCK();

    return (now ? 0 : 45 * 1000 * 1000);
}

IOReturn IOFramebuffer::powerStateWillChangeTo( IOPMPowerFlags flags,
        unsigned long state, IOService * whatDevice )
{
    IOReturn ret;

    DEBG(thisIndex, " (%08lx)\n", flags);

    FBLOCK();

    if (state && !getPowerState())
    {
        gIOFBSystemPower = true;
        sleepConnectCheck = true;
        gIOFBLastClamshellState = 0;
    }

    if (IOPMDeviceUsable & flags)
        ret = IOPMAckImplied;

    else
    {
        notifyServerAll( false );

        if (serverState != serverNotified)
        {
            // server will ack within 45 seconds
            serverPendingAck = true;
            ret = 45 * 1000 * 1000;
        }
        else
            ret = IOPMAckImplied;
    }

    FBUNLOCK();

    return (ret);
}

IOReturn IOFramebuffer::powerStateDidChangeTo( IOPMPowerFlags flags,
        unsigned long, IOService* whatDevice )
{
    DEBG(thisIndex, " (%08lx)\n", flags);

    FBLOCK();

    isUsable = (0 != (IOPMDeviceUsable & flags));

    serverState = serverNotified;

    if (isUsable)
        notifyServerAll( true );

    FBUNLOCK();

    return (kIOReturnSuccess);
}

void IOFramebuffer::clamshellWork( thread_call_param_t p0, thread_call_param_t p1 )
{
    clamshellEnable( (SInt32) p1 );
}

void IOFramebuffer::clamshellEnable( SInt32 delta )
{
    UInt32	change;
    bool	desktopMode;
    bool	notSuspended;
    OSObject *  state;

    FBLOCK();
    gIOFBClamshellEnable += delta;
    notSuspended = gIOFBSystemPower && (0 == gIOFBSuspendCount);
    desktopMode = notSuspended && gIOFBDesktopModeAllowed && (gIOFBClamshellEnable <= 0);
    FBUNLOCK();

    if (delta < 0)
        change = kIOPMDisableClamshell;
    else if (notSuspended)
        change = kIOPMEnableClamshell | kIOPMSetDesktopMode | (desktopMode ? kIOPMSetValue : 0);
    else
        return ;

    gIOFBClamshellState = change;
    getPMRootDomain()->receivePowerNotification( change );

    if ((kIOPMEnableClamshell & change)
            && (state = getPMRootDomain()->getProperty(kAppleClamshellStateKey)))
    {
        publishResource(kAppleClamshellStateKey, gIOFBLastClamshellState ? kOSBooleanTrue : kOSBooleanFalse );
    }

    FBLOCK();
    if (((kIOPMEnableClamshell | kIOPMSetDesktopMode) == change)
     && (kOSBooleanFalse == state)
     && gIOFBLastReadClamshellState)
    {
	// clamshell was closed during probe, now open => reprobe
	probeAll(kIOFBUserRequestProbe);
    }
    FBUNLOCK();
}

IOOptionBits IOFramebuffer::clamshellState( void )
{
    return (gIOFBClamshellState);
}

IOReturn IOFramebuffer::systemPowerChange( void * target, void * refCon,
        UInt32 messageType, IOService * service,
        void * messageArgument, vm_size_t argSize )
{
    IOReturn ret;
    IOPowerStateChangeNotification * params = (IOPowerStateChangeNotification *) messageArgument;

    DEBG(0, "(%08lx, %ld)\n",
         messageType, (UInt32) params->powerRef);

    switch (messageType)
    {
        case kIOMessageSystemWillSleep:

            gIOFBClamshellState = kIOPMDisableClamshell;
            getPMRootDomain()->receivePowerNotification( kIOPMDisableClamshell );

            FBLOCK();

            gIOFBSystemPower       = false;
            gIOGraphicsSystemPower = false;
            gIOFBSystemPowerAckRef = (UInt32) params->powerRef;
            gIOFBSystemPowerAckTo  = service;

            startThread(true);

            FBUNLOCK();

            // We will ack within 20 seconds
            params->returnValue = 20 * 1000 * 1000;
            ret 		= kIOReturnSuccess;
            break;

        case kIOMessageSystemWillPowerOn:
            gIOGraphicsSystemPower = true;
	    /* fall thru */

        case kIOMessageSystemHasPoweredOn:
            params->returnValue = 0;
            ret 		= kIOReturnSuccess;
            break;

	case kIOMessageSystemWillRestart:
        case kIOMessageSystemWillPowerOff:

            FBLOCK();
            if (gAllFramebuffers)
            {
		IOFramebuffer * fb;
                for (UInt32 index = 0;
                        (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
                        index++)
		{
		    fb->deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
                    fb->setAttribute( kIOSystemPowerAttribute, messageType );
		}
            }
            FBUNLOCK();

            params->returnValue = 0;
            ret 		= kIOReturnSuccess;
            break;

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

    if (self->__private->pendingSpeedChange && !gIOFBSleepThread)
    {
        self->__private->pendingSpeedChange = false;
	self->setAttribute(kIOFBSpeedAttribute, self->__private->reducedSpeed);
    }
}

IOReturn IOFramebuffer::setAggressiveness( unsigned long type, unsigned long newLevel )
{
    UInt32 reducedSpeed = newLevel;

    if (__private
     && (type == (unsigned long) kIOFBLowPowerAggressiveness))
    {
	__private->reducedSpeed       = reducedSpeed;
	__private->pendingSpeedChange = true;
	if (__private->allowSpeedChanges && __private->deferredSpeedChangeEvent)
	    __private->deferredSpeedChangeEvent->interruptOccurred(0, 0, 0);
    }

    super::setAggressiveness(type, newLevel);

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::getAggressiveness( unsigned long type, unsigned long * currentLevel )
{
    IOReturn ret;

    if (gIOFBGate && (type == (unsigned long) kIOFBLowPowerAggressiveness))
    {
	FBLOCK();
	*currentLevel = __private->reducedSpeed;
	FBUNLOCK();
	ret = kIOReturnSuccess;
    }
    else
	ret = super::getAggressiveness(type, currentLevel);

    return (ret);
}

IOReturn
IOFramebuffer::extAcknowledgeNotification( void )
{
    IOReturn err;
    bool needConnectCheck, needAck;

    if ((err = extEntry()))
	return (err);

    DEBG(thisIndex, " (%d->%d, %d)\n",
         serverState, serverNotified, serverPendingAck);

    needConnectCheck = (serverState != serverNotified);
    serverState = serverNotified;

    needAck = serverPendingAck;
    serverPendingAck = false;

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

    checkDeferredCLUTSet();

    if (needConnectCheck)
        checkConnectionChange();

    startThread(!gIOFBSystemPower || sleepConnectCheck);

    FBUNLOCK();

    if (needAck)
        acknowledgePowerChange(this);

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extRegisterNotificationPort(
    mach_port_t 	port,
    UInt32		type,
    UInt32		refCon )
{
    mach_msg_header_t * msgh;
    UInt8		currentState;

    FBLOCK();

    msgh = (mach_msg_header_t *) serverMsg;
    bzero( msgh, sizeof(mach_msg_header_t) );

    msgh->msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msgh->msgh_size        = sizeof(mach_msg_header_t);
    msgh->msgh_remote_port = port;

    currentState     = serverNotified;
    serverNotified   = true;    		// server assumes so at startup
    wakeServerState(serverNotified);
    serverPendingAck = false;

    notifyServer( currentState );

    msgh->msgh_id = 0x87654321;
    mach_msg_send_from_kernel( msgh, msgh->msgh_size );

    FBUNLOCK();

    return (kIOReturnSuccess);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::writePrefs( OSObject * owner, IOTimerEventSource * sender )
{
    IOFramebuffer * self = (IOFramebuffer *) owner;
    DEBG(self->thisIndex, "\n");
    self->messageClients( kIOMessageServicePropertyChange, (void *) 'pref' );
}

void IOFramebuffer::connectChangeInterrupt( IOFramebuffer * inst, void * delay )
{
    OSIncrementAtomic( &inst->connectChange);

    DEBG(inst->thisIndex, "(%ld)\n", inst->connectChange);

    if (0 == inst->configPending)
    {
        if (delay && inst->__private->delayedConnectInterrupt)
            inst->__private->delayedConnectInterrupt->setTimeoutMS((UInt32) delay);
        else
            inst->deferredEvents->interruptOccurred(0, 0, 0);
    }
}

void IOFramebuffer::connectChangeDelayedInterrupt( OSObject * owner, IOTimerEventSource * sender )
{
    IOFramebuffer * self = (IOFramebuffer *) owner;

    if (0 == self->configPending)
        self->deferredEvents->interruptOccurred(0, 0, 0);
}

void IOFramebuffer::deferredInterrupt( OSObject * owner,
                                       IOInterruptEventSource * evtSrc, int intCount )
{
    IOFramebuffer * self = (IOFramebuffer *) owner;

    self->checkConnectionChange();
}

void IOFramebuffer::checkConnectionChange( bool message )
{
    bool nowSuspended;
    bool connectChangeOverSleep = (__private->disabledForConnectChange);

    DEBG(thisIndex, " count(%ld), susp(%d), sleep(%d), capt(%d)\n",
         connectChange, suspended, connectChangeOverSleep, captured);

    if ((gIOFBDesktopModeAllowed || !gIOFBLastClamshellState)
	&& (connectChange && (connectChangeOverSleep || !captured)))
    {
        FBLOCK();
        nowSuspended = !suspended;
        if (nowSuspended)
        {
            suspended = true;
            messaged = false;
            connectChange = 0;
            gIOFBSuspendCount++;
        }

        FBUNLOCK();

        if (message)
        {
            IOFramebuffer * next = this;
            while ((next = next->getNextDependent()) && (next != this))
            {
                next->checkConnectionChange(false);
            }
        }

        if (nowSuspended)
        {
            if (message)
                messageClients( kIOMessageServiceIsSuspended, (void *) true );
        }
        else
            DEBG(thisIndex, " spurious\n");
    }
    sleepConnectCheck = false;
    thread_call_enter1(gIOFBClamshellCallout, (thread_call_param_t) 0);
}

static bool 
IOFramebufferLockedSerialize(void * target, void * ref, OSSerialize * s) 
{
    bool ok;
    FBLOCK();
    ok = ((OSObject *) target)->serialize(s);
    FBUNLOCK();
    return (ok);
}                                     

IOReturn IOFramebuffer::open( void )
{
    IOReturn		err = kIOReturnSuccess;
    UInt32		value;
    void *		vblInterrupt;
    void *		connectInterrupt;
    IOFramebuffer *	next;
    OSNumber * 		num;
    bool		firstOpen;

    do
    {
        if (gIOFBGate)
	    FBLOCK();
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

            IORegistryEntry   * root;
            OSData *	        data = 0;
            if ((root = IORegistryEntry::fromPath("/", gIOServicePlane)))
            {
                data = OSDynamicCast(OSData, root->getProperty("graphic-options"));
                root->release();
            }
            gIOFBDesktopModeAllowed = !data || (0 != (8 & *((UInt32 *) data->getBytesNoCopy())));

	    OSIterator * iter = getMatchingServices(serviceMatching("AppleGraphicsControl"));
	    if (iter)
	    {
		gIOGraphicsControl = OSDynamicCast(IOService, iter->getNextObject());
		iter->release();
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
	if (!gIOFramebufferKey || !gIOFBGetSensorValueKey || !gIOFBRotateKey)
            continue;
	
        if (!gAllFramebuffers)
            continue;
        if (!gIOFBRootNotifier)
            gIOFBRootNotifier = getPMRootDomain()->registerInterest(
                                    gIOPriorityPowerStateInterest, &systemPowerChange, 0, 0 );
        if (!gIOFBRootNotifier)
            continue;
        if (!gIOFBSleepCallout)
            gIOFBSleepCallout = thread_call_allocate( (thread_call_func_t)&sleepWork,
                                (thread_call_param_t) 0);
        if (!gIOFBSleepCallout)
            continue;
        if (!gIOFBClamshellCallout)
            gIOFBClamshellCallout = thread_call_allocate( &clamshellWork,
                                    (thread_call_param_t) 0);
        if (!gIOFBClamshellCallout)
            continue;
        if (!gIOFBWorkLoop)
        {
            OSIterator * iter = getMatchingServices( nameMatching("IOHIDSystem") );
            if (iter)
            {
                IOService * hidsystem;
                if ((hidsystem = OSDynamicCast(IOService, iter->getNextObject())))
                {
                    gIOFBWorkLoop = hidsystem->getWorkLoop();
                    if (gIOFBWorkLoop)
                        gIOFBWorkLoop->retain();
                }
                iter->release();
            }
        }
        if (!gIOFBWorkLoop)
            gIOFBWorkLoop = IOWorkLoop::workLoop();
        if (!gIOFBWorkLoop)
            continue;

        if (!gIOFBDelayedPrefsEvent)
        {
            gIOFBDelayedPrefsEvent = IOTimerEventSource::timerEventSource(
                                                    this, &writePrefs);
            if (gIOFBDelayedPrefsEvent)
                getWorkLoop()->addEventSource(gIOFBDelayedPrefsEvent);
        }

	if (!IODisplayWrangler::serverStart())
	    continue;

        if (!gIOFBGate)
	{
            gIOFBGate = IOFBGate::gate( this );
	    if (!gIOFBGate)
		continue;
	    gIOFBWorkLoop->addEventSource( gIOFBGate );

	    FBLOCK();
	}

	if (!gIOFBClamshellProbeTES)
	{
	    gIOFBClamshellProbeTES = IOTimerEventSource::timerEventSource(
						this, &clamshellProbeAction);
	    if (gIOFBClamshellProbeTES)
		gIOFBWorkLoop->addEventSource(gIOFBClamshellProbeTES);

	    gIOFBClamshellNotify = addNotification( gIOPublishNotification,
						       resourceMatching(kAppleClamshellStateKey),
						       &clamshellHandler, NULL, 0, 10000 );
	}

        serverNotified   = true;
        serverState      = true;

        // tell the console if it's on this display, it's going away
        if (isConsoleDevice() /*&& !gIOFBConsoleFramebuffer*/)
            gIOFBConsoleFramebuffer = this;
	if ((this == gIOFBConsoleFramebuffer) || !gIOFBConsoleFramebuffer)
            getPlatform()->setConsoleInfo( 0, kPEDisableScreen);

        deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );

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
            continue;
        }

        pagingState = true;

        thisIndex = gAllFramebuffers->getCount();
        gAllFramebuffers->setObject(this);
        DEBG(thisIndex, " this %p \"%s\" \"%s\"\n", this, getName(), getProvider()->getName());

	setProperty(kIOFramebufferOpenGLIndexKey, thisIndex, 64);

	if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBTransformKey))))
	    __private->selectedTransform = num->unsigned64BitValue();
	__private->selectedTransform |= kIOFBDefaultScalerUnderscan;

        //
        err = registerForInterruptType( kIOFBVBLInterruptType,
                                        (IOFBInterruptProc) &handleVBL,
                                        this, priv, &vblInterrupt );
        haveVBLService = (err == kIOReturnSuccess );

        deferredEvents = IOInterruptEventSource::interruptEventSource(this, deferredInterrupt);
        if (deferredEvents)
            getWorkLoop()->addEventSource(deferredEvents);

        OSObject * obj = copyProperty(kIOFBConnectInterruptDelayKey, gIOServicePlane);
        if (obj)
        {
            OSData * data;
            if ((data = OSDynamicCast(OSData, obj)))
                __private->delayedConnectTime = *((UInt32 *) data->getBytesNoCopy());
            obj->release();
        }
        if (__private->delayedConnectTime)
        {
            __private->delayedConnectInterrupt = IOTimerEventSource::timerEventSource(
                                                    this, &connectChangeDelayedInterrupt);
            if (__private->delayedConnectInterrupt)
                getWorkLoop()->addEventSource(__private->delayedConnectInterrupt);
        }

        err = registerForInterruptType( kIOFBConnectInterruptType,
                                        (IOFBInterruptProc) &connectChangeInterrupt,
                                        this, (void *) __private->delayedConnectTime,
                                        &connectInterrupt );
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
					(UInt32 *) &__private->defaultI2CTiming));

	if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaWidthKey))))
	    __private->desiredGammaDataWidth = num->unsigned32BitValue();
	if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaCountKey))))
	    __private->desiredGammaDataCount = num->unsigned32BitValue();
	if ((num = OSDynamicCast(OSNumber, getProperty(kIOFBGammaHeaderSizeKey))))
	    __private->gammaHeaderSize = num->unsigned32BitValue();

	if (haveVBLService
	 && (kIOReturnSuccess == getAttribute( kIODeferCLUTSetAttribute, &value ))
	 && value)
	{
	    __private->deferredCLUTSetEvent = IOInterruptEventSource::interruptEventSource(
									this, deferredCLUTSetInterrupt);
	    if (__private->deferredCLUTSetEvent)
	    {
		getWorkLoop()->addEventSource(__private->deferredCLUTSetEvent);
		setProperty(kIOFBCLUTDeferKey, kOSBooleanTrue);
	    }
	}
	__private->deferredSpeedChangeEvent = IOInterruptEventSource::interruptEventSource(
								    this, deferredSpeedChangeEvent);
	if (__private->deferredSpeedChangeEvent)
	    getWorkLoop()->addEventSource(__private->deferredSpeedChangeEvent);

        num = OSDynamicCast( OSNumber, getProperty(kIOFBDependentIDKey) );
        firstOpen = num && !nextDependent;

        if (firstOpen)
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
                propMatch->setObject( kIOFBDependentIDKey, num );
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

        UInt32 connectEnabled;
        err = getAttributeForConnection( 0, kConnectionEnable, &connectEnabled );
        if (kIOReturnSuccess != err)
            connectEnabled = true;

	if (!gIOFBConsoleFramebuffer && connectEnabled)
	    gIOFBConsoleFramebuffer = this;

	__private->paramHandler = IOFramebufferParameterHandler::withFramebuffer(this);
	if (__private->paramHandler)
	    setProperty(gIODisplayParametersKey, __private->paramHandler);

        IOFramebufferI2CInterface::create( this );

	__private->online = (0 != connectEnabled);
        if (connectEnabled)
            IODisplayWrangler::makeDisplayConnects(this);
	else
	    IODisplayUpdateNVRAM(this, 0);

	__private->transform = __private->selectedTransform;
	setProperty(kIOFBTransformKey, __private->transform, 64);

        if (firstOpen)
        {
            next = this;
            while ((next = next->getNextDependent()) && (next != this))
            {
                next->open();
            }
        }

        if (connectEnabled)
        {
            setupForCurrentConfig();
            err = kIOReturnSuccess;
        }
        else
	    deliverDisplayModeDidChangeNotification();

        // if( firstOpen)
        {
            next = this;
            do
            {
                next->postOpen();
            }
            while ((next = next->getNextDependent()) && (next != this));
        }
    }
    while (false);

    if (opened)
	checkConnectionChange();

    __private->allowSpeedChanges = true;
    if (__private->pendingSpeedChange)
    {
        __private->pendingSpeedChange = false;
	setAttribute(kIOFBSpeedAttribute, __private->reducedSpeed);
    }
    
    if (gIOFBGate)
        FBUNLOCK();

    return (err);
}

void IOFramebuffer::setTransform( UInt64 newTransform, bool generateChange )
{
    newTransform |= (kIOFBScalerUnderscan & __private->selectedTransform);

    if (newTransform != __private->selectedTransform)
    {
        __private->userSetTransform = true;
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

IOReturn IOFramebuffer::checkMirrorSafe( UInt32 value, IOFramebuffer * other )
{
    IOReturn        err = kIOReturnSuccess;
    IOFramebuffer * next = this;

    while ((next = next->getNextDependent()) && (next != this))
    {
	if (~kIOFBScalerUnderscan & (__private->transform ^ next->getTransform()))
	{
	    err = kIOReturnUnsupported;
	    break;
	}
    }

    return (err);
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
//          if (kIOReturnSuccess == err)
		break;
	}
	for (index = 0;
		(fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
		index++)
	{
	    if (fb->captured || fb->__private->offlinePoweredOff)
		continue;

	    IOReturn
	    thisErr = fb->setAttributeForConnection(0, kConnectionProbe, options);
	    if (kIOReturnSuccess == err)
		err = thisErr;
	}
    }
    while (false);

    return (err);
}

IOReturn IOFramebuffer::requestProbe( IOOptionBits options )
{
    IOReturn err;

    if (!gIOFBGate || gIOFBGate->inGate())
	return (kIOReturnNotReady);

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
        FBUNLOCK();
	return (kIOReturnSuccess);
    }

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
	    clock_get_uptime(&now);
	    if (CMP_ABSOLUTETIME(&now, &gIOFBNextProbeAllTime) >= 0) 
	    {
		err = probeAll(options);
		clock_interval_to_deadline(10, kSecondScale, &gIOFBNextProbeAllTime);
	    }
	}
    }

    FBUNLOCK();

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::postOpen( void )
{
    IOService * sensor = 0;

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

        desc.majorVersion 	= kHardwareCursorDescriptorMajorVersion;
        desc.minorVersion 	= kHardwareCursorDescriptorMinorVersion;
        desc.height 		= 256;
        desc.width 		= 256;
        desc.bitDepth 		= 32;
        desc.maskBitDepth 	= 0;
        desc.colorEncodings	= 0;
        desc.flags 		= 0;
        desc.supportedSpecialEncodings = kTransparentEncodedPixel;

        (*__private->cursorControl.callouts->setCursorImage) (
            __private->cursorControl.self, __private->cursorControl.ref,
            &desc, (void *) 0 );

        if (gIOFBWorkLoop)
            gIOFBWorkLoop->addEventSource(__private->cursorThread);
    }
    __private->testingCursor = false;

    setProperty( kIOFBCursorInfoKey, __private->cursorAttributes );


    UInt32 value[16];

//#define kTempAttribute	kConnectionWSSB
#define kTempAttribute	'thrm'

    if (!__private->temperatureSensor
     && (kIOReturnSuccess == getAttributeForConnection(0, kTempAttribute, &value[0])))
    do
    {
	UInt32	   data;
	OSNumber * num;

        num = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIDKey));
        if (num && num->unsigned32BitValue())
	    continue;

	sensor = new IOService;
	if (!sensor)
	    continue;
	if (!sensor->init())
	    continue;

#define kTempSensorName 	"temp-sensor"
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
    UInt32   value[16];
    IOReturn ret;

    if (functionName != gIOFBGetSensorValueKey)
	return (super::callPlatformFunction(functionName, waitForFunction, p1, p2, p3, p4));

    FBLOCK();
    ret = getAttributeForConnection(0, kTempAttribute, &value[0]);
    FBUNLOCK();

    if (kIOReturnSuccess == ret)
	*((UInt32 *)p2) = ((value[0] & 0xffff) << 16);

    return (ret);
}

IOWorkLoop * IOFramebuffer::getWorkLoop() const
{
    return (gIOFBWorkLoop);
}

void IOFramebuffer::setCaptured( bool isCaptured )
{
    bool wasCaptured = captured;

    captured = isCaptured;
    if (wasCaptured != isCaptured)
    {
	if (isCaptured)
	    setProperty(kIOFBCapturedKey, kOSBooleanTrue);
	else
	    removeProperty(kIOFBCapturedKey);
	deliverFramebufferNotification(kIOFBNotifyCaptureChange, (void *) isCaptured);
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

void IOFramebuffer::close( void )	// called by the user client when
{					// the window server exits
    mach_msg_header_t * msgh;
    unsigned int	idx;

    if (this == gIOFBConsoleFramebuffer)
        getPlatform()->setConsoleInfo( 0, kPEAcquireScreen);

    msgh = (mach_msg_header_t *) serverMsg;
    if (msgh)
	msgh->msgh_remote_port = MACH_PORT_NULL;

    serverConnect = 0;
    captured = false;

    if (gRunawayFramebuffers)
    {
	FBLOCK();

	idx = gAllFramebuffers->getNextIndexOfObject( this, 0 );
	if (idx != (unsigned int) -1)
	    gAllFramebuffers->removeObject(idx);
    
	idx = gRunawayFramebuffers->getNextIndexOfObject( this, 0 );
	if (idx != (unsigned int) -1)
	    gRunawayFramebuffers->removeObject(idx);
    
	FBUNLOCK();

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
    UInt32		i, numRanges;
    IODeviceMemory *	mem;
    numRanges = userAccessRanges->getCount();
    DEBG(thisIndex, " ranges num:%ld\n", numRanges);
    for (i = 0; i < numRanges; i++)
    {
        mem = (IODeviceMemory *) userAccessRanges->getObject( i );
        if (0 == mem)
            continue;
        DEBG(thisIndex, " start:%lx size:%lx\n",
             mem->getPhysicalAddress(), mem->getLength() );
    }
#endif

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::setupForCurrentConfig( void )
{
    return (doSetup(true));
}

IOReturn IOFramebuffer::doSetup( bool full )
{
    IOReturn			err;
    IODisplayModeID		mode;
    IOIndex			depth;
    IOPixelInformation		info;
    IODeviceMemory *		mem;
    IODeviceMemory *		fbRange;
    IOPhysicalAddress		base;
    UInt32			value;
    bool			haveFB;
    PE_Video			newConsole;

    err = getAttribute( kIOHardwareCursorAttribute, &value );
    __private->cursorPanning = ((err == kIOReturnSuccess) && (0 != (kIOFBCursorPans & value)));

    err = getCurrentDisplayMode( &mode, &depth );
    if (kIOReturnSuccess == err)
	err = getPixelInformation( mode, depth, kIOFBSystemAperture, &info );
    haveFB = (__private->online && (kIOReturnSuccess == err) && (info.activeWidth >= 128)); 

    IOTimingInformation timingInfo;
    timingInfo.flags = kIODetailedTimingValid;
    if (haveFB 
     && (kIOReturnSuccess == getTimingInfoForDisplayMode(mode, &timingInfo)))
    {
	if (kIODetailedTimingValid & timingInfo.flags)
	{
	    setProperty(kIOFBCurrentPixelClockKey, timingInfo.detailedInfo.v2.pixelClock, 64);
	    setProperty(kIOFBCurrentPixelCountKey, ((UInt64)(timingInfo.detailedInfo.v2.horizontalActive
							    + timingInfo.detailedInfo.v2.horizontalBlanking))
						* ((UInt64)(timingInfo.detailedInfo.v2.verticalActive
							   + timingInfo.detailedInfo.v2.verticalBlanking)), 64);
	}
	else
	{
	    removeProperty(kIOFBCurrentPixelClockKey);
	    removeProperty(kIOFBCurrentPixelCountKey);
	}

	__private->scaledMode = false;
	if (kIOScalingInfoValid & timingInfo.flags)
	{
	    __private->scaledMode =
	    (timingInfo.detailedInfo.v2.scalerFlags
	     || timingInfo.detailedInfo.v2.horizontalScaledInset
	     || timingInfo.detailedInfo.v2.verticalScaledInset
	     || (timingInfo.detailedInfo.v2.horizontalScaled 
		    && (timingInfo.detailedInfo.v2.horizontalScaled != timingInfo.detailedInfo.v2.horizontalActive))
	     || (timingInfo.detailedInfo.v2.verticalScaled 
		    && (timingInfo.detailedInfo.v2.verticalScaled != timingInfo.detailedInfo.v2.verticalActive)));
	}
    }

    fbRange = getApertureRange( kIOFBSystemAperture );

    if (full && fbRange)
    {
        userAccessRanges->removeObject( kIOFBSystemAperture );
        userAccessRanges->setObject( kIOFBSystemAperture, fbRange );
        err = setUserRanges();

        base = fbRange->getPhysicalAddress();
        if ((mem = getVRAMRange()))
        {
            vramMapOffset = base - mem->getPhysicalAddress();
            if (vramMapOffset > mem->getLength())
                vramMapOffset &= (mem->getLength() - 1);
            setProperty( kIOFBMemorySizeKey, mem->getLength(), 32 );
            mem->release();
        }

        if (vramMap)
            vramMap->release();
        vramMap = fbRange->map( kIOFBMapCacheMode );
        assert( vramMap );
        if (vramMap)
	{
            base = vramMap->getVirtualAddress();
	    frameBuffer = (volatile unsigned char *) base;
	}

        // console now available
        if (haveFB && ((this == gIOFBConsoleFramebuffer) || !gIOFBConsoleFramebuffer))
        {
	    bzero(&newConsole, sizeof(newConsole));
            newConsole.v_baseAddr	= base;
            newConsole.v_rowBytes	= info.bytesPerRow;
            newConsole.v_width		= info.activeWidth;
            newConsole.v_height		= info.activeHeight;
            newConsole.v_depth		= info.bitsPerPixel;
            newConsole.v_display 	= 1;  // graphics mode for i386
            //	strcpy( consoleInfo->v_pixelFormat, "PPPPPPPP");
            getPlatform()->setConsoleInfo( &newConsole, kPEReleaseScreen );
            getPlatform()->setConsoleInfo( &newConsole, kPEEnableScreen );
            gIOFBConsoleFramebuffer	= this;
        }

        DEBG(thisIndex, " using (%ldx%ld,%ld bpp)\n",
             info.activeWidth, info.activeHeight, info.bitsPerPixel );
    }

    if (full)
	deliverDisplayModeDidChangeNotification();

    if (fbRange)
        fbRange->release();
    if (vramMap && haveFB)
        setupCursor( &info );

    return (kIOReturnSuccess);
}

IOReturn IOFramebuffer::extSetDisplayMode( IODisplayModeID displayMode,
        IOIndex depth )
{
    IOReturn	err;
    bool	wasSuspended;

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
        FBUNLOCK();
	return (kIOReturnSuccess);
    }

    stopCursor();

    checkDeferredCLUTSet();

    if (this == gIOFBConsoleFramebuffer)
    {
        getPlatform()->setConsoleInfo( 0, kPEDisableScreen);
	gIOFBConsoleFramebuffer = 0;
    }

    wasSuspended = (suspended && messaged);	// this mode set is during connection change ack

    DEBG(thisIndex, " susp(%d)\n", wasSuspended);

    if (!wasSuspended)
        deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );

    err = setDisplayMode( displayMode, depth );

    clutValid = false;

    DEBG(thisIndex, " paramHandler\n");

    if (__private->paramHandler)
	__private->paramHandler->displayModeChange();

    setupForCurrentConfig();

    if (wasSuspended)
    {
        suspended = false;
        --gIOFBSuspendCount;
        __private->disabledForConnectChange = false;
    }

    if (connectChange)
        checkConnectionChange();

    if (wasSuspended && !suspended)
    {
        AbsoluteTime deadline;

        clock_interval_to_deadline( 5*1000, kMillisecondScale, &deadline );
        thread_call_enter1_delayed( gIOFBClamshellCallout,
                                    (thread_call_param_t) 0, deadline );
    }

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extSetAttribute(
    IOSelect attribute, UInt32 value, IOFramebuffer * other )
{
    IOReturn	err;
    UInt32	data[2];

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
        FBUNLOCK();
	return (kIOReturnSuccess);
    }

    switch (attribute)
    {
        case kIOMirrorAttribute:

            DEBG(thisIndex, " kIOMirrorAttribute(%ld) susp(%d), curr(%d)\n", 
		    value, suspended, __private->mirrorState);

	    value = (value != 0);
	    if (value)
	    {
		err = checkMirrorSafe(value, other);
		if (kIOReturnSuccess != err)
		    break;
	    }

            if (suspended)
                break;

	    if (value == __private->mirrorState)
                break;

            stopCursor();

	    checkDeferredCLUTSet();

            if (this == gIOFBConsoleFramebuffer)
                getPlatform()->setConsoleInfo( 0, kPEDisableScreen);

            deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );

            data[0] = value;
            data[1] = (UInt32) other;
            err = setAttribute( attribute, (UInt32) &data );
	    if (kIOReturnSuccess == err)
		__private->mirrorState = value;

            clutValid = false;

            setupForCurrentConfig();
            break;

        default:
            err = setAttribute( attribute, value );
            break;
    }

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extGetAttribute(
    IOSelect attribute, UInt32 * value, IOFramebuffer * other )
{
    IOReturn	err = kIOReturnSuccess;
    bool	nowOnline;

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
        FBUNLOCK();
	return (kIOReturnOffline);
    }

    switch (attribute)
    {
        case kConnectionChanged:
            {
                UInt32	connectEnabled;

                DEBG(thisIndex, " kConnectionChanged susp(%d)\n", suspended);

                if (!suspended)
                    break;

		checkDeferredCLUTSet();

                if (!__private->disabledForConnectChange)
                {
                    __private->disabledForConnectChange = true;
                    deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
                }

		__private->enableScalerUnderscan = false;

                err = getAttributeForConnection( 0, kConnectionChanged, (UInt32 *) &connectChange );

		__private->mirrorState = false;

		if (__private->paramHandler)
		    __private->paramHandler->setDisplay(0);

                err = getAttributeForConnection( 0, kConnectionEnable, &connectEnabled );
                nowOnline = (!dead && ((kIOReturnSuccess != err) || connectEnabled));

                temporaryPowerClampOn();
		__private->online = nowOnline;
                FBUNLOCK();
                IODisplayWrangler::destroyDisplayConnects( this );
		if (nowOnline)
                    IODisplayWrangler::makeDisplayConnects( this );
                FBLOCK();

                __private->transform = __private->selectedTransform;
                setProperty(kIOFBTransformKey, __private->transform, 64);

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
                messaged = true;

                err = kIOReturnSuccess;
            }
            break;

        default:
            {
                *value = (UInt32) other;
                err = getAttribute( attribute, value );
            }
            break;
    }

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extGetInformationForDisplayMode(
    IODisplayModeID mode, void * info, IOByteCount length )
{
    UInt32			 flags = 0;
    IOReturn			 err;
    bool			 getTiming;
    IOFBDisplayModeDescription * out = (IOFBDisplayModeDescription *) info;

    if (length < sizeof(IODisplayModeInformation))
        return (kIOReturnBadArgument);

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
	out->info.nominalWidth = 1;
	out->info.nominalHeight = 1;
	out->info.refreshRate = 0;
	out->info.maxDepthIndex = 0;
	out->info.flags = kDisplayModeValidFlag 
			| kDisplayModeSafeFlag 
			| kDisplayModeDefaultFlag;
	out->timingInfo.flags = 0;
	out->timingInfo.appleTimingID = kIOTimingIDApple_0x0_0hz_Offline;
    }
    else
    {
	err = getInformationForDisplayMode( mode, &out->info );
	if (kIOReturnSuccess == err)
	{
	    err = IODisplayWrangler::getFlagsForDisplayMode( this, mode, &flags);
	    if (kIOReturnSuccess == err)
	    {
		out->info.flags &= ~kDisplayModeSafetyFlags;
		out->info.flags |= flags;
	    }
	    getTiming = (length >= sizeof(IOFBDisplayModeDescription));
	    out->timingInfo.flags = getTiming ? kIODetailedTimingValid : 0;
	    if (kIOReturnSuccess != getTimingInfoForDisplayMode(mode, &out->timingInfo))
	    {
		out->timingInfo.flags &= ~kIODetailedTimingValid;
		out->timingInfo.appleTimingID = 0;
	    }
	}
    }

    FBUNLOCK();

    return (err);
}

IOReturn IOFramebuffer::extSetProperties( OSDictionary * props )
{
    OSDictionary * dict;
    OSArray *      array;
    OSNumber *     num;
    IOReturn       err = kIOReturnUnsupported;

    if ((err = extEntry()))
	return (err);

    if (__private->offlinePoweredOff)
    {
        FBUNLOCK();
	return (kIOReturnSuccess);
    }

    err = kIOReturnUnsupported;
    if ((dict = OSDynamicCast(OSDictionary, props->getObject(kIOFBConfigKey))))
    {
        setProperty( kIOFBConfigKey, dict );

        if ((num = OSDynamicCast(OSNumber,
                                 dict->getObject(kIODisplayConnectFlagsKey))))
            setAttributeForConnection( 0, kConnectionFlags, num->unsigned32BitValue() );

        if (dict->getObject("IOFBScalerUnderscan"))
        {
	    __private->enableScalerUnderscan = true;
            if (__private->paramHandler)
                __private->paramHandler->displayModeChange();
        }

        if ((array = OSDynamicCast(OSArray,
                                   dict->getObject(kIOFBDetailedTimingsKey))))
            err = setDetailedTimings( array );
        else
            err = kIOReturnSuccess;
    }

    FBUNLOCK();

    return (err);
}

//// Controller attributes

IOReturn IOFramebuffer::setAttribute( IOSelect attribute, UInt32 value )
{
    IOReturn	    ret;
    IOFramebuffer * next;
    unsigned int    index;
    bool	    wasCaptured;
    bool	    newCaptured, newDimDisable;

    switch (attribute)
    {
        case kIOCapturedAttribute:
            {
                wasCaptured = captured;

                DEBG(thisIndex, " kIOCapturedAttribute(%ld)\n", value);

                newCaptured   = (kIOCaptureDisableDisplayChange  & value);
                newDimDisable = (kIOCaptureDisableDisplayDimming & value);

		// -- capture
                next = this;
                do
                {
                    next->setCaptured( newCaptured );
		}
                while ((next = next->getNextDependent()) && (next != this));

                if (wasCaptured && !captured)
                {
		    if (!gIOGraphicsControl)
		    {
			next = this;
			do
			{
			    next->getAttributeForConnection( 0, kConnectionChanged, 0 );
			}
			while ((next = next->getNextDependent()) && (next != this));
		    }

                    next = this;
                    do
                    {
                        next->checkConnectionChange();
                    }
                    while ((next = next->getNextDependent()) && (next != this));
                }

		// -- dim disable

		setDimDisable(newDimDisable);
	    
		for (index = 0;
			(next = (IOFramebuffer *) gAllFramebuffers->getObject(index));
			index++)
		{
		    newDimDisable |= next->getDimDisable();
		}

		if (newDimDisable != gIOFBDimDisable)
		{
		    gIOFBDimDisable = newDimDisable;
		    FBUNLOCK();
		    getPMRootDomain()->setAggressiveness( kIOFBCaptureAggressiveness, newDimDisable );
		    FBLOCK();
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
                    if (gIOFBWorkLoop && __private->cursorThread)
                        gIOFBWorkLoop->addEventSource(__private->cursorThread);
                }
                ret = kIOReturnSuccess;
                break;
            }

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

extern "C" IOReturn
IOGetHardwareClamshellState( IOOptionBits * result )
{
    IOReturn ret = kIOReturnUnsupported;

    // zero -> lid open
    *result = 0;

#if __ppc__

#define	readExtSwitches	0xDC

    UInt8 bootEnvIntData[32];
    IOByteCount iLen = sizeof(UInt8);

    ret = ApplePMUSendMiscCommand(readExtSwitches, 0, NULL, &iLen, &bootEnvIntData[0]);
    if (kIOReturnSuccess == ret)
    {
	gIOFBLastClamshellState = bootEnvIntData[0] & 1;
	if (gIOFBDesktopModeAllowed)
	    *result = bootEnvIntData[0];
    }

#elif __i386__
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
	ret = lidDevice->evaluateInteger("_LID", &lidState);
	if (kIOReturnSuccess == ret)
	    lidState = (lidState == 0);
	else
	    lidState = 0;

	gIOFBLastClamshellState = lidState;
	if (gIOFBDesktopModeAllowed)
	    *result = lidState;

    }
#endif

    gIOFBLastReadClamshellState = gIOFBLastClamshellState;

    DEBG(0, "(%ld) result %d\n", ret, *result);

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::clamshellProbeAction( OSObject * p0, IOTimerEventSource * evtSrc )
{
    OSObject * clamshellProperty;
    IOFramebuffer * fb;

    clamshellProperty = getPMRootDomain()->getProperty(kAppleClamshellStateKey);
    if (!clamshellProperty)
        return;

    for (uint32_t index = 0;
	    (fb = (IOFramebuffer *) gAllFramebuffers->getObject(index));
	    index++)
    {
	fb->deliverFramebufferNotification(kIOFBNotifyClamshellChange, clamshellProperty);
    }

    if (!gIOFBLastClamshellState)
    {
	enum { bits = kIOPMSetDesktopMode | kIOPMSetValue };
	if (bits == (bits & gIOFBClamshellState))
	{
	    // lid open, desktop mode
#if LID_OPEN_PROBE
	    probeAll(kIOFBUserRequestProbe);
#endif
	}
    }
}

bool IOFramebuffer::clamshellHandler( void * target, void * ref,
   				       IOService * resourceService )
{
    OSObject * clamshellProperty;

    clamshellProperty = resourceService->getProperty(kAppleClamshellStateKey);
    if (!clamshellProperty)
        return (true);

    gIOFBLastClamshellState = (clamshellProperty == kOSBooleanTrue);

    resourceService->removeProperty(kAppleClamshellStateKey);

    if (gIOFBClamshellProbeTES)
	gIOFBClamshellProbeTES->setTimeoutMS(kIOFBClamshellProbeDelayMS);

    return (true);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


IOReturn IOFramebuffer::getAttribute( IOSelect attribute, UInt32 * value )
{
    IOReturn ret = kIOReturnUnsupported;

    switch (attribute)
    {
      case kIOClamshellStateAttribute:
	ret = IOGetHardwareClamshellState( value );
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
    OSNumber *	num;
    bool	ok;

    num = OSNumber::withNumber( value, 32 );
    if (!num)
        return (false);

    ok = dict->setObject( key, num );
    num->release();

    return (ok);
}

bool IOFramebuffer::serializeInfo( OSSerialize * s )
{
    IOReturn			err;
    IODisplayModeInformation	info;
    IOPixelInformation		pixelInfo;
    IODisplayModeID *		modeIDs;
    IOItemCount			modeCount, modeNum, aperture;
    IOIndex			depthNum;
    OSDictionary *		infoDict;
    OSDictionary *		modeDict;
    OSDictionary *		pixelDict;
    char			keyBuf[12];
    bool			ok = true;

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

                sprintf( keyBuf, "%lx", depthNum + (aperture << 16) );
                modeDict->setObject( keyBuf, pixelDict );
                pixelDict->release();
            }
        }

        sprintf( keyBuf, "%lx", modeIDs[modeNum] );
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
    bool	ret;

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
    _IOFramebufferNotifier *	notify = 0;

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

        if (0 == fbNotifications)
            fbNotifications = OSSet::withCapacity(1);

        notify->whence = fbNotifications;
        if (fbNotifications)
            fbNotifications->setObject( notify );
    }

    return (notify);
}

IOReturn IOFramebuffer::deliverFramebufferNotification(
    IOIndex event, void * info )
{
    OSIterator *		iter;
    _IOFramebufferNotifier *	notify;
    IOReturn			ret = kIOReturnSuccess;
    IOReturn			r;

    DEBG(thisIndex, "(%ld)\n", event);

    LOCKNOTIFY();

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

IOReturn IOFramebuffer::setAttributeForConnection( IOIndex connectIndex,
        IOSelect attribute, UInt32 info )
{
    IOReturn err;

    switch( attribute )
    {
	case kConnectionOverscan:

            UInt64 newTransform;
            DEBG(thisIndex, " set oscn %d, ena %d\n", info, __private->enableScalerUnderscan);
            if (info) 
                newTransform = __private->selectedTransform & ~kIOFBScalerUnderscan;
            else
                newTransform = __private->selectedTransform | kIOFBScalerUnderscan;
	    if (__private->enableScalerUnderscan)
	    {
                if (newTransform != __private->selectedTransform)
                {
                    __private->selectedTransform = newTransform;
                    if (!__private->disabledForConnectChange)
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

IOReturn IOFramebuffer::getAttributeForConnection( IOIndex connectIndex,
        IOSelect attribute, UInt32 * value )
{
    IOReturn err;

    switch( attribute )
    {
	case kConnectionDisplayParameterCount:
	    if (__private->enableScalerUnderscan)
		*value = 1;
	    else
		*value = 0;
	    err = kIOReturnSuccess;
	    break;

	case kConnectionDisplayParameters:
	    if (__private->enableScalerUnderscan)
		*value = kConnectionOverscan;
	    err = kIOReturnSuccess;
	    break;

	case kConnectionOverscan:
	    if (__private->enableScalerUnderscan)
	    {
		value[0] = (0 == (kIOFBScalerUnderscan & __private->selectedTransform));
		DEBG(thisIndex, " oscn %d (%qx)\n", value[0], __private->selectedTransform);
		value[1] = 0;
		value[2] = 1;
		err = kIOReturnSuccess;
	    }
	    else
		err = kIOReturnUnsupported;
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
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::unregisterInterrupt( void * interruptRef )
{
    return (kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setInterruptState( void * interruptRef, UInt32 state )
{
    return (kIOReturnUnsupported);
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
    UInt8		startAddress;
    IOReturn		err;
    UInt32		badsums, timeouts;
    IOI2CBusTiming *	timing = &__private->defaultI2CTiming;

    if (!__private->lli2c)
	return (kIOReturnUnsupported);
    
    // Assume that we have already attempted to stop DDC1
    
    // Read the requested block (Block 1 is at 0x0, each additional block is at 0x80 offset)
    startAddress = kDDCBlockSize * (blockNumber - 1);
    if (length)
	*length = kDDCBlockSize;
    
    // Attempt to read the DDC data
    //  1.	If the error is a timeout, then it will attempt one more time.  If it gets another timeout, then
    //  	it will return a timeout error to the caller.
    //
    //  2.  If the error is a bad checksum error, it will attempt to read the block up to 2 more times.
    //		If it still gets an error, then it will return a bad checksum error to the caller.  
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
    IOReturn err = kIOReturnError;	// Assume failure

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
    File:	GraphicsCoreUtils.c
    Written by:	Sean Williams, Kevin Williams, Fernando Urbina
*/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      stopDDC1SendCommand(), 
//
//	The VESA spec for DDC says a display in DDC1 will transition from DDC1 to DDC2 when a valid DDC2
//	command is received.
//	DDC1 constantly spews data on the data line if syncs are active...bad for macintosh sensing
//	DDC2 only sends data when requested.
//	The VESA spec illustrates the manner to do this.
//	Read the first byte of data, send a Nack and a Stop.
//	This routine does that.
//
//	There is a delay of two vertical clock periods where the clock line is forced low. The 
//	NEC XE15 monitor has a controller that sometimes pulls the clockline low and never releases it.
//	This is bad, DDC will fail, and the monitor sensing algorithim will think a mono 1152x870 display
//	is attached. This isn't part of the spec but it fixes the NEC XE15.
//

IOReturn IOFramebuffer::stopDDC1SendCommand(IOIndex bus, IOI2CBusTiming * timing)
{
    UInt8	data;
    IOReturn	err = kIOReturnSuccess; 
    UInt8	address;

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
// 	i2cReadData()
//
//	The parameters are described as follows:
//
//			-> deviceAddress	device's I2C address
//			-> count		# of bytes to read
//			<- buffer		buffer for the data

IOReturn IOFramebuffer::i2cReadData(IOIndex bus, IOI2CBusTiming * timing,
				    UInt8 deviceAddress, UInt8 count, UInt8 * buffer)
{
    IOReturn	err = kIOReturnError;
    UInt32	attempts = 10;
    
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
// 	i2cWriteData()
//
//	The parameters are described as follows:
//
//			-> deviceAddress	device's I2C address
//			-> count		# of bytes to read
//			-> buffer		buffer for the data

IOReturn IOFramebuffer::i2cWriteData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 * buffer)
{
    IOReturn	err = kIOReturnError;
    UInt32	attempts = 10;
    
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
//	Watch the DDC data line and see if it is toggling. If the data line is toggling, it means
//	1) DDC display is connected
//	2) DDC controller in the display is ready to receive commands.
//
//			-> waitTime		max duration that the DDC data line should be watched

void IOFramebuffer::waitForDDCDataLine(IOIndex bus, IOI2CBusTiming * timing, UInt32 waitTime)
{
    AbsoluteTime	now, expirationTime;
    UInt32		dataLine;

    setDDCData(bus, kIODDCTristate);		// make sure data line is tristated

    // Set up the timeout timer...watch DDC data line for waitTime, see if it changes
    clock_interval_to_deadline(waitTime, kMillisecondScale, &expirationTime);
			    
    dataLine = readDDCData(bus);		// read present state of dataline
    
    while (true)
    {
	if (dataLine != readDDCData(bus))
	    break;
	
	clock_get_uptime(&now);
	if (CMP_ABSOLUTETIME(&now, &expirationTime) > 0)
	    break;
    }
}
	
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//
//      readDDCBlock()
//	Read one block of DDC data
//
//	The parameters are described as follows:
//
//			-> deviceAddress	device's I2C address
//			-> startAddress		start address to get data from
//			<- data			a block of EDID data

IOReturn IOFramebuffer::readDDCBlock(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 startAddress, UInt8 * data)
{
    IOReturn	err;
    UInt32 	i;
    UInt8 	sum = 0;
    
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
//	Start a I2C transaction

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
//	Send an ACK to acknowledge we received the data

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
//	Send an ACK to acknowledge we received the data

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
//	This routine will poll the SDA line looking for a LOW value and when it finds it, it will pulse
//	the CLK.

IOReturn IOFramebuffer::i2cWaitForAck(IOIndex bus, IOI2CBusTiming * timing)
{
    AbsoluteTime	now, expirationTime;
    IOReturn		err = kIOReturnSuccess;
    
    IODelay( 40 );
    
    // Set up a watchdog timer that will time us out, in case we never see the SDA LOW.
    clock_interval_to_deadline(1, kMillisecondScale, &expirationTime);

    while ((0 != readDDCData(bus)) && (kIOReturnSuccess == err))
    {
	clock_get_uptime(&now);
	if (CMP_ABSOLUTETIME(&now, &expirationTime) > 0)
	    err = kIOReturnNotResponding;				// Timed Out
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
//	Send a byte of data
//

void IOFramebuffer::i2cSendByte(IOIndex bus, IOI2CBusTiming * timing, UInt8 data)
{
    UInt8	valueToSend;
    int		i;
    
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
//	Read a byte of data

IOReturn IOFramebuffer::i2cReadByte(IOIndex bus, IOI2CBusTiming * timing, UInt8 *data)
{
    AbsoluteTime	now, expirationTime;
    IOReturn		err = kIOReturnSuccess;
    UInt32		i;
    UInt32		value;

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
	    clock_get_uptime(&now);
	    if (CMP_ABSOLUTETIME(&now, &expirationTime) > 0)
		err = kIOReturnNotResponding;			// Timed Out
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
//	Tristate DDC Clk and DDC Data lines

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
// 	i2cReadDDCciData()
//
//	The parameters are described as follows:
//
//			-> deviceAddress				device's I2C address
//			-> count						# of bytes to read
//			<- buffer						buffer for the data
//

IOReturn IOFramebuffer::i2cReadDDCciData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 *buffer)
{
    // This is a funky call that encodes the length of the transaction in the response. 
    // According to the VESA DDC/ci spec, the low 7 bits of second byte returned by the display 
    // will contain the length of the message less the checksum.  The card should then attempt to read 
    // that length plus the checksum but should not exceed "count" bytes.  
    // If the size exceeds "count", then the buffer should be filled with "count" bytes and the
    // transaction should be completed without copying more bytes into the buffer.
    
    IOReturn 	err = kIOReturnSuccess;
    UInt32	i;
    UInt8	readLength;
    UInt32	bufferSize;
    Boolean	reportShortRead = false;
    
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
//	Read a bunch of data via I2C
//
//	The parameters are described as follows:
//
//			-> deviceAddress		device's I2C address
//			-> numberOfBytes		number of bytes to read
//			<- data				the requested number of bytes of data

IOReturn IOFramebuffer::i2cRead(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 numberOfBytes, UInt8 * data)
{
    IOReturn 	err = kIOReturnSuccess;
    int		i;
    
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
// 	i2cWrite()
//	Write a bunch of data via I2C
//
//	The parameters are described as follows:
//
//			-> deviceAddress		device's I2C address
//			-> numberOfBytes		number of bytes to write
//			-> data				the number of bytes of data

IOReturn IOFramebuffer::i2cWrite(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 numberOfBytes, UInt8 * data)
{
    IOReturn 	err = kIOReturnSuccess;
    UInt32	i;
    
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
//	Assume we are reading the DDC data, and display misses a few clocks, we send ack, display misses ack
//	The display might still be holding down the data line. Whenever we get an error, send nine stops.
//	If the display is waiting for a clock before going to the next bit, the stop will be interpreted
//	as a clock. It will go onto the next bit. Whenever it has finished writing the eigth bit, the
//	next stop will look like a stop....the display will release the bus.
//	8 bits, 9 stops. The display should see at least one stop....

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
    const OSSymbol * displayKey;
    OSDictionary *   dict;
    OSObject *       value = 0;

    if (!gIOFBPrefs)
        return (value);
    displayKey = (const OSSymbol *) display->copyProperty(kIODisplayPrefKeyKey);
    if (!displayKey)
        return (value);

    if ((dict = OSDynamicCast(OSDictionary, gIOFBPrefs->getObject(displayKey))))
    {
        value = dict->getObject(key);
        if (value)
            value->retain();
    }

    displayKey->release();

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
            DEBG(thisIndex, " found %s = %d\n", key->getCStringNoCopy(), value[0]);
            found = true;
        }
        pref->release();
    }
    return (found);
}

bool IOFramebuffer::setPreference( IODisplay * display, const OSSymbol * key, OSObject * value )
{
    const OSSymbol * displayKey;
    OSDictionary *   dict;
    OSObject *	     oldValue = 0;
    bool	     madeChanges = false;

    if (!gIOFBPrefs)
        return (false);
    displayKey = (const OSSymbol *) display->copyProperty(kIODisplayPrefKeyKey);
    if (!displayKey)
        return (false);

    dict = OSDynamicCast(OSDictionary, gIOFBPrefs->getObject(displayKey));
    if (!dict)
    {
        dict = OSDictionary::withCapacity(4);
        if (dict)
        {
            gIOFBPrefs->setObject(displayKey, dict);
            dict->release();
            madeChanges = true;
        }
    }
    else if (key)
        oldValue = dict->getObject(key);
        
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
        DEBG(thisIndex, " sched prefs\n");
        gIOFBDelayedPrefsEvent->setTimeoutMS((UInt32) 2000);
    }

    displayKey->release();

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
    DEBG(0, " %s = %d\n", key->getCStringNoCopy(), value);
    return (ok);
}

void IOFramebuffer::getTransformPrefs( IODisplay * display )
{
    UInt32 value;

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
    UInt32			    count = 0;

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
    IOReturn	     ret;
    UInt32	     count = 0;
    UInt32	     str[2];
    const OSSymbol * sym;
    UInt32	     value[16];
    UInt32 *	     attributes;
    OSDictionary *   allParams;
    OSDictionary *   newDict = 0;
    OSDictionary *   oldParams;
    const OSSymbol * key;
    OSIterator *     iter;

    fDisplay = display;
    if (!fDisplay)
	return (false);

    FBLOCK();

    fFramebuffer->setPreference(display, 0, 0);		// register display

    fFramebuffer->getTransformPrefs(display);

    allParams = OSDynamicCast(OSDictionary, display->copyProperty(gIODisplayParametersKey));
    if (allParams)
    {
	newDict = OSDictionary::withDictionary(allParams);
	allParams->release();
    }

    ret = fFramebuffer->getAttributeForConnection(
			    0, kConnectionDisplayParameterCount, &count);
    if (kIOReturnSuccess != ret)
	count = 0;

    DEBG(0, " (%x) count %d\n", ret, count);

    oldParams = fDisplayParams;
    do
    {
	if (count)
	    fDisplayParams = OSDictionary::withCapacity(count);
	else
	    fDisplayParams = 0;
	if (!fDisplayParams)
	    continue;

	attributes = IONew(UInt32, count);
	if (!attributes)
	    continue;
    
	if (kIOReturnSuccess != fFramebuffer->getAttributeForConnection(
					0, kConnectionDisplayParameters, attributes))
	    continue;
    
	str[1] = 0;
	for (UInt32 i = 0; i < count; i++)
	{

	    if (attributes[i] < 0x00ffffff)
		continue;

	    OSWriteBigInt32(str, 0, attributes[i]);
	    sym = OSSymbol::withCString((const char *) str);
	    if (!sym)
		continue;

	    if (kIOReturnSuccess == fFramebuffer->getAttributeForConnection(0, attributes[i], &value[0]))
	    {
                if (gIOFBPrefsParameters && gIOFBPrefsParameters->getObject(sym))
                    fFramebuffer->getIntegerPreference(display, sym, &value[0]);

		IODisplay::addParameter(fDisplayParams, sym, value[1], value[2]);
		IODisplay::setParameter(fDisplayParams, sym, value[0]);
	    }
	    DEBG(0, " [%d] %s = %d, (%d - %d)\n", i, (const char *) str, value[0], value[1], value[2]);
	    sym->release();
	}
    
	IODelete(attributes, UInt32, count);
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

    FBUNLOCK();

    return (true);
}

void IOFramebufferParameterHandler::displayModeChange( void )
{
    if (fDisplay)
	setDisplay( fDisplay );
}

bool IOFramebufferParameterHandler::doIntegerSet( OSDictionary * params,
                               const OSSymbol * paramName, UInt32 value )
{
    UInt32	attribute;
    bool	ok;

    FBLOCK();

    if (fDisplayParams && fDisplayParams->getObject(paramName))
    {

        if (fDisplay && gIOFBPrefsParameters && gIOFBPrefsParameters->getObject(paramName))
            fFramebuffer->setIntegerPreference(fDisplay, paramName, value);

	attribute = OSReadBigInt32(paramName->getCStringNoCopy(), 0);
    
	ok = (kIOReturnSuccess == fFramebuffer->setAttributeForConnection(
					0, attribute, value));
    }
    else
	ok = false;

    FBUNLOCK();

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

	UInt64 id = (((UInt64) (UInt32) fFramebuffer) << 32) | fBusID;
	registerI2C(id);

	ok = true;
    }
    while (false);

    return (ok);
}

IOReturn IOFramebufferI2CInterface::startIO( IOI2CRequest * request )
{
    IOReturn	 	err;

    FBLOCK();
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

    FBUNLOCK();

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if __ppc__

class AppleOnboardI2CInterface : public IOI2CInterface
{
    OSDeclareDefaultStructors(AppleOnboardI2CInterface)

    class PPCI2CInterface * fInterface;
    SInt32		    fPort;

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

#endif	/* __ppc__ */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOFramebufferI2CInterface::create( IOFramebuffer * framebuffer )
{
    IOReturn			err = kIOReturnSuccess;
    IOI2CInterface *		interface;
    UInt32			idx;
    OSArray *			busArray;
    OSArray *			interfaceIDArray;
    OSDictionary *		dict;
    OSObject *			num;
    bool			ok = true;

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

#if __ppc__
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
#endif

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

