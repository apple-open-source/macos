/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

//bool CommandExecuted(SccChannel *Channel, UInt32 iterator);
void SuckDataFromTheDBDMAChain(SccChannel *Channel, UInt32 index);
UInt32 CommandStatus(SerialDBDMAStatusInfo *dmaInfo, UInt32 commandNumber);
bool AnyDataReceived(SerialDBDMAStatusInfo *dmaInfo);

void rearmRxTimer(SccChannel *Channel, UInt32 timerDelay);


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
    R4, /*kX16ClockMode |*/ k1StopBit,		// WR  4, 16x clock, 1 stop bit, no parity (this could be 0x04 right? - …MB)
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

    //port->DataRegister = (UInt8 *)(port->ChipBaseAddress + channelDataOffsetRISC);
    port->ControlRegister         = port->ChipBaseAddress + channelControlOffsetRISC;

        // This is very bad but it will work with all of our hardware.
        // The right thing to do is to get the instance variables of the channel
        // and determine which one is channel B and set it to that value.

    //port->ConfigWriteRegister = (unsigned int)(port->ChipBaseAddress & 0xffffff00);

    //port->baudRateGeneratorEnable = kBRGEnable;
    port->rtxcFrequency = 3686400;

        // FIXTHIS  ejk
        
    SccWriteReg(port, R1, 0);

    ELG(0, port->ControlRegister, "ProbeSccDevice - Control register");

}


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
       
        // Now, initialize the chip
        
    SccWriteReg(Channel, 9, (Channel->whichPort == serialPortA ? kChannelResetA : kChannelResetB) | kNV);
        
        // Write configuration table info to the SCC
    
    for (i = 0; i < SCCConfigTblSize; i += 2)
    {
        SccWriteReg(Channel, SCCConfigTable[i], SCCConfigTable[i + 1]);
    }

        // Eanbles the chip interrupts
        
    SccEnableInterrupts(Channel, kSccInterrupts);

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
    
    ELG(0, 0, "SccCloseChannel");

        // This is to be sure that SccHandleMissedTxInterrupt ends
        
    if (Channel->AreTransmitting == true)
    {
        Channel->AreTransmitting = false;
        IOSleep(1000);
    }
    
    SccDisableInterrupts(Channel, kSccInterrupts);		// Disable scc interrupts before doing anything
    SccDisableInterrupts(Channel, kRxInterrupts);		// Disable the receiver
    SccDisableInterrupts(Channel, kTxInterrupts);		// Disable the transmitter

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

    SccEnableInterrupts( Channel, kRxInterrupts);
    SccEnableInterrupts( Channel, kSccInterrupts);

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
    UInt8	wr4Mirror;
    
    ELG(0, NewBaud, "SccSetBaud");

        // Disables the interrupts
        
    //SccDisableInterrupts(Channel, kSerialInterrupts);

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

            // Pin 0x0000 ² brgConstant ² 0xFFFF.

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
        
    //SccEnableInterrupts(Channel, kSerialInterrupts);

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

    ELG(0, ClockMode, "SccConfigureForMIDI");

    Channel->baudRateGeneratorLo = 0x00;
    Channel->baudRateGeneratorHi = 0x00;
 	
        // Disable interrupts
        
    //SccDisableInterrupts(Channel, kSerialInterrupts);

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
         
    //SccEnableInterrupts(Channel, kSerialInterrupts);
 
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
    
    ELG(0, sccRegister, "SccReadReg");
    
        // Make sure we have a valid register number to write to
        
    if (sccRegister != R0 )
    {
            // First write the register value to the chip
            
        *((volatile UInt8 *)Channel->ControlRegister) = sccRegister;
        OSSynchronizeIO();		// eieio()
        REGISTER_DELAY();
    }

        // Next get the data value
        
    ReturnValue = *((volatile UInt8 *)Channel->ControlRegister);
    
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

    ELG(0, Value, "SccWriteReg");

        // Make sure we have a valid register number to write to.

     if (sccRegister <= kNumSCCWR )
     {
        
            // First write the register value to the chip
             
        *((volatile UInt8 *)Channel->ControlRegister) = sccRegister;
        OSSynchronizeIO();		// eieio()
        REGISTER_DELAY();

            // Next write the data value
            
        *((volatile UInt8 *)Channel->ControlRegister) = Value;
        OSSynchronizeIO();		// eieio()
        REGISTER_DELAY();

            // Update the shadow register
            
        Channel->lastWR[sccRegister] = Value;
    }
            
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
    AppleRS232Serial *scc = (AppleRS232Serial *) identity;

    AbsoluteTime	deadline;

    ELG(identity, Channel, "PPCSerialTxDMAISR");
    
        // Request another send, but outside the interrupt handler
        // there is no reason to spend too much time here
    
    clock_interval_to_deadline(1, 1, &deadline);
    if (scc->fdmaStartTransmissionThread != NULL)
    {
		thread_call_enter_delayed(scc->fdmaStartTransmissionThread,deadline);
    }
    
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
    AppleRS232Serial *scc = (AppleRS232Serial *) identity;

    AbsoluteTime	deadline;
    
    ELG(identity, Channel, "PPCSerialRxDMAISR");

    // This is the first received byte, so start the checkData ballet

    clock_interval_to_deadline(1, 1, &deadline);
    thread_call_enter_delayed(scc->dmaRxHandleCurrentPositionThread,deadline);
    
}/* end PPCSerialRxDMAISR */


/****************************************************************************************************/
//
//		Function:	SccCurrentPositionDelayedHandlerAction
//
//		Inputs:		arg0 - The driver
//
//		Outputs:	 
//
//		Desc:		have the command gate again, can start sucking up some rx data.
//
/****************************************************************************************************/

IOReturn SccCurrentPositionDelayedHandlerAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{
    AppleRS232Serial *serialPortPtr = (AppleRS232Serial *) arg0;
    ELG(serialPortPtr, serialPortPtr->fWorkLoop->inGate(), "SccCurrentPositionDelayedHandlerAction - obj, inGate");
    SccdbdmaRxHandleCurrentPosition(&serialPortPtr->fPort, serialPortPtr->fPort.activeRxChannelIndex);
    return kIOReturnSuccess;
}

/****************************************************************************************************/
//
//		Function:	SccCurrentPositionDelayedHandler
//
//		Inputs:		arg - The driver
//
//		Outputs:	 
//
//		Desc:		grab the command gate before sucking up some rx data.
//
/****************************************************************************************************/

void SccCurrentPositionDelayedHandler( thread_call_param_t arg, thread_call_param_t )
{
    AppleRS232Serial *serialPortPtr = (AppleRS232Serial *)arg;
    ELG(serialPortPtr, serialPortPtr->fWorkLoop->inGate(), "SccCurrentPositionDelayedHandler - obj, inGate");
	
    serialPortPtr->fCommandGate->runAction(SccCurrentPositionDelayedHandlerAction, (void *)arg);
}

// **********************************************************************************
//
// Called asynchronously
//
// **********************************************************************************
IOReturn SccStartTransmissionDelayedHandlerAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{
    AppleRS232Serial *serialPortPtr = (AppleRS232Serial *) arg0;
    ELG(serialPortPtr, serialPortPtr->fWorkLoop->inGate(), "SccStartTransmissionDelayedHandlerAction - obj, inGate");
    SccdbdmaStartTransmission(&serialPortPtr->fPort);
    return kIOReturnSuccess;
}

void SccStartTransmissionDelayedHandler ( thread_call_param_t arg, thread_call_param_t )
{
    AppleRS232Serial *serialPortPtr = (AppleRS232Serial *) arg;
    ELG(serialPortPtr, serialPortPtr->fWorkLoop->inGate(), "SccStartTransmissionDelayedHandler - obj, inGate");
    serialPortPtr->fCommandGate->runAction(SccStartTransmissionDelayedHandlerAction, (void *)arg);
}



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
    SccEnableInterrupts(Channel, kSccInterrupts);

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
	} else {
            RS232->setStateGated(0, PD_RS232_S_CAR);
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
            
        if (Channel->ctsTransitionCount > 76)				// Magic number Å 128 transitions per 1/60 second
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
    
    // if we're doing cts flow control, and cts is down, and we're not already paused, then pause tx
    // if we're doing cts flow control, and cts is high, and we're paused, then continue it
    if (HW_FlowControl) 
    {
	dmaInfo = &Channel->TxDBDMAChannel;
	if (!currentCTSState)				// cts is down
	{
	    if (Channel->FlowControlState != PAUSE_SEND)
	    {
		Channel->FlowControlState = PAUSE_SEND;
		IODBDMAPause(dmaInfo->dmaBase);				// Pause transfer
	    }
	}
	else						// cts is high
	    if (Channel->FlowControlState != CONTINUE_SEND)	    // cts is high, but were paused
	    {
		Channel->FlowControlState = CONTINUE_SEND;
		IODBDMAContinue(dmaInfo->dmaBase);				// Continue transfer
			
                // This code is important for the case when we have been paused
                // and finished the last Tx Transmission and in a high water situation
                // where we won't accept any more data. This kicks off another transfer
                
		if (!Channel->AreTransmitting && UsedSpaceinQueue(&(Channel->TX)))
		{
		    AbsoluteTime deadline;
		    clock_interval_to_deadline(1, 1, &deadline);
		    thread_call_enter_delayed(RS232->fdmaStartTransmissionThread, deadline);
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

void SccDisableInterrupts(SccChannel *Channel, UInt32 WhichInts)
{
    UInt8	sccState;
    
    ELG(0, WhichInts, "SccDisableInterrupts");

    if (Channel->ControlRegister)
    {
        switch ( WhichInts)
        {
            case kTxInterrupts:					// Turn off tx interrupts
                sccState = Channel->lastWR[5];
                SccWriteReg(Channel, R5, sccState & ~kTxEnable);
                break;
            
            case kRxInterrupts:					// Turn off rx interrupts
                sccState = Channel->lastWR[3];
                SccWriteReg(Channel, R3, sccState & ~kRxEnable);
                break;
            
            case kSccInterrupts:				// Turn off the scc interrupt processing
                sccState = Channel->lastWR[9];
                SccWriteReg(Channel, R9, sccState & ~kMIE & ~kNV);
                break;
            
            default:
                break;
        }
    }
    
    return;
    
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

void SccEnableInterrupts(SccChannel *Channel, UInt32 WhichInts)
{

    ELG(0, WhichInts, "SccEnableInterrupts");

    switch (WhichInts)
    {
        case kTxInterrupts:					// Turn on tx interrupts
            SccWriteReg(Channel, R5, Channel->lastWR[5] | kTxEnable);
            break;
            
        case kRxInterrupts:					// Turn on rx interrupts
            SccWriteReg(Channel, R3, Channel->lastWR[3] | kRxEnable);
            SccWriteReg(Channel, R0, kResetRxInt);
            break;
            
        case kSccInterrupts:					// Turn on Scc interrupts
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
//		Outputs:	true if worked
//
//		Desc:		Set up the DMA registers.
//
/****************************************************************************************************/

bool SccSetDMARegisters(SccChannel *Channel, IOService *provider)
{
    UInt32	firstDMAMap = 1;
    IOMemoryMap	*map;
    
    ELG(0, 0, "SccSetDMARegisters");

    Channel->TxDBDMAChannel.dmaBase = NULL;
    Channel->rxDBDMAChannels[0].dmaBase = NULL;
    Channel->rxDBDMAChannels[1].dmaBase = NULL;
    
    for(firstDMAMap = 1; ; firstDMAMap++)
    {
        map = provider->mapDeviceMemoryWithIndex(firstDMAMap);
        if (!map)
            return false;

        if (map->getLength() > 1)
            break;
    }

    Channel->TxDBDMAChannel.dmaBase = (IODBDMAChannelRegisters*)map->getVirtualAddress();
    
    map = provider->mapDeviceMemoryWithIndex(firstDMAMap + 1);
    if (!map)
        return false;
        
    Channel->rxDBDMAChannels[0].dmaBase = (IODBDMAChannelRegisters*)map->getVirtualAddress();
    Channel->rxDBDMAChannels[1].dmaBase = Channel->rxDBDMAChannels[0].dmaBase;
    
    return true;
    
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

    SccEnableInterrupts(Channel, kRxInterrupts);

        // Remind the receiver to call when a new character enters the fifo
        
    SccWriteReg(Channel, R0, kResetRxInt);

        // Also when one of the status bits changes
    
    SccWriteReg(Channel, R0, kResetExtStsInt);
    
}/* end SccEnableDMAInterruptSources */

/****************************************************************************************************/
//
//		Function:	SccSetupReceptionChannel
//
//		Inputs:		Channel - The port, index - which rx channel
//
//		Outputs:	
//
//		Desc:		Set up the RX DMA channel.
//
/****************************************************************************************************/

void SccSetupReceptionChannel(SccChannel *Channel, UInt32 index)
{
    SerialDBDMAStatusInfo *dmaInfo;
    
    ELG(0, index, "SccSetupReceptionChannel");
    
    dmaInfo = &Channel->rxDBDMAChannels[index];

        // Just in case
        
    IODBDMAReset(dmaInfo->dmaBase);
    
        // Again just in case
        
    IODBDMAReset(dmaInfo->dmaBase);
    
}/* end SccSetupReceptionChannel */

/****************************************************************************************************/
//
//		Function:	SccFreeReceptionChannel
//
//		Inputs:		Channel - The port, index - which rx channel
//
//		Outputs:	
//
//		Desc:		Free the RX DMA channel.
//
/****************************************************************************************************/

void SccFreeReceptionChannel(SccChannel *Channel, UInt32 index)
{
    SerialDBDMAStatusInfo *dmaInfo;
    
    ELG(0, 0, "SccFreeupReceptionChannel");
    
    if (Channel == NULL)
        return;
    
    SccdbdmaEndReception(Channel, index);

    dmaInfo = &Channel->rxDBDMAChannels[index];


    if (dmaInfo->dmaTransferBufferMDP != NULL)
    {
	dmaInfo->dmaTransferBufferMDP->complete();
	dmaInfo->dmaTransferBufferMDP->release();
	dmaInfo->dmaTransferBufferMDP = NULL;
    }

    dmaInfo->dmaNumberOfDescriptors = 0;
    dmaInfo->dmaChannelCommandArea = NULL;
    dmaInfo->dmaTransferBuffer = NULL;
        
    
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

void SccdbdmaDefineReceptionCommands(SccChannel *Channel, UInt32 index, bool firstReadInterrupts)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    IOPhysicalAddress		physaddr;
    IOByteCount			temp;
    IODBDMADescriptor		*cmds;
    const int main_read_size =	kRxDBDMABufferSize  -1 ;	// all but the first byte for the main read
    
    ELG(0, 0, "SccdbdmaDefineReceptionCommands");
    
    /***
     kRxDBDMACmd_First_Read,		// a single byte read to generate an interrupt
     kRxDBDMACmd_Main_Read,		// then a big read for most of the buffer
     kRxDBDMACmd_Stop,			// finally a stop
    ***/
    
    dmaInfo = &Channel->rxDBDMAChannels[index];
    cmds = dmaInfo->dmaChannelCommandArea;

    physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(0, &temp);
    if (temp < kRxDBDMABufferSize) {
	ELG(temp, kRxDBDMABufferSize, "SccdbdmaDefineReceptionCommands - read buffer not contiguous");
	return;
    }

    // check the legality of the transmission
    if ((cmds == NULL) || (dmaInfo->dmaTransferBuffer == NULL))
	return;
        
    // first read, needs to generate an interrupt if requested
	    
    IOMakeDBDMADescriptor(
	&cmds[kRxDBDMACmd_First_Read],
	kdbdmaInputMore,
	kdbdmaKeyStream0,
	firstReadInterrupts ? kdbdmaIntAlways : kdbdmaIntNever,
	kdbdmaBranchNever, kdbdmaWaitNever,
	1,
	physaddr);
    physaddr += 1;

    // main read, no interrupt
    
    IOMakeDBDMADescriptor(
	&cmds[kRxDBDMACmd_Main_Read],
	kdbdmaInputMore,
	kdbdmaKeyStream0,
	kdbdmaIntNever, kdbdmaBranchNever, kdbdmaWaitNever,
	main_read_size,
	physaddr);
    //physaddr += final_read_size;		    // physaddr unused after this
    
    // and a stop for sanity
    IOMakeDBDMADescriptor(
	&cmds[kRxDBDMACmd_Stop],
	kdbdmaStop,
	kdbdmaKeyStream0,
	kdbdmaIntNever, kdbdmaBranchNever, kdbdmaWaitNever,
	0,
	0);
    
}/* end SccdbdmaDefineReceptionCommands */

/****************************************************************************************************/
//
//		Function:	SccdbdmaStartReception
//
//		Inputs:		Channel - The port, index - which channel
//
//		Outputs:	
//
//		Desc:		Start the RX DMA channel.
//
/****************************************************************************************************/

void SccdbdmaStartReception(SccChannel *Channel, UInt32 index, bool firstReadInterrupts)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    
    ELG(0, 0, "SccdbdmaStartReception");
    
    dmaInfo = &Channel->rxDBDMAChannels[index];

    // reset the dma channel
        
    IODBDMAReset(dmaInfo->dmaBase);
    IODBDMAReset(dmaInfo->dmaBase);
    
    // check the legality of the reception
            
    if ((dmaInfo->dmaChannelCommandArea == NULL) || (dmaInfo->dmaTransferBuffer == NULL))
        return;
    
    // this could be done more surgically, but "for now" it should be fast enough
    // to just reprogram.  that way we get current R5 contents as well as the proper
    // interrupt (or lack of same) on the first read byte
    SccdbdmaDefineReceptionCommands(Channel, index, firstReadInterrupts);
    
    flush_dcache((vm_offset_t)dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors, false);
       
        // Enables the receiver and starts
        
    SccEnableInterrupts(Channel, kRxInterrupts);
    IODBDMAStart(dmaInfo->dmaBase, dmaInfo->dmaChannelCommandAreaPhysical);
    
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
#if 0	    // jdg - unused
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
#endif // 0
/****************************************************************************************************/
//
//		Function:	CommandStatus
//
//		Inputs:		dmaInfo - the dbdma info block
//				commandNumber - The command in question
//
//		Outputs:	Return value - DMA error (or not)
//
//		Desc:		Get the status of the specified command.
//
/****************************************************************************************************/

UInt32 CommandStatus(SerialDBDMAStatusInfo *dmaInfo, UInt32 commandNumber)
{
    UInt32 status = IOGetCCResult(&dmaInfo->dmaChannelCommandArea[commandNumber]);

    ELG(commandNumber, status, "CommandStatus, commandNumber, status");
        
    return status;
    
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

void SccdbdmaRxHandleCurrentPosition(SccChannel *Channel, UInt32 index)
{
    AppleRS232Serial		*RS232;
    SerialDBDMAStatusInfo	*dmaInfo;
    bool			gotData;
    natural_t			numberOfbytes;
    natural_t			bitInterval;
    natural_t			nsec;
    UInt8			savedR1Reg;
    
    ELG(0, 0, "SccdbdmaRxHandleCurrentPosition");
    
    if (Channel == NULL)
        return;
        
    RS232 = Channel->RS232;
    ELG(RS232, RS232->fWorkLoop->inGate(), "SccdbdmaRxHandleCurrentPosition - obj, inGate");
    
    dmaInfo = &Channel->rxDBDMAChannels[index];
                
        // Sanity check
        
    if ((dmaInfo->dmaChannelCommandArea == NULL) || (dmaInfo->dmaTransferBuffer == NULL))
    {
	ELG(0, 0, "SccdbdmaRxHandleCurrentPosition - failed sanity check");
        return;
    }

        // Checks for errors in the SCC since they have the nasty habit of blocking the FIFO
        
    SccHandleExtErrors(Channel);
    
    // we're always pausing the channel in this design

    IODBDMAPause(dmaInfo->dmaBase);						 // Stops the reception
    savedR1Reg = Channel->lastWR[1];
    SccWriteReg(Channel, R1, savedR1Reg & ~kWReqEnable);
    IOSetDBDMAChannelControl(dmaInfo->dmaBase, IOClearDBDMAChannelControlBits(kdbdmaPause));
    
    IODBDMAFlush(dmaInfo->dmaBase);
    IOSetDBDMAChannelControl(dmaInfo->dmaBase, IOClearDBDMAChannelControlBits(kdbdmaRun));
    
    while(IOGetDBDMAChannelStatus(dmaInfo->dmaBase) & (kdbdmaActive))	    // wait for active bit to go low
    {
	OSSynchronizeIO();		// eieio()
    }
    SccWriteReg(Channel, R1, savedR1Reg);
    //*** pause completed, data is ready to pick up
    
    // flip the channel index over to the other one.  0 --> 1, 1--> 0
    Channel->activeRxChannelIndex = 1 - Channel->activeRxChannelIndex;		// flip between 0 and 1

    // channel is paused, just quickly see if we have any data to extract before we start other channel
    gotData = AnyDataReceived(dmaInfo);
    
    // if have data, then we don't want an interrupt (we'll come back here via a timer instead)
    // if no data now, we'll wait for an interrupt when the first byte read completes
    SccdbdmaStartReception(Channel, Channel->activeRxChannelIndex, !gotData);	// start other rx channel, w/ or w/out interrupt    
    
    // as of now, we have the other channel running so we can take our time sucking up data out of this dbdma chain
    if (gotData) {								// if we know there's data to be extracted
	SuckDataFromTheDBDMAChain(Channel, index);				// pull out the data (out of now offline channel)
        RS232->CheckQueues(Channel);						// Clearly our buffer is no longer Empty

	// Reschedule for a later time
	// Checking after a third of the buffer is full seems reasonable
            
        if (Channel->DataLatInterval.tv_nsec == 0)	    // but only if client didn't specify their own timeout
        {
	    // the old driver had a buffer size of a page, but limited reads to less than that due to
	    // the dbdma chain of 1-byte per cmd also having to fit in a page.  The 1/3 buffer full
	    // timeout was based on this size; with the newer, bigger buffer, 1/3 of the buffer is
	    // a much longer value.  Use smaller value to keep the response the same.
	    UInt32 historical_buffer_size = PAGE_SIZE / sizeof(IODBDMADescriptor);
	    numberOfbytes = (min(kRxDBDMABufferSize, historical_buffer_size)) / 3;
            bitInterval = (1000000 * 10)/Channel->BaudRate;        		// bit interval in \xb5Sec
            nsec = bitInterval * 1000 * numberOfbytes;     			// nSec*bits
        } else {
            nsec = Channel->DataLatInterval.tv_nsec;
        }
        
        rearmRxTimer(Channel, nsec);
    }
    // else if we have no data, there's nothing to do now but wait for an interrupt on 1st byte read
    
}/* end SccdbdmaRxHandleCurrentPosition */

/****************************************************************************************************/
//
//		Function:	AnyDataReceived
//
//		Inputs:		dmaInfo - the dbdma info block
//
//		Outputs:	
//
//		Desc:		returns true iff the first one-byte read command completed
//
/****************************************************************************************************/

bool AnyDataReceived(SerialDBDMAStatusInfo *dmaInfo)
{
    const UInt32 kDataIsValid = (kdbdmaStatusRun | kdbdmaStatusActive);
    if ((CommandStatus(dmaInfo, kRxDBDMACmd_First_Read) & kDataIsValid) == kDataIsValid) {
	return true;
    }
    return false;
}

/****************************************************************************************************/
//
//		Function:	DescriptorBytesRead
//
//		Inputs:		cmd - the dbdma command to examine
//
//		Outputs:	returns number of bytes read, or zero if command unexecuted
//
//		Desc:		
//
/****************************************************************************************************/

UInt32  DescriptorBytesRead(IODBDMADescriptor *cmd)
{
    UInt32 cmdRequest;		// amount of original request
    UInt32 cmdResult;		// count from result field, bytes remaining
    UInt32 count;
    UInt32 status;
    const UInt32 kDataIsValid = kdbdmaStatusActive;

    status = IOGetCCResult(cmd);		// get cmd status to see if it executed at all
    ELG(cmd, status, "DescriptorBytesRead, cmd, status");
    
    if ((status & kDataIsValid) != kDataIsValid) {
	return 0;				// if it didn't run, zero
    }
    
    // get original request count and subtract count remaining to get xfer'd count
    cmdRequest = IOGetDBDMADescriptor(cmd, operation) & kdbdmaReqCountMask;      // amount of original request
    cmdResult  = IOGetDBDMADescriptor(cmd, result)    & kdbdmaResCountMask;      // amount remaining

    ELG(cmdRequest, cmdResult, "DescriptorBytesRead, request, remaining");

    count = cmdRequest - cmdResult;         // compute count of bytes transferred
    ELG(cmd, count, "DescriptorBytesRead, cmd, count");
    
    return count;
}


/****************************************************************************************************/
//
//		Function:	SuckDataFromTheDBDMAChain
//
//		Inputs:		Channel - The port
//
//		Outputs:	data put into the rx queue
//
//		Desc:		Gets data from the RX DMA channel.
//
/****************************************************************************************************/

void SuckDataFromTheDBDMAChain(SccChannel *Channel, UInt32 index)
{   
    AppleRS232Serial		*RS232;
    UInt32			SW_FlowControl;
    SerialDBDMAStatusInfo 	*TxdmaInfo;
    SerialDBDMAStatusInfo 	*dmaInfo;
    UInt8			byteRead;
    UInt32			excess = 0;
    UInt32			byteCount = 0;	    // number of bytes read
    UInt8			*bytes;
    
    ELG(0, 0, "SuckDataFromTheDBDMAChain");
    
    RS232 = Channel->RS232;

    SW_FlowControl = Channel->FlowControl & PD_RS232_S_TXO;	    // xon/xoff to control tx?

    dmaInfo = &Channel->rxDBDMAChannels[index];
    TxdmaInfo = &Channel->TxDBDMAChannel;
    
    // we're here because the first 1-byte read finished
    // add in byte counts from main read and final read (if they did anything)
    byteCount = 1;
    byteCount += DescriptorBytesRead(&dmaInfo->dmaChannelCommandArea[kRxDBDMACmd_Main_Read]);

    bytes = dmaInfo->dmaTransferBuffer;			// start sucking up the data
    while (byteCount--)
    {

	// Reset the count for the current command
        //IOSetCCResult(&dmaInfo->dmaChannelCommandArea[iterator], 0);
        //OSSynchronizeIO();		    // eieio()

            
	byteRead = *bytes++;				// grab the read byte
	ELG(byteCount, byteRead, "SuckDataFromTheDBDMAChain - byteCount, byteRead");

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
                }
            } else {
                if (excess <= 0)
                {
                    Channel->aboveRxHighWater = false;
                }
            }
        }
    }
        
    return;
        
}/* end SuckDataFromTheDBDMAChain */

/****************************************************************************************************/
//
//		Function:	SccdbdmaEndReception
//
//		Inputs:		Channel - The port, index - which rx channel
//
//		Outputs:	
//
//		Desc:		End receiving on the RX DMA channel.
//
/****************************************************************************************************/

void SccdbdmaEndReception(SccChannel *Channel, UInt32 index)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    //UInt32			cmdResult = 0;
    
    ELG(0, index, "SccdbdmaEndReception");
    
    dmaInfo = &Channel->rxDBDMAChannels[index];
    
    if (dmaInfo->dmaBase)
    {
        IODBDMAStop(dmaInfo->dmaBase);					// Stop transfer
        IODBDMAReset(dmaInfo->dmaBase);					// reset transfer
    }
    
    /*** nobody cares about data rx'd at this point (except maybe for debugging)
    if (dmaInfo->dmaChannelCommandArea != NULL)
    {
        cmdResult = IOGetDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], result);
        dmaInfo->dmaTransferSize = dmaInfo->dmaTransferSize - (cmdResult & 0xFFFF);
        ELG(0, dmaInfo->dmaTransferSize, "SccdbdmaEndReception - number of bytes received");
    }
    ***/
    
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
        
    SccdbdmaEndTransmission(Channel);

    dmaInfo = &Channel->TxDBDMAChannel;

    if (dmaInfo->dmaTransferBufferMDP != NULL)
    {
	dmaInfo->dmaTransferBufferMDP->complete();
	dmaInfo->dmaTransferBufferMDP->release();
	dmaInfo->dmaTransferBufferMDP = NULL;
    }

    dmaInfo->dmaNumberOfDescriptors = 0;
    dmaInfo->dmaChannelCommandArea = NULL;
    dmaInfo->dmaTransferBuffer = NULL;
    
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

    IOByteCount	temp;
    physaddr = dmaInfo->dmaTransferBufferMDP->getPhysicalSegment(0, &temp);
    IOMakeDBDMADescriptor(&dmaInfo->dmaChannelCommandArea[0], kdbdmaOutputLast, kdbdmaKeyStream0, kdbdmaIntAlways, kdbdmaBranchNever, kdbdmaWaitNever, 0,
                                                                                                                                                    physaddr);                                                                                                                                                    
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
    UInt8			*localBuffer;
    UInt32			sizeReadFromBuffer = 0;
    UInt8			*bufferPtr = 0;
    UInt32			bytesToTransfer;
    bool			wrapped = false;
    
    ELG(0, 0, "SccdbdmaStartTransmission");
    
    if (Channel == NULL)
        return;
        
    RS232 = Channel->RS232;
        
    dmaInfo = &Channel->TxDBDMAChannel;

        // Check for errors in the SCC since they have the nasty habit of blocking the FIFO
        
    SccHandleExtErrors(Channel);

        // Set up everything as we are running, handle the situation where it may get called twice
        
    Channel->AreTransmitting = TRUE;
    RS232->setStateGated(PD_S_TX_BUSY, PD_S_TX_BUSY);
    
        // check the legality of the transmission
        
    if ((dmaInfo->dmaChannelCommandArea != NULL) && (dmaInfo->dmaTransferBuffer != NULL))
    {
        IODBDMAReset(dmaInfo->dmaBase);					// Reset the channel
        
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
                if (sizeReadFromBuffer > kTxDBDMABufferSize)
                    sizeReadFromBuffer = kTxDBDMABufferSize;
                    
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
            OSSynchronizeIO();	    // eieio()

            flush_dcache((vm_offset_t)dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors, false);

            IODBDMAStart(dmaInfo->dmaBase, dmaInfo->dmaChannelCommandAreaPhysical);    // Starts the transmission

            EndDirectReadFromQueue(&(Channel->TX), dmaInfo->dmaTransferSize);
            
                // We just removed a bunch of stuff from the queue, so see if we can free some threads
                // to enqueue more stuff.
                
            RS232->CheckQueues(Channel);

            return;
        } else {
            EndDirectReadFromQueue(&(Channel->TX), 0);
        }
    }

        // Updates all the status flags
        
    RS232->CheckQueues(Channel);
    Channel->AreTransmitting = FALSE;
    RS232->setStateGated(0, PD_S_TX_BUSY);
    
    
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
    
    ELG(0, 0, "HandleRxIntTimeout");
    
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
    
    ELG(0, 0, "rxTimeoutHandler");
	
	// Make sure it's me
        
    serialPortPtr = OSDynamicCast(AppleRS232Serial, owner);
    if (serialPortPtr)
    {
	if (serialPortPtr->fCurrentPowerState)		// if not sleeping
	    SccdbdmaRxHandleCurrentPosition(&serialPortPtr->fPort, serialPortPtr->fPort.activeRxChannelIndex);
    }
    
}/* end rxTimeoutHandler */

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

    ELG(Channel, timerDelay, "rearmRxTimer, channel, timerDelay");

    Channel->rxTimer->setTimeout(timerDelay);
    
}/* end rearmRxTimer */

