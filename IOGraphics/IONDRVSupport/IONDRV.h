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

#define MAKE_REG_ENTRY(regEntryID, obj)                          	   \
        (regEntryID)->opaque[0] = (void *) (((uintptr_t)obj)  - ((uintptr_t)gIOFramebufferKey)); \
        (regEntryID)->opaque[1] = (void *) ~(((uintptr_t)obj) - ((uintptr_t)gIOFramebufferKey)); \
        (regEntryID)->opaque[2] = (void *) 0x53696d65;                  \
        (regEntryID)->opaque[3] = (void *) 0x52756c7a;

#define REG_ENTRY_TO_OBJ_RET(regEntryID, obj, ret)                      \
        uintptr_t __obj;												\
        if ((__obj = ((uintptr_t *)regEntryID)[0])  			        \
             != ~((uintptr_t *)regEntryID)[1])        	return (ret);	\
        obj = (IORegistryEntry *)(__obj + (uintptr_t)gIOFramebufferKey);

#define REG_ENTRY_TO_OBJ(regEntryID, obj) 	REG_ENTRY_TO_OBJ_RET((regEntryID), (obj), -2538)

#define REG_ENTRY_TO_PT(regEntryID,obj)                                 \
        IORegistryEntry * obj;                                          \
		REG_ENTRY_TO_OBJ_RET((regEntryID), (obj), -2538)

#define REG_ENTRY_TO_SERVICE(regEntryID, type, obj)                     \
        IORegistryEntry * regEntry;                                     \
        type            * obj;                                          \
		REG_ENTRY_TO_OBJ(regEntryID, regEntry);							\
        if (0 == (obj = OSDynamicCast(type, regEntry)))                 \
            return (-2542);

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

#endif /* __IONDRV__ */

