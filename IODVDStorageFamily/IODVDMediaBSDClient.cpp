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
#include <IOKit/storage/IODVDMediaBSDClient.h>

#define super IOMediaBSDClient
OSDefineMetaClassAndStructors(IODVDMediaBSDClient, IOMediaBSDClient)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IODVDMedia * IODVDMediaBSDClient::getProvider() const
{
    //
    // Obtain this object's provider.   We override the superclass's method
    // to return a more specific subclass of IOService -- IODVDMedia.  This
    // method serves simply as a convenience to subclass developers.
    //

    return (IODVDMedia *) IOService::getProvider();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int IODVDMediaBSDClient::ioctl( dev_t         dev,
                                u_long        cmd,
                                caddr_t       data,
                                int           flags,
                                struct proc * proc )
{
    //
    // Process a DVD-specific ioctl.
    //

    int error = 0;

    switch ( cmd )
    {
        case DKIOCDVDREADSTRUCTURE: // readStructure(dk_dvd_read_structure_t *);
        {
            IOMemoryDescriptor *      buffer  = 0;
            dk_dvd_read_structure_t * request = (dk_dvd_read_structure_t *)data;
            IOReturn                  status  = kIOReturnSuccess;

            if ( request->reserved0008[0] ||
                 request->reserved0008[1] ||
                 request->reserved0008[2] )
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

            status = getProvider()->readStructure(
                       /* buffer    */                      buffer,
                       /* format    */ (DVDStructureFormat) request->format,
                       /* address   */                      request->address,
                       /* layer     */                      request->layer,
                       /* grantID   */                      request->grantID );

            error = getProvider()->errnoFromReturn(status);

            if ( buffer )
            {
                buffer->complete();                     // (complete the buffer)
                buffer->release();         // (release our retain on the buffer)
            }

        } break;

        case DKIOCDVDREPORTKEY:             // reportKey(dk_dvd_report_key_t *);
        {
            IOMemoryDescriptor *  buffer  = 0;
            dk_dvd_report_key_t * request = (dk_dvd_report_key_t *)data;
            IOReturn              status  = kIOReturnSuccess;

            if ( request->reserved0016[0] ||
                 request->reserved0016[1] ||
                 request->reserved0072[0] )
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

            status = getProvider()->reportKey(
                       /* buffer    */                buffer,
                       /* keyClass  */ (DVDKeyClass)  request->keyClass,
                       /* address   */                request->address,
                       /* grantID   */                request->grantID,
                       /* format    */ (DVDKeyFormat) request->format );

            error = getProvider()->errnoFromReturn(status);

            if ( buffer )
            {
                buffer->complete();                     // (complete the buffer)
                buffer->release();         // (release our retain on the buffer)
            }

        } break;

        case DKIOCDVDSENDKEY:                   // sendKey(dk_dvd_send_key_t *);
        {
            IOMemoryDescriptor * buffer  = 0;
            dk_dvd_send_key_t *  request = (dk_dvd_send_key_t *)data;
            IOReturn             status  = kIOReturnSuccess;

            if ( request->reserved0016[0] ||
                 request->reserved0016[1] ||
                 request->reserved0016[2] ||
                 request->reserved0016[3] ||
                 request->reserved0016[4] ||
                 request->reserved0016[5] ||
                 request->reserved0072[0] )
            {
                error = EINVAL;
                break;
            }

            if ( request->buffer && request->bufferLength )
            {
                buffer = IOMemoryDescriptor::withAddress( 
                           /* address   */ (vm_address_t) request->buffer,
                           /* length    */                request->bufferLength,
                           /* direction */                kIODirectionOut,
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

            status = getProvider()->sendKey(
                       /* buffer    */                buffer,
                       /* keyClass  */ (DVDKeyClass)  request->keyClass,
                       /* grantID   */                request->grantID,
                       /* format    */ (DVDKeyFormat) request->format );

            error = getProvider()->errnoFromReturn(status);

            if ( buffer )
            {
                buffer->complete();                     // (complete the buffer)
                buffer->release();         // (release our retain on the buffer)
            }

        } break;

        case DKIOCDVDGETSPEED:                         // getSpeed(u_int16_t *);
        {
            IOReturn status;

            status = getProvider()->getSpeed((u_int16_t *)data);
            error  = getProvider()->errnoFromReturn(status);

        } break;

        case DKIOCDVDSETSPEED:                         // setSpeed(u_int16_t *);
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

OSMetaClassDefineReservedUnused(IODVDMediaBSDClient, 0);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMediaBSDClient, 1);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMediaBSDClient, 2);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMediaBSDClient, 3);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMediaBSDClient, 4);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMediaBSDClient, 5);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMediaBSDClient, 6);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

OSMetaClassDefineReservedUnused(IODVDMediaBSDClient, 7);
