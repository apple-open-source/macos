/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2018 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef IOHIDActionQueue_h
#define IOHIDActionQueue_h

#include <libkern/c++/OSContainers.h>
#include <IOKit/IOLocks.h>

class IOHIDAction;
class IOInterruptEventSource;
class IOWorkLoop;
class IOCommandGate;

class IOHIDActionQueue : public OSObject
{
    OSDeclareDefaultStructors(IOHIDActionQueue)
public:
    typedef void (^IOHIDActionBlock)();
    
private:
    OSObject                    *_owner;
    IOLock                      *_lock;
    OSArray                     *_actionArray;
    IOWorkLoop                  *_workLoop;
    IOCommandGate               *_commandGate;
    IOInterruptEventSource      *_actionInterrupt;
    UInt32                      _state __attribute__((aligned(sizeof(UInt32))));
    IOHIDActionBlock            _cancelHandler;
    
    void enqueueIOHIDAction(IOHIDAction *action);
    IOHIDAction *dequeueIOHIDAction();
    void handleIOHIDAction(IOInterruptEventSource *sender, int count);
    
protected:
    virtual void free() APPLE_KEXT_OVERRIDE;
    virtual bool init(OSObject *owner, IOWorkLoop *workLoop);
    
public:
    static IOHIDActionQueue *actionQueue(OSObject *owner, IOWorkLoop *workLoop);
    
    void activate();
    void cancel();
    void setCancelHandler(IOHIDActionBlock handler);
    
    void dispatchAsync(IOHIDActionBlock action);
    void dispatchSync(IOHIDActionBlock action);
};

#endif /* IOHIDActionQueue_h */
