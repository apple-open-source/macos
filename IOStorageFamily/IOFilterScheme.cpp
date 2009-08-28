/*
 * Copyright (c) 1998-2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/storage/IOFilterScheme.h>

#define super IOStorage
OSDefineMetaClassAndStructors(IOFilterScheme, IOStorage)

#ifndef __LP64__
extern IOStorageAttributes gIOStorageAttributesUnsupported;
#endif /* !__LP64__ */

IOMedia * IOFilterScheme::getProvider() const
{
    //
    // Obtain this object's provider.  We override the superclass's method
    // to return a more specific subclass of OSObject -- an IOMedia.  This
    // method serves simply as a convenience to subclass developers.
    //

    return (IOMedia *) IOService::getProvider();
}

bool IOFilterScheme::handleOpen(IOService *  client,
                                IOOptionBits options,
                                void *       argument)
{
    //
    // The handleOpen method grants or denies permission to access this object
    // to an interested client.  The argument is an IOStorageAccess value that
    // specifies the level of access desired -- reader or reader-writer.
    //
    // This method can be invoked to upgrade or downgrade the access level for
    // an existing client as well.  The previous access level will prevail for
    // upgrades that fail, of course.   A downgrade should never fail.  If the
    // new access level should be the same as the old for a given client, this
    // method will do nothing and return success.  In all cases, one, singular
    // close-per-client is expected for all opens-per-client received.
    //
    // This implementation replaces the IOService definition of handleOpen().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we make our decision, change our state, and return from this method.
    //

    return getProvider()->open(this, options, (uintptr_t) argument);
}

bool IOFilterScheme::handleIsOpen(const IOService * client) const
{
    //
    // The handleIsOpen method determines whether the specified client, or any
    // client if none is specificed, presently has an open on this object.
    //
    // This implementation replaces the IOService definition of handleIsOpen().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we return from this method.
    //

    return getProvider()->isOpen(this);
}

void IOFilterScheme::handleClose(IOService * client, IOOptionBits options)
{
    //
    // The handleClose method closes the client's access to this object.
    //
    // This implementation replaces the IOService definition of handleClose().
    //
    // We are guaranteed that no other opens or closes will be processed until
    // we change our state and return from this method.
    //

    getProvider()->close(this, options);
}

void IOFilterScheme::read(IOService *           client,
                          UInt64                byteStart,
                          IOMemoryDescriptor *  buffer,
                          IOStorageAttributes * attributes,
                          IOStorageCompletion * completion)
{
    //
    // Read data from the storage object at the specified byte offset into the
    // specified buffer, asynchronously.   When the read completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the read.
    //
    // For simple filter schemes, the default behavior is to simply pass the
    // read through to the provider media.  More complex filter schemes such
    // as RAID will need to do extra processing here.
    //

#ifndef __LP64__
    if ( IOStorage::_expansionData )
    {
        if ( attributes == &gIOStorageAttributesUnsupported )
        {
            attributes = NULL;
        }
        else
        {
            IOStorage::read( client, byteStart, buffer, attributes, completion );

            return;
        }
    }
#endif /* !__LP64__ */

    getProvider( )->read( this, byteStart, buffer, attributes, completion );
}

void IOFilterScheme::write(IOService *           client,
                           UInt64                byteStart,
                           IOMemoryDescriptor *  buffer,
                           IOStorageAttributes * attributes,
                           IOStorageCompletion * completion)
{
    //
    // Write data into the storage object at the specified byte offset from the
    // specified buffer, asynchronously.   When the write completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the write.
    //
    // For simple filter schemes, the default behavior is to simply pass the
    // write through to the provider media. More complex filter schemes such
    // as RAID will need to do extra processing here.
    //

#ifndef __LP64__
    if ( IOStorage::_expansionData )
    {
        if ( attributes == &gIOStorageAttributesUnsupported )
        {
            attributes = NULL;
        }
        else
        {
            IOStorage::write( client, byteStart, buffer, attributes, completion );

            return;
        }
    }
#endif /* !__LP64__ */

    getProvider( )->write( this, byteStart, buffer, attributes, completion );
}

IOReturn IOFilterScheme::synchronizeCache(IOService * client)
{
    //
    // Flush the cached data in the storage object, if any, synchronously.
    //

    return getProvider()->synchronizeCache(this);
}

IOReturn IOFilterScheme::discard(IOService * client,
                                 UInt64      byteStart,
                                 UInt64      byteCount)
{
    //
    // Delete unused data from the storage object at the specified byte offset,
    // synchronously.
    //

    return getProvider( )->discard( this, byteStart, byteCount );
}

OSMetaClassDefineReservedUnused(IOFilterScheme,  0);
OSMetaClassDefineReservedUnused(IOFilterScheme,  1);
OSMetaClassDefineReservedUnused(IOFilterScheme,  2);
OSMetaClassDefineReservedUnused(IOFilterScheme,  3);
OSMetaClassDefineReservedUnused(IOFilterScheme,  4);
OSMetaClassDefineReservedUnused(IOFilterScheme,  5);
OSMetaClassDefineReservedUnused(IOFilterScheme,  6);
OSMetaClassDefineReservedUnused(IOFilterScheme,  7);
OSMetaClassDefineReservedUnused(IOFilterScheme,  8);
OSMetaClassDefineReservedUnused(IOFilterScheme,  9);
OSMetaClassDefineReservedUnused(IOFilterScheme, 10);
OSMetaClassDefineReservedUnused(IOFilterScheme, 11);
OSMetaClassDefineReservedUnused(IOFilterScheme, 12);
OSMetaClassDefineReservedUnused(IOFilterScheme, 13);
OSMetaClassDefineReservedUnused(IOFilterScheme, 14);
OSMetaClassDefineReservedUnused(IOFilterScheme, 15);
OSMetaClassDefineReservedUnused(IOFilterScheme, 16);
OSMetaClassDefineReservedUnused(IOFilterScheme, 17);
OSMetaClassDefineReservedUnused(IOFilterScheme, 18);
OSMetaClassDefineReservedUnused(IOFilterScheme, 19);
OSMetaClassDefineReservedUnused(IOFilterScheme, 20);
OSMetaClassDefineReservedUnused(IOFilterScheme, 21);
OSMetaClassDefineReservedUnused(IOFilterScheme, 22);
OSMetaClassDefineReservedUnused(IOFilterScheme, 23);
OSMetaClassDefineReservedUnused(IOFilterScheme, 24);
OSMetaClassDefineReservedUnused(IOFilterScheme, 25);
OSMetaClassDefineReservedUnused(IOFilterScheme, 26);
OSMetaClassDefineReservedUnused(IOFilterScheme, 27);
OSMetaClassDefineReservedUnused(IOFilterScheme, 28);
OSMetaClassDefineReservedUnused(IOFilterScheme, 29);
OSMetaClassDefineReservedUnused(IOFilterScheme, 30);
OSMetaClassDefineReservedUnused(IOFilterScheme, 31);

#ifndef __LP64__
extern "C" void _ZN14IOFilterScheme4readEP9IOServiceyP18IOMemoryDescriptor19IOStorageCompletion( IOFilterScheme * scheme, IOService * client, UInt64 byteStart, IOMemoryDescriptor * buffer, IOStorageCompletion completion )
{
    scheme->read( client, byteStart, buffer, NULL, &completion );
}

extern "C" void _ZN14IOFilterScheme5writeEP9IOServiceyP18IOMemoryDescriptor19IOStorageCompletion( IOFilterScheme * scheme, IOService * client, UInt64 byteStart, IOMemoryDescriptor * buffer, IOStorageCompletion completion )
{
    scheme->write( client, byteStart, buffer, NULL, &completion );
}
#endif /* !__LP64__ */
