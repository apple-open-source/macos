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

extern "C" {
#include <pexpert/pexpert.h>
};

#include "IONDRV.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndAbstractStructors(IONDRV, OSObject)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if __ppc__

#include "IOPEFLoader.h"

#define LOG		if(1) kprintf

#define super IONDRV
OSDefineMetaClassAndStructors(IOPEFNDRV, IONDRV)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IONDRV * IOPEFNDRV::instantiate( IORegistryEntry * regEntry,
                              IOLogicalAddress container,
                              IOByteCount containerSize,
                              IONDRVUndefinedSymbolHandler undefHandler,
                              void * self )
{
    OSStatus	err = 1;
    IOPEFNDRV *	inst;

    inst = new IOPEFNDRV;

    if( inst) do {
	if( false == inst->init())
	    continue;

        err = PCodeOpen( (void *)container, containerSize, &inst->fPEFInst );
        if( err)
	    continue;

        err = PCodeInstantiate( inst->fPEFInst, undefHandler, self );
        if( err)
	    continue;

	inst->getSymbol( "DoDriverIO", (IOLogicalAddress *) &inst->fDoDriverIO );
	if( kIOReturnSuccess == inst->getSymbol( "TheDriverDescription", 
				(IOLogicalAddress *) &inst->fDriverDesc )) {

            char * 	name;
            IOByteCount	plen;

            name = (char *) inst->fDriverDesc->driverOSRuntimeInfo.driverName;
            plen = name[ 0 ];
            if( plen >= sizeof(inst->fDriverDesc->driverOSRuntimeInfo.driverName))
                plen = sizeof(inst->fDriverDesc->driverOSRuntimeInfo.driverName) - 1;
            strncpy( inst->fName, name + 1, plen);
            sprintf( inst->fName + plen, "-%08lx", *((UInt32 *) &inst->fDriverDesc->driverType.version));
	}

    } while( false);

    if( inst && err) {
	inst->release();
	inst = 0;
    }

    return( inst );
}

void IOPEFNDRV::free( void )
{
    if( fPEFInst)
        PCodeClose( fPEFInst );
    super::free();
}

IOReturn IOPEFNDRV::getSymbol( const char * symbolName,
				IOLogicalAddress * address )
{
    OSStatus            err;

    err = PCodeFindExport( fPEFInst, symbolName,
				(LogicalAddress *)address, NULL );
    if( err)
	*address = 0;

    return( err);
}

IOReturn _IONDRVLibrariesInitialize( IOService * provider );
// osfmk/ppc/mappings.h
extern "C" void ignore_zero_fault(boolean_t);

IOReturn IOPEFNDRV::doDriverIO( UInt32 commandID, void * contents,
				UInt32 commandCode, UInt32 commandKind )
{
    OSStatus			err = kIOReturnSuccess;
    struct DriverInitInfo	initInfo;
    CntrlParam *         	pb;

    if( 0 == fDoDriverIO)
	return( kIOReturnUnsupported );

    switch( commandCode )
    {
        case kIONDRVInitializeCommand:
            err = _IONDRVLibrariesInitialize( (IOService *) contents );
            if( kIOReturnSuccess != err)
                break;
            /* fall thru */

        case kIONDRVFinalizeCommand:
        case kIONDRVReplaceCommand:
        case kIONDRVSupersededCommand:
            initInfo.refNum = 0xffff & ((UInt32) this);
            MAKE_REG_ENTRY(initInfo.deviceEntry, contents)
            contents = &initInfo;
            break;

        case kIONDRVControlCommand:
        case kIONDRVStatusCommand:
        case kIONDRVOpenCommand:
        case kIONDRVCloseCommand:
        case kIONDRVReadCommand:
        case kIONDRVWriteCommand:
        case kIONDRVKillIOCommand:

            pb = (CntrlParam *) contents;
            pb->qLink = 0;
            pb->ioCRefNum = 0xffff & ((UInt32) this);
            break;
    }

    if( kIOReturnSuccess == err) {
        commandCode -= kIONDRVOpenCommand - kOpenCommand;

        ignore_zero_fault( true );
        err = CallTVector( /*AddressSpaceID*/ 0, (void *)commandID, contents,
                    (void *)commandCode, (void *)commandKind, /*p6*/ 0,
                    fDoDriverIO );
        ignore_zero_fault( false );
    }

    return( err);
}

IONDRV * IOPEFNDRV::fromRegistryEntry( IORegistryEntry * regEntry,
                                        IONDRVUndefinedSymbolHandler handler,
                                        void * self )
{
    IOLogicalAddress	pef = 0;
    IOByteCount		propSize = 0;
    OSData *		prop;
    IONDRV *		inst;

    inst = (IONDRV *) regEntry->getProperty("AAPL,ndrvInst");
    if( inst) {
	inst->retain();
	return( inst );
    }

    prop = (OSData *) regEntry->getProperty( "driver,AAPL,MacOS,PowerPC" );
    if( prop) {
        pef = (IOLogicalAddress) prop->getBytesNoCopy();
	propSize = prop->getLength();
    }

    if( pef) {
        kprintf( "pef = %08x, %08x\n", pef, propSize );
	inst = IOPEFNDRV::instantiate( regEntry, pef, propSize, handler, self );
	if( inst )
            regEntry->setProperty( "AAPL,ndrvInst", inst);

    } else
	inst = 0;

    return( inst );
}

const char * IOPEFNDRV::driverName( void )
{
    return( fName );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#else  /* __ppc__ */

IONDRV * IOPEFNDRV::fromRegistryEntry( IORegistryEntry * regEntry,
                                        IONDRVUndefinedSymbolHandler handler,
                                        void * self )
{
    return( 0 );
}

#endif /* !__ppc__ */
