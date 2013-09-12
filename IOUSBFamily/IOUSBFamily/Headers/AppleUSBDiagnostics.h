/*
 * Copyright © 1998-2013 Apple Inc. All rights reserved.
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

#ifndef _IOKIT_APPLEUSBDIAGNOSTICS_H
#define _IOKIT_APPLEUSBDIAGNOSTICS_H

#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBLog.h>




class AppleUSBDiagnostics : public OSObject
{
 	OSDeclareDefaultStructors(AppleUSBDiagnostics);
	friend	class AppleXHCIDiagnostics;

public:
    enum{
        kDiagMaxPorts = 32,
        kXHCIMaxCompletionCodes = 256,
        kXHCILinkStates = 16
    };
    typedef struct
    {
        UInt32          errorCount;
        UInt32          xhciErrorCode[kXHCIMaxCompletionCodes];
        UInt64			prevBytes;
        UInt64			totalBytes;
        UInt32			timeouts;
        UInt32			prevTimeouts;
        UInt32			prevResets;
        UInt32			resets;
        UInt32			enable;
        UInt32			suspend;
        UInt32			resume;
        UInt32			warmReset;
        UInt32			linkState[kXHCILinkStates];
        UInt32			power;
        UInt32			u1Timeout;
        UInt32			u2Timeout;
        UInt32			remoteWakeMask;
    } UIMPortDiagnostics;
    
    typedef struct
    {
        UInt64			lastNanosec;
        UInt64			totalBytes;
        UInt64			prevBytes;
        UInt32			acessCount;
        UInt32			totalErrors;
        UInt32			prevErrors;
        UInt32			timeouts;
        UInt32			prevTimeouts;
        UInt32			resets;
        UInt32			prevResets;
        UInt32			recoveredErrors;
        UInt32			prevRecoveredErrors;
        UInt32			errors2Strikes;
        UInt32			prevErrors2Strikes;
        UInt32			errors3Strikes;
        UInt32			prevErrors3Strikes;
        UInt32			controlBulkTxOut;
        SInt32          numPorts;
        UIMPortDiagnostics portCounts[kDiagMaxPorts];
        UInt32          overFlowPortErrorCount;
    } UIMDiagnostics;
    
private:
	UIMDiagnostics *			_UIMDiagnostics;
	UInt32 *                    _controlBulkTransactionsOut;
    IOService *                 _controller;
public:
    
	
	static OSObject *		createDiagnostics( UIMDiagnostics*, UInt32 *controlBulkTransactionsOut , IOService *controller=0);
    virtual OSObject *      initDiagnostics(AppleUSBDiagnostics *diagnostics, UIMDiagnostics* obj, UInt32 *controlBulkTransactionsOut, IOService *_controller);
	virtual bool			serialize( OSSerialize * s ) const;
    virtual void            serializePort(OSDictionary *	dictionary, int port, UIMPortDiagnostics *counts, IOService *controller) const;
	
protected:
	
	virtual void			UpdateNumberEntry( OSDictionary * dictionary, UInt32 value, const char * name ) const;
	
};

#endif