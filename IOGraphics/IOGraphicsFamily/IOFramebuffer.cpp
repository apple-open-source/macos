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
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 01 Sep 92	Portions from Joe Pasqua, Created. 
 */


#include <IOKit/IOLib.h>
#include <libkern/c++/OSContainers.h>
#include <libkern/OSByteOrder.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define IOFRAMEBUFFER_PRIVATE
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IODisplay.h>

#include "IOFramebufferUserClient.h"
#include "IODisplayWrangler.h"
#include "IOFramebufferReallyPrivate.h"
#include <IOKit/pwr_mgt/RootDomain.h>

#include <string.h>
#include <IOKit/assert.h>
#include <sys/kdebug.h>

#if IOPM_ROOTDOMAIN_REV < 2
enum {
    kIOPMSetValue		= (1<<16),
    // don't sleep on clamshell closure on a portable with AC connected
    kIOPMSetDesktopMode		= (1<<17),
    // set state of AC adaptor connected
    kIOPMSetACAdaptorConnected	= (1<<18),
};
#else
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#endif

#ifndef kAppleClamshellStateKey
#define kAppleClamshellStateKey	"AppleClamshellState"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//#undef DEBUG
//#define DEBUG 1

#if DOANIO
#include <sys/uio.h>
#include <sys/conf.h>
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOGraphicsDevice

OSDefineMetaClass( IOFramebuffer, IOGraphicsDevice )
OSDefineAbstractStructors( IOFramebuffer, IOGraphicsDevice )

static OSArray *	gAllFramebuffers;
static IOLock *		gIOFBSleepStateLock;
static thread_call_t	gIOFBSleepCallout;

static IONotifier *	gIOFBRootNotifier;
static IOService *	gIOFBSystemPowerAckTo;
static UInt32		gIOFBSystemPowerAckRef;
static bool		gIOFBSystemPower = true;
static bool		gIOFBSleepThread;
static thread_call_t	gIOFBClamshellCallout;
static SInt32		gIOFBClamshellEnable;
static IOOptionBits	gIOFBClamshellState;
static SInt32		gIOFBSuspendCount;
bool			gIOFBDesktopModeAllowed = true;
IOOptionBits		gIOFBLastClamshellState;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define	GetShmem(instance)	((StdFBShmem_t *)(instance->priv))

#define CLEARSEMA(shmem)	ev_unlock(&shmem->cursorSema)
#define SETSEMA(shmem)		\
        if (!ev_try_lock(&shmem->cursorSema)) return;
#define TOUCHBOUNDS(one, two) \
        (((one.minx < two.maxx) && (two.minx < one.maxx)) && \
        ((one.miny < two.maxy) && (two.miny < one.maxy)))

/*
 * Cursor rendering
 */

#include "IOCursorBlits.h"

inline void IOFramebuffer::StdFBDisplayCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *shmem;
    Bounds saveRect;
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
    cursorWidth = shmem->cursorSize[shmem->frame].width;

    cursStart = (saveRect.miny - shmem->cursorRect.miny) * cursorWidth +
                (saveRect.minx - shmem->cursorRect.minx);

    if( inst->cursorBlitProc)
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

    if( inst->cursorRemoveProc)
        inst->cursorRemoveProc( inst, (void *)shmem,
				vramPtr, vramRow, width, height);
}

inline void IOFramebuffer::RemoveCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *	shmem = GetShmem(inst);

    if( shmem->hardwareCursorActive ) {
        Point *		hs;

        hs = &shmem->hotSpot[shmem->frame];
	inst->setCursorState(
		shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
		shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, false );
    } else
        StdFBRemoveCursor(inst);
}

inline void IOFramebuffer::DisplayCursor( IOFramebuffer * inst )
{
    Point 	 *	hs;
    StdFBShmem_t *	shmem = GetShmem(inst);
    SInt32		x, y;

    hs = &shmem->hotSpot[shmem->frame];
    x  = shmem->cursorLoc.x - hs->x;
    y  = shmem->cursorLoc.y - hs->y;

    if( shmem->hardwareCursorActive )
	inst->setCursorState( x - shmem->screenBounds.minx,
				y - shmem->screenBounds.miny, true );
    else {
        shmem->cursorRect.maxx = (shmem->cursorRect.minx = x)
		+ shmem->cursorSize[shmem->frame].width;
        shmem->cursorRect.maxy = (shmem->cursorRect.miny = y)
		+ shmem->cursorSize[shmem->frame].height;
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
    Point *		hs;
    int 		intersect;
    Bounds 		tempRect;
    StdFBShmem_t *	shmem = GetShmem(inst);
    
    /* Calculate temp cursorRect */
    hs = &shmem->hotSpot[shmem->frame];
    tempRect.maxx = (tempRect.minx = (shmem->cursorLoc).x - hs->x)
				   + shmem->cursorSize[shmem->frame].width;
    tempRect.maxy = (tempRect.miny = (shmem->cursorLoc).y - hs->y)
				   + shmem->cursorSize[shmem->frame].height;

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
    IOByteCount			cursorImageBytes;

    rowBytes = info->bytesPerRow;
    totalWidth = (rowBytes * 8) / info->bitsPerPixel;
    bytesPerPixel = info->bitsPerPixel / 8;
    frameBuffer = (volatile unsigned char *) vramMap->getVirtualAddress();

    if( shmem) {
        if( (shmem->screenBounds.maxx == shmem->screenBounds.minx)
         || (shmem->screenBounds.maxy == shmem->screenBounds.miny)) {
            // a default if no one calls IOFBSetBounds()
            shmem->screenBounds.minx = 0;
            shmem->screenBounds.miny = 0;
            shmem->screenBounds.maxx = info->activeWidth;
            shmem->screenBounds.maxy = info->activeHeight;
        }

        cursorImageBytes = maxCursorSize.width * maxCursorSize.height
                            * bytesPerPixel;
        bits = shmem->cursor;
        for( int i = 0; i < kIOFBNumCursorFrames; i++ ) {
            shmem->hardwareCursorFlags[i] = kIOFBCursorImageNew;
            cursorImages[i] = bits;
            bits += cursorImageBytes;
	    shmem->cursorSize[i] = maxCursorSize;
        }
        if( info->bitsPerPixel <= 8) {
            for( int i = 0; i < kIOFBNumCursorFrames; i++ ) {
                cursorMasks[i] = bits;
                bits += cursorImageBytes;
            }
        }
        cursorSave = bits;
    }

    switch( info->bitsPerPixel) {
        case 8:
            if( colorConvert.t._bm256To38SampleTable
             && colorConvert.t._bm38To256SampleTable) {
                cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor8P;
                cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor8;
            }
            break;
        case 16:
            if( colorConvert.t._bm34To35SampleTable
             && colorConvert.t._bm35To34SampleTable) {
                cursorBlitProc = (CursorBlitProc) StdFBDisplayCursor555;
                cursorRemoveProc = (CursorRemoveProc) StdFBRemoveCursor16;
            }
            break;
        case 32:
            if( colorConvert.t._bm256To38SampleTable
             && colorConvert.t._bm38To256SampleTable) {
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

IOReturn IOFramebuffer::createSharedCursor(
		int shmemVersion, int maxWidth, int maxHeight )
{
    StdFBShmem_t *		shmem;
    IOByteCount			size, maxImageSize;

    kprintf("createSharedCursor vers = %d, %d x %d\n",
	shmemVersion, maxWidth, maxHeight);

    if( shmemVersion != kIOFBCurrentShmemVersion)
	return( kIOReturnUnsupported);

    shmemClientVersion = shmemVersion;
    maxImageSize = (maxWidth * maxHeight * kIOFBMaxCursorDepth) / 8;

    size = sizeof( StdFBShmem_t)
	 + ((kIOFBNumCursorFrames + 1) * maxImageSize);

    if( !sharedCursor || (size != sharedCursor->getLength())) {
        IOBufferMemoryDescriptor * newDesc;

        priv = 0;
        newDesc = IOBufferMemoryDescriptor::withOptions(
                kIODirectionNone | kIOMemoryKernelUserShared, size );
        if( !newDesc)
            return( kIOReturnNoMemory );
    
        if( sharedCursor)
            sharedCursor->release();
        sharedCursor = newDesc;
    }
    shmem = (StdFBShmem_t *) sharedCursor->getBytesNoCopy();
    priv = shmem;

    // Init shared memory area
    bzero( shmem, size );
    shmem->version = kIOFBCurrentShmemVersion;
    shmem->structSize = size;
    shmem->cursorShow = 1;
    shmem->hardwareCursorCapable = haveHWCursor;
    for( int i = 0; i < kIOFBNumCursorFrames; i++)
        shmem->hardwareCursorFlags[i] = kIOFBCursorImageNew;

    maxCursorSize.width = maxWidth;
    maxCursorSize.height = maxHeight;

    doSetup( false );

    return( kIOReturnSuccess);
}

IOReturn IOFramebuffer::setBoundingRect( Bounds * bounds )
{
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if( NULL == shmem)
	return( kIOReturnUnsupported);

    shmem->screenBounds = *bounds;

    return( kIOReturnSuccess);
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

    switch( type ) {

        case kIOFBServerConnectType:
            if( serverConnect)
                err = kIOReturnExclusiveAccess;
            else {

                if( isConsoleDevice())
                    getPlatform()->setConsoleInfo( 0, kPEReleaseScreen);

                err = open();
		if( kIOReturnSuccess == err)
                    newConnect = IOFramebufferUserClient::withTask(owningTask);
	    }
	    break;

        case kIOFBSharedConnectType:
            if( sharedConnect) {
                theConnect = sharedConnect;
                theConnect->retain();
            } else if( serverConnect)
                newConnect = IOFramebufferSharedUserClient::withTask(owningTask);
            else
                err = kIOReturnNotOpen;
	    break;

	default:
	    err = kIOReturnBadArgument;
    }

    if( newConnect) {
	if( (false == newConnect->attach( this ))
         || (false == newConnect->start( this ))) {
            newConnect->detach( this );
            newConnect->release();
        } else
            theConnect = newConnect;
    }

    *handler = theConnect;
    return( err );
}

IOReturn IOFramebuffer::extGetDisplayModeCount( IOItemCount * count )
{
    *count = getDisplayModeCount();
    return( kIOReturnSuccess);
}

IOReturn IOFramebuffer::extGetDisplayModes( IODisplayModeID * allModes, IOByteCount * size )
{
    IOReturn		err;
    IOByteCount		outSize;

    outSize = getDisplayModeCount() * sizeof( IODisplayModeID);

    if( *size < outSize)
	return( kIOReturnBadArgument);

    *size = outSize;
    err = getDisplayModes( allModes );

    return( err);
}

IOReturn IOFramebuffer::extGetVRAMMapOffset( IOPixelAperture /* aperture */, 
					IOByteCount * offset )
{
    *offset = vramMapOffset;

    return( kIOReturnSuccess );
}

IOReturn IOFramebuffer::extSetBounds( Bounds * bounds )
{
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if( shmem)
        shmem->screenBounds = *bounds;

    return( kIOReturnSuccess );
}

IOReturn IOFramebuffer::extValidateDetailedTiming(
                void * description, void * outDescription,
                IOByteCount inSize, IOByteCount * outSize )
{
    IOReturn	err;

    if( *outSize != inSize)
        return( kIOReturnBadArgument );

    err = validateDetailedTiming( description, inSize );

    if( kIOReturnSuccess == err)
        bcopy( description, outDescription, inSize );

    return( err );
}


IOReturn IOFramebuffer::extSetColorConvertTable( UInt32 select,
                                                 UInt8 * data, IOByteCount length )
{
    static const IOByteCount checkLength[] = {
        16 * sizeof( UInt8),
        32 * sizeof( UInt8),
        256 * sizeof( UInt32),
        5 * 256 * sizeof( UInt8) };

    UInt8 *		table;
    IODisplayModeID	mode;
    IOIndex		depth;
    IOPixelInformation	info;

    if( select > 3)
        return( kIOReturnBadArgument );

    if( length != checkLength[select])
        return( kIOReturnBadArgument );

    table = colorConvert.tables[select];
    if( 0 == table) {
        table = (UInt8 *) IOMalloc( length );
        colorConvert.tables[select] = table;
    }
    if( !table)
        return( kIOReturnNoMemory );

    bcopy( data, table, length );
    if( select == 3)
        white = data[data[255] + data[511] + data[767] + 1024];

    if( (NULL == cursorBlitProc)
      && colorConvert.tables[0] && colorConvert.tables[1]
      && colorConvert.tables[2] && colorConvert.tables[3]
      && vramMap
      && (kIOReturnSuccess == getCurrentDisplayMode( &mode, &depth ))
      && (kIOReturnSuccess == getPixelInformation( mode, depth, kIOFBSystemAperture, &info )))
        setupCursor( &info );

    return( kIOReturnSuccess );
}

IOReturn IOFramebuffer::extSetCLUTWithEntries( UInt32 index, IOOptionBits options,
                                        IOColorEntry * colors, IOByteCount inputCount )
{
    IOReturn	kr;

    kr = setCLUTWithEntries( colors, index,
                            inputCount / sizeof( IOColorEntry),
                            options );

    return( kr );
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
    CLEARSEMA(shmem);
}

#if 0
void IOFramebuffer::free()
{
    if( vblSemaphore)
        semaphore_destroy(kernel_task, vblSemaphore);
    super::free();
}
#endif

void IOFramebuffer::deferredMoveCursor( IOFramebuffer * inst )
{
    StdFBShmem_t *	shmem = GetShmem(inst);
    IOReturn		err = kIOReturnSuccess;

    if( shmem->hardwareCursorActive) {

        if (shmem->cursorObscured) {
            shmem->cursorObscured = 0;
            if (shmem->cursorShow)
                --shmem->cursorShow;
        }
        if (shmem->hardwareCursorShields && shmem->shieldFlag) CheckShield(inst);
	if (!shmem->cursorShow) {
            Point * hs;
            hs = &shmem->hotSpot[shmem->frame];
            err = inst->setCursorState(
		shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
		shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny, true );
	}

    } else {

        if (!shmem->cursorShow++)
            RemoveCursor(inst);
        if (shmem->cursorObscured) {
            shmem->cursorObscured = 0;
            if (shmem->cursorShow)
                --shmem->cursorShow;
        }
        if (shmem->shieldFlag) CheckShield(inst);
        if (shmem->cursorShow)
            if (!--shmem->cursorShow)
                DisplayCursor(inst);

	inst->flushCursor();
    }
    inst->needCursorService = (kIOReturnBusy == err);
}

void IOFramebuffer::moveCursor( Point * cursorLoc, int frame )
{
    nextCursorLoc = *cursorLoc;
    nextCursorFrame = frame;
    needCursorService  = true;

    StdFBShmem_t *shmem = GetShmem(this);

    SETSEMA(shmem);

    if( frame != shmem->frame) {

        if( pagingState && shmem->hardwareCursorFlags[frame]) {
            hwCursorLoaded = (kIOReturnSuccess == setCursorImage( (void *) frame ));
            shmem->hardwareCursorFlags[frame] = hwCursorLoaded ? kIOFBCursorHWCapable : 0;
        } else
            hwCursorLoaded = false;

        shmem->frame = frame;
        if( shmem->hardwareCursorActive != hwCursorLoaded) {
            SysHideCursor( this );
            shmem->hardwareCursorActive = hwCursorLoaded;
            if (shmem->shieldFlag
             && ((false == shmem->hardwareCursorActive) || (shmem->hardwareCursorShields)))
                CheckShield(this);
            SysShowCursor( this );
        }
    }

    if( !haveVBLService) {
        shmem->cursorLoc = *cursorLoc;
        shmem->frame = frame;
        deferredMoveCursor( this );
    }

    CLEARSEMA(shmem);
}

void IOFramebuffer::handleVBL( IOFramebuffer * inst, void * ref )
{
    StdFBShmem_t *	shmem = GetShmem(inst);
    AbsoluteTime	now;

    if( !shmem)
        return;

    clock_get_uptime( &now );
    shmem->vblDelta = now;
    SUB_ABSOLUTETIME( &shmem->vblDelta, &shmem->vblTime );
    shmem->vblTime = now;

    KERNEL_DEBUG(0xc000030 | DBG_FUNC_NONE,
        shmem->vblDelta.hi, shmem->vblDelta.lo, 0, 0, 0);

    if( inst->vblSemaphore)
        semaphore_signal_all(inst->vblSemaphore);
    
    SETSEMA(shmem);

    if( inst->needCursorService) {
        shmem->cursorLoc = inst->nextCursorLoc;
        shmem->frame = inst->nextCursorFrame;
        deferredMoveCursor( inst );
    }

    CLEARSEMA(shmem);
}

void IOFramebuffer::showCursor( Point * cursorLoc, int frame )
{
    StdFBShmem_t *shmem;
    
    shmem = GetShmem(this);
    SETSEMA(shmem);

    if( frame != shmem->frame) {
        if( shmem->hardwareCursorFlags[frame]) {
            hwCursorLoaded = (kIOReturnSuccess == setCursorImage( (void *) frame ));
            shmem->hardwareCursorFlags[frame] = hwCursorLoaded ? kIOFBCursorHWCapable : 0;
        } else
            hwCursorLoaded = false;
        shmem->frame = frame;
    }

    shmem->hardwareCursorActive = hwCursorLoaded;
    shmem->cursorLoc = *cursorLoc;
    if (shmem->shieldFlag
      && ((false == shmem->hardwareCursorActive) || (shmem->hardwareCursorShields)))
        CheckShield(this);

    SysShowCursor(this);
    CLEARSEMA(shmem);
}

void IOFramebuffer::resetCursor( void )
{
    StdFBShmem_t *	shmem;
    int			frame;
    
    shmem = GetShmem(this);
    hwCursorLoaded = false;
    if( !shmem)
        return;

    shmem->hardwareCursorActive = false;
    frame = shmem->frame;
    shmem->frame = frame ^ kIOFBNumCursorFrames;
    showCursor( &shmem->cursorLoc, frame );
}

void IOFramebuffer::getVBLTime( AbsoluteTime * time, AbsoluteTime * delta ) 
{
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if( shmem) {
        *time = shmem->vblTime;
        *delta = shmem->vblDelta;
    } else
        time->hi = time->lo = 0;
}

void IOFramebuffer::getBoundingRect( Bounds ** bounds )
{
    StdFBShmem_t *shmem;

    shmem = GetShmem(this);
    if( NULL == shmem)
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

    if( interruptType != kIOFBVBLInterruptType)
        return( kIOReturnUnsupported );

    if( !haveVBLService)
        return( kIOReturnNoResources );

    if( MACH_PORT_NULL == vblSemaphore) {
        kr = semaphore_create(kernel_task, &sema, SYNC_POLICY_FIFO, 0);
        if( kr == KERN_SUCCESS)
            vblSemaphore = sema;
    } else
        kr = KERN_SUCCESS;

    if( kr == KERN_SUCCESS)
        *semaphore = vblSemaphore;

    return( kr );
}

IOReturn IOFramebuffer::extSetCursorVisible( bool visible )
{
    IOReturn		err;
    Point *		hs;
    StdFBShmem_t *	shmem = GetShmem(this);

    if( shmem->hardwareCursorActive ) {
        hs = &shmem->hotSpot[shmem->frame];
	err = setCursorState(
		shmem->cursorLoc.x - hs->x - shmem->screenBounds.minx,
		shmem->cursorLoc.y - hs->y - shmem->screenBounds.miny,
		visible );
    } else
	err = kIOReturnBadArgument;

    return( err );
}

IOReturn IOFramebuffer::extSetCursorPosition( SInt32 x, SInt32 y )
{
    return( kIOReturnUnsupported );
}

IOReturn IOFramebuffer::extSetNewCursor( void * cursor, IOIndex frame,
					IOOptionBits options )
{
    StdFBShmem_t *	shmem = GetShmem(this);
    IOReturn		err;

    // assumes called with cursorSema held
    if( cursor || options || (frame >= kIOFBNumCursorFrames))
	err = kIOReturnBadArgument;
    else {

	if( (shmem->cursorSize[frame].width > maxCursorSize.width)
	 || (shmem->cursorSize[frame].height > maxCursorSize.height))
            err = kIOReturnBadArgument;

	else if( haveHWCursor) {

            if( frame == shmem->frame) {
                err = setCursorImage( (void *) frame );
                hwCursorLoaded = (kIOReturnSuccess == err);
                shmem->hardwareCursorActive = hwCursorLoaded;
                shmem->hardwareCursorFlags[frame] = hwCursorLoaded ? kIOFBCursorHWCapable : 0;
            } else {
                shmem->hardwareCursorFlags[frame] = kIOFBCursorImageNew;
                err = kIOReturnSuccess;		// I guess
            }

        } else
            err = kIOReturnUnsupported;
    }
    
    return( err );
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
    SInt32 			x, y;
    UInt32			index, numColors = 0;
    UInt32			alpha, red, green, blue;
    UInt16			s16;
    UInt32			s32;
    UInt32			pixel = 0;
    UInt8			data = 0;
    bool			ok = true;
    bool			isDirect;

    assert( frame < kIOFBNumCursorFrames );

    if( bytesPerPixel == 4) {
        cursPtr32 = (volatile unsigned int *) cursorImages[ frame ];
        cursPtr16 = 0;
    } else if( bytesPerPixel == 2) {
        cursPtr32 = 0;
        cursPtr16 = (volatile unsigned short *) cursorImages[ frame ];
    } else
	return( false );

    x = shmem->cursorSize[frame].width;
    y = shmem->cursorSize[frame].height;

    if( (x > (SInt32) hwDesc->width) || (y > (SInt32) hwDesc->height))
	return( false );
    isDirect = (hwDesc->bitDepth > 8);
    if(isDirect && (hwDesc->bitDepth != 32))
        return( false);

#if 0
    hwCursorInfo->cursorWidth = x;
    hwCursorInfo->cursorHeight = y;
    while( (--y != -1) ) {
        x = shmem->cursorSize[frame].width;
        while( (--x != -1) ) {

	    if( cursPtr32) {
		s32 = *(cursPtr32++);
		alpha = (s32 >> 28) & 0xf;
                if( alpha && (alpha != 0xf))
                    *(cursPtr32 - 1) = 0x00ffffff;

	    } else {
		s16 = *(cursPtr16++);
		alpha = s16 & 0x000F;
                if( alpha && (alpha != 0xf))
                    *(cursPtr16 - 1) = 0xfff0;
            }
        }
    }
#endif

    hwCursorInfo->cursorWidth = x;
    hwCursorInfo->cursorHeight = y;

    while( ok && (--y != -1) ) {
        x = shmem->cursorSize[frame].width;
        while( ok && (--x != -1) ) {

	    if( cursPtr32) {
		s32 = *(cursPtr32++);
		alpha = (s32 >> 28) & 0xf;
		red = (s32 >> 16) & 0xff;
		red |= (red << 8);
		green = (s32 >> 8) & 0xff;
		green |= (green << 8);
		blue = (s32) & 0xff;
		blue |= (blue << 8);

	    } else {
#define RMASK16	0xF000
#define GMASK16	0x0F00
#define BMASK16	0x00F0
#define AMASK16	0x000F
		s16 = *(cursPtr16++);
		alpha = s16 & AMASK16;
                red = s16 & RMASK16;
		red |= (red >> 4) | (red >> 8) | (red >> 12);
		green = s16 & GMASK16;
		green |= (green << 4) | (green >> 4) | (green >> 8);
		blue = s16 & BMASK16;
		blue |= (blue << 8) | (blue << 4) | (blue >> 4);
	    }

            if( alpha == 0 ) {

                if( 0 == (red | green | blue)) {
                    /* Transparent black area.  Leave dst as is. */
                    if( kTransparentEncodedPixel
                            & hwDesc->supportedSpecialEncodings)
                        pixel = hwDesc->specialEncodings[kTransparentEncoding];
                    else
                        ok = false;
                } else if (0xffff == (red & green & blue)) {
                    /* Transparent white area.  Invert dst. */
                    if( kInvertingEncodedPixel
                            & hwDesc->supportedSpecialEncodings)
                        pixel = hwDesc->specialEncodings[kInvertingEncoding];
                    else
                        ok = false;
                } else
                    ok = false;

            } else if( isDirect) {
                pixel = (alpha << 24)
                      | (alpha << 28)
                      | ((red & 0xff00) << 8)
                      | (green & 0xff00)
                      | ((blue & 0xff00) >> 8);
            } else {
                /* Indexed pixels */
                if( alpha == 0xf ) {

                    /* Opaque cursor pixel.  Mark it. */
                    for( index = 0; index < numColors; index++ ) {
                        if( (red   == clut[ index ].red)
                        && (green == clut[ index ].green)
                        && (blue  == clut[ index ].blue) ) {
    
                            pixel = clut[ index ].index;
                            break;
                        }
                    }
                    if( index == numColors) {
                        ok = (numColors < maxColors);
                        if( ok) {
                            pixel = hwDesc->colorEncodings[ numColors++ ];
                            clut[ index ].red   = red;
                            clut[ index ].green = green;
                            clut[ index ].blue  = blue;
                            clut[ index ].index = pixel;
                        }
                    }
    
                } else {
                    /* Alpha is not 0 or 1.0.  Sover the cursor. */
                    ok = false;
                    break;
                }
            }

            if( hwDesc->bitDepth <= 8) {
                data <<= hwDesc->bitDepth;
                data |= pixel;
                if( 0 == (x & ((8 / hwDesc->bitDepth) - 1)))
                    *dataOut++ = data;
            } else
                *((UInt32 *)dataOut)++ = pixel;

	} /* x */
    } /* y */

//    if( !ok)	kprintf("Couldnt do a hw curs\n");

    return( ok );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::initialize()
{
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if 0
static bool serializeInfoCB( void * target, void * ref, OSSerialize * s )
{
    return( ((IOFramebuffer *)target)->serializeInfo(s) );
}
#endif


bool IOFramebuffer::start( IOService * provider )
{

    if( ! super::start( provider))
	return( false );

    userAccessRanges = OSArray::withCapacity( 1 );
    if( !userAccessRanges)
	return( false );
    serverMsg = IOMalloc( sizeof (mach_msg_header_t) );
    if( !serverMsg)
	return( false );
    bzero( serverMsg, sizeof (mach_msg_header_t));

#if 0
    OSSerializer * infoSerializer = OSSerializer::forTarget( (void *) this, &serializeInfoCB );
    if( !infoSerializer)
	return( false );

    setProperty( kIOFramebufferInfoKey, infoSerializer );
    infoSerializer->release();
#endif

    // initialize superclass power management variables
    PMinit();
    // attach into the power management hierarchy
    provider->joinPMtree(this);

    registerService();

    return( true );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Apple standard 8-bit CLUT

#if 1
extern UInt8 appleClut8[ 256 * 3 ];
#else
UInt8 appleClut8[ 256 * 3 ] = {

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
extern "C" { int kmputc( int c ); }

#if DOANIO

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

    err = ((*cdevsw[major(device)].d_read)(device, &uio, 0));
}

#endif

IOReturn IOFramebuffer::handleEvent( IOIndex event, void * info = 0 )
{
    IOReturn ret;

#if DEBUG
    IOLog("FBEV(%p:%ld, %d)\n", this, event, pagingState);
#endif

    switch( event ) {

      case kIOFBNotifyWillPowerOff:
        hideCursor();
        if( isConsoleDevice())
            killprint = 1;

        if( pagingState) {
            pagingState = false;
            deliverFramebufferNotification( kIOFBNotifyWillSleep, info );
        }

        ret = deliverFramebufferNotification( event, info );
        configPending = true;
        break;

      case kIOFBNotifyDidPowerOn:

        if( isConsoleDevice()) {
            killprint = 0;
            kmputc( 033 );
            kmputc( 'c' );
        }
        ret = deliverFramebufferNotification( event, info );

        if( !pagingState && gIOFBSystemPower) {
            pagingState = true;
            deliverFramebufferNotification( kIOFBNotifyDidWake, info );
        }

#if DOANIO
        IOLog("FBIO(%p)\n", this);
        doanio();
        IOLog("FBIO(%p:%08lx)\n", this, doaniobuf[0]);
#endif

        resetCursor();
        configPending = false;
//        checkConnectionChange();
        break;

      case kIOFBNotifyWillSleep:

#if DOANIO
        IOLog("FBIO(%p)\n", this);
        doanio();
        IOLog("FBIO(%p:%08lx)\n", this, doaniobuf[0]);
#endif

      default:
        ret = deliverFramebufferNotification( event, info );
    }

    return( ret );
}

IOReturn IOFramebuffer::notifyServer( UInt8 state )
{
    mach_msg_header_t * msgh = (mach_msg_header_t *) serverMsg;
    IOReturn		err = kIOReturnSuccess;

    if( serverNotified != state) {
        serverNotified = state;
#if DEBUG
        IOLog("FB(%p:%p, %d->%d, %d)\n", this, msgh->msgh_remote_port,
                serverState, serverNotified, serverPendingAck);
#endif
        msgh->msgh_id = state;
        if( (MACH_PORT_NULL == msgh->msgh_remote_port)
         || (KERN_SUCCESS != mach_msg_send_from_kernel( msgh, msgh->msgh_size ))) {
            serverState = serverNotified;
        }
    }

    return( err );
}

bool IOFramebuffer::getIsUsable( void )
{
    return( 0 != isUsable );
}

void IOFramebuffer::notifyServerAll( UInt8 state )
{
    unsigned int	index;
    IOFramebuffer *	fb;
    bool		doNotify = true;

    IOLockLock( gIOFBSleepStateLock );

    if( state) {
        doNotify = gIOFBSystemPower;
        for( index = 0;
            doNotify && (fb = (IOFramebuffer *) gAllFramebuffers->getObject( index));
            index++) {
            doNotify = fb->getIsUsable();
        }
        if( doNotify)
            IOSleep(20);
    }

    if( doNotify) {
        for( index = 0;
          (fb = (IOFramebuffer *) gAllFramebuffers->getObject( index));
          index++) {
            fb->notifyServer( state );
        }
    }

    IOLockUnlock( gIOFBSleepStateLock );
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

#if DEBUG
    IOLog("IOFB::sleepWork()\n");
#endif

    IOLockLock( gIOFBSleepStateLock );

    do {
        for( index = 0, allState = 0;
            (fb = (IOFramebuffer *) gAllFramebuffers->getObject( index));
            index++) {

            allState |= fb->checkPowerWork();
        }

        if( (0 == (kIOFBPaging & allState)) && gIOFBSystemPowerAckTo) {
#if DEBUG
            IOLog("IOFBAckRoot(%ld)\n", gIOFBSystemPowerAckRef);
#endif

            IOService * ackTo = gIOFBSystemPowerAckTo;
            UInt32      ackRef = gIOFBSystemPowerAckRef;
            gIOFBSystemPowerAckTo = 0;
            IOLockUnlock( gIOFBSleepStateLock );

            ackTo->allowPowerChange( ackRef );

            IOLockLock( gIOFBSleepStateLock );
            allState |= kIOFBDidWork;
        }

    } while( kIOFBDidWork & allState );

    gIOFBSleepThread = false;

    IOLockUnlock( gIOFBSleepStateLock );
}

IOOptionBits IOFramebuffer::checkPowerWork( void )
{
    UInt32	 newState;
    IOOptionBits ourState = kIOFBPaging;

#if DEBUG
    IOLog("IOFB::checkPowerWork(%p, %d, %d)\n", this,
                gIOFBSystemPower, pendingPowerChange);
#endif

    if( !gIOFBSystemPower) {

        notifyServer( false );
    
        if( !serverState) {
            if( pagingState) {
                pagingState = false;
                IOLockUnlock( gIOFBSleepStateLock );

                handleEvent( kIOFBNotifyWillSleep );

                IOLockLock( gIOFBSleepStateLock );
                ourState |= kIOFBDidWork;
            }
            ourState &= ~kIOFBPaging;
        }
    }

    if( pendingPowerChange) {

        pendingPowerChange = false;
        newState = pendingPowerState;

        IOLockUnlock( gIOFBSleepStateLock );

#if DEBUG
        IOLog("kIOPowerAttribute(%p, ->%ld)\n", this, newState);
#endif
        setAttribute( kIOPowerAttribute, newState );

#if DEBUG
        IOLog("acknowledgeSetPowerState(%p)\n", this);
#endif
        acknowledgeSetPowerState();

        IOLockLock( gIOFBSleepStateLock );
        ourState |= kIOFBDidWork;
    }

    return( ourState );
}

static void startThread( void )
{
    if( !gIOFBSleepThread) {
        gIOFBSleepThread = true;
//      IOCreateThread( &IOFramebuffer::sleepWork, 0 );
        thread_call_enter1( gIOFBSleepCallout, (thread_call_param_t) 0);
    }
}

IOReturn IOFramebuffer::setPowerState( unsigned long powerStateOrdinal,
						IOService * whichDevice )
{
    bool now;

#if DEBUG
    IOLog("IOFB::setPowerState(%p, ->%ld)\n", this, powerStateOrdinal);
#endif

    IOLockLock( gIOFBSleepStateLock );

    pendingPowerState = powerStateOrdinal;

    now = (0xffffffff == gAllFramebuffers->getNextIndexOfObject( this, 0 ));

    if( !now) {
        pendingPowerChange = true;
        startThread();
    }

    IOLockUnlock( gIOFBSleepStateLock );

    if( now)
        setAttribute( kIOPowerAttribute, powerStateOrdinal );

    return( now ? 0 : 45 * 1000 * 1000 );
}

IOReturn IOFramebuffer::powerStateWillChangeTo( IOPMPowerFlags flags,
                                                unsigned long state, IOService * whatDevice )
{
    IOReturn ret;

#if DEBUG
    IOLog("IOFB::powerStateWillChangeTo(%p, ->%08lx)\n", this, flags);
#endif

    if( state && !pm_vars->myCurrentState) {
	gIOFBSystemPower = true;
        sleepConnectCheck = true;
        gIOFBLastClamshellState = 0;
    }

    if( IOPMDeviceUsable & flags)
        return( kIOReturnSuccess );

    notifyServerAll( false );

    IOLockLock( gIOFBSleepStateLock );

    if( serverState != serverNotified) {
        // server will ack within ten seconds
        serverPendingAck = true;
        ret = 10 * 1000 * 1000;
    } else
        ret = IOPMAckImplied;

    IOLockUnlock( gIOFBSleepStateLock );

    return( ret );
}

IOReturn IOFramebuffer::powerStateDidChangeTo( IOPMPowerFlags flags,
                                                unsigned long, IOService* whatDevice )
{
#if DEBUG
    IOLog("IOFB::powerStateDidChangeTo(%p, ->%08lx)\n", this, flags);
#endif

    isUsable = (0 != (IOPMDeviceUsable & flags));

    serverState = serverNotified;

    if( !isUsable)
        return kIOReturnSuccess;

    notifyServerAll( true );

    return( kIOReturnSuccess );
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

    IOLockLock( gIOFBSleepStateLock );
    gIOFBClamshellEnable += delta;
    notSuspended = gIOFBSystemPower && (0 == gIOFBSuspendCount);
    desktopMode = notSuspended && gIOFBDesktopModeAllowed && (gIOFBClamshellEnable <= 0);
    IOLockUnlock( gIOFBSleepStateLock );

    if( delta < 0)
        change = kIOPMDisableClamshell;
    else if( notSuspended)
        change = kIOPMEnableClamshell | kIOPMSetDesktopMode | (desktopMode ? kIOPMSetValue : 0);
    else
        return;

    gIOFBClamshellState = change;
    getPMRootDomain()->receivePowerNotification( change );

    if( (kIOPMEnableClamshell & change)
     && (state = getPMRootDomain()->getProperty(kAppleClamshellStateKey))) {
        publishResource(kAppleClamshellStateKey, gIOFBLastClamshellState ? kOSBooleanTrue : kOSBooleanFalse );
    }
}

IOOptionBits IOFramebuffer::clamshellState( void )
{
    return( gIOFBClamshellState );
}

IOReturn IOFramebuffer::systemPowerChange( void * target, void * refCon,
                                    UInt32 messageType, IOService * service,
                                    void * messageArgument, vm_size_t argSize )
{
    IOReturn ret;
    IOPowerStateChangeNotification * params = (IOPowerStateChangeNotification *) messageArgument;

#if DEBUG
    IOLog("IOFramebuffer::systemPowerChange(%08lx, %ld)\n",
                messageType, (UInt32) params->powerRef);
#endif

    switch (messageType) {

        case kIOMessageSystemWillSleep:

            gIOFBClamshellState = kIOPMDisableClamshell;
            getPMRootDomain()->receivePowerNotification( kIOPMDisableClamshell );

            IOLockLock( gIOFBSleepStateLock );

            gIOFBSystemPower       = false;
            gIOFBSystemPowerAckRef = (UInt32) params->powerRef;
            gIOFBSystemPowerAckTo  = service;

            startThread();

            IOLockUnlock( gIOFBSleepStateLock );

            // We will ack within 20 seconds
            params->returnValue = 20 * 1000 * 1000;
            ret 		= kIOReturnSuccess;
            break;

        case kIOMessageSystemHasPoweredOn:
            params->returnValue = 0;
            ret 		= kIOReturnSuccess;
            break;

        default:
            ret = kIOReturnUnsupported;
            break;
    }

    return( ret );
}


IOReturn
IOFramebuffer::extAcknowledgeNotification( void )
{
    bool needConnectCheck, needAck;

    IOLockLock( gIOFBSleepStateLock );

#if DEBUG
    IOLog("FBACK(%p, %d->%d, %d)\n", this,
            serverState, serverNotified, serverPendingAck);
#endif

    needConnectCheck = (serverState != serverNotified);
    serverState = serverNotified;

    needAck = serverPendingAck;
    serverPendingAck = false;

    startThread();

    IOLockUnlock( gIOFBSleepStateLock );

    if( needConnectCheck) 
        checkConnectionChange();

    if( needAck)
        acknowledgePowerChange(this);

    return( kIOReturnSuccess );
}

IOReturn IOFramebuffer::extRegisterNotificationPort(
                mach_port_t 	port,
                UInt32		type,
                UInt32		refCon )
{
    mach_msg_header_t * msgh;

    IOLockLock( gIOFBSleepStateLock );

    msgh = (mach_msg_header_t *) serverMsg;
    bzero( msgh, sizeof(mach_msg_header_t) );

    msgh->msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msgh->msgh_size        = sizeof(mach_msg_header_t);
    msgh->msgh_remote_port = port;
    
    serverNotified   = true;
    serverState      = true;
    serverPendingAck = false;

    IOLockUnlock( gIOFBSleepStateLock );

    return( kIOReturnSuccess);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOFramebuffer::connectChangeInterrupt( IOFramebuffer * inst, void * ref )
{
    OSIncrementAtomic( &inst->connectChange);

    if( 0 == inst->configPending)
        inst->deferredEvents->interruptOccurred(0, 0, 0);
}

void IOFramebuffer::deferredInterrupt( OSObject * owner,
                                        IOInterruptEventSource * evtSrc, int intCount )
{
    IOFramebuffer * self = (IOFramebuffer *) owner;

    self->checkConnectionChange();
}

void IOFramebuffer::postConnectionChange( void )
{
    bool message;

    IOLockLock( gIOFBSleepStateLock );
    message = (suspended && !messaged);
    if( message)
        messaged = true;
    IOLockUnlock( gIOFBSleepStateLock );

    if( message)
        messageClients( kIOMessageServiceIsSuspended, (void *) true );
}

void IOFramebuffer::checkConnectionChange( bool message )
{
    bool nowSuspended;

    if( connectChange && (sleepConnectCheck || !captured)) {
        IOLockLock( gIOFBSleepStateLock );
        nowSuspended = !suspended;
        if( nowSuspended) {
            suspended = true;
            messaged = message;
            gIOFBSuspendCount++;
#if DEBUG
            IOLog("%s: susp\n", getProvider()->getName());
#endif
        }
        IOLockUnlock( gIOFBSleepStateLock );

        if( message) {
            IOFramebuffer * next = this;
            while( (next = next->getNextDependent()) && (next != this)) {
                next->checkConnectionChange(false);
            }
        }

        if( nowSuspended && message)
            messageClients( kIOMessageServiceIsSuspended, (void *) true );
#if DEBUG
        else
            IOLog("%s: spurious\n", getProvider()->getName());
#endif
    }
    sleepConnectCheck = false;
    clamshellEnable(0);
}

IOReturn IOFramebuffer::open( void )
{
    IOReturn		err = kIOReturnSuccess;
    UInt32		value;
    void *		vblInterrupt;
    void *		connectInterrupt;
    IOFramebuffer *	next;
    bool		firstOpen;

    do {
	if( opened)
	    continue;
	if( dead) {
            err = kIOReturnNotOpen;
	    continue;
        }
        if( !gAllFramebuffers) {
            gAllFramebuffers = OSArray::withCapacity(1);

            IORegistryEntry   * root;
            OSData *	        data = 0;
            if( (root = IORegistryEntry::fromPath("/", gIOServicePlane))) {
                data = OSDynamicCast(OSData, root->getProperty("graphic-options"));
                root->release();
            }
            gIOFBDesktopModeAllowed = !data || (0 != (8 & *((UInt32 *) data->getBytesNoCopy())));
        }

        if( !gAllFramebuffers)
	    continue;
        if( !gIOFBSleepStateLock)
            gIOFBSleepStateLock = IOLockAlloc();
        if( !gIOFBSleepStateLock)
	    continue;
        if( !gIOFBRootNotifier)
	    gIOFBRootNotifier = getPMRootDomain()->registerInterest(
                                    gIOPriorityPowerStateInterest, &systemPowerChange, 0, 0 );
        if( !gIOFBRootNotifier)
	    continue;
        if( !gIOFBSleepCallout)
            gIOFBSleepCallout = thread_call_allocate( (thread_call_func_t)&sleepWork,
                                                            (thread_call_param_t) 0);
        if( !gIOFBSleepCallout)
	    continue;
        if( !gIOFBClamshellCallout)
            gIOFBClamshellCallout = thread_call_allocate( &clamshellWork,
                                                            (thread_call_param_t) 0);
        if( !gIOFBClamshellCallout)
	    continue;

        // tell the console if it's on this display, it's going away
	if( isConsoleDevice())
            getPlatform()->setConsoleInfo( 0, kPEDisableScreen);

        deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );

	err = enableController();
	if( kIOReturnSuccess != err) {
            dead = true;
            if( nextDependent) {
                nextDependent->setNextDependent( NULL );
                nextDependent = NULL;
            }
            deliverFramebufferNotification( kIOFBNotifyDisplayModeDidChange );
	    continue;
	}

        IOLockLock( gIOFBSleepStateLock );
        pagingState = true;
        gAllFramebuffers->setObject(this);
        IOLockUnlock( gIOFBSleepStateLock );

	err = registerForInterruptType( kIOFBVBLInterruptType, 
			(IOFBInterruptProc) &handleVBL, 
			this, priv, &vblInterrupt );
	haveVBLService = (err == kIOReturnSuccess );

        deferredEvents = IOInterruptEventSource::interruptEventSource(this, deferredInterrupt);
        if( deferredEvents)
            getWorkLoop()->addEventSource(deferredEvents);

	err = registerForInterruptType( kIOFBConnectInterruptType, 
			(IOFBInterruptProc) &connectChangeInterrupt,
			this, priv, &connectInterrupt );

	err = getAttribute( kIOHardwareCursorAttribute, &value );
	haveHWCursor = ((err == kIOReturnSuccess) && value);

        OSNumber * num = OSDynamicCast( OSNumber, getProperty(kIOFBDependentIDKey) );
        firstOpen = num && !nextDependent;

        if( firstOpen) do {

            OSDictionary  * matching;
            OSDictionary  * propMatch;
            OSIterator    * iter;

            matching = serviceMatching("IOFramebuffer");
            if( !matching)
                continue;
            propMatch = OSDictionary::withCapacity(1);
            if( !propMatch)
                continue;
            propMatch->setObject( kIOFBDependentIDKey, num );
            matching->setObject( gIOPropertyMatchKey, propMatch );
            propMatch->release();
            iter = getMatchingServices( matching );
            matching->release();
            if( iter) {

                IOFramebuffer * first = 0;
                IOFramebuffer * last = 0;

                while( (next = (IOFramebuffer *) iter->getNextObject())) {
                    if( !first)
                        first = next;
                    else if ( last)
                        last->setNextDependent( next );
                    last = next;
                }
                if( first && last && (first != last))
                    last->setNextDependent( first );
                iter->release();
            }

        } while( false);

        opened = true;

        UInt32 connectEnabled;
	err = getAttributeForConnection( 0, kConnectionEnable, &connectEnabled );
        if( kIOReturnSuccess != err)
            connectEnabled = true;

        if( connectEnabled)
            IODisplayWrangler::makeDisplayConnects( this );

        if( firstOpen) {
            next = this;
            while( (next = next->getNextDependent()) && (next != this)) {
                next->open();
            }
        }

        if( connectEnabled) {
            setupForCurrentConfig();
            err = kIOReturnSuccess;
        } else
            deliverFramebufferNotification( kIOFBNotifyDisplayModeDidChange, 0 );

    } while( false );

    checkConnectionChange();

    return( err );
}

void IOFramebuffer::setCaptured( bool isCaptured )
{
    captured = isCaptured;
}

void IOFramebuffer::setNextDependent( IOFramebuffer * dependent )
{
    nextDependent = dependent;
}

IOFramebuffer * IOFramebuffer::getNextDependent( void )
{
    return( nextDependent );
}

void  IOFramebuffer::close( void )	// called by the user client when
{					// the window server exits
    mach_msg_header_t * msgh;

    if( isConsoleDevice())
        getPlatform()->setConsoleInfo( 0, kPEAcquireScreen);

    msgh = (mach_msg_header_t *) serverMsg;
    msgh->msgh_remote_port = MACH_PORT_NULL;

    serverConnect = 0;
    captured = false;
}

IODeviceMemory * IOFramebuffer::getVRAMRange( void )
{
    return( getApertureRange( kIOFBSystemAperture ));
}

IOReturn IOFramebuffer::setUserRanges( void )
{
#if 1		/* print ranges */

    UInt32		i, numRanges;
    IODeviceMemory *	mem;

	numRanges = userAccessRanges->getCount();
	IOLog("%s: user ranges num:%ld", getName(), numRanges);
	for( i = 0; i < numRanges; i++) {
	    mem = (IODeviceMemory *) userAccessRanges->getObject( i );
	    if( 0 == mem)
		continue;
	    IOLog(" start:%lx size:%lx",
		mem->getPhysicalAddress(), mem->getLength() );
	}
        IOLog("\n");

#endif
    return( kIOReturnSuccess);
}

IOReturn IOFramebuffer::setupForCurrentConfig( void )
{
    return( doSetup( true ));
}

IOReturn IOFramebuffer::doSetup( bool full )
{
    IOReturn			err;
    IODisplayModeID		mode;
    IOIndex			depth;
    IOPixelInformation		info;
    IODisplayModeInformation	dmInfo;
    IODeviceMemory *		mem;
    IODeviceMemory *		fbRange;
    IOPhysicalAddress		base;
    PE_Video			newConsole;

    err = getCurrentDisplayMode( &mode, &depth );
    if( err)
        IOLog("%s: getCurrentDisplayMode %d\n", getName(), err);

    err = getPixelInformation( mode, depth, kIOFBSystemAperture, &info );
    if( err)
	IOLog("%s: getPixelInformation %d\n", getName(),  err);

    if( full && (clutValid == false) && (info.pixelType == kIOCLUTPixels)) {

	IOColorEntry	*	tempTable;
	int			i;

	tempTable = (IOColorEntry *) IOMalloc( 256 * sizeof( *tempTable));
	if( tempTable) {

	    for( i = 0; i < 256; i++) {
                if( currentMono) {
                    UInt32	lum;

		    lum = 0x0101 * i;
                    tempTable[ i ].red   = lum;
                    tempTable[ i ].green = lum;
                    tempTable[ i ].blue  = lum;
                } else {
                    tempTable[ i ].red   = (appleClut8[ i * 3 + 0 ] << 8)
					  | appleClut8[ i * 3 + 0 ];
                    tempTable[ i ].green = (appleClut8[ i * 3 + 1 ] << 8)
					  | appleClut8[ i * 3 + 1 ];
                    tempTable[ i ].blue  = (appleClut8[ i * 3 + 2 ] << 8)
					  | appleClut8[ i * 3 + 2 ];
		}
	    }
	    setCLUTWithEntries( tempTable, 0, 256, 1 * kSetCLUTImmediately );
	    IOFree( tempTable, 256 * sizeof( *tempTable));
	}
        clutValid = true;
    }

    fbRange = getApertureRange( kIOFBSystemAperture );

    if( full && fbRange) {

        userAccessRanges->removeObject( kIOFBSystemAperture );
        userAccessRanges->setObject( kIOFBSystemAperture, fbRange );
        err = setUserRanges();

	base = fbRange->getPhysicalAddress();
        if( (mem = getVRAMRange())) {
            vramMapOffset = base - mem->getPhysicalAddress();
            if( vramMapOffset > mem->getLength())
                vramMapOffset &= (mem->getLength() - 1);
            setProperty( kIOFBMemorySizeKey, mem->getLength(), 32 );
            mem->release();
	}

	if( vramMap)
	    vramMap->release();
	vramMap = fbRange->map();
	assert( vramMap );
	if( vramMap)
	    base = vramMap->getVirtualAddress();

        // console now available
        if( info.activeWidth >= 128) {
            newConsole.v_baseAddr	= base;
            newConsole.v_rowBytes	= info.bytesPerRow;
            newConsole.v_width		= info.activeWidth;
            newConsole.v_height		= info.activeHeight;
            newConsole.v_depth		= info.bitsPerPixel;
            //	strcpy( consoleInfo->v_pixelFormat, "PPPPPPPP");
            getPlatform()->setConsoleInfo( &newConsole, kPEEnableScreen );
        }


        (void) getInformationForDisplayMode( mode, &dmInfo );
        IOLog( "%s: using (%ldx%ld@%ldHz,%ld bpp)\n", getName(),
                    info.activeWidth, info.activeHeight,
                    (dmInfo.refreshRate + 0x8000) >> 16, info.bitsPerPixel );
    }

    if( full)
        deliverFramebufferNotification( kIOFBNotifyDisplayModeDidChange, 0 );

    if( fbRange)
        fbRange->release();
    if( vramMap)
        setupCursor( &info );

    return( kIOReturnSuccess );
}

IOReturn IOFramebuffer::extSetDisplayMode( IODisplayModeID displayMode,
				IOIndex depth )
{
    IOReturn	err;
    bool	wasSuspended;

    stopCursor();

    if( isConsoleDevice())
        getPlatform()->setConsoleInfo( 0, kPEDisableScreen);

#if DEBUG
    IOLog("%s: set mode, ", getProvider()->getName());
    if( suspended)
        IOLog("susp\n");
#endif
    if( !suspended)
        deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );

    err = setDisplayMode( displayMode, depth );

    clutValid = false;

    setupForCurrentConfig();

    IOLockLock( gIOFBSleepStateLock );
    wasSuspended = suspended;
    if( wasSuspended) {
        suspended = false;
        --gIOFBSuspendCount;
        connectChange = 0;
    }
    IOLockUnlock( gIOFBSleepStateLock );

    if( connectChange)
        checkConnectionChange();
    else
        getAttributeForConnection( 0, kConnectionChanged, (UInt32 *) &connectChange );

    if( wasSuspended && !suspended) {
        AbsoluteTime deadline;

        clock_interval_to_deadline( 10*1000, kMillisecondScale, &deadline );
        thread_call_enter1_delayed( gIOFBClamshellCallout,
                                    (thread_call_param_t) 0, deadline );
    }

    return( err );
}

IOReturn IOFramebuffer::extSetAttribute(
            IOSelect attribute, UInt32 value, IOFramebuffer * other )
{
    IOReturn	err;
    UInt32	data[2];


    switch( attribute ) {
    
        case kIOMirrorAttribute:
    
            if( suspended) {
                err = kIOReturnSuccess;
                break;
            }

            if( !value && (0 == getNextDependent())) {
                err = kIOReturnSuccess;
                break;
            }
    
            stopCursor();
        
            if( isConsoleDevice())
                getPlatform()->setConsoleInfo( 0, kPEDisableScreen);
        
#if DEBUG
            IOLog("mirr %d, ", value);
#endif
            deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );
        
            data[0] = value;
            data[1] = (UInt32) other;
            err = setAttribute( attribute, (UInt32) &data );
        
            clutValid = false;
        
            setupForCurrentConfig();
            break;

        default:
            err = setAttribute( attribute, value );
            break;
    }
   
    return( err );
}

IOReturn IOFramebuffer::extGetAttribute(
            IOSelect attribute, UInt32 * value, IOFramebuffer * other )
{
    IOReturn	err;

    if( kConnectionChanged == attribute) {
    
        IOReturn	err;
        UInt32		connectEnabled;

        err = getAttributeForConnection( 0, kConnectionChanged, (UInt32 *) &connectChange );

        temporaryPowerClampOn();
        IODisplayWrangler::destroyDisplayConnects( this );

        err = getAttributeForConnection( 0, kConnectionEnable, &connectEnabled );
        if( (kIOReturnSuccess != err) || connectEnabled)
            IODisplayWrangler::makeDisplayConnects( this );

        IOFramebuffer * next = this;
        while( (next = next->getNextDependent()) && (next != this)) {
            next->postConnectionChange();
        }

        return( kIOReturnSuccess );
    }

    *value = (UInt32) other;

    err = getAttribute( attribute, value );

    return( err );
}

IOReturn IOFramebuffer::extGetInformationForDisplayMode(
		IODisplayModeID mode, void * info, IOByteCount length )
{
    UInt32			 flags = 0;
    IOReturn			 err;
    bool			 getTiming;
    IOFBDisplayModeDescription * out = (IOFBDisplayModeDescription *) info;

    if( length < sizeof( IODisplayModeInformation))
        return( kIOReturnBadArgument );

    err = getInformationForDisplayMode( mode, &out->info );
    if( kIOReturnSuccess == err) {
	err = IODisplayWrangler::getFlagsForDisplayMode( this, mode, &flags);
	if( kIOReturnSuccess == err) {
	    out->info.flags &= ~kDisplayModeSafetyFlags;
	    out->info.flags |= flags;
	}
        getTiming = (length >= sizeof(IOFBDisplayModeDescription));
        out->timingInfo.flags = getTiming ? kIODetailedTimingValid : 0;
        if( kIOReturnSuccess != getTimingInfoForDisplayMode( mode, &out->timingInfo )) {
            out->timingInfo.flags &= ~kIODetailedTimingValid;
            out->timingInfo.appleTimingID = 0;
        }
    }

    return( err );
}

IOReturn IOFramebuffer::extSetProperties( OSDictionary * props )
{
    OSDictionary * dict;
    OSArray *      array;
    OSNumber *     num;
    IOReturn       kr = kIOReturnUnsupported;

    if( (dict = OSDynamicCast( OSDictionary, props->getObject(kIOFBConfigKey)))) {

        setProperty( kIOFBConfigKey, dict );
    
        if( (num = OSDynamicCast(OSNumber,
                    dict->getObject( kIODisplayConnectFlagsKey))))
            setAttributeForConnection( 0, kConnectionFlags, num->unsigned32BitValue() );

        if( (array = OSDynamicCast(OSArray,
                    dict->getObject( kIOFBDetailedTimingsKey))))
            kr = setDetailedTimings( array );
        else
            kr = kIOReturnSuccess;
    }

    return( kr );
}

//// Controller attributes

IOReturn IOFramebuffer::setAttribute( IOSelect attribute, UInt32 value )
{
    IOReturn	    ret;
    IOFramebuffer * next;
    bool	    wasCaptured;

    switch( attribute ) {

        case kIOCapturedAttribute:

            wasCaptured = captured;
            setCaptured( 0 != value );

            next = this;
            while( (next = next->getNextDependent()) && (next != this)) {
                next->setCaptured(captured);
            }

            if( wasCaptured && !captured)
                checkConnectionChange();
            ret = kIOReturnSuccess;
            break;

	default:
            ret = kIOReturnUnsupported;
            break;
    }

    return( ret );
}

IOReturn IOFramebuffer::getAttribute( IOSelect attribute, UInt32 * value )
{
    return( kIOReturnUnsupported);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFramebuffer::setNumber( OSDictionary * dict, const char * key,
				UInt32 value )
{
    OSNumber *	num;
    bool	ok;

    num = OSNumber::withNumber( value, 32 );
    if( !num)
	return( false );

    ok = dict->setObject( key, num );
    num->release();

    return( ok );
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
    if( !modeIDs)
	return( false );

    err = getDisplayModes( modeIDs );
    if( err)
	return( false );

    infoDict = OSDictionary::withCapacity( 10 );
    if( !infoDict)
	return( false );

    for( modeNum = 0; modeNum < modeCount; modeNum++ ) {

	err = getInformationForDisplayMode( modeIDs[ modeNum ], &info );
	if( err)
	    continue;

	modeDict = OSDictionary::withCapacity( 10 );
	if( !modeDict)
	    break;

	ok = setNumber( modeDict, kIOFBWidthKey,
			info.nominalWidth )
	 && setNumber( modeDict, kIOFBHeightKey,
			info.nominalHeight )
	 && setNumber( modeDict, kIOFBRefreshRateKey,
			info.refreshRate )
	 && setNumber( modeDict, kIOFBFlagsKey,
			info.flags );
	if( !ok)
	    break;

	for( depthNum = 0; depthNum < info.maxDepthIndex; depthNum++ ) {
	    
	    for( aperture = 0; ; aperture++ ) {

		err = getPixelInformation( modeIDs[ modeNum ], depthNum,
					aperture, &pixelInfo );
		if( err)
		    break;

		pixelDict = OSDictionary::withCapacity( 10 );
		if( !pixelDict)
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
		if( !ok)
		    break;

		sprintf( keyBuf, "%lx", depthNum + (aperture << 16) );
                modeDict->setObject( keyBuf, pixelDict );
		pixelDict->release();
	    }
	}

        sprintf( keyBuf, "%lx", modeIDs[ modeNum ] );
        infoDict->setObject( keyBuf, modeDict );
	modeDict->release();
    }

    IODelete( modeIDs, IODisplayModeID, modeCount );

    ok &= infoDict->serialize( s );
    infoDict->release();

    return( ok );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(_IOFramebufferNotifier, IONotifier)
#define LOCKNOTIFY()
#define UNLOCKNOTIFY()

void _IOFramebufferNotifier::remove()
{
    LOCKNOTIFY();

    if( whence) {
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

    return( ret );
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
    if( notify && !notify->init()) {
        notify->release();
        notify = 0;
    }

    if( notify) {
        notify->handler = handler;
        notify->self = self;
        notify->ref = ref;
	notify->fEnable = true;

        if( 0 == fbNotifications)
            fbNotifications = OSSet::withCapacity(1);

        notify->whence = fbNotifications;
        if( fbNotifications)
            fbNotifications->setObject( notify );
    }

    return( notify );
}

IOReturn IOFramebuffer::deliverFramebufferNotification(
                            IOIndex event, void * info = 0 )
{
    OSIterator *		iter;
    _IOFramebufferNotifier *	notify;
    IOReturn			ret = kIOReturnSuccess;
    IOReturn			r;

#if DEBUG
    IOLog("%s: event %d\n", getProvider()->getName(), event);
#endif

    LOCKNOTIFY();

    iter = OSCollectionIterator::withCollection( fbNotifications );

    if( iter) {
        while( (notify = (_IOFramebufferNotifier *) iter->getNextObject())) {

            if( notify->fEnable) {
		r = (*notify->handler)( notify->self, notify->ref, this,
					event, info );
		if( kIOReturnSuccess != r)
		    ret = r;
	    }
        }
        iter->release();
    }

    UNLOCKNOTIFY();

    return( ret );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Some stubs

IOReturn IOFramebuffer::enableController ( void )
{
    return( kIOReturnSuccess );
}

bool IOFramebuffer::isConsoleDevice( void )
{
    return( false );
}

// Set display mode and depth
IOReturn IOFramebuffer::setDisplayMode( IODisplayModeID /* displayMode */,
                            IOIndex /* depth */ )
{
    return( kIOReturnUnsupported);
}

// For pages
IOReturn IOFramebuffer::setApertureEnable(
		IOPixelAperture /* aperture */, IOOptionBits /* enable */ )
{
    return( kIOReturnUnsupported);
}

// Display mode and depth for startup
IOReturn IOFramebuffer::setStartupDisplayMode(
			IODisplayModeID /* displayMode */, IOIndex /* depth */ )
{
    return( kIOReturnUnsupported);
}

IOReturn IOFramebuffer::getStartupDisplayMode(
		IODisplayModeID * /* displayMode */, IOIndex * /* depth */ )
{
    return( kIOReturnUnsupported);
}

//// CLUTs

IOReturn IOFramebuffer::setCLUTWithEntries(
	    IOColorEntry * /* colors */, UInt32 /* index */,
            UInt32 /* numEntries */, IOOptionBits /* options */ )
{
    return( kIOReturnUnsupported);
}

//// Gamma

IOReturn IOFramebuffer::setGammaTable( UInt32 /* channelCount */,
		UInt32 /* dataCount */, UInt32 /* dataWidth */, void * /* data */ )
{
    return( kIOReturnUnsupported);
}


//// Display mode timing information

IOReturn IOFramebuffer::getTimingInfoForDisplayMode(
	    IODisplayModeID /* displayMode */,
            IOTimingInformation * /* info */ )
{
    return( kIOReturnUnsupported);
}

IOReturn IOFramebuffer::validateDetailedTiming(
            void * description, IOByteCount descripSize )
{
    return( kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setDetailedTimings( OSArray * array )
{
    return( kIOReturnUnsupported);
}

//// Connections

IOItemCount IOFramebuffer::getConnectionCount( void )
{
    return( 1);
}

IOReturn IOFramebuffer::setAttributeForConnection( IOIndex connectIndex,
                                         IOSelect attribute, UInt32 info )
{
    IOReturn	err = kIOReturnSuccess;

    switch( attribute ) {
        case kConnectionPower:
#if DEBUG
            IOLog("setAttributeForConnection(%p, ->%d)\n", this,
                0 != (kFBDisplayUsablePowerState & info));
#endif
            isUsable = (0 != (kFBDisplayUsablePowerState & info));
            notifyServerAll( 0 != (kFBDisplayUsablePowerState & info) );
        default:
            break;
    }

    return( err );
}

IOReturn IOFramebuffer::getAttributeForConnection( IOIndex /* connectIndex */,
                IOSelect /* attribute */, UInt32  * /* value */ )
{
    return( kIOReturnUnsupported);
}

//// HW Cursors

IOReturn IOFramebuffer::setCursorImage( void * cursorImage )
{
    return( kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setCursorState( SInt32 x, SInt32 y, bool visible )
{
    return( kIOReturnUnsupported);
}

void IOFramebuffer::flushCursor( void )
{
}

//// Interrupts

IOReturn IOFramebuffer::registerForInterruptType( IOSelect interruptType,
            	    IOFBInterruptProc proc, OSObject * target, void * ref,
		    void ** interruptRef )
{
    return( kIOReturnUnsupported);
}

IOReturn IOFramebuffer::unregisterInterrupt( void * interruptRef )
{
    return( kIOReturnUnsupported);
}

IOReturn IOFramebuffer::setInterruptState( void * interruptRef, UInt32 state )
{
    return( kIOReturnUnsupported);
}

// Apple sensing

IOReturn IOFramebuffer::getAppleSense(
	    IOIndex  /* connectIndex */,
            UInt32 * /* senseType */,
            UInt32 * /* primary */,
            UInt32 * /* extended */,
            UInt32 * /* displayType */ )
{
    return( kIOReturnUnsupported);
}

IOReturn IOFramebuffer::connectFlags( IOIndex /* connectIndex */,
                    IODisplayModeID /* displayMode */, IOOptionBits * /* flags */ )
{
    return( kIOReturnUnsupported);
}

//// IOLowLevelDDCSense

void IOFramebuffer::setDDCClock( IOIndex /* connectIndex */, UInt32 /* value */ )
{
}

void IOFramebuffer::setDDCData( IOIndex /* connectIndex */, UInt32 /* value */ )
{
}

bool IOFramebuffer::readDDCClock( IOIndex /* connectIndex */ )
{
    return( false);
}

bool IOFramebuffer::readDDCData( IOIndex /* connectIndex */ )
{
    return( false);
}

IOReturn IOFramebuffer::enableDDCRaster( bool /* enable */ )
{
    return( kIOReturnUnsupported);
}


//// IOHighLevelDDCSense

bool IOFramebuffer::hasDDCConnect( IOIndex /* connectIndex */ )
{
    return( kIOReturnUnsupported);
}

IOReturn IOFramebuffer::getDDCBlock( IOIndex /* connectIndex */, UInt32 /* blockNumber */,
                    IOSelect /* blockType */, IOOptionBits /* options */,
                    UInt8 * /* data */, IOByteCount * /* length */ )
{
    return( kIOReturnUnsupported);
}

OSMetaClassDefineReservedUnused(IOFramebuffer, 0);
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

