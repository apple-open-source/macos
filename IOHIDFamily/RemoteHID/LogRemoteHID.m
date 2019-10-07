//
//  logremotehid.m
//  logremotehid
//
//  Created by yg on 5/24/18.
//

#import <Foundation/Foundation.h>
#import "HIDRemoteSimpleProtocol.h"
#include <os/log_private.h>

NSString * RemoteHIDCommandDataToString (uint8_t * data, size_t length);
NSString * RemoteHIDCommandTypeToString (uint32_t cmdType);
NSString * RemoteHIDCommandToString (HIDDeviceHeader * command);
NSString * RemoteHIDPacketToString (NSData * data);


NSString * RemoteHIDCommandDataToString (uint8_t * data, size_t length)
{
    NSMutableString * dataStr = [[NSMutableString alloc] initWithCapacity:2048];
    [dataStr appendString:@"["];
    for (int i = 0; i < length; i++) {
        [dataStr appendFormat:@"0x%02x%s", *(data + i), i!=(length-1)?",":""];
    }
    [dataStr appendString:@"]"];
    return dataStr;
}

NSString * RemoteHIDCommandTypeToString (uint32_t cmdType)
{
    NSString * cmdTypeStr = nil;
    switch (cmdType) {
        case HIDPacketTypeDeviceConnect:
            cmdTypeStr= @"HIDPacketTypeDeviceConnect";
            break;
        case HIDPacketTypeDeviceDisconnect:
            cmdTypeStr= @"HIDPacketTypeDeviceDisconnect";
            break;
        case HIDPacketTypeHandleReport:
            cmdTypeStr= @"HIDPacketTypeHandleReport";
            break;
        case HIDPacketTypeSetReport:
            cmdTypeStr= @"HIDPacketTypeSetReport";
            break;
        case HIDPacketTypeGetReport:
            cmdTypeStr= @"HIDPacketTypeGetReport";
            break;
        default:
            cmdTypeStr = [NSString stringWithFormat:@"Unknown (%d)", cmdType] ;
            break;
    }
    return cmdTypeStr;
}



NSString * RemoteHIDCommandToString (HIDDeviceHeader * command)
{
    NSMutableString * cmdStr = [[NSMutableString alloc] initWithString:@""];
    [cmdStr appendFormat:@"device:%d, cmd:\"%@\", len:%d ",command->deviceID, RemoteHIDCommandTypeToString(command->packetType), command->length];
    switch (command->packetType) {
        case HIDPacketTypeDeviceConnect:
            [cmdStr appendFormat:@"data:%@", RemoteHIDCommandDataToString(((HIDDeviceControl *)command)->data, command->length - sizeof(HIDDeviceControl))];
            break;
        case HIDPacketTypeHandleReport:
        case HIDPacketTypeSetReport:
        case HIDPacketTypeGetReport:
            if (command->packetType == HIDPacketTypeHandleReport && command->hasTS ) {
                [cmdStr appendFormat:@"type:%d, timestamp:0x%llx data:%@",
                    ((HIDDeviceTimestampedReport *)command)->reportType,
                    ((HIDDeviceTimestampedReport *)command)->timestamp,
                    RemoteHIDCommandDataToString(((HIDDeviceTimestampedReport *)command)->data, command->length - sizeof(HIDDeviceTimestampedReport))];
            } else {
                [cmdStr appendFormat:@"type:%d, data:%@",
                    ((HIDDeviceReport *)command)->reportType,
                    RemoteHIDCommandDataToString(((HIDDeviceReport *)command)->data, command->length - sizeof(HIDDeviceReport))];
            }
            break;
            
        default:
            break;
    }
    return cmdStr;
}

NSString * RemoteHIDPacketToString (NSData * data)
{
    NSMutableString * pktStr = [[NSMutableString alloc] initWithCapacity:4096];
    
    require_action_quiet(data.length >= (sizeof(HIDTransportHeader) + sizeof(HIDDeviceHeader)), exit, pktStr = nil);
    
    [pktStr appendFormat:@"{id:%u, len:%lu, ", *(uint32_t *)(data.bytes), (unsigned long)data.length];
    
    HIDDeviceHeader * packet =  (HIDDeviceHeader *) (data.bytes + sizeof(HIDTransportHeader));
    
    do {
        require_action_quiet((((uint8_t *)packet + sizeof(HIDDeviceHeader)) <= ((uint8_t *)data.bytes + data.length)) &&
                             (packet->length >= sizeof (HIDDeviceHeader)) &&
                             (((uint8_t *)packet + packet->length) <= ((uint8_t *)data.bytes + data.length)) , exit, pktStr = nil);
        
        [pktStr appendFormat:@",{%@}",RemoteHIDCommandToString(packet)];
        
        packet = (HIDDeviceHeader *)((uint8_t *)packet + packet->length);
    } while ((uint8_t *)packet < ((uint8_t *)data.bytes + data.length));
    
    [pktStr appendString:@"}"];

exit:
    
    return pktStr;
}


NSAttributedString *
OSLogCopyFormattedString(const char *type, id value, __unused os_log_type_info_t info) {
    /*
     * Decoder called when a custom format is specified
     */
    NSAttributedString *decoded = nil;
    NSData *data = (NSData *) value;
    if (!strcmp(type, "packet")) {
        decoded = [[NSMutableAttributedString alloc] initWithString:RemoteHIDPacketToString(data)];
    } else if (!strcmp(type, "command")) {
        decoded = [[NSMutableAttributedString alloc] initWithString:RemoteHIDCommandToString((HIDDeviceHeader*)data.bytes)];
    } else {
        decoded = [[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"%@", data]];
    }
    return decoded;
}
