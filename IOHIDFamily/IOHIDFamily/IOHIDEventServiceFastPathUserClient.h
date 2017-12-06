/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef IOHIDEventServiceFastPathUserClient_h
#define IOHIDEventServiceFastPathUserClient_h


#define kIOHIDEventServiceFastPathUserClientType 'HIDF'

enum IOHIDEventServiceFastPathUserClientCommandCodes {
    kIOHIDEventServiceFastPathUserClientOpen,
    kIOHIDEventServiceFastPathUserClientClose,
    kIOHIDEventServiceFastPathUserClientCopyEvent,
    kIOHIDEventServiceFastPathUserClientNumCommands
};

enum IOHIDEventServiceFastPathUserClientCopySpecType {
    kIOHIDEventServiceFastPathCopySpecSerializedType,
    kIOHIDEventServiceFastPathCopySpecDataType
};

#ifdef KERNEL

#include <IOKit/IOUserClient.h>
#include "IOHIDEventService.h"
#include <IOKit/IOCommandGate.h>

class IOHIDEventServiceQueue;

class IOHIDEventServiceFastPathUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOHIDEventServiceFastPathUserClient)
    
private:
  
  typedef struct ExternalMethodGatedArguments {
        uint32_t                    selector;
        IOExternalMethodArguments * arguments;
        IOExternalMethodDispatch *  dispatch;
        OSObject *                  target;
        void *                      reference;
    } ExternalMethodGatedArguments;
  
    static const IOExternalMethodDispatch sMethods[kIOHIDEventServiceFastPathUserClientNumCommands];

    IOHIDEventService *         _owner;
    void *                      _clientContext;
    IOHIDEventServiceQueue *    _queue;
    IOCommandGate *             _commandGate;
    volatile uint32_t           _opened;
    IOOptionBits                _options;
    IOLock *                    _lock;
    
    static IOReturn _open (IOHIDEventServiceFastPathUserClient * target, void * reference, IOExternalMethodArguments * arguments);
    static IOReturn _close (IOHIDEventServiceFastPathUserClient *  target, void * reference, IOExternalMethodArguments * arguments);
    static IOReturn _copyEvent(IOHIDEventServiceFastPathUserClient * target, void * reference, IOExternalMethodArguments * arguments);

    bool    serializeDebugState(void * ref, OSSerialize * serializer);
    
protected:
    // IOUserClient methods
    virtual IOReturn clientClose (void);

    virtual void stop (IOService * provider);
    
    virtual IOService * getService (void);

    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits * options, IOMemoryDescriptor ** memory);
    
    IOReturn clientMemoryForTypeGated (IOOptionBits * options, IOMemoryDescriptor ** memory);

    virtual IOReturn externalMethod (
                        uint32_t                        selector,
                        IOExternalMethodArguments *     arguments,
                        IOExternalMethodDispatch *      dispatch,
                        OSObject *                      target,
                        void *                          reference
                        );
    
    IOReturn externalMethodGated(ExternalMethodGatedArguments *arguments);
    
    IOReturn open(IOOptionBits options, OSDictionary * properties);
    
    IOReturn copyEvent(OSObject * copySpec, IOOptionBits options = 0);
    
    IOReturn setPropertiesGated (OSObject * properties) ;
    
    void copyPropertyGated (const char * aKey, OSObject **result) const;
    
public:
    
    virtual bool initWithTask(task_t owningTask, void * security_id, UInt32 type);
    virtual bool start( IOService * provider );
    virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer);
    virtual void free();
    virtual IOReturn setProperties( OSObject * properties);
    virtual OSObject * copyProperty( const char * aKey) const;
    virtual IOReturn close();
};
#endif /* KERNEL */
#endif /* IOHIDEventServiceFastPathUserClient_h */
