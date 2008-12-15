/*
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/storage/IOStorage.h>

#define super IOService
OSDefineMetaClassAndAbstractStructors(IOStorage, IOService)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#define kIOStorageAttributesUnsupported ( ( IOStorage::ExpansionData * ) 1 )

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOStorageAttributes gIOStorageAttributesUnsupported = { kIOStorageOptionReserved };

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

extern "C" void _ZN9IOStorage4readEP9IOServiceyP18IOMemoryDescriptor19IOStorageCompletion( IOStorage *, IOService *, UInt64, IOMemoryDescriptor *, IOStorageCompletion );
extern "C" void _ZN9IOStorage5writeEP9IOServiceyP18IOMemoryDescriptor19IOStorageCompletion( IOStorage *, IOService *, UInt64, IOMemoryDescriptor *, IOStorageCompletion );
extern "C" void _ZN9IOStorage4readEP9IOServiceyP18IOMemoryDescriptorP19IOStorageAttributesP19IOStorageCompletion( IOStorage *, IOService *, UInt64, IOMemoryDescriptor *, IOStorageAttributes *, IOStorageCompletion * );
extern "C" void _ZN9IOStorage5writeEP9IOServiceyP18IOMemoryDescriptorP19IOStorageAttributesP19IOStorageCompletion( IOStorage *, IOService *, UInt64, IOMemoryDescriptor *, IOStorageAttributes *, IOStorageCompletion * );

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static bool storageAttributes( IOStorage * storage )
{
    return ( OSMemberFunctionCast( void *, storage, ( void ( IOStorage::* )( IOService *, UInt64, IOMemoryDescriptor *, IOStorageCompletion                          ) ) &IOStorage::read  ) == _ZN9IOStorage4readEP9IOServiceyP18IOMemoryDescriptor19IOStorageCompletion                         ) &&
           ( OSMemberFunctionCast( void *, storage, ( void ( IOStorage::* )( IOService *, UInt64, IOMemoryDescriptor *, IOStorageCompletion                          ) ) &IOStorage::write ) == _ZN9IOStorage5writeEP9IOServiceyP18IOMemoryDescriptor19IOStorageCompletion                        ) &&
           ( OSMemberFunctionCast( void *, storage, ( void ( IOStorage::* )( IOService *, UInt64, IOMemoryDescriptor *, IOStorageAttributes *, IOStorageCompletion * ) ) &IOStorage::read  ) != _ZN9IOStorage4readEP9IOServiceyP18IOMemoryDescriptorP19IOStorageAttributesP19IOStorageCompletion  ) &&
           ( OSMemberFunctionCast( void *, storage, ( void ( IOStorage::* )( IOService *, UInt64, IOMemoryDescriptor *, IOStorageAttributes *, IOStorageCompletion * ) ) &IOStorage::write ) != _ZN9IOStorage5writeEP9IOServiceyP18IOMemoryDescriptorP19IOStorageAttributesP19IOStorageCompletion );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void storageCompletion(void *   target,
                              void *   parameter,
                              IOReturn status,
                              UInt64   actualByteCount)
{
    //
    // Internal completion routine for synchronous versions of read and write.
    //

    if (parameter)  *((UInt64 *)parameter) = actualByteCount;
    ((IOSyncer *)target)->signal(status);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOStorage::init(OSDictionary * properties)
{
    //
    // Initialize this object's minimal state.
    //

    if ( super::init( properties ) == false )
    {
        return false;
    }

    if ( storageAttributes( this ) == false )
    {
        IOStorage::_expansionData = kIOStorageAttributesUnsupported;
    }

    if ( IOStorage::_expansionData )
    {
        OSDictionary * features;

        features = OSDictionary::withCapacity( 1 );

        if ( features )
        {
            setProperty( kIOStorageFeaturesKey, features );

            features->release( );
        }
    }

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOStorage::complete(IOStorageCompletion completion,
                         IOReturn            status,
                         UInt64              actualByteCount)
{
    //
    // Invokes the specified completion action of the read/write request.  If
    // the completion action is unspecified, no action is taken.  This method
    // serves simply as a convenience to storage subclass developers.
    //

    complete( &completion, status, actualByteCount );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOStorage::complete(IOStorageCompletion * completion,
                         IOReturn              status,
                         UInt64                actualByteCount)
{
    //
    // Invokes the specified completion action of the read/write request.  If
    // the completion action is unspecified, no action is taken.  This method
    // serves simply as a convenience to storage subclass developers.
    //

    if ( completion && completion->action )
    {
        ( completion->action )( completion->target, completion->parameter, status, actualByteCount );
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOStorage::open(IOService *     client,
                     IOOptionBits    options,
                     IOStorageAccess access)
{
    //
    // Ask the storage object for permission to access its contents; the method
    // is equivalent to IOService::open(), but with the correct parameter types.
    //

    return super::open(client, options, (void *) access);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IOStorage::read(IOService *          client,
                         UInt64               byteStart,
                         IOMemoryDescriptor * buffer,
                         UInt64 *             actualByteCount)
{
    //
    // Read data from the storage object at the specified byte offset into the
    // specified buffer, synchronously.   When the read completes, this method
    // will return to the caller.  The actual byte count field is optional.
    //

    IOStorageCompletion	completion;
    IOSyncer *          completionSyncer;

    // Initialize the lock we will synchronize against.

    completionSyncer = IOSyncer::create();

    // Fill in the completion information for this request.

    completion.target    = completionSyncer;
    completion.action    = storageCompletion;
    completion.parameter = actualByteCount;

    // Issue the asynchronous read.

    read(client, byteStart, buffer, completion);

    // Wait for the read to complete.

    return completionSyncer->wait();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IOStorage::write(IOService *          client,
                          UInt64               byteStart,
                          IOMemoryDescriptor * buffer,
                          UInt64 *             actualByteCount)
{
    //
    // Write data into the storage object at the specified byte offset from the
    // specified buffer, synchronously.   When the write completes, this method
    // will return to the caller.  The actual byte count field is optional.
    //

    IOStorageCompletion completion;
    IOSyncer *          completionSyncer;

    // Initialize the lock we will synchronize against.

    completionSyncer = IOSyncer::create();

    // Fill in the completion information for this request.

    completion.target    = completionSyncer;
    completion.action    = storageCompletion;
    completion.parameter = actualByteCount;

    // Issue the asynchronous write.

    write(client, byteStart, buffer, completion);

    // Wait for the write to complete.

    return completionSyncer->wait();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOStorage::read(IOService *          client,
                     UInt64               byteStart,
                     IOMemoryDescriptor * buffer,
                     IOStorageCompletion  completion)
{
    //
    // Read data from the storage object at the specified byte offset into the
    // specified buffer, asynchronously.   When the read completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the read.
    //

    if ( IOStorage::_expansionData == kIOStorageAttributesUnsupported )
    {
        read( client, byteStart, buffer, &gIOStorageAttributesUnsupported, &completion );
    }
    else
    {
        read( client, byteStart, buffer, NULL, &completion );
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOStorage::write(IOService *          client,
                      UInt64               byteStart,
                      IOMemoryDescriptor * buffer,
                      IOStorageCompletion  completion)
{
    //
    // Write data into the storage object at the specified byte offset from the
    // specified buffer, asynchronously.   When the write completes, the caller
    // will be notified via the specified completion action.
    //
    // The buffer will be retained for the duration of the write.
    //

    if ( IOStorage::_expansionData == kIOStorageAttributesUnsupported )
    {
        write( client, byteStart, buffer, &gIOStorageAttributesUnsupported, &completion );
    }
    else
    {
        write( client, byteStart, buffer, NULL, &completion );
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOStorage::read(IOService *           client,
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

    if ( attributes && attributes->options )
    {
        complete( completion, kIOReturnUnsupported );
    }
    else
    {
        read( client, byteStart, buffer, completion ? *completion : ( IOStorageCompletion ) { 0 } );
    }
}

OSMetaClassDefineReservedUsed(IOStorage, 0);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOStorage::write(IOService *           client,
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

    if ( attributes && attributes->options )
    {
        complete( completion, kIOReturnUnsupported );
    }
    else
    {
        write( client, byteStart, buffer, completion ? *completion : ( IOStorageCompletion ) { 0 } );
    }
}

OSMetaClassDefineReservedUsed(IOStorage, 1);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn IOStorage::discard(IOService * client,
                            UInt64      byteStart,
                            UInt64      byteCount)
{
    return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUsed(IOStorage, 2);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 3);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 4);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 5);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 6);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 7);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 8);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 9);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 10);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 11);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 12);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 13);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 14);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 15);
