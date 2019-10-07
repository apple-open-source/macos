//
//  HIDTransaction.m
//  HID
//
//  Created by dekom on 10/5/17.
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDLib.h>
#import "HIDTransaction.h"
#import "HIDDevicePrivate.h"
#import "HIDElementPrivate.h"
#import "NSError+IOReturn.h"
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

- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
                 error:(out NSError **)outError
{
    IOReturn ret = kIOReturnError;
    
    for (HIDElement *element in elements) {
        IOHIDTransactionAddElement(_transaction,
                                   (__bridge IOHIDElementRef)element);
        
        if (self.direction == kIOHIDTransactionDirectionTypeOutput) {
            IOHIDTransactionSetValue(_transaction,
                                     (__bridge IOHIDElementRef)element,
                                     element.valueRef,
                                     kIOHIDOptionsTypeNone);
        }
    }
    
    ret = IOHIDTransactionCommit(_transaction);
    
    if (ret != kIOReturnSuccess) {
        IOHIDTransactionClear(_transaction);
        
        if (outError) {
            *outError = [NSError errorWithIOReturn:ret];
        }
        
        return NO;
    }
    
    if (self.direction == kIOHIDTransactionDirectionTypeInput) {
        for (HIDElement *element in elements) {
            IOHIDValueRef val = IOHIDTransactionGetValue(
                                            _transaction,
                                            (__bridge IOHIDElementRef)element,
                                            kIOHIDOptionsTypeNone);
            if (val) {
                [element setValueRef:val];
            }
        }
    }
    
    IOHIDTransactionClear(_transaction);
    return (ret == kIOReturnSuccess);
}

@end
