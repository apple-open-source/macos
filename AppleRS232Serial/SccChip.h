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

#ifndef __SCCCHIP__
#define __SCCCHIP__

            // Scc General Routines

    void 	initChip(PortInfo_t *port);
    void	FixZeroBug(SccChannel *Channel);
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
    void	SccSetBreak(SccChannel *Channel, bool setBreak);

        // Scc Interrupt Service Routines
        
    void 	PPCSerialISR(OSObject *identity, void *istate, SccChannel *Channel);
    void 	PPCSerialTxDMAISR(void *identity, void *istate, SccChannel *Channel);
    void 	PPCSerialRxDMAISR(void *identity, void *istate, SccChannel *Channel);
    void 	SccHandleExtInterrupt(OSObject *target, void *refCon, SccChannel *Channel);
    void 	SccHandleExtErrors(SccChannel *Channel);
    void 	SccEnableInterrupts(SccChannel *Channel, UInt32 WhichInts , UInt8 previousState);
    UInt8	SccDisableInterrupts(SccChannel *Channel, UInt32 WhichInts);

        // SCC Data transfer Routines - General
        
    void	SccSetDMARegisters(SccChannel *Channel, IOService *provider);
    void	SccEnableDMAInterruptSources(SccChannel *Channel, bool onOff);

	// SCC Data transfer Routines - RX channel
        
    void 	SccSetupReceptionChannel(SccChannel *Channel);
    void 	SccFreeReceptionChannel(SccChannel *Channel);
    void 	SccdbdmaDefineReceptionCommands(SccChannel *Channel);
    void 	SccdbdmaStartReception(SccChannel *Channel);
    void 	SccdbdmaEndReception(SccChannel *Channel);
    void 	SccdbdmaRxHandleCurrentPosition(SccChannel *Channel);

	// SCC Data transfer Routines - TX channel
        
    void	SccSetupTansmissionChannel(SccChannel *Channel);
    void 	SccFreeTansmissionChannel(SccChannel *Channel);
    void 	SccdbdmaDefineTansmissionCommands(SccChannel *Channel);
    void 	SccdbdmaStartTransmission(SccChannel *Channel);
    void 	SccdbdmaEndTransmission(SccChannel *Channel);

        // SCC Timeout Routines

    void 	HandleRxIntTimeout(SccChannel *Channel);
    void 	rxTimeoutHandler(OSObject *owner, IOTimerEventSource *sender);
    
#endif
