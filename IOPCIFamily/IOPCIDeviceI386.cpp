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
 * 23 Nov 98 sdouglas created from objc version.
 */
 
#ifdef __i386__
 
#include <IOKit/system.h>

#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOLib.h>
#include <IOKit/assert.h>

#include <libkern/c++/OSContainers.h>

//#include <architecture/i386/pio.h>
#warning Should be including these definitions from the Kernel.framework
#ifndef I386_PIO_H
#define I386_PIO_H
//#include <cpus.h>
//#include <mach_assert.h>
#define MACH_ASSERT 0

typedef unsigned short i386_ioport_t;

/* read a longword */
extern unsigned long    inl(
                                i386_ioport_t   port);
/* read a shortword */
extern unsigned short   inw(
                                i386_ioport_t   port);
/* read a byte */
extern unsigned char    inb(
                                i386_ioport_t   port);
/* write a longword */
extern void             outl(
                                i386_ioport_t   port,
                                unsigned long   datum);
/* write a word */
extern void             outw(
                                i386_ioport_t   port,
                                unsigned short  datum);
/* write a longword */
extern void             outb(
                                i386_ioport_t   port,
                                unsigned char   datum);

/* input an array of longwords */
extern void             linl(
                                i386_ioport_t   port,
                                int             * data,
                                int             count);
/* output an array of longwords */
extern void             loutl(
                                i386_ioport_t   port,
                                int             * data,
                                int             count);

/* input an array of words */
extern void             linw(
                                i386_ioport_t   port,
                                int             * data,
                                int             count);
/* output an array of words */
extern void             loutw(
                                i386_ioport_t   port,
                                int             * data,
                                int             count);

/* input an array of bytes */
extern void             linb(
                                i386_ioport_t   port,
                                char            * data,
                                int             count);
/* output an array of bytes */
extern void             loutb(
                                i386_ioport_t   port,
                                char            * data,
                                int             count);

#if defined(__GNUC__) && (!MACH_ASSERT)
extern __inline__ unsigned long inl(
                                i386_ioport_t port)
{
        unsigned long datum;
        __asm__ volatile("inl %1, %0" : "=a" (datum) : "d" (port));
        return(datum);
}

extern __inline__ unsigned short inw(
                                i386_ioport_t port)
{
        unsigned short datum;
        __asm__ volatile(".byte 0x66; inl %1, %0" : "=a" (datum) : "d" (port));
        return(datum);
}

extern __inline__ unsigned char inb(
                                i386_ioport_t port)
{
        unsigned char datum;
        __asm__ volatile("inb %1, %0" : "=a" (datum) : "d" (port));
        return(datum);
}

extern __inline__ void outl(
                                i386_ioport_t port,
                                unsigned long datum)
{
        __asm__ volatile("outl %0, %1" : : "a" (datum), "d" (port));
}

extern __inline__ void outw(
                                i386_ioport_t port,
                                unsigned short datum)
{
        __asm__ volatile(".byte 0x66; outl %0, %1" : : "a" (datum), "d" (port));
}

extern __inline__ void outb(
                                i386_ioport_t port,
                                unsigned char datum)
{
        __asm__ volatile("outb %0, %1" : : "a" (datum), "d" (port));
}
#endif /* defined(__GNUC__) && (!MACH_ASSERT) */
#endif /* I386_PIO_H */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UInt32 IOPCIDevice::ioRead32( UInt16 offset, IOMemoryMap * map = 0 )
{
    UInt32	value;

    if( 0 == map)
	map = ioMap;

    value = inl( map->getPhysicalAddress() + offset );

    return( value );
}

UInt16 IOPCIDevice::ioRead16( UInt16 offset, IOMemoryMap * map = 0 )
{
    UInt16	value;

    if( 0 == map)
	map = ioMap;

    value = inw( map->getPhysicalAddress() + offset );

    return( value );
}

UInt8 IOPCIDevice::ioRead8( UInt16 offset, IOMemoryMap * map = 0 )
{
    UInt32	value;

    if( 0 == map)
	map = ioMap;

    value = inb( map->getPhysicalAddress() + offset );

    return( value );
}

void IOPCIDevice::ioWrite32( UInt16 offset, UInt32 value,
				IOMemoryMap * map = 0 )
{
    if( 0 == map)
	map = ioMap;

    outl( map->getPhysicalAddress() + offset, value );
}

void IOPCIDevice::ioWrite16( UInt16 offset, UInt16 value,
				IOMemoryMap * map = 0 )
{
    if( 0 == map)
	map = ioMap;

    outw( map->getPhysicalAddress() + offset, value );
}

void IOPCIDevice::ioWrite8( UInt16 offset, UInt8 value,
				IOMemoryMap * map = 0 )
{
    if( 0 == map)
	map = ioMap;

    outb( map->getPhysicalAddress() + offset, value );
}


#endif // __i386__