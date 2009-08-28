/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 27 April 99 wgulland created.
 *
 */

#import <IOKit/firewire/IOFireWireBus.h>
#import <IOKit/firewire/IOFWDCLPool.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndAbstractStructors(IOFireWireBusAux, OSObject);

OSMetaClassDefineReservedUsed(IOFireWireBusAux, 0);			// createDCLPool
OSMetaClassDefineReservedUsed(IOFireWireBusAux, 1);			// removed 101007
OSMetaClassDefineReservedUsed(IOFireWireBusAux, 2);			// getMaxRec
OSMetaClassDefineReservedUsed(IOFireWireBusAux, 3);			// getSessionRefExporter
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 4);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 5);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 6);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 7);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 8);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 9);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 10);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 11);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 12);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 13);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 14);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 15);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 16);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 17);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 18);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 19);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 20);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 21);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 22);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 23);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 24);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 25);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 26);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 27);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 28);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 29);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 30);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 31);
OSMetaClassDefineReservedUnused(IOFireWireBusAux, 32);

#pragma mark -

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOFireWireBus, IOService )
OSDefineAbstractStructors(IOFireWireBus, IOService)
//OSMetaClassDefineReservedUnused(IOFireWireBus, 0);
//OSMetaClassDefineReservedUnused(IOFireWireBus, 1);
//OSMetaClassDefineReservedUnused(IOFireWireBus, 2);
//OSMetaClassDefineReservedUnused(IOFireWireBus, 3);
//OSMetaClassDefineReservedUnused(IOFireWireBus, 4);
//OSMetaClassDefineReservedUnused(IOFireWireBus, 5);
//OSMetaClassDefineReservedUnused(IOFireWireBus, 6);
//OSMetaClassDefineReservedUnused(IOFireWireBus, 7);
