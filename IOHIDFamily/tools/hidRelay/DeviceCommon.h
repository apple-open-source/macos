//
//  DeviceCommon.h
//  HIDRelay
//
//  Created by Roberto Yepez on 8/5/14.
//  Copyright (c) 2014 Roberto Yepez. All rights reserved.
//

#ifndef HIDRelay_DeviceCommon_h
#define HIDRelay_DeviceCommon_h

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import "QServer.h"
#import "MCDeviceDiscovery.h"

static NSString * kHIDRelayServerBonjourType = @"_hidrelay_server._tcp.";

#define kHIDRelayServerPort 'hr'

typedef enum {
    kHIDPayloadTypeMatching,
    kHIDPayloadTypeEnumeration,
    kHIDPayloadTypeTermination,
    kHIDPayloadTypeReport
} HIDPayloadType;

typedef struct __attribute__((packed)) {
    uint32_t    length;
    uint8_t *   data[];
} HIDPayloadGeneric;

typedef struct __attribute__((packed)) {
    uint32_t        length;
    uint32_t        reportID;
    uint32_t        reportType;
    uint8_t *       data[];
} HIDPayloadReport;

typedef struct __attribute__((packed)) {
    uint64_t        deviceID;
    HIDPayloadType  type;
} HIDPayloadHeader;

typedef struct __attribute__((packed)) {
    
    HIDPayloadHeader header;
    
    union {
        HIDPayloadGeneric   generic;
        HIDPayloadReport    report;
    }payload;
    
}HIDPayload;

@interface DeviceCommon : NSObject

@property (nonatomic, strong, readwrite) QServer *              server;
@property (nonatomic, strong, readwrite) NSInputStream *        inputStream;
@property (nonatomic, strong, readwrite) NSOutputStream *       outputStream;
@property (nonatomic, assign, readwrite) NSUInteger             streamOpenCount;
@property (nonatomic, strong, readwrite) NSMutableArray *       services;                       // of NSNetService, sorted by name
@property (nonatomic, strong, readwrite) NSNetServiceBrowser *  browser;
@property (nonatomic, strong, readwrite) NSNetService *         localService;
@property (nonatomic, strong, readwrite) MCDeviceDiscovery *    mcDiscovery;
@property (nonatomic, assign, readwrite) NSInteger              port;
@property (nonatomic, assign, readwrite) BOOL                   useBonjour;

- (id)initWithServerType:(NSString *)type port:(NSInteger)port withBonjour:(BOOL)bonjour;

- (void)sendObjectPayload:(NSObject *)object forType: (HIDPayloadType)type forDeviceID: (uint64_t)deviceID;
- (void)sendGenericPayloadBytes:(const uint8_t *)data maxLength:(NSUInteger)length forType:(HIDPayloadType)type forDeviceID: (uint64_t)deviceID;
-(void)sendReportBytes:(uint8_t*)report maxLength:(NSUInteger)reportLength forReportType:(IOHIDReportType)reportType forReportID: (uint8_t)reportID forDeviceID:(uintptr_t)deviceID;
- (void)send:(const uint8_t *)data maxLength:(NSUInteger)length;

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser didRemoveService:(NSNetService *)service moreComing:(BOOL)moreComing;
- (void)openStreams;
- (void)closeStreams;

@end

#endif
