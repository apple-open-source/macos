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
#import <IOKit/hid/IOHIDKeys.h>
#import <mach/mach_time.h>
#import <TimeSync/TimeSync.h>

static void HIDAccesorySessionEventCallback (BTSession session, BTSessionEvent event, BTResult result, void* userData);
static void HIDAccesoryCustomMessageCallback (BTAccessoryManager manager, BTDevice device, BTAccessoryCustomMessageType type, BTData data, size_t dataSize, void* userData);
static void HIDAccesoryServiceEventCallback (BTDevice device, BTServiceMask services, BTServiceEventType eventType, BTServiceSpecificEvent event, BTResult result, void* userData);
static void HIDAccessoryEventCallback(BTAccessoryManager manager, BTAccessoryEvent event, BTDevice device, BTAccessoryState state, void* userData);

static uint16_t generation;

@interface HIDRemoteDeviceAACPServer ()
{
    BTSession               _session;
    BTAccessoryManager      _manager;
    dispatch_queue_t        _queue;
    TSClockManager          *_coreTimeSyncManager;
    TSUserFilteredClock     *_timeSyncClock;
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

- (NSString *)description
{
    uint64_t serviceID = 0;
    return [NSString stringWithFormat:@"<HIDRemoteDeviceAACPServer timeSync:%s %@>",
            (_timeSyncClock ? "YES" : "NO"), [super description]];
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

    const static BTAccessoryCallbacks accessoryCallbacks = {HIDAccessoryEventCallback};
    status = BTAccessoryManagerAddCallbacks(_manager, &accessoryCallbacks, (__bridge void *)self);
    if (status != BT_SUCCESS) {
        os_log_error (RemoteHIDLog (), "BTAccessoryManagerAddCallbacks:%d", status);
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

    // XXX Workaround until BT allows TimeSyncEnable after audio has started.
    // TimeSync Setup
    // Will fail if BT TimeSync is not enabled, so ignore errors
    status = BTAccessoryManagerRemoteTimeSyncEnable(_manager, device, BT_TRUE);
    if (status != BT_SUCCESS) {
        os_log(RemoteHIDLog(), "Couldn't enable timesync for device:%p status:%d", device, status);
    }
    else {
        os_log(RemoteHIDLog(), "Enabled timesync for device:%p", device);
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
    
    os_log_debug (RemoteHIDLogPackets (), "[%p] send packet:%{RemoteHID:packet}.*P", device, (int)size , data);

    BTResult status = BTAccessoryManagerSendCustomMessage (_manager, HID_AACP_MESSAGE_TYPE, device, data,  size);
    if (status) {
        os_log_error (RemoteHIDLog (), "BTAccessoryManagerSendCustomMessage device:%p result:%d", device, status);
    }
    return status;
}


-(void) removeBTDevice:(BTDevice) device
{
    BTResult status;

    os_log (RemoteHIDLog (), "HID AACP device remove:%p", device);
    
    NSValue * endpoint = [NSValue valueWithPointer:device];
    [self disconnectEndpoint:endpoint];
}

-(void) btDeviceMessageHandler:(BTDevice) device type:(BTAccessoryCustomMessageType) type data:(BTData) data size:(size_t)dataSize
{
    os_log_debug (RemoteHIDLog (), "btDeviceMessageHandler device:%p client:0x%x data:%p size:%d", device, type, data, (int)dataSize);
    uint64_t timestamp = mach_absolute_time ();
    NSValue * endpoint =  [NSValue valueWithPointer:device];
    if ((uint32_t)type == HID_AACP_MESSAGE_TYPE) {
        os_log_debug (RemoteHIDLogPackets (), "[%p] receive packet client:0x%x timestamp:%lld packet:%{RemoteHID:packet}.*P", device, type, timestamp, (int)dataSize, (uint8_t*) data);
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

-(void) btAccessoryEventHandler:(BTDevice)device event:(BTAccessoryEvent)event state:(BTAccessoryState)state
{
    os_log_info (RemoteHIDLog (), "btAccessoryEventHandler device:%p event:%d state:%d" , device, event, state);

    if (BT_ACCESSORY_TIMESYNC_AVAILABLE == event) {
        TSClockIdentifier tsID = 0;
        BTResult result;

        os_log(RemoteHIDLog(), "TIMESYNC_AVAILABLE device:%p", device);

        if (!_coreTimeSyncManager) {
            _coreTimeSyncManager = [TSClockManager sharedClockManager];
        }
        require_action(_coreTimeSyncManager, exit, os_log_error(RemoteHIDLog(), "Couldn't create TSClockManager!"));

        // TIMESYNC_AVAILABLE message arrives on a BTDevice, but it applies to all BTDevices with timesync enabled.
        result = BTAccessoryManagerGetTimeSyncId(_manager, device, &tsID);
        os_log(RemoteHIDLog(), "BTAccessoryManagerGetTimeSyncId device:%p tsID:0x%llx", device, (unsigned long long)tsID);
        require_action(BT_SUCCESS == result, exit, os_log_error(RemoteHIDLog(), "BTAccessoryManagerGetTimeSyncId failed result:%d", (int)result));

        _timeSyncClock = (TSUserFilteredClock *)[_coreTimeSyncManager clockWithClockIdentifier:tsID];
        require_action(_timeSyncClock, exit, os_log_error(RemoteHIDLog(), "Couldn't create TSUserFilteredClock!"));

    }
    else if (BT_ACCESSORY_TIMESYNC_NOT_AVAILABLE == event) {
        os_log(RemoteHIDLog(), "TIMESYNC_NOT_AVAILABLE device:%p", device);
        _timeSyncClock = nil;
    }

exit:
    return;
}

-(IOReturn) remoteDeviceSetReport:(HIDRemoteDevice *) device
                             type:(HIDReportType) type
                         reportID:(__unused uint8_t)reportID
                           report:(NSData *) report
{
    IOReturn status = kIOReturnSuccess;
    HIDDeviceReport * devicePacket;
    HIDTransportHeader * header;
    NSMutableData * data = [[NSMutableData alloc] initWithLength:report.length + sizeof(HIDDeviceReport) + sizeof(HIDTransportHeader)];
    
    os_log_debug (RemoteHIDLog (), "remoteDeviceSetReport deviceID:0x%llx type:%ld report:%@", device.deviceID, (long)type, report);
 
    devicePacket = (HIDDeviceReport *) ((uint8_t *)data.bytes + sizeof(HIDTransportHeader));
    header =  (HIDTransportHeader *) ((uint8_t *)data.bytes);
    
    memcpy(devicePacket->data, report.bytes, report.length);
    devicePacket->header.deviceID    = (uint32_t) device.deviceID;
    devicePacket->header.packetType  = HIDPacketTypeSetReport;
    devicePacket->reportType         = type;
    devicePacket->header.length      = (uint32_t)report.length + sizeof(HIDDeviceReport);
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
                             type:(HIDReportType) type
                         reportID:(uint8_t) reportID
                           report:(NSMutableData * __unused) report
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

//                  Always try to TS on W2
//                  W2 TS Success   | W2 TS Failure
// iOS TS Enable  | Do TS           | Drop data on iOS
// iOS TS Disable | Use non-TS time | Use non-TS time
//
// Alternative - send new TSEnabled packet to W2 when TS notification received

-(uint64_t) syncRemoteTimestamp:(uint64_t)inTimestamp forEndpoint:(__nonnull id)endpoint
{
    BTDevice btDevice = (BTDevice)((NSValue *)endpoint).pointerValue;
    mach_timebase_info_data_t timeBase;
    uint64_t localAbs = mach_absolute_time();
    uint64_t syncedAbs = 0;
    uint32_t tsFlags = 0;

    mach_timebase_info(&timeBase);

    // Check if TimeSync is enabled
    require_action_quiet(_timeSyncClock && _timeSyncClock.lockState == TSClockLocked, exit, os_log_error(RemoteHIDLog (), "Timesync: not locked, clockID: 0x%llx state: %d", (unsigned long long)_timeSyncClock.clockIdentifier, (int)_timeSyncClock.lockState));

    syncedAbs = [_timeSyncClock convertFromDomainToMachAbsoluteTime:inTimestamp withFlags:&tsFlags];
    uint64_t syncDeltaAbs = (localAbs > syncedAbs) ? (localAbs - syncedAbs) : (syncedAbs - localAbs);
    uint64_t syncDeltaNs = (uint64_t)((syncDeltaAbs * timeBase.numer) / (timeBase.denom));
    os_log_debug (RemoteHIDLog (), "W2 btclk(ns):%llu local abs:%llu Synced ts:%llu remote->local latency(ns):%s%llu",
            inTimestamp, localAbs, syncedAbs, (localAbs > syncedAbs) ? "+" : "-", syncDeltaNs);

exit:
    return syncedAbs;
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
        os_log_error (RemoteHIDLog (), "HIDAccesoryServiceEventCallback eventType:%d event:%d result:%d data:%p", eventType, event, result, userData);
        return;
    } else {
        os_log_info (RemoteHIDLog (), "HIDAccesoryServiceEventCallback eventType:%d event:%d result:%d data:%p", eventType, event, result, userData);
    }

    HIDRemoteDeviceAACPServer * self = (__bridge HIDRemoteDeviceAACPServer *) userData;

    @autoreleasepool {
        [self btServiceEventHandler:device services:services eventType:eventType event:event result:result];
    }
}

// BTAccessoryEventCallback
void HIDAccessoryEventCallback(__unused BTAccessoryManager manager, BTAccessoryEvent event, BTDevice device, BTAccessoryState state, void* userData)
{
    HIDRemoteDeviceAACPServer * self = (__bridge HIDRemoteDeviceAACPServer *) userData;
    @autoreleasepool {
        [self btAccessoryEventHandler:device event:event state:state];
    }
}
