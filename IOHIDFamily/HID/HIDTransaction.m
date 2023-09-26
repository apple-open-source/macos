/*!
 * HIDTransaction.m
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDLib.h>
#import <HID/HIDTransaction.h>
#import <HID/HIDDevice.h>
#import <HID/HIDElement_Internal.h>
#import <HID/NSError+IOReturn.h>
#import <IOKit/hid/IOHIDLibPrivate.h>

@implementation HIDTransaction {
    IOHIDTransactionRef _transaction;
}

- (instancetype)initWithDevice:(HIDDevice *)device
{
    self = [super init];
    
    if (!self) {
        return self;
    }
    
    _transaction = IOHIDTransactionCreate(kCFAllocatorDefault,
                                          (__bridge IOHIDDeviceRef)device,
                                          kIOHIDTransactionDirectionTypeInput,
                                          kIOHIDTransactionOptionsWeakDevice);
    if (!_transaction) {
        return nil;
    }
    
    return self;
}

- (void)dealloc
{
    if (_transaction) {
        CFRelease(_transaction);
    }
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@", _transaction];
}

- (HIDTransactionDirectionType)direction
{
    return (HIDTransactionDirectionType)IOHIDTransactionGetDirection(_transaction);
}

- (void)setDirection:(HIDTransactionDirectionType)direction
{
    IOHIDTransactionSetDirection(_transaction,
                                 (IOHIDTransactionDirectionType)direction);
}

typedef void (^CommitCallbackInternal)(IOReturn status);

static void asyncCommitCallback(void   * context,
                                IOReturn result,
                                void   * sender __unused)
{
    HIDTransactionCommitCallback callback = (__bridge HIDTransactionCommitCallback)context;

    callback(result);

    Block_release(context);
}

- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
                 error:(out NSError **)outError
{
    return [self commitElements:elements error:outError timeout:0 callback:nil];
}

- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
                 error:(out NSError **)outError
               timeout:(NSInteger)timeout
              callback:(HIDTransactionCommitCallback _Nullable)callback
{
    IOReturn ret = kIOReturnError;
    
    for (HIDElement * element in elements) {
        IOHIDTransactionAddElement(_transaction, (__bridge IOHIDElementRef)element);
    }

    if (callback) {
        ret = IOHIDTransactionCommitWithCallback(_transaction, timeout, asyncCommitCallback, Block_copy((__bridge void *)callback));
    } else {
        ret = IOHIDTransactionCommit(_transaction);
    }
    
    if (ret != kIOReturnSuccess) {
        IOHIDTransactionClear(_transaction);
        
        if (outError) {
            *outError = [NSError errorWithIOReturn:ret];
        }

        if (callback) {
            Block_release((__bridge void *)callback);
        }
        
        return NO;
    }

    IOHIDTransactionClear(_transaction);
    return (ret == kIOReturnSuccess);
}

@end
