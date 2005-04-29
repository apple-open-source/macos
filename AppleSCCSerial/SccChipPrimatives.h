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
 * chipPrimatives.h
 *
 * MacOSX implementation of Serial Port driver
 *
 *
 * Humphrey Looney	MacOSX IOKit ppc
 * Elias Keshishoglou 	MacOSX Server ppc
 * Dean Reece		Original Next Intel  version
 *
 * Copyright ©: 	1999 Apple Computer, Inc.  all rights reserved.
 */

#ifndef scc_chip_primatives_h
#define scc_chip_primatives_h

#include "Z85C30.h"
#include "PPCSerialPort.h"
#include "SccQueuePrimatives.h"

#define MIN_BAUD (50 <<1)

enum {
    R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15
};

/* programChip() - This function programs the UART according to the
 * various variables in the port struct.  This function can be called
 * from any context is the primary method of accessing the hardware,
 * other than the AppleSCCSerialIntHandler() function.
 */
void 	programChip(PortInfo_t *port);
/* initChip() - This function sets up various default values for UART
 * parameters, then calls programChip().  It is intended to be called
 * from the init or acquire methods only.
 */
void 	initChip(PortInfo_t *port);
void	FixZeroBug(SccChannel *Channel);
bool	ProbeSccDevice( PortInfo_t *Port);
bool 	OpenScc(PortInfo_t *Port);
void 	SccCloseChannel(SccChannel *Channel);
bool	SccSetBaud(SccChannel *Channel, UInt32 NewBaud);
bool 	SccConfigureForMIDI(SccChannel *Channel, UInt32 ClockMode);

void 	SccChannelReset(SccChannel *Channel);
bool 	SccWriteByte(SccChannel *Channel, UInt8 Value);
UInt8	SccReadReg(SccChannel *Channel, UInt8 sccRegister);
bool	SccWriteData(SccChannel *Channel, UInt8 Data);
UInt8	SccReadData(SccChannel *Channel);
void 	SccWriteIntSafe(SccChannel *Channel, UInt8 sccRegister, UInt8 control );
bool	SccWriteReg(SccChannel *Channel, UInt8 sccRegister, UInt8 Value);
UInt8 	SccReadByte(SccChannel *Channel);
bool 	SccSetParity(SccChannel *Channel, ParityType ParitySetting);
bool 	SccSetStopBits(SccChannel *Channel, UInt32 otSymbolic );
void 	SendNextChar(SccChannel	*Channel);
bool 	SccSetDataBits(SccChannel *Channel, UInt32 numDataBits );
bool	SetUpTransmit(SccChannel *Channel);
bool	SuspendTX(SccChannel *Channel);
bool 	SccGetCTS( SccChannel *Channel );
bool 	SccGetDCD( SccChannel *Channel );
void 	SccSetCTSFlowControlEnable(SccChannel *Channel, bool enableCTS );

/* added 26/11/99 */
void	SccSetDTR(SccChannel *Channel, bool assertDTR );

/* Interrupt Service Routine */
void 	PPCSerialISR(OSObject *identity, void *istate, SccChannel *Channel);
void 	PPCSerialTxDMAISR(void *identity, void *istate, SccChannel *Channel);
void 	PPCSerialRxDMAISR(void *identity, void *istate, SccChannel *Channel);
//bool 	SccHandleExtInterrupt(SccChannel *Channel);
bool 	SccHandleExtInterrupt(OSObject *target, void *refCon,SccChannel *Channel);
void 	SccHandleExtErrors(SccChannel *Channel);
void 	SccEnableInterrupts(SccChannel *Channel, UInt32 WhichInts , InterruptState previousState);
InterruptState SccDisableInterrupts(SccChannel *Channel, UInt32 WhichInts);

/* data transfer routine */
void SccSetDMARegisters(SccChannel *Channel, IOService *provider);
void SccEnableDMAInterruptSources(SccChannel *Channel, bool onOff);

/* RX channel */
void SccSetupReceptionChannel(SccChannel *Channel);
void SccFreeReceptionChannel(SccChannel *Channel);
void SccdbdmaDefineReceptionCommands(SccChannel *Channel);
void SccdbdmaStartReception(SccChannel *Channel);
void SccdbdmaEndReception(SccChannel *Channel);
void SccdbdmaRxHandleCurrentPosition(SccChannel *Channel);

/* TX channel */
void SccSetupTansmissionChannel(SccChannel *Channel);
void SccFreeTansmissionChannel(SccChannel *Channel);
void SccdbdmaDefineTansmissionCommands(SccChannel *Channel);
void SccdbdmaStartTransmission(SccChannel *Channel);
void SccdbdmaEndTransmission(SccChannel *Channel);

//Tiger Cleanup
void SccCurrentPositionDelayedHandler( thread_call_param_t arg, thread_call_param_t );
void SccStartTransmissionDelayedHandler ( thread_call_param_t self, thread_call_param_t );
void HandleRxIntTimeout(SccChannel *Channel);
void rxTimeoutHandler(OSObject *owner, IOTimerEventSource *sender);

#endif
