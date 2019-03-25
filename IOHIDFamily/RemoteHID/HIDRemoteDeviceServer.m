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

@interface HIDRemoteDevice ()

@property        dispatch_semaphore_t   semaphore;
@property        NSData *               lastGetReport;
@property        BOOL                   waitForReport;

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
    return [NSString stringWithFormat:@"<HIDRemoteHIDUserDevice:%p id:%lld service:%llx device:%@>",(void *) self, self.deviceID, serviceID, [super description]];
}

@end

@interface HIDRemoteDeviceServer ()

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
    return self;
}


-(BOOL) connectEndpoint:(__nonnull id) endpoint
{
    NSMutableDictionary * endpointDevices = [[NSMutableDictionary alloc] init];
    self.devices [endpoint] = endpointDevices;
    return TRUE;
}

-(void) disconnectEndpoint:(__nonnull id) endpoint
{
    NSMutableDictionary * endpointDevices = self.devices [endpoint];
    
    [endpointDevices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key __unused, id  _Nonnull obj, BOOL * _Nonnull stop __unused) {
        HIDUserDevice * device = (HIDUserDevice *)obj;
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
    
    os_log (RemoteHIDLog (), "Create device:%@ for endpoint:%@ property:%@", device, endpoint, property);
    
    __weak HIDRemoteDevice  *weakDevice = device;

    [device setSetReportHandler:^IOReturn(IOHIDReportType type, uint32_t reportID, uint8_t * _Nullable report, NSUInteger reportLength) {
        __strong HIDRemoteDevice * strongDevice = weakDevice;
        if (!strongDevice) {
            return kIOReturnNoDevice;
        }
        IOReturn status = [self remoteDeviceSetReport:strongDevice type:type reportID:reportID report:report reportLength:reportLength];
        return status;
    }];

    [device setGetReportHandler:^IOReturn(IOHIDReportType type, uint32_t reportID, uint8_t * _Nullable report, NSUInteger * reportLength) {
        __strong HIDRemoteDevice * strongDevice = weakDevice;
        if (!strongDevice) {
            return kIOReturnNoDevice;
        }
        
        os_log_debug (RemoteHIDLog (), "[device:%d] getReport type:%d reportID:%d", (int)strongDevice.deviceID, type, reportID);
        
        NSData *    reportData;
        long        semaStatus;
        IOReturn    status;
        
        strongDevice.lastGetReport = nil;
        strongDevice.waitForReport = YES;
        
        status = [self remoteDeviceGetReport:strongDevice type:type reportID:reportID report:report reportLength:reportLength];
        require_action_quiet(status == kIOReturnSuccess, exit, os_log_error (RemoteHIDLog (), "[device:%d] remoteDeviceGetReport:0x%x", (int)strongDevice.deviceID, status));

        semaStatus = dispatch_semaphore_wait(strongDevice.semaphore, dispatch_time(DISPATCH_TIME_NOW, kRemoteHIDDeviceTimeout * NSEC_PER_SEC));
        require_action_quiet(semaStatus == 0, exit, status = kIOReturnTimeout; os_log_error (RemoteHIDLog (), "[device:%d] remoteDeviceGetReport timeout", (int)strongDevice.deviceID));
        
        reportData = strongDevice.lastGetReport;
        require_action_quiet(reportData, exit, status = kIOReturnError; os_log_error (RemoteHIDLog (), "[device:%d] invalid report :%@", (int)strongDevice.deviceID, reportData));
        
        memcpy(report, ((NSData *)reportData).bytes, ((NSData *)reportData).length);
        *reportLength = ((NSData *)reportData).length;
    
    exit:
        
        strongDevice.waitForReport = NO;
        
        return status;
    }];

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

    return YES;
}

-(IOReturn) remoteDeviceSetReport:(__unused HIDRemoteDevice *) device type:(__unused IOHIDReportType) type reportID:(__unused uint8_t) reportID report:(__unused uint8_t *) report reportLength:(__unused NSUInteger) reportLength
{
    return kIOReturnUnsupported;
}

-(IOReturn) remoteDeviceGetReport:(__unused HIDRemoteDevice *) device type:(__unused IOHIDReportType) type reportID:(__unused uint8_t) reportID report:(__unused uint8_t *) report reportLength:(__unused NSUInteger *) reportLength
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

-(BOOL) remoteDeviceReportHandler:(__nonnull id) endpoint packet:(HIDDeviceReport *) packet
{
    if (packet->header.length <= sizeof(HIDDeviceReport)) {
        os_log_error (RemoteHIDLog (), "Invalid report size:%d", packet->header.length);
        return NO;
    }
    
    NSMutableDictionary * devices = self.devices[endpoint];
    HIDUserDevice * device = devices[@(packet->header.deviceID)];
    if (!device) {
        os_log_error (RemoteHIDLog (), "HID Device for deviceID:%d does not exist", packet->header.deviceID);
        return NO;
    }

    IOReturn status = [device handleReport:&(packet->data[0]) reportLength:(packet->header.length - sizeof(HIDDeviceReport))];
    if (status) {
        os_log_error (RemoteHIDLog (), "handleReport:%x", status);
        return NO;
    }
    return YES;
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
    NSData * deviceDescriptionData = [NSData dataWithBytes:&packet->data length:packet->header.length];
    NSError * err = nil;
    id deviceDescription = [NSPropertyListSerialization propertyListWithData:deviceDescriptionData options:NSPropertyListMutableContainersAndLeaves format:NULL error:&err];
    if (err) {
        os_log_error (RemoteHIDLog (), "HIDPacketDevice de-serialization error:%@", err);
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
            [self remoteDeviceReportHandler:endpoint packet:(HIDDeviceReport *)packet];
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
}

-(void) cancel
{
    
}

-(void) disconnectAll
{
    [self.devices removeAllObjects];
}
@end
