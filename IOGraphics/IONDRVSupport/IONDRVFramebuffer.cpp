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
 * Copyright (c) 1997-1998 Apple Computer, Inc.
 *
 *
 * HISTORY
 *
 * sdouglas  22 Oct 97 - first checked in.
 * sdouglas  24 Jul 98 - start IOKit.
 * sdouglas  15 Dec 98 - cpp.
 *
 */

#include <IOKit/IOLib.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLocks.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/ndrvsupport/IONDRVFramebuffer.h>
#include <IOKit/graphics/IOGraphicsInterfaceTypes.h>
#include <IOKit/pci/IOAGPDevice.h>
#include <IOKit/assert.h>

#include <libkern/c++/OSContainers.h>

#include "IONDRV.h"

IOReturn _IONDRVLibrariesInitialize( IOService * provider );

#include <string.h>

#ifndef kAppleAudioVideoJackStateKey
#define kAppleAudioVideoJackStateKey	"AppleAudioVideoJackState"
#endif

#ifndef kIOPMIsPowerManagedKey
#define kIOPMIsPowerManagedKey	"IOPMIsPowerManaged"
#endif

#define kFirstDepth	kDepthMode1

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOATINDRV : public IONDRVFramebuffer
{
    OSDeclareDefaultStructors(IOATINDRV)

public:
    virtual IOReturn getStartupDisplayMode( IODisplayModeID * displayMode,
                            IOIndex * depth );
    virtual IODeviceMemory * findVRAM( void );

};

class IOATI128NDRV : public IOATINDRV
{
    OSDeclareDefaultStructors(IOATI128NDRV)

public:
    virtual void flushCursor( void );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* generic nub for multihead devices */

class IONDRVDevice : public IOPlatformDevice
{
    OSDeclareDefaultStructors(IONDRVDevice)

public:
    virtual void joinPMtree( IOService * driver );
};


#undef super
#define super IOPlatformDevice

OSDefineMetaClassAndStructors(IONDRVDevice, IOPlatformDevice)

void IONDRVDevice::joinPMtree( IOService * driver )
{
    IOService * realDevice;
    realDevice = OSDynamicCast( IOService, getParentEntry(gIODTPlane) );
    if( realDevice)
        realDevice->addPowerChild(driver);
    else
        super::joinPMtree( driver );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct _VSLService {
    class IONDRVFramebuffer *	framebuffer;
    IOSelect			type;
    IOFBInterruptProc  		handler;
    OSObject *			target;
    void *			ref;
    _VSLService *		next;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// IONDRVFramebuffer has three power states: sleep, doze, wake.

enum {
    kNDRVFramebufferSleepState		= 0,
    kNDRVFramebufferDozeState		= 1,
    kNDRVFramebufferWakeState		= 2,
    kIONDRVFramebufferPowerStateCount	= 3,
    kIONDRVFramebufferPowerStateMax	= kIONDRVFramebufferPowerStateCount - 1
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOFramebuffer

OSDefineMetaClassAndStructors(IONDRVFramebuffer, IOFramebuffer)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService * IONDRVFramebuffer::probe( IOService * 	provider,
                                        SInt32 *	score )
{
    IOService *		inst = this;
    IOService *		newInst = 0;
    const char *	name;

    if( !super::probe( provider, score ))
	return( 0 );

    if( 0 != provider->getProperty(kIONDRVIgnoreKey))
        return( 0 );

    if( getProperty(gIONameMatchedKey)) {
        // matched 
        provider->setProperty(kIONDRVForXKey, this, sizeof(this) );

        // temporary for in-kernel acceleration
        name = provider->getName();
        if( (0 == strncmp("ATY,Rage128", name, strlen("ATY,Rage128")))
         || (0 == strncmp("ATY,RageM3", name, strlen("ATY,RageM3"))))
            newInst = new IOATI128NDRV;
        else if( 0 == strncmp("ATY,", name, strlen("ATY,")))
            newInst = new IOATINDRV;

	if( newInst) {
            if( ! newInst->init( inst->getPropertyTable())) {
                newInst->release();
                newInst = 0;
            }
	    inst = newInst;
	}
    }

    return( inst );
}

IOReturn IONDRVFramebuffer::setProperties( OSObject * properties )
{
    OSDictionary *	dict;
    OSData *		data;
    IOReturn		kr = kIOReturnUnsupported;

    if( !(dict = OSDynamicCast( OSDictionary, properties)))
	return( kIOReturnBadArgument);

    if( (data = OSDynamicCast(OSData,
		dict->getObject("driver,AAPL,MacOS,PowerPC")))) {

        if( ndrvState)
            return( kIOReturnStillOpen );

	if( ndrv)
            ndrv->release();
	nub->setProperty("driver,AAPL,MacOS,PowerPC", data);
	nub->removeProperty("AAPL,ndrvInst");
        ndrv = IONDRV::fromRegistryEntry( nub, &_undefinedSymbolHandler, (void *) this );
	if( ndrv)
            setName( ndrv->driverName());

	kr = kIOReturnSuccess;
    }

    return( kr );
}

bool IONDRVFramebuffer::start( IOService * provider )
{
    bool		ok = false;
    IOService *		parent = 0;
    OSData *		data;

    if( 0 == getProperty(gIONameMatchedKey)) {

        // !matched 
        OSIterator *	  iter;
        IORegistryEntry * next;
        IOService *	  nub;

        if( 0 == provider->getProperty("AAPL,interrupts")) {
            iter = provider->getChildIterator( gIODTPlane );
            if( iter) {
                bool haveDoneLibInit = false;

                while( (next = (IORegistryEntry *) iter->getNextObject())) {
        
                    data = OSDynamicCast( OSData, next->getProperty("device_type"));
                    if( !data || (0 != strcmp("display", (char *) data->getBytesNoCopy())))
                        continue;
        
                    if( !haveDoneLibInit) {
                        haveDoneLibInit = (kIOReturnSuccess == _IONDRVLibrariesInitialize(provider));
                        if( !haveDoneLibInit)
                            continue;
                    }
                    nub = new IONDRVDevice;
                    if( !nub)
                        continue;
                    if( !nub->init( next, gIODTPlane )) {
                        nub->free();
                        nub = 0;
                        continue;
                    }

                    nub->setProperty( kIOFBDependentIDKey, (UInt64) provider, 64 );
                    nub->attach( getPlatform() );
                    nub->registerService();
                }
                iter->release();
            }
        }
        return( false );
    }

    do {
        cachedVDResolution.csDisplayModeID = kDisplayModeIDInvalid;

   	nub = provider;
        ndrv = IONDRV::fromRegistryEntry( provider, &_undefinedSymbolHandler, this );
	if( ndrv)
            setName( ndrv->driverName());
        consoleDevice = (0 != provider->getProperty("AAPL,boot-display"));
        powerState = kIONDRVFramebufferPowerStateMax;

        if( 0 == nub->getDeviceMemoryCount()) {
            parent = OSDynamicCast( IOService, nub->getParentEntry(gIODTPlane));
            if( parent) {
                parent->getResources();
                OSArray * array = parent->getDeviceMemory();
                array->retain();
                nub->setDeviceMemory( array);
                array->release();
            }
        }
        if( parent)
            device = parent;
        else
            device = nub;

        if( false == super::start( nub ))
	    continue;

        OSObject * obj;
        if( (obj = nub->getProperty( kIOFBDependentIDKey )))
            setProperty( kIOFBDependentIDKey, obj );

        platformSleep = (false == getPlatform()->hasPrivPMFeature( kPMHasLegacyDesktopSleepMask )
                      && (false == getPlatform()->hasPMFeature( kPMCanPowerOffPCIBusMask )));

        IOOptionBits flags = getPMRootDomain()->getSleepSupported();
        getPMRootDomain()->setSleepSupported(flags & ~kFrameBufferDeepSleepSupported);

        // default flags can be overriden
        accessFlags = 0;
        if(0 == strncmp("3Dfx", provider->getName(), strlen("3Dfx")))
            accessFlags |= kFramebufferDisableAltivecAccess;

	if( (data = OSDynamicCast( OSData, provider->getProperty(kIOFBHostAccessFlagsKey))))
            accessFlags = *((UInt32 *) data->getBytesNoCopy());

	ok = true;			// Success

    } while( false);
    
    return( ok);
}

bool IONDRVFramebuffer::isConsoleDevice( void )
{
    return( consoleDevice );
}

// osfmk/ppc/mappings.h
extern "C" { extern void ignore_zero_fault(boolean_t); }

IOReturn IONDRVFramebuffer::enableController( void )
{
    IOReturn		err;
    const char *	logname;

    logname = getProvider()->getName();

    if( 0 == strcmp( "control", logname))
        waitForService( resourceMatching( "IOiic0" ));

    if( 0 == ndrv)
	err = kIOReturnNotReady;
    else
        err = _IONDRVLibrariesInitialize( getProvider() );


    if( kIOReturnSuccess == err) do {

        // wait for accelerator module, display parameter drivers
        // device->waitQuiet();
        // find out about onboard audio/video jack state
        // OSObject * notify =
        addNotification( gIOPublishNotification,
                         resourceMatching(kAppleAudioVideoJackStateKey), 
                         _videoJackStateChangeHandler, this, 0 );

        ignore_zero_fault( true );
	err = checkDriver();
        ignore_zero_fault( false );

        if( err) {
            IOLog("%s: Not usable\n", logname );
            if( err == -999)
                IOLog("%s: driver incompatible.\n", logname );
            continue;
        }
        UInt32 isOnline = true;
        if( (kIOReturnSuccess != getAttributeForConnection( 0, kConnectionEnable, &isOnline ))
          || isOnline || true) {
            getCurrentConfiguration();
        }
        online = isOnline;
        vramMemory = findVRAM();

        ignore_zero_fault( true );
        if( (nub != device) && (noErr == doStatus( cscProbeConnection, 0 )))
            setProperty( kIOFBProbeOptionsKey, kIOFBUserRequestProbe, 32);
        ignore_zero_fault( false );

        // initialize power management of the device
        initForPM();
	device->setProperty(kIOPMIsPowerManagedKey, true);

    } while( false);

    return( err);
}

bool IONDRVFramebuffer::_videoJackStateChangeHandler( void * target, void * ref,
                                                        IOService * resourceService )
{
    IONDRVFramebuffer * self = (IONDRVFramebuffer *) target;
    IOReturn		err;
    UInt32		jackData;

    OSObject * jackValue = resourceService->getProperty(kAppleAudioVideoJackStateKey);
    if( !jackValue)
        return( true );

    jackData = (jackValue == kOSBooleanTrue);
    IOLog(kAppleAudioVideoJackStateKey " %ld\n", jackData);

    self->nub->setProperty( kAppleAudioVideoJackStateKey, &jackData, sizeof(jackData) );
    resourceService->removeProperty(kAppleAudioVideoJackStateKey);

    if( self->avJackState != jackData) {
        ignore_zero_fault( true );
        err = self->doControl( cscProbeConnection, 0 );
        ignore_zero_fault( false );
        self->avJackState = jackData;
    }

    return( true );
}

IOReturn IONDRVFramebuffer::_probeAction( IONDRVFramebuffer * self, IOOptionBits options )
{
    IOReturn err;

    if( options & kIOFBUserRequestProbe) {

        ignore_zero_fault( true );

        err = self->doControl( cscProbeConnection, 0 );

        IONDRVFramebuffer * other;
        if( (other = OSDynamicCast( IONDRVFramebuffer, self->nextDependent))) {
            other->doControl( cscProbeConnection, 0 );
        }

        ignore_zero_fault( false );

    } else
        err = kIOReturnBadArgument;

    return( err );
}

IOReturn IONDRVFramebuffer::requestProbe( IOOptionBits options )
{
    IOReturn	 err;
    IOWorkLoop * wl;

    if( (wl = getWorkLoop())) {
        err = wl->runAction( (IOWorkLoop::Action) &_probeAction, this, (void *) options );
    } else
        err = kIOReturnNotReady;

    return( err );
}

IODeviceMemory * IONDRVFramebuffer::getVRAMRange( void )
{
    if( vramMemory)
	vramMemory->retain();

    return( vramMemory );
}

const IOTVector * IONDRVFramebuffer::_undefinedSymbolHandler( void * self, 
                            const char * libraryName, const char * symbolName )
{
    return( ((IONDRVFramebuffer *)self)->undefinedSymbolHandler( libraryName, symbolName) );
}

const IOTVector * IONDRVFramebuffer::undefinedSymbolHandler( const char * libraryName,
                                                       const char * symbolName )
{
    return( 0 );
}

void IONDRVFramebuffer::free( void )
{
    super::free();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::registerForInterruptType( IOSelect interruptType,
                        IOFBInterruptProc proc, OSObject * target, void * ref,
			void ** interruptRef )

{
    _VSLService *	service;
    IOReturn		err;

    if( (interruptType == kIOFBVBLInterruptType)
        && (getProvider()->getProperty("Ignore VBL")))
	return( kIOReturnUnsupported );

    for( service = vslServices;
	 service && (service->type != interruptType);
	 service = service->next ) {}

    if( service) {

	if( service->handler)
	    err = kIOReturnBusy;

	else {
	    service->target	= target;
	    service->ref	= ref;
	    service->handler	= proc;
	    *interruptRef	= service;
	    err			= kIOReturnSuccess;
	}

    } else
	err = kIOReturnNoResources;

    return( err );
}

IOReturn IONDRVFramebuffer::unregisterInterrupt( void * interruptRef )
{
    _VSLService *	service = (_VSLService *) interruptRef;

    service->handler = 0;

    return( kIOReturnSuccess );
}

IOReturn IONDRVFramebuffer::setInterruptState( void * interruptRef, 
						UInt32 state )
{
    return( kIOReturnUnsupported );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//// VSL calls

OSStatus IONDRVFramebuffer::VSLNewInterruptService(
                                        void * entryID,
                                        IOSelect serviceType,
                                        _VSLService ** vslService )
{
    IORegistryEntry *	regEntry;
    IONDRVFramebuffer *	fb;
    _VSLService *	service;
    IOReturn		err = kIOReturnSuccess;

    REG_ENTRY_TO_OBJ( (const RegEntryID *) entryID, regEntry)

    fb = OSDynamicCast( IONDRVFramebuffer,
		regEntry->getChildEntry( gIOServicePlane ));
    assert( fb );

    IOLog("VSLNewInterruptService('%4s')\n", (char *) &serviceType);

    if( fb) {
	service = IONew( _VSLService, 1 );

	if( service) {
            service->framebuffer	= fb;
            service->type		= serviceType;
	    service->handler		= 0;
            service->next = fb->vslServices;
            fb->vslServices = service;

            *vslService = service;

	} else
	    err = kIOReturnNoMemory;

    } else
	err = kIOReturnBadArgument;

    return( err );
}

OSStatus IONDRVFramebuffer::VSLDisposeInterruptService(_VSLService * vslService)
{
    IONDRVFramebuffer *	fb;
    _VSLService * 	next;
    _VSLService * 	prev;

    if( vslService) {

	fb = vslService->framebuffer;

        prev = fb->vslServices;
	if( prev == vslService)
	    fb->vslServices = vslService->next;
	else {
	    while( ((next = prev->next) != vslService) && next)
		prev = next;
	    if( next)
		prev->next = vslService->next;
	}

	IODelete( vslService, _VSLService, 1 );
    }

    return( kIOReturnSuccess );
}

OSStatus IONDRVFramebuffer::VSLDoInterruptService( _VSLService * vslService )
{
    IOFBInterruptProc	proc;

    if( vslService) {
	if( (proc = vslService->handler))
	    (*proc) (vslService->target, vslService->ref);
    }

    return( kIOReturnSuccess );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct _VSLCursorRef {
    IOFramebuffer *	framebuffer;
    void * 		cursorImage;
};

Boolean IONDRVFramebuffer::VSLPrepareCursorForHardwareCursor(
                                        void * cursorRef,
                                        IOHardwareCursorDescriptor * hwDesc,
                                        IOHardwareCursorInfo * hwCursorInfo )
{
    _VSLCursorRef *	cursor = (_VSLCursorRef *) cursorRef;
    bool		ok;

    if( hwCursorInfo->colorMap)
        hwCursorInfo->colorMap += 1;
    ok = cursor->framebuffer->convertCursorImage(
		cursor->cursorImage, hwDesc, hwCursorInfo );
    if( hwCursorInfo->colorMap)
        hwCursorInfo->colorMap -= 1;

    return( ok );
}

IOReturn IONDRVFramebuffer::setCursorImage( void * cursorImage )
{
    _VSLCursorRef		cursorRef;
    VDSetHardwareCursorRec	setCursor;
    IOReturn			err;

    if( 0 == powerState)
        return( kIOReturnSuccess );

    cursorRef.framebuffer = this;
    cursorRef.cursorImage = cursorImage;

    setCursor.csCursorRef = (void *) &cursorRef;
    setCursor.csReserved1 = 0;
    setCursor.csReserved2 = 0;

    err = doControl( cscSetHardwareCursor, &setCursor );

    return( err );
}

IOReturn IONDRVFramebuffer::setCursorState( SInt32 x, SInt32 y, bool visible )
{
    VDDrawHardwareCursorRec	drawCursor;
    IOReturn			err;

    if( 0 == powerState)
        return( kIOReturnSuccess );

    if( 0 == OSIncrementAtomic( &ndrvEnter))
    {

        drawCursor.csCursorX 	= x;
        drawCursor.csCursorY 	= y;
        drawCursor.csCursorVisible 	= visible;
        drawCursor.csReserved1 	= 0;
        drawCursor.csReserved2 	= 0;

        err = doControl( cscDrawHardwareCursor, &drawCursor );

    } else
	err = kIOReturnBusy;

    OSDecrementAtomic( &ndrvEnter );

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//============
//= Internal =
//============

IOReturn IONDRVFramebuffer::doControl( UInt32 code, void * params )
{
    IOReturn	err;
    CntrlParam	pb;

    if( ndrvState == 0)
	return( kIOReturnNotOpen);
    
    pb.qLink = 0;
    pb.csCode = code;
    pb.csParams = params;

    OSIncrementAtomic( &ndrvEnter );
    err = ndrv->doDriverIO( /*ID*/ (UInt32) &pb, &pb,
                            kControlCommand, kImmediateIOCommandKind );
    OSDecrementAtomic( &ndrvEnter );

    return( err);
}

IOReturn IONDRVFramebuffer::doStatus( UInt32 code, void * params )
{
    IOReturn	err;
    CntrlParam	pb;

    if( ndrvState == 0)
	return( kIOReturnNotOpen);

    pb.qLink = 0;
    pb.csCode = code;
    pb.csParams = params;

    OSIncrementAtomic( &ndrvEnter );
    err = ndrv->doDriverIO( /*ID*/ (UInt32) &pb, &pb,
                            kStatusCommand, kImmediateIOCommandKind );
    OSDecrementAtomic( &ndrvEnter );

    return( err);
}


IOReturn IONDRVFramebuffer::checkDriver( void )
{
    OSStatus			err = noErr;
    struct DriverInitInfo	initInfo;
    CntrlParam          	pb;
    VDClutBehavior		clutSetting;
    VDGammaRecord		gammaRec;
    VDSwitchInfoRec		switchInfo;
    VDPageInfo			pageInfo;

    if( ndrvState == 0) {
	do {
	    initInfo.refNum = 0xffcd;			// ...sure.
	    MAKE_REG_ENTRY(initInfo.deviceEntry, nub )
    
	    err = ndrv->doDriverIO( 0, &initInfo,
				kInitializeCommand, kImmediateIOCommandKind );
	    if( err) continue;

	    err = ndrv->doDriverIO( 0, &pb,
				kOpenCommand, kImmediateIOCommandKind );

	} while( false);

	if( err)
	    return( err);

        // allow calls to ndrv
        ndrvState = 2;

#if IONDRVI2CLOG
        do {
            VDCommunicationInfoRec	commInfo;
        
            bzero( &commInfo, sizeof( commInfo));
            commInfo.csBusID = kVideoDefaultBus;

            err = doStatus( cscGetCommunicationInfo, &commInfo );
            IOLog("%s: cscGetCommunicationInfo: ", getName());
            if( kIOReturnSuccess != err) {
                IOLog("fails with %ld\n", err);
                continue;
            }
            if( commInfo.csSupportedTypes & (1<<kVideoDDCciReplyType) )
                IOLog("supports kVideoDDCciReplyType, ");
            if( commInfo.csSupportedTypes & (1<<kVideoSimpleI2CType) ) {
                IOLog("supports kVideoSimpleI2CType");
                VDCommunicationRec	comm;
                UInt8			edidData[132];
                UInt8			edidRequest[2];
        
                edidData[0]		= 0xAA;
                edidData[1]		= 0xAA;
                edidData[2]		= 0xAA;
                edidData[3]		= 0xAA;
                edidData[128]		= 0xAA;
                edidData[129]		= 0xAA;
                edidData[130]		= 0xAA;
                edidData[131]		= 0xAA;

                memset( edidData, 0xAA, sizeof( edidData));

                edidRequest[0]		= 0;
                edidRequest[1]		= 0;
                
                comm.csBusID		= kVideoDefaultBus;
                comm.csCommFlags	= 0;
                comm.csMinReplyDelay	= 0;
                comm.csReserved2	= 0;
        
                comm.csSendAddress	= 0xA0;
                comm.csSendType		= kVideoSimpleI2CType;
                comm.csSendBuffer	= &edidRequest[0];
                comm.csSendSize		= 0x01;
        
                comm.csReplyAddress	= 0xA1;
                comm.csReplyType	= kVideoSimpleI2CType;
                comm.csReplyBuffer	= &edidData[0];
                comm.csReplySize	= 128;
        
                comm.csReserved3	= 0;
                comm.csReserved4	= 0;
                comm.csReserved5	= 0;
                comm.csReserved6	= 0;

                do {
                    err = doControl( cscDoCommunication, &comm );
                    IOLog("\nI2C read block[%x](%ld)\n", edidRequest[0], err);
                    if( kIOReturnSuccess != err)
                        break;
                    IOLog("    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
                    for( int i = 0; i < 128; i++) {
                        if( 0 == (i & 15))
                            IOLog("\n%02x: ", i);
                        IOLog("%02x ", edidData[i]);
                    }
                    IOLog("\n");
                    if( edidRequest[0] || (0 == edidData[126]))
                        break;
                    edidRequest[0] = 0x80;
                } while( true );
            }
        
        } while( false );
#endif /* IONDRVI2CLOG */

        // duplicate QD InitGDevice
        pageInfo.csMode = switchInfo.csMode;
        pageInfo.csData = 0;
        pageInfo.csPage = 0;
        doControl( cscGrayPage, &pageInfo);

        clutSetting = kSetClutAtSetEntries;
        lastClutSetting = clutSetting;
        doControl( cscSetClutBehavior, &clutSetting);

        do {
            VDDisplayTimingRangeRec	rangeRec;
                
            bzero( &rangeRec, sizeof( rangeRec));
            err = doStatus( cscGetTimingRanges, &rangeRec );
            if( kIOReturnSuccess != err)
                continue;

            setProperty( kIOFBTimingRangeKey, &rangeRec, sizeof( rangeRec));
        
        } while( false );

#if 1
	// bogus for ROM control
	do {

	    VDGetGammaListRec	scan;
	    VDRetrieveGammaRec	get;
	    GammaTbl *		table;
	    char		name[ 64 ];

	    scan.csPreviousGammaTableID = kGammaTableIDFindFirst;
	    scan.csGammaTableName = name;
	    err = doStatus( cscGetGammaInfoList, &scan);
	    if( err || (scan.csGammaTableID == (GammaTableID) kGammaTableIDNoMoreTables))
		continue;

	    table = (GammaTbl *)IOMalloc( scan.csGammaTableSize);
	    if( 0 == table)
		continue;
	    get.csGammaTableID = scan.csGammaTableID;
	    get.csGammaTablePtr = table;
	    
	    err = doStatus( cscRetrieveGammaTable, &get );
	    if( noErr == err) {
		kprintf("Setting gamma %s\n", scan.csGammaTableName);
		gammaRec.csGTable = (Ptr) table;
		doControl( cscSetGamma, &gammaRec );
	    }

	    IOFree( table, scan.csGammaTableSize);

	} while( false);
#endif
    }
    return( noErr);
}


UInt32 IONDRVFramebuffer::iterateAllModes( IODisplayModeID * displayModeIDs )
{
    VDResolutionInfoRec	info;
    UInt32		num = 0;

    info.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;

    while( 
 	   (noErr == doStatus( cscGetNextResolution, &info))
	&& ((SInt32) info.csDisplayModeID > 0) ) {

	    if( displayModeIDs)
		displayModeIDs[ num ] = info.csDisplayModeID;

	    info.csPreviousDisplayModeID = info.csDisplayModeID;
	    num++;
    }

    if( detailedTimings) {
        IOItemCount	count, i;

        count = detailedTimings->getCount();
        if( displayModeIDs) {
            for( i = 0; i < count; i++)
		displayModeIDs[ num + i ] = kDisplayModeIDReservedBase + i;
        }
        num += count;
    }

    return( num);
}

IOReturn IONDRVFramebuffer::getResInfoForArbMode( IODisplayModeID modeID,
                                                IODisplayModeInformation * info )
{
    VDVideoParametersInfoRec	pixelParams;
    VPBlock			pixelInfo;
    VDDetailedTimingRec *	detailed;
    IOIndex			depth;
    IOReturn			err;

    err = validateDisplayMode( modeID, 0, &detailed );

    for( depth = -1; err == kIOReturnSuccess; ) {
        pixelParams.csDisplayModeID = modeID;
        pixelParams.csDepthMode = ++depth + kFirstDepth;
        pixelParams.csVPBlockPtr = &pixelInfo;
        err = doStatus( cscGetVideoParameters, &pixelParams );
    }

    if( depth) {
        info->maxDepthIndex	= depth - 1;
	info->nominalWidth	= pixelInfo.vpBounds.right;
	info->nominalHeight	= pixelInfo.vpBounds.bottom;
        if( detailed)
            info->refreshRate	= detailed->csPixelClock * 65536ULL / 
                                ((detailed->csVerticalActive + detailed->csVerticalBlanking)
                                * (detailed->csHorizontalActive + detailed->csHorizontalBlanking));
        else
            info->refreshRate	= 0;

        err = kIOReturnSuccess;
    }

    return( err );
}

IOReturn IONDRVFramebuffer::getResInfoForMode( IODisplayModeID modeID,
                                                IODisplayModeInformation * info )
{

    bzero( info, sizeof( *info));

    if( (UInt32) modeID >= (UInt32) kDisplayModeIDReservedBase)
        return( getResInfoForArbMode( modeID, info ));

    // unfortunately, there is no "kDisplayModeIDFindSpecific"
    if( cachedVDResolution.csDisplayModeID != (UInt32) modeID) {

        // try the next after cached mode
        cachedVDResolution.csPreviousDisplayModeID = cachedVDResolution.csDisplayModeID;
        if( (noErr != doStatus( cscGetNextResolution, &cachedVDResolution))
         || (cachedVDResolution.csDisplayModeID != (UInt32) modeID) ) {
    
            // else full blown iterate
            cachedVDResolution.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;
            while(
                (noErr == doStatus( cscGetNextResolution, &cachedVDResolution))
             && (cachedVDResolution.csDisplayModeID != (UInt32) modeID) 
             && ((SInt32) cachedVDResolution.csDisplayModeID > 0)) {
        
                cachedVDResolution.csPreviousDisplayModeID = cachedVDResolution.csDisplayModeID;
            }
        }
    }

    if( cachedVDResolution.csDisplayModeID != (UInt32) modeID) {
        cachedVDResolution.csDisplayModeID = kDisplayModeIDInvalid;
        return( kIOReturnUnsupportedMode);

    } else {

	info->maxDepthIndex	= cachedVDResolution.csMaxDepthMode - kFirstDepth;
	info->nominalWidth	= cachedVDResolution.csHorizontalPixels;
	info->nominalHeight	= cachedVDResolution.csVerticalLines;
	info->refreshRate	= cachedVDResolution.csRefreshRate;

	return( noErr);
    }
}

enum {
    kModePreflight = 1,
    kDisplayModeIDPreflight = kDisplayModeIDReservedBase + 1000
};

IOReturn IONDRVFramebuffer::setDetailedTiming(
            IODisplayModeID mode, IOOptionBits options,
            void * _desc, IOByteCount descripSize )
{
    IOReturn		  err;
    VDResolutionInfoRec	  info;
    VDDetailedTimingRec	* desc = (VDDetailedTimingRec *)_desc;
    VDDetailedTimingRec	  look;
    IOIndex		  index;
    UInt32		  checkCurrent = (UInt32) currentDisplayMode;
    bool		  notPreflight = (0 == (options & kModePreflight));

    // current must be ok
    if( mode == currentDisplayMode)
        return( kIOReturnSuccess );

    index = mode - kDisplayModeIDReservedBase;
    bzero( &look, sizeof( VDDetailedTimingRec) );
    look.csTimingSize = sizeof( VDDetailedTimingRec);

    // look for a programmable
    for(
       info.csPreviousDisplayModeID = kDisplayModeIDFindFirstProgrammable;
       (noErr == (err = doStatus( cscGetNextResolution, &info)));
       info.csPreviousDisplayModeID = info.csDisplayModeID) {

        if( (SInt32) info.csDisplayModeID < 0) {
            err = kIOReturnNoResources;
            break;
        }

        look.csDisplayModeID = info.csDisplayModeID;
	err = doStatus( cscGetDetailedTiming, &look );
        if( err != kIOReturnSuccess)
            continue;

        // don't toss current
        if( look.csDisplayModeAlias == checkCurrent) {
            checkCurrent = 0xffffffff;
            continue;
        }

        // see if already set to the right timing
        if( (look.csDisplayModeAlias == (UInt32) mode)
         && (look.csDisplayModeState == kDMSModeReady)
         && (notPreflight)
         && (detailedTimingsCurrent[index] == detailedTimingsSeed))
            break;

        // set it free
        if( look.csDisplayModeState != kDMSModeFree) {
            look.csDisplayModeID    = info.csDisplayModeID;
            look.csDisplayModeAlias = 0;
            look.csDisplayModeState = kDMSModeFree;
            err = doControl( cscSetDetailedTiming, &look );
            if( err != kIOReturnSuccess)
                continue;
        }
        // set it ready
	desc->csDisplayModeID    = info.csDisplayModeID;
        desc->csDisplayModeAlias = mode;
        desc->csDisplayModeSeed  = look.csDisplayModeSeed;
        desc->csDisplayModeState = kDMSModeReady;
        err = doControl( cscSetDetailedTiming, desc );

        if( kIOReturnSuccess == err) {
            if( notPreflight)
                // don't stomp orig record
                desc = &look;
            err = doStatus( cscGetDetailedTiming, desc );
        }
        if( notPreflight && (kIOReturnSuccess == err))
            detailedTimingsCurrent[index] = detailedTimingsSeed;

        break;
    }

    return( err );
}

IOReturn IONDRVFramebuffer::validateDisplayMode(
            IODisplayModeID _mode, IOOptionBits flags,
            VDDetailedTimingRec ** detailed )
{
    UInt32		mode = _mode;
    IOReturn		err = kIOReturnSuccess;
    OSData *		data;
    const void *	bytes;

    if( detailed)
        *detailed = (VDDetailedTimingRec *) 0;

    if( mode >= (UInt32) kDisplayModeIDReservedBase) do {

        if( mode == (UInt32) kDisplayModeIDBootProgrammable)
            continue;

        err = kIOReturnBadArgument;
        if( !detailedTimings)
            continue;

        data = OSDynamicCast( OSData, detailedTimings->getObject(
                                        mode - kDisplayModeIDReservedBase));
        if( !data)
            continue;

        bytes = data->getBytesNoCopy();
        err = setDetailedTiming( mode, 0, (void *) bytes, data->getLength() );
        if( err != kIOReturnSuccess)
            continue;

        if( detailed)
            *detailed = (VDDetailedTimingRec *) bytes;

    } while( false );

    return( err );
}

void IONDRVFramebuffer::getCurrentConfiguration( void )
{
    IOReturn		err;
    VDSwitchInfoRec	switchInfo;
    VDGrayRecord	grayRec;

    grayRec.csMode = 0;			// turn off luminance map
    err = doControl( cscSetGray, &grayRec );
    // driver refused => mono display
    grayMode = ((noErr == err) && (0 != grayRec.csMode));
#if 0
    VDPageInfo		pageInfo;
    if( noErr == doStatus( cscGetMode, &pageInfo )) {
        doControl( cscSetMode, &pageInfo);
        doControl( cscGrayPage, &pageInfo);
    }
#endif

    err = doStatus( cscGetCurMode, &switchInfo );
    if( err == noErr) {
        currentDisplayMode	= switchInfo.csData;
        currentDepth		= switchInfo.csMode - kFirstDepth;
        currentPage		= switchInfo.csPage;
	if( 0 == (physicalFramebuffer = pmap_extract( kernel_pmap,
		((vm_address_t) switchInfo.csBaseAddr) )))
	    physicalFramebuffer = (UInt32) switchInfo.csBaseAddr;
    } else
	IOLog("%s: cscGetCurMode failed\n", nub->getName());
}

IODeviceMemory * IONDRVFramebuffer::makeSubRange( 
	IOPhysicalAddress	start,
	IOPhysicalLength	length ) 
{
    IODeviceMemory *	mem = 0;
    UInt32		numMaps, i;
    IOService *		device;

    device = nub;
    numMaps = device->getDeviceMemoryCount();

    for( i = 0; (!mem) && (i < numMaps); i++) {
	mem = device->getDeviceMemoryWithIndex(i);
	if( !mem)
	    continue;
	mem = IODeviceMemory::withSubRange( mem,
			start - mem->getPhysicalAddress(), length );
    }
    if( !mem)
	mem = IODeviceMemory::withRange( start, length );

    return( mem );
}

IODeviceMemory * IONDRVFramebuffer::getApertureRange( IOPixelAperture aper )
{
    IOReturn			err;
    IOPixelInformation		info;
    IOByteCount			bytes;

    err = getPixelInformation( currentDisplayMode, currentDepth, aper,
                                &info );
    if( err)
	return( 0 );

    bytes = (info.bytesPerRow * info.activeHeight) + 128;

    return( makeSubRange( physicalFramebuffer, bytes ));
}

#if 1

IODeviceMemory * IONDRVFramebuffer::findVRAM( void )
{
    VDVideoParametersInfoRec	pixelParams;
    VPBlock			pixelInfo;
    VDResolutionInfoRec		vdRes;
    UInt32			size;
    IOPhysicalAddress		vramBase = physicalFramebuffer;
    IOByteCount			vramLength;
    IOReturn			err;
    OSData *			prop;

    vramLength = 0;
    prop = OSDynamicCast( OSData, nub->getProperty("VRAM,memsize"));

    if( prop) {
        vramLength = *((IOByteCount *)prop->getBytesNoCopy());
        if( vramLength) {
            vramLength = (vramLength + (vramBase & 0xffff)) & 0xffff0000;
            vramBase &= 0xffff0000;
        }
    }

    if( !vramLength) {

        vdRes.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;
        while(
            (noErr == doStatus( cscGetNextResolution, &vdRes))
        && ((SInt32) vdRes.csDisplayModeID > 0) )
        {
            pixelParams.csDisplayModeID = vdRes.csDisplayModeID;
            pixelParams.csDepthMode = vdRes.csMaxDepthMode;
            pixelParams.csVPBlockPtr = &pixelInfo;
            err = doStatus( cscGetVideoParameters, &pixelParams);
            if( err)
                continue;
    
            // Control hangs its framebuffer off the end of the aperture to support
            // 832 x 624 @ 32bpp. The commented out version will correctly calculate
            // the vram length, but DPS needs the full extent to be mapped, so we'll
            // end up mapping an extra page that will address vram through the
            // little endian aperture. No other drivers like this known.
#if 1
            size = 0x40 + pixelInfo.vpBounds.bottom *
                            (pixelInfo.vpRowBytes & 0x7fff);
#else
            size = ( (pixelInfo.vpBounds.right * pixelInfo.vpPixelSize) / 8)	// last line
                    + (pixelInfo.vpBounds.bottom - 1) *
                    (pixelInfo.vpRowBytes & 0x7fff);
#endif
            if( size > vramLength)
                vramLength = size;
    
            vdRes.csPreviousDisplayModeID = vdRes.csDisplayModeID;
        }
    
        vramLength = (vramLength + (vramBase & 0xffff) + 0xffff) & 0xffff0000;
        vramBase &= 0xffff0000;
    }

    return( makeSubRange( vramBase, vramLength ));
}

#else

IODeviceMemory * IONDRVFramebuffer::findVRAM( void )
{
    IOPhysicalAddress	vramBase = physicalFramebuffer;
    IODeviceMemory *	mem = 0;
    UInt32		numMaps, i;
    IOService *		device;

    device = nub;
    numMaps = device->getDeviceMemoryCount();

    for( i = 0; (!mem) && (i < numMaps); i++) {
	mem = device->getDeviceMemoryWithIndex(i);
	if( !mem)
	    continue;
        if( (vramBase >= mem->getPhysicalAddress())
         && (vramBase < (mem->getPhysicalAddress() + mem->getLength())))
            break;
    }
    return( mem );
}

#endif

//============
//= External =
//============

const char * IONDRVFramebuffer::getPixelFormats( void )
{
    static const char * ndrvPixelFormats =
        IO1BitIndexedPixels "\0"
        IO2BitIndexedPixels "\0"
        IO4BitIndexedPixels "\0"
        IO8BitIndexedPixels "\0"
        IO16BitDirectPixels "\0"
        IO32BitDirectPixels "\0"
        "\0";

    return( ndrvPixelFormats);
}

IOItemCount IONDRVFramebuffer::getDisplayModeCount( void )
{
    return( iterateAllModes( 0 ));
}

IOReturn IONDRVFramebuffer::getDisplayModes( IODisplayModeID * allDisplayModes )
{
    iterateAllModes( allDisplayModes );
    return( kIOReturnSuccess );
}

IOReturn IONDRVFramebuffer::validateDetailedTiming(
            void * desc, IOByteCount descripSize )
{
    IOReturn err;

    err = setDetailedTiming( kDisplayModeIDPreflight,
                             kModePreflight, desc, descripSize);

    return( err );
}

IOReturn IONDRVFramebuffer::setDetailedTimings( OSArray * array )
{
    IOReturn	err;
    UInt32 *	newCurrent;
    IOItemCount	newCount;

    if( !array) {
        if( detailedTimings) {
            IODelete( detailedTimingsCurrent, UInt32, detailedTimings->getCount());
            detailedTimingsCurrent = 0;
        }
        removeProperty( kIOFBDetailedTimingsKey );
        detailedTimings = 0;
        detailedTimingsSeed++;
        return( kIOReturnSuccess );
    }

    newCount = array->getCount();
    newCurrent = IONew(UInt32, newCount);
    if( newCurrent) {
        if( detailedTimings)
            IODelete( detailedTimingsCurrent, UInt32, detailedTimings->getCount());
        detailedTimingsCurrent = newCurrent;
        bzero( newCurrent, newCount * sizeof( UInt32));
        setProperty( kIOFBDetailedTimingsKey, array );	// retains
        detailedTimings = array;
        detailedTimingsSeed++;

        if( currentDisplayMode == kDisplayModeIDBootProgrammable) {
            VDDetailedTimingRec		look;
            VDDetailedTimingRec *	detailed;
            OSData *			data;
            IODisplayModeID		newDisplayMode;

            newDisplayMode = currentDisplayMode;

            bzero( &look, sizeof( VDDetailedTimingRec) );
            look.csTimingSize = sizeof( VDDetailedTimingRec);
            look.csDisplayModeID = kDisplayModeIDBootProgrammable;
            err = doStatus( cscGetDetailedTiming, &look );

            if( kIOReturnSuccess == err)
              for( int i = 0;
                 (data = OSDynamicCast( OSData, detailedTimings->getObject(i)));
                 i++) {

                detailed = (VDDetailedTimingRec *) data->getBytesNoCopy();
                if( (detailed->csHorizontalActive == look.csHorizontalActive)
                 && (detailed->csVerticalActive == look.csVerticalActive)) {

                    newDisplayMode = i + kDisplayModeIDReservedBase;
                    break;
                }
            }
            if( newDisplayMode != currentDisplayMode) {
                err = validateDisplayMode( newDisplayMode, 0, 0 );
                currentDisplayMode = newDisplayMode;
            }
        }

        err = kIOReturnSuccess;
    } else
        err = kIOReturnNoMemory;

    return( err );
}

IOReturn IONDRVFramebuffer::getInformationForDisplayMode(
		IODisplayModeID displayMode, IODisplayModeInformation * info )
{
    IOReturn			err;

    err = getResInfoForMode( displayMode, info );
    if( err)
        err = kIOReturnUnsupportedMode;

    return( err );
}


UInt64 IONDRVFramebuffer::getPixelFormatsForDisplayMode(
		IODisplayModeID /* displayMode */, IOIndex depthIndex )
{
    return( 1 << depthIndex );
}

IOReturn IONDRVFramebuffer::getPixelInformation(
	IODisplayModeID displayMode, IOIndex depth,
	IOPixelAperture aperture, IOPixelInformation * info )
{
    SInt32			err;
    VDVideoParametersInfoRec	pixelParams;
    VPBlock			pixelInfo;

    bzero( info, sizeof( *info));

    if( aperture)
        return( kIOReturnUnsupportedMode);

    err = validateDisplayMode( displayMode, 0, 0 );
    if( err)
        return( err );

    do {
    	pixelParams.csDisplayModeID = displayMode;
	pixelParams.csDepthMode = depth + kFirstDepth;
	pixelParams.csVPBlockPtr = &pixelInfo;
	err = doStatus( cscGetVideoParameters, &pixelParams );
	if( err)
	    continue;

	info->flags		= accessFlags;

	info->activeWidth	= pixelInfo.vpBounds.right;
	info->activeHeight	= pixelInfo.vpBounds.bottom;
	info->bytesPerRow       = pixelInfo.vpRowBytes & 0x7fff;
	info->bytesPerPlane	= pixelInfo.vpPlaneBytes;
	info->bitsPerPixel 	= pixelInfo.vpPixelSize;

        switch( pixelInfo.vpPixelSize / 8 ) {

          default:
            pixelInfo.vpPixelSize = 8;
          case 0:
          case 1:
            strncpy( info->pixelFormat, "PPPPPPPP", pixelInfo.vpPixelSize);
            info->pixelType = kIOCLUTPixels;
            info->componentMasks[0] = (1 << pixelInfo.vpPixelSize) - 1;
            info->bitsPerPixel = pixelInfo.vpPixelSize;
            info->componentCount = 1;
            info->bitsPerComponent = pixelInfo.vpPixelSize;
            break;

          case 2:
            strcpy( info->pixelFormat, "-RRRRRGGGGGBBBBB");
            info->pixelType = kIORGBDirectPixels;
            info->componentMasks[0] = 0x7c00;
            info->componentMasks[1] = 0x03e0;
            info->componentMasks[2] = 0x001f;
            info->bitsPerPixel = 16;
            info->componentCount = 3;
            info->bitsPerComponent = 5;
            break;

          case 4:
            strcpy( info->pixelFormat, "--------RRRRRRRRGGGGGGGGBBBBBBBB");
            info->pixelType = kIORGBDirectPixels;
            info->componentMasks[0] = 0x00ff0000;
            info->componentMasks[1] = 0x0000ff00;
            info->componentMasks[2] = 0x000000ff;
            info->bitsPerPixel = 32;
            info->componentCount = 3;
            info->bitsPerComponent = 8;
            break;
        }

    } while( false);

    return( err);
}

IOReturn IONDRVFramebuffer::getTimingInfoForDisplayMode(
		IODisplayModeID displayMode, IOTimingInformation * info )
{
    VDTimingInfoRec		timingInfo;
    OSStatus			err;

    err = validateDisplayMode( displayMode, 0, 0 );
    if( err)
        return( err );

    timingInfo.csTimingMode = displayMode;
    // in case the driver doesn't do it:
    timingInfo.csTimingFormat = kDeclROMtables;
    err = doStatus( cscGetModeTiming, &timingInfo);
    if( err == noErr) {
	if( timingInfo.csTimingFormat == kDeclROMtables)
	    info->appleTimingID = timingInfo.csTimingData;
	else
	    info->appleTimingID = timingInvalid;

        if( info->flags & kIODetailedTimingValid) {
            VDDetailedTimingRec	* look = (VDDetailedTimingRec *) &info->detailedInfo.v2;

            bzero( look, sizeof( VDDetailedTimingRec) );
            look->csTimingSize = sizeof( VDDetailedTimingRec);
            look->csDisplayModeID = displayMode;
            err = doStatus( cscGetDetailedTiming, look );
            if( kIOReturnSuccess != err)
                info->flags &= ~kIODetailedTimingValid;
        }

	return( kIOReturnSuccess);
    }

    return( kIOReturnUnsupportedMode);
}

IOReturn IONDRVFramebuffer::getCurrentDisplayMode( 
				IODisplayModeID * displayMode, IOIndex * depth )
{
    if( displayMode)
	*displayMode = currentDisplayMode;
    if( depth)
	*depth = currentDepth;

    return( kIOReturnSuccess);
}

IOReturn IONDRVFramebuffer::setDisplayMode( IODisplayModeID displayMode, IOIndex depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo;
    VDPageInfo		pageInfo;

    err = validateDisplayMode( displayMode, 0, 0 );
    if( err)
        return( err );

    ignore_zero_fault( true );
    switchInfo.csData = displayMode;
    switchInfo.csMode = depth + kFirstDepth;
    switchInfo.csPage = 0;
    err = doControl( cscSwitchMode, &switchInfo);
    if(err)
	IOLog("%s: cscSwitchMode:%d\n", nub->getName(), (int)err);

    // duplicate QD InitGDevice
    pageInfo.csMode = switchInfo.csMode;
    pageInfo.csData = 0;
    pageInfo.csPage = 0;
    doControl( cscSetMode, &pageInfo);
    doControl( cscGrayPage, &pageInfo);
    ignore_zero_fault( false );

    getCurrentConfiguration();

    return( err);
}

IOReturn IONDRVFramebuffer::setStartupDisplayMode(
			IODisplayModeID displayMode, IOIndex depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo;

    err = validateDisplayMode( displayMode, 0, 0 );
    if( err)
        return( err );

    switchInfo.csData = displayMode;
    switchInfo.csMode = depth + kFirstDepth;
    err = doControl( cscSavePreferredConfiguration, &switchInfo);
    return( err);
}

IOReturn IONDRVFramebuffer::getStartupDisplayMode(
				IODisplayModeID * displayMode, IOIndex * depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo;

    err = doStatus( cscGetPreferredConfiguration, &switchInfo);
    if( err == noErr) {
	*displayMode	= switchInfo.csData;
	*depth		= switchInfo.csMode - kFirstDepth;
    }
    return( err);
}

IOReturn IONDRVFramebuffer::setApertureEnable( IOPixelAperture /* aperture */,
						IOOptionBits /* enable */ )
{
    return( kIOReturnSuccess);
}

IOReturn IONDRVFramebuffer::setCLUTWithEntries(
			IOColorEntry * colors, UInt32 index, UInt32 numEntries,
			IOOptionBits options )
{
    IOReturn		err;
    VDSetEntryRecord	setEntryRec;
    VDClutBehavior	clutSetting;
    VDGrayRecord	grayRec;

    if( 0 == powerState)
        return( kIOReturnSuccess );

    if( options & kSetCLUTWithLuminance)
        grayRec.csMode = 1;		// turn on luminance map
    else
        grayRec.csMode = 0;		// turn off luminance map

    if( grayRec.csMode != lastGrayMode) {
	doControl( cscSetGray, &grayRec);
	lastGrayMode = grayRec.csMode;
    }

    if( options & kSetCLUTImmediately)
        clutSetting = kSetClutAtSetEntries;
    else
        clutSetting = kSetClutAtVBL;

    if( clutSetting != lastClutSetting) {
	doControl( cscSetClutBehavior, &clutSetting);
	lastClutSetting = clutSetting;
    }

    if( options & kSetCLUTByValue)
        setEntryRec.csStart = -1;
    else
        setEntryRec.csStart = index;

    setEntryRec.csTable = (ColorSpec *) colors;
    setEntryRec.csCount = numEntries - 1;
    err = doControl( cscSetEntries, &setEntryRec);

    return( err);
}

IOReturn IONDRVFramebuffer::setGammaTable( UInt32 channelCount, UInt32 dataCount,
                                            UInt32 dataWidth, void * data )
{
    IOReturn		err;
    VDGammaRecord	gammaRec;
    struct GammaTbl {
        short gVersion;		/*gamma version number*/
        short gType;		/*gamma data type*/
        short gFormulaSize;	/*Formula data size */
        short gChanCnt;		/*number of channels of data */
        short gDataCnt;		/*number of values/channel */
        short gDataWidth;	/*bits/corrected value */
				/* (data packed to next larger byte size) */
        UInt8 gFormulaData[0];	/* data for formulas followed by gamma values */
    };
    GammaTbl * 	table = NULL;
    IOByteCount	dataLen = 0;

    if( 0 == powerState)
        return( kIOReturnSuccess );

    if( data) {
        dataLen = (dataWidth + 7) / 8;
        dataLen *= dataCount * channelCount;
        table = (GammaTbl *) IOMalloc( dataLen + sizeof( struct GammaTbl));
        if( NULL == table)
            return( kIOReturnNoMemory);

	table->gVersion		= 0;
	table->gType		= 0;
	table->gFormulaSize	= 0;
	table->gChanCnt		= channelCount;
	table->gDataCnt		= dataCount;
	table->gDataWidth	= dataWidth;
	bcopy( data, table->gFormulaData, dataLen);
    }

    gammaRec.csGTable = (Ptr) table;
    err = doControl( cscSetGamma, &gammaRec);
    if( table)
        IOFree( table, dataLen + sizeof( struct GammaTbl));

    return( err);
}


IOReturn IONDRVFramebuffer::setAttribute( IOSelect attribute, UInt32 _value )
{
    IOReturn		err = kIOReturnSuccess;
    IOFramebuffer *	other;
    UInt32 *		data = (UInt32 *) _value;
    UInt32		value;

    switch( attribute ) {

        case kIOPowerAttribute:
            err = ndrvSetPowerState( _value );
	    break;


	default:
	    err = super::setAttribute( attribute, _value );
    }

    return( err );
}

IOReturn IONDRVFramebuffer::getAttribute( IOSelect attribute, UInt32 * value )
{
    IOReturn			err = kIOReturnSuccess;
    VDSupportsHardwareCursorRec	hwCrsrSupport;
    VDMirrorRec			mirror;
    IOFramebuffer *		other;

    switch( attribute ) {

	case kIOHardwareCursorAttribute:

	    *value = ((kIOReturnSuccess ==
			doStatus( cscSupportsHardwareCursor, &hwCrsrSupport))
                    && true && (hwCrsrSupport.csSupportsHardwareCursor));
	    break;

	default:
	    err = super::getAttribute( attribute, value );
    }

    return( err );
}

UInt32 IONDRVFramebuffer::getConnectionCount( void )
{
    VDMultiConnectInfoRec	multiConnection;

    multiConnection.csDisplayCountOrNumber = kGetConnectionCount;
    if( noErr == doStatus( cscGetMultiConnect, &multiConnection))
        return( multiConnection.csDisplayCountOrNumber );
    else
        return( 1 );
}

/*
    File:	DDCPowerOnOffUtils.c <CS3>
*/

enum{
    kVCPSendSize			= 8,
    kVCPReplySize			= 64,
    kI2CDisplayWriteAddress		= 0x6E,
    kI2CDisplayReadAddress		= 0x6F,
    // Messed up specification says checksum should be calculated with ACCESS.bus value of 50.
    kI2CDisplayReadHostCheckSumAddress	= 0x50,
    // Messed up specification says checksum should be calculated with ACCESS.bus value of 50.
    kI2CDisplayReadHostAddress		= 0x51,

    kI2CVCPGetCode			= 0x01,
    kI2CVCPGetLength			= 0x82,
    kI2CVCPGetMessageSize		= 0x05,

    kI2CVCPReplyLength			= 0x88,
    kI2CNullReplyLength			= 0x80,
    kI2CNullReplyCheckSum		= 0xBE,

    kI2CVCPSetCode			= 0x03,
    kI2CVCPSetLength			= 0x84,
    kI2CVCPReplyCode			= 0x02,
            
    kDDCPowerOn				= 0x01,
    kDDCPowerOff			= 0x04
};
enum {
    kBasicI2CCommTransactionsMask = ( (1<<kVideoNoTransactionType) | (1<<kVideoSimpleI2CType)
                                    | (1<<kVideoDDCciReplyType) )
};

void IONDRVFramebuffer::displayI2CPower( bool enable )
{
    VDCommunicationRec	i2CRecord;
    VDCommunicationInfoRec i2cInfoRecord;
    Byte		sendBuffer[8];
    Byte		replyBuffer[kVCPReplySize];
    UInt32		supportedCommFlags = 0;
    // Don't do it if we're told it's not supported
    bool		setThisDisplay = true;

    // 
    // Some displays (like Fiji) do not support the reading
    // of the current power state.  Others (like Mitsubishi
    // Diamond Pro 710) report that they do not support 
    // power management calls.
    //
    // I'll avoid sending the power command only in the 
    // case that I get a valid reply that does says 
    // it does not support the power selector.
    //

    bzero( &i2cInfoRecord, sizeof(i2cInfoRecord) );
    if( noErr != doStatus( cscGetCommunicationInfo, &i2cInfoRecord))
        return;
    if( kBasicI2CCommTransactionsMask != (i2cInfoRecord.csSupportedTypes & kBasicI2CCommTransactionsMask))
        return;

    supportedCommFlags = i2cInfoRecord.csSupportedCommFlags;
    bzero( &i2CRecord, sizeof(i2CRecord) );
    bzero( &sendBuffer, sizeof(sendBuffer) );
    bzero( &replyBuffer, sizeof(replyBuffer) );

    sendBuffer[0]	= kI2CDisplayReadHostAddress;
    sendBuffer[1]	= kI2CVCPGetLength;
    sendBuffer[2]	= kI2CVCPGetCode;		// GetVCP command
    sendBuffer[3]	= 0xD6;
    sendBuffer[4]	= kI2CDisplayWriteAddress ^
                            sendBuffer[0] ^ sendBuffer[1] ^
                            sendBuffer[2] ^ sendBuffer[3];

    i2CRecord.csBusID		= kVideoDefaultBus;
    i2CRecord.csSendType	= kVideoSimpleI2CType;
    i2CRecord.csSendAddress	= kI2CDisplayWriteAddress;
    i2CRecord.csSendBuffer	= &sendBuffer;
    i2CRecord.csSendSize	= 7;
    i2CRecord.csReplyType	= kVideoDDCciReplyType;
    i2CRecord.csReplyAddress	= kI2CDisplayReadAddress;
    i2CRecord.csReplyBuffer	= &replyBuffer;
    i2CRecord.csReplySize	= kVCPReplySize;

    if( supportedCommFlags & kVideoReplyMicroSecDelayMask )
    {
        // We know some displays are slow, this is an important call to get right
	i2CRecord.csCommFlags	|= kVideoReplyMicroSecDelayMask;
        // 50 milliseconds should be enough time for the display to respond.
	i2CRecord.csMinReplyDelay = 50 * 1000;
    }

    if( (noErr == doControl( cscDoCommunication, &i2CRecord))
      && (kI2CDisplayWriteAddress == replyBuffer[0])
      && (kI2CVCPReplyLength == replyBuffer[1])
      && (kI2CVCPReplyCode == replyBuffer[2])) {
	Byte checkSum = kI2CDisplayReadHostCheckSumAddress ^	// host address
            replyBuffer[0] ^	// source address
            replyBuffer[1] ^	// message length (0x88)
            replyBuffer[2] ^	// VCP type code
            replyBuffer[3] ^	// result code
            replyBuffer[4] ^	// VCP op code
            replyBuffer[5] ^	// VCP type code
            replyBuffer[6] ^	// Max value MSB
            replyBuffer[7] ^	// Max value LSB
            replyBuffer[8] ^	// Current value MSB
            replyBuffer[9];	// Current value LSB

        if( (checkSum == replyBuffer[10]) &&		// Did the check sum match AND
                        (0 != replyBuffer[3]))		// Are we not supposed to support this feature?
            setThisDisplay = false;			// Don't do it if we're told it's not supported
    }
        
    if( setThisDisplay) {

        bzero( &i2CRecord, sizeof(i2CRecord) );
        bzero( &sendBuffer, sizeof(sendBuffer) );
        bzero( &replyBuffer, sizeof(replyBuffer) );

        sendBuffer[0]	= kI2CDisplayReadHostAddress;
        sendBuffer[1]	= kI2CVCPSetLength;
        sendBuffer[2]	= kI2CVCPSetCode;			// SetVCP command
        sendBuffer[3]	= 0xD6;
        sendBuffer[4]	= 0;					// MSB
        sendBuffer[5]	= enable ? kDDCPowerOn : kDDCPowerOff;	// LSB
        sendBuffer[6]	= kI2CDisplayWriteAddress ^
                            sendBuffer[0] ^ sendBuffer[1] ^
                            sendBuffer[2] ^ sendBuffer[3] ^
                            sendBuffer[4] ^ sendBuffer[5];

        i2CRecord.csBusID		= kVideoDefaultBus;
        i2CRecord.csSendAddress		= kI2CDisplayWriteAddress;
        i2CRecord.csSendType		= kVideoSimpleI2CType;
        i2CRecord.csSendBuffer		= &sendBuffer;
        i2CRecord.csSendSize		= 7;
        i2CRecord.csReplyType		= kVideoNoTransactionType;
        i2CRecord.csReplyAddress	= 0;
        i2CRecord.csReplyBuffer		= 0;
        i2CRecord.csReplySize		= 0;

        if( supportedCommFlags & kVideoReplyMicroSecDelayMask) {
            // We know some displays are slow, this is an important call to get right
            i2CRecord.csCommFlags |= kVideoReplyMicroSecDelayMask;
            // 50 milliseconds should be enough time for the display to respond.
            i2CRecord.csMinReplyDelay	= 50 * 1000;
        }

        doControl( cscDoCommunication, &i2CRecord);
    }
}

IOReturn IONDRVFramebuffer::setAttributeForConnection( IOIndex connectIndex,
                                         IOSelect attribute, UInt32 info )
{
    IOReturn		err;
    VDSyncInfoRec	syncInfo;
    VDPowerStateRec	sleepInfo;
    UInt32		state;

    enum {		kNumStates = 4 };
    const UInt8		dpmsStates[kNumStates] =
        { kDPMSSyncOff, kDPMSSyncSuspend, kDPMSSyncStandby, kDPMSSyncOn };
    const UInt8		avStates[kNumStates] =
        { kAVPowerOff, kAVPowerSuspend, kAVPowerStandby, kAVPowerOn };

    switch( attribute ) {

        case kConnectionPower:

            state = info & (kNumStates - 1);
            super::setAttributeForConnection( connectIndex, attribute, info );

            // what are the sync-controlling capabilities of the ndrv?
            syncInfo.csMode = 0xff;
            err = doStatus( cscGetSync, &syncInfo );

            if( kIOReturnSuccess == err) {
                if( syncInfo.csMode & (1<<kNoSeparateSyncControlBit))
                    syncInfo.csMode = info ? kDPMSSyncOn : kDPMSSyncOff;
                else
                    syncInfo.csMode = dpmsStates[state];
                syncInfo.csFlags = kDPMSSyncMask;
                doControl( cscSetSync, &syncInfo);
            }

            if( kAVPowerOff != avStates[state]) {
                // the framebuffer setPowerState() will handle off
                sleepInfo.powerState     = avStates[state];
                sleepInfo.powerFlags     = 0;
                sleepInfo.powerReserved1 = 0;
                sleepInfo.powerReserved2 = 0;
                doControl( cscSetPowerState, &sleepInfo );
            }

            if( shouldDoI2CPower)
                displayI2CPower( (state == (kNumStates - 1)) ? true : false );

            err = kIOReturnSuccess;
            break;

        case kConnectionSyncEnable:
            syncInfo.csMode = (UInt8) (info >> 8);
            syncInfo.csFlags = (UInt8) (info & 0xFF);
            doControl( cscSetSync, &syncInfo);
            err = kIOReturnSuccess;
            break;

        default:
            err = super::setAttributeForConnection( connectIndex,
					attribute, info );
            break;
    }
    return( err );
}

            
IOReturn IONDRVFramebuffer::getAttributeForConnection( IOIndex connectIndex,
                                         IOSelect attribute, UInt32  * value )
{
    IOReturn		ret;
    VDSyncInfoRec	syncInfo;

    switch( attribute ) {

        case kConnectionSyncFlags:
            // find out current state of sync lines
            syncInfo.csMode = 0x00;
            doStatus( cscGetSync, &syncInfo);
            *value = syncInfo.csMode;
            ret = kIOReturnSuccess;
            break;

        case kConnectionSyncEnable:
            // what are the sync-controlling capabilities of the ndrv?
            syncInfo.csMode = 0xFF;
            doStatus( cscGetSync, &syncInfo);
            *value = (UInt32) syncInfo.csMode;
            ret = kIOReturnSuccess;
            break;

        case kConnectionSupportsHLDDCSense:
        case kConnectionSupportsAppleSense:
            ret = kIOReturnSuccess;
            break;

        case kConnectionFlags:
            VDMultiConnectInfoRec multiConnect;
            if( connectIndex == 0 )
                ret = doStatus( cscGetConnection, &multiConnect.csConnectInfo);
            else {
                multiConnect.csDisplayCountOrNumber = connectIndex;
                ret = doStatus( cscGetMultiConnect, &multiConnect);
            }
            if( kIOReturnSuccess == ret)
                *value = multiConnect.csConnectInfo.csConnectFlags;
            else
                *value = 0;
            break;

        case kConnectionEnable:

            VDResolutionInfoRec		info;
            VDTimingInfoRec		timingInfo;

            if( connectIndex == 0 )
                ret = doStatus( cscGetConnection, &multiConnect.csConnectInfo);
            else {
                multiConnect.csDisplayCountOrNumber = connectIndex;
                ret = doStatus( cscGetMultiConnect, &multiConnect);
            }
            if( (kIOReturnSuccess == ret) 
             && ((1 << kConnectionInactive) & multiConnect.csConnectInfo.csConnectFlags)) {
                *value = false;
                break;
            } else
                *value = true;

            info.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;
            while( 
                (noErr == doStatus( cscGetNextResolution, &info))
                && ((SInt32) info.csDisplayModeID > 0) ) {
            
                timingInfo.csTimingMode   = info.csDisplayModeID;
                timingInfo.csTimingFormat = kDeclROMtables;
                ret = doStatus( cscGetModeTiming, &timingInfo);

                if( (noErr == ret)
                 && (kDeclROMtables == timingInfo.csTimingFormat)
                 && (timingApple_0x0_0hz_Offline == timingInfo.csTimingData) 
                 && (kDisplayModeSafeFlag & timingInfo.csTimingFlags) ) {
                    *value = false;
                    break;
                }
                info.csPreviousDisplayModeID = info.csDisplayModeID;
            }
            ret = kIOReturnSuccess;
            break;

        case kConnectionChanged:

            UInt32 connectEnabled;
            ret = getAttributeForConnection( 0, kConnectionEnable, &connectEnabled );
            setDetailedTimings( 0 );
            ret = kIOReturnSuccess;

        default:
            ret = super::getAttributeForConnection( connectIndex,
				attribute, value );
            break;
    }

    return( ret );
}

IOReturn IONDRVFramebuffer::getAppleSense( IOIndex  connectIndex,
                                            UInt32 * senseType,
                                            UInt32 * primary,
                                            UInt32 * extended,
                                            UInt32 * displayType )
{
    OSStatus		err;
    VDMultiConnectInfoRec	multiConnect;
    UInt32			sense, extSense;

    if( connectIndex == 0 )
        err = doStatus( cscGetConnection, &multiConnect.csConnectInfo);
    else {
        multiConnect.csDisplayCountOrNumber = connectIndex;
        err = doStatus( cscGetMultiConnect, &multiConnect);
    }
    if( err)
	return( err);

    if( (primary || extended)
     && (0 == ((1<<kReportsTagging) & multiConnect.csConnectInfo.csConnectFlags)))

	err = kIOReturnUnsupported;

    else {

        sense 		= multiConnect.csConnectInfo.csConnectTaggedType;
        extSense 	= multiConnect.csConnectInfo.csConnectTaggedData;
	// bug fixes for really old ATI driver
        if( sense == 0) {
            if( extSense == 6) {
                sense          	= kRSCSix;
                extSense        = kESCSixStandard;
            } else if( extSense == 4) {
                sense		= kRSCFour;
                extSense        = kESCFourNTSC;
            }
        }
        if( primary)
            *primary = sense;
        if( extended)
            *extended = extSense;
        if( senseType)
            *senseType = (0 != ((1<<kTaggingInfoNonStandard) & multiConnect.csConnectInfo.csConnectFlags));
        if( displayType)
            *displayType = multiConnect.csConnectInfo.csDisplayType;
    }
    return( err);
}

IOReturn IONDRVFramebuffer::connectFlags( IOIndex /* connectIndex */,
                             IODisplayModeID displayMode, IOOptionBits * flags )
{
    VDTimingInfoRec		timingInfo;
    OSStatus			err;

    timingInfo.csTimingMode = displayMode;
    // in case the driver doesn't do it:
    timingInfo.csTimingFormat = kDeclROMtables;
    err = doStatus( cscGetModeTiming, &timingInfo);

    if( kDetailedTimingFormat == timingInfo.csTimingFormat)
        *flags = kDisplayModeValidFlag | kDisplayModeSafeFlag;
    else
        *flags = timingInfo.csTimingFlags;

    return( err );
}


bool IONDRVFramebuffer::hasDDCConnect( IOIndex  connectIndex )
{
    OSStatus		err;
    VDMultiConnectInfoRec	multiConnect;
    enum		{	kNeedFlags = (1<<kReportsDDCConnection)
					   | (1<<kHasDDCConnection) };
    if( connectIndex == 0 )
        err = doStatus( cscGetConnection, &multiConnect.csConnectInfo);
    else {
        multiConnect.csDisplayCountOrNumber = connectIndex;
        err = doStatus( cscGetMultiConnect, &multiConnect);
    }
    if( err)
        return( err);

    return( (multiConnect.csConnectInfo.csConnectFlags & kNeedFlags)
		== kNeedFlags );
}

// I2C first year for Apple displays.
// Apple monitors older than this (and Manta)
// are never called with I2C commands
enum {
    kFirstAppleI2CYear	= 1999,
    kAppleVESAVendorID	= 0x0610
};

struct EDID {
    UInt8	header[8];
    UInt8	vendorProduct[4];
    UInt8	serialNumber[4];
    UInt8	weekOfManufacture;
    UInt8	yearOfManufacture;
    UInt8	version;
    UInt8	revision;
    UInt8	displayParams[5];
    UInt8	colorCharacteristics[10];
    UInt8	establishedTimings[3];
    UInt16	standardTimings[8];
    UInt8	descriptors[4][18];
    UInt8	extension;
    UInt8	checksum;
};

static bool IsApplePowerBlock(UInt8 * theBlock)
{    
    return( theBlock &&
            0x00000000		== *(UInt32 *)&theBlock[0] &&
            0x00		== theBlock[4] &&
            0x06		== theBlock[5] &&
            0x10		== theBlock[6] );
}

IOReturn IONDRVFramebuffer::getDDCBlock( IOIndex /* connectIndex */, 
					UInt32 blockNumber,
                                        IOSelect blockType,
					IOOptionBits options,
                                        UInt8 * data, IOByteCount * length )

{
    OSStatus		err = 0;
    VDDDCBlockRec	ddcRec;
    ByteCount		actualLength = *length;

    ddcRec.ddcBlockNumber 	= blockNumber;
    ddcRec.ddcBlockType 	= blockType;
    ddcRec.ddcFlags 		= options;

    err = doStatus( cscGetDDCBlock, &ddcRec);

    if( err == noErr) {

	if( actualLength < kDDCBlockSize)
            actualLength = actualLength;
	else
            actualLength = kDDCBlockSize;
        bcopy( ddcRec.ddcBlockData, data, actualLength);
	*length = actualLength;

        if( (1 == blockNumber) && (kIODDCBlockTypeEDID == blockType)
         && (actualLength >= sizeof( EDID))) do {

            EDID * edid;
            UInt32 vendor;
            UInt32 product;

            edid = (EDID *) data;
            vendor = (edid->vendorProduct[0] << 8) | edid->vendorProduct[1];
            product = (edid->vendorProduct[3] << 8) | edid->vendorProduct[2];
            if( kAppleVESAVendorID == vendor) {
                if( (0x01F4 == product) || (0x9D02 == product) || (0x9216 == product))
                    continue;
                if( edid->yearOfManufacture && ((edid->yearOfManufacture + 1990) < kFirstAppleI2CYear))
                    continue;
            }
            shouldDoI2CPower =    (IsApplePowerBlock( &edid->descriptors[1][0])
                                || IsApplePowerBlock( &edid->descriptors[2][0])
                                || IsApplePowerBlock( &edid->descriptors[3][0]));

        } while( false );
    }

    IOLog("%s: i2cPower %d\n", getName(), shouldDoI2CPower);

    return( err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IONDRVFramebuffer::initForPM( void )
{
    IOPMPowerState powerStates[ kIONDRVFramebufferPowerStateCount ] = {
      // version,
      // capabilityFlags, outputPowerCharacter, inputPowerRequirement,
      { 1, 0,		     0,	          0,	       0, 0, 0, 0, 0, 0, 0, 0 },
      { 1, 0,                0,           IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
      // staticPower, unbudgetedPower, powerToAttain, timeToAttain, settleUpTime, 
      // timeToLower, settleDownTime, powerDomainBudget
    };
    VDPowerStateRec sleepInfo;
    IOReturn err;
    bool dozeOnly;

    dozeOnly = getPlatform()->hasPrivPMFeature( kPMHasLegacyDesktopSleepMask );
    if( !dozeOnly && getPlatform()->hasPMFeature( kPMCanPowerOffPCIBusMask )) {
        sleepInfo.powerState = 0;
        sleepInfo.powerFlags = 0;
        sleepInfo.powerReserved1 = 0;
        sleepInfo.powerReserved2 = 0;
        // can this ndrv power off?
        err = doStatus( cscGetPowerState, &sleepInfo);
        dozeOnly = ((kIOReturnSuccess != err)
                 || (0 == (kPowerStateSleepCanPowerOffMask & sleepInfo.powerFlags)));
    }

    if( dozeOnly) {
        powerStates[kNDRVFramebufferSleepState].capabilityFlags |= kIOPMPreventSystemSleep;
        powerStates[kNDRVFramebufferDozeState].capabilityFlags  |= kIOPMPreventSystemSleep;
        powerStates[kNDRVFramebufferWakeState].capabilityFlags  |= kIOPMPreventSystemSleep;
    }

    // register ourselves with superclass policy-maker
    registerPowerDriver( this, powerStates, kIONDRVFramebufferPowerStateCount );
    // no sleep until children
    temporaryPowerClampOn();
    // not below doze until system sleep
    changePowerStateTo( kNDRVFramebufferDozeState );
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// This simple device needs only power.  If the power domain is supplying
// power, the frame buffer can be on.  If there is no power it can only be off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned long IONDRVFramebuffer::maxCapabilityForDomainState(
					IOPMPowerFlags domainState )
{
   if( domainState & IOPMPowerOn)
       return( kIONDRVFramebufferPowerStateMax );
   else
       return( kNDRVFramebufferSleepState );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned long IONDRVFramebuffer::initialPowerStateForDomainState(
					 IOPMPowerFlags domainState )
{
   if( domainState & IOPMPowerOn)
       return( kIONDRVFramebufferPowerStateMax );
   else
       return( kNDRVFramebufferSleepState );
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// powerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned long IONDRVFramebuffer::powerStateForDomainState( 
                                            IOPMPowerFlags domainState )
{
   if( domainState & IOPMPowerOn)
       return( pm_vars->myCurrentState );
   else
       return( kNDRVFramebufferSleepState );
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum {
    cscSleepWake = 0x86,
    sleepWakeSig = 'slwk',
    vdSleepState = 0,
    vdWakeState  = 1
};
#pragma options align=mac68k

struct VDSleepWakeInfo {
    UInt8	csMode;
    UInt8	fill;
    UInt32	csData;
};
typedef struct VDSleepWakeInfo VDSleepWakeInfo;

#pragma options align=reset

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// ndrvSetPowerState
//
// Called by the superclass to turn the frame buffer on and off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::ndrvSetPowerState( UInt32 newState )
{
    static const unsigned long
    // [sleep][fromState][toState]
    states[2][kIONDRVFramebufferPowerStateCount][kIONDRVFramebufferPowerStateCount] = {
     {
	{ 0, kAVPowerOff, kAVPowerOn },
	{ kAVPowerOff, 0, kAVPowerOn },
	{ kAVPowerOff, kAVPowerOff, 0 },
     }, {
	{ 0, kHardwareWakeToDoze, kHardwareWake },
	{ kHardwareSleep, 0, kAVPowerOn },
	{ kHardwareSleep, kAVPowerSuspend, 0 }
     }
    };

    IOReturn		err;
    UInt32		sleep = 0;
    UInt32		ndrvPowerState;
    UInt32		oldState;
    IOIndex		postEvent = 0;
    IOAGPDevice *	agpDev;
    IODTPlatformExpert * pe;

    if( newState == powerState)
        return( kIOReturnSuccess );

    if( newState > kIONDRVFramebufferPowerStateMax)
        newState = kIONDRVFramebufferPowerStateMax;

    oldState = powerState;

    if( kIONDRVFramebufferPowerStateMax == oldState) {
        super::handleEvent( kIOFBNotifyWillPowerOff );
        postEvent = kIOFBNotifyDidPowerOff;
    } else if( kIONDRVFramebufferPowerStateMax == newState) {
        super::handleEvent( kIOFBNotifyWillPowerOn );
        postEvent = kIOFBNotifyDidPowerOn;
    }

    if( kNDRVFramebufferSleepState == newState) {
        IOMemoryDescriptor * vram;
        if( (vram = getVRAMRange())) {
            vram->redirect( kernel_task, true );
            vram->release();
        }
    }

    if( platformSleep
     && (pe = OSDynamicCast(IODTPlatformExpert, getPlatform()))
     && (pe->getChipSetType() < kChipSetTypeCore99)) {

        VDSleepWakeInfo sleepInfo;

        ndrvPowerState = newState ? vdWakeState : vdSleepState;

        err = doStatus( cscSleepWake, &sleepInfo);

        powerState = newState;

        if( (kIOReturnSuccess == err) && (sleepWakeSig == sleepInfo.csData)
            && (ndrvPowerState != sleepInfo.csMode)) {

            sleepInfo.csMode = ndrvPowerState;
            ignore_zero_fault( true );
            err = doControl( cscSleepWake, &sleepInfo);
            ignore_zero_fault( false );
        }

    } else {

        VDPowerStateRec	sleepInfo;

        sleepInfo.powerState = 0;
        sleepInfo.powerFlags = 0;
        sleepInfo.powerReserved1 = 0;
        sleepInfo.powerReserved2 = 0;
    
        err = doStatus( cscGetPowerState, &sleepInfo);

        if( (kIOReturnSuccess == err)
        && ((kPowerStateSleepCanPowerOffMask & sleepInfo.powerFlags)
        || platformSleep))
            sleep = 1;
    
        ndrvPowerState = states[sleep][oldState][newState];
    
        if( (kHardwareWakeToDoze == ndrvPowerState)
        && (0 == (kPowerStateSleepWaketoDozeMask & sleepInfo.powerFlags)))
            ndrvPowerState = kHardwareWake;
    
#if DEBUG
        IOLog("ndrv powerFlags %08lx, state->%02lx\n", sleepInfo.powerFlags, ndrvPowerState);
#endif
    
        powerState = newState;
    
        if( (kIOReturnSuccess != err) || (sleepInfo.powerState != ndrvPowerState)) {
            sleepInfo.powerState = ndrvPowerState;
            sleepInfo.powerFlags = 0;
            sleepInfo.powerReserved1 = 0;
            sleepInfo.powerReserved2 = 0;
    
            ignore_zero_fault( true );
            err = doControl( cscSetPowerState, &sleepInfo);
            ignore_zero_fault( false );
        }
    }

    agpDev = OSDynamicCast(IOAGPDevice, device);

    if( kIONDRVFramebufferPowerStateMax == newState) {

        UInt32 isOnline;
        if( kIOReturnSuccess != getAttributeForConnection( 0,
                                            kConnectionEnable, &isOnline ))
            isOnline = true;
        if( isOnline != online) {
            online = isOnline;
//            vramMemory = findVRAM();
            if( online)
                getCurrentConfiguration();
        }
        if( agpDev)
            agpDev->resetAGP();
    }

    if( kNDRVFramebufferSleepState == oldState) {
        IOMemoryDescriptor * vram;
        if( (vram = getVRAMRange())) {
            vram->redirect( kernel_task, false );
            vram->release();
        }
    }

    if( postEvent)
        super::handleEvent( postEvent );

    IONDRVFramebuffer * other;
    if( (other = OSDynamicCast( IONDRVFramebuffer, nextDependent))) {
        other->ndrvSetPowerState( newState );
    }

    return( kIOReturnSuccess );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// ATI patches.

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IONDRVFramebuffer

OSDefineMetaClassAndStructors(IOATINDRV, IONDRVFramebuffer)
OSDefineMetaClassAndStructors(IOATI128NDRV, IOATINDRV)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOATINDRV::getStartupDisplayMode(
				IODisplayModeID * displayMode, IOIndex * depth )
{
    UInt16 *		nvram;
    OSData *		prop;

    prop = OSDynamicCast( OSData, nub->getProperty("Sime"));
    if( prop) {
	nvram = (UInt16 *) prop->getBytesNoCopy();
	*displayMode = nvram[ 0 ];	// 1 is physDisplayMode
	*depth = nvram[ 2 ] - kFirstDepth;
        return( kIOReturnSuccess);
    } else
        return(super::getStartupDisplayMode( displayMode, depth));
}

IODeviceMemory * IOATINDRV::findVRAM( void )
{
    OSData *		prop;
    IOByteCount *	lengths;
    IOIndex		count;
    IOPhysicalAddress	vramBase;
    IOByteCount		vramLength;

    prop = OSDynamicCast( OSData, nub->getProperty("ATY,memsize"));
    if( !prop)
	return( super::findVRAM());

    lengths = (IOByteCount *) prop->getBytesNoCopy();
    count = prop->getLength() / sizeof(IOByteCount);

    prop = OSDynamicCast( OSData, nub->getProperty("ATY,Base"));

    if( prop && (count > 1)) {
        vramBase =  *((IOPhysicalAddress *)prop->getBytesNoCopy());
        vramBase = physicalFramebuffer;
        vramLength = lengths[1];
        //
        if( (!lengths[0]) && (0 != (vramBase & (vramLength >> 1)))) {
            if( vramBase & 0x02000000)
                vramBase &= ~0x02000000;
            else
                vramBase &= ~0x04000000;
        }
        //
        vramBase &= ~(vramLength - 1);
    } else {
        vramBase = physicalFramebuffer;
        vramLength = lengths[0];
    }

    if( !vramLength)
        return( super::findVRAM());

    vramLength = (vramLength + (vramBase & 0xffff)) & 0xffff0000;
    vramBase &= 0xffff0000;

    IOLog("%lx: VRAM found %08lx:%08lx\n", physicalFramebuffer, vramBase, vramLength);

    return( makeSubRange( vramBase, vramLength ));
}

static int g128ExtraCurs = 8;
static int g128DeltaCurs = 0x25c0;

void IOATI128NDRV::flushCursor( void )
{
    volatile UInt32 *	fb;
    UInt32		x;
    int			i;

    fb = (volatile UInt32 *) frameBuffer;
    for( i = 0; i < g128ExtraCurs; i++) {
	x += *(fb++);
	fb += g128DeltaCurs;
    }
}


    
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 0);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 1);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 2);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 3);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 4);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 5);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 6);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 7);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 8);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 9);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 10);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 11);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 12);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 13);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 14);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 15);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 16);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 17);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 18);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 19);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 20);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 21);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 22);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 23);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 24);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 25);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 26);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 27);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 28);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 29);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 30);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 31);
