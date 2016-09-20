//
//  DeviceServer.m
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#import "DeviceServer.h"
#import "DeviceCommon.h"
#import "QServer.h"
#import <IOKit/hid/IOHIDManager.h>

@interface DeviceServer () <
    QServerDelegate,
    NSStreamDelegate
>

@property (nonatomic, strong, readwrite) QServer *              server;
@property (nonatomic, strong, readwrite) NSInputStream *        inputStream;
@property (nonatomic, strong, readwrite) NSOutputStream *       outputStream;
@property (nonatomic, assign, readwrite) NSUInteger             streamOpenCount;

@end

@implementation DeviceServer
{
    IOHIDManagerRef manager;
}

- (id)initWithBonjour:(BOOL)bonjour
// See comment in header.
{
    self = [super initWithServerType:kHIDRelayServerBonjourType port:kHIDRelayServerPort withBonjour:bonjour];
    
    if ( self != nil ) {
        self.server = [[QServer alloc] initWithDomain:@"local." type:kHIDRelayServerBonjourType name:nil preferredPort:kHIDRelayServerPort];
        [self.server setDelegate:self];
        [self.server start];
        
        if ( !self.useBonjour ) {
            self.mcDiscovery = [[MCDeviceDiscovery alloc] init];
            [self.mcDiscovery performHandshakeWithCompletionBlock:^(BOOL didComplete, NSString *localInterfaceName __unused, NSString *remoteIP __unused) {
                if (didComplete) {
                    NSLog(@"MC Service Vending Completed");
                }
            }];
        }
    }
    
    return self;
}

#pragma mark - HID management

static void MatchingCallback (DeviceServer * server, IOReturn result, void * sender, IOHIDDeviceRef device)
{
    NSMutableDictionary * dictionary = [NSMutableDictionary dictionaryWithCapacity:0];
    
    NSObject * object;
    
    object = (__bridge id)(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey)));
    if ( object )
        [dictionary setObject:object forKey:CFSTR(kIOHIDVendorIDKey)];
    
    object = (__bridge id)(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey)));
    if ( object )
        [dictionary setObject:object forKey:CFSTR(kIOHIDProductIDKey)];
    
    object = (__bridge id)(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDReportDescriptorKey)));
    if ( object )
        [dictionary setObject:object forKey:CFSTR(kIOHIDReportDescriptorKey)];

    [dictionary setObject:@"Relay" forKey:CFSTR(kIOHIDTransportKey)];
    
    [server sendObjectPayload:dictionary forType:kHIDPayloadTypeEnumeration forDeviceID:(uintptr_t)device];
}

static void TerminationCallback (DeviceServer * server, IOReturn result, void * sender, IOHIDDeviceRef device)
{
    [server sendGenericPayloadBytes:NULL maxLength:0 forType:kHIDPayloadTypeTermination forDeviceID:(uintptr_t)device];
}

static void InputReportCallback(DeviceServer * server, IOReturn result, void * sender, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength)
{
    
    [server sendReportBytes:report maxLength:reportLength forReportType:type forReportID: reportID forDeviceID:(uintptr_t)sender];
}

#pragma mark - Connection management

- (void)stream:(NSStream *)stream handleEvent:(NSStreamEvent)eventCode
{
#pragma unused(stream)
    
    switch(eventCode) {
            
        case NSStreamEventOpenCompleted: {
            
            /* start the transmission of HID reports */
            
            self.streamOpenCount += 1;
            assert(self.streamOpenCount <= 2);
            
        } break;
            
        case NSStreamEventHasSpaceAvailable: {
            assert(stream == self.outputStream);
            // do nothing
        } break;
            
        case NSStreamEventHasBytesAvailable: {
            HIDPayloadHeader    header = {};
            NSInteger           bytesRead;
            
            assert(stream == self.inputStream);
            
            bytesRead = [self.inputStream read:(uint8_t *)&header maxLength:sizeof(HIDPayloadHeader)];
            if (bytesRead < sizeof(HIDPayloadHeader)) {
                // Do nothing; we'll handle EOF and error in the
                // NSStreamEventEndEncountered and NSStreamEventErrorOccurred case,
                // respectively.
            } else {
                
                switch ( header.type ) {
                    case kHIDPayloadTypeMatching: {
                        
                        HIDPayloadGeneric matchingHeader = {};
                        
                        bytesRead = [self.inputStream read:(uint8_t *)&matchingHeader maxLength:sizeof(HIDPayloadGeneric)];
                        
                        if ( bytesRead >= sizeof(HIDPayloadGeneric)) {
                            uint8_t * descriptor = malloc(matchingHeader.length);
                            
                            if ( descriptor ) {
                                
                                bytesRead = [self.inputStream read:descriptor maxLength:matchingHeader.length];
                                
                                if ( bytesRead >= matchingHeader.length) {
                                    
                                    CFReadStreamRef stream = CFReadStreamCreateWithBytesNoCopy(kCFAllocatorDefault, descriptor, matchingHeader.length, kCFAllocatorNull);
                                    
                                    if ( stream ) {
                                        
                                        if ( CFReadStreamOpen(stream) ) {
                                            CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0;
                                            CFTypeRef object;
                                            object = CFPropertyListCreateFromStream(kCFAllocatorDefault,stream,matchingHeader.length,kCFPropertyListMutableContainersAndLeaves,&format,NULL);
                                            if ( object ) {
                                                if ( CFDictionaryGetTypeID() == CFGetTypeID(object)) {
                                                    NSLog(@"Matching dictionary received %@\n", object);
                                                    if ( !self->manager ) {
                                                        self->manager = IOHIDManagerCreate(kCFAllocatorDefault, 0);
                                                        IOHIDManagerRegisterDeviceMatchingCallback(manager, (IOHIDDeviceCallback)MatchingCallback, (__bridge void *)(self));
                                                        IOHIDManagerRegisterDeviceRemovalCallback(manager, (IOHIDDeviceCallback)TerminationCallback, (__bridge void *)(self));
                                                        IOHIDManagerRegisterInputReportCallback(manager, (IOHIDReportCallback)InputReportCallback, (__bridge void *)(self));
                                                        IOHIDManagerScheduleWithRunLoop(self->manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
                                                    }
                                                    IOHIDManagerSetDeviceMatching(self->manager, object);
                                                    IOHIDManagerOpen(self->manager, 0);
                                                }
                                                CFRelease(object);
                                            }
                                            CFReadStreamClose(stream);
                                        }
                                        CFRelease(stream);
                                    }
                                }
                            }
                            
                            free(descriptor);
                        }
                    } break;
                    default:
                        break;
                }
                
                

            }
        } break;
            // fall through
        case NSStreamEventEndEncountered: {
            if ( self->manager ) {
                CFRelease(self->manager);
                self->manager = NULL;
            }
            [self closeStreams];
            
            self.mcDiscovery = [[MCDeviceDiscovery alloc] init];
            [self.mcDiscovery performHandshakeWithCompletionBlock:^(BOOL didComplete, NSString *localInterfaceName __unused, NSString *remoteIP __unused) {
                if (didComplete) {
                    NSLog(@"MC Service Vending Completed");
                }
            }];

        } break;
        default:
            break;
    }
}

@end
