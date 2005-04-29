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

#ifndef __SCCCHIP__
#define __SCCCHIP__

            // Scc General Routines

    void 	initChip(PortInfo_t *port);
    //void	FixZeroBug(SccChannel *Channel);
    void	ProbeSccDevice(PortInfo_t *Port);
    void 	OpenScc(PortInfo_t *Port);
    void 	SccCloseChannel(SccChannel *Channel);
    bool	SccSetBaud(SccChannel *Channel, UInt32 NewBaud);
    bool 	SccConfigureForMIDI(SccChannel *Channel, UInt32 ClockMode);
    void 	SccChannelReset(SccChannel *Channel);
    bool 	SccWriteByte(SccChannel *Channel, UInt8 Value);
    UInt8	SccReadReg(SccChannel *Channel, UInt8 sccRegister);
    bool	SccWriteData(SccChannel *Channel, UInt8 Data);
    UInt8	SccReadData(SccChannel *Channel);
    bool	SccWriteReg(SccChannel *Channel, UInt8 sccRegister, UInt8 Value);
    UInt8 	SccReadByte(SccChannel *Channel);
    bool 	SccSetParity(SccChannel *Channel, ParityType ParitySetting);
    bool 	SccSetStopBits(SccChannel *Channel, UInt32 numbits);
    bool 	SccSetDataBits(SccChannel *Channel, UInt32 numDataBits);
    bool	SetUpTransmit(SccChannel *Channel);
    bool	SuspendTX(SccChannel *Channel);
    bool 	SccGetCTS(SccChannel *Channel);
    bool 	SccGetDCD(SccChannel *Channel);
    void 	SccSetCTSFlowControlEnable(SccChannel *Channel, bool enableCTS);
    void	SccSetDTR(SccChannel *Channel, bool assertDTR);
    void	SccSetRTS(SccChannel *Channel, bool assertRTS);
    void	SccSetBreak(SccChannel *Channel, bool setBreak);

        // Scc Interrupt Service Routines
        
    void 	PPCSerialISR(OSObject *identity, void *istate, SccChannel *Channel);
    void 	PPCSerialTxDMAISR(void *identity, void *istate, SccChannel *Channel);
    void 	PPCSerialRxDMAISR(void *identity, void *istate, SccChannel *Channel);
    void 	SccHandleExtInterrupt(OSObject *target, void *refCon, SccChannel *Channel);
    void 	SccHandleExtErrors(SccChannel *Channel);
    void 	SccEnableInterrupts(SccChannel *Channel, UInt32 WhichInts);
    void	SccDisableInterrupts(SccChannel *Channel, UInt32 WhichInts);

        // SCC Data transfer Routines - General
        
    bool	SccSetDMARegisters(SccChannel *Channel, IOService *provider);
    void	SccEnableDMAInterruptSources(SccChannel *Channel, bool onOff);

	// SCC Data transfer Routines - RX channel
        
    void 	SccSetupReceptionChannel(SccChannel *Channel, UInt32 index);
    void 	SccFreeReceptionChannel(SccChannel *Channel, UInt32 index);
    void 	SccdbdmaDefineReceptionCommands(SccChannel *Channel, UInt32 index, bool firstReadInterrupts);
    void 	SccdbdmaStartReception(SccChannel *Channel, UInt32 index, bool firstReadInterrupts);
    void 	SccdbdmaEndReception(SccChannel *Channel, UInt32 index);
    void 	SccdbdmaRxHandleCurrentPosition(SccChannel *Channel, UInt32 index);

	// SCC Data transfer Routines - TX channel
        
    void	SccSetupTansmissionChannel(SccChannel *Channel);
    void 	SccFreeTansmissionChannel(SccChannel *Channel);
    void 	SccdbdmaDefineTansmissionCommands(SccChannel *Channel);
    void 	SccdbdmaStartTransmission(SccChannel *Channel);
    void 	SccdbdmaEndTransmission(SccChannel *Channel);

        // SCC Timeout Routines

    void	SccCurrentPositionDelayedHandler( thread_call_param_t arg, thread_call_param_t );
    void	SccStartTransmissionDelayedHandler ( thread_call_param_t self, thread_call_param_t );
    void 	HandleRxIntTimeout(SccChannel *Channel);
    void 	rxTimeoutHandler(OSObject *owner, IOTimerEventSource *sender);
    
#endif
