/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#include <sys/errno.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>

#define super IOMediaBSDClient
OSDefineMetaClassAndStructors(IOCDMediaBSDClient, IOMediaBSDClient)

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

    int error = 0;

    switch ( cmd )
    {
        case DKIOCCDREAD:                             // readCD(dk_cd_read_t *);
        {
            UInt64               actualByteCount = 0;
            IOMemoryDescriptor * buffer          = 0;
            dk_cd_read_t *       request         = (dk_cd_read_t *)data;
            IOReturn             status          = kIOReturnSuccess;

            if ( request->reserved0080[0] ||
                 request->reserved0080[1] ||
                 request->reserved0080[2] ||
                 request->reserved0080[3] ||
                 request->reserved0080[4] ||
                 request->reserved0080[5] )
            {
                error = EINVAL;
                break;
            }

            if ( request->buffer && request->bufferLength )
            {
                buffer = IOMemoryDescriptor::withAddress( 
                           /* address   */ (vm_address_t) request->buffer,
                           /* length    */                request->bufferLength,
                           /* direction */                kIODirectionIn,
                           /* task      */                current_task() );

                if ( buffer == 0 )                               // (no buffer?)
                {
                    error = ENOMEM;
                    break;
                }

                if ( buffer->prepare() != kIOReturnSuccess ) // (prepare buffer)
                {
                    buffer->release();
                    error = EFAULT;           // (wiring or permissions failure)
                    break;
                }
            }

            status = getProvider()->readCD(
                       /* client          */                this,
                       /* byteStart       */                request->offset,
                       /* buffer          */                buffer,
                       /* sectorArea      */ (CDSectorArea) request->sectorArea,
                       /* sectorType      */ (CDSectorType) request->sectorType,
                       /* actualByteCount */                &actualByteCount );

            error = getProvider()->errnoFromReturn(status);

            request->bufferLength = (UInt32) actualByteCount;

            if ( buffer )
            {
                buffer->complete();                     // (complete the buffer)
                buffer->release();         // (release our retain on the buffer)
            }

        } break;

        case DKIOCCDREADISRC:                  // readISRC(dk_cd_read_isrc_t *);
        {
            dk_cd_read_isrc_t * request = (dk_cd_read_isrc_t *)data;
            IOReturn            status  = kIOReturnSuccess;

            if ( request->reserved0112[0] ||
                 request->reserved0112[1] )
            {
                error = EINVAL;
                break;
            }

            status = getProvider()->readISRC(request->track, request->isrc);
            error  = getProvider()->errnoFromReturn(status);

        } break;

        case DKIOCCDREADMCN:                     // readMCN(dk_cd_read_mcn_t *);
        {
            dk_cd_read_mcn_t * request = (dk_cd_read_mcn_t *)data;
            IOReturn           status  = kIOReturnSuccess;

            if ( request->reserved0112[0] ||
                 request->reserved0112[1] )
            {
                error = EINVAL;
                break;
            }

            status = getProvider()->readMCN(request->mcn);
            error  = getProvider()->errnoFromReturn(status);

        } break;

        case DKIOCCDGETSPEED:                          // getSpeed(u_int16_t *);
        {
            IOReturn status;

            status = getProvider()->getSpeed((u_int16_t *)data);
            error  = getProvider()->errnoFromReturn(status);

        } break;

        case DKIOCCDSETSPEED:                          // setSpeed(u_int16_t *);
        {
            IOReturn status;

            status = getProvider()->setSpeed(*(u_int16_t *)data);
            error  = getProvider()->errnoFromReturn(status);

        } break;

        default:
        {
            //
            // A foreign ioctl was received.  Ask our superclass' opinion.
            //

            error = super::ioctl(dev, cmd, data, flags, proc);

        } break;
    }

    return error;                                       // (return error status)
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
