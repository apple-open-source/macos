//
//  HIDAccesoryServer.m
//  HIDAccesoryServer
//
//  Created by yg on 12/19/17.
//  Copyright © 2017 apple. All rights reserved.
//

#import "HIDRemoteDeviceAACPServer.h"

#import <IOBluetooth/IOBluetooth.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothDevicePriv.h>
#import <IOBluetooth/objc/IOBluetoothAudioManager.h>
#import <IOBluetooth/IOBluetoothDaemonNSXPCClient.h>
#import "HIDRemoteSimpleProtocol.h"
#import "RemoteHIDPrivate.h"
#import  <IOKit/hid/IOHIDKeys.h>



static uint16_t generation;

@interface IOBluetoothDevice (RemoteHID)

-(NSString *) description;

@end

@implementation IOBluetoothDevice (RemoteHID)

-(NSString *) description
{
#if !RC_HIDE_B288
    return [NSString stringWithFormat:@"<IOBluetoothDevicename:%@ address:%@ h1:%d>", self.name, self.addressString, self.isH1];
#else
    return [NSString stringWithFormat:@"<IOBluetoothDevicename:%@ address:%@>", self.name, self.addressString];
#endif
}


@end

@interface HIDRemoteDeviceAACPServer () <IOBluetoothDaemonNSXPCDelegate>
{
    dispatch_queue_t        _queue;
}

@property IOBluetoothDaemonNSXPCClient * client;

@end

@implementation HIDRemoteDeviceAACPServer

-(nullable instancetype) initWithQueue:(dispatch_queue_t) queue
{
    self = [super initWithQueue:queue ];
    if (!self) {
        return self;
    }
    
    _queue = dispatch_queue_create("com.apple.hidrc.bluetooth", DISPATCH_QUEUE_SERIAL);
    
    self.client = [[IOBluetoothDaemonNSXPCClient alloc] initWithDelegate:self];
    if (!self.client) {
        return nil;
    }

    return self;
}

-(void) activate
{
    dispatch_async(_queue, ^{
        [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(audioDeviceCreatedHandler:) name:IOBluetoothAACPDeviceConnected object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(disconnectBTDevicesHandler:) name:kIOBluetoothDeviceNotificationNameDisconnected object:nil];
 
        [self connectBTDevices];
        
        [super activate];
    });
}

-(void) cancel
{
    dispatch_async(_queue, ^{
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:IOBluetoothAudioDeviceCreated object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:self name:kIOBluetoothDeviceNotificationNameDisconnected object:nil];
        
        [self.devices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj __unused, BOOL * _Nonnull stop __unused) {
            [self removeBTDevice:key];
        }];
        
        [super cancel];
    });
}

-(void) audioDeviceCreatedHandler:(NSNotification *) notification
{
    IOBluetoothDevice * device = [IOBluetoothDevice deviceWithAddressString:(NSString *)[notification object]];
    dispatch_async(self.queue, ^{
        [self connectBTDevice:device];
    });
}

-(void) connectBTDevices
{
    for (IOBluetoothDevice * device in [IOBluetoothDevice connectedDevices]) {
        [self connectBTDevice:device];
    }
}

-(void) connectBTDevice:(IOBluetoothDevice *)device
{
    os_log_info (RemoteHIDLog (), "Connect device:%@", device);
#if !RC_HIDE_B288
    if (device.isH1) {
        if (!self.devices[device]) {
            [self addBTDevice:device];
        }
    }
#endif
}

-(void) disconnectBTDevicesHandler:(NSNotification *) notification
{
    os_log_info (RemoteHIDLog (), "disconnectBTDevicesHandler:%@", notification);

    IOBluetoothDevice * device = (IOBluetoothDevice *)[notification object];
    dispatch_async(self.queue, ^{
        [self removeBTDevice:device];
    });
}

-(void) addBTDevice:(IOBluetoothDevice *) device
{
    IOReturn status;
    
    os_log (RemoteHIDLog (), "HID AACP device connect:%@", device);
    
    [self connectEndpoint:device];

    size_t  packetLength = sizeof(HIDTransportHeader) + sizeof(HIDDeviceControl);
    uint8_t packet[packetLength];
 
    HIDTransportHeader * header = (HIDTransportHeader *)&packet[0];
    header->generation = ++generation;

    HIDDeviceControl * devicePacket = (HIDDeviceControl *)&packet[sizeof(HIDTransportHeader)];
    devicePacket->header.packetType = HIDPacketTypeDeviceConnect;
    devicePacket->header.length = sizeof(HIDDeviceControl);

    // sort crash protection send message to other side to notify we are ready to handle events
    status = [self sendMessageBTDevice:device data:(const uint8_t *) &packet[0] size:packetLength];
    require_action_quiet(status == kIOReturnSuccess, exit, os_log_error (RemoteHIDLog (), "addDevice device:%p result:%d", device, status));

exit:
    
    return;
}


-(IOReturn) sendMessageBTDevice:(IOBluetoothDevice *) device data:(const uint8_t *) data size:(size_t) size
{
    IOReturn status = kIOReturnSuccess;
    
    NSData * message = [NSData dataWithBytes:data length:size];
    
    os_log_debug (RemoteHIDLogPackets (), "[%p] send packet:%{RemoteHID:packet}.*P", device, (int)size , data);
    
    [self.client sendAACPCustomData:HID_AACP_MESSAGE_TYPE withData:message forDevice:device.addressString];
    
    return status;
}


-(void) removeBTDevice:(IOBluetoothDevice *) device
{
    os_log (RemoteHIDLog (), "HID AACP device disconnect:%@", device);
    
    [self disconnectEndpoint:device];
}

- (void)aacpCustomDataEvent:(BTAccessoryCustomMessageType)type withData:(NSData *)data forDevice:(NSString *)deviceAddress;
{
    uint64_t timestamp = mach_absolute_time ();
    
    os_log_debug (RemoteHIDLog (), "aacpCustomDataEvent address:%@ type:0x%x size:%lu", deviceAddress, type, (unsigned long)data.length);
    
    if (type != HID_AACP_MESSAGE_TYPE) {
        return;
    }
    
    dispatch_sync(_queue, ^{
        __block IOBluetoothDevice * endpoint = nil;

        [self.devices enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj __unused, BOOL * _Nonnull stop ) {
            IOBluetoothDevice * device = (IOBluetoothDevice *) key;
            if ([deviceAddress isEqualToString:device.addressString]) {
                endpoint = device;
                *stop = YES;
            }
        }];
        if (endpoint) {
            os_log_debug (RemoteHIDLogPackets (), "[%p] receive packet client:0x%x timestamp:%lld packet:%{RemoteHID:packet}.*P", endpoint, type, timestamp, (int)data.length, data.bytes);
            
            [self endpointMessageHandler:(id) endpoint data:(uint8_t*) data.bytes size:(size_t)data.length];
        }
    });
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
    
    IOBluetoothDevice * btDevice =  (IOBluetoothDevice *) device.endpoint;
    status = [self sendMessageBTDevice:btDevice data:(const uint8_t *) data.bytes size:data.length];
    require_action_quiet(status == kIOReturnSuccess, exit, status = kIOReturnError; os_log_error (RemoteHIDLog (), "SetReport device:%@ status:0x%x", device, status));

exit:
    return status;
}

-(BOOL) createRemoteDevice:(id) endpoint  deviceID:(uint64_t) deviceID property:(NSMutableDictionary *) property
{
    IOBluetoothDevice * btDevice = (IOBluetoothDevice *) endpoint;

    property[@kIOHIDTransportKey] = @kIOHIDTransportBTAACPValue;
    property[@kIOHIDRequestTimeoutKey] = @(kRemoteHIDDeviceTimeout * USEC_PER_SEC);
    property[@"BT_ADDR"] = btDevice.addressString;

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

    
    IOBluetoothDevice * btDevice =  (IOBluetoothDevice *) device.endpoint;
    status = [self sendMessageBTDevice:btDevice data:(const uint8_t *) data.bytes  size:data.length];
    require_action_quiet(status == kIOReturnSuccess, exit, status = kIOReturnError; os_log_error (RemoteHIDLog (), "GetReport device:%@ status:0x%x", btDevice, status));
    
exit:

    return status;
}

-(uint64_t) syncRemoteTimestamp:(__unused uint64_t)inTimestamp forEndpoint:(__unused __nonnull id)endpoint
{
    // BT timestamp API not available
    return mach_absolute_time();
}

@end

