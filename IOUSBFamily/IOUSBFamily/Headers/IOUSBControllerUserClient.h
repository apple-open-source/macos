/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _IOKIT_IOUSBCONTROLLERUSERCLIENT_H
#define _IOKIT_IOUSBCONTROLLERUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/usb/IOUSBUserClient.h>
#include <IOKit/usb/IOUSBLog.h>

class IOUSBController;

class IOUSBControllerUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOUSBControllerUserClient)

private:

    IOUSBController *			fOwner;
    task_t				fTask;
    const IOExternalMethod *		fMethods;
    IOCommandGate *			fGate;
    UInt32				fNumMethods;
    mach_port_t 			fWakePort;
    bool				fDead;

    static const IOExternalMethod	sMethods[kNumUSBControllerMethods];
    static const IOItemCount 		sMethodCount;

    struct ExpansionData { /* */ };
    ExpansionData * 			fExpansionData;

    virtual void 			SetExternalMethodVectors(void);
    
public:

    //	IOService overrides
    //	
    virtual IOReturn  			open(bool seize);
    virtual IOReturn  			close(void);
    virtual bool 			start( IOService * provider );
    virtual void 			stop( IOService * provider );

    //	IOUserClient overrides
    //
    virtual bool 			initWithTask( task_t owningTask, void * securityID,
                                                    UInt32 type,  OSDictionary * properties );
    virtual IOExternalMethod * 		getTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOReturn 			clientClose( void );

  
    // Kernel Debugging methods
    //
    virtual IOReturn			EnableKernelLogger(bool enable);
    virtual IOReturn			SetDebuggingLevel(KernelDebugLevel inLevel);
    virtual IOReturn			SetDebuggingType(KernelDebuggingOutputType inType);
    virtual IOReturn			GetDebuggingLevel(KernelDebugLevel * inLevel);
    virtual IOReturn			GetDebuggingType(KernelDebuggingOutputType * inType);
    virtual IOReturn			SetTestMode(UInt32 mode, UInt32 port);
};


#endif /* ! _IOKIT_IOUSBCONTROLLERUSERCLIENT_H */

