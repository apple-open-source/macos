/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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


#ifndef _IOKIT_IOFRAMEBUFFERUSERCLIENT_H
#define _IOKIT_IOFRAMEBUFFERUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#include <IOKit/pci/IOAGPDevice.h>
#pragma clang diagnostic pop

#include <IOKit/graphics/IOFramebuffer.h>

#include "IOGraphicsDiagnose.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*

TERMINATION NOTES:

IOFramebufferUserClient:

We override requestTerminate() to return false so that IOFramebufferUserClients
are not terminated just because their provider terminates. We'll instead safely
stop and detach from the provider, and stick around until WS closes it or exits.

When IOFramebuffer terminates, IOFramebufferUserClient::willTerminate()
sets fTerminating to stop new threads from coming in from user space. In
IOFramebufferUserClient::didTerminate(), we hold off IOFramebuffer's
termination until all threads are guaranteed to be no longer in
IOFramebuffer (see RpcGuard for details). Once all threads are out of
IOFramebuffer, IOFramebufferUserClient calls super::didTerminate(defer=false)
to let IOFramebuffer's termination continue. This leads to
IOFramebufferUserClient's stop() and detach() methods being called.

At this point, we have an "orphaned" IOFramebufferUserClient. WindowServer still
has it open, but it's disconnected from the IOFramebuffer and only kept around
in the kernel to respond "sanely" to WindowServer's method calls and to keep
its mappings in place.

The key sequences are:
 - willTerminate->didTerminate->stop->detach
 - clientClose->finalize
Note that the above steps can be interleaved differently depending on how
userspace behaves; e.g., specifically stop() may be before or after
clientClose()/finalize(), so the first to occur is responsible for
calling fOwner->close().

IOFramebufferSharedUserClient:

Similar to IOFramebufferUserClient, except requestTerminate() returns true,
allowing the IOFramebufferSharedUserClient to be terminated before provider
stop. Any new method calls will be blocked at the mach level, returning errors
in err_mach_ipc space (such as MACH_SEND_INVALID_DEST 0x10000003).

We don't do anything in IOFramebufferSharedUserClient::clientClose() because
it's not particularly useful for a shared client (i.e. one which sets
kIOUserClientSharedInstanceKey). (We get clientClose() twice for a task that
calls IOServiceClose and then exits, but not at all if the task just exits,
unless it's the last task, in which case we get one clientClose() call.)
Luckily the two things we do in IOFramebufferUserClient::clientClose() don't
apply for the shared client: the FB isn't "opened" for exclusive access, so
there's no call to close it, and there's no need to call terminate() on
ourselves, because the shared client lives as long as the IOFramebuffer does.

RpcGuard:

The fActive flag provides the thread-safe mechanism to ensure no user threads
are in the provider when the provider stops, as long as all such sites are
RPC_GAURD()'d.

In an initialized but idle object, fActive is 0. During normal operation, user
threads increment fActive as they enter the user client's RPCs, and decrement
it as they return, so fActive will always be >= 0. RpcGuard is defined as a
class to ensure that these increments and decrements are balanced, so that
adding an early return doesn't accidentally introduce a counting bug.

When the user client terminates, fActive is decremented an extra time. This
serves two purposes:
1) A thread attempting to enter an RPC is never allowed entry past this point
   thanks to a check for >= 0 that is part of the atomic increment in rpcEnter().
   Decrements are simple decrements; increments are conditional so that they
   cannot happen once the value falls below 0.
2) The last thread out is either a user client's RPC returning (see
   RpcGuard::rpcLeave()) or it may be that no user threads were active when
   the UserClient's didTerminate() arrived, and therefore didTerminate() can
   carry on if it can guarantee threads are barred from entry.

The fTerminating flag provides an assurance that user threads will drain
eventually, since a pathological client could otherwise spawn a gazillion
threads and call the RPCs forever to hold off destruction indefinitely (which
would be bad because the provider is busy whilst terminating and we want to
allow it to finish as quickly as it can; DEVELOPMENT kernels will panic if
a service is busy > 60 seconds).

Threading/Workloops:

Note that many of the termination methods are called on the provider's
workloop; e.g., the stop() method of every service under an IOFramebuffer
will run on the IOFBController workloop.

A corollary to the above, in conjunction with the established system/controller
workloop locking order, is that the stop() method of anything attached under
IOFramebuffer must not attempt to acquire the system workloop. Doing so could
result in deadlock. (The DEADLOCK_DETECT code can help detect such cases, but
is only enabled in Development builds.)

*/

class IOFramebufferUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFramebufferUserClient)

    class RpcGuard {
    public:
        RpcGuard(IOFramebufferUserClient *uc) : fUC(uc) { fReturn = rpcEnter(); }
        ~RpcGuard() { rpcLeave(); }
        IOReturn                      fReturn;
        IOFramebufferUserClient      *fUC;
        IOReturn rpcEnter();
        void rpcLeave();
    };

public:
    IOFramebuffer *     fOwner;
    IOFramebuffer *     fOther;

private:
#if RLOG
    char                fName[32];
#endif
    bool                fTerminating;
    volatile int32_t    fActive;

public:
    // IOUserClient methods
    virtual IOReturn clientClose( void ) APPLE_KEXT_OVERRIDE;

    virtual IOService * getService( void ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn clientMemoryForType( UInt32 type,
        IOOptionBits * options, IOMemoryDescriptor ** memory ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn externalMethod( uint32_t selector, IOExternalMethodArguments * args,
                                        IOExternalMethodDispatch * dispatch, OSObject * target, void * reference ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn registerNotificationPort( mach_port_t, UInt32, UInt32 ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn connectClient( IOUserClient * other ) APPLE_KEXT_OVERRIDE;

    // others

    static IOFramebufferUserClient * withTask( task_t owningTask );

    virtual void free() APPLE_KEXT_OVERRIDE;
    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setProperties( OSObject * properties ) APPLE_KEXT_OVERRIDE;
    
    virtual bool willTerminate(IOService *provider, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer) APPLE_KEXT_OVERRIDE;
    virtual bool requestTerminate(IOService *provider, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual bool terminate(IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
    virtual bool finalize(IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferSharedUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFramebufferSharedUserClient)

    class RpcGuard {
    public:
        RpcGuard(IOFramebufferSharedUserClient *uc) : fUC(uc) { fReturn = rpcEnter(); }
        ~RpcGuard() { rpcLeave(); }
        IOReturn                            fReturn;
        IOFramebufferSharedUserClient      *fUC;
        IOReturn rpcEnter();
        void rpcLeave();
    };

private:

    IOFramebuffer *     fOwner;
#if RLOG
    char                fName[32];
#endif
    bool                fTerminating;
    volatile int32_t    fActive;

public:
    virtual void free() APPLE_KEXT_OVERRIDE;

    virtual IOService * getService( void ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn clientMemoryForType( UInt32 type,
        IOOptionBits * options, IOMemoryDescriptor ** memory ) APPLE_KEXT_OVERRIDE;

    static IOFramebufferSharedUserClient * withTask( task_t owningTask );

    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
    virtual bool willTerminate(IOService *provider, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer) APPLE_KEXT_OVERRIDE;
    virtual bool requestTerminate(IOService *provider, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual bool terminate(IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
    virtual bool finalize(IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOFramebufferDiagnosticUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOFramebufferDiagnosticUserClient)

    class RpcGuard {
    public:
        RpcGuard(IOFramebufferDiagnosticUserClient *uc) : fUC(uc)
            { fReturn = rpcEnter(); }
        ~RpcGuard() { rpcLeave(); }
        IOReturn rpcEnter();
        void rpcLeave();
        IOReturn fReturn;
        IOFramebufferDiagnosticUserClient *fUC;
    };

private:

    IOFramebuffer *     fOwner;
#if RLOG
    char                fName[32];
#endif
    bool                fTerminating;
    volatile int32_t    fActive;

public:
    static IOFramebufferDiagnosticUserClient *client();

    // IOService overrides
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual bool willTerminate(IOService *provider, IOOptionBits options)
        APPLE_KEXT_OVERRIDE;
    virtual bool didTerminate(IOService *provider, IOOptionBits options,
                              bool *defer) APPLE_KEXT_OVERRIDE;
    virtual bool requestTerminate(IOService *provider, IOOptionBits options)
        APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

    // IOUserClient override
    virtual IOReturn clientClose( void ) APPLE_KEXT_OVERRIDE;
    virtual IOReturn externalMethod(
            uint32_t selector, IOExternalMethodArguments *args,
            IOExternalMethodDispatch *dispatch, OSObject *target,
            void * reference) APPLE_KEXT_OVERRIDE;
};


#endif /* ! _IOKIT_IOFRAMEBUFFERUSERCLIENT_H */
