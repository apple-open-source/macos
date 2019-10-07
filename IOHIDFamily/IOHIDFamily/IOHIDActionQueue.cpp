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

#include "IOHIDActionQueue.h"
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <AssertMacros.h>
#include <libkern/Block.h>
#include <IOKit/assert.h>

typedef enum {
    kIOHIDActionQueueStateInactive      = 0,
    kIOHIDActionQueueStateActivated     = 1 << 0,
    kIOHIDActionQueueStateCancelled     = 1 << 1
} IOHIDActionQueueState;

class IOHIDAction : public OSObject
{
    OSDeclareDefaultStructors(IOHIDAction)
    
    IOHIDActionQueue::IOHIDActionBlock _action;
protected:
    virtual void free() APPLE_KEXT_OVERRIDE {
        if (_action) {
            Block_release(_action);
        }
        OSObject::free();
    }
    
public:
    static IOHIDAction *hidAction(IOHIDActionQueue::IOHIDActionBlock action) {
        if (!action) {
            return 0;
        }
        
        IOHIDAction *me = new IOHIDAction;
        
        if (me && !me->init()) {
            me->release();
            return 0;
        }
        
        me->_action = Block_copy(action);
        
        return me;
    }
    
    virtual void runAction() {
        (_action)();
    }
};

OSDefineMetaClassAndStructors(IOHIDAction, OSObject)

#define super OSObject

OSDefineMetaClassAndStructors(IOHIDActionQueue, OSObject)

bool IOHIDActionQueue::init(OSObject *owner, IOWorkLoop *workLoop)
{
    bool result = false;
    
    require(owner && workLoop, exit);
    require(super::init(), exit);
    
    _owner = owner;
    _workLoop = workLoop;
    _workLoop->retain();
    
    _commandGate = IOCommandGate::commandGate(this);
    require(_commandGate, exit);
    
    _actionArray = OSArray::withCapacity(1);
    require(_actionArray, exit);
    
    _lock = IOLockAlloc();
    require(_lock, exit);
    
    _actionInterrupt = IOInterruptEventSource::interruptEventSource(this,
                                                                    OSMemberFunctionCast(IOInterruptEventAction,
                                                                                         this, &IOHIDActionQueue::handleIOHIDAction));
    require(_actionInterrupt, exit);
    
    result = true;
    
exit:
    return result;
}

void IOHIDActionQueue::free()
{
    if (_state) {
        assert(_state & kIOHIDActionQueueStateCancelled);
    }
    
    OSSafeReleaseNULL(_workLoop);
    OSSafeReleaseNULL(_commandGate);
    OSSafeReleaseNULL(_actionArray);
    OSSafeReleaseNULL(_actionInterrupt);
    
    if (_lock) {
        IOLockFree(_lock);
        _lock = NULL;
    }
    
    super::free();
}

IOHIDActionQueue *IOHIDActionQueue::actionQueue(OSObject *owner, IOWorkLoop *workLoop)
{
    IOHIDActionQueue *me = new IOHIDActionQueue;
    
    if (me && !me->init(owner, workLoop)) {
        me->release();
        return 0;
    }
    
    return me;
}

void IOHIDActionQueue::enqueueIOHIDAction(IOHIDAction *action)
{
    unsigned int count = 0;
    
    IOLockLock(_lock);
    _actionArray->setObject(action);
    count = _actionArray->getCount();
    IOLockUnlock(_lock);
    
    if (count == 1) {
        _actionInterrupt->interruptOccurred(NULL, NULL, 0);
    }
}

IOHIDAction *IOHIDActionQueue::dequeueIOHIDAction()
{
    IOHIDAction *action = NULL;
    
    IOLockLock(_lock);
    if (_actionArray->getCount()) {
        action = (IOHIDAction *)_actionArray->getObject(0);
        if (action) {
            action->retain();
            _actionArray->removeObject(0);
        }
    }
    IOLockUnlock(_lock);
    
    return action;
}

void IOHIDActionQueue::handleIOHIDAction(IOInterruptEventSource *sender __unused, int count __unused)
{
    IOHIDAction *action = NULL;
    
    while ((action = dequeueIOHIDAction())) {
        action->runAction();
        action->release();
    }
}

void IOHIDActionQueue::dispatchAsync(IOHIDActionBlock action)
{
    IOHIDAction *hidAction = NULL;
    
    assert(_state == kIOHIDActionQueueStateActivated);
    
    hidAction = IOHIDAction::hidAction(action);
    if (hidAction) {
        enqueueIOHIDAction(hidAction);
        hidAction->release();
    }
}

void IOHIDActionQueue::dispatchSync(IOHIDActionBlock action)
{
    assert(_state == kIOHIDActionQueueStateActivated);
    
    _commandGate->runActionBlock(^IOReturn{
        (action)();
        return kIOReturnSuccess;
    });
}

void IOHIDActionQueue::activate()
{
    if (OSBitOrAtomic(kIOHIDActionQueueStateActivated, &_state) & kIOHIDActionQueueStateActivated) {
        return;
    }
    
    _workLoop->addEventSource(_actionInterrupt);
    _workLoop->addEventSource(_commandGate);
}

void IOHIDActionQueue::cancel()
{
    IOHIDAction *hidAction = NULL;
    
    if (OSBitOrAtomic(kIOHIDActionQueueStateCancelled, &_state) & kIOHIDActionQueueStateCancelled) {
        return;
    }
    
    IOHIDActionBlock cancelBlock = ^() {
        _workLoop->removeEventSource(_actionInterrupt);
        _workLoop->removeEventSource(_commandGate);
        
        if (_cancelHandler) {
            (_cancelHandler)();
            Block_release(_cancelHandler);
        }
    };
    
    hidAction = IOHIDAction::hidAction(cancelBlock);
    if (hidAction) {
        enqueueIOHIDAction(hidAction);
        hidAction->release();
    }
}

void IOHIDActionQueue::setCancelHandler(IOHIDActionBlock handler)
{
    assert(_state == kIOHIDActionQueueStateInactive);
    
    if (handler) {
        _cancelHandler = Block_copy(handler);
    }
}
