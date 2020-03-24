//
//  HIDRemoeSimpleServer.m
//  RemoteHID
//
//  Created by yg on 1/14/18.
//

#import <Foundation/Foundation.h>
#import "HIDRemoteDeviceServer.h"
#import "HIDRemoteSimpleProtocol.h"
#import "RemoteHIDPrivate.h"
#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFUnserialize.h>
#import <IOKit/IOMessage.h>
#import <os/state_private.h>

#import "IOHIDPrivateKeys.h"

@interface HIDRemoteDevice ()

@property        dispatch_semaphore_t   semaphore;
@property        NSData *               lastGetReport;
@property        BOOL                   waitForReport;
@property        uint32_t               handleReportCount;
@property        uint32_t               handleReportError;
@property        io_object_t            intNotify;
@property        IONotificationPortRef  intPort;
@property __weak HIDRemoteDeviceServer *server;

@end


@implementation HIDRemoteDevice

-(nullable instancetype)initWithProperties:(nonnull NSDictionary *)properties
{
    self = [super initWithProperties:properties];
    if (!self) {
        return self;
    }
    self.semaphore = dispatch_semaphore_create(0);
    return self;
}

-(IOReturn) getReportHandler:(__unused IOHIDReportType) type
                    reportID:(__unused uint8_t) reportID
                      report:( uint8_t * _Nonnull ) report
                reportLength:(NSUInteger) reportLength
{
    IOReturn status = kIOReturnAborted;
    if (self.waitForReport) {
        self.lastGetReport = [NSData dataWithBytes:report length:reportLength];
        dispatch_semaphore_signal(self.semaphore);
        status = kIOReturnSuccess;
    }
    return status;
}

- (NSString *)description {
    uint64_t serviceID = 0;
    IORegistryEntryGetRegistryEntryID(self.service, &serviceID);
    return [NSString stringWithFormat:@"<HIDRemoteHIDUserDevice:%p id:%lld service:%llx handleReportCount:%u handleReportError:%u device:%@>",
                                      (void *) self, self.deviceID, serviceID, self.handleReportCount, self.handleReportError, [super description]];
}

- (void)cancel
{
    if (self.intPort) {
        IONotificationPortSetDispatchQueue (self.intPort, NULL);
        IONotificationPortDestroy(self.intPort);
    }
    if (self.intNotify) {
        IOObjectRelease(self.intNotify);
    }

    [super cancel];
}

@end

#define REMOTE_DEVICE_LOG_SIZE 50

@interface HIDRemoteDeviceServer ()
{
    NSMutableArray<NSString *>* _prevDeviceLog;
    os_state_handle_t           _stateHandler;
}

@end

@implementation HIDRemoteDeviceServer

-(nullable instancetype) initWithQueue:(dispatch_queue_t) queue
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    self->_queue   = queue;
    self->_devices = [[NSMutableDictionary alloc] init];
    _prevDeviceLog = [NSMutableArray new];
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<HIDRemoteDeviceServer state:%@ %@>",
            [self copyState], [super description]];
}


-(BOOL) connectEndpoint:(__nonnull id) endpoint
{
    NSMutableDictionary * endpointDevices = self.devices [endpoint];
    if (endpointDevices) {
        [endpointDevices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key __unused, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
            HIDUserDevice * device = (HIDUserDevice *)obj;
            [device cancel];
        }];
        [endpointDevices removeAllObjects];
    } else {
        self.devices [endpoint] = [[NSMutableDictionary alloc] init];
    }
    return TRUE;
}

-(void) disconnectEndpoint:(__nonnull id) endpoint
{
    NSMutableDictionary * endpointDevices = self.devices [endpoint];
    
    [endpointDevices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key __unused, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
        HIDUserDevice * device = (HIDUserDevice *)obj;

        if (_prevDeviceLog.count >= REMOTE_DEVICE_LOG_SIZE) {
            [_prevDeviceLog removeObjectAtIndex:0];
        }
        [_prevDeviceLog addObject:device.description];

        [device cancel];
    }];
    
    self.devices [endpoint] = nil;
    
    os_log_debug (RemoteHIDLog (), "DisconnectEP devices:%@", self.devices);
}

-(BOOL) createRemoteDevice:(__nonnull id) endpoint  deviceID:(uint64_t) deviceID property:( NSMutableDictionary * __nonnull) property
{
    
    HIDRemoteDevice * device = [[HIDRemoteDevice alloc] initWithProperties:property];
    if (!device) {
        os_log_error (RemoteHIDLog (), "HIDUserDevice failed, property:%@", property);
        return NO;
    }
    
    __weak HIDRemoteDevice  *weakDevice = device;

    [device setSetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, const void *report, NSInteger reportLength) {
        __strong HIDRemoteDevice * strongDevice = weakDevice;
        if (!strongDevice) {
            return kIOReturnNoDevice;
        }
        
        NSData *reportData = [NSData dataWithBytesNoCopy:(void *)report length:reportLength freeWhenDone:false];

        os_log_info (RemoteHIDLog (), "[device:%d] setReport type:%ld reportID:%ld report:%@", (int)strongDevice.deviceID, (long)type, (long)reportID, reportData);
        
        IOReturn status = [self remoteDeviceSetReport:strongDevice type:type reportID:reportID report:reportData];
        return status;
    }];

    [device setGetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, void *report, NSInteger *reportLength) {
        __strong HIDRemoteDevice * strongDevice = weakDevice;
        if (!strongDevice) {
            return kIOReturnNoDevice;
        }
        
        os_log_info (RemoteHIDLog (), "[device:%d] getReport type:%ld reportID:%ld", (int)strongDevice.deviceID, (long)type, (long)reportID);
        NSMutableData * reportCopy;
        NSData *        reportData;
        long            semaStatus;
        IOReturn        status;

        reportData = [NSData dataWithBytesNoCopy:report length:*reportLength freeWhenDone:false];
        reportCopy = [reportData mutableCopy];
        
        strongDevice.lastGetReport = nil;
        strongDevice.waitForReport = YES;
        
        status = [self remoteDeviceGetReport:strongDevice type:type reportID:reportID report:reportCopy];
        require_action_quiet(status == kIOReturnSuccess, exit, os_log_error (RemoteHIDLog (), "[device:%d] remoteDeviceGetReport:0x%x", (int)strongDevice.deviceID, status));

        semaStatus = dispatch_semaphore_wait(strongDevice.semaphore, dispatch_time(DISPATCH_TIME_NOW, kRemoteHIDDeviceTimeout * NSEC_PER_SEC));
        require_action_quiet(semaStatus == 0, exit, status = kIOReturnTimeout; os_log_error (RemoteHIDLog (), "[device:%d] remoteDeviceGetReport timeout", (int)strongDevice.deviceID));

        require_action_quiet(strongDevice.lastGetReport, exit, status = kIOReturnError; os_log_error (RemoteHIDLog (), "[device:%d] invalid report :%@", (int)strongDevice.deviceID, strongDevice.lastGetReport));
        
        memcpy((void *)reportData.bytes, ((NSData *)strongDevice.lastGetReport).bytes, ((NSData *)strongDevice.lastGetReport).length);
        *reportLength = ((NSData *)strongDevice.lastGetReport).length;
    
    exit:
        
        strongDevice.waitForReport = NO;
        
        return status;
    }];

    device.server = self;

    [device setDispatchQueue:self.queue];
    
    device.endpoint = endpoint;
    device.deviceID = deviceID;
    
    [device activate];

    NSMutableDictionary * endpointDevices = self.devices [endpoint];
    
    HIDRemoteDevice * tmpDevice = endpointDevices [@(deviceID)];
    if (tmpDevice) {
        [tmpDevice cancel];
    }
    endpointDevices [@(deviceID)] = device;

    os_log (RemoteHIDLog (), "Create device:%@ for endpoint:%@ property:%@", device, endpoint, property);

    return YES;
}

-(uint64_t) syncRemoteTimestamp:(__unused uint64_t)inTimestamp forEndpoint:(__unused __nonnull id)endpoint
{
    // Implemented in subclasses
    return 0;
}

-(IOReturn) remoteDeviceSetReport:(__unused HIDRemoteDevice *) device type:(__unused HIDReportType) type reportID:(__unused uint8_t) reportID report:(__unused NSData *) report
{
    return kIOReturnUnsupported;
}

-(IOReturn) remoteDeviceGetReport:(__unused HIDRemoteDevice *) device type:(__unused HIDReportType) type reportID:(__unused uint8_t) reportID report:(__unused NSMutableData *) report
{
    return kIOReturnUnsupported;
}

-(void) removeRemoteDevice:(__nonnull id) endpoint deviceID:(uint64_t) deviceID
{
    NSMutableDictionary * endpointDevices = self.devices [endpoint];
    HIDUserDevice * device = endpointDevices [@(deviceID)];
    if (device) {
        os_log (RemoteHIDLog (), "Remove device:%@ for endpoint:%@", device, endpoint);
        [device cancel];
    } else {
         os_log_error (RemoteHIDLog (), "Device:%lld not found for endpoint:%@", deviceID, endpoint);
    }
    endpointDevices [@(deviceID)] = nil;
    
    os_log_debug (RemoteHIDLog (), "Remaining devices:%@", self.devices);
    
}

-(BOOL) remoteDeviceReportHandler:(HIDUserDevice * __nonnull ) device packet:(HIDDeviceReport *) packet
{
    NSData * report = [NSData dataWithBytes:&(packet->data[0]) length:(packet->header.length - sizeof(HIDDeviceReport))];
    NSError * error = nil;
    BOOL status = [device handleReport:report error:&error];
    if (status == NO) {
        os_log_error (RemoteHIDLog (), "handleReport:%@", error);
    }
    return status;
}

-(BOOL) remoteDeviceTimestampedReportHandler:(__nonnull id) endpoint device:(HIDUserDevice * __nonnull ) device packet:(HIDDeviceTimestampedReport *) packet
{
    uint64_t localTimestamp = [self syncRemoteTimestamp:packet->timestamp forEndpoint:endpoint];
    if (localTimestamp == 0) {
        os_log_info(RemoteHIDLog (), "Error syncing time with BT, dropping report! W2 TS:%llu", packet->timestamp);
        return NO;
    }

    NSData * report = [NSData dataWithBytes:&(packet->data[0]) length:(packet->header.length - sizeof(HIDDeviceTimestampedReport))];
    NSError * error = nil;
    BOOL status = [device handleReport:report withTimestamp:localTimestamp error:&error];
    if (status == NO) {
        os_log_error (RemoteHIDLog (), "handleReport:%@", error);
    }
    return status;
}

-(BOOL) remoteDeviceReportHandler:(__nonnull id) endpoint header:(HIDDeviceHeader *) header
{
    BOOL status;

    if (header->length <= sizeof(HIDDeviceReport)) {
        os_log_error (RemoteHIDLog (), "Invalid report size:%d", header->length);
        return NO;
    }

    NSMutableDictionary * devices = self.devices[endpoint];
    HIDRemoteDevice * device = devices[@(header->deviceID)];
    if (!device) {
        os_log_error (RemoteHIDLog (), "HID Device for deviceID:%d does not exist", header->deviceID);
        return NO;
    }

    if (header->hasTS) {
        status = [self remoteDeviceTimestampedReportHandler:endpoint device:device packet:(HIDDeviceTimestampedReport *)header];
    }
    else {
        status = [self remoteDeviceReportHandler:device packet:(HIDDeviceReport *)header];
    }

    device.handleReportCount++;
    if (!status) {
        device.handleReportError++;
    }

    return status;
}

-(BOOL) remoteDeviceGetReportHandler:(__nonnull id) endpoint packet:(HIDDeviceReport *) packet
{
    if (packet->header.length <= sizeof(HIDDeviceReport)) {
        os_log_error (RemoteHIDLog (), "Invalid report size:%d", packet->header.length);
        return NO;
    }
    
    NSMutableDictionary * devices = self.devices[endpoint];
    HIDRemoteDevice * device = devices[@(packet->header.deviceID)];
    if (!device) {
        os_log_error (RemoteHIDLog (), "HID Device for deviceID:%d does not exist", packet->header.deviceID);
        return NO;
    }
    
    return [device getReportHandler:packet->reportType reportID:packet->data[0] report:packet->data reportLength:(packet->header.length - sizeof(HIDDeviceReport))] == kIOReturnSuccess;
}

-(void) remoteDeviceConnectHandler:(__nonnull id) endpoint packet:(HIDDeviceControl *) packet
{
    static const uint8_t plistStart[] = "<?xml";
    static const uint8_t binaryStart[] = "\323\0\0";
    NSData * deviceDescriptionData = [NSData dataWithBytes:&packet->data length:packet->header.length];
    NSString * errorString = nil;
    NSMutableDictionary * deviceDescription = nil;
    if (deviceDescriptionData.length >= sizeof(plistStart) &&
        0 == memcmp((void *)plistStart, deviceDescriptionData.bytes, sizeof(plistStart)-1)) {
        NSError * err = nil;
        deviceDescription = [NSPropertyListSerialization propertyListWithData:deviceDescriptionData options:NSPropertyListMutableContainersAndLeaves format:NULL error:&err];
        if (err) {
            errorString = err.description;
        }
    } else if (deviceDescriptionData.length >= sizeof(binaryStart) &&
               0 == memcmp((void *)binaryStart, deviceDescriptionData.bytes, sizeof(binaryStart)-1)) {
        CFStringRef cfStrErr = NULL;
        CFTypeRef unserialized = IOCFUnserializeBinary(deviceDescriptionData.bytes, deviceDescriptionData.length, kCFAllocatorDefault, 0, &cfStrErr);
        if (unserialized) {
            if (CFGetTypeID(unserialized) == CFDictionaryGetTypeID()) {
                deviceDescription = (__bridge_transfer NSMutableDictionary *)unserialized;
            }
            else {
                errorString = @"Unserialized data is not a dictionary";
                CFRelease(unserialized);
            }
        }
        if (cfStrErr) {
            errorString = (__bridge_transfer NSString *)cfStrErr;
        }
    } else {
        errorString = @"Unrecognized data format";
    }
    if (errorString || !deviceDescription) {
        os_log_error (RemoteHIDLog (), "HIDPacketDevice de-serialization error:%@", errorString);
        os_log_debug (RemoteHIDLog (), "HIDPacketDevice config data:%@",  [[NSString alloc] initWithData:deviceDescriptionData encoding:NSASCIIStringEncoding]);
        return;
    }
    
    [self createRemoteDevice: endpoint  deviceID:packet->header.deviceID property:deviceDescription];
    
}


-(void) endpointMessageHandler:(__nonnull id) endpoint data:(uint8_t*) data size:(size_t) dataSize
{

    if (!_devices[endpoint]) {
        os_log_info (RemoteHIDLog (), "Unknown accesory:%@", endpoint);
        return;
    }

    require_quiet(dataSize >= (sizeof(HIDTransportHeader) + sizeof(HIDDeviceHeader)), error);
    
    HIDDeviceHeader * packet =  (HIDDeviceHeader *) (data + sizeof(HIDTransportHeader));

    do {
        require_quiet((((uint8_t *)packet + sizeof(HIDDeviceHeader)) <= (data + dataSize)) &&
                      (packet->length >= sizeof (HIDDeviceHeader)) &&
                      (((uint8_t *)packet + packet->length) <= (data + dataSize)) , error);
        
        [self endpointPacketHandler: endpoint packet:packet];
        packet = (HIDDeviceHeader *)((uint8_t *)packet + packet->length);
    } while ((uint8_t *)packet < (data + dataSize));

    return;

error:

    os_log_error (RemoteHIDLog (), "Invalid message length:%zu data:%@", dataSize ,  [NSData dataWithBytes:data length:dataSize]);
}

-(void) endpointPacketHandler:(__nonnull id) endpoint packet:(HIDDeviceHeader *) packet
{
    
    os_log_debug (RemoteHIDLog (), "endpointPacketHandler deviceid:%u type:%d size:%u", packet->deviceID, packet->packetType, packet->length);

    switch (packet->packetType) {
        case HIDPacketTypeDeviceConnect:
            [self remoteDeviceConnectHandler:endpoint packet:(HIDDeviceControl*) packet];
            break;
        case HIDPacketTypeDeviceDisconnect:
            [self removeRemoteDevice:endpoint deviceID:packet->deviceID];
            break;
        case HIDPacketTypeHandleReport:
            [self remoteDeviceReportHandler:endpoint header:(HIDDeviceHeader *)packet];
            break;
        case HIDPacketTypeGetReport:
            [self remoteDeviceGetReportHandler:endpoint packet:(HIDDeviceReport *)packet];
            break;
        default:
            os_log_error (RemoteHIDLog (), "Unsupported packet type:%u", packet->packetType);
            break;
    }
}

-(void) activate
{
    _stateHandler = os_state_add_handler(self.queue,
                                         ^os_state_data_t(os_state_hints_t hints) {
                                             return [self stateHandler:hints];
                                         });
}

-(void) cancel
{
    if (_stateHandler) {
        os_state_remove_handler(_stateHandler);
    }
}

-(void) disconnectAll
{
    [self.devices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key __unused, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
        NSDictionary * endpointDevices = (NSDictionary *)obj;
        [endpointDevices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key2 __unused, id  _Nonnull obj2, BOOL * _Nonnull stop2 __unused) {
            HIDUserDevice * device = (HIDUserDevice *)obj2;

            if (_prevDeviceLog.count >= REMOTE_DEVICE_LOG_SIZE) {
                [_prevDeviceLog removeObjectAtIndex:0];
            }
            [_prevDeviceLog addObject:device.description];

            [device cancel];
        }];
    }];
    [self.devices removeAllObjects];

    os_log_debug (RemoteHIDLog (), "DisconnectAll");
}

-(os_state_data_t) stateHandler:(os_state_hints_t)hints
{
    os_state_data_t stateData       = NULL;
    NSDictionary *  stateDict       = nil;
    NSData *        serializedState = nil;
    NSError *       err             = nil;

    require(hints->osh_api == OS_STATE_API_FAULT || hints->osh_api == OS_STATE_API_REQUEST, exit);

    stateDict = [self copyState];
    require(stateDict, exit);

    serializedState = [NSPropertyListSerialization dataWithPropertyList:stateDict format:NSPropertyListBinaryFormat_v1_0 options:0 error:&err];
    require(serializedState, exit);

    stateData = calloc(1, OS_STATE_DATA_SIZE_NEEDED(serializedState.length));
    require(stateData, exit);

    strlcpy(stateData->osd_title, "RemoteHID State", sizeof(stateData->osd_title));
    stateData->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
    stateData->osd_data_size = (uint32_t)serializedState.length;
    memcpy(stateData->osd_data, [serializedState bytes], serializedState.length);

exit:

    if (err) {
        os_log_error(RemoteHIDLog (), "Plist Serialization error %@", err);
    }

    return stateData;
}

- (NSDictionary *) copyState
{
    NSMutableDictionary * stateDict = [NSMutableDictionary new];

    NSMutableArray<NSString *>* devices = [NSMutableArray new];

    [self.devices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key __unused, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
        NSDictionary * endpointDevices = (NSDictionary *)obj;
        [endpointDevices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key2 __unused, id  _Nonnull obj2, BOOL * _Nonnull stop2 __unused) {
            HIDRemoteDevice * device = (HIDRemoteDevice *)obj2;

            [devices addObject:device.description];
        }];
    }];

    stateDict[@"RemoteDevices"] = devices;

    stateDict[@"PreviousRemoteDevices"] = _prevDeviceLog;

    return stateDict;
}

@end
