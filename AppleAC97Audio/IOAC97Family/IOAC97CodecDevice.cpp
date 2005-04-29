/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>
#include "IOAC97CodecDevice.h"
#include "IOAC97Controller.h"
#include "IOAC97Debug.h"

#define CLASS IOAC97CodecDevice
#define super IOService
OSDefineMetaClassAndStructors( IOAC97CodecDevice, IOService )

//---------------------------------------------------------------------------

CLASS * CLASS::codec( IOAC97Controller * controller,
                      IOAC97CodecID      codecID,
                      void *             codecParam )
{
    CLASS * codec = new CLASS;

    if (codec && !codec->init(controller, codecID, codecParam))
    {
        codec->release();
        codec = 0;
    }

    return codec;
}

//---------------------------------------------------------------------------

bool CLASS::init( IOAC97Controller * controller,
                  IOAC97CodecID      codecID,
                  void *             codecParam )
{
    char             location[2];
    char             idstring[8];
    IOAC97CodecWord  id_hi, id_lo;

    if ( super::init( 0 ) == false ) return false;

    fController = controller;
    fCodecID    = codecID;
    fCodecParam = codecParam;

    // Read codec vendor ID for client matching.

    if (codecRead(kCodecVendorID1, &id_hi) != kIOReturnSuccess ||
        codecRead(kCodecVendorID2, &id_lo) != kIOReturnSuccess)
    {
        DebugLog("%s: VendorID read error\n", getName());
        return false;
    }

    sprintf(idstring, "%c%c%c%02x", (UInt8)(id_hi >> 8),
                                    (UInt8)(id_hi >> 0),
                                    (UInt8)(id_lo >> 8),
                                    (UInt8)(id_lo >> 0));

    setProperty(kIOAC97CodecPNPVendorIDKey, idstring);

    // Set location based on codec ID to differentiate among peers.

    location[0] = fCodecID + '0';
    location[1] = '\0';

    setLocation(location);

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::handleOpen( IOService * client, IOOptionBits options, void * arg )
{
    bool success = false;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, client);

    if ( handleIsOpen( 0 ) == false )
    {
        if ( fController->open( this ) )
        {
            success = super::handleOpen( client, options, arg );
            if ( success != true )
                fController->close( this );
        }
    }
    return success;
}

//---------------------------------------------------------------------------

void CLASS::handleClose( IOService * client, IOOptionBits options )
{
    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, client);

    if ( handleIsOpen( client ) )
        fController->close( this );

    super::handleClose( client, options );
}

//---------------------------------------------------------------------------

IOReturn CLASS::codecRead( IOAC97CodecOffset offset,
                           IOAC97CodecWord * word )
{
    return fController->codecRead(fCodecID, offset, word);
}

//---------------------------------------------------------------------------

IOReturn CLASS::codecWrite( IOAC97CodecOffset offset,
                            IOAC97CodecWord   word )
{
    return fController->codecWrite(fCodecID, offset, word);
}

//---------------------------------------------------------------------------

IOAC97CodecID CLASS::getCodecID( void ) const
{
    return fCodecID;
}

void * CLASS::getCodecParameter( void ) const
{
    return fCodecParam;
}

IOAC97Controller * CLASS::getController( void ) const
{
    return fController;
}

//---------------------------------------------------------------------------

IOReturn CLASS::message( UInt32 type, IOService * provider, void * argument )
{
    switch (type)
    {
        case kIOAC97MessagePrepareAudioConfiguration:
        case kIOAC97MessageActivateAudioConfiguration:
        case kIOAC97MessageDeactivateAudioConfiguration:
        case kIOAC97MessageCreateAudioControls:
            return messageClients(type, argument);
    }

    return super::message(type, provider, argument);
}

//---------------------------------------------------------------------------

bool CLASS::matchPropertyTable( OSDictionary * table,
                                SInt32 *       score )
{
    OSString *   string;
    const char * matchPtr;
    int          matchLen;
    const char * pnpPtr;
    int          pnpLen;
    int          look;
    bool         isMatch = false;

    if (!table || !score || !super::matchPropertyTable(table, score))
        return false;

    string = OSDynamicCast(OSString, getProperty(kIOAC97CodecPNPVendorIDKey));
    if (string == 0 || !(pnpLen = string->getLength()))
        return false;

    pnpPtr = string->getCStringNoCopy();

    string = OSDynamicCast(OSString, table->getObject(kIOAC97CodecPNPVendorIDKey));
    if (string == 0)
        return true;

    matchLen = string->getLength();
    matchPtr = string->getCStringNoCopy();

    // partial (5 char) string compare

    look = 0;
    for (int i = 0; i < matchLen; i++)
    {
        char next = matchPtr[i];
        
        if ((look < 0) && (next != ' '))
            continue;

        if (next == ' ')
        {
            look = 0;
            continue;
        }

        if (next == pnpPtr[look])
            look++;
        else
            look = -1;

        if (look == pnpLen)
        {
            isMatch = true;
            break;
        }
    }

    return isMatch;
}
