/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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

#include "3C90x.h"

//---------------------------------------------------------------------------
// setRegisterWindow

UInt8
Apple3Com3C90x::setRegisterWindow( UInt8 newWindow )
{
    UInt8 currentWindow = _window;

    if ( _window != newWindow )
    {
        sendCommand( SelectWindow, newWindow );
        _window = newWindow;
    }

    return currentWindow;
}

//---------------------------------------------------------------------------
// setStationAddress

void
Apple3Com3C90x::setStationAddress( const IOEthernetAddress * addr )
{
    setRegisterWindow( kStationAddressWindow );

    // Clear station address mask.

    for ( int i = 0 ; i < kIOEthernetAddressSize ; i++ )
        writeRegister8( kStationAddressMaskOffset + i, 0 );    
    
    // Set the station address.

    for ( int i = 0 ; i < kIOEthernetAddressSize ; i++ )
        writeRegister8( kStationAddressOffset + i, addr->bytes[i] );
}

//---------------------------------------------------------------------------
// sendCommand

void
Apple3Com3C90x::sendCommand( UInt16 cmd, UInt16 arg )
{
    setCommandStatus( cmd | (arg & 0x7ff) );
}

//---------------------------------------------------------------------------
// sendCommandWait

void
Apple3Com3C90x::sendCommandWait( UInt16 cmd, UInt16 arg )
{
    UInt32 us = 0;

    setCommandStatus( cmd | (arg & 0x7ff) );

    // Wait for command to complete
    
    while ( ( us < 1000000 ) &&
            ( getCommandStatus() & kCommandStatusCmdInProgressMask ) )
    {
        if ( us < 5000 )
        {
            IODelay( 1 );
            us++;
        }
        else
        {
            IOSleep( 10 );
            us += 10000;
        }
    }
}

//---------------------------------------------------------------------------
// selectTransceiverPort
//
// Sets the hardware port selection multiplexer to the desired port.
// The port selection bits are located in the InternalConfig register.

void
Apple3Com3C90x::selectTransceiverPort( MediaPort port )
{
    UInt32 internalConfig = getInternalConfig();

    internalConfig &= ~kInternalConfigXcvrSelectMask;
    internalConfig |= SetBitField( InternalConfig, XcvrSelect, port );

    setInternalConfig( internalConfig );
}

//---------------------------------------------------------------------------
// hashMulticastAddress - Based on the 3C90xB documentation.
//
// This function takes a 6-byte Ethernet address value and
// returns the bit position in the 3C905B multicast hash filter
// that corresponds to that address. It runs the AUTODIN II CRC
// algorithm on the address value, and then returns the lower
// eight bits of the CRC.
//
// Argument:
//     address - pointer to an Ethernet address
//
// Return Value:
//     filter bit position

UInt8
Apple3Com3C90x::hashMulticastAddress( UInt8 * Address )
{
    UInt32 Crc, Carry;
    UInt32 i, j;
    UInt8  ThisByte;

    /* Compute the CRC for the address value. */
    Crc = 0xffffffff;    /* initial value */
    
    /* For each byte of the address. */
    for (i = 0; i < 6; i++) {
        ThisByte = Address[i];
        
        /* For each bit in the byte. */
        for (j = 0; j < 8; j++) {
            Carry = ((Crc & 0x80000000) ? 1 : 0) ^ (ThisByte & 0x01);
            Crc <<= 1;
            ThisByte >>= 1;
            if (Carry)
                Crc = (Crc ^ 0x04c11db6) | Carry;
        }
    }

    /* Return the filter bit position. */
    return Crc & 0x000000FF;
}

//---------------------------------------------------------------------------
// waitForTransmitterIdle

void
Apple3Com3C90x::waitForTransmitterIdle()
{
    const int maxPollLoops = 1000 * 1000;  // wait up to 1 sec (BAD!)

    // Poll dnInProg bit in PktStatus register.
    
    for ( int i = maxPollLoops; i > 0; i-- )
    {
        if ( ( getDMACtrl() & kDMACtrlDnInProgMask ) == 0 )
            break;
        IODelay(1);
    }

    // Poll txInProg bit in MediaStatus register.

    for ( int i = maxPollLoops; i > 0; i-- )
    {
        if ( ( getMediaStatus() & kMediaStatusTxInProgMask ) == 0 )
            break;
        IODelay(1);
    }        
}

//---------------------------------------------------------------------------
// readEEPROM

#define kEEPROMPollLoops 10  // number of times to poll for not busy
#define kEEPROMPollSleep  1  // ms sleep before next poll

UInt16
Apple3Com3C90x::readEEPROM( UInt8 offset )
{
    UInt16 word;

    // Wait for EEPROM not busy.

    for ( int i = kEEPROMPollLoops; i > 0; i-- )
    {
        if ( ( getEEPROMCommand() & kEEPROMCommandBusyMask ) == 0 )
            break;
        IOSleep( kEEPROMPollSleep );
    }

    word = SetBitField( EEPROMCommand, Address, offset ) | kEEPROMOpcodeRead;

    setEEPROMCommand( word );

    // Wait for EEPROM not busy.

    for ( int i = kEEPROMPollLoops; i > 0; i-- )
    {
        if ( ( getEEPROMCommand() & kEEPROMCommandBusyMask ) == 0 )
            break;
        IOSleep( kEEPROMPollSleep );
    }

    return ( getEEPROMData() );
}

//---------------------------------------------------------------------------
// getStationAddress

void
Apple3Com3C90x::getStationAddress( IOEthernetAddress * addr )
{
    UInt8   eepromAddr = eepromOAddr0;
    bool    retry      = true;
    UInt16  word;

start:

    // Read from OEM address field first, then if necessary from the
    // 3Com address field.

    for ( int i = 0; i < 3; i++, eepromAddr++ )
    {
        // Wait for EEPROM not busy.
    
        for ( int j = kEEPROMPollLoops; j > 0; j-- )
        {
            if ( ( getEEPROMCommand() & kEEPROMCommandBusyMask ) == 0 )
                break;
            IOSleep( kEEPROMPollSleep );
        }

        word = SetBitField( EEPROMCommand, Address, eepromAddr ) |
               kEEPROMOpcodeRead;
        
        setEEPROMCommand( word );

        // Wait for EEPROM not busy.
    
        for ( int j = kEEPROMPollLoops; j > 0; j-- )
        {
            if ( ( getEEPROMCommand() & kEEPROMCommandBusyMask ) == 0 )
                break;
            IOSleep( kEEPROMPollSleep );
        }

        word = getEEPROMData();   
        addr->bytes[i*2]   = (word >> 8) & 0xff;
        addr->bytes[i*2+1] = (word & 0xff);
    }

    // If the OEM fields are all zeroes, then try the 3Com node
    // address fields.

    if ( retry )
    {
        for ( int i = 0; i < 6; i++ )
        {
            if ( addr->bytes[i] != 0 ) return;
        }
        eepromAddr = eepromAddr0;
        retry      = false;

        goto start;
    }
}
