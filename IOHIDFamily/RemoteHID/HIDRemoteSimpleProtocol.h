//
//  HIDRemoteSimpleProtocol_h
//  RemoteHID
//
//  Created by yg on 1/9/18.
//  Copyright © 2018 apple. All rights reserved.
//

#ifndef HIDRemoteSimpleProtocol_h
#define HIDRemoteSimpleProtocol_h

#include <TargetConditionals.h>
#include <AssertMacros.h>

#define HID_AACP_MESSAGE_TYPE   0x800  //AACP_CUSTOM_MESSAGE_TYPE_SENSOR
// New message type needed for generalized AFK packets?

typedef enum {
    HIDPacketTypeDeviceConnect      = 0,
    HIDPacketTypeDeviceDisconnect   = 1,
    HIDPacketTypeHandleReport       = 2,
    HIDPacketTypeSetReport          = 3,
    HIDPacketTypeGetReport          = 4,
    HIDPacketTypeAFKCommand         = 5,
    HIDPacketTypeAFKReport          = 6,
} HIDPacketType;

#if TARGET_OS_RTKIT
typedef enum {
    HIDReportTypeInput              = 0,
    HIDReportTypeOutput             = 1,
    HIDReportTypeFeature            = 2
} HIDReportType;
#endif

typedef struct __attribute__((__packed__)) {
    uint8_t         version;
} HIDTransportVersion;

enum {
    HIDTransportVersion0    = 0,
    AFKBTTransportVersion1  = 1,
};

typedef struct __attribute__((__packed__)) {
    HIDTransportVersion version;
    uint16_t            generation;
    uint8_t             reserved;
} HIDTransportHeader;

typedef HIDTransportHeader HIDTransportHeaderV0;
typedef HIDTransportHeader AFKBTTransportHeaderV1;

check_compile_time (sizeof (HIDTransportHeader) == 4);


// Packet Definitions for HIDTransportHeaderV0

typedef struct __attribute__((__packed__)) {
    uint32_t        deviceID   : 7;
    uint32_t        length     : 10;
    uint32_t        packetType : 3;
    uint32_t        hasTS      : 1;
    uint32_t        isResponse : 1;
    uint32_t        isError    : 1;
    uint32_t        reserved1  : 9;
} HIDDeviceHeader;

typedef HIDDeviceHeader HIDDeviceHeaderV0;

check_compile_time (sizeof (HIDDeviceHeader) == 4);

typedef struct __attribute__((__packed__)) {
    HIDDeviceHeader header;
    uint8_t         reportType;
    uint8_t         data[0];
} HIDDeviceReport;

check_compile_time (sizeof(HIDDeviceReport) == 5);

typedef struct __attribute__((__packed__)) {
    HIDDeviceHeader header;
    uint8_t         reportType;
    uint64_t        timestamp;
    uint8_t         data[0];
} HIDDeviceTimestampedReport;

check_compile_time (sizeof(HIDDeviceTimestampedReport) == 13);

typedef struct __attribute__((__packed__)) {
    HIDDeviceHeader header;
    uint8_t         data[0];
} HIDDeviceControl;

check_compile_time (sizeof(HIDDeviceControl) == 4);


// Packet Definitions for AFKBTTransportHeaderV1

typedef struct __attribute__((__packed__)) {
    uint32_t        deviceID   : 7;  // 128 - reasonable max # of EndpointInterfaces
    uint32_t        length     : 10; // 1024 - limitation of the AACP Packet
    uint32_t        isResponse : 1;  // If set, packet is response to command.
    uint32_t        isCommand  : 1;  // Applies only if isReponse not set. If set, packet is command, else is report.
    uint32_t        hasError   : 1;  // Applies if isResponse is set. Packet contains errorCode field.
    uint32_t        hasTS      : 1;  // If set, packet contains timestamp field.
    uint32_t        reserved   : 11;
} AFKBTInterfaceHeaderV1;

check_compile_time (sizeof (AFKBTInterfaceHeaderV1) == 4);

typedef struct __attribute__((__packed__)) {
    AFKBTInterfaceHeaderV1  header;
    uint8_t                 packetID;
    uint8_t                 payload[0];
} AFKBTInterfacePacketV1Response;

check_compile_time (sizeof (AFKBTInterfacePacketV1Response) == 5);

typedef struct __attribute__((__packed__)) {
    AFKBTInterfaceHeaderV1  header;
    uint8_t                 packetID;
    uint32_t                errorCode;
    uint8_t                 payload[0];
} AFKBTInterfacePacketV1ResponseError;

check_compile_time (sizeof (AFKBTInterfacePacketV1ResponseError) == 9);

typedef struct __attribute__((__packed__)) {
    AFKBTInterfaceHeaderV1  header;
    uint8_t                 packetID;
    uint64_t                timestamp;
    uint8_t                 payload[0];
} AFKBTInterfacePacketV1ResponseTimestamp;

check_compile_time (sizeof (AFKBTInterfacePacketV1ResponseTimestamp) == 13);

typedef struct __attribute__((__packed__)) {
    AFKBTInterfaceHeaderV1  header;
    uint8_t                 packetID;
    uint32_t                errorCode;
    uint64_t                timestamp;
    uint8_t                 payload[0];
} AFKBTInterfacePacketV1ResponseErrorTimestamp;

check_compile_time (sizeof (AFKBTInterfacePacketV1ResponseErrorTimestamp) == 17);

typedef struct __attribute__((__packed__)) {
    AFKBTInterfaceHeaderV1  header;
    uint8_t                 packetType;
    uint8_t                 payload[0];
} AFKBTInterfacePacketV1Report;

check_compile_time (sizeof (AFKBTInterfacePacketV1Report) == 5);

typedef struct __attribute__((__packed__)) {
    AFKBTInterfaceHeaderV1  header;
    uint8_t                 packetType;
    uint64_t                timestamp;
    uint8_t                 payload[0];
} AFKBTInterfacePacketV1ReportTimestamp;

check_compile_time (sizeof (AFKBTInterfacePacketV1ReportTimestamp) == 13);

typedef struct __attribute__((__packed__)) {
    AFKBTInterfaceHeaderV1  header;
    uint8_t                 packetType;
    uint8_t                 packetID;
    uint8_t                 payload[0];
} AFKBTInterfacePacketV1Command;

check_compile_time (sizeof (AFKBTInterfacePacketV1Command) == 6);

typedef struct __attribute__((__packed__)) {
    AFKBTInterfaceHeaderV1  header;
    uint8_t                 packetType;
    uint8_t                 packetID;
    uint64_t                timestamp;
    uint8_t                 payload[0];
} AFKBTInterfacePacketV1CommandTimestamp;

check_compile_time (sizeof (AFKBTInterfacePacketV1CommandTimestamp) == 14);

#endif /* HIDRemoteSimpleProtocol_h */

