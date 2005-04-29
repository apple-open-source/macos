/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 * chipPrimatives.cpp
 *
 * MacOSX implementation of Serial Port driver
 *
 *
 * Humphrey Looney	MacOSX IOKit ppc
 * Elias Keshishoglou 	MacOSX Server ppc
 * Dean Reece		Original Next Intel  version
 *
 * 18/04/01 David Laupmanis		Add SccConfigForMIDI function to configure the SCC
 *								for MIDI i.e. turn off the baud rate generator, set
 *								parity off, one stop bit.
  *
 *
 * 02/07/01	Paul Sun	Implemented the software and hardware flow control.
 *
 * 01/27/01	Paul Sun	Fixed bug # 2550140 & 2553750 by adding mutex locks
 *						in the following routines: SccdbdmaStartTansmission(),
 *						SccFreeReceptionChannel(), SccdbdmaRxHandleCurrentPosition(),
 *						and  SccFreeTanmissionChannel().
 *
 * Copyright ©: 	1999 Apple Computer, Inc.  all rights reserved.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <IOKit/IOTimerEventSource.h>

#include <sys/kdebug.h>

#include "Z85C30.h"
#include "PPCSerialPort.h"
#include "SccChipPrimatives.h"
#include "SccQueuePrimatives.h"

#include <IOKit/serial/IORS232SerialStreamSync.h>

extern void flush_dcache(vm_offset_t addr, unsigned count, int phys);

UInt32 gTimerCanceled = 0;

bool CommandExecuted(SccChannel *Channel, UInt32 iterator);
bool SuckDataFromTheDBDMAChain(SccChannel *Channel);
UInt32 CommandStatus(SccChannel *Channel, UInt32 commandNumber);

#if USE_WORK_LOOPS
void rearmRxTimer(SccChannel *Channel, UInt32 timerDelay);
#endif



//#define SHOW_DEBUG_STRINGS
#ifdef  SHOW_DEBUG_STRINGS
#define DLOG(fmt, args...)      IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

/* Marco:
   the following define forces the code to ignore that this is an APPLE
   SCC implementation and trys to read the registers as they were in the
   original Z85C30 */
#define STRICT_85C30

/* Marco:
   The SCC may need a short delay when accessing to its registers, the
   delay is defined here: */
#if 0
#define REGISTER_DELAY() IODelay(10)
#else
#define REGISTER_DELAY()
#endif

/* Array for resetting data bits */
static UInt8 LocalDataBits[2][4] = {
    { kRx5Bits, kRx6Bits, kRx7Bits, kRx8Bits},
    { kTx5Bits, kTx6Bits, kTx7Bits, kTx8Bits}
};



/* initChip() - This function sets up various default values for UART
 * parameters, then calls programChip().  It is intended to be called
 * from the init method only.
 */
void initChip(PortInfo_t *port)
{
    port->TX_Parity = PD_RS232_PARITY_NONE;
    port->RX_Parity = PD_RS232_PARITY_DEFAULT;
    port->DLRimage = 0x0000;
    port->FCRimage = 0x00;

    /* Added this ejk.*/
    ProbeSccDevice(port);
}


/* programChip() - This function programs the UART according to the
 * various variables in the port struct.  This function can be called
 * from any context is the primary method of accessing the hardware,
 * other than the AppleSCCSerialIntHandler() function.
 */
void programChip(PortInfo_t *port)
{
}

/*---------------------------------------------------------------------
 *		ProbeSccDevice
 * ejk		Thu, Jan 1, 1998	15:25
 *		First determine if we have a device where we think we do.
 *	If we do then set it and some of the structures up to be able to use it.
 *
 *-------------------------------------------------------------------*/
bool ProbeSccDevice( PortInfo_t *Port) {

    DLOG("In ProbeSccDevice\n");

    Port->DataRegister 		= (UInt8 *)(Port->ChipBaseAddress + channelDataOffsetRISC);
    Port->ControlRegister 	= (UInt8 *)(Port->ChipBaseAddress + channelControlOffsetRISC);

    /* This is very bad but it will work with all of our hardware.
        The right thing to do is to get the instance variables of the channel
        and determine which one is channel B and set it to that value.
    */
    Port->ConfigWriteRegister 		= (unsigned int)(Port->ChipBaseAddress & 0xffffff00);

    Port->baudRateGeneratorEnable	= kBRGEnable;
    Port->rtxcFrequency			= 3686400;

        /* FIXTHIS  ejk	*/
    SccWriteReg(Port, R1, 0);

    DLOG("SccProbe %x %x\n", (int)Port->DataRegister, (int)Port->ControlRegister);
    return(TRUE);
}

#define USE_OLD_SCC_ROUTINES 0


#if USE_OLD_SCC_ROUTINES
/*---------------------------------------------------------------------
*		New FixZeroBug
*
*		Works around a bug in the SCC receving channel.
*
*-------------------------------------------------------------------*/
void FixZeroBug(SccChannel *Channel)
{
    /*
     HDL SCC “Stuck Zero” Workaround

     The following sequence prevents a problem that is seen with O’Hare ASICs
     (most versions -- also with some Heathrow and Hydra ASICs) where a zero
     at the input to the receiver becomes “stuck” and locks up the receiver.

     Affected machines include the following shipping machines and prototypes:

     CODE NAME		SHIPPED AS

     “Alchemy”		Performa 5400/6400
     “Gazelle”		Performa 6500
     “Spartacus”	20th Anniversary Mac
     “Comet”		PowerBook 2400
     “Hooper”		PowerBook 3400
     “Tanzania”		Motorola StarMax 3000, PowerMac 4400
     “Gossamer”		Power Macintosh G3 <----- This is supported by MacOS X so
                                                  this workaround is required.

     “Viper”			n/a (Apple/Motorola CHRP EVT2 prototype, < Hydra 4)

     “PowerExpress”	cancelled

     This problem can occur as a result of a zero bit at the receiver input
     coincident with any of the following events:

     •	The SCC is initialized (hardware or software).
     •	A framing error is detected.
     •	The clocking option changes from synchronous or X1 asynchronous
        clocking to X16, X32, or X64 asynchronous clocking.
     •	The decoding mode is changed among NRZ, NRZI, FM0, or FM1.

     This workaround attempts to recover from the lockup condition by placing
     the SCC in synchronous loopback mode with a fast clock before programming
     any of the asynchronous modes.

     The necessity of the workaround is determined at module initialization.
     */

    SccWriteReg(Channel, 9, kChannelResetA | kChannelResetB);
    IOSleep(10);

    SccWriteReg(Channel, 9, (Channel->whichPort == serialPortA ? kChannelResetA : kChannelResetB) | kNV);

    SccWriteReg(Channel, 4, kX1ClockMode | kExtSyncMode);
    SccWriteReg(Channel, 3, 8 & ~kRxEnable);
    SccWriteReg(Channel, 5, 8 & ~kTxEnable);
    SccWriteReg(Channel, 9, kNV);						// no interrupt vector
    SccWriteReg(Channel, 11, kRxClockBRG | kTxClockBRG);
    SccWriteReg(Channel, 12, 0);							// BRG low-order count
    SccWriteReg(Channel, 13, 0);							// BRG high-order count
    SccWriteReg(Channel, 14, kLocalLoopback | kBRGFromPCLK);

    SccWriteReg(Channel, 14, kLocalLoopback | kBRGFromPCLK | kBRGEnable);
    SccWriteReg(Channel, 3, 8 | kRxEnable);

    SccWriteReg(Channel, 0, kResetExtStsInt);			// reset pending Ext/Sts interrupts
    SccWriteReg(Channel, 0, kResetExtStsInt);			// (and kill some time)

    // The channel should be OK now, but it is probably receiving loopback garbage.

    // Switch to asynchronous mode, disable the receiver,
    // and discard everything in the receive buffer.

    SccWriteReg(Channel, 9, kNV);

    SccWriteReg(Channel, 4, 1);
    SccWriteReg(Channel, 3, 8 & ~kRxEnable);

    while (SccReadReg(Channel, R0) & kRxCharAvailable) {
        (void) SccReadReg(Channel, R8);	// 8 is the data register
        SccWriteReg(Channel, R0, kResetExtStsInt ); // and reset possible errors
        SccWriteReg(Channel, R0, kErrorReset); //  .. all the possible errors
    }
}


#else

/*---------------------------------------------------------------------
*		FixZeroBug
*
*		Works around a bug in the SCC receving channel.
*
*-------------------------------------------------------------------*/
void FixZeroBug(SccChannel *Channel)
{
    /*
     HDL SCC “Stuck Zero” Workaround

     The following sequence prevents a problem that is seen with O’Hare ASICs
     (most versions -- also with some Heathrow and Hydra ASICs) where a zero
     at the input to the receiver becomes “stuck” and locks up the receiver.

     Affected machines include the following shipping machines and prototypes:

     CODE NAME		SHIPPED AS

     “Alchemy”		Performa 5400/6400
     “Gazelle”		Performa 6500
     “Spartacus”	20th Anniversary Mac
     “Comet”		PowerBook 2400
     “Hooper”		PowerBook 3400
     “Tanzania”		Motorola StarMax 3000, PowerMac 4400
     “Gossamer”		Power Macintosh G3 <----- This is supported by MacOS X so
                                                  this workaround is required.

     “Viper”			n/a (Apple/Motorola CHRP EVT2 prototype, < Hydra 4)

     “PowerExpress”	cancelled

     This problem can occur as a result of a zero bit at the receiver input
     coincident with any of the following events:

     •	The SCC is initialized (hardware or software).
     •	A framing error is detected.
     •	The clocking option changes from synchronous or X1 asynchronous
        clocking to X16, X32, or X64 asynchronous clocking.
     •	The decoding mode is changed among NRZ, NRZI, FM0, or FM1.

     This workaround attempts to recover from the lockup condition by placing
     the SCC in synchronous loopback mode with a fast clock before programming
     any of the asynchronous modes.

     The necessity of the workaround is determined at module initialization.
     */
     
     
    SccWriteReg(Channel, 9, (Channel->whichPort == serialPortA ? kChannelResetA : kChannelResetB) | kNV);

    SccWriteReg(Channel, 4, kX1ClockMode | k8BitSyncMode);	//used to be ExtSyncMode
    SccWriteReg(Channel, 3, kRx8Bits & ~kRxEnable);
    SccWriteReg(Channel, 5, kTx8Bits | kRTS & ~kTxEnable);
    SccWriteReg(Channel, 9, kNV);						// no interrupt vector
    SccWriteReg(Channel, 11, kRxClockBRG | kTxClockBRG);
    SccWriteReg(Channel, 12, 0);							// BRG low-order count
    SccWriteReg(Channel, 13, 0);							// BRG high-order count
    SccWriteReg(Channel, 14, kLocalLoopback | kBRGFromPCLK);

    SccWriteReg(Channel, 14, kLocalLoopback | kBRGFromPCLK | kBRGEnable);
    SccWriteReg(Channel, 3, kRx8Bits | kRxEnable);

    SccWriteReg(Channel, 0, kResetExtStsInt);			// reset pending Ext/Sts interrupts
    SccWriteReg(Channel, 0, kResetExtStsInt);			// (and kill some time)

    // The channel should be OK now, but it is probably receiving loopback garbage.

    // Switch to asynchronous mode, disable the receiver,
    // and discard everything in the receive buffer.

    SccWriteReg(Channel, 9, kNV);

    SccWriteReg(Channel, 4, kX16ClockMode | kStopBitsMask);
    SccWriteReg(Channel, 3, kRx8Bits & ~kRxEnable);

    while (SccReadReg(Channel, R0) & kRxCharAvailable) {
        (void) SccReadReg(Channel, R8);	// 8 is the data register
        SccWriteReg(Channel, R0, kResetExtStsInt ); // and reset possible errors
        SccWriteReg(Channel, R0, kErrorReset); //  .. all the possible errors
    }
}
#endif

#if USE_OLD_SCC_ROUTINES

/*---------------------------------------------------------------------
 *		OpenScc
 * ejk		Thu, Jan 1, 1998	19:49
 *		This routine Opens a serial port.
 *
 *-------------------------------------------------------------------*/
bool OpenScc(SccChannel *Channel)
{
    UInt32	i;

    /* SCC hardware configuration table
    */
    static UInt8 SCCConfigTable[] = {
        R9, 0,					// WR  9, set below depending from the port:
        R4, kX16ClockMode | k1StopBit,		// WR  4, 16x clock, 1 stop bit, no parity (this could be 0x04 right? - ÖMB)
        R3, kRx8Bits,				// WR  3, Rx 8 bits, disabled
        R5, kDTR | kTx8Bits | kRTS,		// WR  5, Tx 8 bits, DTR and RTS asserted, transmitter disabled
        R2, 0x00,				// WR  2, zero interrupt vector
        R10, kNRZ,				// WR 10, NRZ encoding
        R11, kRxClockBRG | kTxClockBRG,		// WR 11, baud rate generator clock to receiver, transmitter
        R12, 0x00,				// WR 12, low byte baud rate ( 0x00 = 57.6K, 0x06 = 14400, 0x0A = 9600 baud ) -- 3.6864 MHz, 16x clock
        R13, 0x00,				// WR 13, high byte baud rate (9.6K up)
        R3, kRx8Bits | kRxEnable,		// WR  3, Rx 8 bits, enabled
        R5, kDTR | kTx8Bits | kTxEnable | kRTS, 	// WR  5, Tx 8 bits, DTR and RTS asserted, transmitter enabled
        R14, kBRGEnable,				// WR 14, enable baud rate generator
        R15, kCTSIE | kDCDIE,				// WR 15, enable CTS Ecternal/Status interrupt, disable break/abort interrupts
        R0, kResetExtStsInt,			// WR  0, reset ext/status interrupts
        R0, kResetExtStsInt,			// WR  0, reset ext/status interrupts (again)
        R1, kDMAReqSelect | kDMAReqOnRx | kDMAReqOnTx | kWReqEnable | kExtIntEnable | kRxIntOnlySC,	// WR  1, receive interrupts (but not external & status) enabled
        R9, kMIE | kNV				// WR  9, SCC interrupts enabled (MIE), status in low bits
    };
    UInt32 SCCConfigTblSize = sizeof(SCCConfigTable)/sizeof(UInt8);	// number of entries in SCC configuration table
    DLOG("In OpenSCC %d\n", Channel->whichPort);

   // Make sure the scc commands are appropr
    if ( Channel->whichPort == serialPortB )
        SCCConfigTable[1] |= kChannelResetB;
    else
        SCCConfigTable[1] |= kChannelResetA;

    // Fix the case of a "0" stuck in the receiver:
    FixZeroBug(Channel);

    // now, initialize the chip
    for ( i = 0; i < SCCConfigTblSize; i += 2 )		// write configuration table info to the SCC
    {
        SccWriteReg(Channel, SCCConfigTable[i], SCCConfigTable[i + 1] );
    }
    
    // Eanbles the chip interrupts:
    SccEnableInterrupts( Channel, kSccInterrupts, 0 );

    // Free the fifo in case of leftover errors:
    SccWriteReg(Channel, R0, kErrorReset);
    SccWriteReg(Channel, R0, kErrorReset);

    DLOG("PPCSerOpen End \n");

    // we're done - let the caller know if we had any problems
    return( TRUE );
}
#else
bool OpenScc(SccChannel *Channel)
{
    UInt32	i;
   
    /* SCC hardware configuration table
    */
    static UInt8 SCCConfigTable[] = {
     //   R9, kNV,					// WR  9, set below depending from the port: // jdg: static table shared, moved to code
        R4, /*kX16ClockMode |*/ k1StopBit,		// WR  4, 16x clock, 1 stop bit, no parity (this could be 0x04 right? - ÖMB)
        R1, kDMAReqSelect | kDMAReqOnRx | kParityIsSpCond,
        R3, kRx8Bits & ~kRxEnable,				// WR  3, Rx 8 bits, disabled
        R5, /*kDTR | */ kTx8Bits | kRTS & ~kTxEnable,		// WR  5, Tx 8 bits, DTR and RTS asserted, transmitter disabled
      //  R2, 0x00,				// WR  2, zero interrupt vector
        R9, kNV,					// WR  9, set below depending from the port:
        R10, kNRZ,				// WR 10, NRZ encoding
        R11, kRxClockBRG | kTxClockBRG,		// WR 11, baud rate generator clock to receiver, transmitter
        R12, 0x0A,				// WR 12, low byte baud rate ( 0x00 = 57.6K, 0x06 = 14400, 0x0A = 9600 baud ) -- 3.6864 MHz, 16x clock
        R13, 0x00,				// WR 13, high byte baud rate (9.6K up)
        R14, kBRGFromRTxC,				// WR 14, RTxC clock drives the BRG
        R15, kERegEnable,				// WR 15, Enhancement register 7' enabled
        R7, kEReq,						// WR 7, DTR/REQ timing = W/REQ timing
        R14, kBRGEnable,				// WR 14, enable baud rate generator
        R3, kRx8Bits | kRxEnable,		// WR  3, Rx 8 bits, enabled
        R5, kTx8Bits | kTxEnable | kRTS, 	// WR  5, Tx 8 bits, RTS asserted, transmitter enabled
        R1, kDMAReqSelect | kDMAReqOnRx | kParityIsSpCond | kWReqEnable,
        R15, kBreakAbortIE | kCTSIE | kDCDIE | kERegEnable,	// WR 15, Enhancement register 7' enabled
        R0, kResetExtStsInt,			// WR  0, reset ext/status interrupts
        R0, kResetExtStsInt,			// WR  0, reset ext/status interrupts (again)
        R1, kDMAReqSelect | kDMAReqOnRx | kParityIsSpCond | kExtIntEnable | kRxIntOnlySC | kWReqEnable,	// WR  1, receive interrupts (but not external & status) enabled
        R9, kMIE | kNV				// WR  9, SCC interrupts enabled (MIE), status in low bits
    };
    UInt32 SCCConfigTblSize = sizeof(SCCConfigTable)/sizeof(UInt8);	// number of entries in SCC configuration table
    DLOG("In OpenSCC %d\n", Channel->whichPort);

    /*** jdg: moved to code (below) so we don't have to worry about the table being shared between sccA and sccB
   // Make sure the scc commands are appropr
    if ( Channel->whichPort == serialPortB )
        SCCConfigTable[1] |= kChannelResetB;
    else
        SCCConfigTable[1] |= kChannelResetA;
    ***/
    
    // Fix the case of a "0" stuck in the receiver:
    FixZeroBug(Channel);

    // now, initialize the chip
    SccWriteReg(Channel, 9, (Channel->whichPort == serialPortA ? kChannelResetA : kChannelResetB) | kNV);
    for ( i = 0; i < SCCConfigTblSize; i += 2 )		// write configuration table info to the SCC
            SccWriteReg(Channel, SCCConfigTable[i], SCCConfigTable[i + 1] );

    // Eanbles the chip interrupts:
    SccEnableInterrupts( Channel, kSccInterrupts, 0 );

    // Free the fifo in case of leftover errors:
    SccWriteReg(Channel, R0, kErrorReset);
    SccWriteReg(Channel, R0, kErrorReset);

    DLOG("PPCSerOpen End \n");

    // we're done - let the caller know if we had any problems
    return( TRUE );
}
#endif

void SccCloseChannel(SccChannel *Channel)
{
    InterruptState	dontCare;

    // This is to be sure that SccHandleMissedTxInterrupt ends
    if (Channel->AreTransmitting == TRUE) {
        Channel->AreTransmitting = FALSE;
        IOSleep(1000);
    }
    
    dontCare = SccDisableInterrupts( Channel, kSccInterrupts );		// Disable scc interrupts before doing anything
    dontCare = SccDisableInterrupts( Channel, kRxInterrupts );		// Disable the receiver
    dontCare = SccDisableInterrupts( Channel, kTxInterrupts );		// Disable the transmitter

    // Disable the Wait/Request function and interrupts.
    SccWriteReg(Channel, R1, kWReqDisable );

    // Set RTxC clocking and disable the baud rate generator.
    SccWriteReg(Channel, R11, kRxClockRTxC | kTxClockRTxC );
    SccWriteReg(Channel, R14, kBRGDisable );

    SccWriteReg(Channel, R15, kDCDIE );					// enable DCD interrupts (?!)
    SccWriteReg(Channel, R0, kResetExtStsInt );
    SccWriteReg(Channel, R0, kResetExtStsInt );				// reset pending Ext/Sts interrupts
    SccWriteReg(Channel, R1, kExtIntEnable );				// external/status interrupts enabled

    if ( Channel->whichPort == serialPortA )
        SccWriteReg(Channel, R9, kChannelResetA );
    else
        SccWriteReg(Channel, R9, kChannelResetB );

    // If we are running on DMA turs off the dma
    // interrupts:
    SccEnableDMAInterruptSources(Channel, false);

    DLOG("In SccCloseChannel %d\n", Channel->whichPort);
}

/*---------------------------------------------------------------------
 *		SccSetStopBits
 * ejk		Fri, Jan 2, 1998	00:05
 *		Set the number of stop bits for the current channel.
 *
 *-------------------------------------------------------------------*/
bool SccSetStopBits(SccChannel *Channel, UInt32 otSymbolic )
{
    UInt8			value;

    DLOG("SccSetStopBits %d\n", (int)otSymbolic);

    switch( otSymbolic ) {
        case 00 :
            value = 0x00;
            break;
        case 2 :
            value = k1StopBit;
            break;
        case 3 :
            value = k1pt5StopBits;
            break;
        case 4 :
            value = k2StopBits;
            break;
        default :
            return FALSE;
            break;
    }

    SccWriteReg(Channel, R4, (Channel->lastWR[ 4 ] & ~kStopBitsMask ) | value );
    return TRUE;
}

/*---------------------------------------------------------------------
 *		SccSetParity
 * ejk		Fri, Jan 2, 1998	00:02
 *		Set the parity of the current Channel.
 *
 *-------------------------------------------------------------------*/
bool SccSetParity(SccChannel *Channel, ParityType ParitySetting )
{
    DLOG("SccSetParity %d\n",ParitySetting);

    switch( ParitySetting) {
        case PD_RS232_PARITY_NONE:  // PD_RS232_PARITY_NONE = 1
            SccWriteReg(Channel, 4, Channel->lastWR[4] & ~kParityEnable );
            SccWriteReg(Channel, 1, Channel->lastWR[1] & ~kParityIsSpCond );
            break;
        case PD_RS232_PARITY_ODD:
            SccWriteReg(Channel, 4, Channel->lastWR[4] & ~kParityEven | kParityEnable );
            SccWriteReg(Channel, 1, Channel->lastWR[1] | kParityIsSpCond );
            break;
        case PD_RS232_PARITY_EVEN:
            SccWriteReg(Channel, 4, Channel->lastWR[4] | kParityEven | kParityEnable );
            SccWriteReg(Channel, 1, Channel->lastWR[1] | kParityIsSpCond );
            break;
        case PD_RS232_PARITY_MARK:
        case PD_RS232_PARITY_SPACE :
        default:
            return FALSE;
    }

    SccEnableInterrupts( Channel, kRxInterrupts, 0 );
    SccEnableInterrupts( Channel, kSccInterrupts, 0 );

    // reminds the receiver to call when a new character
    // enters in the fifo:
    SccWriteReg(Channel, R0, kResetRxInt);

    // And to wake me up when one of the status bits changes
    SccWriteReg(Channel, R0, kResetExtStsInt );

    return TRUE;
}

/*---------------------------------------------------------------------
 *		SccSetDataBits
 * ejk		Thu, Jan 1, 1998	22:03
 *		Reset the number of data bits for this port.
 *
 *-------------------------------------------------------------------*/
bool SccSetDataBits(SccChannel *Channel, UInt32 numDataBits )
{
    if (numDataBits >= 5 && numDataBits <= 8) {
        numDataBits -= 5;	 /* let the index start at 0 */

        DLOG("In Set Data Bits %d Tx %x Rx %x\n", (int)numDataBits , LocalDataBits[1][numDataBits],
        LocalDataBits[0][numDataBits]);

        /* Set Tx Bits. */
        SccWriteReg(Channel, R5, ( Channel->lastWR[ 5 ] & ~kTxBitsMask ) | LocalDataBits[1][numDataBits]);

        /* Set Rx Bits. */
        SccWriteReg(Channel, R3, ( Channel->lastWR[ 3 ] & ~kRxBitsMask ) | LocalDataBits[0][numDataBits]);

        return TRUE;
    }
    return FALSE;
}

/*---------------------------------------------------------------------
 *		SccSetCTSFlowControlEnable
 * hsjb		Mon, Aug 20, 2001	16:00
 *		Turns on and off CTS
 *
 *-------------------------------------------------------------------*/
void SccSetCTSFlowControlEnable(SccChannel *Channel, bool enableCTS )
{
    if ( enableCTS )
    {
        SccWriteReg(Channel, R15, Channel->lastWR[ 15 ] | kCTSIE );
    }
    else
    {
        SccWriteReg(Channel, R15, Channel->lastWR[ 15 ] & ~kCTSIE );
    }
}

/*---------------------------------------------------------------------
 *		SccChannelReset
 * ejk		Thu, Jan 1, 1998	21:53
 *		This Routine will send a reset to the specific channel of
 *	85C30 chip.
 *
 *-------------------------------------------------------------------*/
void SccChannelReset(SccChannel *Channel)
{
    switch(Channel->whichPort ) {
        case serialPortA:
            SccWriteReg(Channel, R9, kChannelResetA | kNV );
            break;
        case serialPortB:
            SccWriteReg(Channel, R9, kChannelResetB | kNV );
            break;
        default:
            break;
    }
}

/*---------------------------------------------------------------------
 *		SccSetBaud
 * ejk		Thu, Jan 1, 1998	21:05
 *		This routine will set the baud rate of the specified SCC Port.
 *
 *-------------------------------------------------------------------*/
bool SccSetBaud(SccChannel *Channel, UInt32 NewBaud)
{
    UInt32	brgConstant;
    UInt8	wr4Mirror;

    /* disables the interrupts */
    InterruptState previousState = SccDisableInterrupts(Channel, kSerialInterrupts);

    /* Tricky thing this, when we wish to go up in speed we also need to switch the
        * clock generator for the via:
        */
    wr4Mirror = Channel->lastWR[ 4 ] & (~kClockModeMask);
    if (NewBaud == 115200) {
        SccWriteReg(Channel, 4, wr4Mirror | kX32ClockMode);
    }
    else {
        SccWriteReg(Channel, 4, wr4Mirror | kX16ClockMode);
    }

    /* Calculate the closest SCC baud rate constant (brgConstant) and actual rate */
    /* This is what we should have as default value */
    brgConstant = 0;

    if ((NewBaud < 115200) && (NewBaud > 0)) {
        /* The fundamental expression is (rtxcFrequency / (2 * clockMode * baudRate) - 2).
        It is necessary, however, to round the quotient to the nearest integer.
        */
        brgConstant = -2 + ( 1 + Channel->rtxcFrequency / ( 16 * NewBaud )) / 2;

        /* Pin 0x0000 ≤ brgConstant ≤ 0xFFFF.
        */
        if ( brgConstant < 0 ) {
            brgConstant = 0;
        }
        else {
            if ( brgConstant > 0xFFFF )
                brgConstant = 0xFFFF;
        }
    }

    /* Again, round correctly when calculating the actual baud rate.
        */
    Channel->baudRateGeneratorLo = (UInt8)brgConstant;			// just the low byte
    Channel->baudRateGeneratorHi = (UInt8)(brgConstant >> 8 );		// just the high byte

    SccWriteReg(Channel, R14, Channel->lastWR[ 14 ] & (~kBRGEnable));
    //  SccWriteReg(Channel, R11, (kRxClockBRG | kTxClockBRG) );
    SccWriteReg(Channel, R12, Channel->baudRateGeneratorLo );
    SccWriteReg(Channel, R13, Channel->baudRateGeneratorHi );

    if ((NewBaud == 115200) || (NewBaud == 230400))
        SccWriteReg(Channel, R11, (kRxClockRTxC | kTxClockRTxC) );
    else {
        SccWriteReg(Channel, R11, (kRxClockBRG | kTxClockBRG) );
        SccWriteReg(Channel, R14, (kBRGEnable) );
    }

    /* And at the end re-enables the interrupts */
    SccEnableInterrupts(Channel, kSerialInterrupts,previousState);

    /* Set the global parameter.*/
    Channel->BaudRate = NewBaud;

    return TRUE;
}

/*---------------------------------------------------------------------
 *		SccConfigForMIDI
 * 	dgl (laupmanis)	Thu, Apr 18, 2001	21:05
 *
 *		This routine will configure the SCC Port for MIDI externally
 *		clocked devices. Turn off the buad rate generator, and set the 
 *		clock mode to one of 4 speeds.  MIDI's default rate = 31250.
 *		Clock modes are either 1, 16, 32, or 64 times the 31250 rate.
 *		See Z85C30.h for the constants.
 *-------------------------------------------------------------------*/
bool SccConfigureForMIDI(SccChannel *Channel, UInt32 ClockMode)
{  
	Channel->baudRateGeneratorLo = 0x00;
	Channel->baudRateGeneratorHi = 0x00;
 	
    // Disable interrupts
    InterruptState previousState = SccDisableInterrupts(Channel, kSerialInterrupts);

	    //  Set clock mode.
	    SccWriteReg(Channel, R4, (Channel->lastWR[ 4 ] & ~kClockModeMask | ClockMode));

	    // Transmit/Receive clock = TRxC pin 
	    SccWriteReg(Channel, R11, (kRxClockTRxC | kTxClockTRxC) );

	    // Disable Baud Rate Generator
	    SccWriteReg(Channel, R14, Channel->lastWR[14] & (~kBRGEnable) );
	    SccWriteReg(Channel, R12, Channel->baudRateGeneratorLo );
	    SccWriteReg(Channel, R13, Channel->baudRateGeneratorHi );
	    
	    // Enable external interupts
	    SccWriteReg(Channel, R1, Channel->lastWR[ 1 ] & ~kExtIntEnable );					
	    
	    // Disable CTS handshaking
	    SccWriteReg(Channel, R15, Channel->lastWR[ 15 ] & ~kCTSIE );					
    
    // Re-enable interrupts 
    SccEnableInterrupts(Channel, kSerialInterrupts,previousState);
 
    return TRUE; 
}

/*---------------------------------------------------------------------
 *		SccReadReg
 * ejk		Thu, Jan 1, 1998	23:10
 *		Read a register from the 8530
 *
 *-------------------------------------------------------------------*/
UInt8	SccReadReg(SccChannel *Channel, UInt8 sccRegister) {
    UInt8	ReturnValue;

//    IOLockLock(Channel->SCCReadLock);
//rcs    IOLockLock(Channel->SCCAccessLock);
    if (!IOLockTryLock(Channel->SCCAccessLock))
		return -1;
		
    // Make sure we have a valid register number to write to.
    if (sccRegister != R0 ) {

        // First let's write the register value to the chip.
        *((volatile UInt8 *) Channel->ControlRegister) = sccRegister;
        SynchronizeIO();
        REGISTER_DELAY();
    }

    // Next we'll write the data value.
    ReturnValue = *((volatile UInt8 *) Channel->ControlRegister);
    
    //DLOG("SccRead %x %x %x\n", Channel->ControlRegister, sccRegister, ReturnValue);
    IOLockUnlock(Channel->SCCAccessLock);

    return ReturnValue;
}

/*---------------------------------------------------------------------
 *		SccWriteReg
 * ejk		Thu, Jan 1, 1998	02:56
 *		Use this routine to write a value to the 8530 register.
 *
 *-------------------------------------------------------------------*/
bool SccWriteReg(SccChannel *Channel, UInt8 sccRegister, UInt8 Value)
{
    // Make sure we have a valid register number to write to.
    IOLockLock(Channel->SCCAccessLock);
     if (sccRegister <= kNumSCCWR ) {
        
        // First let's write the register value to the chip. 
        *((volatile UInt8 *) Channel->ControlRegister) = sccRegister;
        SynchronizeIO();
        REGISTER_DELAY();

        // Next we'll write the data value.
        *((volatile UInt8 *) Channel->ControlRegister) = Value;
        SynchronizeIO();
        REGISTER_DELAY();

        // Update the shadow register.
        Channel->lastWR[sccRegister] = Value;

// DLOG("SccWrite %x %x %x\n", Channel->ControlRegister, sccRegister, Value);
    }
        
    IOLockUnlock(Channel->SCCAccessLock);
  return TRUE;
}

/*---------------------------------------------------------------------
*		SccHandleExtErrors
 * ejk		Thu, Jan 8, 1998	18:43
 * 		Check for errors and if there are errors it does
 *              the "right thing" (nothing for now).
 *-------------------------------------------------------------------*/

void SccHandleExtErrors(SccChannel *Channel)
{
	if (Channel == NULL)
		return; //rcs 3938921 The Channel should never be null, but lets just make sure.

    UInt8 errorCode = SccReadReg(Channel, 1);
    DLOG("RecErrorStatus Int %x\n\r", errorCode);

#if 0
    if (IOGetDBDMAChannelStatus(&Channel->RxDBDMAChannel) & kdbdmaStatusDead)
   {
//rs!         panic("SCC DMA DEAD!\n");
    }
#endif

    if (errorCode & kRxErrorsMask) {
#ifdef TRACE
        KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 0x100)) | DBG_FUNC_NONE, errorCode, SccReadReg(Channel, R0), 0, 0, 0
); //0x06080400
#endif
        IOLog("************An SCC Error Occurred 0x%X\n", errorCode);
        // handles the error code
        SccWriteReg(Channel, R0, kErrorReset);
        SccWriteReg(Channel, R0, kErrorReset);
    }
}

/*---------------------------------------------------------------------
*		PPCSerialRxDMAISR & PPCSerialRxDMAISR
 * ejk		Thu, Jan 8, 1998	18:43
 * 		Handle a DMA Interrupts
 *
 *-------------------------------------------------------------------*/
void PPCSerialTxDMAISR(void *identity, void *istate, SccChannel	*Channel)
{
    AppleSCCSerial *scc = (AppleSCCSerial *) identity;
	
    Channel->Stats.ints++;

    // Request an other send, but outside the interrupt handler
    // there are no reasons to spend too much time here:
    AbsoluteTime deadline;

    clock_interval_to_deadline(1, 1, &deadline);

	if (scc->fdmaStartTransmissionThread != NULL)
	{
		OSIncrementAtomic(&scc->fTransmissionCount);
		thread_call_enter_delayed(scc->fdmaStartTransmissionThread,deadline);
	}
}


void PPCSerialRxDMAISR(void *identity, void *istate, SccChannel	*Channel)
{
    AppleSCCSerial *scc = (AppleSCCSerial *) identity;
	
    Channel->Stats.ints++;

    // This is the first received byte, so start the checkData ballet:
    AbsoluteTime deadline;

    clock_interval_to_deadline(1, 1, &deadline);
	if (scc->dmaRxHandleCurrentPositionThread != NULL)
	{
		OSIncrementAtomic(&scc->fCurrentPositionCount);	
		thread_call_enter_delayed(scc->dmaRxHandleCurrentPositionThread,deadline);
	}
}

void SccCurrentPositionDelayedHandler( thread_call_param_t arg, thread_call_param_t )
{
    AppleSCCSerial *serialPortPtr = (AppleSCCSerial *)arg;
	
	SccdbdmaRxHandleCurrentPosition(&serialPortPtr->Port);
	OSDecrementAtomic(&serialPortPtr->fCurrentPositionCount);	
}

// **********************************************************************************
//
// Called asynchronously
//
// **********************************************************************************
void SccStartTransmissionDelayedHandler ( thread_call_param_t arg, thread_call_param_t )
{
    AppleSCCSerial *serialPortPtr = (AppleSCCSerial *) arg;
    SccdbdmaStartTransmission(&serialPortPtr->Port);
	OSDecrementAtomic(&serialPortPtr->fTransmissionCount);	
}

/*---------------------------------------------------------------------
 *		PPCSerialISR
 * ejk		Wed, Jan 7, 1998	1:47 AM
 *		This is the main interrupt handler for the 85C30 serial driver.
 *		Since we are running on DMA he only case we reach this interrupt
 *              is for special case.
 *-------------------------------------------------------------------*/
void PPCSerialISR(OSObject *identity, void *istate, SccChannel *Channel)
{
    // Receive interrupts are also for incoming errors:
    SccHandleExtErrors(Channel);

    // The only reaon I am here is that I got an exteral interrupt
    SccHandleExtInterrupt(identity, istate,Channel);

    SccEnableInterrupts(Channel, kSccInterrupts, 0);

#ifdef TRACE
    KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 3)) | DBG_FUNC_END, 0, 0, 0, 0, 0);  //0x0608000E
#endif
}

bool SccHandleExtInterrupt(OSObject *identity, void *istate, SccChannel *Channel)
{
    UInt8		ExtCondition;
	UInt32		HW_FlowControl;

    if (!Channel)
        return false;

    AppleSCCSerial *scc = (AppleSCCSerial *) identity;
            
    ExtCondition = SccReadReg(Channel, R0 );

    if (ExtCondition &  kRxCharAvailable)
        DLOG("Ext-> kRxCharAvailable\n");

    if (ExtCondition &  kZeroCount)
        DLOG("Ext-> kZeroCount\n");

    if (ExtCondition &  kTxBufferEmpty)
    	DLOG("Ext-> kTxBufferEmpty\n");

    bool dcdChanged = false;
    if ( (ExtCondition & kDCDAsserted) && !Channel->DCDState) {
        Channel->DCDState = true;
        dcdChanged = true;
		// Begin For DCP
		if ((Channel->whichPort == serialPortA) && (Channel->gDCPModemFound))
		{
			if (Channel->DCPModemSupportPtr)
			{
				if (Channel->gDCPUserClientSet)
				{
//					IOLog ("AppleSCCSerial -- SccHandleExtInterrupt -- DCD is set, calling SetDCD function\n");
					Channel->DCPModemSupportPtr->callDCPModemSupportFunctions (DCPFunction_SetDCD, (UInt8) kOn, NULL, NULL);
				}
			}
		}
		// End For DCP
    }
    else if ( !(ExtCondition & kDCDAsserted) && Channel->DCDState)
    {
        Channel->DCDState = false;
        dcdChanged = true;
		// Begin For DCP
		if ((Channel->whichPort == serialPortA) && (Channel->gDCPModemFound))
		{
			if (Channel->DCPModemSupportPtr)
			{
				if (Channel->gDCPUserClientSet)
				{
//					IOLog ("AppleSCCSerial -- SccHandleExtInterrupt -- DCD is not set, calling SetDCD function\n");
					Channel->DCPModemSupportPtr->callDCPModemSupportFunctions (DCPFunction_SetDCD, (UInt8) kOff, NULL, NULL);
				}
			}
		}
		// End For DCP
    }

    if (dcdChanged) {
        //IOLog("SccHandleExtInterrupt- DCD Changed\n");
        OSIncrementAtomic(&scc->fCarrierHackCount);
        if ( thread_call_enter1(scc->fPollingThread, (thread_call_param_t) Channel->DCDState) )
            OSDecrementAtomic(&scc->fCarrierHackCount);

        //pay attention to DCD changes so clients get notified when carrier changes    
        if (Channel->DCDState)
            AppleSCCSerial::changeState(Channel, PD_RS232_S_CAR, PD_RS232_S_CAR);
        else 
            AppleSCCSerial::changeState(Channel, 0, PD_RS232_S_CAR);        
    }

    if (ExtCondition &  kSyncHunt)
    	DLOG("Ext-> kSyncHunt\n");

    //hsjb & rcs 08/21/01-	MIDI and other devices clock the SCC by sending a stream
    // 						of CTS Transitions. When we notice this clocking occuring, we need
    //						to turn off CTS Interrupt Enabling. Otherwise, the  system will be
    //						brought to a crawl because of the constant stream of transitions.
    //						We consider 77 transitions in a 10 ms period to be sufficient to 
    //						trigger the disabling. (This is what OS 9 did)
    bool currentCTSState = ((ExtCondition &  kCTSAsserted) == kCTSAsserted);
    if (Channel->lastCTSState != currentCTSState)
    {
		Channel->lastCTSState = currentCTSState;
        
		// Detect when CTS is used as a clock input and disable status
		// interrupts for CTS transitions in this case.
		if (Channel->ctsTransitionCount > 76)	//magical number ≈ 128 transitions per 1/60 second
        {
			// Disable SCC status interrupts for CTS transitions.
			SccSetCTSFlowControlEnable(Channel, false);
		}

		AbsoluteTime	currentTime;
		UInt64	 		uint64_currentTime;
        UInt32 			uint32_currentTime;
        	
		clock_get_uptime (&currentTime);
        absolutetime_to_nanoseconds (currentTime, &uint64_currentTime);
        uint32_currentTime = uint64_currentTime/1000000;	//now it's in ms.

		if (uint32_currentTime >= Channel->lastCTSTime + 10)	//every 10 ms, we should watch for 76 transitions
        {
			Channel->lastCTSTime = uint32_currentTime;
			Channel->ctsTransitionCount = 0;
		}

		++Channel->ctsTransitionCount;
	}

    if (ExtCondition &  kCTSAsserted) {
        DLOG("Ext-> kCTSAsserted\n");
		
		HW_FlowControl = Channel->FlowControl & PD_RS232_S_CTS;
		
		if (HW_FlowControl && (Channel->FlowControlState != PAUSE_SEND))
		{
			SerialDBDMAStatusInfo *dmaInfo = &Channel->TxDBDMAChannel;
	
	//        DLOG("Ext-> kCTSAsserted -- Pause DBDMA\n");
			Channel->FlowControlState = PAUSE_SEND;
			IODBDMAPause(dmaInfo->dmaBase);	/* Pause transfer */
		}
    }
    else {
		HW_FlowControl = Channel->FlowControl & PD_RS232_S_CTS;
		if (HW_FlowControl && (Channel->FlowControlState == PAUSE_SEND))
		{
			SerialDBDMAStatusInfo *dmaInfo = &Channel->TxDBDMAChannel;

	//        DLOG("Ext-> kCTSAsserted -- Continue DBDMA\n");
			Channel->FlowControlState = CONTINUE_SEND;
			IODBDMAContinue(dmaInfo->dmaBase);	/* Continue transfer */
			
			//hsjb 8/22/01 This code is important for the case when we have been paused
			//and finished the last Tx Transmission and in a high water situation
			//where we won't accept any more data. This kicks off another transfer
			if (!Channel->AreTransmitting && UsedSpaceinQueue(&(Channel->TX)))
			{
				//we need to re-kick off things
				AbsoluteTime deadline;
				clock_interval_to_deadline(1, 1, &deadline);
//rcs				thread_call_func_delayed((thread_call_func_t) SccdbdmaStartTransmission, Channel, deadline);
				thread_call_enter_delayed(scc->fdmaStartTransmissionThread, deadline);
				
			}
		}
    }

    if (ExtCondition &  kTXUnderRun) {
        AppleSCCSerial::changeState(Channel, 0, PD_S_TX_BUSY);
	//	Channel->State &= ~PD_S_TX_BUSY;
    //SccWriteReg(Channel, R0, kTxUnderrunReset ); //hsjb 8/21/01 should we?
  	DLOG("Ext-> kTXUnderRun\n");
    }

    if (ExtCondition &  kBreakReceived)
        DLOG("Ext-> kBreakReceived\n");

    // Allow more External / status interrupts
    SccWriteReg(Channel, R0, kResetExtStsInt );

    return noErr;
}

/*---------------------------------------------------------------------
 *		SetUpTransmit
 * ejk		Mon, Jan 12, 1998	11:26 PM
 * 		When we have placed some data in the buffer, let the TX know
 *	we are ready to send it.Rx
 *
 *-------------------------------------------------------------------*/
bool	SetUpTransmit(SccChannel *Channel)
{
    DLOG("++> SetUpTransmit\n");
    //  If we are already in the cycle of transmitting characters,
    //  then we do not need to do anything.
    if (Channel->AreTransmitting == TRUE) {
        DLOG("--> SetUpTransmit Already Set\n");
        return FALSE;
    }

    // To start the ball rolling let's place first block in the transmit
    // buffer, after it's done with the one the ISR will take over.
    if (GetQueueStatus(&(Channel->TX)) != queueEmpty) {
        SccdbdmaStartTransmission(Channel);
        DLOG("Write First block.\n\r");
    }
 
    return TRUE;
}

/*---------------------------------------------------------------------
 *		SuspendTX
 * ejk		Tue, Jan 13, 1998	23:25
 * 		Stop the trasnmitter in case of flow control.
 *
 *-------------------------------------------------------------------*/

InterruptState SccDisableInterrupts(SccChannel *Channel, UInt32 WhichInts)
{
    InterruptState	previousState = 0;
    UInt8		sccState;

    switch ( WhichInts) {
        case kTxInterrupts:	// turn off tx interrupts
            sccState = Channel->lastWR[ 5 ];
            previousState	= (InterruptState)sccState;
            SccWriteReg(Channel, R5, sccState & ~kTxEnable );
            break;
        case kRxInterrupts:	// turn off rx interrupts
            sccState		= Channel->lastWR[ 3 ];
            previousState	= (InterruptState)sccState;
            SccWriteReg(Channel, R3, sccState & ~kRxEnable );
            break;
        case kSccInterrupts:	// turn off the scc interrupt processing
            sccState		= Channel->lastWR[ 9 ];
            previousState	= (InterruptState)sccState;
            SccWriteReg(Channel, R9, sccState & ~kMIE & ~kNV );
            break;
        default:
            break;
    }
    return previousState;
}

void SccEnableInterrupts(SccChannel *Channel, UInt32 WhichInts, InterruptState previousState)
{
//UInt8				sccState			= (UInt8)previousState;

    switch ( WhichInts) {
        case kTxInterrupts:	// turn on tx interrupts (regardless of previousState)
            SccWriteReg(Channel, R5, Channel->lastWR[ 5 ] | kTxEnable );
            break;
        case kRxInterrupts:	// turn on rx interrupts (regardless of previousState)
            SccWriteReg(Channel, R3, Channel->lastWR[ 3 ] | kRxEnable );
            SccWriteReg(Channel, R0, kResetRxInt );
            break;
        case kSccInterrupts:	// turn on rx interrupts (regardless of previousState)
            SccWriteReg(Channel, R9, Channel->lastWR[ 9 ] | kMIE | kNV );
            break;
        default:
            break;
    }
}

/*---------------------------------------------------------------------
 *		SccSetDTR
 * ejk		Mon, Mar 23, 1998	9:30 PM
 * 		Set the DTR Line either High or Low.
 *
 *-------------------------------------------------------------------*/
void SccSetDTR( SccChannel *Channel, bool assertDTR )
{

    if ( assertDTR )
        SccWriteReg(Channel, R5, Channel->lastWR[ 5 ] | kDTR );
    else
        SccWriteReg(Channel, R5, Channel->lastWR[ 5 ] & ~kDTR );
}

/*---------------------------------------------------------------------
 *		SccSetRTS
 * ejk		Mon, Mar 23, 1998	9:52 PM
 * 		Set the RTS line.
 *
 *-------------------------------------------------------------------*/
void SccSetRTS( SccChannel *Channel, bool assertRTS )
{

    if ( assertRTS )
        SccWriteReg(Channel, R5, Channel->lastWR[ 5 ] | kRTS );
    else
        SccWriteReg(Channel, R5, Channel->lastWR[ 5 ] & ~kRTS );
}

/*---------------------------------------------------------------------
 *		SccGetDCD
 * ejk		Thu, Mar 26, 1998	12:41 AM
 * 		Comment Me
 *
 *-------------------------------------------------------------------*/
bool SccGetDCD( SccChannel *Channel )
{
    bool	Value;

    Value = (SccReadReg(Channel, R0 ) &  kDCDAsserted);
    if (Value) {
        Channel->State |= PD_RS232_S_CAR;
    }
    else {
        Channel->State &= ~PD_RS232_S_CAR;
    }
        

    return Value;
}

/*---------------------------------------------------------------------
 *		SccGetCTS
 * ejk		Thu, Mar 26, 1998	12:41 AM
 * 		Comment Me
 *
 *-------------------------------------------------------------------*/
bool SccGetCTS( SccChannel *Channel )
{
    bool	Value;

    Value = (SccReadReg(Channel, R0 ) &  kCTSAsserted);
    if (Value)
        Channel->State |= PD_RS232_S_CTS;
    else
        Channel->State &= ~PD_RS232_S_CTS;

    return Value;
}

/*---------------------------------------------------------------------
*		SccSetDMARegisters
*-------------------------------------------------------------------*/
#include <IOKit/IODeviceTreeSupport.h>

void SccSetDMARegisters(SccChannel *Channel, IOService *provider)
{
    UInt32 firstDMAMap = 1;
    IOMemoryMap *map;

    Channel->TxDBDMAChannel.dmaChannelAddress = NULL;
    Channel->TxDBDMAChannel.dmaBase = NULL;
    Channel->RxDBDMAChannel.dmaChannelAddress = NULL;
    Channel->RxDBDMAChannel.dmaBase = NULL;
    
    for(firstDMAMap = 1;; firstDMAMap++) {
        if ( !(map = provider->mapDeviceMemoryWithIndex(firstDMAMap)) )
            return;

        if (map->getLength() > 1)
            break;
    }

    Channel->TxDBDMAChannel.dmaChannelAddress = (IODBDMAChannelRegisters*)map->getVirtualAddress();
//    Channel->TxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getPhysicalAddress();
    Channel->TxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getVirtualAddress();

    if ( !(map = provider->mapDeviceMemoryWithIndex( firstDMAMap + 1 )) ) return;
    Channel->RxDBDMAChannel.dmaChannelAddress = (IODBDMAChannelRegisters*)map->getVirtualAddress();
//    Channel->RxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getPhysicalAddress();
    Channel->RxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getVirtualAddress();
}

/*---------------------------------------------------------------------
*		SccEnableDMAInterruptSources
*-------------------------------------------------------------------*/
void SccEnableDMAInterruptSources(SccChannel *Channel, bool onOff)
{
    /* We wish to remove the tx interrupt (since dma cares of it), and
     * add all the dma interrupts.
     */
    UInt8 dmaRemoveInterrupt = kRxIntAllOrSC | kTxIntEnable;
    UInt8 dmaInterruptMask = kDMAReqSelect | kDMAReqOnRx | kDMAReqOnTx | kWReqEnable | kExtIntEnable | kRxIntOnlySC;
    UInt8 newRegisterValue;

    if (onOff) {
        newRegisterValue =  Channel->lastWR[1] & (~dmaRemoveInterrupt) | dmaInterruptMask;
    }
    else{
        newRegisterValue =  Channel->lastWR[1] & (~dmaInterruptMask) | dmaRemoveInterrupt;
    }

    SccWriteReg(Channel, R1, newRegisterValue);

    SccEnableInterrupts( Channel, kRxInterrupts, 0 );

    // reminds the receiver to call when a new character
    // enters in the fifo:
    SccWriteReg(Channel, R0, kResetRxInt);

    // And to wake me up when one of the status bits changes
    SccWriteReg(Channel, R0, kResetExtStsInt );
}

/*---------------------------------------------------------------------
*		SccSetupReceptionChannel
*-------------------------------------------------------------------*/
void SccSetupReceptionChannel(SccChannel *Channel)
{
    SerialDBDMAStatusInfo *dmaInfo = &Channel->RxDBDMAChannel;

    /* Just in case */
    IODBDMAReset(dmaInfo->dmaBase);
    dmaInfo->lastPosition = 0;             /* last position of the dma */
     /* Just in case */
    IODBDMAReset(dmaInfo->dmaBase);
}

/*---------------------------------------------------------------------
*		SccFreeReceptionChannel
*-------------------------------------------------------------------*/
void SccFreeReceptionChannel(SccChannel *Channel)
{
	if (Channel == NULL)
		return;
	IOLockLock (Channel->IODBDMARxLock);
    SccdbdmaEndReception(Channel);

    SerialDBDMAStatusInfo *dmaInfo = &Channel->RxDBDMAChannel;

    if (dmaInfo->dmaChannelCommandAreaMDP != NULL)
    {
        dmaInfo->dmaChannelCommandAreaMDP->complete();
        dmaInfo->dmaChannelCommandAreaMDP->release();
        dmaInfo->dmaChannelCommandAreaMDP = NULL;
    }

//    if (dmaInfo->dmaChannelCommandArea != NULL)
//        IOFreeAligned(dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors);

    if (dmaInfo->dmaTransferBufferMDP != NULL)
    {
        dmaInfo->dmaTransferBufferMDP->complete();
        dmaInfo->dmaTransferBufferMDP->release();
        dmaInfo->dmaTransferBufferMDP = NULL;
    }

//    if (dmaInfo->dmaTransferBuffer != NULL)
//        IOFreeAligned(dmaInfo->dmaTransferBuffer, PAGE_SIZE);
        
    dmaInfo->lastPosition = 0;
    dmaInfo->dmaNumberOfDescriptors = 0;
    dmaInfo->dmaChannelCommandArea = NULL;
	IOLockUnlock(Channel->IODBDMARxLock);
}

/*---------------------------------------------------------------------
*		SccdbdmaDefineReceptionCommands
*-------------------------------------------------------------------*/
void SccdbdmaDefineReceptionCommands(SccChannel *Channel)
{
    SerialDBDMAStatusInfo *dmaInfo = &Channel->RxDBDMAChannel;
    UInt32 iterator;
    IOPhysicalAddress physaddr;
    IOPhysicalAddress physaddr1;
    IOByteCount	temp;

    // check the "legality of the transmission:
    if ((dmaInfo->dmaChannelCommandArea == NULL) ||
        (dmaInfo->dmaTransferBuffer == NULL))
        return;
        
    if (dmaInfo->dmaTransferBufferMDP == NULL)
        return;

    for (iterator = 0; iterator < dmaInfo->dmaNumberOfDescriptors; iterator ++) {
        if (iterator == 0) {
            // The first is important becuse it has to generate an interrupt:
            physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(0, &temp);
            IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[iterator],
                                        kdbdmaInputMore,
                                        kdbdmaKeyStream0,
                                        kdbdmaIntAlways,
                                        kdbdmaBranchNever,
                                        kdbdmaWaitNever,
                                        1,
                                        physaddr);
                                        
//            IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[iterator],
//                                      kdbdmaInputMore,
//                                      kdbdmaKeyStream0,
//                                      kdbdmaIntAlways,
//                                      kdbdmaBranchNever,
//                                      kdbdmaWaitNever,
//                                      1, 			/* transfers one byte */
//                                      pmap_extract(kernel_pmap, (vm_address_t) dmaInfo->dmaTransferBuffer));
            
        }
         else if (iterator == (dmaInfo->dmaNumberOfDescriptors - 1)) {
            // The last one is special because it closes the chain, even if it is the last command it looks
            // exacly like the first, excpt that it does not generate an interrupt and that it branches to
            // the second command:
            physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(0, &temp);
            physaddr1 = dmaInfo->dmaChannelCommandAreaMDP->getPhysicalSegment(sizeof(IODBDMADescriptor), &temp);
            IOMakeDBDMADescriptorDep(&dmaInfo->dmaChannelCommandArea[iterator],
                                        kdbdmaInputMore,
                                        kdbdmaKeyStream0,
                                        kdbdmaIntNever,
                                        kdbdmaBranchAlways,
                                        kdbdmaWaitNever,
                                        1,
                                        physaddr,
                                        physaddr1);
                                        
//            IOMakeDBDMADescriptorDep(&dmaInfo->dmaChannelCommandArea[iterator],
//                                     kdbdmaInputMore,
//                                     kdbdmaKeyStream0,
//                                     kdbdmaIntNever,
//                                     kdbdmaBranchAlways,
//                                     kdbdmaWaitNever,
//                                     1, 			/* transfers one byte */
//                                     pmap_extract(kernel_pmap, (vm_address_t) dmaInfo->dmaTransferBuffer),
//                                     pmap_extract(kernel_pmap, (vm_address_t) &dmaInfo->dmaChannelCommandArea[1]));
        }
        else {
            // all the others just transfer a byte an move along the next one:
            physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(iterator, &temp);
            IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[iterator],
                                        kdbdmaInputMore,
                                        kdbdmaKeyStream0,
                                        kdbdmaIntNever,
                                        kdbdmaBranchNever,
                                        kdbdmaWaitNever,
                                        1,
                                        physaddr);
                                        
//            IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[iterator],
//                                      kdbdmaInputMore,
//                                      kdbdmaKeyStream0,
//                                      kdbdmaIntNever,
//                                      kdbdmaBranchNever,
//                                      kdbdmaWaitNever,
//                                      1, 			/* transfers one byte */
//                                      pmap_extract(kernel_pmap, (vm_address_t) dmaInfo->dmaTransferBuffer + iterator));
            
        }
    }
}

/*---------------------------------------------------------------------
*		SccdbdmaStartReception
*-------------------------------------------------------------------*/
void SccdbdmaStartReception(SccChannel *Channel)
{
    IOByteCount	temp;
    SerialDBDMAStatusInfo *dmaInfo = &Channel->RxDBDMAChannel;

    // resets the dma channel
    IODBDMAReset(dmaInfo->dmaBase);
    IODBDMAReset(dmaInfo->dmaBase);
    
    // check the "legality of the transmission:
    if ((dmaInfo->dmaChannelCommandArea == NULL) ||
        (dmaInfo->dmaTransferBuffer == NULL))
        return;

    IODBDMADescriptor *baseCommands = (IODBDMADescriptor *)dmaInfo->dmaChannelCommandAreaMDP->getPhysicalSegment(0, &temp);
//    IODBDMADescriptor *baseCommands = (IODBDMADescriptor*)pmap_extract(kernel_pmap, (vm_address_t)dmaInfo->dmaChannelCommandArea);

    flush_dcache((vm_offset_t) dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors, false );

    /* since we are at the begin of the read the start position is : */
    dmaInfo->lastPosition = 0;
       
     // Enables the receiver and starts:
    SccEnableInterrupts( Channel, kRxInterrupts, 0 );	//hsjb-should we move this to after the IODBDMAStart
    IODBDMAStart(dmaInfo->dmaBase, baseCommands);
}

/*---------------------------------------------------------------------
*		SccdbdmaHandleCurrentPosition
*-------------------------------------------------------------------*/
bool CommandExecuted(SccChannel *Channel, UInt32 iterator)
{
    SerialDBDMAStatusInfo *dmaInfo = &Channel->RxDBDMAChannel;
    UInt32 result = IOGetCCResult(&dmaInfo->dmaChannelCommandArea[iterator]);

    if (result != 0)
        return true;

    return false;
}

UInt32 CommandStatus(SccChannel *Channel, UInt32 commandNumber)
{
    SerialDBDMAStatusInfo *dmaInfo = &Channel->RxDBDMAChannel;
   return IOGetCCResult(&dmaInfo->dmaChannelCommandArea[commandNumber]);
}

void SccdbdmaRxHandleCurrentPosition(SccChannel *Channel)
{
	void *owner;
	AppleSCCSerial *scc;

	
    if (OSCompareAndSwap(1,1,&gTimerCanceled))
        return;

	if (Channel == NULL)
		return;

	owner = Channel->fAppleSCCSerialInstance;
	scc = (AppleSCCSerial *) owner;


	IOLockLock (Channel->IODBDMARxLock);
    SerialDBDMAStatusInfo *dmaInfo = &Channel->RxDBDMAChannel;
	UInt32 kDataIsValid = (kdbdmaStatusRun | kdbdmaStatusActive);


    // Sanity check:
    if ((dmaInfo->dmaChannelCommandArea == NULL) ||
        (dmaInfo->dmaTransferBuffer == NULL))
	{
		IOLockUnlock (Channel->IODBDMARxLock);
        return;
	}

    // Checks for errors in the SCC since they have the
    // nasty habit of blocking the FIFO:
    SccHandleExtErrors(Channel);

#ifdef TRACE
     /* The debug code consists of the following
        *
        * ----------------------------------------------------------------------
        *|              |               |                               |Func   |
        *| Class (8)    | SubClass (8)  |          Code (14)            |Qual(2)|
        * ----------------------------------------------------------------------
        * Class     = drivers = 0x06
        * Sub Class = serial  = 0x08
        * Code      = func ID = 1
        * FuncQulif: it is
        * DBG_FUNC_START          1
        * DBG_FUNC_END            2
        * DBG_FUNC_NONE           0
        * 0x06080005
        * how to trace:
        * trace -i enables the tracing and sets up the kernel buffer for tracing
        * trace -g shows the trace setup.
        * trace -e start tracing (but I use trace -e -c 6 -s 8 to trace only the calls in the
                                  *          serial driver.
                                  * trace -d stop the tracer.
                                  * trace -t codefile >result dumps the content of the trace buffer in the file "result"
                                  */

    KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 4)) | DBG_FUNC_START, 0, 0, 0, 0, 0); //0x06080011
#endif
    
    // DO NOT FEEL TEMPTED TO REMOVE THIS VARIABLE: it is very
    // important since it handles the case where we read as many
    // bytes as there are in the circualr buffer:
//	UInt32 newPosition = 0;
    bool gotData = SuckDataFromTheDBDMAChain(Channel);
	
    
    // If we actually added stuff to the queue
    if (gotData)
	{
        natural_t numberOfbytes;
        natural_t bitInterval;
        natural_t nsec;
    
        // Updates the last position:
//        dmaInfo->lastPosition = newPosition;
        
        // Clearly our buffer is no longer Empty.
        AppleSCCSerial::CheckQueues(Channel);

        // reschedule itself for a later time:
        // the time is a personal preference. I believe that
        // checking after on third of the buffer is full is
        // a fairly good time.
        if (Channel->DataLatInterval.tv_nsec == 0)
        {
            numberOfbytes = (dmaInfo->dmaNumberOfDescriptors - 1) / 3;
            bitInterval = (1000000 * 10)/Channel->BaudRate;        // bit interval in \xb5Sec
            nsec = bitInterval * 1000 * numberOfbytes;     // nSec*bits
        }
        else
            nsec = Channel->DataLatInterval.tv_nsec;

		IOLockUnlock (Channel->IODBDMARxLock);

#if USE_WORK_LOOPS
		rearmRxTimer(Channel, nsec);
#else
		AbsoluteTime deadline;

		clock_interval_to_deadline(nsec, 1, &deadline);
//rcs		thread_call_func_delayed((thread_call_func_t) SccdbdmaRxHandleCurrentPosition, Channel, deadline);
		thread_call_enter_delayed(scc->dmaRxHandleCurrentPositionThread, deadline);
#endif
    }
	else
	{
        // O.k. since we did not get anything let's stop the channel and restart for the
        // the byte with the interrupt, the first one.

#ifdef TRACE
        KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 5)) | DBG_FUNC_START, 0, 0, 0, 0, 0); //0x06080015
#endif        
        // Stops the reception:
		IODBDMAPause(dmaInfo->dmaBase);
		UInt8 savedR1Reg = Channel->lastWR[ 1 ];
		SccWriteReg(Channel, R1, savedR1Reg & ~kWReqEnable );
		IOSetDBDMAChannelControl( dmaInfo->dmaBase, IOClearDBDMAChannelControlBits( kdbdmaPause ) );
        IODBDMAFlush(dmaInfo->dmaBase);
		IOSetDBDMAChannelControl( dmaInfo->dmaBase, IOClearDBDMAChannelControlBits( kdbdmaRun ));	
		while( IOGetDBDMAChannelStatus( dmaInfo->dmaBase) & ( kdbdmaActive ))
		eieio();
		SccWriteReg(Channel, R1, savedR1Reg);
		
		UInt32	commandStatus = CommandStatus(Channel, dmaInfo->lastPosition);
		if ((commandStatus & kDataIsValid) == kDataIsValid)	//if the current command is valid...there's some data!
		{
			if (SuckDataFromTheDBDMAChain(Channel))
			{
				// Clearly our buffer is no longer Empty.
				AppleSCCSerial::CheckQueues(Channel);
			}
		}

		// This is stopped, so reset its status:
        IOSetCCResult(&dmaInfo->dmaChannelCommandArea[dmaInfo->lastPosition],0);
        eieio();
		
        
        SccdbdmaStartReception(Channel);
		IOLockUnlock (Channel->IODBDMARxLock);
    }
	
#ifdef TRACE
    KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 4)) | DBG_FUNC_END, 0, 0, 0, 0, 0); //0x06080012
#endif
}

bool SuckDataFromTheDBDMAChain(SccChannel *Channel)
{    // Starting from the last known position read all the
    // bytes transfer in dma. I know which bytes were
    // transfered because I know which command is the
    // last to run:
	UInt32	SW_FlowControl = Channel->FlowControl & PD_RS232_S_RXO;
	SerialDBDMAStatusInfo 	*TxdmaInfo = &Channel->TxDBDMAChannel;
	SerialDBDMAStatusInfo 	*dmaInfo = &Channel->RxDBDMAChannel;
    UInt32 iterator = dmaInfo->lastPosition;
	bool gotData = false;
	// for DCP -- begin
	bool checkByte = true;
	// for DCP -- end



    iterator = dmaInfo->lastPosition;
	
	UInt32 kDataIsValid = (kdbdmaStatusRun | kdbdmaStatusActive);  
	while ((CommandStatus(Channel, iterator) & kDataIsValid) == kDataIsValid)
	{
        UInt8 byteRead;

        // resets the count for the current command:
        IOSetCCResult(&dmaInfo->dmaChannelCommandArea[iterator],0);
        eieio();

        // This is a special ring where the last command does not point to the command 0
        // but to the command 1 and it does not write on the last byte but on byte 0
        if (iterator == (dmaInfo->dmaNumberOfDescriptors - 1)) {
            // Transfers the new byte in the buffers:
            byteRead =  dmaInfo->dmaTransferBuffer[0];

            iterator = 1;
        }
        else {
            // Transfers the new byte in the buffers:
            byteRead = dmaInfo->dmaTransferBuffer[iterator];

            iterator++;
        }

		// for DCP -- begin
		// This is the place to parse received data for finding the header and storing sound into sound buffers

		if ((Channel->whichPort == serialPortA) && (Channel->gDCPModemFound) && (Channel->gDCPUserClientSet))
		{
			bool	addByte = false;
			bool	result;
			
			if (!(Channel->DCDState))
			{
				result = Channel->DCPModemSupportPtr->callDCPModemSupportFunctions (DCPFunction_ParseData, byteRead, &checkByte, &addByte);
				if (!result)
				{
//					IOLog ("AppleSCCSerial::acquirePort -- callDCPModemSupportFunctions returns error result code\n");
					Channel->gDCPUserClientSet = false;
					checkByte = true;
				}
				else
				{
					if (addByte)
					{
//						IOLog ("AppleSCCSerial::acquirePort -- callDCPModemSupportFunctions returns true for addByte\n");
						AddBytetoQueue(&(Channel->RX), 0x7E);
					}
				}
			}
		}	// if the port is port A and DCP modem is found
	
		// for DCP -- end

#ifdef TRACE
        KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 4)) | DBG_FUNC_NONE, byteRead, byteRead, 0, 0, 0); //0x06080010
#endif

		if (checkByte)
		{
		//**************************************************/
		// Begin software flow control code
		//**************************************************/
	
		if (SW_FlowControl)
		{
			if (byteRead == Channel->XONchar)
			{
				if (Channel->RXOstate == NEEDS_XON)
				{
					Channel->RXOstate = NEEDS_XOFF;
					IODBDMAContinue(TxdmaInfo->dmaBase);	// continue transfer to the modem
					Channel->FlowControlState = CONTINUE_SEND;
				}
			}
			else if (byteRead == Channel->XOFFchar)
			{
				if (Channel->RXOstate == NEEDS_XOFF)
				{
					Channel->RXOstate = NEEDS_XON;
					IODBDMAPause(TxdmaInfo->dmaBase);		// Pause transfer to the modem
					Channel->FlowControlState = PAUSE_SEND;
				}
			}
			else	// char is not XON or XOFF, so need to put it to the queue
			{
				AddBytetoQueue(&(Channel->RX), byteRead);
			}
		}
		else	// Not software flow control
		{
			int	excess = 0;
			AddBytetoQueue(&(Channel->RX), byteRead);

			excess = UsedSpaceinQueue(&(Channel->RX)) - Channel->RXStats.HighWater;
			if (!Channel->aboveRxHighWater)
			{
				if (excess > 0)
				{
					Channel->aboveRxHighWater = true;
					AppleSCCSerial::CheckQueues(Channel);
				}
			}
			else if (excess <= 0)
			{
				Channel->aboveRxHighWater = false;
			}
		}

		//**************************************************/
		// End software flow control code
		//**************************************************/
		}
        gotData = true;
    }

        dmaInfo->lastPosition = iterator;
        
	return gotData;
}

/*---------------------------------------------------------------------
*		SccdbdmaEndReception
*-------------------------------------------------------------------*/
void SccdbdmaEndReception(SccChannel *Channel)
{
    SerialDBDMAStatusInfo *dmaInfo = &Channel->RxDBDMAChannel;
    UInt32		cmdResult = 0;
    
    IODBDMAStop(dmaInfo->dmaBase);	/* Stop transfer */
    IODBDMAReset(dmaInfo->dmaBase);	/* reset transfer */

    if (dmaInfo != NULL) {
        cmdResult = IOGetDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], result);
        dmaInfo->dmaTransferSize = dmaInfo->dmaTransferSize - (cmdResult & 0xFFFF);
        DLOG("SccdbdmaEndTansmission: dma sent %u bytes\n", (unsigned) dmaInfo->dmaTransferSize);
    }
}

/*---------------------------------------------------------------------
*		SccSetupTansmissionChannel
*-------------------------------------------------------------------*/
void SccSetupTansmissionChannel(SccChannel *Channel)
{
    SerialDBDMAStatusInfo *dmaInfo = &Channel->TxDBDMAChannel;

    /* Just in case */
    IODBDMAReset(dmaInfo->dmaBase);
    dmaInfo->lastPosition = 0;             /* last position of the dma */
    /* Just in case */
    IODBDMAReset(dmaInfo->dmaBase);
}

/*---------------------------------------------------------------------
*		SccFreeTansmissionChannel
*-------------------------------------------------------------------*/
void SccFreeTansmissionChannel(SccChannel *Channel)
{
	if (Channel == NULL)
		return;
	IOLockLock (Channel->IODBDMATrLock);
    SccdbdmaEndTransmission(Channel);

    SerialDBDMAStatusInfo *dmaInfo = &Channel->TxDBDMAChannel;

    if (dmaInfo->dmaChannelCommandAreaMDP != NULL)
    {
        dmaInfo->dmaChannelCommandAreaMDP->complete();
        dmaInfo->dmaChannelCommandAreaMDP->release();
        dmaInfo->dmaChannelCommandAreaMDP = NULL;
    }

//    if (dmaInfo->dmaChannelCommandArea != NULL)
//rcs  7/25/01 IOFreeAligned(dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors);
//        IOFreeAligned(dmaInfo->dmaChannelCommandArea, PAGE_SIZE);

    if (dmaInfo->dmaTransferBufferMDP != NULL)
    {
        dmaInfo->dmaTransferBufferMDP->complete();
        dmaInfo->dmaTransferBufferMDP->release();
        dmaInfo->dmaTransferBufferMDP = NULL;
    }

//    if (dmaInfo->dmaTransferBuffer != NULL)
//        IOFreeAligned(dmaInfo->dmaTransferBuffer, PAGE_SIZE);

    dmaInfo->dmaNumberOfDescriptors = 0;
    dmaInfo->dmaChannelCommandArea = NULL;
	IOLockUnlock (Channel->IODBDMATrLock);
}

/*---------------------------------------------------------------------
*		SccdbdmaDefineReceptionCommands
*-------------------------------------------------------------------*/
void SccdbdmaDefineTansmissionCommands(SccChannel *Channel)
{
    IOPhysicalAddress physaddr;
    IOByteCount	temp;
    SerialDBDMAStatusInfo *dmaInfo = &Channel->TxDBDMAChannel;

    physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(0, &temp);
    IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0],
                        kdbdmaOutputLast,
                        kdbdmaKeyStream0,
                        kdbdmaIntAlways,
                        kdbdmaBranchNever,
                        kdbdmaWaitNever,
                        0,
                        physaddr); 

//    IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0],
//                          kdbdmaOutputLast,
//                          kdbdmaKeyStream0,	/* Default stream	*/
//                          kdbdmaIntAlways,
//                          kdbdmaBranchNever,
//                          kdbdmaWaitNever,
//                          0,
//                          pmap_extract(kernel_pmap, (vm_address_t) dmaInfo->dmaTransferBuffer)
//                          );

    IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[1],
                          kdbdmaStop,
                          kdbdmaKeyStream0,
                          kdbdmaIntNever,
                          kdbdmaBranchNever,
                          kdbdmaWaitNever,
                          0, 0
                          );
}

/*---------------------------------------------------------------------
*		SccdbdmaStartTransmission
*-------------------------------------------------------------------*/
void SccdbdmaStartTransmission(SccChannel *Channel)
{
    IOByteCount	temp;
    
    if (Channel == NULL)
        return;
    IOLockLock (Channel->IODBDMATrLock);
	
    SerialDBDMAStatusInfo *dmaInfo = &Channel->TxDBDMAChannel;

    // Checks for errors in the SCC since they have the
    // nasty habit of blocking the FIFO:
    SccHandleExtErrors(Channel);

    // Sets up everything as we are running so I am sure to do not start this
    // channel twice if a call occures twice to this function:
    Channel->AreTransmitting = TRUE;
    AppleSCCSerial::changeState(Channel, PD_S_TX_BUSY, PD_S_TX_BUSY);
    
    // check the "legality of the transmission:
    if ((dmaInfo->dmaChannelCommandArea != NULL) &&
        (dmaInfo->dmaTransferBuffer != NULL)) {
       
        // Reset the cahnnel for the new transfer:
        IODBDMAReset(dmaInfo->dmaBase);

        IODBDMADescriptor *baseCommands = (IODBDMADescriptor *)dmaInfo->dmaChannelCommandAreaMDP->getPhysicalSegment(0, &temp);
//        IODBDMADescriptor *baseCommands = (IODBDMADescriptor*)pmap_extract(kernel_pmap, (vm_address_t)dmaInfo->dmaChannelCommandArea);

        UInt8 *localBuffer = dmaInfo->dmaTransferBuffer;

        /* Fill up the buffer with characters from the queue */
		UInt32	sizeReadFromBuffer = 0;
		u_char* bufferPtr = 0;
		UInt32 bytesToTransfer = UsedSpaceinQueue(&(Channel->TX));
 		if (bytesToTransfer)
		{
			//IOLog("Something in the Queue. Size is %d\n", bytesToTransfer);
			Boolean wrapped = false;
			sizeReadFromBuffer = bytesToTransfer;
			bufferPtr = BeginDirectReadFromQueue(&(Channel->TX), &sizeReadFromBuffer, &wrapped);
			if (bufferPtr && sizeReadFromBuffer)
			{
				//IOLog("About to copy from Queue. Size is %d\n", sizeReadFromBuffer);
				//LogData(sizeReadFromBuffer, (char*)bufferPtr);
				if (sizeReadFromBuffer > MAX_BLOCK_SIZE)
					sizeReadFromBuffer = MAX_BLOCK_SIZE;
				
				dmaInfo->dmaTransferSize = sizeReadFromBuffer;
				bcopy(bufferPtr, localBuffer, sizeReadFromBuffer);
			}
		}
		else
		{
			dmaInfo->dmaTransferSize = 0;
		}
        // If there are not bytes to send just exit:
        if ((Channel->FlowControlState != PAUSE_SEND) && (dmaInfo->dmaTransferSize > 0)) {
            // Otherwise create the next transfer:
            IOSetDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], operation,
                                 IOMakeDBDMAOperation(kdbdmaOutputLast,
                                                      kdbdmaKeyStream0,
                                                      kdbdmaIntAlways,
                                                      kdbdmaBranchNever,
                                                      kdbdmaWaitNever,
                                                      dmaInfo->dmaTransferSize));
            eieio();

            flush_dcache((vm_offset_t) dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors, false );

            // and starts the transmission:
            IODBDMAStart(dmaInfo->dmaBase, baseCommands);

			EndDirectReadFromQueue(&(Channel->TX), dmaInfo->dmaTransferSize);
            // We just removed a bunch of stuff from the
            // queue, so see if we can free some thread
            // to enqueue more stuff.
            AppleSCCSerial::CheckQueues(Channel);

            // and return
			IOLockUnlock(Channel->IODBDMATrLock);
            return;
        }
		else
		{
			EndDirectReadFromQueue(&(Channel->TX), 0);
		}

    }

    // Updates all the status flags:
    AppleSCCSerial::CheckQueues(Channel);
    Channel->AreTransmitting = FALSE;
    AppleSCCSerial::changeState(Channel, 0, PD_S_TX_BUSY);
	IOLockUnlock(Channel->IODBDMATrLock);
}

/*---------------------------------------------------------------------
*		SccdbdmaEndTransmission
*-------------------------------------------------------------------*/
void SccdbdmaEndTransmission(SccChannel *Channel)
{
    SerialDBDMAStatusInfo *dmaInfo = &Channel->TxDBDMAChannel;
    UInt32		cmdResult = 0;
    
    IODBDMAStop(dmaInfo->dmaBase);	/* Stop transfer */
    IODBDMAReset(dmaInfo->dmaBase);	/* reset transfer */

    // disables the transmitter:
    SccDisableInterrupts( Channel, kTxInterrupts);

    if (dmaInfo != NULL) {
        cmdResult = IOGetDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], result);
        dmaInfo->dmaTransferSize = dmaInfo->dmaTransferSize - (cmdResult & 0xFFFF);
        DLOG("SccdbdmaEndTansmission: dma sent %u bytes\n", (unsigned) dmaInfo->dmaTransferSize);
    }
}

void HandleRxIntTimeout(SccChannel *Channel)
{
    //Currently not used other than for debugging
}

void rxTimeoutHandler(OSObject *owner, IOTimerEventSource *sender)
{
	AppleSCCSerial*	serialPortPtr;
	
    if (OSCompareAndSwap(1,1,&gTimerCanceled))
        return;
            
	// make sure that the owner of the timer is us
	serialPortPtr = OSDynamicCast( AppleSCCSerial, owner );
	if( serialPortPtr )	// it's us
	{		
		SccdbdmaRxHandleCurrentPosition(serialPortPtr->portPtr());
	}
}

#if USE_WORK_LOOPS
void rearmRxTimer(SccChannel *Channel, UInt32 timerDelay)
{
    if (OSCompareAndSwap(1,1,&gTimerCanceled))
        return;

	Channel->rxTimer->setTimeout( timerDelay );
}
#endif
