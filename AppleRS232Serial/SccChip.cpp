/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
/*
 * SccChip.cpp
 *
 * MacOSX implementation of Serial Port driver
 *
 *
 * Humphrey Looney	MacOSX IOKit ppc
 * Elias Keshishoglou 	MacOSX Server ppc
 * Dean Reece		Original Next Intel version
 *
 * 18/04/01 David Laupmanis	Add SccConfigForMIDI function to configure the SCC
 *				for MIDI i.e. turn off the baud rate generator, set
 *				parity off, one stop bit.
  *
 *
 * 02/07/01	Paul Sun	Implemented the software and hardware flow control.
 *
 * 01/27/01	Paul Sun	Fixed bug # 2550140 & 2553750 by adding mutex locks
 *				in the following routines: SccdbdmaStartTansmission(),
 *				SccFreeReceptionChannel(), SccdbdmaRxHandleCurrentPosition(),
 *				and SccFreeTanmissionChannel().
 *
 * Copyright ©: 1999 Apple Computer, Inc.  all rights reserved.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <sys/kdebug.h>

#include "AppleRS232Serial.h"

#include <IOKit/serial/IORS232SerialStreamSync.h>

extern void flush_dcache(vm_offset_t addr, unsigned count, int phys);

#if USE_ELG
    extern com_apple_iokit_XTrace	*gXTrace;
    extern UInt32			gTraceID;
#endif

bool CommandExecuted(SccChannel *Channel, UInt32 iterator);
bool SuckDataFromTheDBDMAChain(SccChannel *Channel);
UInt32 CommandStatus(SccChannel *Channel, UInt32 commandNumber);

#if USE_WORK_LOOPS
    void rearmRxTimer(SccChannel *Channel, UInt32 timerDelay);
#endif

    // Marco:
    // the following define forces the code to ignore that this is an APPLE
    // SCC implementation and tries to read the registers as they were in the
    // original Z85C30
    
#define STRICT_85C30

    // Marco:
    // The SCC may need a short delay when accessing its registers, the
    // delay is defined here
    
#if 0
#define REGISTER_DELAY() IODelay(10)
#else
#define REGISTER_DELAY()
#endif

    // Array for resetting data bits
    
static UInt8 LocalDataBits[2][4] =
{
    { kRx5Bits, kRx6Bits, kRx7Bits, kRx8Bits},
    { kTx5Bits, kTx6Bits, kTx7Bits, kTx8Bits}
};

    // SCC hardware configuration table
        
static UInt8 SCCConfigTable[] = 
{
    R4, /*kX16ClockMode |*/ k1StopBit,		// WR  4, 16x clock, 1 stop bit, no parity (this could be 0x04 right? - ÖMB)
    R1, kDMAReqSelect | kDMAReqOnRx | kParityIsSpCond,
    R3, kRx8Bits & ~kRxEnable,			// WR  3, Rx 8 bits, disabled
    R5, /*kDTR | */ kTx8Bits | kRTS & ~kTxEnable,	// WR  5, Tx 8 bits, DTR and RTS asserted, transmitter disabled
    R9, kNV,					// WR  9, set below depending from the port:
    R10, kNRZ,					// WR 10, NRZ encoding
    R11, kRxClockBRG | kTxClockBRG,			// WR 11, baud rate generator clock to receiver, transmitter
    R12, 0x0A,					// WR 12, low byte baud rate (0x00 = 57.6K, 0x06 = 14400, 0x0A = 9600 baud) -- 3.6864 MHz, 16x clock
    R13, 0x00,					// WR 13, high byte baud rate (9.6K up)
    R14, kBRGFromRTxC,				// WR 14, RTxC clock drives the BRG
    R15, kERegEnable,				// WR 15, Enhancement register 7' enabled
    R7, kEReq,					// WR 7, DTR/REQ timing = W/REQ timing
    R14, kBRGEnable,				// WR 14, enable baud rate generator
    R3, kRx8Bits | kRxEnable,			// WR  3, Rx 8 bits, enabled
    R5, kTx8Bits | kTxEnable | kRTS, 		// WR  5, Tx 8 bits, RTS asserted, transmitter enabled
    R1, kDMAReqSelect | kDMAReqOnRx | kParityIsSpCond | kWReqEnable,
    R15, kBreakAbortIE | kCTSIE| kDCDIE | kERegEnable,	// WR 15, Enhancement register 7' enabled
    R0, kResetExtStsInt,			// WR  0, reset ext/status interrupts
    R0, kResetExtStsInt,			// WR  0, reset ext/status interrupts (again)
    R1, kDMAReqSelect | kDMAReqOnRx | kParityIsSpCond | kExtIntEnable | kRxIntOnlySC | kWReqEnable, // WR  1, receive interrupts (not external & status) enabled
    R9, kMIE | kNV				// WR  9, SCC interrupts enabled (MIE), status in low bits
};

/****************************************************************************************************/
//
//		Function:	initChip
//
//		Inputs:		port - The port
//
//		Outputs:	
//
//		Desc:		This function sets up various default values for the UART
//				parameters. It is intended to be called from the init method only.
//
/****************************************************************************************************/

void initChip(PortInfo_t *port)
{

    ELG(0, 0, "initChip");
    
    port->TX_Parity = PD_RS232_PARITY_NONE;
    port->RX_Parity = PD_RS232_PARITY_DEFAULT;
    port->DLRimage = 0x0000;
    port->FCRimage = 0x00;

    ProbeSccDevice(port);
    
}/* end initChip */

/****************************************************************************************************/
//
//		Function:	ProbeSccDevice
//
//		Inputs:		port - The port
//
//		Outputs:	 
//
//		Desc:		Set up the port and initialize some of the structures
//				to be able to use it.
//
/****************************************************************************************************/

void ProbeSccDevice(PortInfo_t *port)
{

    ELG(0, 0, "ProbeSccDevice");

    port->DataRegister = (UInt8 *)(port->ChipBaseAddress + channelDataOffsetRISC);
    port->ControlRegister = (UInt8 *)(port->ChipBaseAddress + channelControlOffsetRISC);

        // This is very bad but it will work with all of our hardware.
        // The right thing to do is to get the instance variables of the channel
        // and determine which one is channel B and set it to that value.

    port->ConfigWriteRegister = (unsigned int)(port->ChipBaseAddress & 0xffffff00);

    port->baudRateGeneratorEnable = kBRGEnable;
    port->rtxcFrequency = 3686400;

        // FIXTHIS  ejk
        
    SccWriteReg(port, R1, 0);

    ELG(port->DataRegister, port->ControlRegister, "ProbeSccDevice - Data register, Control register");

}

/****************************************************************************************************/
//
//		Function:	FixZeroBug
//
//		Inputs:		SccChannel - The port
//
//		Outputs:	 
//
//		Desc:		Work around a bug in the SCC receving channel.
//
/****************************************************************************************************/

void FixZeroBug(SccChannel *Channel)
{

    ELG(0, 0, "FixZeroBug");
    
        // HDL SCC “Stuck Zero” Workaround

        // The following sequence prevents a problem that is seen with O’Hare ASICs
        // (most versions -- also with some Heathrow and Hydra ASICs) where a zero
        // at the input to the receiver becomes “stuck” and locks up the receiver.

        // Affected machines include the following shipping machines and prototypes:

        // CODE NAME		SHIPPED AS

        // “Alchemy”		Performa 5400/6400
        // “Gazelle”		Performa 6500
        // “Spartacus”		20th Anniversary Mac
        // “Comet”		PowerBook 2400
        // “Hooper”		PowerBook 3400
        // “Tanzania”		Motorola StarMax 3000, PowerMac 4400
        // “Gossamer”		Power Macintosh G3 <----- This is supported by MacOS X so
        //                      this workaround is required.

        // “Viper”			n/a (Apple/Motorola CHRP EVT2 prototype, < Hydra 4)

        // “PowerExpress”		cancelled

        //	This problem can occur as a result of a zero bit at the receiver input
        //	coincident with any of the following events:

        // •	The SCC is initialized (hardware or software).
        // •	A framing error is detected.
        // •	The clocking option changes from synchronous or X1 asynchronous
        //	clocking to X16, X32, or X64 asynchronous clocking.
        // •	The decoding mode is changed among NRZ, NRZI, FM0, or FM1.

        // This workaround attempts to recover from the lockup condition by placing
        // the SCC in synchronous loopback mode with a fast clock before programming
        // any of the asynchronous modes.

        // The necessity of the workaround is determined at module initialization.
     
    SccWriteReg(Channel, 9, (Channel->whichPort == serialPortA ? kChannelResetA : kChannelResetB) | kNV);

    SccWriteReg(Channel, 4, kX1ClockMode | k8BitSyncMode);			//used to be ExtSyncMode
    SccWriteReg(Channel, 3, kRx8Bits & ~kRxEnable);
    SccWriteReg(Channel, 5, kTx8Bits | kRTS & ~kTxEnable);
    SccWriteReg(Channel, 9, kNV);						// no interrupt vector
    SccWriteReg(Channel, 11, kRxClockBRG | kTxClockBRG);
    SccWriteReg(Channel, 12, 0);						// BRG low-order count
    SccWriteReg(Channel, 13, 0);						// BRG high-order count
    SccWriteReg(Channel, 14, kLocalLoopback | kBRGFromPCLK);

    SccWriteReg(Channel, 14, kLocalLoopback | kBRGFromPCLK | kBRGEnable);
    SccWriteReg(Channel, 3, kRx8Bits | kRxEnable);

    SccWriteReg(Channel, 0, kResetExtStsInt);					// reset pending Ext/Sts interrupts
    SccWriteReg(Channel, 0, kResetExtStsInt);					// (and kill some time)

        // The channel should be OK now, but it is probably receiving loopback garbage
        // Switch to asynchronous mode, disable the receiver, and discard everything in the receive buffer.

    SccWriteReg(Channel, 9, kNV);

    SccWriteReg(Channel, 4, kX16ClockMode | kStopBitsMask);
    SccWriteReg(Channel, 3, kRx8Bits & ~kRxEnable);

    while (SccReadReg(Channel, R0) & kRxCharAvailable)
    {
        (void)SccReadReg(Channel, R8);						// 8 is the data register
        SccWriteReg(Channel, R0, kResetExtStsInt ); 				// and reset possible errors
        SccWriteReg(Channel, R0, kErrorReset); 					//  .. all the possible errors
    }
    
}/* end FixZeroBug */

/****************************************************************************************************/
//
//		Function:	OpenScc
//
//		Inputs:		SccChannel - The port
//
//		Outputs:	 
//
//		Desc:		Open and configure the serial port.
//
/****************************************************************************************************/

void OpenScc(SccChannel *Channel)
{
    UInt32	SCCConfigTblSize = sizeof(SCCConfigTable)/sizeof(UInt8);
    UInt32	i;
    
    ELG(0, 0, "OpenScc");
       
        // Fix the case of a "0" stuck in the receiver
        
    FixZeroBug(Channel);

        // Now, initialize the chip
        
    SccWriteReg(Channel, 9, (Channel->whichPort == serialPortA ? kChannelResetA : kChannelResetB) | kNV);
        
        // Write configuration table info to the SCC
    
    for (i = 0; i < SCCConfigTblSize; i += 2)
    {
        SccWriteReg(Channel, SCCConfigTable[i], SCCConfigTable[i + 1]);
    }

        // Eanbles the chip interrupts
        
    SccEnableInterrupts(Channel, kSccInterrupts, 0);

        // Free the fifo in case of leftover errors
        
    SccWriteReg(Channel, R0, kErrorReset);
    SccWriteReg(Channel, R0, kErrorReset);

    SccGetDCD(Channel);		// jdg: let's init DCD status in State and DCDState from the hardware

}/* end OpenSCC */

/****************************************************************************************************/
//
//		Function:	SccCloseChannel
//
//		Inputs:		Channel - The port
//
//		Outputs:	 
//
//		Desc:		Close the serial port.
//
/****************************************************************************************************/

void SccCloseChannel(SccChannel *Channel)
{
    UInt8	dontCare;
    
    ELG(0, 0, "SccCloseChannel");

        // This is to be sure that SccHandleMissedTxInterrupt ends
        
    if (Channel->AreTransmitting == true)
    {
        Channel->AreTransmitting = false;
        IOSleep(1000);
    }
    
    dontCare = SccDisableInterrupts(Channel, kSccInterrupts);		// Disable scc interrupts before doing anything
    dontCare = SccDisableInterrupts(Channel, kRxInterrupts);		// Disable the receiver
    dontCare = SccDisableInterrupts(Channel, kTxInterrupts);		// Disable the transmitter

        // Disable the Wait/Request function and interrupts
        
    SccWriteReg(Channel, R1, kWReqDisable);

        // Set RTxC clocking and disable the baud rate generator
        
    SccWriteReg(Channel, R11, kRxClockRTxC | kTxClockRTxC);
    SccWriteReg(Channel, R14, kBRGDisable);

    SccWriteReg(Channel, R15, kDCDIE);					// enable DCD interrupts (?!)
    SccWriteReg(Channel, R0, kResetExtStsInt);
    SccWriteReg(Channel, R0, kResetExtStsInt);				// reset pending Ext/Sts interrupts
    SccWriteReg(Channel, R1, kExtIntEnable);				// external/status interrupts enabled

    if (Channel->whichPort == serialPortA)
    {
        SccWriteReg(Channel, R9, kChannelResetA);
    } else {
        SccWriteReg(Channel, R9, kChannelResetB);
    }

        // If we are running on DMA turns off the dma interrupts
    
    SccEnableDMAInterruptSources(Channel, false);

}/* end SccCloseChannel */

/****************************************************************************************************/
//
//		Function:	SccSetStopBits
//
//		Inputs:		Channel - The port
//				numbits - Number of stop bits
//
//		Outputs:	 
//
//		Desc:		Set the number of stop bits for the current port.
//
/****************************************************************************************************/

bool SccSetStopBits(SccChannel *Channel, UInt32 numbits)
{
    UInt8	value;

    ELG(0, numbits, "SccSetStopBits");

    switch(numbits)
    {
        case 0:
            value = 0;
            break;
            
        case 2:
            value = k1StopBit;
            break;
            
        case 3:
            value = k1pt5StopBits;
            break;
            
        case 4:
            value = k2StopBits;
            break;
            
        default:
            return false;
            break;
    }

    SccWriteReg(Channel, R4, (Channel->lastWR[4] & ~kStopBitsMask) | value);
    
    return true;
    
}/* end SccSetStopBits */

/****************************************************************************************************/
//
//		Function:	SccSetParity
//
//		Inputs:		Channel - The port
//				ParitySetting - As the name suggests
//
//		Outputs:	 
//
//		Desc:		Set the parity of the current port.
//
/****************************************************************************************************/

bool SccSetParity(SccChannel *Channel, ParityType ParitySetting)
{

    ELG(0, ParitySetting, "SccSetParity");

    switch(ParitySetting)
    {
        case PD_RS232_PARITY_NONE:
            SccWriteReg(Channel, 4, Channel->lastWR[4] & ~kParityEnable);
            SccWriteReg(Channel, 1, Channel->lastWR[1] & ~kParityIsSpCond);
            break;
            
        case PD_RS232_PARITY_ODD:
            SccWriteReg(Channel, 4, Channel->lastWR[4] & ~kParityEven | kParityEnable);
            SccWriteReg(Channel, 1, Channel->lastWR[1] | kParityIsSpCond);
            break;
            
        case PD_RS232_PARITY_EVEN:
            SccWriteReg(Channel, 4, Channel->lastWR[4] | kParityEven | kParityEnable);
            SccWriteReg(Channel, 1, Channel->lastWR[1] | kParityIsSpCond);
            break;
            
        case PD_RS232_PARITY_MARK:
        
        case PD_RS232_PARITY_SPACE:
        
        default:
            return false;
    }

    SccEnableInterrupts( Channel, kRxInterrupts, 0);
    SccEnableInterrupts( Channel, kSccInterrupts, 0);

        // Reminds the receiver to call when a new character enters in the fifo
        
    SccWriteReg(Channel, R0, kResetRxInt);

        // And to wake me up when one of the status bits changes
        
    SccWriteReg(Channel, R0, kResetExtStsInt);

    return true;
    
}/* end SccSetParity */

/****************************************************************************************************/
//
//		Function:	SccSetDataBits
//
//		Inputs:		Channel - The port
//				numDataBits - Number of data bits
//
//		Outputs:	 
//
//		Desc:		Reset the number of data bits for current port.
//
/****************************************************************************************************/

bool SccSetDataBits(SccChannel *Channel, UInt32 numDataBits)
{

    ELG(0, numDataBits, "SccSetDataBits");
    
    if (numDataBits >= 5 && numDataBits <= 8)
    {
        numDataBits -= 5;	 			// Set the index

            // Set Tx Bits
            
        SccWriteReg(Channel, R5, (Channel->lastWR[5] & ~kTxBitsMask) | LocalDataBits[1][numDataBits]);

            // Set Rx Bits
            
        SccWriteReg(Channel, R3, (Channel->lastWR[3] & ~kRxBitsMask) | LocalDataBits[0][numDataBits]);

        return true;
    }
    
    return false;
    
}/* end SccSetDataBits */

/****************************************************************************************************/
//
//		Function:	SccSetCTSFlowControlEnable
//
//		Inputs:		Channel - The port
//				enableCTS - Enable/Disable
//
//		Outputs:	 
//
//		Desc:		Turns on or off CTS for current port.
//
/****************************************************************************************************/

void SccSetCTSFlowControlEnable(SccChannel *Channel, bool enableCTS )
{

    ELG(0, enableCTS, "SccSetCTSFlowControlEnable");
    
    if (enableCTS)
    {
        SccWriteReg(Channel, R15, Channel->lastWR[15] | kCTSIE);
    } else {
        SccWriteReg(Channel, R15, Channel->lastWR[15] & ~kCTSIE);
    }
    
}/* end SccSetCTSFlowControlEnable */

/****************************************************************************************************/
//
//		Function:	SccChannelReset
//
//		Inputs:		Channel - The port
//
//		Outputs:	 
//
//		Desc:		Resets the current port.
//
/****************************************************************************************************/

void SccChannelReset(SccChannel *Channel)
{

    ELG(0, 0, "SccChannelReset");
    
    switch(Channel->whichPort)
    {
        case serialPortA:
            SccWriteReg(Channel, R9, kChannelResetA | kNV);
            break;
            
        case serialPortB:
            SccWriteReg(Channel, R9, kChannelResetB | kNV);
            break;
            
        default:
            break;
    }
    
}/* end SccChannelReset */

/****************************************************************************************************/
//
//		Function:	SccSetBaud
//
//		Inputs:		Channel - The port
//				NewBaud - Requetsed baud
//
//		Outputs:	Return code - true(it's set) 
//
//		Desc:		Sets the baud rate for the current port.
//
/****************************************************************************************************/

bool SccSetBaud(SccChannel *Channel, UInt32 NewBaud)
{
    UInt32	brgConstant;
    UInt8	previousState;
    UInt8	wr4Mirror;
    
    ELG(0, NewBaud, "SccSetBaud");

        // Disables the interrupts
        
    previousState = SccDisableInterrupts(Channel, kSerialInterrupts);

        // Tricky thing this, when we wish to go up in speed we also need to switch the
        // clock generator for the via

    wr4Mirror = Channel->lastWR[4] & (~kClockModeMask);
    if (NewBaud == 115200)
    {
        SccWriteReg(Channel, 4, wr4Mirror | kX32ClockMode);
    } else {
        SccWriteReg(Channel, 4, wr4Mirror | kX16ClockMode);
    }

        // Calculate the closest SCC baud rate constant (brgConstant) and actual rate
        // This is what we should have as default value
        
    brgConstant = 0;

    if ((NewBaud < 115200) && (NewBaud > 0))
    {
    
            // The fundamental expression is (rtxcFrequency / (2 * clockMode * baudRate) - 2).
            // It is necessary, however, to round the quotient to the nearest integer.
            
        brgConstant = -2 + (1 + Channel->rtxcFrequency / (16 * NewBaud)) / 2;

            // Pin 0x0000 ≤ brgConstant ≤ 0xFFFF.

        if (brgConstant < 0 )
        {
            brgConstant = 0;
        } else {
            if (brgConstant > 0xFFFF)
            {
                brgConstant = 0xFFFF;
            }
        }
    }

        // Again, round correctly when calculating the actual baud rate.

    Channel->baudRateGeneratorLo = (UInt8)brgConstant;				// just the low byte
    Channel->baudRateGeneratorHi = (UInt8)(brgConstant >> 8);			// just the high byte

    SccWriteReg(Channel, R14, Channel->lastWR[ 14 ] & (~kBRGEnable));
    SccWriteReg(Channel, R12, Channel->baudRateGeneratorLo);
    SccWriteReg(Channel, R13, Channel->baudRateGeneratorHi);

    if ((NewBaud == 115200) || (NewBaud == 230400))
    {
        SccWriteReg(Channel, R11, (kRxClockRTxC | kTxClockRTxC));
    } else {
        SccWriteReg(Channel, R11, (kRxClockBRG | kTxClockBRG));
        SccWriteReg(Channel, R14, (kBRGEnable));
    }

        // And at the end re-enables the interrupts
        
    SccEnableInterrupts(Channel, kSerialInterrupts, previousState);

        // Set the global parameter
        
    Channel->BaudRate = NewBaud;

    return true;
    
}/* end SccSetBaud */

/****************************************************************************************************/
//
//		Function:	SccConfigForMIDI
//
//		Inputs:		Channel - The port
//				ClockMode - Requetsed mode
//
//		Outputs:	Return code - true(it's set) 
//
//		Desc:		This routine will configure the SCC Port for MIDI externally
//				clocked devices. Turn off the buad rate generator, and set the 
//				clock mode to one of 4 speeds.  MIDI's default rate = 31250.
//				Clock modes are either 1, 16, 32, or 64 times the 31250 rate.
//
/****************************************************************************************************/

bool SccConfigureForMIDI(SccChannel *Channel, UInt32 ClockMode)
{
    UInt8	previousState;

    ELG(0, ClockMode, "SccConfigureForMIDI");

    Channel->baudRateGeneratorLo = 0x00;
    Channel->baudRateGeneratorHi = 0x00;
 	
        // Disable interrupts
        
    previousState = SccDisableInterrupts(Channel, kSerialInterrupts);

        //  Set clock mode
        
    SccWriteReg(Channel, R4, (Channel->lastWR[4] & ~kClockModeMask | ClockMode));

        // Transmit/Receive clock = TRxC pin
         
    SccWriteReg(Channel, R11, (kRxClockTRxC | kTxClockTRxC));

        // Disable Baud Rate Generator
        
    SccWriteReg(Channel, R14, Channel->lastWR[14] & (~kBRGEnable));
    SccWriteReg(Channel, R12, Channel->baudRateGeneratorLo);
    SccWriteReg(Channel, R13, Channel->baudRateGeneratorHi);
	    
        // Enable external interupts
        
    SccWriteReg(Channel, R1, Channel->lastWR[1] & ~kExtIntEnable);					
	    
        // Disable CTS handshaking
        
    SccWriteReg(Channel, R15, Channel->lastWR[15] & ~kCTSIE);					
    
        // Re-enable interrupts
         
    SccEnableInterrupts(Channel, kSerialInterrupts, previousState);
 
    return true;
     
}/* end SccConfigureForMIDI */

/****************************************************************************************************/
//
//		Function:	SccReadReg
//
//		Inputs:		Channel - The port
//				sccRegister - The register
//
//		Outputs:	Return value - The data 
//
//		Desc:		Read a register from the 8530.
//
/****************************************************************************************************/

UInt8 SccReadReg(SccChannel *Channel, UInt8 sccRegister)
{
    UInt8	ReturnValue;
    
//    ELG(0, sccRegister, "SccReadReg");

    IOLockLock(Channel->SCCAccessLock);
    
        // Make sure we have a valid register number to write to
        
    if (sccRegister != R0 )
    {

            // First write the register value to the chip
            
        *((volatile UInt8 *)Channel->ControlRegister) = sccRegister;
        SynchronizeIO();
        REGISTER_DELAY();
    }

        // Next get the data value
        
    ReturnValue = *((volatile UInt8 *)Channel->ControlRegister);
    
    IOLockUnlock(Channel->SCCAccessLock);

//    ELG(0, ReturnValue, "SccReadReg - Return value");

    return ReturnValue;
    
}/* end SccReadReg */

/****************************************************************************************************/
//
//		Function:	SccWriteReg
//
//		Inputs:		Channel - The port
//				sccRegister - The register
//				Value - Data to be written
//
//		Outputs:	Return code - true(wrote it) 
//
//		Desc:		Write a value to the 8530 register.
//
/****************************************************************************************************/

bool SccWriteReg(SccChannel *Channel, UInt8 sccRegister, UInt8 Value)
{

//    ELG(0, Value, "SccWriteReg");

    IOLockLock(Channel->SCCAccessLock);

        // Make sure we have a valid register number to write to.

     if (sccRegister <= kNumSCCWR )
     {
        
            // First write the register value to the chip
             
        *((volatile UInt8 *)Channel->ControlRegister) = sccRegister;
        SynchronizeIO();
        REGISTER_DELAY();

            // Next write the data value
            
        *((volatile UInt8 *)Channel->ControlRegister) = Value;
        SynchronizeIO();
        REGISTER_DELAY();

            // Update the shadow register
            
        Channel->lastWR[sccRegister] = Value;
    }
        
    IOLockUnlock(Channel->SCCAccessLock);
    
  return true;
  
}/* end SccWriteReg */

/****************************************************************************************************/
//
//		Function:	SccHandleExtErrors
//
//		Inputs:		Channel - The port
//
//		Outputs:	 
//
//		Desc:		Check for errors and if there are any it does
//				the "right thing" (nothing for now - reset).
//
/****************************************************************************************************/

void SccHandleExtErrors(SccChannel *Channel)
{
    UInt8 errorCode;
    
    ELG(0, 0, "SccHandleExtErrors");
    
    errorCode = SccReadReg(Channel, 1);
    
    ELG(0, errorCode, "SccHandleExtErrors");

    if (errorCode & kRxErrorsMask) 
    {
        ALERT(0, errorCode, "SccHandleExtErrors - An SCC Error Occurred ***");

            // Handles the error
        
        SccWriteReg(Channel, R0, kErrorReset);
        SccWriteReg(Channel, R0, kErrorReset);
    }
    
}/* end SccHandleExtErrors */

/****************************************************************************************************/
//
//		Function:	PPCSerialTxDMAISR
//
//		Inputs:		identity - unused
//				istate - unused
//				Channel - The port
//
//		Outputs:	 
//
//		Desc:		Handle the TX DMA interrupt.
//
/****************************************************************************************************/

void PPCSerialTxDMAISR(void *identity, void *istate, SccChannel	*Channel)
{
    AbsoluteTime	deadline;

    ELG(identity, Channel, "PPCSerialTxDMAISR");
    
    Channel->Stats.ints++;

        // Request another send, but outside the interrupt handler
        // there is no reason to spend too much time here
    
    clock_interval_to_deadline(1, 1, &deadline);
    thread_call_func_delayed((thread_call_func_t)SccdbdmaStartTransmission, Channel, deadline);
    
}/* end PPCSerialTxDMAISR */

/****************************************************************************************************/
//
//		Function:	PPCSerialRxDMAISR
//
//		Inputs:		identity - unused
//				istate - unused
//				Channel - The port
//
//		Outputs:	 
//
//		Desc:		Handle the RX DMA interrupt.
//
/****************************************************************************************************/

void PPCSerialRxDMAISR(void *identity, void *istate, SccChannel	*Channel)
{
    AbsoluteTime	deadline;
    
    ELG(identity, Channel, "PPCSerialRxDMAISR");

    Channel->Stats.ints++;

        // This is the first received byte, so start the checkData ballet

    clock_interval_to_deadline(1, 1, &deadline);
    thread_call_func_delayed((thread_call_func_t) SccdbdmaRxHandleCurrentPosition, Channel, deadline);
    
}/* end PPCSerialRxDMAISR */

/****************************************************************************************************/
//
//		Function:	PPCSerialISR
//
//		Inputs:		identity - Should be me (just passed on)
//				istate - ?? (just passed on)
//				Channel - The port
//
//		Outputs:	 
//
//		Desc:		Main interrupt handler for the 85C30.
//				Since we are running on DMA this is special case only.
//
/****************************************************************************************************/

void PPCSerialISR(OSObject *identity, void *istate, SccChannel *Channel)
{

    ELG(identity, Channel, "PPCSerialISR");

        // Receive interrupts are also for incoming errors
        
    SccHandleExtErrors(Channel);

        // The only reaon I am here is that I got an exteral interrupt
        
    SccHandleExtInterrupt(identity, istate, Channel);
    SccEnableInterrupts(Channel, kSccInterrupts, 0);

}/* end PPCSerialISR */

/****************************************************************************************************/
//
//		Function:	SccHandleExtInterrupt
//
//		Inputs:		identity - Should be me (just passed on)
//				istate - ?? (just passed on)
//				Channel - The port
//
//		Outputs:	
//
//		Desc:		Handles any external interrupts.
//
/****************************************************************************************************/

void SccHandleExtInterrupt(OSObject *identity, void *istate, SccChannel *Channel)
{
    AppleRS232Serial		*RS232;
    UInt8			ExtCondition;
    UInt32			HW_FlowControl;
    bool 			dcdChanged = false;
    bool 			currentCTSState;
    AbsoluteTime		currentTime;
    UInt64	 		uint64_currentTime;
    UInt32 			uint32_currentTime;
    SerialDBDMAStatusInfo	*dmaInfo;
    
    ELG(0, 0, "SccHandleExtInterrupt");

    if (!Channel)
        return;

    RS232 = (AppleRS232Serial *)identity;
            
    ExtCondition = SccReadReg(Channel, R0);

    if (ExtCondition &  kRxCharAvailable)
        ELG(0, ExtCondition, "SccHandleExtInterrupt - kRxCharAvailable");

    if (ExtCondition &  kZeroCount)
        ELG(0, ExtCondition, "SccHandleExtInterrupt - kZeroCount");

    if (ExtCondition &  kTxBufferEmpty)
        ELG(0, ExtCondition, "SccHandleExtInterrupt - kTxBufferEmpty");
        
    if (ExtCondition & kDCDAsserted)
        ELG(0, ExtCondition, "SccHandleExtInterrupt - kDCDAsserted");
    
    if ((ExtCondition & kDCDAsserted) && !Channel->DCDState) 
    {
        Channel->DCDState = true;
        dcdChanged = true;
    } else {
        if (!(ExtCondition & kDCDAsserted) && Channel->DCDState)
        {
            Channel->DCDState = false;
            dcdChanged = true;
        }
    }
    if (dcdChanged) {			// jdg:	let's pay attention to DCD changes
	//IOLog("rs574 - dcd changed, now %d\n", Channel->DCDState);
	// change state and wake up anyone that's listening
	if (Channel->DCDState)
        {
            RS232->setStateGated(PD_RS232_S_CAR, PD_RS232_S_CAR);
//            AppleRS232Serial::setStateGated(PD_RS232_S_CAR, PD_RS232_S_CAR);
	} else {
            RS232->setStateGated(0, PD_RS232_S_CAR);
//            AppleRS232Serial::setStateGated(0, PD_RS232_S_CAR);
        }
    }

    if (ExtCondition &  kSyncHunt)
        ELG(0, ExtCondition, "SccHandleExtInterrupt - kSyncHunt");

        // hsjb & rcs 08/21/01 -	
        // MIDI and other devices clock the SCC by sending a stream
        // of CTS Transitions. When we notice this clocking occuring, we need
        // to turn off CTS Interrupt Enabling. Otherwise, the  system will be
        // brought to a crawl because of the constant stream of transitions.
        // We consider 77 transitions in a 10 ms period to be sufficient to 
        // trigger the disabling. (This is what OS 9 did)
        
    currentCTSState = ((ExtCondition & kCTSAsserted) == kCTSAsserted);
    if (Channel->lastCTSState != currentCTSState)
    {
        Channel->lastCTSState = currentCTSState;
        
            // Detect when CTS is used as a clock input and disable status
            // interrupts for CTS transitions in this case
            // and disable SCC status interrupts for CTS transitions
            
        if (Channel->ctsTransitionCount > 76)				// Magic number ≈ 128 transitions per 1/60 second
        {
            SccSetCTSFlowControlEnable(Channel, false);			// This is the disable
        }
        	
        clock_get_uptime (&currentTime);
        absolutetime_to_nanoseconds (currentTime, &uint64_currentTime);
        uint32_currentTime = uint64_currentTime/1000000;		//now it's in ms.

        if (uint32_currentTime >= Channel->lastCTSTime + 10)		//every 10 ms, we should watch for 76 transitions
        {
            Channel->lastCTSTime = uint32_currentTime;
            Channel->ctsTransitionCount = 0;
        }
        ++Channel->ctsTransitionCount;
    }

    if (ExtCondition &  kCTSAsserted)
        ELG(0, ExtCondition, "SccHandleExtInterrupt - kCTSAsserted");
		
    HW_FlowControl = Channel->FlowControl & PD_RS232_S_CTS;
		
    if (HW_FlowControl && (Channel->FlowControlState != PAUSE_SEND))
    {
        dmaInfo = &Channel->TxDBDMAChannel;
        Channel->FlowControlState = PAUSE_SEND;
        IODBDMAPause(dmaInfo->dmaBase);					// Pause transfer
    } else {
        HW_FlowControl = Channel->FlowControl & PD_RS232_S_CTS;
        if (HW_FlowControl && (Channel->FlowControlState == PAUSE_SEND))
        {
            dmaInfo = &Channel->TxDBDMAChannel;
            Channel->FlowControlState = CONTINUE_SEND;
            IODBDMAContinue(dmaInfo->dmaBase);				// Continue transfer
			
                // This code is important for the case when we have been paused
                // and finished the last Tx Transmission and in a high water situation
                // where we won't accept any more data. This kicks off another transfer
                
            if (!Channel->AreTransmitting && UsedSpaceinQueue(&(Channel->TX)))
            {
                AbsoluteTime deadline;
                clock_interval_to_deadline(1, 1, &deadline);
                thread_call_func_delayed((thread_call_func_t) SccdbdmaStartTransmission, Channel, deadline);
            }
        }
    }

    if (ExtCondition &  kTXUnderRun)
    {
        ELG(0, ExtCondition, "SccHandleExtInterrupt - kTXUnderRun");
        RS232->setStateGated(0, PD_S_TX_BUSY);
//        AppleRS232Serial::setStateGated(0, PD_S_TX_BUSY);
    }

    if (ExtCondition &  kBreakReceived)
        ELG(0, ExtCondition, "SccHandleExtInterrupt - kBreakReceived");

        // Allow more External/status interrupts
        
    SccWriteReg(Channel, R0, kResetExtStsInt);
    
}/* end SccHandleExtInterrupt */

/****************************************************************************************************/
//
//		Function:	SetUpTransmit
//
//		Inputs:		Channel - The port
//
//		Outputs:	Return code - true(started), false(already started)
//
//		Desc:		Set up the transmit DMA engine.
//
/****************************************************************************************************/

bool SetUpTransmit(SccChannel *Channel)
{

    ELG(0, 0, "SetUpTransmit");

        //  If we are already in the cycle of transmitting characters,
        //  then we do not need to do anything
        
    if (Channel->AreTransmitting == TRUE)
    {
        ELG(0, 0, "SetUpTransmit - Already transmitting");
        return false;
    }

        // To start the ball rolling let's place first block in the transmit
        // buffer, after it's done with the one the ISR will take over.
        
    if (GetQueueStatus(&(Channel->TX)) != queueEmpty)
    {
        SccdbdmaStartTransmission(Channel);
    }
 
    return true;
    
}/* end SetUpTransmit */

/****************************************************************************************************/
//
//		Function:	SccDisableInterrupts
//
//		Inputs:		Channel - The port
//				WhichInts - The interrupts to disable
//
//		Outputs:	Return value - Previous interrupt state
//
//		Desc:		Disable the specified interrupt(s).
//
/****************************************************************************************************/

UInt8 SccDisableInterrupts(SccChannel *Channel, UInt32 WhichInts)
{
    UInt8	previousState = 0;
    UInt8	sccState;
    
    ELG(0, 0, "SccDisableInterrupts");

    if (Channel->ControlRegister)
    {
        switch ( WhichInts)
        {
            case kTxInterrupts:					// Turn off tx interrupts
                sccState = Channel->lastWR[5];
                previousState = (UInt8)sccState;
                SccWriteReg(Channel, R5, sccState & ~kTxEnable);
                break;
            
            case kRxInterrupts:					// Turn off rx interrupts
                sccState = Channel->lastWR[3];
                previousState = (UInt8)sccState;
                SccWriteReg(Channel, R3, sccState & ~kRxEnable);
                break;
            
            case kSccInterrupts:				// Turn off the scc interrupt processing
                sccState = Channel->lastWR[9];
                previousState = (UInt8)sccState;
                SccWriteReg(Channel, R9, sccState & ~kMIE & ~kNV);
                break;
            
            default:
                break;
        }
    }
    
    return previousState;
    
}/* end SccDisableInterrupts */

/****************************************************************************************************/
//
//		Function:	SccEnableInterrupts
//
//		Inputs:		Channel - The port
//				WhichInts - The interrupts to disable
//				previousState - Unused
//
//		Outputs:	
//
//		Desc:		Enable the specified interrupt(s).
//
/****************************************************************************************************/

void SccEnableInterrupts(SccChannel *Channel, UInt32 WhichInts, UInt8 previousState)
{

    ELG(0, 0, "SccEnableInterrupts");

    switch (WhichInts)
    {
        case kTxInterrupts:					// Turn on tx interrupts (regardless of previous state)
            SccWriteReg(Channel, R5, Channel->lastWR[5] | kTxEnable);
            break;
            
        case kRxInterrupts:					// Turn on rx interrupts (regardless of previous state)
            SccWriteReg(Channel, R3, Channel->lastWR[3] | kRxEnable);
            SccWriteReg(Channel, R0, kResetRxInt);
            break;
            
        case kSccInterrupts:					// Turn on Scc interrupts (regardless of previous state)
            SccWriteReg(Channel, R9, Channel->lastWR[9] | kMIE | kNV);
            break;
            
        default:
            break;
    }
    
}/* end SccEnableInterrupts */

/****************************************************************************************************/
//
//		Function:	SccSetBreak
//
//		Inputs:		Channel - The port
//				setBreak - true(send break), false(clear break)
//
//		Outputs:	
//
//		Desc:		Set and clear line break.
//
/****************************************************************************************************/

void SccSetBreak(SccChannel *Channel, bool setBreak)
{

    ELG(0, setBreak, "SccSetBreak");
    
    if (setBreak)
    {
        SccWriteReg(Channel, R5, Channel->lastWR[5] | kSendBreak);
    } else {
        SccWriteReg(Channel, R5, Channel->lastWR[5] & ~kSendBreak);
    }
    
}/* end SccSetBreak */


/****************************************************************************************************/
//
//		Function:	SccSetDTR
//
//		Inputs:		Channel - The port
//				assertDTR - true(assert), false(de-assert)
//
//		Outputs:	
//
//		Desc:		Set the DTR Line.
//
/****************************************************************************************************/

void SccSetDTR(SccChannel *Channel, bool assertDTR)
{

    ELG(0, assertDTR, "SccSetDTR");
    
    if (assertDTR)
    {
        SccWriteReg(Channel, R5, Channel->lastWR[5] | kDTR);
    } else {
        SccWriteReg(Channel, R5, Channel->lastWR[5] & ~kDTR);
    }
    
}/* end SccSetDTR */

/****************************************************************************************************/
//
//		Function:	SccSetRTS
//
//		Inputs:		Channel - The port
//				assertRTS - true(assert), false(de-assert)
//
//		Outputs:	
//
//		Desc:		Set the RTS Line.
//
/****************************************************************************************************/

void SccSetRTS(SccChannel *Channel, bool assertRTS)
{

    ELG(0, assertRTS, "SccSetRTS");
    
    if (assertRTS)
    {
        SccWriteReg(Channel, R5, Channel->lastWR[5] | kRTS);
    } else {
        SccWriteReg(Channel, R5, Channel->lastWR[5] & ~kRTS);
    }
    
}/* end SccSetRTS */

/****************************************************************************************************/
//
//		Function:	SccGetDCD
//
//		Inputs:		Channel - The port
//
//		Outputs:	Return value - true(asserted), false(de-asserted)
//
//		Desc:		Get the state of the Carrier Detect Line.
//
/****************************************************************************************************/

bool SccGetDCD(SccChannel *Channel)
{
    bool	Value;

    ELG(0, 0, "SccGetDCD");

    Value = (SccReadReg(Channel, R0) &  kDCDAsserted);
    if (Value) 
    {
        Channel->State |= PD_RS232_S_CAR;
	Channel->DCDState = true;			// jdg: do we need two bits for this?
    } else {
        Channel->State &= ~PD_RS232_S_CAR;
	Channel->DCDState = false;			// jdg: do we need two bits for this?
    }
        
    return Value;
    
}/* end SccGetDCD */

/****************************************************************************************************/
//
//		Function:	SccGetCTS
//
//		Inputs:		Channel - The port
//
//		Outputs:	Return value - true(asserted), false(de-asserted)
//
//		Desc:		Get the state of the Clear to Send Line.
//
/****************************************************************************************************/

bool SccGetCTS(SccChannel *Channel)
{
    bool	Value;
    
    ELG(0, 0, "SccGetCTS");

    Value = (SccReadReg(Channel, R0) &  kCTSAsserted);
    if (Value)
    {
        Channel->State |= PD_RS232_S_CTS;
    } else {
        Channel->State &= ~PD_RS232_S_CTS;
    }

    return Value;
    
}/* end SccGetCTS */

/****************************************************************************************************/
//
//		Function:	SccSetDMARegisters
//
//		Inputs:		Channel - The port
//				provider - The provider
//
//		Outputs:	
//
//		Desc:		Set up the DMA registers.
//
/****************************************************************************************************/

void SccSetDMARegisters(SccChannel *Channel, IOService *provider)
{
    UInt32	firstDMAMap = 1;
    IOMemoryMap	*map;
    
    ELG(0, 0, "SccSetDMARegisters");

    Channel->TxDBDMAChannel.dmaChannelAddress = NULL;
    Channel->TxDBDMAChannel.dmaBase = NULL;
    Channel->RxDBDMAChannel.dmaChannelAddress = NULL;
    Channel->RxDBDMAChannel.dmaBase = NULL;
    
    for(firstDMAMap = 1; ; firstDMAMap++)
    {
        map = provider->mapDeviceMemoryWithIndex(firstDMAMap);
        if (!map)
            return;

        if (map->getLength() > 1)
            break;
    }

    Channel->TxDBDMAChannel.dmaChannelAddress = (IODBDMAChannelRegisters*)map->getVirtualAddress();
//    Channel->TxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getPhysicalAddress();
    Channel->TxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getVirtualAddress();
    
    map = provider->mapDeviceMemoryWithIndex(firstDMAMap + 1);
    if (!map)
        return;
        
    Channel->RxDBDMAChannel.dmaChannelAddress = (IODBDMAChannelRegisters*)map->getVirtualAddress();
//    Channel->RxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getPhysicalAddress();
    Channel->RxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getVirtualAddress();
    
}/* end SccSetDMARegisters */

/****************************************************************************************************/
//
//		Function:	SccEnableDMAInterruptSources
//
//		Inputs:		Channel - The port
//				onOff - true(on), false(off)
//
//		Outputs:	
//
//		Desc:		Set up the DMA interrupts.
//
/****************************************************************************************************/

void SccEnableDMAInterruptSources(SccChannel *Channel, bool onOff)
{
    UInt8	dmaRemoveInterrupt = kRxIntAllOrSC | kTxIntEnable;
    UInt8	dmaInterruptMask = kDMAReqSelect | kDMAReqOnRx | kDMAReqOnTx | kWReqEnable | kExtIntEnable | kRxIntOnlySC;
    UInt8	newRegisterValue;
    
    ELG(0, onOff, "SccEnableDMAInterruptSources");

    if (onOff)
    {
        newRegisterValue = Channel->lastWR[1] & (~dmaRemoveInterrupt) | dmaInterruptMask;
    } else {
        newRegisterValue =  Channel->lastWR[1] & (~dmaInterruptMask) | dmaRemoveInterrupt;
    }

    SccWriteReg(Channel, R1, newRegisterValue);

    SccEnableInterrupts(Channel, kRxInterrupts, 0);

        // Remind the receiver to call when a new character enters the fifo
        
    SccWriteReg(Channel, R0, kResetRxInt);

        // Also when one of the status bits changes
    
    SccWriteReg(Channel, R0, kResetExtStsInt);
    
}/* end SccEnableDMAInterruptSources */

/****************************************************************************************************/
//
//		Function:	SccSetupReceptionChannel
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Set up the RX DMA channel.
//
/****************************************************************************************************/

void SccSetupReceptionChannel(SccChannel *Channel)
{
    SerialDBDMAStatusInfo *dmaInfo;
    
    ELG(0, 0, "SccSetupReceptionChannel");
    
    dmaInfo = &Channel->RxDBDMAChannel;

        // Just in case
        
    IODBDMAReset(dmaInfo->dmaBase);
    dmaInfo->lastPosition = 0;             		// last position of the dma
    
        // Again just in case
        
    IODBDMAReset(dmaInfo->dmaBase);
    
}/* end SccSetupReceptionChannel */

/****************************************************************************************************/
//
//		Function:	SccFreeReceptionChannel
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Free the RX DMA channel.
//
/****************************************************************************************************/

void SccFreeReceptionChannel(SccChannel *Channel)
{
    SerialDBDMAStatusInfo *dmaInfo;
    
    ELG(0, 0, "SccFreeupReceptionChannel");
    
    if (Channel == NULL)
        return;
    
    if (Channel->IODBDMARxLock)
    {
        IOLockLock(Channel->IODBDMARxLock);
        SccdbdmaEndReception(Channel);

        dmaInfo = &Channel->RxDBDMAChannel;

#if NEWPHYS    
        if (dmaInfo->dmaChannelCommandAreaMDP != NULL)
        {
            dmaInfo->dmaChannelCommandAreaMDP->complete();
            dmaInfo->dmaChannelCommandAreaMDP->release();
            dmaInfo->dmaChannelCommandAreaMDP = NULL;
        }
#else
        if (dmaInfo->dmaChannelCommandArea != NULL)
            IOFreeAligned(dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors);
#endif

#if NEWPHYS
        if (dmaInfo->dmaTransferBufferMDP != NULL)
        {
            dmaInfo->dmaTransferBufferMDP->complete();
            dmaInfo->dmaTransferBufferMDP->release();
            dmaInfo->dmaTransferBufferMDP = NULL;
        }
#else
        if (dmaInfo->dmaTransferBuffer != NULL)
            IOFreeAligned(dmaInfo->dmaTransferBuffer, PAGE_SIZE);
#endif

        dmaInfo->lastPosition = 0;
        dmaInfo->dmaNumberOfDescriptors = 0;
        dmaInfo->dmaChannelCommandArea = NULL;
        dmaInfo->dmaTransferBuffer = NULL;
        
        IOLockUnlock(Channel->IODBDMARxLock);
    }
    
}/* end SccFreeReceptionChannel */

/****************************************************************************************************/
//
//		Function:	SccdbdmaDefineReceptionCommands
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Set up the RX DMA channel commands.
//
/****************************************************************************************************/

void SccdbdmaDefineReceptionCommands(SccChannel *Channel)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    UInt32			iterator;
    IOPhysicalAddress		physaddr;
    IOPhysicalAddress		physaddr1;
    
    ELG(0, 0, "SccdbdmaDefineReceptionCommands");
    
    dmaInfo = &Channel->RxDBDMAChannel;

        // check the legality of the transmission
        
    if ((dmaInfo->dmaChannelCommandArea == NULL) || (dmaInfo->dmaTransferBuffer == NULL))
        return;
        
#if NEWPHYS
    IOByteCount	temp;
    if (dmaInfo->dmaTransferBufferMDP == NULL)
        return;
#endif

    for (iterator = 0; iterator < dmaInfo->dmaNumberOfDescriptors; iterator ++)
    {
        if (iterator == 0)
        {
        
                // The first is important becuse it has to generate an interrupt

#if NEWPHYS
//            physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalAddress();
            physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(0, &temp);
            ELG(physaddr, dmaInfo->dmaTransferBuffer, "SccdbdmaDefineReceptionCommands - Physical/Virtual RX buffer");
            ELG(0, &dmaInfo->dmaChannelCommandArea[iterator], "SccdbdmaDefineReceptionCommands - RX Command area");
            IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[iterator], kdbdmaInputMore, kdbdmaKeyStream0, kdbdmaIntAlways, kdbdmaBranchNever,
                                                                                                                            kdbdmaWaitNever, 1, physaddr);
#else
            physaddr = pmap_extract(kernel_pmap, (vm_address_t)dmaInfo->dmaTransferBuffer);
            ELG(physaddr, dmaInfo->dmaTransferBuffer, "SccdbdmaDefineReceptionCommands - Physical/Virtual RX buffer");
            ELG(0, &dmaInfo->dmaChannelCommandArea[iterator], "SccdbdmaDefineReceptionCommands - RX Command area");
            IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[iterator], kdbdmaInputMore, kdbdmaKeyStream0, kdbdmaIntAlways, kdbdmaBranchNever,
                                                                                                                            kdbdmaWaitNever, 1, physaddr);
#endif            
        } else {
            if (iterator == (dmaInfo->dmaNumberOfDescriptors - 1))
            {
            
                    // The last one is special because it closes the chain, even if it is the last command it looks
                    // exacly like the first, except that it does not generate an interrupt and that it branches to
                    // the second command

#if NEWPHYS
//                physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalAddress();
                physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(0, &temp);
                ELG(physaddr, dmaInfo->dmaTransferBuffer, "SccdbdmaDefineReceptionCommands - Physical/Virtual RX buffer");
//                physaddr1 = dmaInfo->dmaChannelCommandAreaMDP->getPhysicalAddress() + 1;
//                physaddr1 = dmaInfo->dmaChannelCommandAreaMDP->getPhysicalSegment(1, &temp);
                physaddr1 = dmaInfo->dmaChannelCommandAreaMDP->getPhysicalSegment(sizeof(IODBDMADescriptor), &temp);
                ELG(physaddr1, &dmaInfo->dmaChannelCommandArea[1], "SccdbdmaDefineReceptionCommands - Physical/Virtual RX Command area [1]");
                ELG(0, &dmaInfo->dmaChannelCommandArea[iterator], "SccdbdmaDefineReceptionCommands - RX Command area");
                IOMakeDBDMADescriptorDep(&dmaInfo->dmaChannelCommandArea[iterator], kdbdmaInputMore, kdbdmaKeyStream0, kdbdmaIntNever, kdbdmaBranchAlways,
                                                                                                                    kdbdmaWaitNever, 1, physaddr, physaddr1);
#else
                physaddr = pmap_extract(kernel_pmap, (vm_address_t)dmaInfo->dmaTransferBuffer);
                ELG(physaddr, dmaInfo->dmaTransferBuffer, "SccdbdmaDefineReceptionCommands - Physical/Virtual");
                physaddr1 = pmap_extract(kernel_pmap, (vm_address_t)&dmaInfo->dmaChannelCommandArea[1]);
                ELG(physaddr1, &dmaInfo->dmaChannelCommandArea[1], "SccdbdmaDefineReceptionCommands - Physical/Virtual RX Command area [1]");
                ELG(0, &dmaInfo->dmaChannelCommandArea[iterator], "SccdbdmaDefineReceptionCommands - RX Command area");
                IOMakeDBDMADescriptorDep(&dmaInfo->dmaChannelCommandArea[iterator], kdbdmaInputMore, kdbdmaKeyStream0, kdbdmaIntNever, kdbdmaBranchAlways,
                                                                                                                    kdbdmaWaitNever, 1, physaddr, physaddr1);
#endif
            } else {
        
                    // All the others just transfer a byte and move along to the next one

#if NEWPHYS                
//                physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalAddress() + iterator;
                physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(iterator, &temp);
                ELG(physaddr, &dmaInfo->dmaTransferBuffer[iterator], "SccdbdmaDefineReceptionCommands - Physical/Virtual RX buffer");
                ELG(0, &dmaInfo->dmaChannelCommandArea[iterator], "SccdbdmaDefineReceptionCommands - RX Command area");
                IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[iterator], kdbdmaInputMore, kdbdmaKeyStream0, kdbdmaIntNever, kdbdmaBranchNever,
                                                                                                                                kdbdmaWaitNever, 1, physaddr);
#else
                physaddr = pmap_extract(kernel_pmap, (vm_address_t)dmaInfo->dmaTransferBuffer + iterator);
                ELG(physaddr, &dmaInfo->dmaTransferBuffer[iterator], "SccdbdmaDefineReceptionCommands - Physical/Virtual RX buffer");
                ELG(0, &dmaInfo->dmaChannelCommandArea[iterator], "SccdbdmaDefineReceptionCommands - RX Command area");
                IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[iterator], kdbdmaInputMore, kdbdmaKeyStream0, kdbdmaIntNever, kdbdmaBranchNever,
                                                                                                                                kdbdmaWaitNever, 1, physaddr);
#endif
            }
        }
    }
    
}/* end SccdbdmaDefineReceptionCommands */

/****************************************************************************************************/
//
//		Function:	SccdbdmaStartReception
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Start the RX DMA channel.
//
/****************************************************************************************************/

void SccdbdmaStartReception(SccChannel *Channel)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    IODBDMADescriptor		*baseCommands;
    
    ELG(0, 0, "SccdbdmaStartReception");
    
    dmaInfo = &Channel->RxDBDMAChannel;

        // reset the dma channel
        
    IODBDMAReset(dmaInfo->dmaBase);
    IODBDMAReset(dmaInfo->dmaBase);
    
        // check the legality of the reception
        
    if ((dmaInfo->dmaChannelCommandArea == NULL) || (dmaInfo->dmaTransferBuffer == NULL))
        return;

#if NEWPHYS
    IOByteCount	temp;
    baseCommands = (IODBDMADescriptor *)dmaInfo->dmaChannelCommandAreaMDP->getPhysicalSegment(0, &temp);
//    baseCommands = (IODBDMADescriptor *)dmaInfo->dmaChannelCommandAreaMDP->getPhysicalAddress();
#else
    baseCommands = (IODBDMADescriptor *)pmap_extract(kernel_pmap, (vm_address_t)dmaInfo->dmaChannelCommandArea);
#endif
    ELG(0, baseCommands, "SccdbdmaStartReception - Base physical address");
    
    flush_dcache((vm_offset_t)dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors, false);

    dmaInfo->lastPosition = 0;
       
        // Enables the receiver and starts
        
    SccEnableInterrupts(Channel, kRxInterrupts, 0);
    IODBDMAStart(dmaInfo->dmaBase, baseCommands);
    
}/* end SccdbdmaStartReception */

/****************************************************************************************************/
//
//		Function:	CommandExecuted
//
//		Inputs:		Channel - The port
//				iterator - Where we're up to
//
//		Outputs:	Return code - true(error), false(ok)
//
//		Desc:		Get the status of the command just executed.
//
/****************************************************************************************************/

bool CommandExecuted(SccChannel *Channel, UInt32 iterator)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    UInt32			result;
    
    ELG(0, iterator, "CommandExecuted");
    
    dmaInfo = &Channel->RxDBDMAChannel;
    result = IOGetCCResult(&dmaInfo->dmaChannelCommandArea[iterator]);

    if (result != 0)
        return true;

    return false;
    
}/* end CommandExecuted */

/****************************************************************************************************/
//
//		Function:	CommandStatus
//
//		Inputs:		Channel - The port
//				commandNumber - The command in question
//
//		Outputs:	Return value - DMA error (or not)
//
//		Desc:		Get the status of the specified command.
//
/****************************************************************************************************/

UInt32 CommandStatus(SccChannel *Channel, UInt32 commandNumber)
{
    SerialDBDMAStatusInfo *dmaInfo;
    
//    ELG(0, commandNumber, "CommandStatus");
    
    dmaInfo = &Channel->RxDBDMAChannel;
    
    return IOGetCCResult(&dmaInfo->dmaChannelCommandArea[commandNumber]);
    
}/* end CommandStatus */

/****************************************************************************************************/
//
//		Function:	SccdbdmaRxHandleCurrentPosition
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Handles the current position/situation in the RX DMA channel.
//
/****************************************************************************************************/

void SccdbdmaRxHandleCurrentPosition(SccChannel *Channel)
{
    AppleRS232Serial		*RS232;
    SerialDBDMAStatusInfo	*dmaInfo;
    UInt32			kDataIsValid;
    bool			gotData;
    natural_t			numberOfbytes;
    natural_t			bitInterval;
    natural_t			nsec;
    UInt8			savedR1Reg;
    UInt32			commandStatus;
    
    ELG(0, 0, "SccdbdmaRxHandleCurrentPosition");
    
    if (Channel == NULL)
        return;
        
    RS232 = (AppleRS232Serial *)Channel->RS232;
    
    dmaInfo = &Channel->RxDBDMAChannel;
            
    IOLockLock (Channel->IODBDMARxLock);
    
    kDataIsValid = (kdbdmaStatusRun | kdbdmaStatusActive);


        // Sanity check
        
    if ((dmaInfo->dmaChannelCommandArea == NULL) || (dmaInfo->dmaTransferBuffer == NULL))
    {
        IOLockUnlock(Channel->IODBDMARxLock);
        return;
    }

        // Checks for errors in the SCC since they have the nasty habit of blocking the FIFO
        
    SccHandleExtErrors(Channel);
    
    gotData = SuckDataFromTheDBDMAChain(Channel);
    if (gotData)								// Did we get anything?
    {
        RS232->CheckQueues(Channel);						// Clearly our buffer is no longer Empty
//        AppleRS232Serial::CheckQueues(Channel);					// Clearly our buffer is no longer Empty

            // Reschedule for a later time
            // Checking after a third of the buffer is full seems reasonable
            
        if (Channel->DataLatInterval.tv_nsec == 0)
        {
            numberOfbytes = (dmaInfo->dmaNumberOfDescriptors - 1) / 3;
            bitInterval = (1000000 * 10)/Channel->BaudRate;        		// bit interval in \xb5Sec
            nsec = bitInterval * 1000 * numberOfbytes;     			// nSec*bits
        } else {
            nsec = Channel->DataLatInterval.tv_nsec;
        }
        
        IOLockUnlock (Channel->IODBDMARxLock);

#if USE_WORK_LOOPS
        rearmRxTimer(Channel, nsec);
#else
        clock_interval_to_deadline(nsec, 1, &deadline);
        thread_call_func_delayed((thread_call_func_t) SccdbdmaRxHandleCurrentPosition, Channel, deadline);
#endif
    } else {
    
            // Ok since we didn't get anything let's stop the channel and restart for the
            // the byte with the interrupt, the first one.
     
        IODBDMAPause(dmaInfo->dmaBase);						 // Stops the reception
        savedR1Reg = Channel->lastWR[1];
        SccWriteReg(Channel, R1, savedR1Reg & ~kWReqEnable);
        IOSetDBDMAChannelControl(dmaInfo->dmaBase, IOClearDBDMAChannelControlBits(kdbdmaPause));
        
        IODBDMAFlush(dmaInfo->dmaBase);
        IOSetDBDMAChannelControl(dmaInfo->dmaBase, IOClearDBDMAChannelControlBits(kdbdmaRun));
        	
        while(IOGetDBDMAChannelStatus(dmaInfo->dmaBase) & (kdbdmaActive))
        {
            eieio();
        }
        
        SccWriteReg(Channel, R1, savedR1Reg);
		
        commandStatus = CommandStatus(Channel, dmaInfo->lastPosition);
        if ((commandStatus & kDataIsValid) == kDataIsValid)				// If the current command is valid...there's some data!
        {
            if (SuckDataFromTheDBDMAChain(Channel))
            {
                RS232->CheckQueues(Channel);						// Clearly our buffer is no longer Empty

//                AppleRS232Serial::CheckQueues(Channel);					// Clearly our buffer is no longer Empty
            }
        }

        IOSetCCResult(&dmaInfo->dmaChannelCommandArea[dmaInfo->lastPosition], 0);	// This is stopped, so reset its status
        eieio();
        
        SccdbdmaStartReception(Channel);
        IOLockUnlock (Channel->IODBDMARxLock);
    }
    
}/* end SccdbdmaRxHandleCurrentPosition */

/****************************************************************************************************/
//
//		Function:	SuckDataFromTheDBDMAChain
//
//		Inputs:		Channel - The port
//
//		Outputs:	Return code - true(got data), false(none)
//
//		Desc:		Gets data from the RX DMA channel.
// 				Starting from the last known position read all the bytes transferred.
//				The last command to run is known so the last position is known.
//
/****************************************************************************************************/

bool SuckDataFromTheDBDMAChain(SccChannel *Channel)
{   
    AppleRS232Serial		*RS232;
    UInt32			SW_FlowControl;
    SerialDBDMAStatusInfo 	*TxdmaInfo;
    SerialDBDMAStatusInfo 	*dmaInfo;
    UInt32 			iterator;
    UInt32			kDataIsValid;
    UInt8			byteRead;
    bool 			gotData = false;
    UInt32			excess = 0;
    
    ELG(0, 0, "SuckDataFromTheDBDMAChain");
    
    RS232 = (AppleRS232Serial *)Channel->RS232;

    SW_FlowControl = Channel->FlowControl & PD_RS232_S_RXO;
    dmaInfo = &Channel->RxDBDMAChannel;
    TxdmaInfo = &Channel->TxDBDMAChannel;
    iterator = dmaInfo->lastPosition;
	
    kDataIsValid = (kdbdmaStatusRun | kdbdmaStatusActive);  
    while ((CommandStatus(Channel, iterator) & kDataIsValid) == kDataIsValid)
    {

            // Reset the count for the current command
            
        IOSetCCResult(&dmaInfo->dmaChannelCommandArea[iterator], 0);
        eieio();

            // This is a special ring where the last command does not point to command 0
            // but to command 1 and it does not write on the last byte but on byte 0
            
        if (iterator == (dmaInfo->dmaNumberOfDescriptors - 1))
        {
            byteRead = dmaInfo->dmaTransferBuffer[0];					// Transfer the new byte in the buffer
            iterator = 1;
        } else {
            byteRead = dmaInfo->dmaTransferBuffer[iterator];				// Transfer the new byte in the buffer
            iterator++;
        }

            // Begin software flow control code
	
        if (SW_FlowControl)
        {
            if (byteRead == Channel->XONchar)
            {
                if (Channel->RXOstate == NEEDS_XON)
                {
                    Channel->RXOstate = NEEDS_XOFF;
                    IODBDMAContinue(TxdmaInfo->dmaBase);				// Continue transfer
                    Channel->FlowControlState = CONTINUE_SEND;
                }
            } else {
                if (byteRead == Channel->XOFFchar)
                {
                    if (Channel->RXOstate == NEEDS_XOFF)
                    {
                        Channel->RXOstate = NEEDS_XON;
                        IODBDMAPause(TxdmaInfo->dmaBase);				// Pause transfer
                        Channel->FlowControlState = PAUSE_SEND;
                    }
                } else {
                    AddBytetoQueue(&(Channel->RX), byteRead);				// Char is not XON or XOFF, so need to put it on the queue
                }
            }
        } else {
            AddBytetoQueue(&(Channel->RX), byteRead);
            excess = UsedSpaceinQueue(&(Channel->RX)) - Channel->RXStats.HighWater;
            if (!Channel->aboveRxHighWater)
            {
                if (excess > 0)
                {
                    Channel->aboveRxHighWater = true;
                    RS232->CheckQueues(Channel);
//                    AppleRS232Serial::CheckQueues(Channel);
                }
            } else {
                if (excess <= 0)
                {
                    Channel->aboveRxHighWater = false;
                }
            }
        }
        gotData = true;
    }

    dmaInfo->lastPosition = iterator;
        
    return gotData;
        
}/* end SuckDataFromTheDBDMAChain */

/****************************************************************************************************/
//
//		Function:	SccdbdmaEndReception
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		End receiving on the RX DMA channel.
//
/****************************************************************************************************/

void SccdbdmaEndReception(SccChannel *Channel)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    UInt32			cmdResult = 0;
    
    ELG(0, 0, "SccdbdmaEndReception");
    
    dmaInfo = &Channel->RxDBDMAChannel;
    
    if (dmaInfo->dmaBase)
    {
        IODBDMAStop(dmaInfo->dmaBase);					// Stop transfer
        IODBDMAReset(dmaInfo->dmaBase);					// reset transfer
    }
    
    if (dmaInfo->dmaChannelCommandArea != NULL)
    {
        cmdResult = IOGetDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], result);
        dmaInfo->dmaTransferSize = dmaInfo->dmaTransferSize - (cmdResult & 0xFFFF);
        ELG(0, dmaInfo->dmaTransferSize, "SccdbdmaEndReception - number of bytes received");
    }
    
}/* end SccdbdmaEndReception */

/****************************************************************************************************/
//
//		Function:	SccSetupTansmissionChannel
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Set up the TX DMA channel.
//
/****************************************************************************************************/

void SccSetupTansmissionChannel(SccChannel *Channel)
{
    SerialDBDMAStatusInfo *dmaInfo;
    
    ELG(0, 0, "SccSetupTansmissionChannel");
    
    dmaInfo = &Channel->TxDBDMAChannel;
    
    IODBDMAReset(dmaInfo->dmaBase);					// Just in case
    dmaInfo->lastPosition = 0;
    IODBDMAReset(dmaInfo->dmaBase);					// Again just in case
    
}/* end SccSetupTansmissionChannel */

/****************************************************************************************************/
//
//		Function:	SccFreeTansmissionChannel
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Free the TX DMA channel.
//
/****************************************************************************************************/

void SccFreeTansmissionChannel(SccChannel *Channel)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    
    ELG(0, 0, "SccFreeTansmissionChannel");
    
    if (Channel == NULL)
        return;
        
    if (Channel->IODBDMATrLock)
    {
        IOLockLock(Channel->IODBDMATrLock);
        SccdbdmaEndTransmission(Channel);

        dmaInfo = &Channel->TxDBDMAChannel;

#if NEWPHYS        
        if (dmaInfo->dmaChannelCommandAreaMDP != NULL)
        {
            dmaInfo->dmaChannelCommandAreaMDP->complete();
            dmaInfo->dmaChannelCommandAreaMDP->release();
            dmaInfo->dmaChannelCommandAreaMDP = NULL;
        }
#else
        if (dmaInfo->dmaChannelCommandArea != NULL)
            IOFreeAligned(dmaInfo->dmaChannelCommandArea, PAGE_SIZE);
#endif

#if NEWPHYS
        if (dmaInfo->dmaTransferBufferMDP != NULL)
        {
            dmaInfo->dmaTransferBufferMDP->complete();
            dmaInfo->dmaTransferBufferMDP->release();
            dmaInfo->dmaTransferBufferMDP = NULL;
        }
#else
        if (dmaInfo->dmaTransferBuffer != NULL)
            IOFreeAligned(dmaInfo->dmaTransferBuffer, PAGE_SIZE);
#endif

        dmaInfo->dmaNumberOfDescriptors = 0;
        dmaInfo->dmaChannelCommandArea = NULL;
        dmaInfo->dmaTransferBuffer = NULL;
    
        IOLockUnlock(Channel->IODBDMATrLock);
    }
    
}/* end SccFreeTansmissionChannel */

/****************************************************************************************************/
//
//		Function:	SccdbdmaDefineTansmissionCommands
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Set up the TX DMA channel commands.
//
/****************************************************************************************************/

void SccdbdmaDefineTansmissionCommands(SccChannel *Channel)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    IOPhysicalAddress		physaddr;
    
    ELG(0, 0, "SccdbdmaDefineTansmissionCommands");
    
    dmaInfo = &Channel->TxDBDMAChannel;

#if NEWPHYS
    IOByteCount	temp;
//    physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalAddress();
    physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(0, &temp);
    IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], kdbdmaOutputLast, kdbdmaKeyStream0, kdbdmaIntAlways, kdbdmaBranchNever, kdbdmaWaitNever, 0,
                                                                                                                                                    physaddr);                                                                                                                                                    
#else   
    physaddr = pmap_extract(kernel_pmap, (vm_address_t)dmaInfo->dmaTransferBuffer);
    IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], kdbdmaOutputLast, kdbdmaKeyStream0, kdbdmaIntAlways, kdbdmaBranchNever, kdbdmaWaitNever, 0,
                                                                                                                                                    physaddr);
#endif
    ELG(physaddr, dmaInfo->dmaTransferBuffer, "SccdbdmaDefineTansmissionCommands - Physical/Virtual");

    IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[1], kdbdmaStop, kdbdmaKeyStream0, kdbdmaIntNever, kdbdmaBranchNever, kdbdmaWaitNever, 0, 0);
    
}/* end SccdbdmaDefineTansmissionCommands */

/****************************************************************************************************/
//
//		Function:	SccdbdmaStartTransmission
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Start the TX DMA channel.
//
/****************************************************************************************************/

void SccdbdmaStartTransmission(SccChannel *Channel)
{
    AppleRS232Serial		*RS232;
    SerialDBDMAStatusInfo	*dmaInfo;
    IODBDMADescriptor		*baseCommands;
    UInt8			*localBuffer;
    UInt32			sizeReadFromBuffer = 0;
    UInt8			*bufferPtr = 0;
    UInt32			bytesToTransfer;
    bool			wrapped = false;
    
    ELG(0, 0, "SccdbdmaStartTransmission");
    
    if (Channel == NULL)
        return;
        
    RS232 = (AppleRS232Serial *)Channel->RS232;
        
    IOLockLock(Channel->IODBDMATrLock);
	
    dmaInfo = &Channel->TxDBDMAChannel;

        // Check for errors in the SCC since they have the nasty habit of blocking the FIFO
        
    SccHandleExtErrors(Channel);

        // Set up everything as we are running, handle the situation where it may get called twice
        
    Channel->AreTransmitting = TRUE;
    RS232->setStateGated(PD_S_TX_BUSY, PD_S_TX_BUSY);
//    AppleRS232Serial::setStateGated(PD_S_TX_BUSY, PD_S_TX_BUSY);
    
        // check the legality of the transmission
        
    if ((dmaInfo->dmaChannelCommandArea != NULL) && (dmaInfo->dmaTransferBuffer != NULL))
    {
        IODBDMAReset(dmaInfo->dmaBase);					// Reset the channel

#if NEWPHYS
    IOByteCount	temp;
//        baseCommands = (IODBDMADescriptor *)dmaInfo->dmaChannelCommandAreaMDP->getPhysicalAddress();
        baseCommands = (IODBDMADescriptor *)dmaInfo->dmaChannelCommandAreaMDP->getPhysicalSegment(0, &temp);
#else
        baseCommands = (IODBDMADescriptor *)pmap_extract(kernel_pmap, (vm_address_t)dmaInfo->dmaChannelCommandArea);
#endif
        
        localBuffer = dmaInfo->dmaTransferBuffer;

            // Fill up the buffer with characters from the queue
		
        bytesToTransfer = UsedSpaceinQueue(&(Channel->TX));
        if (bytesToTransfer)
        {
            sizeReadFromBuffer = bytesToTransfer;
            bufferPtr = BeginDirectReadFromQueue(&(Channel->TX), &sizeReadFromBuffer, &wrapped);
            if (bufferPtr && sizeReadFromBuffer)
            {
                LogData(kSerialOut, sizeReadFromBuffer, (char*)bufferPtr);
                if (sizeReadFromBuffer > MAX_BLOCK_SIZE)
                    sizeReadFromBuffer = MAX_BLOCK_SIZE;
                    
                dmaInfo->dmaTransferSize = sizeReadFromBuffer;
                bcopy(bufferPtr, localBuffer, sizeReadFromBuffer);
            }
        } else {
            dmaInfo->dmaTransferSize = 0;
        }
        
            // If there are no bytes to send just exit otherwise create the next transfer
            
        if ((Channel->FlowControlState != PAUSE_SEND) && (dmaInfo->dmaTransferSize > 0))
        {
            IOSetDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], operation, IOMakeDBDMAOperation(kdbdmaOutputLast, kdbdmaKeyStream0, kdbdmaIntAlways,
                                                                                                kdbdmaBranchNever, kdbdmaWaitNever, dmaInfo->dmaTransferSize));
            eieio();

            flush_dcache((vm_offset_t)dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors, false);

            IODBDMAStart(dmaInfo->dmaBase, baseCommands);			// Starts the transmission

            EndDirectReadFromQueue(&(Channel->TX), dmaInfo->dmaTransferSize);
            
                // We just removed a bunch of stuff from the queue, so see if we can free some threads
                // to enqueue more stuff.
                
            RS232->CheckQueues(Channel);
//            AppleRS232Serial::CheckQueues(Channel);

            IOLockUnlock(Channel->IODBDMATrLock);
            return;
        } else {
            EndDirectReadFromQueue(&(Channel->TX), 0);
        }
    }

        // Updates all the status flags
        
    RS232->CheckQueues(Channel);
//    AppleRS232Serial::CheckQueues(Channel);
    Channel->AreTransmitting = FALSE;
    RS232->setStateGated(0, PD_S_TX_BUSY);
//    AppleRS232Serial::setStateGated(0, PD_S_TX_BUSY);
    
    IOLockUnlock(Channel->IODBDMATrLock);
    
}/* end SccdbdmaStartTransmission */

/****************************************************************************************************/
//
//		Function:	SccdbdmaEndTransmission
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		End transmission on the TX DMA channel.
//
/****************************************************************************************************/

void SccdbdmaEndTransmission(SccChannel *Channel)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    UInt32			cmdResult = 0;
    
    ELG(0, 0, "SccdbdmaEndTransmission");
    
    dmaInfo = &Channel->TxDBDMAChannel;
    
    if (dmaInfo->dmaBase)
    {
        IODBDMAStop(dmaInfo->dmaBase);					// Stop transfer
        IODBDMAReset(dmaInfo->dmaBase);					// reset transfer
    }
    
    SccDisableInterrupts(Channel, kTxInterrupts);			// Disable the transmitter

    if (dmaInfo->dmaChannelCommandArea != NULL)
    {
        cmdResult = IOGetDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], result);
        dmaInfo->dmaTransferSize = dmaInfo->dmaTransferSize - (cmdResult & 0xFFFF);
        ELG(0, dmaInfo->dmaTransferSize, "SccdbdmaEndTransmission - number of bytes sent");
    }
    
}/* end SccdbdmaEndTransmission */

/****************************************************************************************************/
//
//		Function:	HandleRxIntTimeout
//
//		Inputs:		Channel - The port
//
//		Outputs:	
//
//		Desc:		Handles the RX interrupt timeout.
//				Currently not used other than for debugging.
//
/****************************************************************************************************/

void HandleRxIntTimeout(SccChannel *Channel)
{
    
//    ELG(0, 0, "HandleRxIntTimeout");
    
}/* end HandleRxIntTimeout */

/****************************************************************************************************/
//
//		Function:	rxTimeoutHandler
//
//		Inputs:		owner - Should be me
//				sender - Unused (should also be me)
//
//		Outputs:	
//
//		Desc:		RX timeout handler.
//
/****************************************************************************************************/

void rxTimeoutHandler(OSObject *owner, IOTimerEventSource *sender)
{
    AppleRS232Serial	*serialPortPtr;
    
//    ELG(0, 0, "rxTimeoutHandler");
	
	// Make sure it's me
        
    serialPortPtr = OSDynamicCast(AppleRS232Serial, owner);
    if(serialPortPtr)
    {		
        SccdbdmaRxHandleCurrentPosition(&serialPortPtr->fPort);
    }
    
}/* end rxTimeoutHandler */

#if USE_WORK_LOOPS
/****************************************************************************************************/
//
//		Function:	rearmRxTimer
//
//		Inputs:		Channel - The port
//				timerDelay - How long to set it
//
//		Outputs:	
//
//		Desc:		Re-arm the RX timer.
//
/****************************************************************************************************/
void rearmRxTimer(SccChannel *Channel, UInt32 timerDelay)
{

//    ELG(0, 0, "rearmRxTimer");

    Channel->rxTimer->setTimeout(timerDelay);
    
}/* end rearmRxTimer */
#endif
