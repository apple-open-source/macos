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

    IOMemoryDescriptor * buffer = 0;
    int                  error  = 0;
    IOReturn             status = kIOReturnSuccess;

    switch ( cmd )
    {
        case DKIOCDVDREADSTRUCTURE:  // readStructure(dk_dvd_read_structure_t *)
        {
            dk_dvd_read_structure_t * request;

            request = (dk_dvd_read_structure_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0x000E) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn );

            status = getProvider()->readStructure(
                       /* buffer    */                      buffer,
                       /* format    */ (DVDStructureFormat) request->format,
                       /* address   */                      request->address,
                       /* layer     */                      request->layer,
                       /* grantID   */                      request->grantID );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCDVDREPORTKEY:              // reportKey(dk_dvd_report_key_t *)
        {
            dk_dvd_report_key_t * request = (dk_dvd_report_key_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0x020C) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn );

            status = getProvider()->reportKey(
                       /* buffer    */                buffer,
                       /* keyClass  */ (DVDKeyClass)  request->keyClass,
                       /* address   */                request->address,
                       /* grantID   */                request->grantID,
                       /* format    */ (DVDKeyFormat) request->format );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCDVDSENDKEY:                    // sendKey(dk_dvd_send_key_t *)
        {
            dk_dvd_send_key_t * request = (dk_dvd_send_key_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0x02FC) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionOut );

            status = getProvider()->sendKey(
                       /* buffer    */                buffer,
                       /* keyClass  */ (DVDKeyClass)  request->keyClass,
                       /* grantID   */                request->grantID,
                       /* format    */ (DVDKeyFormat) request->format );

            status = (status == kIOReturnUnderrun) ? kIOReturnSuccess : status;

            DKIOC_COMPLETE_BUFFER(buffer);

        } break;

        case DKIOCDVDGETSPEED:                          // getSpeed(u_int16_t *)
        {
            status = getProvider()->getSpeed((u_int16_t *)data);

        } break;

        case DKIOCDVDSETSPEED:                          // setSpeed(u_int16_t *)
        {
            status = getProvider()->setSpeed(*(u_int16_t *)data);

        } break;

        case DKIOCDVDREADDISCINFO:    // readDiscInfo(dk_dvd_read_disc_info_t *)
        {
            dk_dvd_read_disc_info_t * request;

            request = (dk_dvd_read_disc_info_t *) data;

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

        case DKIOCDVDREADRZONEINFO: // readRZoneInfo(dk_dvd_read_rzone_info_t *)
        {
            dk_dvd_read_rzone_info_t * request;

            request = (dk_dvd_read_rzone_info_t *) data;

            if ( DKIOC_IS_RESERVED(data, 0x020F) )  { error = EINVAL;  break; }

            buffer = DKIOC_PREPARE_BUFFER(
                       /* address   */ request->buffer,
                       /* length    */ request->bufferLength,
                       /* direction */ kIODirectionIn );

            status = getProvider()->readRZoneInfo(
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
