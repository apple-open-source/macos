
    /* Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

    /* AppleSCCIrDA.cpp - MacOSX implementation of SCC IrDA Port Driver. */

#include <machine/limits.h>         /* UINT_MAX */
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/IOModemSerialStreamSync.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>

#include "AppleSCCIrDA.h"
#include "IrDAComm.h"
#include "IrDAUser.h"
#include "IrDALog.h"
#include "IrDADebugging.h"

static IrDAglobals  g;  /**** Instantiate the globals ****/

#ifdef Trace_SCCSerial              // running w/debug AppleSCCSerial driver
#define IRDLOG_HACK (PD_DATA_LONG | PD_OP(100)) // send irdalog to SCCSerial driver
#endif

#if (hasTracing > 0 && hasAppleSCCIrDATracing > 0)

enum tracecodes
{
    kLoginit = 1,
	kLogprob,
	kLogatch,
	kLogdtch,
    kLogstrt,
	kLogfree,
    kLogstop,
	
	kLogAdRB,
	
	kLogSbof,
	
	kLogStSp,
	kLogSSpminus,
	kLogSSsplus,
	
	kLogGIrS,
	
	kLogSICS,
		
	kLogCIrS,
	
	kLogSIrS,
	kLogstcn,
	kLogstrminus,
	kLogstbr,
	kLogstar,
	
	kLogeSCC,
	kLogeSCH,
	kLogeSCminus,
	kLogeSCK,
	
	kLogdSCC,
	kLogdSCH,
	kLogdSCminus,
	kLogdSCK,
	
	kLogsnBd,
	kLogsnBminus,

	kLogsnCd,
	kLogsCeminus,
	kLogsCtC,
	kLogsCnminus,
	kLogsCnplus,
	kLogsCdminus,
	
	kLoggFWV,
	kLoggFdplus,
	
	kLogrDTR,
	
	kLogklIO,
	kLogklRd,
	kLogklTd,
	
	kLogcrRX,
	kLogcREminus,
	kLogcrRplus,
	kLogcrRminus,
	
	kLogcrTX,
	kLogcTEminus,
	kLogcrTplus,
	kLogcrTminus,
	
	kLogsuDv,
	kLogsDpminus,
	kLogsDeminus,
	kLogsDdminus,
	kLogsDiminus,
	kLogsDsminus,
	kLogsDaminus,
	
	kLogcDev,
	kLogcDiminus,
	kLogcDominus,
	kLogcDuminus,
	kLogcDaminus,
	kLogcDIminus,
	kLogcDFminus,
	kLogcDvplus,
	kLogcDvminus,
	
	kLogcSSt,
	
	kLogacqP,
	kLogacqS,
	kLogacqplus,
	kLogacqminus,
	
	kLogrelP,
	
	kLogstSt,
	kLogstSm,
	kLogstsa,
	kLogstma,
	
	kLoggtSt,
	
	kLogexEv,
	kLogee01,
	kLogee02,
	kLogee03,
	kLogee04,
	kLogee05,
	kLogee06,
	kLogee07,
	kLogee08,
	kLogee09,
	kLogee10,
	kLogee11,
	kLogee12,
	kLogee13,
	kLogee14,
	kLogee15,
	kLogee16,
	kLogee17,
	kLogee18,
	kLogee19,
	kLogee20,
	kLogee21,
	kLogee22,
	kLogee23,
	kLogee24,
	kLogee25,
	kLogee26,
	kLogeexminus,
	
	kLogreEv,
	kLogre01,
	kLogre02,
	kLogre03,
	kLogre04,
	kLogre05,
	kLogre06,
	kLogre07,
	kLogre08,
	kLogre09,
	kLogre10,
	kLogre11,
	kLogre12,
	kLogre13,
	kLogre14,
	kLogre15,
	kLogre16,
	kLogre17,
	kLogre18,
	kLogre19,
	kLogre20,
	kLogre21,
	kLogre22,
	kLogre23,
	kLogre24,
	kLogreeminus,
	
	kLogeqDt,
	kLogeDWminus,
	kLogeDWplus,
	kLogeDfminus,
	kLogeDdminus,
	
	kLogdqDt,
	kLogdDpminus,
	kLogdqRt,
	kLogdqDplus,
	
	kLogSUTm,
	kLogSIWminus,
	kLogSIWplus,
	
	kLogstTx,
	kLogStTn,
	
	kLogfrRB,
	
	kLogalRB,
	
	kLogmess,
	kLogms01,
	kLogms02,
	kLogms03,
	kLogms04,
	kLogms05,
	kLogms06,
	kLogmesminus,
	
	kLogpIpB,
	kLogpImminus,
	
	kLogpISR,
	kLogpStI,
	kLogpStB,
	kLogpStd,
	kLogpSIminus,
	kLogpSCminus,
	kLogpSEminus,
	kLogpStp,
	kLogpSte,
	
	kLogpIFR,
	
	kLogpIRt,
	
	kLogPBFT,
	
	kLogrxLp,
	kLogrxok,
	kLogrxDminus,
	kLogrxLminus,
	kLogrxKl,
	
	kLogtxLp,
	kLogtxRs,
	kLogtxDt,
	kLogtxSE,
	kLogtxLminus,
	kLogtxKl,
	
	kLogXmitCount,
	kLogSpeedChangeThread,
	
	kLogTxThread,
	kLogTxThreadDeferred,
	kLogTxThreadResumed,
	
	kLogRxThread,
	kLogRxThreadResumed,
	kLogRxThreadSleeping,
	kLogRxThreadGotData,
	
	kLogSetStructureDefaults,
	kLogAcquirePort,
	kLogReleasePort,
	
	kLogSetState1,
	kLogSetState2,
	kLogGetState,
	
	kLogWatchState,
	kLogWatchState1,
	kLogWatchState2,
	
	kLogExecEvent,
	kLogExecEventData,
	
	kLogReqEvent,
	kLogReqEventData,
	
	kLogChangeStateBits,
	kLogChangeStateMask,
	kLogChangeStateDelta,
	kLogChangeStateNew,
	
	kLogInitForPM,
	kLogInitialPowerState,
	kLogSetPowerState
};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kLoginit,          "AppleSCCIrDA: init"},
	{kLogprob,          "AppleSCCIrDA: probe"},
	{kLogatch,          "AppleSCCIrDA: attach"},
	{kLogdtch,          "AppleSCCIrDA: detach"},
    {kLogstrt,          "AppleSCCIrDA: start"},
	{kLogfree,          "AppleSCCIrDA: free"},
    {kLogstop,          "AppleSCCIrDA: stop"},
	
	{kLogAdRB,          "AppleSCCIrDA: Add_RXBytes"},
	
	{kLogSbof,          "AppleSCCIrDA: SetBofCount"},
	
	{kLogStSp,          "AppleSCCIrDA: SetSpeed"},
	{kLogSSpminus,          "AppleSCCIrDA: SetSpeed - Unsupported baud rate"},
	{kLogSSsplus,           "AppleSCCIrDA: SetSpeed - sendCommand successful"},
	
	{kLogGIrS,          "AppleSCCIrDA: GetIrDAStatus"},
	
	{kLogSICS,          "AppleSCCIrDA: SetIrDAUserClientState"},
	
	{kLogCIrS,          "AppleSCCIrDA: CheckIrDAState"},
	
	{kLogSIrS,          "AppleSCCIrDA: SetIrDAState"},
	{kLogstcn,          "AppleSCCIrDA: SetIrDAState - successful (IrDA On)"},
	{kLogstrminus,          "AppleSCCIrDA: SetIrDAState - configureDevice failed"},
	{kLogstbr,          "AppleSCCIrDA: SetIrDAState - before releasePort"},
	{kLogstar,          "AppleSCCIrDA: SetIrDAState - after releasePort"},
	
	{kLogeSCC,          "AppleSCCIrDA: enableSCC"},
	{kLogeSCH,          "AppleSCCIrDA: enableSCC - Heathrow"},
	{kLogeSCminus,          "AppleSCCIrDA: enableSCC - no Heathrow or Keylargo"},
	{kLogeSCK,          "AppleSCCIrDA: enableSCC - Keylargo"},
	
	{kLogdSCC,          "AppleSCCIrDA: disable SCC"},
	{kLogdSCH,          "AppleSCCIrDA: disable SCC - heathrow"},
	{kLogdSCminus,      "AppleSCCIrDA: disable SCC - no heathrow or keylargo"},
	{kLogdSCK,          "AppleSCCIrDA: disable SCC - keylargo"},
	
	{kLogsnBd,          "AppleSCCIrDA: sendBaud"},
	{kLogsnBminus,          "AppleSCCIrDA: sendBaud - execute event (baud rate) failed"},
	
	{kLogsnCd,          "AppleSCCIrDA: sendCommand"},
	{kLogsCeminus,          "AppleSCCIrDA: sendCommand - enqueue data failed"},
	{kLogsCtC,          "AppleSCCIrDA: sendCommand - dequeue data"},
	{kLogsCnminus,          "AppleSCCIrDA: sendCommand - dequeue data, no data returned"},
	{kLogsCnplus,           "AppleSCCIrDA: sendCommand - dequeue data, successful"},
	{kLogsCdminus,          "AppleSCCIrDA: sendCommand - dequeue data failed"},
	
	{kLoggFWV,          "AppleSCCIrDA: getFIReWorksVersion"},
	{kLoggFdplus,           "AppleSCCIrDA: getFIReWorksVersion - dequeue data (getversion) successful"},
	
	{kLogrDTR,          "AppleSCCIrDA: resetDTR"},
	
	{kLogklIO,          "AppleSCCIrDA: killIO"},
	{kLogklRd,          "AppleSCCIrDA: killIO - RX saved by deadman"},
	{kLogklTd,          "AppleSCCIrDA: killIO - TX saved by deadman"},
	
	{kLogcrRX,          "AppleSCCIrDA: createRX"},
	{kLogcREminus,          "AppleSCCIrDA: createRX - timerEventSource failed"},
	{kLogcrRplus,           "AppleSCCIrDA: createRX - successful"},
	{kLogcrRminus,          "AppleSCCIrDA: createRX - failed"},
	
	{kLogcrTX,          "AppleSCCIrDA: createTX"},
	{kLogcTEminus,          "AppleSCCIrDA: createTX - timerEventSource failed"},
	{kLogcrTplus,           "AppleSCCIrDA: createTX - successful"},
	{kLogcrTminus,          "AppleSCCIrDA: createTX - failed"},
	
	{kLogsuDv,          "AppleSCCIrDA: setupDevice"},
	{kLogsDpminus,          "AppleSCCIrDA: setupDevice - acquirePort failed"},
	{kLogsDeminus,          "AppleSCCIrDA: setupDevice - enableSCC failed"},
	{kLogsDdminus,          "AppleSCCIrDA: setupDevice - execute event (data size) failed"},
	{kLogsDiminus,          "AppleSCCIrDA: setupDevice - execute event (data integrity) failed"},
	{kLogsDsminus,          "AppleSCCIrDA: setupDevice - execute event (stop bits) failed"},
	{kLogsDaminus,          "AppleSCCIrDA: setupDevice - execute event (active) failed"},
	
	{kLogcDev,          "AppleSCCIrDA: configureDevice"},
	{kLogcDiminus,          "AppleSCCIrDA: configureDevice - input buffer allocation failed"},
	{kLogcDominus,          "AppleSCCIrDA: configureDevice - output buffer allocation failed"},
	{kLogcDuminus,          "AppleSCCIrDA: configureDevice - in use buffer allocation failed"},
	{kLogcDaminus,          "AppleSCCIrDA: configureDevice - allocate ring buffer failed"},
	{kLogcDIminus,          "AppleSCCIrDA: configureDevice - IrDA initialize failed"},
	{kLogcDFminus,          "AppleSCCIrDA: configureDevice - getFIReWorksVersion failed"},
	{kLogcDvplus,           "AppleSCCIrDA: configureDevice - successful"},
	{kLogcDvminus,          "AppleSCCIrDA: configureDevice - failed"},
	
	{kLogcSSt,          "AppleSCCIrDA: createSerialStream"},
	
	{kLogacqP,          "AppleSCCIrDA: acquirePort"},
	{kLogacqS,          "AppleSCCIrDA: acquirePort - returned kIOReturnExclusiveAccess"},
	{kLogacqplus,           "AppleSCCIrDA: acquirePort - successful"},
	{kLogacqminus,          "AppleSCCIrDA: acquirePort - failed"},
	
	{kLogrelP,          "AppleSCCIrDA: releasePort"},
	
	{kLogstSt,          "AppleSCCIrDA: setState - state="},
	{kLogstSm,          "AppleSCCIrDA: setState - mask="},
	{kLogstsa,          "AppleSCCIrDA: setState - after change, state="},
	{kLogstma,          "AppleSCCIrDA: setState - after change, mask="},
	
	{kLoggtSt,          "AppleSCCIrDA: getState - state="},
	
	{kLogexEv,          "AppleSCCIrDA: executeEvent"},
	{kLogee01,          "AppleSCCIrDA: executeEvent - PD_RS232_E_XON_BYTE, data="},
	{kLogee02,          "AppleSCCIrDA: executeEvent - PD_RS232_E_XOFF_BYTE, data="},
	{kLogee03,          "AppleSCCIrDA: executeEvent - PD_E_SPECIAL_BYTE, data="},
	{kLogee04,          "AppleSCCIrDA: executeEvent - PD_E_VALID_DATA_BYTE, data="},
	{kLogee05,          "AppleSCCIrDA: executeEvent - PD_E_FLOW_CONTROL, data="},
	{kLogee06,          "AppleSCCIrDA: executeEvent - PD_E_ACTIVE, data="},
	{kLogee07,          "AppleSCCIrDA: executeEvent - PD_E_DATA_LATENCY, data="},
	{kLogee08,          "AppleSCCIrDA: executeEvent - PD_RS232_E_MIN_LATENCY, data="},
	{kLogee09,          "AppleSCCIrDA: executeEvent - PD_E_DATA_INTEGRITY, data="},
	{kLogee10,          "AppleSCCIrDA: executeEvent - PD_E_DATA_RATE, data="},
	{kLogee11,          "AppleSCCIrDA: executeEvent - PD_E_DATA_SIZE, data="},
	{kLogee12,          "AppleSCCIrDA: executeEvent - PD_RS232_E_STOP_BITS, data="},
	{kLogee13,          "AppleSCCIrDA: executeEvent - PD_E_RXQ_FLUSH, data="},
	{kLogee14,          "AppleSCCIrDA: executeEvent - PD_E_RX_DATA_INTEGRITY, data="},
	{kLogee15,          "AppleSCCIrDA: executeEvent - PD_E_RX_DATA_RATE, data="},
	{kLogee16,          "AppleSCCIrDA: executeEvent - PD_E_RX_DATA_SIZE, data="},
	{kLogee17,          "AppleSCCIrDA: executeEvent - PD_RS232_E_RX_STOP_BITS, data="},
	{kLogee18,          "AppleSCCIrDA: executeEvent - PD_E_TXQ_FLUSH, data="},
	{kLogee19,          "AppleSCCIrDA: executeEvent - PD_RS232_E_LINE_BREAK, data="},
	{kLogee20,          "AppleSCCIrDA: executeEvent - PD_E_DELAY, data="},
	{kLogee21,          "AppleSCCIrDA: executeEvent - PD_E_RXQ_SIZE, data="},
	{kLogee22,          "AppleSCCIrDA: executeEvent - PD_E_TXQ_SIZE, data="},
	{kLogee23,          "AppleSCCIrDA: executeEvent - PD_E_RXQ_HIGH_WATER, data="},
	{kLogee24,          "AppleSCCIrDA: executeEvent - PD_E_RXQ_LOW_WATER, data="},
	{kLogee25,          "AppleSCCIrDA: executeEvent - PD_E_TXQ_HIGH_WATER, data="},
	{kLogee26,          "AppleSCCIrDA: executeEvent - PD_E_TXQ_LOW_WATER, data="},
	{kLogeexminus,          "AppleSCCIrDA: executeEvent - unrecognized event, data="},
	
	{kLogreEv,          "AppleSCCIrDA: requestEvent"},
	{kLogre01,          "AppleSCCIrDA: requestEvent - PD_E_ACTIVE, data="},
	{kLogre02,          "AppleSCCIrDA: requestEvent - PD_E_FLOW_CONTROL, data="},
	{kLogre03,          "AppleSCCIrDA: requestEvent - PD_E_DELAY, data="},
	{kLogre04,          "AppleSCCIrDA: requestEvent - PD_E_DATA_LATENCY, data="},
	{kLogre05,          "AppleSCCIrDA: requestEvent - PD_E_TXQ_SIZE, data="},
	{kLogre06,          "AppleSCCIrDA: requestEvent - PD_E_RXQ_SIZE, data="},
	{kLogre07,          "AppleSCCIrDA: requestEvent - PD_E_TXQ_LOW_WATER, data="},
	{kLogre08,          "AppleSCCIrDA: requestEvent - PD_E_RXQ_LOW_WATER, data="},
	{kLogre09,          "AppleSCCIrDA: requestEvent - PD_E_TXQ_HIGH_WATER, data="},
	{kLogre10,          "AppleSCCIrDA: requestEvent - PD_E_RXQ_HIGH_WATER, data="},
	{kLogre11,          "AppleSCCIrDA: requestEvent - PD_E_TXQ_AVAILABLE, data="},
	{kLogre12,          "AppleSCCIrDA: requestEvent - PD_E_RXQ_AVAILABLE, data="},
	{kLogre13,          "AppleSCCIrDA: requestEvent - PD_E_DATA_RATE, data="},
	{kLogre14,          "AppleSCCIrDA: requestEvent - PD_E_RX_DATA_RATE, data="},
	{kLogre15,          "AppleSCCIrDA: requestEvent - PD_E_DATA_SIZE, data="},
	{kLogre16,          "AppleSCCIrDA: requestEvent - PD_E_RX_DATA_SIZE, data="},
	{kLogre17,          "AppleSCCIrDA: requestEvent - PD_E_DATA_INTEGRITY, data="},
	{kLogre18,          "AppleSCCIrDA: requestEvent - PD_E_RX_DATA_INTEGRITY, data="},
	{kLogre19,          "AppleSCCIrDA: requestEvent - PD_RS232_E_STOP_BITS, data="},
	{kLogre20,          "AppleSCCIrDA: requestEvent - PD_RS232_E_RX_STOP_BITS, data="},
	{kLogre21,          "AppleSCCIrDA: requestEvent - PD_RS232_E_XON_BYTE, data="},
	{kLogre22,          "AppleSCCIrDA: requestEvent - PD_RS232_E_XOFF_BYTE, data="},
	{kLogre23,          "AppleSCCIrDA: requestEvent - PD_RS232_E_LINE_BREAK, data="},
	{kLogre24,          "AppleSCCIrDA: requestEvent - PD_RS232_E_MIN_LATENCY, data="},
	{kLogreeminus,          "AppleSCCIrDA: requestEvent - unrecognized event, data="},
	
	{kLogeqDt,          "AppleSCCIrDA: enqueData"},
	{kLogeDWminus,          "AppleSCCIrDA: enqueueData - IrDA write problem, data has been dropped"},
	{kLogeDWplus,           "AppleSCCIrDA: enqueueData - data successfully handed to IrDA"},
	{kLogeDfminus,          "AppleSCCIrDA: enqueueData - frame allocation failed"},
	{kLogeDdminus,          "AppleSCCIrDA: enqueueData - buffer allocation failed"},
	
	{kLogdqDt,          "AppleSCCIrDA: dequeueData"},
	{kLogdDpminus,          "AppleSCCIrDA: dequeueData - parameter error"},
	{kLogdqRt,          "AppleSCCIrDA: dequeueData - read interrupted"},
	{kLogdqDplus,           "AppleSCCIrDA: dequeueData - successful"},
	
	{kLogSUTm,          "AppleSCCIrDA: SetUpTransmit"},
	{kLogSIWminus,          "AppleSCCIrDA: SetUpTransmit - IrDA write problem, data has been dropped"},
	{kLogSIWplus,           "AppleSCCIrDA: SetUpTransmit - data successfully handed to IrDA"},
	
	{kLogstTx,          "AppleSCCIrDA: StartTransmit"},
	{kLogStTn,          "AppleSCCIrDA: StartTransmit - no data (count = 0)"},
	
	{kLogfrRB,          "AppleSCCIrDA: freeRingBuffer"},
	
	{kLogalRB,          "AppleSCCIrDA: allocateRingBuffer"},
	
	{kLogmess,          "AppleSCCIrDA: message"},
	{kLogms01,          "AppleSCCIrDA: message - kIOMessageServiceIsTerminated"},
	{kLogms02,          "AppleSCCIrDA: message - kIOMessageServiceIsSuspended"},
	{kLogms03,          "AppleSCCIrDA: message - kIOMessageServiceIsResumed"},
	{kLogms04,          "AppleSCCIrDA: message - kIOMessageServiceIsRequestingClose"},
	{kLogms05,          "AppleSCCIrDA: message - kIOMessageServiceWasClosed"},
	{kLogms06,          "AppleSCCIrDA: message - kIOMessageServiceBusyStateChange"},
	{kLogmesminus,          "AppleSCCIrDA: message - unknown message"},
	
	{kLogpIpB,          "AppleSCCIrDA: parseInputBuffer"},
	{kLogpImminus,          "AppleSCCIrDA: parseInputBuffer - incorrect mode, data dropped"},
	
	{kLogpISR,          "AppleSCCIrDA: parseInputSIR"},
	{kLogpStI,          "AppleSCCIrDA: parseInputSIR - parse idle state, byte="},
	{kLogpStB,          "AppleSCCIrDA: parseInputSIR - parse BOF statef, byte="},
	{kLogpStd,          "AppleSCCIrDA: parseInputSIR - parse data state, byte="},
	{kLogpSIminus,          "AppleSCCIrDA: parseInputSIR - IrDA ReadComplete problem"},
	{kLogpSCminus,          "AppleSCCIrDA: parseInputSIR - CRC error"},
	{kLogpSEminus,          "AppleSCCIrDA: parseInputSIR - EOF early < 2 bytes of data"},
	{kLogpStp,          "AppleSCCIrDA: parseInputSIR - parse pad state, byte="},
	{kLogpSte,          "AppleSCCIrDA: parseInputSIR - state error"},
	
	{kLogpIFR,          "AppleSCCIrDA: parseInputFIR - Currently not supported"},
	
	{kLogpIRt,          "AppleSCCIrDA: parseInputReset"},
	
	{kLogPBFT,          "AppleSCCIrDA: Prepare_Buffer_For_Transmit"},
	
	{kLogrxLp,          "AppleSCCIrDA: rxLoop"},
	{kLogrxok,          "AppleSCCIrDA: rxLoop - read bytes, count="},
	{kLogrxDminus,          "AppleSCCIrDA: rxLoop - dequeueData (read) failed, err="},
	{kLogrxLminus,          "AppleSCCIrDA: rxLoop - Timer problem (SCC read has stopped)"},
	{kLogrxKl,          "AppleSCCIrDA: rxLoop - RX stopped (kill)"},
	
	{kLogtxLp,          "AppleSCCIrDA: txLoop"},
	{kLogtxRs,          "AppleSCCIrDA: txLoop - kTX_Reset"},
	{kLogtxDt,          "AppleSCCIrDA: txLoop - kTX_Data"},
	{kLogtxSE,          "AppleSCCIrDA: txLoop - State Error"},
	{kLogtxLminus,          "AppleSCCIrDA: txLoop - Timer problem (SCC write has stopped)"},
	{kLogtxKl,          "AppleSCCIrDA: txLoop - TX stopped (kill)"},
	
	{kLogXmitCount,         "AppleSCCIrDA - start transmit, queue count="},
	{kLogSpeedChangeThread, "AppleSCCIrDA: - speed change thread"},
	
	{kLogTxThread,          "AppleSCCIrDA: tx thread"},
	{kLogTxThreadDeferred,  "AppleSCCIrDA: tx thread deferred"},
	{kLogTxThreadResumed,   "AppleSCCIrDA: tx thread resumed"},
	
	{kLogRxThread,          "AppleSCCIrDA: rx thread"},
	{kLogRxThreadResumed,   "AppleSCCIrDA: rx thread resumed"},
	{kLogRxThreadSleeping,  "AppleSCCIrDA: rx thread waiting for sendcommand"},
	{kLogRxThreadGotData,   "AppleSCCIrDA: rx thread has data"},
	
	{kLogSetStructureDefaults,  "AppleSCCIrDA: set port structure defaults, init="},
	{kLogAcquirePort,           "AppleSCCIrDA: acquire port"},
	{kLogReleasePort,           "AppleSCCIrDA: release port"},
	
	{kLogSetState1,             "AppleSCCIrDA: set state, state="},
	{kLogSetState2,             "AppleSCCIrDA: set state, mask="},
	{kLogGetState,              "AppleSCCIrDA; get state, state="},
	
	{kLogWatchState,            "AppleSCCIrDA: watch state"},
	{kLogWatchState1,           "AppleSCCIrDA: watch state, state="},
	{kLogWatchState2,           "AppleSCCIrDA: watch state, mask="},
	{kLogExecEvent,             "AppleSCCIrDA: exec event"},
	{kLogExecEventData,         "AppleSCCIrDA: exec event, data="},
	
	{kLogReqEvent,              "AppleSCCIrDA: request event"},
	{kLogReqEventData,          "AppleSCCIrDA: request event data"},
	
	{kLogChangeStateBits,       "AppleSCCIrDA: change state, new bits="},
	{kLogChangeStateMask,       "AppleSCCIrDA: change state, mask="},
	{kLogChangeStateDelta,      "AppleSCCIrDA: change state, delta="},
	{kLogChangeStateNew,        "AppleSCCIrDA: change state, new state="},
	
	{kLogInitForPM,             "AppleSCCIrDA: init power management"},
	{kLogInitialPowerState,     "AppleSCCIrDA: get initial power state, flags="},
	{kLogSetPowerState,         "AppleSCCIrDA: set power state, ordinal="}

};

#define XTRACE(x, y, z) IrDALogAdd ( x, (UInt32)y, (UInt16)z, gTraceEvents, true)
#else
#define XTRACE(x, y, z) ((void)0)
#endif

enum {
    kIrDAPowerOffState  = 0,
    kIrDAPowerOnState   = 1,
    kNumIrDAStates = 2
};

static IOPMPowerState gOurPowerStates[kNumIrDAStates] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};


#define super AppleIrDASerial

    OSDefineMetaClassAndStructors( AppleSCCIrDA, AppleIrDASerial );

/****************************************************************************************************/
//      CRC checking and generation (both 16 bit and 32 bit)
/****************************************************************************************************/

#define POLYNOMIAL 0xedb88320               // 32-bit CRC polynomial, with lsb bits first
#define P 0x8408                    // The HDLC 16 bit polynomial: x**0 + x**5 + x**12 + x**16 (0x8408)

#define VALID_CRC   0xDEBB20E3          // The 32 bit result after running over the CRC
#define INITIAL_CRC 0xFFFFFFFF          // Init 32 bit crc to this before starting

static unsigned long    crc_table[256];         // 32 bit table
static UInt16       crc16_table[256];       // 16 bit table

/****************************************************************************************************/
//
//      Function:   gen_crc32_table
//
//      Inputs:     
//
//      Outputs:    
//
//      Desc:       Generate the table of CRC remainders for all possible bytes
//
/****************************************************************************************************/

void gen_crc32_table()
{
    register int i, j;
    register unsigned long crc_accum;
    
    for ( i = 0;  i < 256;  i++ )
    {
	crc_accum = ( (unsigned long) i  );
	for ( j = 0;  j < 8;  j++ )
	{
	    if ( crc_accum & 1 )
		crc_accum = ( crc_accum >> 1 ) ^ POLYNOMIAL;
	    else   crc_accum = ( crc_accum >> 1 );
	}
	crc_table[i] = crc_accum;
    }
    return;
    
}/* end gen_crc32_table */

/****************************************************************************************************/
//
//      Function:   gen_crc16_table
//
//      Inputs:     
//
//      Outputs:    
//
//      Desc:       Generate the CRC16 table
//
/****************************************************************************************************/

void gen_crc16_table()
{
    register unsigned int b, v;
    register int i;
    
    for (b = 0; b < 256; b++)
    {
	v = b;
	for (i = 8; i--; )
	    v = v & 1 ? (v >> 1) ^ P : v >> 1;
	crc16_table[b] = v;
    }
    
}/* end gen_crc16_table */

/****************************************************************************************************/
//
//      Function:   update_crc32
//
//      Inputs:     crc - the current crc value
//              cp - new data
//              len - length of the new data
//
//      Outputs:    crc - the new crc
//
//      Desc:       Update the CRC on the data block one byte at a time
//
/****************************************************************************************************/

UInt32 update_crc32( UInt32 crc, unsigned char *cp, int len )
{
    register int i, j;
    
    for ( j = 0;  j < len;  j++ )
    {
	i = ( (int) ( crc ^ *cp++ ) & 0xff );
	crc = ( crc >> 8 ) ^ crc_table[i];
    }
    return crc;
   
}/* end update_crc32 */

/****************************************************************************************************/
//
//      Function:   update_crc16
//
//      Inputs:     crc - the current crc value
//              cp - new data
//              len - length of the new data
//
//      Outputs:    crc - the new crc
//
//      Desc:       Update the CRC on the data block one byte at a time
//
/****************************************************************************************************/

UInt16 update_crc16( UInt16 crc, unsigned char *cp, int len )
{

    while (len--)
	crc = (crc >> 8) ^ crc16_table[(crc ^ *cp++) & 0xff];
	
    return (crc);
    
}/* end update_crc16 */

/****************************************************************************************************/
//
//      Function:   check_crc32
//
//      Inputs:     buf - the data 
//              len - length of the data
//
//      Outputs:    bool - true (good), false (not so good)
//
//      Desc:       Call this to see if the crc-32 at the end of the block is valid
//
/****************************************************************************************************/

bool check_crc32( unsigned char *buf, int len )
{
    unsigned long crc = INITIAL_CRC;
    
    crc = update_crc32(crc, buf, len);
    
    return (crc == VALID_CRC);
    
}/* end check_crc32 */

/****************************************************************************************************/
//
//      Function:   check_crc16
//
//      Inputs:     buf - the data 
//              len - length of the data
//
//      Outputs:    bool - true (good), false (not so good)
//
//      Desc:       Call this to see if the crc-16 at the end of the block is valid
//
/****************************************************************************************************/

bool check_crc16( unsigned char *buf, int len )
{
    UInt16 crc;
    
    if (len < 2) return false;          // sanity check
    
    crc = 0xffff;               // init crc
    crc = update_crc16(crc, buf, len-2);    // run over the data bytes
    crc = ~crc;                 // finalize it
    
    return ( ((crc & 0xff) == buf[len-2]) && ((crc >> 8)   == buf[len-1]) );
	
}/* end check_crc16 */
    
    
#if LOG_DATA
/****************************************************************************************************/
//
//      Function:   Asciify
//
//      Inputs:     i - the nibble
//
//      Outputs:    return byte - ascii byte
//
//      Desc:       Converts to ascii. 
//
/****************************************************************************************************/
 
static UInt8 Asciify(UInt8 i)
{

    i &= 0xF;
    if ( i < 10 )
	return( '0' + i );
    else return( 55  + i );
    
}/* end Asciify */
#endif // LOG_DATA

#if USE_ELG
/****************************************************************************************************/
//
//      Function:   AllocateEventLog
//
//      Inputs:     size - amount of memory to allocate
//
//      Outputs:    None
//
//      Desc:       Allocates the event log buffer
//
/****************************************************************************************************/

void AllocateEventLog( UInt32 size )
{
    if ( g.evLogBuf )
	return;

    g.evLogFlag = 0;            /* assume insufficient memory   */
    g.evLogBuf = (UInt8*)IOMalloc( size );
    if ( !g.evLogBuf )
    {
	IOLog( "AppleSCCIrDA: evLog allocation failed" );
	return;
    }

    bzero( g.evLogBuf, size );
    g.evLogBufp = g.evLogBuf;
    g.evLogBufe = g.evLogBufp + kEvLogSize - 0x20; // ??? overran buffer?
    g.evLogFlag  = 0xFEEDBEEF;  // continuous wraparound
//  g.evLogFlag  = 'step';      // stop at each ELG
//  g.evLogFlag  = 0x0333;      // any nonzero - don't wrap - stop logging at buffer end

    IOLog( "AppleSCCIrDA: AllocateEventLog - &globals=%8x buffer=%8x", (unsigned int)&g, (unsigned int)g.evLogBuf );

    return;
    
}/* end AllocateEventLog */

/****************************************************************************************************/
//
//      Function:   EvLog
//
//      Inputs:     a - anything 
//              b - anything 
//              ascii - 4 charater tag
//              str - any info string           
//
//      Outputs:    None
//
//      Desc:       Writes the various inputs to the event log buffer
//
/****************************************************************************************************/

void EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
    register UInt32 *lp;           /* Long pointer      */
    mach_timespec_t time;

    if ( g.evLogFlag == 0 )
	return;

    IOGetTime( &time );

    lp = (UInt32*)g.evLogBufp;
    g.evLogBufp += 0x10;

    if ( g.evLogBufp >= g.evLogBufe )       /* handle buffer wrap around if any */
    {    
	g.evLogBufp  = g.evLogBuf;
	if ( g.evLogFlag != 0xFEEDBEEF )    // make 0xFEEDBEEF a symbolic ???
	    g.evLogFlag = 0;                /* stop tracing if wrap undesired   */
    }

	/* compose interrupt level with 3 byte time stamp:  */

    *lp++ = (g.intLevel << 24) | ((time.tv_nsec >> 10) & 0x003FFFFF);   // ~ 1 microsec resolution
    *lp++ = a;
    *lp++ = b;
    *lp   = ascii;

    if( g.evLogFlag == 'step' )
    {   
	static char code[ 5 ] = {0,0,0,0,0};
	*(UInt32*)&code = ascii;
	IOLog( "AppleSCCIrDA: %8x %8x %8x %s\n", time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code );
    }

    return;
    
}/* end EvLog */
#endif // USE_ELG

#if LOG_DATA
#define dumplen     32      // Set this to the number of bytes to dump and the rest should work out correct

#define buflen      ((dumplen*2)+dumplen)+3
#define Asciistart  (dumplen*2)+3

/****************************************************************************************************/
//
//      Function:   DEVLogData
//
//      Inputs:     Dir - direction 
//              Count - number of bytes
//              buf - the data
//
//      Outputs:    None
//
//      Desc:       Puts the data in the log. 
//
/****************************************************************************************************/

void DEVLogData(UInt8 Dir, UInt32 Count, char *buf)
{
    UInt8   wlen, i, Aspnt, Hxpnt;
    UInt8   wchr;
    char    LocBuf[buflen+1];

    for ( i=0; i<=buflen; i++ )
    {
	    LocBuf[i] = 0x20;
    }
    LocBuf[i] = 0x00;
    
    switch (Dir)
    {
	case kDriverIn: 
	    IOLog( "AppleSCCIrDA: SCCLogData - from SCC, size = %8lx\n", Count );
	    break;
	case kAppIn:
	    IOLog( "AppleSCCIrDA: SCCLogData - to Application, size = %8lx\n", Count );
	    break;
	case kDriverOut:
	    IOLog( "AppleSCCIrDA: SCCLogData - to SCC, size = %8lx\n", Count );
	    break;
	case kAppOut:
	    IOLog( "AppleSCCIrDA: SCCLogData - from Application, size = %8lx\n", Count );
	    break;
	case kAny:
	    IOLog( "AppleSCCIrDA: SCCLogData - Other, size = %8lx\n", Count );
	    break;
	default:
	    IOLog( "AppleSCCIrDA: SCCLogData - Unknown, size = %8lx\n", Count );
	    break;
    }

    if ( Count > dumplen )
    {
	wlen = dumplen;
    } else {
	wlen = Count;
    }
    
    if ( wlen > 0 )
    {
	Aspnt = Asciistart;
	Hxpnt = 0;
	for ( i=1; i<=wlen; i++ )
	{
	    wchr = buf[i-1];
	    LocBuf[Hxpnt++] = Asciify( wchr >> 4 );
	    LocBuf[Hxpnt++] = Asciify( wchr );
	    if (( wchr < 0x20) || (wchr > 0x7F ))       // Non printable characters
	    {
		LocBuf[Aspnt++] = 0x2E;             // Replace with a period
	    } else {
		LocBuf[Aspnt++] = wchr;
	    }
	}
	LocBuf[(wlen + Asciistart) + 1] = 0x00;
	IOLog( LocBuf );
	IOLog( "\n" );
    } else {
	IOLog( "AppleSCCIrDA: SCCLogData - No data, Count = 0\n" );
    }
    
}/* end DEVLogData */
#endif // LOG_DATA

    
/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::AddBytetoQueue
//
//      Inputs:     Queue - the queue to be added to
//
//      Outputs:    Value - Byte to be added, Queue status - full or no error
//
//      Desc:       Add a byte to the circular queue.
//
/****************************************************************************************************/

QueueStatus AppleSCCIrDA::AddBytetoQueue( CirQueue *Queue, char Value )
{
    /* Check to see if there is space by comparing the next pointer,    */
    /* with the last, If they match we are either Empty or full, so     */
    /* check the InQueue of being zero.                 */

    require(fPort && fPort->serialRequestLock, Fail);
    IOLockLock( fPort->serialRequestLock );

    if ( (Queue->NextChar == Queue->LastChar) && Queue->InQueue ) {
	IOLockUnlock( fPort->serialRequestLock);
	return queueFull;
    }

    *Queue->NextChar++ = Value;
    Queue->InQueue++;

	/* Check to see if we need to wrap the pointer. */
	
    if ( Queue->NextChar >= Queue->End )
	Queue->NextChar =  Queue->Start;

    IOLockUnlock( fPort->serialRequestLock);
    return queueNoError;
    
Fail:
    return queueFull;       // for lack of a better error
    
}/* end AddBytetoQueue */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::GetBytetoQueue
//
//      Inputs:     Queue - the queue to be removed from
//
//      Outputs:    Value - where to put the byte, Queue status - empty or no error
//
//      Desc:       Remove a byte from the circular queue.
//
/****************************************************************************************************/

QueueStatus AppleSCCIrDA::GetBytetoQueue( CirQueue *Queue, UInt8 *Value )
{

    require(fPort && fPort->serialRequestLock, Fail);
    IOLockLock( fPort->serialRequestLock );

	/* Check to see if the queue has something in it.   */
	
    if ( (Queue->NextChar == Queue->LastChar) && !Queue->InQueue ) {
	IOLockUnlock(fPort->serialRequestLock);
	return queueEmpty;
    }

    *Value = *Queue->LastChar++;
    Queue->InQueue--;

	/* Check to see if we need to wrap the pointer. */
	
    if ( Queue->LastChar >= Queue->End )
	Queue->LastChar =  Queue->Start;

    IOLockUnlock(fPort->serialRequestLock);
    return queueNoError;
    
Fail:
    return queueEmpty;          // can't get to it, pretend it's empty
    
}/* end GetBytetoQueue */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::InitQueue
//
//      Inputs:     Queue - the queue to be initialized, Buffer - the buffer, size - length of buffer
//
//      Outputs:    Queue status - queueNoError.
//
//      Desc:       Pass a buffer of memory and this routine will set up the internal data structures.
//
/****************************************************************************************************/

QueueStatus AppleSCCIrDA::InitQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size )
{
    Queue->Start    = Buffer;
    Queue->End      = (UInt8*)((size_t)Buffer + Size);
    Queue->Size     = Size;
    Queue->NextChar = Buffer;
    Queue->LastChar = Buffer;
    Queue->InQueue  = 0;

    IOSleep( 1 );
    
    return queueNoError ;
    
}/* end InitQueue */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::CloseQueue
//
//      Inputs:     Queue - the queue to be closed
//
//      Outputs:    Queue status - queueNoError.
//
//      Desc:       Clear out all of the data structures.
//
/****************************************************************************************************/

QueueStatus AppleSCCIrDA::CloseQueue( CirQueue *Queue )
{

    Queue->Start    = 0;
    Queue->End      = 0;
    Queue->NextChar = 0;
    Queue->LastChar = 0;
    Queue->Size     = 0;

    return queueNoError;
    
}/* end CloseQueue */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::AddtoQueue
//
//      Inputs:     Queue - the queue to be added to, Buffer - data to add, Size - length of data
//
//      Outputs:    BytesWritten - Number of bytes actually put in the queue.
//
//      Desc:       Add an entire buffer to the queue.
//
/****************************************************************************************************/

size_t AppleSCCIrDA::AddtoQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size )
{
    size_t  BytesWritten = 0;

    while ( FreeSpaceinQueue( Queue ) && (Size > BytesWritten) )
    {
	AddBytetoQueue( Queue, *Buffer++ );
	BytesWritten++;
    }

    return BytesWritten;
    
}/* end AddtoQueue */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::RemovefromQueue
//
//      Inputs:     Queue - the queue to be removed from, Size - size of buffer
//
//      Outputs:    Buffer - Where to put the data, BytesReceived - Number of bytes actually put in Buffer.
//
//      Desc:       Get a buffers worth of data from the queue.
//
/****************************************************************************************************/

size_t AppleSCCIrDA::RemovefromQueue( CirQueue *Queue, UInt8 *Buffer, size_t MaxSize )
{
    size_t  BytesReceived = 0;
    UInt8   Value;
    
    while( (MaxSize > BytesReceived) && (GetBytetoQueue(Queue, &Value) == queueNoError) ) 
    {
	*Buffer++ = Value;
	BytesReceived++;
    }

    return BytesReceived;
    
}/* end RemovefromQueue */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::FreeSpaceinQueue
//
//      Inputs:     Queue - the queue to be queried
//
//      Outputs:    Return Value - Free space left
//
//      Desc:       Return the amount of free space left in this buffer.
//
/****************************************************************************************************/

size_t AppleSCCIrDA::FreeSpaceinQueue( CirQueue *Queue )
{
    size_t  retVal = 0;

    require(fPort && fPort->serialRequestLock, Fail);
    IOLockLock( fPort->serialRequestLock );

    retVal = Queue->Size - Queue->InQueue;
    
    IOLockUnlock(fPort->serialRequestLock);
    
Fail:
    return retVal;
    
}/* end FreeSpaceinQueue */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::UsedSpaceinQueue
//
//      Inputs:     Queue - the queue to be queried
//
//      Outputs:    UsedSpace - Amount of data in buffer
//
//      Desc:       Return the amount of data in this buffer.
//
/****************************************************************************************************/

size_t AppleSCCIrDA::UsedSpaceinQueue( CirQueue *Queue )
{
    return Queue->InQueue;
    
}/* end UsedSpaceinQueue */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::GetQueueSize
//
//      Inputs:     Queue - the queue to be queried
//
//      Outputs:    QueueSize - The size of the queue.
//
//      Desc:       Return the total size of the queue.
//
/****************************************************************************************************/

size_t AppleSCCIrDA::GetQueueSize( CirQueue *Queue )
{
    return Queue->Size;
    
}/* end GetQueueSize */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::GetQueueStatus
//
//      Inputs:     Queue - the queue to be queried
//
//      Outputs:    Queue status - full, empty or no error
//
//      Desc:       Returns the status of the circular queue.
//
/****************************************************************************************************/
/*
QueueStatus AppleSCCIrDA::GetQueueStatus( CirQueue *Queue )
{
    QueueStatus retval = queueFull;

    require(fPort && fPort->serialRequestLock, Fail);
    IOLockLock( fPort->serialRequestLock );

    if ( (Queue->NextChar == Queue->LastChar) && Queue->InQueue )
	retval = queueFull;
    else if ( (Queue->NextChar == Queue->LastChar) && !Queue->InQueue )
	retval = queueEmpty;
    else
	retval = queueNoError;
	
    IOLockUnlock(fPort->serialRequestLock);
	
Fail:
    return retval;

} */ /* end GetQueueStatus */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::CheckQueues
//
//      Inputs:     port - the port to check
//
//      Outputs:    None
//
//      Desc:       Checks the various queue's etc and manipulates the state(s) accordingly
//
/****************************************************************************************************/

void AppleSCCIrDA::CheckQueues( PortInfo_t *port )
{
    unsigned long   Used;
    unsigned long   Free;
    unsigned long   QueuingState;
    unsigned long   DeltaState;

    // Initialise the QueueState with the current state.
    QueuingState = readPortState( port );

	/* Check to see if there is anything in the Transmit buffer. */
    Used = UsedSpaceinQueue( &port->TX );
    Free = FreeSpaceinQueue( &port->TX );
//  ELG( Free, Used, 'CkQs', "CheckQueues" );
    if ( Free == 0 )
    {
	QueuingState |=  PD_S_TXQ_FULL;
	QueuingState &= ~PD_S_TXQ_EMPTY;
    }
    else if ( Used == 0 )
    {
	QueuingState &= ~PD_S_TXQ_FULL;
	QueuingState |=  PD_S_TXQ_EMPTY;
    }
    else
    {
	QueuingState &= ~PD_S_TXQ_FULL;
	QueuingState &= ~PD_S_TXQ_EMPTY;
    }

	/* Check to see if we are below the low water mark. */
    if ( Used < port->TXStats.LowWater )
	 QueuingState |=  PD_S_TXQ_LOW_WATER;
    else QueuingState &= ~PD_S_TXQ_LOW_WATER;

    if ( Used > port->TXStats.HighWater )
	 QueuingState |= PD_S_TXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_TXQ_HIGH_WATER;


	/* Check to see if there is anything in the Receive buffer. */
    Used = UsedSpaceinQueue( &port->RX );
    Free = FreeSpaceinQueue( &port->RX );

    if ( Free == 0 )
    {
	QueuingState |= PD_S_RXQ_FULL;
	QueuingState &= ~PD_S_RXQ_EMPTY;
    }
    else if ( Used == 0 )
    {
	QueuingState &= ~PD_S_RXQ_FULL;
	QueuingState |= PD_S_RXQ_EMPTY;
    }
    else
    {
	QueuingState &= ~PD_S_RXQ_FULL;
	QueuingState &= ~PD_S_RXQ_EMPTY;
    }

	/* Check to see if we are below the low water mark. */
    if ( Used < port->RXStats.LowWater )
	 QueuingState |= PD_S_RXQ_LOW_WATER;
    else QueuingState &= ~PD_S_RXQ_LOW_WATER;

    if ( Used > port->RXStats.HighWater )
	 QueuingState |= PD_S_RXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_RXQ_HIGH_WATER;

	/* Figure out what has changed to get mask.*/
    DeltaState = QueuingState ^ readPortState( port );
    changeState( port, QueuingState, DeltaState );
    
    return;
    
}/* end CheckQueues */


/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::Add_RXBytes
//
//      Inputs:     Buffer - the raw input data
//              Size - the length
//
//      Outputs:
//
//      Desc:       Adds data to the circular receive queue 
//
/****************************************************************************************************/

void AppleSCCIrDA::Add_RXBytes( UInt8 *Buffer, size_t Size )
{
    
    XTRACE(kLogAdRB, 0, Size);
    ELG( 0, Size, 'AdRB', "Add_RXBytes" );
    
    AddtoQueue( &fPort->RX, Buffer, Size );
    CheckQueues( fPort );
}/* end Add_RXBytes */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::SetBofCount
//
//      Inputs:     bof_count - the requested number of Beginning Of Frames
//
//      Outputs:    return word - the actual count (not bofs)
//
//      Desc:       Encode the requested number of BOF bytes to the first value that's big enough 
//
/****************************************************************************************************/  

SInt16 AppleSCCIrDA::SetBofCount( SInt16 bof_count )
{

    ELG( 0, bof_count, 'Sbof', "SetBofCount" );
    XTRACE(kLogSbof, 0, bof_count);
    
    fBofsCode = bof_count;
    
    return fBofsCode;
	
}/* end SetBofCount */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::SetSpeed
//
//      Inputs:     brate - the requested baud rate
//
//      Outputs:    return word - baud coding
//
//      Desc:       Set the baudrate for the device 
//
/****************************************************************************************************/  

UInt16 AppleSCCIrDA::SetSpeed( UInt32 brate )
{
    UInt8   command = 0;
    
    ELG( 0, brate, 'StSp', "SetSpeed" );
    XTRACE(kLogStSp, brate >> 16, brate);
    
    require(fSpeedChanging == false, Fail);     // sanity check
    
    switch (brate)
    {
	default:
	    ELG( 0, brate, 'SSp-', "SetSpeed - Unsupported baud rate");
	    XTRACE(kLogSSpminus, brate >> 16, brate);
	    //break;            // fall through and set to 9600

	case 9600: 
	    fBaudCode = brate;
	    fModeFlag = kMode_SIR;
	    command = kSetModeHP9600_1_6;
	    break;
	    
	case 19200: 
	    fBaudCode = brate;
	    fModeFlag = kMode_SIR;
	    command = kSetModeHP19200_1_6;
	    break;
	    
	case 38400: 
	    fBaudCode = brate;
	    fModeFlag = kMode_SIR;
	    command = kSetModeHP38400_1_6;
	    break;
	    
	case 57600: 
	    fBaudCode = brate;
	    fModeFlag = kMode_SIR;
	    command = kSetModeHP57600_1_6;
	    break;
	    
	case 115200:
	    fBaudCode = brate;
	    fModeFlag = kMode_SIR;
	    command = kSetModeHP115200_1_6;
	    break;
	    
	case 576000:
	    fBaudCode = brate;
	    fModeFlag = kMode_MIR;
	    break;
	    
	case 1152000:
	    fBaudCode = brate;
	    fModeFlag = kMode_FIR;
	    break;
	    
	case 4000000:
	    fBaudCode = brate;
	    fModeFlag = kMode_FIR;
	    break;
	    
    }
    
    if (command) {
	bool rc;
	fSpeedChanging = true;
	rc = thread_call_enter1(speed_change_thread_call, (thread_call_param_t)command);
	check(rc == false);     // true here meant it was already running
    }
    return fBaudCode;

Fail:
    return (short)-1;
    
}/* end SetSpeed */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::GetIrDAComm
//
//      Inputs: 
//
//      Outputs:    IrDAComm - Address of the IrDA object
//
//      Desc:       Returns the address of the IrDA object 
//
/****************************************************************************************************/

IrDAComm* AppleSCCIrDA::GetIrDAComm( void )
{
    return fIrDA;
	
}/* end GetIrDAComm */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::GetIrDAQoS
//
//      Inputs: 
//
//      Outputs:    USBIrDAQoS - Address of the QoS structure
//
//      Desc:       Returns the address of the Quality of Service structure
//
/****************************************************************************************************/

USBIrDAQoS* AppleSCCIrDA::GetIrDAQoS( void )
{
    return &fQoS;
	
}/* end GetIrDAQoS */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::GetIrDAStatus
//
//      Inputs:     status - status structure
//
//      Outputs:    
//
//      Desc:       Sets the connection state and CRC errors of the status structure
//
/****************************************************************************************************/

void AppleSCCIrDA::GetIrDAStatus( IrDAStatus *status )
{

    ELG( 0, 0, 'GIrS', "GetIrDAStatus" );
    XTRACE(kLogGIrS, 0, 0);
    
    if ( !fIrDAOn )
    {
	//bzero( status, sizeof(IrDAStatus) );
	status->connectionState = kIrDAStatusOff;
    } else {
	if ( status->connectionState == kIrDAStatusOff )
	{
	    status->connectionState = kIrDAStatusIdle;
	}
	status->crcErrors = fICRCError;
    }
    
}/* end GetIrDAStatus */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::SetIrDAUserClientState
//
//      Inputs:     state - true = IrDA on, false = IrDA off
//
//      Outputs:    
//
//      Desc:       User client stub to change the state of IrDA
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::SetIrDAUserClientState( bool IrDAOn )
{
    ELG( 0, IrDAOn, 'SICS', "SetIrDAUserClientState" );
    XTRACE(kLogSICS, 0, IrDAOn);

    fUserClientStarted = IrDAOn;
    return CheckIrDAState();
    
}/* SetIrDAUserClientState */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::CheckIrDAState
//
//      Inputs:     
//
//      Outputs:    
//
//      Desc:       Turns IrDA on or off if needed
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::CheckIrDAState()
{
    IOReturn    ior = kIOReturnSuccess;
    bool    newState;           // new irda on/off state
    
    newState = fSCCStarted &&                       // we have to be started (and not stopped) for irda to run, and
		(fPowerState == kIrDAPowerOnState) &&   // powered on by the power manager, and
		(fUserClientStarted | (fSessions > 0)); // Either the user client or a real app must be started
    
    ELG( 0, newState, 'CIrS', "CheckIrDAState" );
    ELG( fUserClientStarted, fSessions, 'CIrS', "CheckIrDAState *" );
    XTRACE(kLogCIrS, 0, newState);
    
    if (newState != fIrDAOn)            // if desired state not the current state
	ior = SetIrDAState(newState);   // then change it

    return ior;
    
}/* end CheckIrDAState */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::SetIrDAState
//
//      Inputs:     state - true = IrDA on, false = IrDA off
//
//      Outputs:    
//
//      Desc:       Turns IrDA on or off
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::SetIrDAState( bool IrDAOn )
{
    IOReturn    ior = kIOReturnSuccess;
    
    ELG( 0, IrDAOn, 'SIrS', "SetIrDAState" );
    XTRACE(kLogSIrS, 0, IrDAOn);
    
    if ( IrDAOn && !fIrDAOn)        // if want on, but we're off, then turn IrDA on
    {
	fIrDAOn = true;
	fTerminate = false;
	
	if ( configureDevice() )
	{       
	    ELG( 0, 0, 'stcn', "SetIrDAState - successful (IrDA On)" );
	    XTRACE(kLogstcn, 0, 0);
	    fICRCError = 0;
	} else {
	    ELG( 0, 0, 'str-', "SetIrDAState - configureDevice failed" );
	    XTRACE(kLogstrminus, 0, 0);
	    //fpDevice->close(this);        -- jdg, ths is done in stop
	    // let's stop irda if we've failed to start
	    SetIrDAState(false);        // recurse to turn off irda
	    ior = kIOReturnIOError;
	}
	
    } else {
	if ( !IrDAOn && fIrDAOn )       // if want off, but currently on, turn it off
	{
	    if (fIrDA) 
	    {
		fIrDA->Stop();
		fIrDA->release();
		fIrDA = NULL;
	    }
	    
	    fpDevice->executeEvent( PD_E_ACTIVE, false );
	    IOSleep(100);		// give completed reads time to finish up
	    
	    if ( fInBuffer ) {
		IOFree( fInBuffer, SCCLapPayLoad );
		fInBuffer = NULL;
	    }
	 
	    if ( fOutBuffer ) {
		IOFree( fOutBuffer, SCCLapPayLoad );
		fOutBuffer = NULL;
	    }
	
	    if ( fInUseBuffer ) {
		IOFree( fInUseBuffer, SCCLapPayLoad );
		fInUseBuffer = NULL;
	    }
	
	    ELG( 0, 0, 'stbr', "SetIrDAState - before releasePort" );
	    XTRACE(kLogstbr, 0, 0);
	    fpDevice->releasePort();
	    ELG( 0, 0, 'star', "SetIrDAState - after releasePort" );
	    XTRACE(kLogstar, 0, 0);
	
	    freeRingBuffer( &fPort->TX );
	    freeRingBuffer( &fPort->RX );

	    fIrDAOn = false;
	    fTerminate = true;              // Make it look like we've been terminated
	    
	    disableSCC();                   // and turn off the power to the scc
	}
    }
    
    return ior;
    
}/* end SetIrDAState */

/****************************************************************************************************/
//
//      Function:   AppleSCCIrDA::init
//
//      Inputs:     dict - Dictionary
//
//      Outputs:    Return code - from IOService::init
//
//      Desc:       Driver initialization
//
/****************************************************************************************************/

bool AppleSCCIrDA::init( OSDictionary *dict )
{
    bool    rc;
	
    ELG( 0, 0, 'init', "init" );
    XTRACE(kLoginit, 0, 0);
#if (hasTracing > 0)
    IOLog("AppleSCCIrDA: irdalog info at 0x%lx\n", (UInt32)IrDALogGetInfo());
#endif
    
	// Skip the IORS232SerialStreamSync's super classes, they aren't 'driver side'
    
    rc = IOService::init( dict );
    IOLogIt( 0, rc, 'init', "init" );
    
    return rc;
    
}/* end init */

/****************************************************************************************************/
//
//      Function:   AppleSCCIrDA::probe
//
//      Inputs:     provider - my provider
//
//      Outputs:    IOService - me (successful), 0 (failed)
//                  score - probe score
//
//      Desc:       Check to see if we match correctly
//
/****************************************************************************************************/

IOService* AppleSCCIrDA::probe( IOService *provider, SInt32 *score )
{ 
    
    OSString    *matchEntry = 0;
    OSData  *ssn;
    UInt32  *nWords;
    char    *tmpPtr, *tmpName;

    ELG( 0, 0, 'prob', "probe" );
    XTRACE(kLogprob, 0, 0);
    
    if (1) {                            // limit to machines we've tested
	IOService *iter = provider;
	while (iter != NULL) {
//#if (hasTracing > 0)
	    if (strcmp(iter->getName(), "PowerBook3,1") == 0)
		break;
	    if (strcmp(iter->getName(), "PowerBook3,2") == 0)
		break;
//#endif
	    if (strcmp(iter->getName(), "PowerBook3,3") == 0)
		break;
	    iter = iter->getProvider();
	}
	if (iter == NULL) return 0;     // fail if didn't match
    }

    matchEntry = OSDynamicCast (OSString, getProperty (kIONameMatchedKey));
    if ( (matchEntry == 0) || (matchEntry->isEqualTo("chrp,es3") == false) )
    {
	IOLogIt( matchEntry, 0, 'prm-', "probe - failed, name match incorrect" );
	return 0;
    }
    
    ssn = OSDynamicCast( OSData, provider->getProperty("slot-names") );
    if ( !ssn )
    {
	IOLogIt( ssn, 0, 'prs-', "probe - failed, retrieving slot-name error" );
	return 0;
    }

    nWords = (UInt32*)ssn->getBytesNoCopy();

    if ( *nWords < 1 ) 
    {
	IOLogIt( 0, 0, 'pns-', "probe - failed, no slot-name" );
	return 0;
    }
    
	// Get the slot-name
    
    tmpName = (char *)ssn->getBytesNoCopy() + sizeof(UInt32);

	// To make parsing easy set the sting to lower case
	
    for ( tmpPtr = tmpName; *tmpPtr != 0; tmpPtr++ )
    {
	*tmpPtr |= 32;
    }
    
    LogData( kAny, 4, (char *)tmpName );
    
    if ( strncmp (tmpName, "irda", 4) == 0 )
    {
	IOLogIt( 0, 0, 'prs+', "probe - successful" );
	*score = 10000;
	return this;
    } else {
	IOLogIt( 0, 0, 'prb-', "probe - failed, did not match with slot-name(irda)" );
	return 0;
    }
    
}/* end probe */

/****************************************************************************************************/
//
//      Function:   AppleSCCIrDA::attach
//
//      Inputs:     provider - my provider
//
//      Outputs:    Return code - from IOService::attach
//
//      Desc:       Attach with our provider
//
/****************************************************************************************************/

bool AppleSCCIrDA::attach(IOService *provider)
{

    ELG( 0, 0, 'atch', "attach" );
    XTRACE(kLogatch, 0, 0);
    
	// Skip the IORS232SerialStreamSync's super classes, they aren't 'driver side'
	
    return IOService::attach(provider);
    
}/* end attach */

/****************************************************************************************************/
//
//      Function:   AppleSCCIrDA::detach
//
//      Inputs:     provider - my provider
//
//      Outputs:    
//
//      Desc:       detach from our provider
//
/****************************************************************************************************/

void AppleSCCIrDA::detach(IOService *provider)
{
    
    ELG( 0, 0, 'dtch', "detach" );
    XTRACE(kLogdtch, 0, 0);
    
	// Skip the IORS232SerialStreamSync's super classes, they aren't 'driver side'
	
    IOService::detach(provider);
    
}/* detach */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::start
//
//      Inputs:     provider - my provider
//
//      Outputs:    Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//      Desc:       This is called once it has beed determined I'm probably the best driver.
//
/****************************************************************************************************/

bool AppleSCCIrDA::start( IOService *provider )
{
    XTRACE(kLogstrt, provider >> 16, provider);
    
    g.evLogBufp = NULL;
    fUserClientNub = NULL;
    fPort = NULL;
    
    fIrDAOn = false;
    fTerminate = false;     // Make sure we don't think we're being terminated
    fKillRead = false;
    rx_thread_call = NULL;
    tx_thread_call = NULL;
    speed_change_thread_call = NULL;
    
    fUserClientStarted = false;     // user/client has not started us yet
    fSCCStarted = false;
    fSessions = 0;
    fSendingCommand = false;
    fSpeedChanging = false;
    
    fPowerState = kIrDAPowerOffState;       // set to on when we're registered with the power mgr
    fProvider = provider;           // Remember that masked man
    
    disableSCC();                   // turn off the scc clocks until we start irda stack
    
#if USE_ELG
    AllocateEventLog( kEvLogSize );
    ELG( &g, g.evLogBufp, 'SCCM', "start - event logging set up." );

    waitForService( resourceMatching( "kdp" ) );
#endif /* USE_ELG */

    ELG( this, provider, 'strt', "start - this, provider." );
    
	// Skip the IORS232SerialStreamSync's super classes, they aren't 'driver side'
    
    if( !IOService::start( provider ) )
    {
	IOLogIt( 0, 0, 'SS--', "start - IOService failed" );
	return false;
    }

    /* Get my device - RS232 device */

    fpDevice = OSDynamicCast( IORS232SerialStreamSync, provider );
    if( !fpDevice )
    {
	IOLogIt( 0, 0, 'Dev-', "start - device invalid" );
	IOService::stop( provider );
	return false;
    }
    
    rx_thread_call           = thread_call_allocate(rx_thread, this);
    tx_thread_call           = thread_call_allocate(tx_thread, this);
    speed_change_thread_call = thread_call_allocate(speed_change_thread, this);
    
    check(rx_thread_call && tx_thread_call && speed_change_thread_call);
    if (!rx_thread_call || !tx_thread_call || !speed_change_thread_call)
	return false;
    
    /* Now take control of the device and configure it */
	
    if (!fpDevice->open(this))
    {
	IOLogIt( 0, 0, 'Opn-', "start - unable to open device" );
	IOService::stop( provider );
	return false;
    }
    
    
    // build a nub to make user-client name easy
    
    fUserClientNub = AppleIrDA::withNub(this);
    if (fUserClientNub) 
    {
	fUserClientNub->attach(this);
    }
    
    if (fPort == NULL) {
	fPort = (PortInfo_t*)IOMalloc( sizeof(PortInfo_t) );
    }
    if (fPort)
	bzero(fPort, sizeof(PortInfo_t));
    else return false;
    SetStructureDefaults(fPort, true);      // init the port
    fPort->serialRequestLock = IOLockAlloc();   // init lock used to protect code on MP
    if ( !fPort->serialRequestLock )
	return false;

    
    if (!createSerialStream() )     // Publish Serial Stream (bsd) services
    {
	IOLogIt( 0, 0, 'cSS-', "start - createSerialStream failed" );
	IOService::stop( provider );
	return false;
    }
    
    if (!initForPM(fProvider))
	return false;
    
    fSCCStarted = true;     // we can start up irda next time
    
    IOLogIt( 0, fProvider, 'stcf', "start - successful (IrDA Off)" );

    return true;
    
}/* end start */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::free
//
//      Inputs:     
//
//      Outputs:    
//
//      Desc:       Clean up and free the log 
//
/****************************************************************************************************/

void AppleSCCIrDA::free()
{

    ELG( 0, 0, 'free', "free" );
    XTRACE(kLogfree, 0, 0);
    
    if ( fIrDA )
	fIrDA->release();   // we don't do delete's in the kernal I suppose ...
    
#if USE_ELG
    if ( g.evLogBuf )
    IOFree( g.evLogBuf, kEvLogSize );
#endif /* USE_ELG */

	// Skip the IORS232SerialStreamSync's super classes, they aren't 'driver side'
	
    IOService::free();
    return;
    
}/* end free */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::stop
//
//      Inputs:     provider - my provider
//
//      Outputs:    
//
//      Desc:       Stops
//
/****************************************************************************************************/

void AppleSCCIrDA::stop( IOService *provider )
{
    
    ELG( 0, 0, 'stop', "stop" );
    XTRACE(kLogstop, provider >> 16, provider);
    
    fSCCStarted = false;
    CheckIrDAState();           //  stop the irda stack
    
    if (fPort) {
	if (fPort->serialRequestLock) {
	    IOLockFree(fPort->serialRequestLock);   // free the Serial Request Lock
	    fPort->serialRequestLock = NULL;
	}
	IOFree( fPort, sizeof(PortInfo_t));
	fPort = NULL;
    }
    
    if (fUserClientNub) {
	fUserClientNub->detach(this);
	fUserClientNub->release();
	fUserClientNub = NULL;
    }
    
    check(fIrDA == NULL);       // sanity
    check(fInBuffer == NULL);   // eludes us
    check(fOutBuffer == NULL);
    check(fInUseBuffer == NULL);
	    
    fpDevice->executeEvent( PD_E_ACTIVE, false );
    IOSleep(100);		// give completed reads time to finish up
    fpDevice->releasePort();
    
#ifdef Trace_SCCSerial
    (void) fpDevice->executeEvent(IRDLOG_HACK, 0);  // don't let scc call irdalog anymore
#endif

    fpDevice->close(this);

#define THREAD_FREE(x) do { if (x) {               \
			    thread_call_cancel(x); \
			    thread_call_free(x);   \
			    x = NULL; } } while(0)

    THREAD_FREE(rx_thread_call);
    THREAD_FREE(tx_thread_call);
    THREAD_FREE(speed_change_thread_call);
    
#undef THREAD_FREE
	
    // release our power manager state
    PMstop();
    
	// Skip the IORS232SerialStreamSync's super classes, they aren't 'driver side'
    
    IOService::stop( provider );
    return;
    
}/* end stop */

/**
#define RevertEndianness32(X) ((X & 0x000000FF) << 24) | \
			      ((X & 0x0000FF00) << 16) | \
			      ((X & 0x00FF0000) >> 16) | \
			      ((X & 0xFF000000) >> 24)
**/
/****************************************************************************************************/
//
//      Function:   AppleSCCIrDA::enableSCC
//
//      Inputs:     None 
//
//      Outputs:    return code - true (did it), false (didn't)
//
//      Desc:       Enables the SCC cell for IrDA on channel B
//
/****************************************************************************************************/
enum {
    kFcr0Offset = 0x38
};

bool AppleSCCIrDA::enableSCC( void )
{
    IOService   *topProvider = NULL;
    IOService   *provider = NULL;
    bool    isCore99 = false;
    bool    isHeathrow = false;
    UInt8   *comp;
   
    ELG( 0, 0, 'eSCC', "enableSCC" );
    XTRACE(kLogeSCC, 0, 0);

	// See if we can find the top of the tree (with the machine type)
	// iterating all the way up:
	
    provider = fProvider;
    while (provider != NULL) 
    {
	    // Check if we are at the IOService mac-io registry entry
	    
	OSData *s = OSDynamicCast( OSData, provider->getProperty("name") );
	if ( (s != NULL) && s->isEqualTo("mac-io", 6) ) 
	{
	    // Look to see if we have keylargo or Heathrow

	    OSData *kl = OSDynamicCast( OSData, provider->getProperty("compatible") );
	    if ( kl != NULL )
	    {
		comp = (UInt8 *)kl->getBytesNoCopy();
		LogData( kAny, 20, (char *)comp );
		if ( kl->isEqualTo("Keylargo", 8) )
		{
		    isCore99 = true;
		} else {
		    if ( (kl->isEqualTo("paddington", 10)) || (kl->isEqualTo("heathrow", 8)) )
		    {
			isHeathrow = true;
		    }
		}
	    }
	}

	// remember who is the topmost provider and iterate again
	
	topProvider = provider;
	provider = topProvider->getProvider();
    }

    if ( isHeathrow )
    {
	ELG( 0, 0, 'eSCH', "enableSCC - Heathrow" );
	XTRACE(kLogeSCH, 0, 0);
	callPlatformFunction("EnableSCC", false, (void *)true, 0, 0, 0);
	return true;
    }

    if ( !isCore99 )
    {
	ELG( 0, 0, 'eSC-', "enableSCC - no Heathrow or Keylargo" );
	XTRACE(kLogeSCminus, 0, 0);
	return false;
    };
    
    /** this code is now part of the keylargo platform expert -- we just need to
       pass a few undocumented parameters to EnableSCC
    ***/
    
    /**
    
    UInt32  bitsON;
    UInt32  bitsOFF;
    UInt32  bitValues, bitMask;

    ELG( 0, 0, 'eSCK', "enableSCC - Keylargo" );
    XTRACE(kLogeSCK, 0, 0);
    
    // Enables the SCCb (irda) cell:
    
    bitsON = bitsOFF = 0;
    bitsON |= (1 << 17);    // irda 19.584 MHz clock
    bitsON |= (1 << 16);    // irda 32 mhz clock on
    bitsON |= (1 << 15);    // IrDA Enable
    bitsOFF|= (1 << 14);    // fast connect
    bitsOFF|= (1 << 13);    // default0
    bitsOFF|= (1 << 12);    // default1 
				// 11 IrDA software reset   
    bitsON |= (1 << 10);    // use ir source 1
    bitsOFF|= (1 << 9);     // do not use ir source 2
    bitsOFF|= (1 << 8);     // high band for 1mbit.  0 for low speed?
				// 7 VIA
    bitsON |= (1 << 6);     // SccEnable.  Enables SCC
    bitsON |= (1 << 5);     // SccBEnable. Enables SCC B
   // bitsON |= (1 << 4);       // SccAEnable. Enables SCC A
				// 3 software reset of scc
    bitsOFF|= (1 << 2);     // SlowPCLK.    0 for SCCPCLK at 24.576 MHZ, 1 for 15.6672 MHz
  //  bitsON |= (1 << 1);       // ChooseSCCA.  0 for I2S1, 1 for SCCA
    bitsOFF|= (1 << 0);     // ChooseSCCB.  0 for IrDA hardware, 1 for SCC interface pins
    
    bitValues = bitsON;
    bitMask   = bitsON | bitsOFF;
    callPlatformFunction("keyLargo_safeWriteRegUInt32", true,
			(void *)kFcr0Offset, (void *)bitMask, (void *)bitValues, 0);


    if (1)              // Now reset the SCC
    {
	bitMask = (1 << 3);     // Resets the SCC
	callPlatformFunction("keyLargo_safeWriteRegUInt32", true,
			    (void *)kFcr0Offset, (void *)bitMask, (void *)bitMask, 0);


	IOSleep(15);

	callPlatformFunction("keyLargo_safeWriteRegUInt32", true,
			    (void *)kFcr0Offset, (void *)bitMask, 0, 0);

    }

    if (1)      // And finally reset the IrDA cell
    {
	bitMask = (1 << 11);    // Resets FIReworks

	callPlatformFunction("keyLargo_safeWriteRegUInt32", true,
			    (void *)kFcr0Offset, (void *)bitMask, (void *)bitMask, 0);


	IOSleep(15);

	callPlatformFunction("keyLargo_safeWriteRegUInt32", true,
			    (void *)kFcr0Offset, (void *)bitMask, 0, 0);


    }
    
    ***/
    
    // keylargo enablescc call
    // param1 == enable/disable
    // param2 == 1 if sccb, else scca
    // param3 == 1 if irda, else sccb serial port
    // parma4 == unused
    callPlatformFunction("EnableSCC", false, (void *)true, (void *)1, (void *)1, 0);

    
    return true;
    
}/* end enableSCC */

/****************************************************************************************************/
//
//      Function:   AppleSCCIrDA::disableSCC
//
//      Inputs:     None 
//
//      Outputs:    return code - true (did it), false (didn't)
//
//      Desc:       Enables the SCC cell for IrDA on channel B
//
/****************************************************************************************************/
	
bool AppleSCCIrDA::disableSCC( void )
{
    IOService   *topProvider = NULL;
    IOService   *provider = NULL;
   //IOMemoryMap    *macIOBase;
   //UInt8  *ioPhisicalBaseAddress = NULL;
    bool    isCore99 = false;
    bool    isHeathrow = false;
    UInt8   *comp;
   
    ELG( 0, 0, 'dSCC', "disableSCC" );
    XTRACE(kLogdSCC, 0, 0);

    // See if we can find the top of the tree (with the machine type)
    // iterating all the way up:
	
    provider = fProvider;
    while (provider != NULL) 
    {
	// Check if we are at the IOService mac-io registry entry
	    
	OSData *s = OSDynamicCast( OSData, provider->getProperty("name") );
	if ( (s != NULL) && s->isEqualTo("mac-io", 6) ) 
	{
	    //if ( (macIOBase = provider->mapDeviceMemoryWithIndex( 0 )) )
	    //{
	    //   ioPhisicalBaseAddress = (UInt8*)macIOBase->getPhysicalAddress();
	    //}
	     
		// Look to see if we have keylargo or Heathrow

	    OSData *kl = OSDynamicCast( OSData, provider->getProperty("compatible") );
	    if ( kl != NULL )
	    {
		comp = (UInt8 *)kl->getBytesNoCopy();
		LogData( kAny, 20, (char *)comp );
		if ( kl->isEqualTo("Keylargo", 8) )
		{
		    isCore99 = true;
		} else {
		    if ( (kl->isEqualTo("paddington", 10)) || (kl->isEqualTo("heathrow", 8)) )
		    {
			isHeathrow = true;
		    }
		}
	    }
	}

	// remember who is the topmost provider and iterate again
	
	topProvider = provider;
	provider = topProvider->getProvider();
    }

    if ( isHeathrow )
    {
	ELG( 0, 0, 'dSCH', "disableSCC - Heathrow" );
	XTRACE(kLogdSCH, 0, 0);
	// disable scc -- this is a big, fat, nop!  and if it
	// wasn't, then it'd tread on the modem's toes
	callPlatformFunction("EnableSCC", false, (void *)false, 0, 0, 0);
	return true;
    }

    if ( !isCore99 )
    {
	ELG( 0, 0, 'dSC-', "disableSCC - no Heathrow or Keylargo" );
	XTRACE(kLogdSCminus, 0, 0);
	return false;
    };
    
    /**** moved to platform expert
    
    UInt32  bitsON, bitsOFF;
    UInt32 bitValues, bitMask;

    ELG( 0, 0, 'dSCK', "disableSCC - Keylargo" );
    XTRACE(kLogdSCK, 0, 0);
    
	// disables the SCC cell:
    
    bitsON = bitsOFF = 0;
    bitsOFF |= (1 << 17);   // irda 19.584 MHz clock
    bitsOFF |= (1 << 16);   // irda 32 mhz clock on
    bitsOFF |= (1 << 15);   // IrDA Enable
    
    bitsOFF |= (1 << 14);   // fast connect
    bitsOFF |= (1 << 13);   // default0
    bitsOFF |= (1 << 12);   // default1 
				// 11 IrDA software reset   
    bitsOFF |= (1 << 10);       // use ir source 1
    bitsOFF |= (1 << 9);        // do not use ir source 2
    bitsOFF |= (1 << 8);        // high band for 1mbit.  0 for low speed?
				// 7 VIA
   // bitsON |= (1 << 6);       // SccEnable.  Enables SCC
    bitsOFF |= (1 << 5);        // SccBEnable. Enables SCC B
   // bitsON |= (1 << 4);       // SccAEnable. Enables SCC A
				// 3 software reset of scc
    bitsOFF |= (1 << 2);        // SlowPCLK.    0 for SCCPCLK at 24.576 MHZ, 1 for 15.6672 MHz
   // bitsON |= (1 << 1);       // ChooseSCCA.  0 for I2S1, 1 for SCCA
   // bitsOFF|= (1 << 0);       // ChooseSCCB.  0 for IrDA hardware, 1 for SCC interface pins
	
    bitValues = bitsON;
    bitMask   = bitsON | bitsOFF;
    callPlatformFunction("keyLargo_safeWriteRegUInt32", true,
			(void *)kFcr0Offset, (void *)bitMask, (void *)bitValues, 0);
    ****/
    
    // keylargo enablescc call
    // param1 == enable/disable
    // param2 == 1 if sccb, else scca
    // param3 == 1 if irda, else sccb serial port
    // parma4 == unused
    callPlatformFunction("EnableSCC", false, (void *)false, (void *)1, (void *)1, 0);

    
    return true;
    
}/* end disableSCC */

//
// initForPM
//
// Add ourselves to the power management tree so we
// can do the right thing on sleep/wakeup.
//
bool AppleSCCIrDA::initForPM(IOService * provider)
{
    XTRACE(kLogInitForPM, 0, 0);
    
    fPowerState = kIrDAPowerOnState;        // init our power state to be 'on'
    PMinit();                               // init power manager instance variables
    provider->joinPMtree(this);             // add us to the power management tree
    require(pm_vars != NULL, Fail);

    // register ourselves with ourself as policy-maker
    registerPowerDriver(this, gOurPowerStates, kNumIrDAStates);
    return true;
    
Fail:
    return false;
}

//
// request for our initial power state
//
unsigned long AppleSCCIrDA::initialPowerStateForDomainState ( IOPMPowerFlags flags)
{
    XTRACE(kLogInitialPowerState, flags >> 16, (short)flags);
    return fPowerState;
}

//
// request to turn device on or off
//
IOReturn AppleSCCIrDA::setPowerState(unsigned long powerStateOrdinal, IOService * whatDevice)
{
    XTRACE(kLogSetPowerState, 0, powerStateOrdinal);
    
    require(powerStateOrdinal == kIrDAPowerOffState || powerStateOrdinal == kIrDAPowerOnState, Fail);

    if (powerStateOrdinal == fPowerState)
	return IOPMAckImplied;

    fPowerState = powerStateOrdinal;
    CheckIrDAState();
    
    return IOPMNoErr;

Fail:
    return IOPMNoSuchState;
}

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::sendBaud
//
//      Inputs:     brate - the requested baud rate
//
//      Outputs:    return code - kIOReturnSuccess and others
//
//      Desc:       Send the baudrate to the SCC driver 
//
/****************************************************************************************************/  

IOReturn AppleSCCIrDA::sendBaud( UInt32 baud )
{
    IOReturn rtn = kIOReturnSuccess;

    ELG( 0, baud, 'snBd', "sendBaud" );
    XTRACE(kLogsnBd, baud >> 16, baud);
	
    if (baud < 115200)
    {
	rtn = fpDevice->executeEvent( PD_E_DATA_RATE, (baud << 1) );
    } else {
	rtn = fpDevice->executeEvent( PD_E_EXTERNAL_CLOCK_MODE, 0);
    }
    
    if ( rtn != kIOReturnSuccess )
    {
	ELG( baud, rtn, 'snB-', "sendBaud - execute event (baud rate) failed" );
	XTRACE(kLogsnBminus, rtn >> 16, rtn);
    }
    
    return rtn;
    
}/* end sendBaud */

/****************************************************************************************************/
//
//      Function:   AppleSCCIrDA::sendCommand
//
//      Inputs:     command - the command to be sent 
//
//      Outputs:    result - result returned from the cell
//              return code - kIOReturnSuccess, kIOReturnIOError and others
//
//      Desc:       Set up and send the command to the FIReworks cell
//
/****************************************************************************************************/
	
IOReturn AppleSCCIrDA::sendCommand( UInt8 command, UInt8* result )
{
    IOReturn    rtn = kIOReturnSuccess;
    UInt32  tCount = 0;
    UInt8   tbuff[8];
    
    ELG( 0, 0, 'snCd', "sendCommand" );
    XTRACE(kLogsnCd, 0, command);
    
    fSendingCommand = true;     // flag to rx thread to *not* consume data
    
    if (result)
	*result = 0;        // init return zero
    
    fpDevice->setState( PD_RS232_S_DTR, PD_RS232_S_DTR );
    sendBaud( kDefaultRawBaudRate );

    rtn = fpDevice->enqueueData( &command, 1, &tCount, false );
    if ( rtn != kIOReturnSuccess )
    {
	ELG( command, rtn, 'sCe-', "sendCommand - enqueue data failed" );
	XTRACE(kLogsCeminus, rtn >> 16, rtn);
    } else {
	// could do a watchState here, but we want to make sure we drain
	// the fifo and not just get the first byte of noise
	IOSleep( 50 );      // fine tune timer?
    
	rtn = fpDevice->dequeueData( tbuff, sizeof(tbuff), &tCount, 0 );
	if ( rtn == kIOReturnSuccess )
	{
	    XTRACE(kLogsCtC, tCount, tbuff[tCount-1]);
	    ELG( tCount, tbuff[tCount-1], 'sCtC', "sendCommand - dequeue data" );
	    if ( tCount == 0 )
	    {
		XTRACE(kLogsCnminus, 0, 0);
		ELG( command, 0, 'sCn-', "sendCommand - dequeue data, no data returned" );
		rtn = kIOReturnIOError;
	    } else {
		if (result)
		    *result = tbuff[tCount-1];  // Could have leading garbage because of the DTR wiggle so we take the last byte read
		XTRACE(kLogsCnplus, 0, *result);
		ELG( command, *result, 'sCn+', "sendCommand - dequeue data, successful" );
	    }
	} else {
	    ELG( command, rtn, 'sCd-', "sendCommand - dequeue data failed" );
	    XTRACE(kLogsCdminus, rtn >> 16, rtn);
	}
    }

    fpDevice->setState( 0, PD_RS232_S_DTR );
    sendBaud( fBaudCode );
    
    fSendingCommand = false;        // let rcv thread consume data again
    
    return rtn;

}/* end sendCommand */

/****************************************************************************************************/
//
//      Function:   AppleSCCIrDA::getFIReWorksVersion
//
//      Inputs:     None 
//
//      Outputs:    return code - kIOReturnSuccess, kIOReturnIOError and others
//
//      Desc:       Gets the version of the FIReworks cell in the SCC
//              (part of initialization)
//
/****************************************************************************************************/
	
IOReturn AppleSCCIrDA::getFIReWorksVersion( void )
{
    UInt8   getversion = 0x01;
    IOReturn    rtn = kIOReturnSuccess;
    
    ELG( 0, 0, 'gFWV', "getFIReWorksVersion" );
    XTRACE(kLoggFWV, 0, 0);

    rtn = sendCommand( getversion, fInBuffer );
    if ( rtn == kIOReturnSuccess )
    {
	ELG( 0, fInBuffer[0], 'gFd+', "getFIReWorksVersion - dequeue data (getversion) successful" );
	XTRACE(kLoggFdplus, 0, fInBuffer[0]);
    }

    return rtn;
    
}/* end getFIReWorksVersion */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::resetDTR
//
//      Inputs:     
//
//      Outputs:    
//
//      Desc:       Reset and wiggle the DTR line
//              Holding DTR high over 100ms resets the FIReworks chip
//
/****************************************************************************************************/

void AppleSCCIrDA::resetDTR()
{
    ELG( 0, 0, 'rDTR', "resetDTR" );
    XTRACE(kLogrDTR, 0, 0);

    sendBaud( kDefaultRawBaudRate );
    fpDevice->setState( PD_RS232_S_DTR, PD_RS232_S_DTR );
    IOSleep(101);           // Actually 100 for the reset +1 for fudge
    fpDevice->setState( 0, PD_RS232_S_DTR );
    sendBaud( fBaudCode );
    
}/* end resetDTR */



/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::setupDevice
//
//      Inputs:     
//
//      Outputs:    IOReturn - kIOReturnSuccess, various others
//
//      Desc:       Set up and open the port
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::setupDevice()
{
    IOReturn    rtn = kIOReturnSuccess;
    
    ELG( 0, 0, 'suDv', "setupDevice" );
    XTRACE(kLogsuDv, 0, 0);
    
    rtn = fpDevice->acquirePort( false );           // Get the port on behalf of IrDA (so we can do the discovers)

    if ( rtn != kIOReturnSuccess )
    {
	ELG( 0, rtn, 'sDp-', "setupDevice - acquirePort failed" );
	XTRACE(kLogsDpminus, rtn >> 16, rtn);
    } else {

#ifdef Trace_SCCSerial
	rtn = fpDevice->executeEvent(IRDLOG_HACK, (UInt32)IrDALogAdd);
	if (rtn == kIOReturnSuccess)
	{
	    IOLog("told serial of irdalog %d\n", rtn);
	} else {
	    IOLog("scc serial didn't know of irdalog hack\n");
	}
#endif

	if ( !enableSCC() )
	{
	    ELG( 0, 0, 'sDe-', "setupDevice - enableSCC failed" );
	    XTRACE(kLogsDeminus, 0, 0);
	    rtn = kIOReturnIOError;
	} else {
		// Set some defaults
	
	    //SetSpeed( kDefaultBaudRate );     // too soon for this
	    fBaudCode = 9600;                   // but set line speed for SendCommand to revert back to

	    rtn = fpDevice->executeEvent( PD_E_DATA_SIZE, (kDefaultDataSize << 1) );
	    if ( rtn != kIOReturnSuccess )
	    {
		ELG( kDefaultDataSize, rtn, 'sDd-', "setupDevice - execute event (data size) failed" );
		XTRACE(kLogsDdminus, rtn >> 16, rtn);
	    } else {
		rtn = fpDevice->executeEvent( PD_E_DATA_INTEGRITY, PD_RS232_PARITY_NONE );
		if ( rtn != kIOReturnSuccess )
		{
		    ELG( PD_RS232_PARITY_NONE, rtn, 'sDi-', "setupDevice - execute event (data integrity) failed" );
		    XTRACE(kLogsDiminus, rtn >> 16, rtn);
		} else {
		    rtn = fpDevice->executeEvent( PD_RS232_E_STOP_BITS, (kDefaultStopBits << 1) );
		    if ( rtn != kIOReturnSuccess )
		    {
			ELG( kDefaultStopBits, rtn, 'sDs-', "setupDevice - execute event (stop bits) failed" );
			XTRACE(kLogsDsminus, rtn >> 16, rtn);
		    } else {
			rtn = fpDevice->executeEvent( PD_E_ACTIVE, true );
			if ( rtn != kIOReturnSuccess )
			{
			    ELG( true, rtn, 'sDa-', "setupDevice - execute event (active) failed" );
			    XTRACE(kLogsDaminus, rtn >> 16, rtn);
			}
			else {
			    // notes: both calling acquirePort and executing PD_E_ACTIVE in
			    // the current implementation will reset this back to the default.
			    // see speed_change_thread for where this is really set
			    rtn = fpDevice->executeEvent(PD_E_DATA_LATENCY, 1);
			    check(rtn == kIOReturnSuccess);
			}
		    }
		}
	    }
	}   
    }
    
    if ( rtn != kIOReturnSuccess )
    {
	fpDevice->releasePort();
    }
    
    return rtn;

}/* end setupDevice */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::configureDevice
//
//      Inputs:     
//
//      Outputs:    return Code - true (device configured), false (device not configured)
//
//      Desc:       Sets up and configure the SCC
//
/****************************************************************************************************/

bool AppleSCCIrDA::configureDevice( void )
{
    IOReturn    rtn = kIOReturnSuccess;
       
    ELG( 0, 0, 'cDev', "configureDevice" );
    XTRACE(kLogcDev, 0, 0);
	    
    /* Set some defaults - qos values need tuning of course */
	
    fQoS.version = 0x100;
    fQoS.datasize = 0x3f;       // 1k = 1f, 2k = 3f
    fQoS.windowsize = 1;
    fQoS.minturn = 2;           // review & tune.
    fQoS.baud1 = 0x00;          // 4mbit no mir, all sir
    fQoS.baud2 = 0x3e;          // 3e for 115k thru 9600
    fQoS.bofs = 4;          // review and tune.
    fQoS.sniff = 0;
    fQoS.unicast = 0;

    SetBofCount(10);                // start with about 10 bofs (sets fBofsCode)
    
    ::gen_crc16_table();            // Initialize the crc 16 table (for SIR mode)
    ::gen_crc32_table();            // Initialize the crc 32 table (for FIR mode)
    
    if ( !fInBuffer )
    {
	fInBuffer = (UInt8 *)IOMalloc( SCCLapPayLoad );
	
	if ( !fInBuffer )
	{
	    ELG( 0, 0, 'cDi-', "configureDevice - input buffer allocation failed" );
	    XTRACE(kLogcDiminus, 0, 0);
	    return false;
	}
	bzero( fInBuffer, SCCLapPayLoad );
    }
    
    if ( !fOutBuffer )
    {
	fOutBuffer = (UInt8 *)IOMalloc( SCCLapPayLoad );
	
	if ( !fOutBuffer )
	{
	    ELG( 0, 0, 'cDo-', "configureDevice - output buffer allocation failed" );
	    XTRACE(kLogcDominus, 0, 0);
	    IOFree( fInBuffer, SCCLapPayLoad );
	    fInBuffer = NULL;
	    return false;
	}
	bzero( fOutBuffer, SCCLapPayLoad );
    }
    
    if ( !fInUseBuffer )
    {
	fInUseBuffer = (UInt8 *)IOMalloc( SCCLapPayLoad );
	
	if ( !fInUseBuffer )
	{
	    ELG( 0, 0, 'cDu-', "configureDevice - in use buffer allocation failed" );
	    XTRACE(kLogcDuminus, 0, 0);
	    IOFree( fInBuffer, SCCLapPayLoad );
	    IOFree( fOutBuffer, SCCLapPayLoad );
	    fInBuffer = NULL;
	    fOutBuffer = NULL;
	    return false;
	}
	bzero( fInUseBuffer, SCCLapPayLoad );
    }
    
    fDataLength = 0;
	
    if (!allocateRingBuffer(&(fPort->TX), kMaxCirBufferSize) ||
	!allocateRingBuffer(&(fPort->RX), kMaxCirBufferSize)) 
    {
	ELG( 0, 0, 'cDa-', "configureDevice - allocate ring buffer failed" );
	XTRACE(kLogcDaminus, 0, 0);
	IOFree( fInBuffer, SCCLapPayLoad );
	IOFree( fOutBuffer, SCCLapPayLoad );
	IOFree( fInUseBuffer, SCCLapPayLoad );
	fInBuffer = NULL;
	fOutBuffer = NULL;
	fInUseBuffer = NULL;
	return false;
    }

    rtn = setupDevice();
    if ( rtn == kIOReturnSuccess )
    {
	// verify that we can talk to the hardware by reading the
	// FIReworks version number
	resetDTR();             // really reset the FIR cell

	//rtn = getFIReWorksVersion();          // won't work with workloops
	if ( rtn != kIOReturnSuccess )
	{
	    ELG( 0, rtn, 'cDF-', "configureDevice - getFIReWorksVersion failed" );
	    XTRACE(kLogcDFminus, rtn >> 16, rtn);
	} else {
	    
	    // Now initialze IrDA and set the name for this port 
	
	    fIrDA = IrDAComm::irDAComm( this, fUserClientNub);  // create and init in one call  
	    if ( !fIrDA )
	    {  
		ELG( 0, 0, 'cDI-', "configureDevice - IrDA initialize failed" );
		XTRACE(kLogcDIminus, 0, 0);
		fpDevice->releasePort();
		rtn = kIOReturnIOError;
	    }
	    else {
		bool rc;
		rc = thread_call_enter(rx_thread_call);     // start the read loop
		check(rc == false);
		return true;
	    }
	}
    }

    //killIO(); 
    IOFree( fInBuffer, SCCLapPayLoad );
    IOFree( fOutBuffer, SCCLapPayLoad );
    IOFree( fInUseBuffer, SCCLapPayLoad );
    fInBuffer = NULL;
    fOutBuffer = NULL;
    fInUseBuffer = NULL;
    
    freeRingBuffer( &fPort->TX );
    freeRingBuffer( &fPort->RX );
    ELG( 0, rtn, 'cDv-', "configureDevice - failed" );
    XTRACE(kLogcDvminus, rtn >> 16, rtn);
    return false;

}/* end configureDevice */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::createSerialStream
//
//      Inputs:     
//
//      Outputs:    return Code - true (created and initialilzed ok), false (it failed)
//
//      Desc:       Sets the name and registers the service
//
/****************************************************************************************************/

bool AppleSCCIrDA::createSerialStream()
{
    
    ELG( 0, 0, 'cSSt', "createSerialStream" );
    XTRACE(kLogcSSt, 0, 0);

	// Report the base name to be used for generating device nodes
    
    setProperty( kIOTTYBaseNameKey, baseName );
    setProperty( kIOTTYSuffixKey, SCCsuffix );          // Need to chnage this
    registerService();  
    return true;
	
}/* end createSerialStream */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::acquirePort
//
//      Inputs:     sleep - true (wait for it), false (don't)
//
//      Outputs:    Return Code - from provider acquirePort
//
//      Desc:       Acquires the IrDA port
//              The first aquirePort is done on behalf of IrDA (and sent to the SCC driver). 
//              The first real (application) aquirePort is not sent on to the SCC driver, 
//              all subsequent ones are returned with kIOReturnExclusiveAccess for the moment.
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::acquirePort(bool sleep)
{
    PortInfo_t          *port = fPort;
    UInt32              busyState = 0;
    IOReturn            rtn = kIOReturnSuccess;

    ELG( port, sleep, 'acqP', "acquirePort" );
    XTRACE(kLogAcquirePort, 0, 0);
    
    if ( fTerminate ) {
	int review_fTerminate;
	//return kIOReturnOffline;
    }
    SetStructureDefaults( port, FALSE );    /* Initialize all the structures */
    
    for (;;)
    {
	XTRACE(kLogAcquirePort, 1, 1);
	busyState = readPortState( port ) & PD_S_ACQUIRED;
	if ( !busyState )
	{       
	    // Set busy bit, and clear everything else
	    XTRACE(kLogAcquirePort, 2, 2);
	    changeState( port, (UInt32)PD_S_ACQUIRED | DEFAULT_STATE, (UInt32)STATE_ALL);
	    break;
	} else {
	    if ( !sleep )
	    {
		XTRACE(kLogAcquirePort, 3, 3);
		ELG( 0, 0, 'busy', "acquirePort - Busy exclusive access" );
		return kIOReturnExclusiveAccess;
	    } else {
		XTRACE(kLogAcquirePort, 4, 4);
		busyState = 0;
		rtn = watchState( &busyState, PD_S_ACQUIRED);
		XTRACE(kLogAcquirePort, 5, 5);
		if ( (rtn == kIOReturnIOError) || (rtn == kIOReturnSuccess) )
		{
		    continue;
		} else {
		    ELG( 0, 0, 'int-', "acquirePort - Interrupted!" );
		    return rtn;
		}
	    }
	}
    } /* end for */
    
    fSessions++;    //bump number of active sessions and turn on clear to send
    XTRACE(kLogAcquirePort, 6, 6);
    //changeState( port, PD_RS232_S_CTS, PD_RS232_S_CTS);
    changeState( port, PD_RS232_S_CTS | PD_RS232_S_CAR, PD_RS232_S_CTS | PD_RS232_S_CAR);

    CheckIrDAState();       // turn irda on/off if appropriate
    
    XTRACE(kLogAcquirePort,7, 7);
    if (1) {                // wait for initial connect
	int counter = 0;
	while (fIrDA && fIrDA->Starting()) {
	    counter++;          // debugging 
	    IOSleep(100);
	    XTRACE(kLogAcquirePort, 8, counter);
	    if (counter > (10*10)) {		// 10 second max on hanging open
		//IOLog("sccirda: open timed out\n");
		break;
	    }
	}
	//IOLog("AppleUSBIrDA: acquire port paused %d ms for initial connection\n", counter*100);
    }
    
    XTRACE(kLogAcquirePort, 0xffff, 0xffff);
    return rtn;
    
}/* end acquirePort */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::releasePort
//
//      Inputs:     
//
//      Outputs:    Return Code - kIOReturnSuccess and various others
//
//      Desc:       Releases the IrDA port and does clean up.
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::releasePort()
{
    PortInfo_t          *port = fPort;
    UInt32              busyState;

    ELG( 0, port, 'relP', "releasePort" );
    XTRACE(kLogReleasePort, 0, 0);
    
    busyState = (readPortState( port ) & PD_S_ACQUIRED);
    if ( !busyState )
    {
	ELG( 0, 0, 'rlP-', "releasePort - NOT OPEN" );
	return kIOReturnNotOpen;
    }
    
    changeState( port, 0, (UInt32)STATE_ALL );  // Clear the entire state word which also deactivates the port

    fSessions--;        // reduce number of active sessions
    CheckIrDAState();   // turn irda off if appropriate
    
    ELG( 0, 0, 'RlP+', "releasePort - OK" );
    
    return kIOReturnSuccess;
}/* end releasePort */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::setState
//
//      Inputs:     state - state to set
//              mask - state mask
//
//      Outputs:    Return Code - from provider setState
//
//      Desc:       Sets the state for the port device.
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::setState( UInt32 state, UInt32 mask )
{
    PortInfo_t *port = fPort;
    
    ELG( state, mask, 'stSt', "setState" );
    XTRACE(kLogSetState1, state >> 16, (short)state);
    XTRACE(kLogSetState2, mask >> 16, (short)mask);
    
    if ( mask & (PD_S_ACQUIRED | PD_S_ACTIVE | (~EXTERNAL_MASK)) )
	return kIOReturnBadArgument;

    if ( readPortState( port ) & PD_S_ACQUIRED )
    {
	    // ignore any bits that are read-only
	mask &= (~port->FlowControl & PD_RS232_A_MASK) | PD_S_MASK;

	if ( mask)
	    changeState( port, state, mask );

	return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
}/* end setState */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::getState
//
//      Inputs:     
//
//      Outputs:    state - port state
//
//      Desc:       Get the state for the port.
//
/****************************************************************************************************/

UInt32 AppleSCCIrDA::getState()
{
    PortInfo_t  *port = fPort;
    UInt32      state;
    
    ELG( 0, port, 'gtSt', "getState" );
    
    CheckQueues(port);
	
    state = readPortState(port) & EXTERNAL_MASK;
    
    ELG( state, EXTERNAL_MASK, 'gtS-', "getState-->State" );
    XTRACE(kLogGetState, state >> 16, (short)state);
    
    return state;
    
}/* end getState */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::watchState
//
//      Inputs:     state - state to watch for
//              mask - state mask bits
//
//      Outputs:    Return Code - from provider watchState
//
//      Desc:       Wait for state bit changes
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::watchState( UInt32 *state, UInt32 mask )
{
    PortInfo_t  *port = fPort;
    IOReturn    ret = kIOReturnNotOpen;

    ELG( *state, mask, 'WatS', "watchState" );
    XTRACE(kLogWatchState1, state >> 16, (short)state);
    XTRACE(kLogWatchState2, mask >> 16, (short)mask);

    if ( readPortState( port ) & PD_S_ACQUIRED )
    {
	ret = kIOReturnSuccess;
	mask &= EXTERNAL_MASK;
	ret = privateWatchState( port, state, mask );
	*state &= EXTERNAL_MASK;
    }
    
    ELG( ret, 0, 'WatS', "watchState --> watchState" );
    XTRACE(kLogWatchState, 0xffff, 0xffff);
    return ret;
    
}/* end watchState */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::nextEvent
//
//      Inputs:     
//
//      Outputs:    Return Code - from provider nextEvent
//
//      Desc:       Get the next event
//
/****************************************************************************************************/

UInt32 AppleSCCIrDA::nextEvent()
{
    UInt32  rtn = kIOReturnSuccess;

    ELG( 0, 0, 'NxtE', "nextEvent" );

    return rtn;
    
}/* end nextEvent */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::executeEvent
//
//      Inputs:     event - The event
//              data - any data associated with the event
//
//      Outputs:    Return Code - from provider executeEvent
//
//      Desc:       executeEvent causes the specified event to be processed immediately.
//              This is primarily used for channel control commands like START & STOP
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::executeEvent( UInt32 event, UInt32 data )
{
    PortInfo_t  *port = fPort;
    IOReturn    ret = kIOReturnSuccess;
    UInt32      state, delta;
    
    delta = 0;
    state = readPortState( port );  
    ELG( port, state, 'ExIm', "executeEvent" );
    XTRACE(kLogExecEvent, event >> 16, (short)event);
    XTRACE(kLogExecEventData, data >> 16, (short)data);
    
    if ( (state & PD_S_ACQUIRED) == 0 )
	return kIOReturnNotOpen;

    switch ( event )
    {
    case PD_RS232_E_XON_BYTE:
	ELG( data, event, 'ExIm', "executeEvent - PD_RS232_E_XON_BYTE" );
	port->XONchar = data;
	break;
    case PD_RS232_E_XOFF_BYTE:
	ELG( data, event, 'ExIm', "executeEvent - PD_RS232_E_XOFF_BYTE" );
	port->XOFFchar = data;
	break;
    case PD_E_SPECIAL_BYTE:
	ELG( data, event, 'ExIm', "executeEvent - PD_E_SPECIAL_BYTE" );
	port->SWspecial[ data >> SPECIAL_SHIFT ] |= (1 << (data & SPECIAL_MASK));
	break;

    case PD_E_VALID_DATA_BYTE:
	ELG( data, event, 'ExIm', "executeEvent - PD_E_VALID_DATA_BYTE" );
	port->SWspecial[ data >> SPECIAL_SHIFT ] &= ~(1 << (data & SPECIAL_MASK));
	break;

    case PD_E_FLOW_CONTROL:
	ELG( data, event, 'ExIm', "executeEvent - PD_E_FLOW_CONTROL" );
	break;

    case PD_E_ACTIVE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_ACTIVE" );
	if ( (bool)data )
	{
	    if ( !(state & PD_S_ACTIVE) )
	    {
		SetStructureDefaults( port, FALSE );
		changeState( port, (UInt32)PD_S_ACTIVE, (UInt32)PD_S_ACTIVE ); // activate port
	    }
	} else {
	    if ( (state & PD_S_ACTIVE) )
	    {
		changeState( port, 0, (UInt32)PD_S_ACTIVE );
	    }
	}
	break;

    case PD_E_DATA_LATENCY:
	ELG( data, event, 'ExIm', "executeEvent - PD_E_DATA_LATENCY" );
	port->DataLatInterval = long2tval( data * 1000 );
	break;

    case PD_RS232_E_MIN_LATENCY:
	ELG( data, event, 'ExIm', "executeEvent - PD_RS232_E_MIN_LATENCY" );
	port->MinLatency = bool( data );
	break;

    case PD_E_DATA_INTEGRITY:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_DATA_INTEGRITY" );
	if ( (data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE))
	{
	    ret = kIOReturnBadArgument;
	}
	else
	{
	    port->TX_Parity = data;
	    port->RX_Parity = PD_RS232_PARITY_DEFAULT;          
	}
	break;

    case PD_E_DATA_RATE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_DATA_RATE" );
	    /* For API compatiblilty with Intel.    */
	data >>= 1;
	ELG( 0, data, 'Exlm', "executeEvent - actual data rate" );
	if ( (data < kMinBaudRate) || (data > kMaxBaudRate) )       // Do we really care
	    ret = kIOReturnBadArgument;
	else
	{
	    port->BaudRate = data;
	}       
	break;

    case PD_E_DATA_SIZE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_DATA_SIZE" );
	    /* For API compatiblilty with Intel.    */
	data >>= 1;
	ELG( 0, data, 'Exlm', "executeEvent - actual data size" );
	if ( (data < 5) || (data > 8) )
	    ret = kIOReturnBadArgument;
	else
	{
	    port->CharLength = data;            
	}
	break;

    case PD_RS232_E_STOP_BITS:
	ELG( data, event, 'Exlm', "executeEvent - PD_RS232_E_STOP_BITS" );
	if ( (data < 0) || (data > 20) )
	    ret = kIOReturnBadArgument;
	else
	{
	    port->StopBits = data;
	}
	break;

    case PD_E_RXQ_FLUSH:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RXQ_FLUSH" );
	break;

    case PD_E_RX_DATA_INTEGRITY:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RX_DATA_INTEGRITY" );
	if ( (data != PD_RS232_PARITY_DEFAULT) &&  (data != PD_RS232_PARITY_ANY) )
	    ret = kIOReturnBadArgument;
	else
	    port->RX_Parity = data;
	break;

    case PD_E_RX_DATA_RATE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RX_DATA_RATE" );
	if ( data )
	    ret = kIOReturnBadArgument;
	break;

    case PD_E_RX_DATA_SIZE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RX_DATA_SIZE" );
	if ( data )
	    ret = kIOReturnBadArgument;
	break;

    case PD_RS232_E_RX_STOP_BITS:
	ELG( data, event, 'Exlm', "executeEvent - PD_RS232_E_RX_STOP_BITS" );
	if ( data )
	    ret = kIOReturnBadArgument;
	break;

    case PD_E_TXQ_FLUSH:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_TXQ_FLUSH" );
	break;

    case PD_RS232_E_LINE_BREAK:
	ELG( data, event, 'Exlm', "executeEvent - PD_RS232_E_LINE_BREAK" );
	state &= ~PD_RS232_S_BRK;
	delta |= PD_RS232_S_BRK;
	break;

    case PD_E_DELAY:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_DELAY" );
	port->CharLatInterval = long2tval(data * 1000);
	break;
	
    case PD_E_RXQ_SIZE:
	ELG( 0, event, 'Exlm', "executeEvent - PD_E_RXQ_SIZE" );
	break;

    case PD_E_TXQ_SIZE:
	ELG( 0, event, 'Exlm', "executeEvent - PD_E_TXQ_SIZE" );
	break;

    case PD_E_RXQ_HIGH_WATER:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RXQ_HIGH_WATER" );
	break;

    case PD_E_RXQ_LOW_WATER:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RXQ_LOW_WATER" );
	break;

    case PD_E_TXQ_HIGH_WATER:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_TXQ_HIGH_WATER" );
	break;

    case PD_E_TXQ_LOW_WATER:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_TXQ_LOW_WATER" );
	break;

    default:
	ELG( data, event, 'Exlm', "executeEvent - unrecognized event" );
	ret = kIOReturnBadArgument;
	break;
    }

    state |= state;/* ejk for compiler warnings. ?? */
    changeState( port, state, delta );
    
    return ret;
    
}/* end executeEvent */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::requestEvent
//
//      Inputs:     event - The event
//
//      Outputs:    Return Code - from provider requestEvent
//              data - any data associated with the event
//
//      Desc:       requestEvent processes the specified event as an immediate request and
//              returns the results in data.  This is primarily used for getting link
//              status information and verifying baud rate and such.
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::requestEvent( UInt32 event, UInt32 *data )
{
    PortInfo_t  *port = fPort;
    IOReturn    returnValue = kIOReturnSuccess;

    ELG( 0, readPortState( port ), 'ReqE', "requestEvent" );
    XTRACE(kLogReqEvent, event >> 16, (short)event);

    if ( data == NULL ) {
	ELG( 0, event, 'ReqE', "requestEvent - data is null" );
	returnValue = kIOReturnBadArgument;
    }
    else
    {
	XTRACE(kLogReqEventData, (*data) >> 16, (short)*data);
	switch ( event )
	{
	    case PD_E_ACTIVE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_ACTIVE" );
		*data = bool(readPortState( port ) & PD_S_ACTIVE);  
		break;
	    
	    case PD_E_FLOW_CONTROL:
		ELG( port->FlowControl, event, 'ReqE', "requestEvent - PD_E_FLOW_CONTROL" );
		*data = port->FlowControl;                          
		break;
	    
	    case PD_E_DELAY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DELAY" );
		*data = tval2long( port->CharLatInterval )/ 1000;   
		break;
	    
	    case PD_E_DATA_LATENCY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DATA_LATENCY" );
		*data = tval2long( port->DataLatInterval )/ 1000;   
		break;

	    case PD_E_TXQ_SIZE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_TXQ_SIZE" );
		*data = GetQueueSize( &port->TX );
		break;
	    
	    case PD_E_RXQ_SIZE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RXQ_SIZE" );
		*data = GetQueueSize( &port->RX );  
		break;

	    case PD_E_TXQ_LOW_WATER:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_TXQ_LOW_WATER" );
		*data = 0; 
		returnValue = kIOReturnBadArgument; 
		break;
	
	    case PD_E_RXQ_LOW_WATER:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RXQ_LOW_WATER" );
		*data = 0; 
		returnValue = kIOReturnBadArgument; 
		break;
	
	    case PD_E_TXQ_HIGH_WATER:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_TXQ_HIGH_WATER" );
		*data = 0; 
		returnValue = kIOReturnBadArgument; 
		break;
	
	    case PD_E_RXQ_HIGH_WATER:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RXQ_HIGH_WATER" );
		*data = 0; 
		returnValue = kIOReturnBadArgument; 
		break;
	
	    case PD_E_TXQ_AVAILABLE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_TXQ_AVAILABLE" );
		*data = FreeSpaceinQueue( &port->TX );
		break;
	    
	    case PD_E_RXQ_AVAILABLE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RXQ_AVAILABLE" );
		*data = UsedSpaceinQueue( &port->RX );  
		break;

	    case PD_E_DATA_RATE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DATA_RATE" );
		*data = port->BaudRate << 1;        
		break;
	    
	    case PD_E_RX_DATA_RATE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RX_DATA_RATE" );
		*data = 0x00;                   
		break;
	    
	    case PD_E_DATA_SIZE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DATA_SIZE" );
		*data = port->CharLength << 1;  
		break;
	    
	    case PD_E_RX_DATA_SIZE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RX_DATA_SIZE" );
		*data = 0x00;                   
		break;
	    
	    case PD_E_DATA_INTEGRITY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DATA_INTEGRITY" );
		*data = port->TX_Parity;            
		break;
	    
	    case PD_E_RX_DATA_INTEGRITY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RX_DATA_INTEGRITY" );
		*data = port->RX_Parity;            
		break;

	    case PD_RS232_E_STOP_BITS:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_STOP_BITS" );
		*data = port->StopBits << 1;        
		break;
	    
	    case PD_RS232_E_RX_STOP_BITS:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_RX_STOP_BITS" );
		*data = 0x00;                   
		break;
	    
	    case PD_RS232_E_XON_BYTE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_XON_BYTE" );
		*data = port->XONchar;          
		break;
	    
	    case PD_RS232_E_XOFF_BYTE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_XOFF_BYTE" );
		*data = port->XOFFchar;         
		break;
	    
	    case PD_RS232_E_LINE_BREAK:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_LINE_BREAK" );
		*data = bool(readPortState( port ) & PD_RS232_S_BRK);
		break;
	    
	    case PD_RS232_E_MIN_LATENCY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_MIN_LATENCY" );
		*data = bool( port->MinLatency );       
		break;

	    default:
		ELG( 0, event, 'ReqE', "requestEvent - unrecognized event" );
		returnValue = kIOReturnBadArgument;             
		break;
	}
    }

    return kIOReturnSuccess;
    
}/* end requestEvent */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::enqueueEvent
//
//      Inputs:     event - The event, data - any data associated with the event, 
//              sleep - true (wait for it), false (don't)
//
//      Outputs:    Return Code - provider enqueueEvent
//
//      Desc:       Enqueue the event.  
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::enqueueEvent( UInt32 event, UInt32 data, bool sleep )
{
    PortInfo_t *port = fPort;
    
    ELG( data, event, 'EnqE', "enqueueEvent" );

    if ( readPortState( port ) & PD_S_ACTIVE )
    {
	return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
    
}/* end enqueueEvent */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::dequeueEvent
//
//      Inputs:     sleep - true (wait for it), false (don't)
//
//      Outputs:    Return Code - from provider dequeueEvent
//                  event - the event
//                  data - any associated data
//
//      Desc:       Dequeue next event.     
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::dequeueEvent( UInt32 *event, UInt32 *data, bool sleep )
{
    PortInfo_t *port = fPort;
    
    ELG( 0, 0, 'DeqE', "dequeueEvent" );

    if ( (event == NULL) || (data == NULL) )
	return kIOReturnBadArgument;

    if ( readPortState( port ) & PD_S_ACTIVE )
    {
	return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
    
}/* end dequeueEvent */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::enqueueData
//
//      Inputs:     buffer - the data
//              size - number of bytes
//              sleep - true (wait for it), false (don't),
//
//      Outputs:    Return Code - kIOReturnSuccess, kIOReturnIOError, kIOReturnBadArgument or kIOReturnNotOpen
//              count - bytes transferred,  
//
//      Desc:   Enqueue data to be sent to IrDA     
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::enqueueData( UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep )
{
    PortInfo_t  *port = fPort;
    UInt32      state = PD_S_TXQ_LOW_WATER;
    IOReturn    rtn = kIOReturnSuccess;

    ELG( 0, sleep, 'eqDt', "enqueData" );

    if ( fTerminate )
	return kIOReturnOffline;

    if ( count == NULL || buffer == NULL )
	return kIOReturnBadArgument;

    *count = 0;

    if ( !(readPortState( port ) & PD_S_ACTIVE) )
	return kIOReturnNotOpen;
    
    ELG( port->State, size, 'eqDt', "enqueData State" );    
    LogData( kUSBOut, size, buffer );

	/* OK, go ahead and try to add something to the buffer  */
    *count = AddtoQueue( &port->TX, buffer, size );
    CheckQueues( port );

	/* Let the tranmitter know that we have something ready to go   */
    SetUpTransmit( );

	/* If we could not queue up all of the data on the first pass and   */
	/* the user wants us to sleep until it's all out then sleep */

    while ( (*count < size) && sleep )
    {
	state = PD_S_TXQ_LOW_WATER;
	rtn = watchState( &state, PD_S_TXQ_LOW_WATER );
	if ( rtn != kIOReturnSuccess )
	{
	    ELG( 0, rtn, 'EqD-', "enqueueData - interrupted" );
	    return rtn;
	}

	*count += AddtoQueue( &port->TX, buffer + *count, size - *count );
	CheckQueues( port );

	/* Let the tranmitter know that we have something ready to go.  */

	SetUpTransmit( );
    }/* end while */

    ELG( *count, size, 'enqd', "enqueueData - Enqueue" );

    return kIOReturnSuccess;
    
}/* end enqueueData */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::dequeueData
//
//      Inputs:     size - buffer size
//              min - minimum bytes required
//
//      Outputs:    buffer - data returned
//              Return Code - from provider dequeueData
//
//      Desc:       dequeueData will attempt to copy data from the RX queue to the
//              specified buffer.  
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::dequeueData( UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min )
{
    PortInfo_t  *port = fPort;
    IOReturn    rtn = kIOReturnSuccess;
    UInt32      state = 0;

    ELG( size, min, 'dqDt', "dequeueData" );
    
	/* Check to make sure we have good arguments.   */
    if ( (count == NULL) || (buffer == NULL) || (min > size) )
	return kIOReturnBadArgument;

	/* If the port is not active then there should not be any chars.    */
    *count = 0;
    if ( !(readPortState( port ) & PD_S_ACTIVE) )
	return kIOReturnNotOpen;

	/* Get any data living in the queue.    */
    *count = RemovefromQueue( &port->RX, buffer, size );
    if (fIrDA)
	fIrDA->ReturnCredit( *count );      // return credit when room in the queue
    
    CheckQueues( port );

    while ( (min > 0) && (*count < min) )
    {
	int count_read;
	
	    /* Figure out how many bytes we have left to queue up */
	state = 0;

	rtn = watchState( &state, PD_S_RXQ_EMPTY );

	if ( rtn != kIOReturnSuccess )
	{
	    ELG( 0, rtn, 'DqD-', "dequeueData - Interrupted!" );
	    LogData( kUSBIn, *count, buffer );
	    return rtn;
	}
	/* Try and get more data starting from where we left off */
	count_read = RemovefromQueue( &port->RX, buffer + *count, (size - *count) );
	if (fIrDA)
	    fIrDA->ReturnCredit(count_read);        // return credit when room in the queue
	    
	*count += count_read;
	CheckQueues( port );
	
    }/* end while */

    LogData( kUSBIn, *count, buffer );

    ELG( *count, size, 'deqd', "dequeueData -->Out Dequeue" );

    return rtn;
    
}/* end dequeueData */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::SetUpTransmit
//
//      Inputs: 
//
//      Outputs:    return code - true (transmit started), false (transmission already in progress or error)
//
//      Desc:       Setup and then start transmisson
//
/****************************************************************************************************/

bool AppleSCCIrDA::SetUpTransmit( void )
{
    size_t      count = 0;
    size_t      data_Length, tCount;
    UInt8       *TempOutBuffer;

    ELG( fPort, fPort->AreTransmitting, 'upTx', "SetUpTransmit" );
    XTRACE(kLogSUTm, 0, fPort->AreTransmitting);
    
	//  If we are already in the cycle of transmitting characters,
	//  then we do not need to do anything.
	
    if ( fPort->AreTransmitting == TRUE )
	return false;

	// First check if we can actually do anything, also if IrDA has no room we're done for now

    //if ( GetQueueStatus( &fPort->TX ) != queueEmpty )
    if (UsedSpaceinQueue(&fPort->TX) > 0)
    {
	data_Length = fIrDA->TXBufferAvailable();
	if ( data_Length == 0 )
	{
	    return false;
	}
    
	if ( data_Length > MAX_BLOCK_SIZE )
	{
	    data_Length = MAX_BLOCK_SIZE;
	}
	
	TempOutBuffer = (UInt8*)IOMalloc( data_Length );
	if ( !TempOutBuffer )
	{
	    ELG( 0, 0, 'STA-', "SetUpTransmit - buffer allocation problem" );
	    return false;
	}
	bzero( TempOutBuffer, data_Length );

	// Fill up the buffer with characters from the queue

	count = RemovefromQueue( &fPort->TX, TempOutBuffer, data_Length );
	XTRACE(kLogSIWplus, fPort->State, count);
	ELG( fPort->State, count, ' Tx+', "SetUpTransmit - Sending to IrDA" );
    
	fPort->AreTransmitting = TRUE;
	changeState( fPort, PD_S_TX_BUSY, PD_S_TX_BUSY );

	tCount = fIrDA->Write( TempOutBuffer, count );      // do the "transmit" -- send to IrCOMM

	changeState( fPort, 0, PD_S_TX_BUSY );
	fPort->AreTransmitting = false;

	IOFree( TempOutBuffer, data_Length );
	if ( tCount != count )
	{
	    XTRACE(kLogSIWminus, tCount, count);
	    ELG( tCount, count, 'IrW-', "SetUpTransmit - IrDA write problem, data has been dropped" );
	    return false;
	}

	// We potentially removed a bunch of stuff from the
	// queue, so see if we can free some thread(s)
	// to enqueue more stuff.
	
	CheckQueues( fPort );
    }

    return true;
    
}/* SetUpTransmit */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::StartTransmit
//
//      Inputs:     control_length - Length of control data
//              control_buffer - Control data
//              data_length - Length of raw data
//              data_buffer - raw data  
//
//      Outputs:    Return code - kIOReturnSuccess and others
//
//      Desc:       Start the transmisson
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::StartTransmit( UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer )
{
    IOReturn    rtn = kIOReturnSuccess;
    UInt32      count = 0;
    
    ELG( control_length, data_length, 'StTx', "StartTransmit" );
    XTRACE(kLogstTx, control_length, data_length);
    
    parseInputReset();          // discard any partial input packet
    
    require(control_length + data_length > 0, Fail);        // sanity check
    
    
    count = Prepare_Buffer_For_Transmit(fOutBuffer, control_length, control_buffer, data_length, data_buffer);
    LogData( kDriverOut, count, fOutBuffer );
    XTRACE(kLogXmitCount, 0, count);
    
    thread_call_enter1(tx_thread_call, (thread_call_param_t)count);     // send it out and wait for it in another workloop
    
	
    return rtn;
    
Fail:
    return -1;
    
}/* end StartTransmit */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::SetStructureDefaults
//
//      Inputs:     port - the port to set the defaults, Init - Probe time or not
//
//      Outputs:    None
//
//      Desc:       Sets the defaults for the specified port structure
//
/****************************************************************************************************/

void AppleSCCIrDA::SetStructureDefaults( PortInfo_t *port, bool Init )
{
    UInt32  tmp;
    
    ELG( 0, 0, 'StSD', "SetStructureDefaults" );
    XTRACE(kLogSetStructureDefaults, 0, Init);

	/* These are initialized when the port is created and shouldn't be reinitialized. */
    if ( Init )
    {
	port->FCRimage          = 0x00;
	port->IERmask           = 0x00;

	port->State             = ( PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER );
	port->WatchStateMask    = 0x00000000;              
 //       port->serialRequestLock = 0;
//      port->readActive        = false;
    }

    port->BaudRate          = kDefaultBaudRate;         // 9600 bps
    port->CharLength        = 8;                        // 8 Data bits
    port->StopBits          = 2;                        // 1 Stop bit
    port->TX_Parity         = 1;                        // No Parity
    port->RX_Parity         = 1;                        // --ditto--
    port->MinLatency        = false;
    port->XONchar           = '\x11';
    port->XOFFchar          = '\x13';
    port->FlowControl       = 0x00000000;
    port->RXOstate          = IDLE_XO;
    port->TXOstate          = IDLE_XO;
    //port->FrameTOEntry        = NULL;
    
    port->RXStats.BufferSize    = kMaxCirBufferSize;
    port->RXStats.HighWater     = (port->RXStats.BufferSize << 1) / 3;
    port->RXStats.LowWater      = port->RXStats.HighWater >> 1;

    port->TXStats.BufferSize    = kMaxCirBufferSize;
    port->TXStats.HighWater     = (port->RXStats.BufferSize << 1) / 3;
    port->TXStats.LowWater      = port->RXStats.HighWater >> 1;

    port->FlowControl           = (DEFAULT_AUTO | DEFAULT_NOTIFY);
//  port->FlowControl           = DEFAULT_NOTIFY;

    port->AreTransmitting   = FALSE;

    for ( tmp=0; tmp < (256 >> SPECIAL_SHIFT); tmp++ )
	port->SWspecial[ tmp ] = 0;

    return;
    
}/* end SetStructureDefaults */


/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::freeRingBuffer
//
//      Inputs:     Queue - the specified queue to free
//
//      Outputs:    
//
//      Desc:       Frees all resources assocated with the queue, then sets all queue parameters 
//                  to safe values.
//
/****************************************************************************************************/

void AppleSCCIrDA::freeRingBuffer( CirQueue *Queue )
{
    ELG( 0, Queue, 'frRB', "freeRingBuffer" );
    XTRACE(kLogfrRB, 0, 0);

    if ( Queue->Start )
    {
	IOFree( Queue->Start, Queue->Size );
	CloseQueue( Queue );
    }
    return;
    
}/* end freeRingBuffer */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::allocateRingBuffer
//
//      Inputs:     Queue - the specified queue to allocate, BufferSize - size to allocate
//
//      Outputs:    return Code - true (buffer allocated), false (it failed)
//
//      Desc:       Allocates resources needed by the queue, then sets up all queue parameters. 
//
/****************************************************************************************************/

bool AppleSCCIrDA::allocateRingBuffer( CirQueue *Queue, size_t BufferSize )
{
    UInt8   *Buffer;
	
    ELG( 0, BufferSize, 'alRB', "allocateRingBuffer" );
    XTRACE(kLogalRB, 0, 0);
    
    if ( !Queue->Start )
    {
	Buffer = (UInt8*)IOMalloc( BufferSize );
	InitQueue( Queue, Buffer, BufferSize );
    }

    if ( Queue->Start )
	return true;

    return false;
    
}/* end allocateRingBuffer */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::readPortState
//
//      Inputs:     port - the specified port
//
//      Outputs:    returnState - current state of the port
//
//      Desc:       Reads the current Port->State. 
//
/****************************************************************************************************/

UInt32 AppleSCCIrDA::readPortState( PortInfo_t *port )
{
    UInt32              returnState;
    
//  ELG( 0, port, 'rPSt', "readPortState" );

    IOLockLock( port->serialRequestLock );

    returnState = port->State;

    IOLockUnlock( port->serialRequestLock);

//  ELG( returnState, 0, 'rPS-', "readPortState" );

    return returnState;
    
}/* end readPortState */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::changeState
//
//      Inputs:     port - the specified port, state - new state, mask - state mask (the specific bits)
//
//      Outputs:    None
//
//      Desc:       Change the current Port->State to state using the mask bits.
//                  if mask = 0 nothing is changed.
//                  delta contains the difference between the new and old state taking the
//                  mask into account and it's used to wake any waiting threads as appropriate. 
//
/****************************************************************************************************/

void AppleSCCIrDA::changeState( PortInfo_t *port, UInt32 state, UInt32 mask )
{
    UInt32              delta;
    
//  ELG( state, mask, 'chSt', "changeState" );
    XTRACE(kLogChangeStateBits, state >> 16, (short)state);
    XTRACE(kLogChangeStateMask, mask >> 16, (short)mask);

    IOLockLock( port->serialRequestLock );
    state = (port->State & ~mask) | (state & mask); // compute the new state
    delta = state ^ port->State;                    // keep a copy of the diffs
    port->State = state;
    
    XTRACE(kLogChangeStateDelta, delta >> 16, (short)delta);
    XTRACE(kLogChangeStateNew, port->State >> 16, (short)port->State);

	// Wake up all threads asleep on WatchStateMask
	
    if ( delta & port->WatchStateMask )
    {
	thread_wakeup_with_result( &port->WatchStateMask, THREAD_RESTART );
    }

    IOLockUnlock( port->serialRequestLock );

    ELG( port->State, delta, 'chSt', "changeState - exit" );
    return;
    
}/* end changeState */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::privateWatchState
//
//      Inputs:     port - the specified port, state - state watching for, mask - state mask (the specific bits)
//
//      Outputs:    IOReturn - kIOReturnSuccess, kIOReturnIOError or kIOReturnIPCError
//
//      Desc:       Wait for the at least one of the state bits defined in mask to be equal
//                  to the value defined in state. Check on entry then sleep until necessary.
//                  A return value of kIOReturnSuccess means that at least one of the port state
//                  bits specified by mask is equal to the value passed in by state.  A return
//                  value of kIOReturnIOError indicates that the port went inactive.  A return
//                  value of kIOReturnIPCError indicates sleep was interrupted by a signal. 
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::privateWatchState( PortInfo_t *port, UInt32 *state, UInt32 mask )
{
    unsigned            watchState, foundStates;
    bool                autoActiveBit   = false;
    IOReturn            rtn             = kIOReturnSuccess;

//    ELG( mask, *state, 'wsta', "privateWatchState" );

    watchState              = *state;
    IOLockLock( port->serialRequestLock );

	// hack to get around problem with carrier detection
	
    if ( *state | 0x40 )        // never wait on dsr going high or low
    {
	port->State |= 0x40;
    }

    if ( !(mask & (PD_S_ACQUIRED | PD_S_ACTIVE)) )
    {
	watchState &= ~PD_S_ACTIVE; // Check for low PD_S_ACTIVE
	mask       |=  PD_S_ACTIVE; // Register interest in PD_S_ACTIVE bit
	autoActiveBit = true;
    }

    for (;;)
    {
	    // Check port state for any interesting bits with watchState value
	    // NB. the '^ ~' is a XNOR and tests for equality of bits.
	    
	foundStates = (watchState ^ ~port->State) & mask;

	if ( foundStates )
	{
	    *state = port->State;
	    if ( autoActiveBit && (foundStates & PD_S_ACTIVE) )
	    {
		rtn = kIOReturnIOError;
	    } else {
		rtn = kIOReturnSuccess;
	    }
//          ELG( rtn, foundStates, 'FndS', "privateWatchState - foundStates" );
	    break;
	}

	    // Everytime we go around the loop we have to reset the watch mask.
	    // This means any event that could affect the WatchStateMask must
	    // wakeup all watch state threads.  The two events are an interrupt
	    // or one of the bits in the WatchStateMask changing.
	    
	port->WatchStateMask |= mask;

	    // note: Interrupts need to be locked out completely here,
	    // since as assertwait is called other threads waiting on
	    // &port->WatchStateMask will be woken up and spun through the loop.
	    // If an interrupt occurs at this point then the current thread
	    // will end up waiting with a different port state than assumed
	    //  -- this problem was causing dequeueData to wait for a change in
	    // PD_E_RXQ_EMPTY to 0 after an interrupt had already changed it to 0.

	assert_wait( &port->WatchStateMask, true ); /* assert event */

	IOLockUnlock( port->serialRequestLock );
	rtn = thread_block( 0 );         /* block ourselves */
	IOLockLock( port->serialRequestLock );

	if ( rtn == THREAD_RESTART )
	{
	    continue;
	} else {
	    rtn = kIOReturnIPCError;
	    break;
	}
    }/* end for */

	    // As it is impossible to undo the masking used by this
	    // thread, we clear down the watch state mask and wakeup
	    // every sleeping thread to reinitialize the mask before exiting.
	
    port->WatchStateMask = 0;

    thread_wakeup_with_result( &port->WatchStateMask, THREAD_RESTART );
    IOLockUnlock( port->serialRequestLock);
    
    //    ELG( rtn, *state, 'wEnd', "privateWatchState end" );
    
    return rtn;
    
}/* end privateWatchState */


/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::message
//
//      Inputs:     type - message type
//              provider - my provider
//              argument - additional parameters
//
//      Outputs:    return Code - kIOReturnSuccess
//
//      Desc:       Handles IOKit messages. 
//
/****************************************************************************************************/

IOReturn AppleSCCIrDA::message( UInt32 type, IOService *provider,  void *argument )
{
    
    ELG( 0, type, 'mess', "message" );
    XTRACE(kLogmess, type >> 16, type);
    
    switch ( type )
    {
	case kIOMessageServiceIsTerminated:
	    ELG( 0, type, 'ms01', "message - kIOMessageServiceIsTerminated" );
	    XTRACE(kLogms01, 0, 0);
	    fTerminate = true;      // we're being terminated
	    break;
	    
	case kIOMessageServiceIsSuspended:  
	    ELG( 0, type, 'ms02', "message - kIOMessageServiceIsSuspended" );
	    XTRACE(kLogms02, 0, 0);
	    break;
	    
	case kIOMessageServiceIsResumed:    
	    ELG( 0, type, 'ms03', "message - kIOMessageServiceIsResumed" );
	    XTRACE(kLogms03, 0, 0);
	    break;
	    
	case kIOMessageServiceIsRequestingClose: 
	    ELG( 0, type, 'ms04', "message - kIOMessageServiceIsRequestingClose" );
	    XTRACE(kLogms04, 0, 0); 
	    break;
	    
	case kIOMessageServiceWasClosed:    
	    ELG( 0, type, 'ms05', "message - kIOMessageServiceWasClosed" );
	    XTRACE(kLogms05, 0, 0); 
	    break;
	    
	case kIOMessageServiceBusyStateChange:  
	    ELG( 0, type, 'ms06', "message - kIOMessageServiceBusyStateChange" );
	    XTRACE(kLogms06, 0, 0); 
	    break;
	    
	default:
	    ELG( 0, type, 'mes-', "message - unknown message" );
	    XTRACE(kLogmesminus, 0, 0); 
	    break;
    }
    
    return kIOReturnSuccess;
    
}/* end message */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::parseInputBuffer
//
//      Inputs:     length - length of data
//                  data - data received
//
//      Outputs:    
//
//      Desc:       Kick off the real parse engine
//
/****************************************************************************************************/

void AppleSCCIrDA::parseInputBuffer( UInt32 length, UInt8 *data )
{
//  ELG( 0, length, 'pIpB', "parseInputBuffer" );
    XTRACE(kLogpIpB, 0, length);
    
    if ( fModeFlag == kMode_SIR )
    {
	parseInputSIR( length, data );
    } else {
	if ( fModeFlag == kMode_FIR )
	{
	    parseInputFIR( length, data );
	} else {
	    ELG( 0, fModeFlag, 'pIm-', "parseInputBuffer - incorrect mode, data dropped" );
	    XTRACE(kLogpImminus, 0, fModeFlag);
	}
    }
    
}/* end parseInputBuffer */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::parseInputSIR
//
//      Inputs:     length - length of data
//              data - data received
//
//      Outputs:    
//
//      Desc:       Parse the input buffer in SIR mode
//
/****************************************************************************************************/

void AppleSCCIrDA::parseInputSIR( UInt32 length, UInt8 *data )
{
    IOReturn    rtn = kIOReturnSuccess;
    UInt8   byte;
    
//  ELG( 0, 0, 'pISR', "parseInputSIR" );
    XTRACE(kLogpISR, 0, 0);

	// Loop over newly received data

    while (length-- > 0)
    {
	byte = *data++;
	
	switch ( fParseState )
	{
	    case kParseStateIdle:
	    XTRACE(kLogpStI, 0, byte);
//        ELG( fParseState, byte, 'pStI', "parseInputSIR - parse idle state" );                         
		if (byte == 0xC0)               // Idle State - discard everything but BOF
		{
		    fParseState = kParseStateBOF;
		}
		break;                                      
	    case kParseStateBOF:
		XTRACE(kLogpStB, 0, byte);
//        ELG( fParseState, byte, 'pStB', "parseInputSIR - parse BOF state" );
		switch (byte) 
		{
		    case 0xC0:                  // BOF State - multiple BOFs are ignored
			break;                                  
		    case 0xC1:                  // BOF State - EOF is invalid   
			fParseState = kParseStateIdle;
			fDataLength = 0;
			break;                      
		    case 0x7D:                  // BOF State - ESCAPE, starts with a pad byte
			fParseState = kParseStatePad;
			break;                                  
		    default:                    // BOF State - data, starts with a normal byte  
			fInUseBuffer[fDataLength++] = byte;
			fParseState = kParseStateData;
			break;
		}
		break;                                      
	    case kParseStateData:
		XTRACE(kLogpStd, 0, byte);
//        ELG( fParseState, byte, 'pStd', "parseInputSIR - parse data state" );
		switch (byte) 
		{
		    case 0xC0:                  // Data State - BOF is invalid in the data section of a packet
			fParseState = kParseStateIdle;
			fDataLength = 0;
			break;                  
		    case 0xC1:                  // Data State - EOF, done with the packet, check the CRC
			if ( fDataLength > 2 ) 
			{
			    if ( ::check_crc16(fInUseBuffer, fDataLength) )     // Check CRC is valid
			    {
				fDataLength -= 2;                   // Don't pass back the CRC
				if ( fIrDA )
				{
				    LogData( kDriverIn, fDataLength, fInUseBuffer );
				    rtn = fIrDA->ReadComplete( fInUseBuffer, fDataLength );
				    if ( rtn != kIOReturnSuccess )
				    {
					ELG( 0, rtn, 'pSI-', "parseInputSIR - IrDA ReadComplete problem" );
					XTRACE(kLogpSIminus, rtn >> 16, rtn);
				    }
				}                               
			    } else {
				ELG( 0, 0, 'pSC-', "parseInputSIR - CRC error" );
				XTRACE(kLogpSCminus, 0, 0);
				fICRCError++;
			    }
			} else {
			    ELG( 0, 0, 'pSE-', "parseInputSIR - EOF early < 2 bytes of data" );
			    XTRACE(kLogpSEminus, fParseState, 0);
			}
			fParseState = kParseStateIdle;
			fDataLength = 0;
			break;
		    case 0x7D:                  // Data State - pad byte, switch to pad state
			fParseState = kParseStatePad;
			break;                          
		    default:                    // Data State - by golly it's data
			fInUseBuffer[fDataLength++] = byte;
			break;
		}
		break;                                      
	    case kParseStatePad:
		XTRACE(kLogpStp, 0, byte);
//                ELG( fParseState, byte, 'pStp', "parseInputSIR - parse pad state" );
		fInUseBuffer[fDataLength++] = byte ^ 0x20;
		fParseState = kParseStateData;
		break;                                      
	    default:
		ELG( fParseState, byte, 'pSte', "parseInputSIR - state error" );
		XTRACE(kLogpSte, fParseState, byte);
		break;      
	}
    }
}/* end parseInputSIR */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::parseInputFIR
//
//      Inputs:     length - length of data
//              data - data received
//
//      Outputs:    
//
//      Desc:       Parse the input buffer in FIR mode
//
/****************************************************************************************************/

void AppleSCCIrDA::parseInputFIR( UInt32 length, UInt8 *data )
{
    ELG( 0, 0, 'pIFR', "parseInputFIR - Currently not supported" );
    XTRACE(kLogpIFR, 0, 0);
    
}/* end parseInputFIR */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::parseInputReset
//
//      Inputs:     none
//
//      Outputs:    fParseState and fDataLength get reset
//
//      Desc:       initialize input parse engine (on each packet transmit)
//
/****************************************************************************************************/
void AppleSCCIrDA::parseInputReset( void )
{
    ELG( 0, 0, 'pIRt', "parseInputReset" );
    XTRACE(kLogpIRt, 0, 0);

    fParseState = kParseStateIdle;
    fDataLength = 0;
    
}/* end parseInputReset */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::Prepare_Buffer_For_Transmit
//
//      Inputs:     fOutBuffer - the transmit buffer
//              control_length - Length of control data
//              control_buffer - Control data
//              data_length - Length of raw data
//              data_buffer - raw data
//
//      Outputs:    length - actual length to transmit
//
//      Desc:       Sets up the transmit buffer as follows:
//              first N-1 bytes of XBOF characters
//                  then the BOF byte
//                  then the IrLAP frame
//                  then the CRC16
//                  then the EOF
//
/****************************************************************************************************/

UInt32 AppleSCCIrDA::Prepare_Buffer_For_Transmit( UInt8 *fOutBuffer, UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer )
{
    UInt32  length;             // length of xmit buffer (not including 4 byte header)
    UInt8   *dest;              // pointer into constructed xmit buffer
    UInt8   *src;               // pointer into input xmit packet
    int     src_length;         // original write length from user (irlap length)
    int     i;
    UInt16  crc16;              // sir crc
    
    ELG( 0, 0, 'PBFT', "Prepare_Buffer_For_Transmit" );
    XTRACE(kLogPBFT, 0, 0);
    
    dest = fOutBuffer;              // where to start building the xmit 
    
    // Start off with a few BOF bytes. The old irda spec (1.0) says to start with C0 bytes, 
	// but the 1.1 spec says to use 0xFF and a single 0xC0 as a BOF flag.
    
    for (i = 0; i < fBofsCode; i++)         // N-1 pad bytes of 0xFF
    {
	*dest++ = 0xFF;             // change these to 0xC0 to match old spec (for testing)
    }
    *dest++ = 0xC0;             // one BOF byte after the 0xFF pad bytes
    length = fBofsCode+1;           // review
    
    // Now copy the irlap packet, doing byte stuffing as needed
	
    src = control_buffer;           // start of client's irlap packet
    src_length = control_length;        // grab count of client's bytes
    
    while (src_length-- > 0)            // copy over the user bytes 
    {
	SIRStuff(*src++, &dest, &length);   // byte stuffing along the way
    }
    
    src = data_buffer;              // and again for the non-control portion
    src_length = data_length;
    
    while (src_length-- > 0)            // copy over the user bytes
    {
	SIRStuff(*src++, &dest, &length);   // byte stuffing along the way
    }
    
	// now we append the CRC16.  Make it from the original buffer, non-escape byte
    
    crc16 = 0xffff;                                 // initialize the crc
    crc16 = ::update_crc16(crc16, control_buffer, control_length);          // update w/the packet
    crc16 = ::update_crc16(crc16, data_buffer, data_length);                // update w/the packet
    crc16 = ~crc16;                                 // finalize the crc
    
    SIRStuff(crc16 >> 0, &dest, &length);   // add in the crc16, lsb then msb
    SIRStuff(crc16 >> 8, &dest, &length);   // crc bytes are byte-stuffed too
    
    // and finally end with EOF
	
    *dest++ = 0xC1;
    length++;
    
    // length is now what we want to actually transmit
	
    return length;
    
}/* end Prepare_Buffer_For_Transmit */

/****************************************************************************************************/
//
//      Method:     AppleSCCIrDA::SIRStuff
//
//      Inputs:     byte - data to be stuffed
//              destptr - where to put it
//
//      Outputs:    lengthPtr - updated lenth
//
//      Desc:       Stuff byte into the output buffer in SIR mode
//
/****************************************************************************************************/

void AppleSCCIrDA::SIRStuff(UInt8 byte, UInt8 **destPtr, UInt32 *lengthPtr)
{
    UInt8 *dest   = *destPtr;
    UInt32 length = *lengthPtr;
    
    switch (byte) 
    {
	case 0xC0:                          
	case 0xC1:
	case 0x7D:                      
	    *dest++ = 0x7D;         // the escape byte followed by ...
	    *dest++ = byte ^ 0x20;      // c0 to e0, c1 to e1, 7d to 5d
	    length += 2;
	    break;
				
	default:    
	    *dest++ = byte;         // the usual case, non-escaped byte to copy
	    length++;
	    break;
    }
	
    *destPtr   = dest;
    *lengthPtr = length;
	
    return;
    
}/* end SIRStuff */

/* static */
void AppleSCCIrDA::tx_thread(thread_call_param_t param0, thread_call_param_t param1)
{
    AppleSCCIrDA *obj;
    UInt32 count = (UInt32)param1;
    UInt32 tCount;
    UInt32  waitBits;
    IOReturn rtn;
    
    XTRACE(kLogTxThread, 0, (short)param1);
    
    require(param0, Fail);
    obj = OSDynamicCast(AppleSCCIrDA, (OSObject *)param0);
    require(obj, Fail);
    
    require(obj->fpDevice, Fail);
    
    // now that we have SetSpeedComplete, this nonsense is not
    // needed anymore ...
    /**if (obj->fSpeedChanging == true) {          // doing a write, but speed still changing
	AbsoluteTime deadline;
	bool rc;
	
	XTRACE(kLogTxThreadDeferred, 0, 0);
	clock_interval_to_deadline(1000, kMicrosecondScale, &deadline);     // wait a ms?
	rc = thread_call_enter1_delayed(obj->tx_thread_call, param1, deadline);
	IOLog("AppleSCCIrDA: tx thread called self w/delay, rc %d\n", rc);
	return;
    }
    ***/
    // do normal xmit now
    rtn = obj->fpDevice->enqueueData(obj->fOutBuffer, count, &tCount, true);
    require(rtn == kIOReturnSuccess && tCount == count, Fail);

    waitBits = 0;           // want tx busy to go to zero
    rtn = obj->fpDevice->watchState(&waitBits, PD_S_TX_BUSY);
    check(rtn == kIOReturnSuccess);
    
    XTRACE(kLogTxThreadResumed, 0, 0);

    if (obj->fIrDA)
	obj->fIrDA->Transmit_Complete(rtn == kIOReturnSuccess);

Fail:
    XTRACE(kLogTxThread, 0xffff, 0xffff);
    return;
}


/* static */
void AppleSCCIrDA::rx_thread(thread_call_param_t param0, thread_call_param_t param1)
{ 
    IOReturn    rtn;
    UInt32  count;
    AppleSCCIrDA *obj;

    XTRACE(kLogRxThread, 0, 0);
    
    require(param0, Fail);
    obj = OSDynamicCast(AppleSCCIrDA, (OSObject *)param0);
    require(obj, Fail);
    
    while (obj->fpDevice) {
	UInt32  want;
	want = 0;           // want empty bit to be zero
	rtn = obj->fpDevice->watchState(&want, PD_S_RXQ_EMPTY);
	XTRACE(kLogRxThreadResumed, rtn >> 16, (short)rtn);
	// this returns an error when we stop irda and inactivate the scc
	if (rtn != kIOReturnSuccess) {
	    break;
	}
		
	if (obj->fSendingCommand) {     // if sending a command, then it's hands-off for me
	    XTRACE(kLogRxThreadSleeping, 0, 0);
	    IOSleep(25);                // so just sleep and do it again
	}
	else {                          // else, I can grab the data
	    require(obj->fpDevice && obj->fInBuffer, Fail);
	    rtn = obj->fpDevice->dequeueData(obj->fInBuffer, SCCLapPayLoad, &count, false);
	    require(rtn == kIOReturnSuccess, Fail);
	    if (count > 0) {
		XTRACE(kLogRxThreadGotData, 0, count);
		obj->parseInputBuffer(count, obj->fInBuffer);
	    }
	}
    }

Fail:
    return;
}/* end rxLoop */

/* static */
void
AppleSCCIrDA::speed_change_thread(thread_call_param_t param0, thread_call_param_t param1)
{
    AppleSCCIrDA *obj;
    IOReturn rtn;
    int command = (int)param1;
    UInt32 delay;
    
    XTRACE(kLogSpeedChangeThread, 0, (short)param1);
    
    require(param0, Fail);
    obj = OSDynamicCast(AppleSCCIrDA, (OSObject *)param0);
    require(obj, Fail);
    require(obj->fSpeedChanging == true, Fail);
    require(obj->fpDevice, Fail);
    
    // Hack #824
    // The write complete gets called when all of the data is transferred to the SCC cell's xmit fifo.
    // Not when the bytes have actually made it out of the SCC to the IR hardware.
    // Changing the speed before the transmit fifo is empty, will flush the fifo (or xmit at new speed?).
    // This is initially a problem when sending the UA packet, just before switching from 9600 baud
    // to the newly negotiated speed, but can also truncate the last packet sent at a higher speed
    // before switching back to 9600.  Trial and error came up with 20ms as a reasonable delay.
    
    if (1) {                                    // if xmit complete isn't to be trusted
	XTRACE(kLogSpeedChangeThread, 1, 20);
	IOSleep(20);
    }
    
    rtn = obj->fpDevice->executeEvent(PD_E_DATA_LATENCY, 1);    // set to no delay for command echo
    require(rtn == kIOReturnSuccess, Fail);
    
    rtn = obj->sendCommand(command, obj->fInBuffer);    // sets scc baud to fBaudCode after command runs
    if ( rtn == kIOReturnSuccess )
    {
	ELG( 0, fInBuffer[0], 'SSs+', "SetSpeed - sendCommand successful" );
	XTRACE(kLogSSsplus, 0, obj->fInBuffer[0]);
    }
    
    // compute the delay (in nanoseconds) for about 5 bytes at the current speed
    // the math is done this way so that the numbers all fit in a UInt32 w/out
    // have to use floating point.  Should get about 5 million nanoseconds for
    // 9600 baud, down to about 400,000 nanoseconds for 115k.
    // should be 50 million / baud 
    delay = ( 50000000 / obj->fBaudCode);
    rtn = obj->fpDevice->executeEvent(PD_E_DATA_LATENCY, delay);
    require(rtn == kIOReturnSuccess, Fail);
    
    obj->fSpeedChanging = false;
    
    if (obj->fIrDA)
	obj->fIrDA->SetSpeedComplete(true);
    
Fail:
    XTRACE(kLogSpeedChangeThread, 0xffff, 0xffff);
    return;
}   

