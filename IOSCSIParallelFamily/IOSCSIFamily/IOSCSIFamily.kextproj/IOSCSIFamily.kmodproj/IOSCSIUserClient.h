/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * Changes to this API are expected.
 */

#ifndef _IOKIT_IOSCSIUSERCLIENT_H_
#define _IOKIT_IOSCSIUSERCLIENT_H_

#include "IOCDBUserClient.h"

enum IOSCSIUserClientCommandCodes {
    kIOSCSIUserClientSetTargetParms	// kIOUCStructIStructO,	 n,  0
	= kIOCDBUserClientNumCommands,
    kIOSCSIUserClientGetTargetParms,	// kIOUCStructIStructO,	 0,  n
    kIOSCSIUserClientSetLunParms,	// kIOUCStructIStructO,	 n,  0
    kIOSCSIUserClientGetLunParms,	// kIOUCStructIStructO,	 0,  n
    kIOSCSIUserClientHoldQueue,		// kIOUCScalarIScalarO,	 1,  0
    kIOSCSIUserClientReleaseQueue,	// kIOUCScalarIScalarO,	 1,  0
    kIOSCSIUserClientFlushQueue,	// kIOUCScalarIScalarO,	 2,  0
    kIOSCSIUserClientNotifyIdle,	// kIOUCScalarIScalarO,	 3,  0
    kIOSCSIUserClientNumCommands,

    kIOSCSIUserClientNumOnlySCSICommands =
        (kIOSCSIUserClientNumCommands - kIOCDBUserClientNumCommands)
};

#if KERNEL

class IOSCSIUserClient : public IOCDBUserClient 
{
    OSDeclareDefaultStructors(IOSCSIUserClient)

public:

protected:
    OSAsyncReference fIdleAsyncRef;
    void *fIdleArgs[2];

protected:
    static const IOExternalMethod
                sSCSIOnlyMethods[kIOSCSIUserClientNumOnlySCSICommands];
    static IOExternalMethod
		sMethods[kIOSCSIUserClientNumCommands];
    static void initialize();

    // Methods
    virtual bool start(IOService *provider);

    virtual void setExternalMethodVectors();

    virtual IOReturn setTargetParms(void *vInParms, void *vInSize,
                                    void *, void *, void *, void *);
    virtual IOReturn getTargetParms(void *vOutParms, void *vOutSize,
                                    void *, void *, void *, void *);

    /* 
     * Lun management commands
     */
    virtual IOReturn setLunParms(void *vInParms, void *vInSize,
                                 void *, void *, void *, void *);
    virtual IOReturn getLunParms(void *vOutParms, void *vOutSize,
                                 void *, void *, void *, void *);

    /* 
     * Queue management commands
     */
    virtual IOReturn
        holdQueue(void *vInQType, void *, void *, void *, void *, void *);
    virtual IOReturn
        releaseQueue(void *vInQType, void *, void *, void *, void *, void *);
    virtual IOReturn
        flushQueue(void *vInQType, void *vInResultCode,
                   void *, void *, void *, void *);
    virtual IOReturn notifyIdle(void *target, void *callback, void *refcon,
                                void *sender, void *, void *);
    static void notifyIdleCallBack(void *vSelf, void *refcon);
};

#endif /* KERNEL */

#endif /* ! _IOKIT_IOSCSIUSERCLIENT_H_ */

