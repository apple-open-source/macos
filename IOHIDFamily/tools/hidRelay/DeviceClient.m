//
//  DeviceClient.m
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#import "DeviceClient.h"
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDUserDevice.h>

@interface DeviceClient ()

@property (nonatomic, strong, readwrite) NSDictionary *         matching;
@property (nonatomic, strong, readwrite) NSMutableDictionary *  devices;
@property (nonatomic, assign, readwrite) BOOL                   matchingDispatched;
@property (nonatomic, assign, readwrite) NSString *             interfaceName;

@end

@implementation DeviceClient

- (id)initWithMatchingDictionary:(NSDictionary *)matching withBonjour:(BOOL)bonjour withInterface:(NSString *)interface
// See comment in header.
{
    self = [super initWithServerType:nil port:kHIDRelayServerPort withBonjour:bonjour];
    if (self != nil) {
        self.matching = matching ? matching : [NSDictionary dictionary];
        self.devices = [NSMutableDictionary dictionaryWithCapacity:0];
        self.interfaceName = interface;
        
        [self startBrowserWithType:kHIDRelayServerBonjourType];
    }
    return self;
}

- (void)startBrowserWithType:(NSString *) type
{
    if ( self.useBonjour ) {
        [self.browser stop];
        self.browser = nil;
        
        [self.services removeAllObjects];
        
        self.browser = [[NSNetServiceBrowser alloc] init];
        [self.browser setDelegate:self];
        [self.browser searchForServicesOfType:type inDomain:@"local"];
        [self.browser scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    } else {
        NSLog(@"Starting Multicast discovery\n");
        self.matchingDispatched = NO;
        self.mcDiscovery = [[MCDeviceDiscovery alloc] initWithInterface:self.interfaceName];
        [self.mcDiscovery performHandshakeWithCompletionBlock:^(BOOL didComplete, NSString *localInterfaceName __unused, NSString *remoteIP) {
            if (didComplete) {
                NSInputStream *     inStream;
                NSOutputStream *    outStream;

#if TARGET_OS_EMBEDDED
                [NSStream getStreamsToHostWithName:remoteIP port:self.port inputStream:&inStream outputStream:&outStream];
#else
                [NSStream getStreamsToHost:[NSHost hostWithAddress:remoteIP] port:self.port inputStream:&inStream outputStream:&outStream];
#endif
                
                if (self.inputStream != inStream && self.outputStream != outStream){
                    
                    [self closeStreams];
                    
                    self.inputStream  = inStream;
                    self.outputStream = outStream;
                    
                    [self openStreams];
                }
            }
        }];
        
    }
    
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
            
            if ( self.matchingDispatched == NO ) {
                [self sendObjectPayload:self.matching forType:kHIDPayloadTypeMatching forDeviceID:0];
                self.matchingDispatched = YES;
            }
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
                    case kHIDPayloadTypeEnumeration: {
                        
                        HIDPayloadGeneric descriptorHeader = {};
                        
                        bytesRead = [self.inputStream read:(uint8_t *)&descriptorHeader maxLength:sizeof(HIDPayloadGeneric)];
                        
                        if ( bytesRead >= sizeof(HIDPayloadGeneric)) {
                            uint8_t * descriptor = malloc(descriptorHeader.length);
                            
                            if ( descriptor ) {
                                
                                bytesRead = [self.inputStream read:descriptor maxLength:descriptorHeader.length];

                                if ( bytesRead >= descriptorHeader.length) {
                                    
                                    CFReadStreamRef stream = CFReadStreamCreateWithBytesNoCopy(kCFAllocatorDefault, descriptor, descriptorHeader.length, kCFAllocatorNull);
                                    
                                    if ( stream ) {
                                        
                                        if ( CFReadStreamOpen(stream) ) {
                                            CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0;
                                            CFTypeRef object;
                                            object = CFPropertyListCreateFromStream(kCFAllocatorDefault,stream,descriptorHeader.length,kCFPropertyListMutableContainersAndLeaves,&format,NULL);
                                            if ( object ) {
                                                if (CFGetTypeID(object) == CFDictionaryGetTypeID()) {
                                                    IOHIDUserDeviceRef device = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)(object));
                                                    if ( device ) {
                                                        NSLog(@"Device created for 0x%llx with %@\n", header.deviceID, object);
                                                        [self.devices setObject:(__bridge id)(device) forKey:[NSNumber numberWithLongLong:header.deviceID]];
                                                        CFRelease(device);
                                                    }
                                                }
                                                CFRelease(object);
                                            }
                                            
                                            CFReadStreamClose(stream);
                                        }
                                        
                                        CFRelease(stream);
                                    }
                                    
                                }
                                
                                free(descriptor);
                            }
                        }
                        
                    } break;
                    case kHIDPayloadTypeReport: {
                        IOHIDUserDeviceRef device = (__bridge IOHIDUserDeviceRef)[self.devices objectForKey: [NSNumber numberWithLongLong:header.deviceID]];

                        HIDPayloadReport reportHeader = {};
                        
                        bytesRead = [self.inputStream read:(uint8_t *)&reportHeader maxLength:sizeof(HIDPayloadReport)];
                        
                        if ( bytesRead >= sizeof(HIDPayloadReport)) {
                            uint8_t * report = malloc(reportHeader.length);
                            
                            if ( report ) {
                                
                                bytesRead = [self.inputStream read:report maxLength:reportHeader.length];
                                
                                if ( bytesRead >= reportHeader.length) {
                                    IOHIDUserDeviceHandleReport(device, report, reportHeader.length);
                                }
                                
                                free(report);
                            }
                        }                        
                    } break;
                    case kHIDPayloadTypeTermination: {
                        NSLog(@"Device terminated for 0x%llx\n", header.deviceID);
                        [self.devices removeObjectForKey:[NSNumber numberWithLongLong:header.deviceID]];
                    } break;
                    default:
                        break;
                    
                        
                }
            }
        } break;
            
        case NSStreamEventErrorOccurred:
        case NSStreamEventEndEncountered:
            [self closeStreams];
            [self.devices removeAllObjects];
            if ( !self.useBonjour ) {
                [self startBrowserWithType: kHIDRelayServerBonjourType];
            }
            break;
    }
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser didRemoveService:(NSNetService *)service moreComing:(BOOL)moreComing
{
    self.matchingDispatched = NO;
    [super netServiceBrowser: browser didRemoveService: service moreComing:moreComing];
}

@end
