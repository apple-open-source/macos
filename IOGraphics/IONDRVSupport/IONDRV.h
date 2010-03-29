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


#ifndef __IONDRV__
#define __IONDRV__

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOInterruptEventSource.h>

#include <IOKit/ndrvsupport/IOMacOSTypes.h>
#include <IOKit/ndrvsupport/IONDRVSupport.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef kAAPLRegEntryIDKey
#define kAAPLRegEntryIDKey      "AAPL,RegEntryID"
#endif

#ifndef kAAPLDisableMSIKey
#define kAAPLDisableMSIKey      "AAPL,DisableMSI"
#endif

#define MAKE_REG_ENTRY(regEntryID,obj)                          \
        (regEntryID)->opaque[ 0 ] = (void *) obj;                       \
        (regEntryID)->opaque[ 1 ] = (void *) ~(uintptr_t)obj;           \
        (regEntryID)->opaque[ 2 ] = (void *) 0x53696d65;                \
        (regEntryID)->opaque[ 3 ] = (void *) 0x52756c7a;

#define REG_ENTRY_TO_OBJ(regEntryID,obj)                                \
        if( (uintptr_t)((obj = ((IORegistryEntry **)regEntryID)[ 0 ]))  \
         != ~((uintptr_t *)regEntryID)[ 1 ] )                           \
            return( -2538);

#define REG_ENTRY_TO_OBJ_RET(regEntryID,obj,ret)                        \
        if( (uintptr_t)((obj = ((IORegistryEntry **)regEntryID)[ 0 ]))  \
         != ~((uintptr_t *)regEntryID)[ 1 ] )                           \
            return( ret);

#define REG_ENTRY_TO_PT(regEntryID,obj)                                 \
        IORegistryEntry * obj;                                          \
        if( (uintptr_t)((obj = ((IORegistryEntry **)regEntryID)[ 0 ]))  \
         != ~((uintptr_t *)regEntryID)[ 1 ] )                           \
            return( -2538);

#define REG_ENTRY_TO_SERVICE(regEntryID,type,obj)                       \
        IORegistryEntry * regEntry;                                     \
        type            * obj;                                          \
        if( (uintptr_t)((regEntry = ((IORegistryEntry **)regEntryID)[ 0 ])) \
         != ~((uintptr_t *)regEntryID)[ 1 ] )                           \
            return( -2538);                                             \
        if( 0 == (obj = OSDynamicCast( type, regEntry)))                \
            return( -2542);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IONDRV : public OSObject
{
    OSDeclareAbstractStructors(IONDRV)

public:
    virtual IOReturn getSymbol( const char * symbolName,
                                IOLogicalAddress * address ) = 0;

    virtual const char * driverName( void ) = 0;

    virtual IOReturn doDriverIO( UInt32 commandID, void * contents,
                                 UInt32 commandCode, UInt32 commandKind ) = 0;
};

#if __ppc__

#if VERSION_MAJOR <= 9
#define CREATE_PEF_KMOD 1
#else
#define CREATE_PEF_KMOD 0
#endif

class IOPEFNDRV : public IONDRV
{
    OSDeclareDefaultStructors(IOPEFNDRV)

private:
    void *                      fPEFInst;
    struct IOTVector *          fDoDriverIO;
    struct DriverDescription *  fDriverDesc;
    char                        fName[64];
#if CREATE_PEF_KMOD
    kmod_t                      fKModID;
#endif

public:
    static void initialize( void );
    static IOPEFNDRV * instantiate( IORegistryEntry * regEntry,
                                 IOLogicalAddress container,
                                 IOByteCount containerSize,
                                 bool checkDate,
                                 IONDRVUndefinedSymbolHandler handler,
                                 void * self );

    static IOPEFNDRV * fromRegistryEntry( IORegistryEntry * regEntry, OSData * newPEF,
                                        IONDRVUndefinedSymbolHandler handler, void * self);

    virtual void free( void );

    virtual IOReturn getSymbol( const char * symbolName,
                                IOLogicalAddress * address );

    virtual const char * driverName( void );

    virtual IOReturn doDriverIO( UInt32 commandID, void * contents,
                                 UInt32 commandCode, UInt32 commandKind );
};

#endif /* __ppc__ */

struct IONDRVInterruptSource {
    void *      refCon;
    IOTVector   handlerStore;
    IOTVector   enablerStore;
    IOTVector   disablerStore;
    IOTVector * handler;
    IOTVector * enabler;
    IOTVector * disabler;
    bool        registered;
    bool        enabled;
};

class IONDRVInterruptSet : public OSObject {

    OSDeclareDefaultStructors(IONDRVInterruptSet)

public:
    IOService *                 provider;
    IOOptionBits                options;
    UInt32                      count;
    SInt32                      providerInterruptSource;
    IONDRVInterruptSource *     sources;
    IONDRVInterruptSet *        child;

    static IONDRVInterruptSet * with(IOService * provider,
                                    IOOptionBits options, SInt32 count);
    void free();
};

extern "C" OSStatus
CallTVector( 
            void * p1, void * p2, void * p3, void * p4, void * p5, void * p6,
            struct IOTVector * entry );

#endif /* __IONDRV__ */

