#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
#pragma once

enum : uint64_t {
    kGgFwUpdateTypeFirmware = 1 << 0,
    kGgFwUpdateTypeConfiguration = 1 << 1,
    kGgFwUpdateTypeDnvd = 1 << 2,
};

struct AppleGasGaugeUpdateUserClientInfo {
    uint32_t fwVersion;
    uint32_t configVersion;
    uint32_t dnvd1Version;
    uint32_t dnvd2Version;
    uint32_t cryptoVersion;
    uint32_t chipId;
    uint32_t deviceType;
    uint32_t chemistry;
    uint32_t hwId;
    uint16_t updaterStatus;
    uint8_t setIdsSupported[12];
    uint8_t setIds[12];
    union {
        uint8_t ui8raw;
        struct {
            uint8_t isTwoStageSupported:1;
            uint8_t reserved:7;
        };
    } flags;
};

struct AppleGasGaugeUpdateUserClientOData {
    uint8_t nonce[32];
    uint32_t batteryId;
};

enum AppleGasGaugeUpdateUserClientDataType : uint64_t {
    kGgFwUpdDataTypeCertificate,
    kGgFwUpdDataTypeImg4Manifest,
    kGgFwUpdDataTypeDigestDictionary,
    kGgFwUpdDataTypeDnvdImage,
    kGgFwUpdDataTypeConfigImage,
    kGgFwUpdDataTypeFirmwareImage,
    kGgFwUpdDataTypeMax
};

struct AppleGasGaugeUpdateUserClientIData {
    union {
        uint8_t signature[64];
        uint8_t updateImage;
    };
    uint64_t data_length;
    enum AppleGasGaugeUpdateUserClientDataType type;
    uint8_t data[];
};

/* Method index */
enum {
    kGgFwUpdInfo,
    kGgFwUpdStartUpdate,
    kGgFwUpdSendData,
    kGgFwUpdCommitImage,
    kGgFwUpdMax
};

#endif // TARGET_OS_IOS || TARGET_OS_WATCH || (TARGET_OS_OSX && TARGET_CPU_ARM64)
