//
//  AWDMetricIds_Keychain.h
//  AppleWirelessDiagnostics
//
//  WARNING :: DO NOT MODIFY THIS FILE!
//
//     This file is auto-generated! Do not modify it or your changes will get overwritten!
//

#ifndef AWD_MetricId_HeaderGuard_Keychain
#define AWD_MetricId_HeaderGuard_Keychain

// Component Id:
// ---------------
//    Use this value for any API requesting the "component id" for your component.
enum {
    AWDComponentId_Keychain = 0x60
};


// Simple Metrics:
// ---------------
//    The following metrics are compatible with the 'simple metric' API:
enum {

    AWDMetricId_Keychain_SOSKeychainBackupFailed = 0x600004
};

// General Metrics:
// ----------------
enum {
    AWDMetricId_Keychain_CKKSRateLimiterOverload = 0x600000,
    AWDMetricId_Keychain_CKKSRateLimiterTopWriters = 0x600001,
    AWDMetricId_Keychain_CKKSRateLimiterAggregatedScores = 0x600002,
    AWDMetricId_Keychain_SecDbMarkedCorrupt = 0x600003
};

#endif  // AWD_MetricId_HeaderGuard_Keychain
