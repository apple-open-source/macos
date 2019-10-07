//
//  IOHIDActionQueueTestDriver.cpp
//  IOHIDFamily
//
//  Created by dekom on 3/14/18.
//

#include "IOHIDActionQueueTestDriver.h"
#include <AssertMacros.h>
#include <IOKit/assert.h>
#include <os/log.h>

#define LOG(fmt, ...) os_log_error(OS_LOG_DEFAULT, fmt, ##__VA_ARGS__); kprintf(fmt, ##__VA_ARGS__)

#define super IOService

OSDefineMetaClassAndStructors(IOHIDActionQueueTestDriver, super)

bool IOHIDActionQueueTestDriver::start(IOService *provider)
{
    bool result = false;
    
    require(super::start(provider), exit);
    
    _testWL1 = IOWorkLoop::workLoop();
    _interrupt1 = IOInterruptEventSource::interruptEventSource(this,
                                                               OSMemberFunctionCast(IOInterruptEventAction,
                                                                                    this, &IOHIDActionQueueTestDriver::interruptAction1));
    require_noerr(_testWL1->addEventSource(_interrupt1), exit);
    
    _testWL2 = IOWorkLoop::workLoop();
    _interrupt2 = IOInterruptEventSource::interruptEventSource(this,
                                                               OSMemberFunctionCast(IOInterruptEventAction,
                                                                                    this, &IOHIDActionQueueTestDriver::interruptAction2));
    require_noerr(_testWL2->addEventSource(_interrupt2), exit);
    
    _actionWL = IOWorkLoop::workLoop();
    require(_actionWL, exit);
    
    _actionQueue = IOHIDActionQueue::actionQueue(this, _actionWL);
    require(_actionQueue, exit);
    
    _actionQueue->setCancelHandler(^{
        cancelHandlerCall();
    });
    
    _actionQueue->activate();
    
    registerService();
    result = true;
    
    LOG("IOHIDActionQueueTestDriver::start\n");
    
exit:
    if (!result) {
        stop(provider);
    }
    
    return result;
}

void IOHIDActionQueueTestDriver::stop(IOService *provider)
{
    _testWL1->removeEventSource(_interrupt1);
    _testWL2->removeEventSource(_interrupt2);
    
    LOG("IOHIDActionQueueTestDriver::stop\n");
    super::stop(provider);
}

void IOHIDActionQueueTestDriver::free()
{
    LOG("IOHIDActionQueueTestDriver::free\n");
    OSSafeReleaseNULL(_testWL1);
    OSSafeReleaseNULL(_testWL2);
    OSSafeReleaseNULL(_interrupt1);
    OSSafeReleaseNULL(_interrupt2);
    
    OSSafeReleaseNULL(_actionWL);
    OSSafeReleaseNULL(_actionQueue);
    super::free();
}

IOReturn IOHIDActionQueueTestDriver::setProperties(OSObject *properties)
{
    OSDictionary *propertyDict = NULL;
    OSBoolean *start = NULL;
    IOReturn result = kIOReturnUnsupported;
    
    propertyDict = OSDynamicCast(OSDictionary, properties);
    require(propertyDict, exit);
    
    start = OSDynamicCast(OSBoolean, propertyDict->getObject("StartTest"));
    if (start == kOSBooleanTrue) {
        runTest();
        result = kIOReturnSuccess;
    }
    
exit:
    if (result != kIOReturnSuccess) {
        result = super::setProperties(properties);
    }
    
    return result;
}

void IOHIDActionQueueTestDriver::interruptAction1(IOInterruptEventSource *sender __unused, int count __unused)
{
    LOG("IOHIDActionQueueTestDriver::interruptAction1\n");
    
    for (unsigned int i = 1; i <= 1000; i++) {
        _actionQueue->dispatchAsync(^{
            assert(_actionCounter1 == i - 1);
            _actionCounter1 = i;
            IOSleep(1);
        });
    }
    
    LOG("IOHIDActionQueueTestDriver::interruptAction1 exit\n");
}

void IOHIDActionQueueTestDriver::interruptAction2(IOInterruptEventSource *sender __unused, int count __unused)
{
    LOG("IOHIDActionQueueTestDriver::interruptAction2\n");
    
    for (unsigned int i = 1; i <= 1000; i++) {
        _actionQueue->dispatchAsync(^{
            assert(_actionCounter2 == i - 1);
            _actionCounter2 = i;
            IOSleep(1);
        });
    }
    
    LOG("IOHIDActionQueueTestDriver::interruptAction2 exit\n");
}

void IOHIDActionQueueTestDriver::runTest()
{
    __block int counter = 0;
    __block int counter1 = 0;
    __block int counter2 = 0;
    
    LOG("IOHIDActionQueueTestDriver::runTest\n");
    
    setProperty("TestStarted", kOSBooleanTrue);
    
    _interrupt1->interruptOccurred(0, 0, 0);
    _interrupt2->interruptOccurred(0, 0, 0);
    
    for (unsigned int i = 1; i <= 1000; i++) {
        _actionQueue->dispatchAsync(^{
            assert(_actionCounter == i - 1);
            _actionCounter = i;
            IOSleep(1);
        });
    }
    
    LOG("IOHIDActionQueueTestDriver::runTest done, sleeping to allow other threads to run\n");
    
    IOSleep(5000);
    
    LOG("IOHIDActionQueueTestDriver::runTest calling dispatchSync\n");
    
    _actionQueue->dispatchSync(^{
        LOG("IOHIDActionQueueTestDriver::runTest dispatchSync _actionCounter: %d _actionCounter1: %d _actionCounter2: %d\n",
                    _actionCounter, _actionCounter1, _actionCounter2);
        counter = _actionCounter;
        counter1 = _actionCounter1;
        counter2 = _actionCounter2;
    });
    
    LOG("IOHIDActionQueueTestDriver::runTest dispatch sync done counter: %d counter1: %d counter2: %d\n",
        counter, counter1, counter2);
    
    assert(counter == 1000);
    assert(counter1 == 1000);
    assert(counter2 == 1000);
    
    setProperty("TestFinished", kOSBooleanTrue);
    
    _actionQueue->cancel();
}

void IOHIDActionQueueTestDriver::cancelHandlerCall()
{
    LOG("IOHIDActionQueueTestDriver::cancelHandlerCall\n");
    setProperty("CancelHandlerCalled", kOSBooleanTrue);
}
