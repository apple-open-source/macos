/*
 * Copyright (c) 1998-2005 Apple Computer, Inc. All rights reserved.
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
// Local Functions

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

void IOStorage::complete(IOStorageCompletion completion,
                         IOReturn            status,
                         UInt64              actualByteCount)
{
    //
    // Invokes the specified completion action of the read/write request.  If
    // the completion action is unspecified, no action is taken.  This method
    // serves simply as a convenience to storage subclass developers.
    //

    if (completion.action)  (*completion.action)(completion.target, 
                                                 completion.parameter,
                                                 status,
                                                 actualByteCount);
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

OSMetaClassDefineReservedUnused(IOStorage, 0);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 1);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOStorage, 2);

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
