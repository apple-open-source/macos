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

#include <IOKit/storage/IODVDBlockStorageDriver.h>
#include <IOKit/storage/IODVDMedia.h>

#define	super IOMedia
OSDefineMetaClassAndStructors(IODVDMedia, IOMedia)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IODVDBlockStorageDriver * IODVDMedia::getProvider() const
{
    //
    // Obtain this object's provider.   We override the superclass's method to
    // return a more specific subclass of IOService -- IODVDBlockStorageDriver.
    // This method serves simply as a convenience to subclass developers.
    //

    return (IODVDBlockStorageDriver *) IOService::getProvider();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IODVDMedia::reportKey( IOMemoryDescriptor * buffer,
                                const DVDKeyClass    keyClass,
                                const UInt32         address,
                                const UInt8          grantID,
                                const DVDKeyFormat   format )
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    if (buffer == 0 && format != kDVDKeyFormatAGID_Invalidate)
    {
        return kIOReturnBadArgument;
    }

    return getProvider()->reportKey( /* buffer   */ buffer,
                                     /* keyClass */ keyClass,
                                     /* address  */ address,
                                     /* grantID  */ grantID,
                                     /* format   */ format );
}

OSMetaClassDefineReservedUsed(IODVDMedia, 0);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IODVDMedia::sendKey( IOMemoryDescriptor * buffer,
                              const DVDKeyClass    keyClass,
                              const UInt8          grantID,
                              const DVDKeyFormat   format )
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    if (buffer == 0 && format != kDVDKeyFormatAGID_Invalidate)
    {
        return kIOReturnBadArgument;
    }

    return getProvider()->sendKey( /* buffer   */ buffer,
                                   /* keyClass */ keyClass,
                                   /* grantID  */ grantID,
                                   /* format   */ format );
}

OSMetaClassDefineReservedUsed(IODVDMedia, 1);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IODVDMedia::readStructure( IOMemoryDescriptor *     buffer,
                                    const DVDStructureFormat format,
                                    const UInt32             address,
                                    const UInt8              layer,
                                    const UInt8              grantID )
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    if (buffer == 0)
    {
        return kIOReturnBadArgument;
    }

    return getProvider()->readStructure( /* buffer  */ buffer,
                                         /* format  */ format,
                                         /* address */ address,
                                         /* layer   */ layer,
                                         /* grantID */ grantID );
}

OSMetaClassDefineReservedUsed(IODVDMedia, 2);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IODVDMedia::getSpeed(UInt16 * kilobytesPerSecond)
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->getSpeed(kilobytesPerSecond);
}

OSMetaClassDefineReservedUsed(IODVDMedia, 3);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IODVDMedia::setSpeed(UInt16 kilobytesPerSecond)
{
    if (isInactive())
    {
        return kIOReturnNoMedia;
    }

    return getProvider()->setSpeed(kilobytesPerSecond);
}

OSMetaClassDefineReservedUsed(IODVDMedia, 4);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IODVDMedia::readDiscInfo( IOMemoryDescriptor * buffer,
                                   UInt16 *             actualByteCount )
{
    if (isInactive())
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnNoMedia;
    }

    if (buffer == 0)
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnBadArgument;
    }

    return getProvider()->readDiscInfo( /* buffer          */ buffer,
                                        /* actualByteCount */ actualByteCount );
}

OSMetaClassDefineReservedUsed(IODVDMedia, 5);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IODVDMedia::readRZoneInfo( IOMemoryDescriptor *    buffer,
                                    UInt32                  address,
                                    DVDRZoneInfoAddressType addressType,
                                    UInt16 *                actualByteCount )
{
    if (isInactive())
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnNoMedia;
    }

    if (buffer == 0)
    {
        if (actualByteCount)  *actualByteCount = 0;

        return kIOReturnBadArgument;
    }

    return getProvider()->readTrackInfo(
                                        /* buffer          */ buffer,
                                        /* address         */ address,
                                        /* addressType     */ addressType,
                                        /* actualByteCount */ actualByteCount );
}

OSMetaClassDefineReservedUsed(IODVDMedia, 6);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia,  7);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia,  8);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia,  9);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 10);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 11);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 12);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 13);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 14);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 15);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 16);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 17);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 18);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 19);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 20);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 21);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 22);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 23);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 24);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 25);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 26);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 27);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 28);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 29);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 30);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMedia, 31);
