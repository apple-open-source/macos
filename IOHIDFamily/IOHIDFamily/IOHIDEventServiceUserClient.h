/*
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
#ifndef _IOKIT_IOHIDEVENTSERVICEUSERCLIENT_H
#define _IOKIT_IOHIDEVENTSERVICEUSERCLIENT_H

enum IOHIDEventServiceUserClientCommandCodes {
    kIOHIDEventServiceUserClientOpen,
    kIOHIDEventServiceUserClientClose,
    kIOHIDEventServiceUserClientCopyEvent,
    kIOHIDEventServiceUserClientSetElementValue,
    kIOHIDEventServiceUserClientNumCommands
};

#ifdef KERNEL

#include <IOKit/IOUserClient.h>
#include "IOHIDEventService.h"

class IOHIDEventServiceQueue;

class IOHIDEventServiceUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOHIDEventServiceUserClient)

private:
    static const IOExternalMethodDispatch
		sMethods[kIOHIDEventServiceUserClientNumCommands];

    IOHIDEventService *         _owner;
    IOHIDEventServiceQueue *    _queue;
    IOOptionBits                _options;
    task_t                      _client;
    
    void eventServiceCallback(  IOHIDEventService *             sender, 
                                void *                          context,
                                IOHIDEvent *                    event, 
                                IOOptionBits                    options);

    static IOReturn _open(      IOHIDEventServiceUserClient *   target, 
                                void *                          reference, 
                                IOExternalMethodArguments *     arguments);
                                
    static IOReturn _close(     IOHIDEventServiceUserClient *   target, 
                                void *                          reference, 
                                IOExternalMethodArguments *     arguments);

    static IOReturn _copyEvent(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference, 
                                IOExternalMethodArguments *     arguments);

    static IOReturn _setElementValue(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference, 
                                IOExternalMethodArguments *     arguments);

protected:
    // IOUserClient methods
    virtual IOReturn clientClose( void );
    virtual void stop( IOService * provider );

    virtual IOService * getService( void );

    virtual IOReturn registerNotificationPort(
                                mach_port_t                     port, 
                                UInt32                          type, 
                                UInt32                          refCon );

    virtual IOReturn clientMemoryForType(
                                UInt32                          type,
                                IOOptionBits *                  options,
                                IOMemoryDescriptor **           memory );

	virtual IOReturn externalMethod(
                                uint32_t                        selector, 
                                IOExternalMethodArguments *     arguments,
                                IOExternalMethodDispatch *      dispatch, 
                                OSObject *                      target, 
                                void *                          reference);


public:
    // others
    virtual bool initWithTask(task_t owningTask, void * security_id, UInt32 type);
    virtual bool start( IOService * provider );
    virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer);
    virtual void free();
    virtual IOReturn setProperties( OSObject * properties );
    virtual IOReturn open(IOOptionBits options);
    virtual IOReturn close();
    virtual IOHIDEvent * copyEvent(IOHIDEventType type, IOHIDEvent * matching, IOOptionBits options = 0);
    virtual void setElementValue(UInt32 usagePage, UInt32 usage, UInt32 value);
};

#endif /* KERNEL */

#endif /* _IOKIT_IOHIDEVENTSERVICEUSERCLIENT_H */
