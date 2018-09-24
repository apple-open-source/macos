//
//  HIDAccesoryServer.m
//  HIDAccesoryServer
//
//  Created by yg on 12/19/17.
//  Copyright © 2017 apple. All rights reserved.
//

#import "HIDRemoteDeviceAACPServer.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#import <MobileBluetooth/BTDevice.h>
#import <MobileBluetooth/BTLocalDevice.h>
#import <MobileBluetooth/BTAccessory.h>
#import <MobileBluetooth/BTFeatures.h>
#pragma clang diagnostic pop
#import "HIDRemoteSimpleProtocol.h"
#import "RemoteHIDPrivate.h"
#import  <IOKit/hid/IOHIDKeys.h>

static void HIDAccesorySessionEventCallback (BTSession session, BTSessionEvent event, BTResult result, void* userData);
static void HIDAccesoryCustomMessageCallback (BTAccessoryManager manager, BTDevice device, BTAccessoryCustomMessageType type, BTData data, size_t dataSize, void* userData);
static void HIDAccesoryServiceEventCallback (BTDevice device, BTServiceMask services, BTServiceEventType eventType, BTServiceSpecificEvent event, BTResult result, void* userData);

static uint16_t generation;



@interface HIDRemoteDeviceAACPServer ()
{
    BTSession               _session;
    BTAccessoryManager      _manager;
    dispatch_queue_t        _queue;
}

@end

@implementation HIDRemoteDeviceAACPServer

-(nullable instancetype) initWithQueue:(dispatch_queue_t) queue
{
    self = [super initWithQueue:queue ];
    if (!self) {
        return self;
    }
    
    _queue = dispatch_queue_create("com.apple.hidrc.bluetooth", DISPATCH_QUEUE_SERIAL);
    
    return self;
}

-(void) activate
{
    [self btSessionCreate];
    
    [super activate];
}

-(void) cancel
{
    if (_session) {
        BTSessionDetachWithQueue (&_session);
    }
    
    [self.devices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj __unused, BOOL * _Nonnull stop __unused) {
        [self removeBTDevice:(BTDevice)(((NSNumber *)key).unsignedLongLongValue)];
    }];
    
    [super cancel];
}

-(void) btSessionCreate
{
    
    [self disconnectAll];
    
    static BTSessionCallbacks sessionCallbacks = {HIDAccesorySessionEventCallback};
    BTResult result = BTSessionAttachWithQueue("HIDRemoteAACPServer", &sessionCallbacks, (__bridge void *)self, _queue);
    if (result != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "BTSessionAttachWithQueue:%d", result);
    }
}

-(void) btSessionInit:(BTSession) session
{
    BTResult status;
    
     _session = session;
    
    status = BTAccessoryManagerGetDefault(_session, &_manager);
    if (status != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "BTAccessoryManagerGetDefault:%d", status);
        return;
    }
    
    status = BTServiceAddCallbacks(_session, HIDAccesoryServiceEventCallback,  (__bridge void *)self);
    if (status != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "BTServiceAddCallbacks:%d", status);
        return;
    }
    
    BTLocalDevice localDevice;
    status = BTLocalDeviceGetDefault(_session, &localDevice);
    if (status != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "BTLocalDeviceGetDefault:%d", status);
        return;
    }
    
    size_t  count = 256;
    BTDevice btDevices [count];
    status = BTLocalDeviceGetConnectedDevices(localDevice, btDevices, &count, count);
    if (status != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "BTLocalDeviceGetConnectedDevices:%d", status);
        return;
    }
    
    for (unsigned int index = 0 ; index < count ; index ++) {
        BTServiceMask service = 0;
        BTDeviceGetConnectedServices (btDevices[index], &service);
        [self btServiceEventHandler:btDevices[index] services:service eventType:BT_SERVICE_CONNECT event:BT_SERVICE_CONNECTION_RESULT result:BT_SUCCESS];
    }
    
}


-(void) btSessionEventHandler: (BTSession) session event:(BTSessionEvent) event result:(BTResult) result
{
    os_log (RemoteHIDLog (), "btSessionEventHandler session:%p event:%d result:%d", session, event, result);
    
    if (event == BT_SESSION_TERMINATED) {
        _session = NULL;
        //@todo this is wrong , we really need times source with ability to cancel
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), _queue, ^{
            [self btSessionCreate];
        });

    } else if (event == BT_SESSION_ATTACHED) {
        
        [self btSessionInit:session];
    
    } else if (event == BT_SESSION_ATTACHED && result != BT_SUCCESS) {
       
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), _queue, ^{
            [self btSessionCreate];
        });
    }
}

-(void) addBTDevice:(BTDevice) device
{
    BTResult  status;
    BTBool    capable = false;
    NSValue * endpoint;

    static BTAccessoryCustomMessageCallbacks messageCallback = {HIDAccesoryCustomMessageCallback};

    status = BTAccessoryManagerGetFeatureCapability(_manager, device, FEATURE_SENSOR_DATA , &capable);
    os_log_info (RemoteHIDLog (), "BTAccessoryManagerGetFeatureCapability:%d (FEATURE_SENSOR_DATA:%d)", status, capable);
    
//    require_action_quiet(status == BT_SUCCESS, exit, os_log_error (RemoteHIDLog (), "BTAccessoryManagerGetFeatureCapability device:%p result:%d", device, status));
//    require_action_quiet(capable, exit, os_log_info (RemoteHIDLog (), "HID over AACP not supported for device:%p", device));
    
    status = BTAccessoryManagerRegisterCustomMessageClient(_manager, &messageCallback, HID_AACP_MESSAGE_TYPE,  (__bridge void *)self);
    if (status != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "BTAccessoryManagerRegisterCustomMessageClient:%d", status);
        return;
    }
 
    endpoint =  [NSValue valueWithPointer:device];
    
    if (self.devices[endpoint]) {
        os_log (RemoteHIDLog (), "HID AACP device:%p already connected", device);
    } else {
        os_log (RemoteHIDLog (), "HID AACP device:%p", device);
    }
    
    [self connectEndpoint:endpoint];

    size_t  packetLength = sizeof(HIDTransportHeader) + sizeof(HIDDeviceControl);
    uint8_t packet[packetLength];
 
    HIDTransportHeader * header = (HIDTransportHeader *)&packet[0];
    header->generation = ++generation;

    HIDDeviceControl * devicePacket = (HIDDeviceControl *)&packet[sizeof(HIDTransportHeader)];
    devicePacket->header.packetType = HIDPacketTypeDeviceConnect;
    devicePacket->header.length = sizeof(HIDDeviceControl);

    // sort crash protection send message to other side to notify we are ready to handle events
    status = [self sendMessageBTDevice:device data:(BTData) &packet[0] size:packetLength];
    require_action_quiet(status == BT_SUCCESS, exit, os_log_error (RemoteHIDLog (), "addDevice device:%p result:%d", device, status));

exit:
    
    return;
}


-(BTResult) sendMessageBTDevice:(BTDevice) device data:(BTData) data size:(size_t) size
{
    os_log_debug (RemoteHIDLogPackets (), "[%p] Send packet len:%zu data:%@", device, size , [NSData dataWithBytes:data length:size]);
    BTResult status = BTAccessoryManagerSendCustomMessage (_manager, HID_AACP_MESSAGE_TYPE, device, data,  size);
    if (status) {
        os_log_error (RemoteHIDLog (), "BTAccessoryManagerSendCustomMessage device:%p result:%d", device, status);
    }
    return status;
}


-(void) removeBTDevice:(BTDevice) device
{
    os_log (RemoteHIDLog (), "HID AACP device remove:%p", device);
    
    NSValue * endpoint = [NSValue valueWithPointer:device];
    [self disconnectEndpoint:endpoint];
}

-(void) btDeviceMessageHandler:(BTDevice) device type:(BTAccessoryCustomMessageType) type data:(BTData) data size:(size_t)dataSize
{
    os_log_debug (RemoteHIDLog (), "btDeviceMessageHandler device:%p client:0x%x data:%p size:%d", device, type, data, (int)dataSize);
    NSValue * endpoint =  [NSValue valueWithPointer:device];
    if ((uint32_t)type == HID_AACP_MESSAGE_TYPE) {
        os_log_debug (RemoteHIDLogPackets (), "[%p] Receive packet len:%zu data:%@", device, dataSize ,[NSData dataWithBytes:data length:dataSize]);
        [self endpointMessageHandler:(id) endpoint data:(uint8_t*) data size:(size_t)dataSize];
    }
}

-(void) btServiceEventHandler:(BTDevice) device
                     services:(BTServiceMask) services
                    eventType:(BTServiceEventType) eventType
                        event:(BTServiceSpecificEvent) event
                       result:(BTResult) result
{
    os_log_debug (RemoteHIDLog (), "btServiceEventHandler services:0x%x eventType:%d event:%d result:%d" , services, eventType, event, result);

    if ((services & BT_SERVICE_AACP) == 0) {
        return;
    } else if ((BT_SERVICE_CONNECT == eventType) && (BT_SERVICE_CONNECTION_RESULT == event) && (BT_SUCCESS == result)) {
        [self addBTDevice:device];
    } else if ((BT_SERVICE_DISCONNECT == eventType) && (BT_SERVICE_DISCONNECTION_RESULT == event)) {
        [self removeBTDevice:device];
    }
}

-(IOReturn) remoteDeviceSetReport:(HIDRemoteDevice *) device
                             type:(IOHIDReportType) type
                         reportID:(__unused uint8_t)reportID
                           report:(uint8_t *) report
                     reportLength:(NSUInteger) reportLength
{
    IOReturn status = kIOReturnSuccess;
    HIDDeviceReport * devicePacket;
    HIDTransportHeader * header;
    NSMutableData * data = [[NSMutableData alloc] initWithLength:reportLength + sizeof(HIDDeviceReport) + sizeof(HIDTransportHeader)];
    
    os_log_debug (RemoteHIDLog (), "remoteDeviceSetReport deviceID:0x%llx type:%d reportLength:%d", device.deviceID, type, (int)reportLength);
 
    devicePacket = (HIDDeviceReport *) ((uint8_t *)data.bytes + sizeof(HIDTransportHeader));
    header =  (HIDTransportHeader *) ((uint8_t *)data.bytes);
    
    memcpy(devicePacket->data, report, reportLength);
    devicePacket->header.deviceID    = (uint32_t) device.deviceID;
    devicePacket->header.packetType  = HIDPacketTypeSetReport;
    devicePacket->reportType         = type;
    devicePacket->header.length      = (uint32_t)reportLength + sizeof(HIDDeviceReport);
    header->generation               = ++generation;
    
    BTDevice btDevice =  (BTDevice) ((NSValue *)device.endpoint).pointerValue;
    BTResult btStatus = [self sendMessageBTDevice:btDevice data:(BTData) data.bytes size:data.length];
    require_action_quiet(btStatus == BT_SUCCESS, exit, status = kIOReturnError; os_log_error (RemoteHIDLog (), "SetReport device:%p status:%d", btDevice, btStatus));

exit:
    return status;
}

-(BOOL) createRemoteDevice:(id) endpoint  deviceID:(uint64_t) deviceID property:(NSMutableDictionary *) property
{
    property[@kIOHIDTransportKey] = @kIOHIDTransportBTAACPValue;
    property[@kIOHIDRequestTimeoutKey] = @(kRemoteHIDDeviceTimeout * USEC_PER_SEC);

    char addrStr[255];
    BTResult status = BTDeviceGetAddressString((BTDevice) ((NSValue *)endpoint).pointerValue, addrStr, sizeof(addrStr));
    if (status == BT_SUCCESS) {
        BTDeviceAddress addr;
        status = BTDeviceAddressFromString(addrStr, &addr);
        if (status == BT_SUCCESS) {
            property[@"BT_ADDR"] = [NSData dataWithBytes:&addr length:sizeof(addr)];
        }
    }

    return [super createRemoteDevice:endpoint deviceID:deviceID property:property];
}

-(IOReturn) remoteDeviceGetReport:(HIDRemoteDevice *) device
                             type:(IOHIDReportType) type
                         reportID:(uint8_t) reportID
                           report:(__unused uint8_t *) report
                     reportLength:(__unused NSUInteger *) reportLength
{
    NSMutableData * data = [[NSMutableData alloc] initWithLength:sizeof(HIDDeviceReport) + sizeof(HIDTransportHeader) + 1];
    IOReturn status = kIOReturnSuccess;
    HIDDeviceReport * devicePacket;
    HIDTransportHeader * header;
    
    devicePacket = (HIDDeviceReport *) ((uint8_t *)data.bytes + sizeof(HIDTransportHeader));
    header =  (HIDTransportHeader *) ((uint8_t *)data.bytes);
    
    devicePacket->header.deviceID    = (uint32_t) device.deviceID;
    devicePacket->header.packetType  = HIDPacketTypeGetReport;
    devicePacket->reportType         = type;
    devicePacket->header.length      = 1 + sizeof(HIDDeviceReport);
    devicePacket->data[0]            = reportID;
    header->generation               = ++generation;

    
    BTDevice btDevice =  (BTDevice) ((NSValue *)device.endpoint).pointerValue;
    BTResult btStatus = [self sendMessageBTDevice:btDevice data:(BTData) data.bytes size:data.length];
    require_action_quiet(btStatus == BT_SUCCESS, exit, status = kIOReturnError; os_log_error (RemoteHIDLog (), "GetReport device:%p status:%d", btDevice, btStatus));
    
exit:

    return status;
}

@end

void HIDAccesorySessionEventCallback (BTSession session, BTSessionEvent event, BTResult result, void* userData)
{
    if (result != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "HIDAccesorySessionEventCallback event:%d result:%d", event, result);
    }
    HIDRemoteDeviceAACPServer * self = (__bridge HIDRemoteDeviceAACPServer *) userData;
    [self btSessionEventHandler:session event:event result:result];
}


void HIDAccesoryCustomMessageCallback (BTAccessoryManager manager __unused, BTDevice device, BTAccessoryCustomMessageType type, BTData data, size_t dataSize, void* userData)
{

//    os_log_info (RemoteHIDLog (), "HIDAccesoryCustomMessageCallback device:%p type:%x data:%p size:%d ctx:%p", device, type, data, (int)dataSize, userData);
//    if (userData != (__bridge void *)instance) {
//        os_log_error (RemoteHIDLog (), "HIDAccesoryCustomMessageCallback duserData is corrupted");
//        userData = (__bridge void *)instance;
//    }
    
    HIDRemoteDeviceAACPServer * self = (__bridge HIDRemoteDeviceAACPServer *) userData;
    @autoreleasepool {
        [self btDeviceMessageHandler:device type:type data:data size:dataSize];
    }
}

void HIDAccesoryServiceEventCallback (BTDevice device, BTServiceMask services, BTServiceEventType eventType, BTServiceSpecificEvent event, BTResult result, void* userData)
{
    
    
    if (result != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "HIDAccesorySessionEventCallback event:%d result:%d data:%p", event, result, userData);
        return;
    } else {
        os_log_info (RemoteHIDLog (), "HIDAccesorySessionEventCallback event:%d result:%d data:%p", event, result, userData);
    }

    HIDRemoteDeviceAACPServer * self = (__bridge HIDRemoteDeviceAACPServer *) userData;

    @autoreleasepool {
        [self btServiceEventHandler:device services:services eventType:eventType event:event result:result];
    }
}
