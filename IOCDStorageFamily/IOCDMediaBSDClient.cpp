/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <sys/errno.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>

#define super IOMediaBSDClient
OSDefineMetaClassAndStructors(IOCDMediaBSDClient, IOMediaBSDClient)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static bool DKIOC_IS_RESERVED(caddr_t data, u_int16_t reserved)
{
    UInt32 index;

    for ( index = 0; index < sizeof(reserved) * 8; index++, reserved >>= 1 )
    {
        if ( (reserved & 1) )
        {
            if ( data[index] )  return true;
        }
    }

    return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static IOMemoryDescriptor * DKIOC_PREPARE_BUFFER( void *      address,
                                                  UInt32      length,
                                                  IODirection direction )
{
    IOMemoryDescriptor * buffer = 0;

    if ( address && length )
    {
        buffer = IOMemoryDescriptor::withAddress(         // (create the buffer)
                                /* address   */ (vm_address_t) address,
                                /* length    */                length,
                                /* direction */                direction,
                                /* task      */                current_task() );
    }

    if ( buffer )
    {
        if ( buffer->prepare() != kIOReturnSuccess )     // (prepare the buffer)
        {
            buffer->release();
            buffer = 0;
        }
    }

    return buffer;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void DKIOC_COMPLETE_BUFFER(IOMemoryDescriptor * buffer)
{
    if ( buffer )
    {
        buffer->complete();                             // (complete the buffer)
        buffer->release();                               // (release the buffer)
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOCDMedia * IOCDMediaBSDClient::getProvider() const
{
    //
    // Obtain this object's provider.  We override the superclass's method
    // to return a more specific subclass of IOService -- IOCDMedia.  This
    // method serves simply as a convenience to subclass developers.
    //

    return (IOCDMedia *) IOService::getProvider();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int IOCDMediaBSDClient::ioctl( dev_t         dev,
                               u_long        cmd,
                               caddr_t       data,
                               int           flags,
                               struct proc * proc )
{
    //
    // Process a CD-specific ioctl.
    //

    IOMemoryDescriptor * buffer = 0;
    int                  error  = 0;
    IOReturn             status = kIOReturnSuccess;

    switch ( cmd )
    {
        case DKIOCCDREAD:                              // readCD(dk_cd_read_t *)
        {
            UInt64         actualByteCount = 0;
            dk_cd_read_t * request         = (dk_cd_read_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0xFC00) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn );

            status = getProvider()->readCD(
                       /* client          */                this,
                       /* byteStart       */                request->offset,
                       /* buffer          */                buffer,
                       /* sectorArea      */ (CDSectorArea) request->sectorArea,
                       /* sectorType      */ (CDSectorType) request->sectorType,
                       /* actualByteCount */                &actualByteCount );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            request->bufferLength = (UInt32) actualByteCount;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADISRC:                   // readISRC(dk_cd_read_isrc_t *)
        {
            dk_cd_read_isrc_t * request = (dk_cd_read_isrc_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0xC000) )  { error = EINVAL;  break; }

            status = getProvider()->readISRC(request->track, request->isrc);

        } break;

        case DKIOCCDREADMCN:                      // readMCN(dk_cd_read_mcn_t *)
        {
            dk_cd_read_mcn_t * request = (dk_cd_read_mcn_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0xC000) )  { error = EINVAL;  break; }

            status = getProvider()->readMCN(request->mcn);

        } break;

        case DKIOCCDGETSPEED:                           // getSpeed(u_int16_t *)
        {
            status = getProvider()->getSpeed((u_int16_t *)data);

        } break;

        case DKIOCCDSETSPEED:                           // setSpeed(u_int16_t *)
        {
            status = getProvider()->setSpeed(*(u_int16_t *)data);

        } break;

        case DKIOCCDREADTOC:                     // readTOC(dk_cd_read_toc_t *);
        {
            dk_cd_read_toc_t * request = (dk_cd_read_toc_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0x037C) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn );

            status = getProvider()->readTOC(
                       /* buffer          */ buffer,
                       /* format          */ request->format,
                       /* formatAsTime    */ request->formatAsTime,
                       /* address         */ request->address.session,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADDISCINFO:      // readDiscInfo(dk_cd_read_disc_info_t *)
        {
            dk_cd_read_disc_info_t * request = (dk_cd_read_disc_info_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0x03FF) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn );

            status = getProvider()->readDiscInfo(
                       /* buffer          */ buffer,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCCDREADTRACKINFO:   // readTrackInfo(dk_cd_read_track_info_t *)
        {
            dk_cd_read_track_info_t * request;

            request = (dk_cd_read_track_info_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0x020F) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn );

            status = getProvider()->readTrackInfo(
                       /* buffer          */ buffer,
                       /* address         */ request->address,
                       /* addressType     */ request->addressType,
                       /* actualByteCount */ &request->bufferLength );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        default:
        {
            //
            // A foreign ioctl was received.  Ask our superclass' opinion.
            //

            error = super::ioctl(dev, cmd, data, flags, proc);

        } break;
    }

    return error ? error : getProvider()->errnoFromReturn(status);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 0);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 1);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 2);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 3);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 4);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 5);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 6);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IOCDMediaBSDClient, 7);
