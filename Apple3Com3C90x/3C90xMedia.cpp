/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <sys/systm.h>  // snprintf
#include "3C90x.h"

//---------------------------------------------------------------------------
// resetMedia
//
// Initialize the media state. This must be the only method called by
// the driver for media setup.

void Apple3Com3C90x::resetMedia( const IONetworkMedium * medium )
{
    UInt32 mediumType;

    LOG_MEDIA("%s::%s(%p)\n", getName(), __FUNCTION__, medium);

    // Default settings.

    _media.mediaPort    = kMediaPortAuto;
    _media.duplexMode   = kHalfDuplex;
    _media.phyLink      = kMIILinkNone;
    _media.phyStatus    = 0;
    _media.linkStatus   = kIONetworkLinkValid;
    _media.syncStatus   = true;

    if ( medium )
    {
        MediaPort  port = (MediaPort) medium->getIndex();
        DuplexMode mode;

        if ( medium->getType() & kIOMediumOptionFullDuplex )
            mode = kFullDuplex;
        else
            mode = kHalfDuplex;
        
        if ( checkMediaPortSupport( port, mode ) )
        {
            _media.mediaPort  = port;
            _media.duplexMode = mode;
        }
    }

    // Report the current selection.

    mediumType = getIOMediumType( _media.mediaPort, _media.duplexMode );
    medium = IONetworkMedium::getMediumWithType( _mediumDict, mediumType );
    if ( medium ) setSelectedMedium( medium );

    setLinkStatus( _media.linkStatus );

    // Handle automatic media port selection.

    if ( _media.mediaPort == kMediaPortAuto )
    {
        // If the adapter has only a single network port and it does not
        // perform Nway negotiation on that port, then hardcode to use
        // that port. This applies to the 3C900-TPO.

        if ( _adapterInfo->deviceID == 0x9000 )
        {
            _media.mediaPort  = kMediaPort10BaseT;
            _media.duplexMode = kHalfDuplex;
        }
        else
        {
            autoSelectMediaPort();
        }
    }

    if ( _media.mediaPort != kMediaPortAuto )
    {
        // Map to MII media port.

        mapSelectionToMIIMediaPort();

        // Program the hardware.

        setMediaRegisters();

        // Configure the PHY for MII/AutoNeg media port.

        configurePHY();

        // Reset adapter after the transceiver port is changed.
        // Hope non of the media registers are affected by this.

        resetAndEnableAdapter( true );

        // Re-configure PHY following reset. Perhaps a reset mask
        // should be specified to avoid having to perform the second
        // PHY configuration, but which bit mask?

        _media.phyConfigured = false;
    }
}

//---------------------------------------------------------------------------
// monitorLinkStatus

void Apple3Com3C90x::monitorLinkStatus()
{
    UInt32      linkStatus = _media.linkStatus;
    DuplexMode  duplexMode = _media.duplexMode;
    LinkSpeed   linkSpeed  = _media.linkSpeed;
    MediaPort   mediaPort  = _media.mediaPort;
    MIILink     phyLink;

	// LOG_MEDIA("%s::%s\n", getName(), __FUNCTION__);

    switch ( _media.mediaPort )
    {
        case kMediaPortAuto:

            // FIXME: Not tested.

            resetMedia( getSelectedMedium() );
            return;

        case kMediaPortAutoNeg:
        case kMediaPortMII:

            if ( _media.phyConfigured == false )
            {
                // IOSleep( 1000 );
                configurePHY();
                // IOSleep( 1000 );
                return;
            }

            phyLink = phyGetBestNegotiatedLink( _media.phyAddress,
                                                _media.phyLink,
                                                _media.phyLinkActive,
                                               &_media.phyStatus );

            if ( phyLink == _media.phyLinkActive )
                break;  // No PHY link change.

            _media.phyLinkActive = phyLink;  // Record the new PHY link.

            phyLink &= kMIILinkMask;

            linkStatus &= ~kIONetworkLinkActive;

            if ( phyLink != kMIILinkNone )
            {
                linkStatus |= kIONetworkLinkActive;

                // Update duplex mode and link speed.

                if ( phyLink >= kMIILink100TX )
                    linkSpeed = kLinkSpeed100;
                else
                    linkSpeed = kLinkSpeed10;

                duplexMode = getDuplexModeFromMIILink( phyLink );
                mediaPort  = getMediaPortFromMIILink( phyLink );
            }

            break;

        case kMediaPort10BaseT:

            if ( getMediaStatus() & kMediaStatusLinkDetectMask )
                linkStatus |= kIONetworkLinkActive;
            else
                linkStatus &= ~kIONetworkLinkActive;

            linkSpeed = kLinkSpeed10;
            break;

        case kMediaPort100BaseTX:
        case kMediaPort100BaseFX:

            if ( getMediaStatus() & kMediaStatusLinkDetectMask )
                linkStatus |= kIONetworkLinkActive;
            else
                linkStatus &= ~kIONetworkLinkActive;

            linkSpeed = kLinkSpeed100;
            break;

        // No link detection for AUI and 10Base2 ports.

        default:

            linkStatus |= kIONetworkLinkActive;
            linkSpeed = kLinkSpeed10;
            break;
    }

    // Update current link status.

    if ( linkStatus != _media.linkStatus )
    {
        IONetworkMedium * activeMedium = 0;

        LOG_MEDIA("%s: link %lx speed %d duplex %d status %lx\n",
                  getName(), _media.phyLinkActive, linkSpeed,
                  duplexMode, linkStatus);

        if ( linkStatus & kIONetworkLinkActive )
        {
            activeMedium = IONetworkMedium::getMediumWithType(
                              _mediumDict,
                              getIOMediumType( mediaPort, duplexMode ) );
        }

        setLinkStatus( linkStatus, activeMedium );

        _media.linkStatus = linkStatus;
    }

    // Update current duplex mode.

    if ( duplexMode != _media.duplexMode )
    {
        setDuplexMode( duplexMode );
        _media.duplexMode = duplexMode;
    }

    // Update current link speed.

    if ( linkSpeed != _media.linkSpeed )
    {
        setLinkSpeed( linkSpeed );
        _media.linkSpeed = linkSpeed;
    }
}

//---------------------------------------------------------------------------
// autoSelectMediaPort
//
// Select the active media port.

bool Apple3Com3C90x::autoSelectMediaPort()
{
    bool      success    = true;
    MediaPort activePort = kMediaPortAuto;

    // Set linkTest flag to be able to use the KDP transmit and receive
    // methods before the driver is fully initialized.

    _linkTest = true;

    if ( ( _mediaOptions &
         ( kMediaOptionsBaseTXMask | kMediaOptionsBase10BTMask ) ) && 
         ( _adapterInfo->type >= kAdapterType3C90xB ) )
    {
        // Test NIC's internal auto-neg function for an
        // active 10Base-T or 100Base-TX link.

        LOG_MEDIA("%s::%s kMediaPortAutoNeg\n", getName(), __FUNCTION__);

        selectTransceiverPort( kMediaPortAutoNeg );

        if ( testNwayMIIPort() )
        {
            activePort = kMediaPortAutoNeg;
            goto done;
        }
    }
    if ( _mediaOptions &
         ( kMediaOptionsBaseMIIMask | kMediaOptionsBaseT4Mask ) )
    {
        // Test any MII device. T4 interface should also have
        // an external MII PHY device.

        LOG_MEDIA("%s::%s kMediaPortMII\n", getName(), __FUNCTION__);

        selectTransceiverPort( kMediaPortMII );

        if ( testNwayMIIPort() )
        {
            activePort = kMediaPortMII;
            goto done;
        }
    }
    if ( _mediaOptions & kMediaOptionsBaseFXMask )
    {
        LOG_MEDIA("%s::%s kMediaPort100BaseFX\n", getName(), __FUNCTION__);

        selectTransceiverPort( kMediaPort100BaseFX );

        if ( testLinkStatus() )
        {
            activePort = kMediaPort100BaseFX;
            goto done;
        }
    }
    if ( ( _mediaOptions & kMediaOptionsBaseTXMask ) &&
         ( _adapterInfo->type == kAdapterType3C90x ) )
    {
        LOG_MEDIA("%s::%s kMediaPort100BaseFX\n", getName(), __FUNCTION__);

        selectTransceiverPort( kMediaPort100BaseTX );

        if ( testLinkStatus() )
        {
            activePort = kMediaPort100BaseTX;
            goto done;
        }        
    }
    if ( ( _mediaOptions & kMediaOptionsBase10BTMask ) && 
         ( _adapterInfo->type == kAdapterType3C90x ) )
    {
        LOG_MEDIA("%s::%s kMediaPort10BaseT\n", getName(), __FUNCTION__);

        selectTransceiverPort( kMediaPort10BaseT );

        if ( testLinkStatus() )
        {
            activePort = kMediaPort10BaseT;
            goto done;
        }        
    }
    if ( _mediaOptions & kMediaOptionsBaseAUIMask )
    {
        LOG_MEDIA("%s::%s kMediaPortAUI\n", getName(), __FUNCTION__);

        selectTransceiverPort( kMediaPortAUI );

        if ( testLoopBack( kMediaPortAUI ) )
        {
            activePort = kMediaPortAUI;
            goto done;
        }
    }
    if ( _mediaOptions & kMediaOptionsBaseCoaxMask )
    {
        LOG_MEDIA("%s::%s kMediaPort10Base2\n", getName(), __FUNCTION__);

        selectTransceiverPort( kMediaPort10Base2 );

        if ( testLoopBack( kMediaPort10Base2 ) )
        {
            activePort = kMediaPort10Base2;
            goto done;
        }
    }

    success = false;
	LOG_MEDIA("%s::%s no active port found\n", getName(), __FUNCTION__);

done:
    _linkTest = false;

    // Update Media State.

    _media.mediaPort  = activePort;
    _media.duplexMode = kHalfDuplex;

    return success;
}

//---------------------------------------------------------------------------
// testLoopBack
//
// For AUI and 10Base-2 port detection.

bool Apple3Com3C90x::testLoopBack( MediaPort port )
{
    UInt16  macCtrl;
    bool    success = false;

    // Configure the NIC for external loopback by setting the
    // fullDuplexEnable bit in MacControl register.

    macCtrl = getMacControl();
    macCtrl |= kMacControlFullDuplexEnableMask;
    setMacControl( macCtrl );

    if ( port == kMediaPort10Base2 )
    {
        sendCommand( EnableDCConv );
        IOSleep(2);
    }

    for ( int i = 0; i < 2; i++ )
    {
        // Flush the receive ring.

        flushReceiveRing();

        // Send a self-addressed packet and wait 1500 milliseconds
        // to receive a return echo.

        transmitTestFrame();

        if ( receiveTestFrame( 1500 ) )
        {
            success = true;
            break;
        }
    }

    if ( port == kMediaPort10Base2 )
    {
        sendCommand( DisableDCConv );
        IOSleep(2);
    }

    // Clear fullDuplexEnable bit.

    macCtrl &= ~kMacControlFullDuplexEnableMask;
    setMacControl( macCtrl );

    return success;
}

//---------------------------------------------------------------------------
// testLinkStatus
//
// Determines the link state based on a combination of packets received,
// no link signal detection, and carrier sense.

bool Apple3Com3C90x::testLinkStatus()
{
    UInt32         i;
    UInt16         mediaStatus;
    const UInt32   maxLoops  = 5000;
    UInt32         csCounter = 0;
    bool           success   = false;

    // Set linkBeatEnable bit in the MediaStatus register. This bit
    // must be set in order for the linkDetect bit to be valid.
    
    mediaStatus = getMediaStatus();

    mediaStatus |= ( kMediaStatusLinkBeatEnabledMask    |
                     kMediaStatusJabberGuardEnabledMask );

    setMediaStatus( mediaStatus );

    // Enable promiscuous mode.

    sendCommand( SetRxFilter, kFilterPromiscuous );

    // Transmit a self-directed frame. Some hubs require this to
    // un-partition the port.

    transmitTestFrame();

    // Flush the receive ring before attempting to receive packets.

    flushReceiveRing();

    // Loop and listens for received packets. If linkDetect is off, then
    // the link is bad. if we receive a packet, then the link is good.

    for ( i = 0; i < maxLoops; i++ )
    {
        mediaStatus = getMediaStatus();
    
        if ( mediaStatus & kMediaStatusCarrierSenseMask )
            csCounter++;

        if ( ( mediaStatus & kMediaStatusLinkDetectMask ) == 0 )
        {
            success = false;
            break;
        }

        if ( receiveTestFrame( 0 ) )
        {
            success = true;
            LOG_MEDIA("%s::%s frame received\n", getName(), __FUNCTION__);
            break;
        }
    }

    // Restore original settings.

    mediaStatus &= ~( kMediaStatusLinkBeatEnabledMask    |
                      kMediaStatusJabberGuardEnabledMask );

    setMediaStatus( mediaStatus );

    sendCommand( SetRxFilter, _rxFilterMask );

    if ( i >= maxLoops )
    {
        // Did not receive any packets during the loop, and yet a valid link
        // was detected throughout. If carrierSense was on more than 25% of
        // the time, then a packet should have been received, therefore
        // consider the link bad.

        success = ( csCounter < (maxLoops / 4) );
    }

    LOG_MEDIA("%s::%s link is %s\n", getName(), __FUNCTION__,
              success ? "Good" : "Bad");

    return success;
}

//---------------------------------------------------------------------------
// transmitTestFrame
//
// Transmit a self-addressed min-sized Ethernet frame.

void Apple3Com3C90x::transmitTestFrame()
{
    UInt8 packet[ kIOEthernetMinPacketSize ];

    // Copy this stations ether address to the packet src + dst fields.
    
    bcopy( &_etherAddress, packet, kIOEthernetAddressSize );
    bcopy( &_etherAddress, packet + kIOEthernetAddressSize, kIOEthernetAddressSize );

    // Use the KDP send method to send out the frame.

    sendPacket( packet, kIOEthernetMinPacketSize );
}

//---------------------------------------------------------------------------
// receiveTestFrame
//
// Poll the receiver for a packet received within the timeout period.
// Returns true if a packet is received without errors.

bool Apple3Com3C90x::receiveTestFrame( UInt32 timeoutMS )
{
    UInt32  len = 0;

    receivePacket( NULL, &len, timeoutMS );

    return ( len > 0 );
}

//---------------------------------------------------------------------------
// flushReceiveRing
//
// Flush the receive ring.

void Apple3Com3C90x::flushReceiveRing()
{
    UInt32  len;

    for ( UInt32 i = 0; i < _rxRingSize; i++ )
    {
        receivePacket( NULL, &len, 1 );
    }
}

//---------------------------------------------------------------------------
// testNwayMIIPort

bool Apple3Com3C90x::testNwayMIIPort()
{
    // If the adapter has a MII port and there is a Nway capable PHY attached,
    // then return true to select the MII/AutoNeg port.

    return ( _media.phyAddress != kPHYAddressInvalid );
}

//---------------------------------------------------------------------------
// probeMediaSupport

void Apple3Com3C90x::probeMediaSupport()
{
    // Initialize the Media State.

    bzero( &_media, sizeof(_media) );

    _media.phyAddress = kPHYAddressInvalid;
    _media.phyID      = kPHYIDInvalid;

    // Auto is always supported.

	_media.mediaPortMask = (1 << kMediaPortAuto);

	// Convert the available ports reported by Media Options register
    // to a mask of Media Ports.

    if ( _mediaOptions & kMediaOptionsBase10BTMask )
         _media.mediaPortMask |= (1 << kMediaPort10BaseT);
    if ( _mediaOptions & kMediaOptionsBaseAUIMask )
         _media.mediaPortMask |= (1 << kMediaPortAUI);
    if ( _mediaOptions & kMediaOptionsBaseCoaxMask )
         _media.mediaPortMask |= (1 << kMediaPort10Base2);
    if ( _mediaOptions & kMediaOptionsBaseTXMask )
         _media.mediaPortMask |= (1 << kMediaPort100BaseTX);
    if ( _mediaOptions & kMediaOptionsBaseFXMask )
         _media.mediaPortMask |= (1 << kMediaPort100BaseFX);

    // Get the MII links.

    for ( PHYAddress addr = kPHYAddrMin; addr <= kPHYAddrMax; addr++ )
    {
        // Get the MII Links supported by the PHY.

        UInt32 linkMask = phyGetSupportedLinks( addr );

        if ( linkMask )
        {
            _media.phyAddress  = addr;
            _media.miiLinkMask = linkMask;
            _media.phyID       = phyGetIdentifier( addr );
            break;
        }
    }

    if ( _media.miiLinkMask & kMIILink10BT )
         _media.mediaPortMask |= (1 << kMediaPort10BaseT);
    if ( _media.miiLinkMask & kMIILink100TX )
         _media.mediaPortMask |= (1 << kMediaPort100BaseTX);
    if ( _media.miiLinkMask & kMIILink100T4 )
         _media.mediaPortMask |= (1 << kMediaPort100BaseT4);
    if ( _media.miiLinkMask & kMIILink10BT_FD )
         _media.mediaPortFDMask |= (1 << kMediaPort10BaseT);
    if ( _media.miiLinkMask & kMIILink100TX_FD )
         _media.mediaPortFDMask |= (1 << kMediaPort100BaseTX);

    LOG_MEDIA("%s::%s mediaPortMask %04lx FDMask %04lx miiLinkMask %02lx\n",
              getName(), __FUNCTION__,
              _media.mediaPortMask, _media.mediaPortFDMask,
              _media.miiLinkMask);
}

//---------------------------------------------------------------------------
// getIOMediumType

UInt32 Apple3Com3C90x::getIOMediumType( MediaPort  mediaPort,
                                        DuplexMode duplexMode )
{
    UInt32 mediumType = mediaPortTable[mediaPort].ioType;

    if ( duplexMode == kFullDuplex )
        mediumType |= kIOMediumOptionFullDuplex;
    else if ( mediaPort != kMediaPortAuto )
        mediumType |= kIOMediumOptionHalfDuplex;
    
    return mediumType;
}

//---------------------------------------------------------------------------
// addMediaPort

bool Apple3Com3C90x::addMediaPort( OSDictionary * mediaDict,
                                   MediaPort      mediaPort,
                                   DuplexMode     duplexMode )
{
    #define MBPS 1000000 

	IONetworkMedium	* medium;
    char              mediumName[ 40 ];
    UInt32            mediumType;
	bool              ret = false;

    snprintf( mediumName, 40, "%s%s", mediaPortTable[mediaPort].name,
                                      duplexMode == kFullDuplex ?
                                      " Full-Duplex" : "");

    mediumType = getIOMediumType( mediaPort, duplexMode );

    medium = IONetworkMedium::medium( mediumType,
                                      mediaPortTable[mediaPort].speed * MBPS,
                                      0,
                                      mediaPort,
                                      mediumName );

	if ( medium )
    {
		ret = IONetworkMedium::addMedium( mediaDict, medium );
		medium->release();
	}

	return ret;
}

//---------------------------------------------------------------------------
// publishMediaCapability

bool Apple3Com3C90x::publishMediaCapability( OSDictionary * mediaDict )
{
    for ( UInt32 port = 0; port < kMediaPortCount; port++ )
    {
        if ( mediaPortTable[port].selectable &&
            ( (1 << port) & _media.mediaPortMask ) )
        {
            addMediaPort( mediaDict, (MediaPort) port, kHalfDuplex );

            if ( (1 << port) & _media.mediaPortFDMask )
            {
                addMediaPort( mediaDict, (MediaPort) port, kFullDuplex );
            }
        }
    }
    
    return publishMediumDictionary( mediaDict );
}

//---------------------------------------------------------------------------
// checkMediaPortSupport
//
// Check if the adapter is able to support the selected Media Port.

bool Apple3Com3C90x::checkMediaPortSupport( MediaPort  mediaPort,
                                            DuplexMode duplexMode )
{
    bool support = false;

    do {
        // Is the chosen media port supported.

        if ( mediaPort >= kMediaPortCount )
            break;

        if ( !( _media.mediaPortMask & (1 << mediaPort) ) )
            break;

        // If full-duplex mode is requested, is it supported?

        if ( ( duplexMode == kFullDuplex ) &&
            !( _media.mediaPortFDMask & (1 << mediaPort) ) )
            break;

        support = true;
    }
    while ( false );

    LOG_MEDIA("%s::%s %s support = %s\n",  getName(), __FUNCTION__,
              mediaPortTable[mediaPort].name,
              support ? "true" : "false");

    return support;
}

//---------------------------------------------------------------------------
// getMIILinkFromMediaPort
//
// Return the corresponding MII Link from Media Port and Duplex Mode.

MIILink
Apple3Com3C90x::getMIILinkFromMediaPort( MediaPort  mediaPort,
                                         DuplexMode duplexMode )
{
    MIILink link;

    switch ( mediaPort )
    {
        case kMediaPort10BaseT:
            if ( duplexMode == kFullDuplex )
                link = kMIILink10BT_FD;
            else
                link = kMIILink10BT;
            break;

        case kMediaPort100BaseTX:
            if ( duplexMode == kFullDuplex )
                link = kMIILink100TX_FD;
            else
                link = kMIILink100TX;
            break;

        case kMediaPort100BaseT4:
            link = kMIILink100T4;
            break;

        default:
            link = kMIILinkNone;
    }

    LOG_MEDIA("%s::%s port %d converted to %lx\n",  getName(), __FUNCTION__,
              mediaPort, link);

    return link;
}

//---------------------------------------------------------------------------
// getMediaPortFromMIILink
//
// Return the Media Port from the MII Link.

MediaPort
Apple3Com3C90x::getMediaPortFromMIILink( MIILink miiLink )
{
    MediaPort mediaPort;

    switch ( miiLink & kMIILinkMask )
    {
        case kMIILink10BT:
        case kMIILink10BT_FD:
            mediaPort = kMediaPort10BaseT;
            break;

        case kMIILink100TX:
        case kMIILink100TX_FD:
            mediaPort = kMediaPort100BaseTX;
            break;

        case kMIILink100T4:
            mediaPort = kMediaPort100BaseT4;
            break;

        default:
            mediaPort = kMediaPortAuto;
            break;
    }

    LOG_MEDIA("%s::%s link %lx converted to %d\n",  getName(), __FUNCTION__,
              miiLink, mediaPort);

    return mediaPort;
}

//---------------------------------------------------------------------------
// getDuplexModeFromMIILink
//
// Return the Duplex Mode from the MII Link.

DuplexMode
Apple3Com3C90x::getDuplexModeFromMIILink( MIILink miiLink )
{
    if ( miiLink & ( kMIILink10BT_FD | kMIILink100TX_FD ) )
        return kFullDuplex;
    else
        return kHalfDuplex;
}

//---------------------------------------------------------------------------
// setupMIIMediaPort

bool
Apple3Com3C90x::mapSelectionToMIIMediaPort()
{
    bool success = true;

    // Indicate failure unless there is a PHY present.

    if ( _media.phyAddress == kPHYAddressInvalid )
        return false;

    // Schedule a PHY configuration on the next opportunity.

    _media.phyConfigured = false;

    if ( ( _media.mediaPort == kMediaPortAutoNeg ) ||
         ( _media.mediaPort == kMediaPortMII ) )
    {
        LOG_MEDIA("%s::%s NWay\n",  getName(), __FUNCTION__);

        _media.phyLink = ( kMIILinkNway | kMIILinkNone );
    }
    else
    {
        // Certain ports such as 10Base-T or 100Base-TX should be remapped
        // to use the AutoNeg or the MII port instead. For cards such as the
        // 3C900-TPO, this will fail and 'media' will remain unchanged. For
        // 3C90xB or the 3C905-TX (Boomerang) cards, we want this mapping to
        // occur.

        MIILink link = getMIILinkFromMediaPort( _media.mediaPort,
                                                _media.duplexMode );

        // Is the MII Link supported by the PHY?

        if ( link & _media.miiLinkMask )
        {
            LOG_MEDIA("%s::%s Non-NWay link %lx\n",  getName(), __FUNCTION__,
                      link);

            // Re-map the Media Port selection.

            if ( ( _adapterInfo->type >= kAdapterType3C90xB ) &&
                 ( _media.phyAddress == kPHYAddrCyclone ) )
                _media.mediaPort = kMediaPortAutoNeg;
            else
                _media.mediaPort = kMediaPortMII;

            // Record the non-nway link.

            _media.phyLink = link;
        }
        else
        {
            success = false;  // mapping to supported MII Link failed.
        }
    }

    LOG_MEDIA("%s::%s %s\n",  getName(), __FUNCTION__,
              success ? "success" : "failed");

    return success;
}

//---------------------------------------------------------------------------
// configurePHY

bool
Apple3Com3C90x::configurePHY()
{
    bool success = false;

    if ( ( _media.mediaPort == kMediaPortAutoNeg ) ||
         ( _media.mediaPort == kMediaPortMII ) )
    {
        if ( _media.phyLink & kMIILinkNway )
        {
            // Start negotiation.

            success = phyStartNegotiation( _media.phyAddress );
        }
        else
        {
            // Disable auto-negotiation, and force the PHY link.

            success = phyForceMIILink( _media.phyAddress,
                                       _media.phyLink );
        }

        _media.phyConfigured = true;
        _media.phyLinkActive = kMIILinkNone;
        _media.phyStatus     = 0;
        
        // Wait a bit after the PHY changes.
        
        IOSleep( 250 );
    }
    
    return success;
}

//---------------------------------------------------------------------------
// setMediaRegisters

bool
Apple3Com3C90x::setMediaRegisters()
{
    UInt16 macControl;
    UInt16 mediaStatus;

    if ( _media.phyAddress != kPHYAddressInvalid )
    {
        LOG_MEDIA("%s::%s MII link on PHY %d\n", getName(), __FUNCTION__,
                  _media.phyAddress);
    }
    else
    {
        LOG_MEDIA("%s: %s%s media port\n",
                  getName(), mediaPortTable[_media.mediaPort].name,
                  (_media.duplexMode == kFullDuplex) ? " Full-Duplex" : "");
    }

    LOG_MEDIA("  Port   : %s\n",     mediaPortTable[_media.mediaPort].name);
    LOG_MEDIA("  MIILink: %lx\n",   _media.phyLink);
    LOG_MEDIA("  PHYAddr: %d\n",    _media.phyAddress);
    LOG_MEDIA("  PHYID  : %08lx\n", _media.phyID);
    LOG_MEDIA("  FDuplex: %s\n",    _media.duplexMode == kFullDuplex ?
                                    "Yes" : "No");

    // Set MacControl register.

    setDuplexMode( _media.duplexMode );
    setLinkSpeed( _media.linkSpeed );

    macControl = getMacControl();

    if ( _extendAfterCollision )
        macControl |= kMacControlExtendAfterCollisionMask;
    else
        macControl &= ~kMacControlExtendAfterCollisionMask;

    setMacControl( macControl );

    // Select the transceiver port. According to the 3C90x
    // documentation it is necessary to issue a RxReset and
    // TxReset after the transceiver port is changed.

    selectTransceiverPort( _media.mediaPort );

    // Enable DC-DC converter for 10Base2.

    if ( _media.mediaPort == kMediaPort10Base2 )
        sendCommand( EnableDCConv );
    else
        sendCommand( DisableDCConv );
    IOSleep(2);  // recommended delay

    // Set MediaStatus register.

    mediaStatus = getMediaStatus();

    mediaStatus &= ~kMediaCodeMask;
    mediaStatus |= mediaPortTable[_media.mediaPort].mediaCode;

    setMediaStatus( mediaStatus );
    LOG_MEDIA("%s: port: %d mediaCode: %04x mediaStatus: %04x\n", getName(),
              _media.mediaPort, mediaPortTable[_media.mediaPort].mediaCode,
              mediaStatus);

    return true;
}

//---------------------------------------------------------------------------
// setLinkSpeed

void
Apple3Com3C90x::setLinkSpeed( LinkSpeed speed )
{
    LOG_MEDIA("%s: set %s link speed\n", getName(),
              speed == kLinkSpeed10 ? "10" : "100");

    // Set transmit threshold.

    if ( _storeAndForward == false )
    {
        if ( speed == kLinkSpeed10 )
            sendCommand( SetTxStartThresh, _txStartThresh_10 >> 2 );
        else /* kLinkSpeed100 */
            sendCommand( SetTxStartThresh, _txStartThresh_100 >> 2 );
    }
}

//---------------------------------------------------------------------------
// setDuplexMode

void
Apple3Com3C90x::setDuplexMode( DuplexMode mode )
{
    UInt16 macControl;

    LOG_MEDIA("%s: set %s duplex mode\n", getName(),
              mode == kFullDuplex ? "full" : "half");

    macControl = getMacControl();

    macControl &= ~( kMacControlFullDuplexEnableMask
                   | kMacControlFlowControlEnableMask );

    if ( mode == kFullDuplex )
    {
        macControl |= kMacControlFullDuplexEnableMask;

        if ( _flowControlEnabled )
            macControl |= kMacControlFlowControlEnableMask;
    }

    setMacControl( macControl );
}
