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

#include "IOAC97Controller.h"
#include "IOAC97CodecDevice.h"
#include "IOAC97Debug.h"

#define CLASS IOAC97Controller
#define super IOService
OSDefineMetaClassAndAbstractStructors( IOAC97Controller, IOService )

//---------------------------------------------------------------------------

bool CLASS::start( IOService * provider )
{
    if (!super::start(provider))
        return false;

    fProvider = provider;
    fProvider->retain();

    return true;
}

void CLASS::free( void )
{
    if (fProvider)
    {
        fProvider->release();
        fProvider = 0;
    }
    
    super::free();
}

//---------------------------------------------------------------------------

bool CLASS::handleOpen( IOService * client, IOOptionBits options, void * arg )
{
    IOAC97CodecDevice * codec = OSDynamicCast(IOAC97CodecDevice, client);
    bool success = true;  

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, client);

    if ((codec == 0) ||
        (fCodecOpenMask == 0 && fProvider->open(this) == false))
    {
        success = false;
    }

    if (success)
    {
        fCodecOpenMask |= (1 << codec->getCodecID());
    }

    return success;
}

//---------------------------------------------------------------------------

void CLASS::handleClose( IOService * client, IOOptionBits options )
{
    IOAC97CodecDevice * codec = OSDynamicCast(IOAC97CodecDevice, client);
    IOOptionBits oldMask = fCodecOpenMask;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, client);

    if (codec)
        fCodecOpenMask &= ~(1 << codec->getCodecID());

    if (oldMask && !fCodecOpenMask)
        fProvider->close(this);
}

//---------------------------------------------------------------------------

bool CLASS::handleIsOpen( const IOService * client ) const
{
    bool isOpen;

    if ( client )
    {
        IOAC97CodecDevice * codec = OSDynamicCast( IOAC97CodecDevice, client );
        isOpen = (codec && (fCodecOpenMask & (1 << codec->getCodecID())));
    }
    else
        isOpen = ( fCodecOpenMask != 0 );

    return isOpen;
}

//---------------------------------------------------------------------------

IOReturn CLASS::prepareAudioConfiguration( IOAC97AudioConfig * config )
{
    IOAC97MessageArgument args;

    args.param[0] = config;
    args.param[1] = 0;
    args.param[2] = 0;
    args.param[3] = 0;

    return messageCodecs( kIOAC97MessagePrepareAudioConfiguration, &args, 
                          kMessageCodecOrderAscending );
}

IOReturn CLASS::activateAudioConfiguration( IOAC97AudioConfig *   config,
                                            void *                target,
                                            IOAC97DMAEngineAction action,
                                            void *                param )
{
    IOAC97MessageArgument args;

    args.param[0] = config;
    args.param[1] = 0;
    args.param[2] = 0;
    args.param[3] = 0;

    return messageCodecs( kIOAC97MessageActivateAudioConfiguration, &args,
                          kMessageCodecOrderAscending );
}

void CLASS::deactivateAudioConfiguration( IOAC97AudioConfig * config )
{
    IOAC97MessageArgument args;

    args.param[0] = config;
    args.param[1] = 0;
    args.param[2] = 0;
    args.param[3] = 0;

    messageCodecs( kIOAC97MessageDeactivateAudioConfiguration, &args,
                   kMessageCodecOrderAscending );
}

IOReturn CLASS::createAudioControls( IOAC97AudioConfig * config,
                                     OSArray *           controls )
{
    IOAC97MessageArgument args;

    args.param[0] = config;
    args.param[1] = controls;
    args.param[2] = 0;
    args.param[3] = 0;

    return messageCodecs( kIOAC97MessageCreateAudioControls, &args,
                          kMessageCodecOrderAscending );
}

//---------------------------------------------------------------------------

IOReturn CLASS::messageCodecs( UInt32       type,
                               void *       argument,
                               IOOptionBits options )
{
    IOAC97CodecDevice *  codecs[kIOAC97MaxCodecCount];
    IORegistryIterator * iter;
    IORegistryEntry *    nub;
    IOReturn             ret = kIOReturnSuccess;

    memset(codecs, 0, sizeof(codecs));

    iter = IORegistryIterator::iterateOver(this, gIOServicePlane);
    if (iter == 0)
        return kIOReturnNoMemory;

    while ((nub = iter->getNextObject()))
    {
        IOAC97CodecDevice * codec = OSDynamicCast(IOAC97CodecDevice, nub);
        if (codec && (codec->getCodecID() < kIOAC97MaxCodecCount))
        {
            codecs[codec->getCodecID()] = codec;
        }
    }

    if (options & kMessageCodecOrderDescending)
    {
        for (int i = 0; i < kIOAC97MaxCodecCount; i++)
        {
            if (codecs[i])
            {
                ret = messageClient(type, codecs[i], argument);
                if (ret != kIOReturnSuccess) break;
            }
        }
    }
    else
    {
        for (int i = (kIOAC97MaxCodecCount-1); i >= 0; i--)
        {
            if (codecs[i])
            {
                ret = messageClient(type, codecs[i], argument);
                if (ret != kIOReturnSuccess) break;
            }
        }
    }

    iter->release();
    return ret;
}
