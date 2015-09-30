/*
 * Copyright (c) 2009,2012,2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef APPLEMOBILEPERSONALIZEDTICKET_H
#define APPLEMOBILEPERSONALIZEDTICKET_H

const unsigned kApECIDTag                                 = 1;
const unsigned kApChipIDTag                               = 2;
const unsigned kApBoardIDTag                              = 3;
const unsigned kApProductionModeTag                       = 4;
const unsigned kApSecurityDomainTag                       = 5;
const unsigned kLLBBuildStringTag                         = 6;
const unsigned kiBootDigestTag                            = 7;
const unsigned kAppleLogoDigestTag                        = 8;
const unsigned kDeviceTreeDigestTag                       = 9;
const unsigned kKernelCacheDigestTag                      = 10;
const unsigned kDiagsDigestTag                            = 11;
const unsigned kBatteryChargingDigestTag                  = 12;
const unsigned kBatteryPluginDigestTag                    = 13;
const unsigned kBatteryLow0DigestTag                      = 14;
const unsigned kBatteryLow1DigestTag                      = 15;
const unsigned kRecoveryModeDigestTag                     = 16;
const unsigned kNeedServiceDigestTag                      = 17;
const unsigned kApNonceTag                                = 18;
const unsigned kApPriorTicketIDTag                        = 19;
const unsigned kiBSSBuildStringTag                        = 20;
const unsigned kHostiBootTag                              = 21;
const unsigned kiBECBuildStringTag                        = 22;
const unsigned kRestoreLogoDigestTag                      = 23;
const unsigned kRestoreDeviceTreeDigestTag                = 24;
const unsigned kRestoreKernelCacheDigestTag               = 25;
const unsigned kRestoreRamDiskDigestTag                   = 26;
const unsigned kOSDigestTag                               = 27;
const unsigned kApBindingDigestTag                        = 28;
const unsigned kApServerNonceTag                          = 29;
const unsigned kLLBPartialDigestTag                       = 30;
const unsigned kiBootPartialDigestTag                     = 31;
const unsigned kAppleLogoPartialDigestTag                 = 32;
const unsigned kDeviceTreePartialDigestTag                = 33;
const unsigned kKernelCachePartialDigestTag               = 34;
const unsigned kDiagsPartialDigestTag                     = 35;
const unsigned kBatteryChargingPartialDigestTag           = 36;
const unsigned kBatteryPluginPartialDigestTag             = 37;
const unsigned kBatteryLow0PartialDigestTag               = 38;
const unsigned kBatteryLow1PartialDigestTag               = 39;
const unsigned kRecoveryModePartialDigestTag              = 40;
const unsigned kNeedServicePartialDigestTag               = 41;
const unsigned kiBSSPartialDigestTag                      = 42;
const unsigned kiBECPartialDigestTag                      = 43;
const unsigned kRestoreLogoPartialDigestTag               = 44;
const unsigned kRestoreDeviceTreePartialDigestTag         = 45;
const unsigned kRestoreKernelCachePartialDigestTag        = 46;
const unsigned kRestoreRamDiskPartialDigestTag            = 47;
const unsigned kiBootTrustedTag                           = 48;
const unsigned kAppleLogoTrustedTag                       = 49;
const unsigned kDeviceTreeTrustedTag                      = 50;
const unsigned kKernelCacheTrustedTag                     = 51;
const unsigned kDiagsTrustedTag                           = 52;
const unsigned kBatteryChargingTrustedTag                 = 53;
const unsigned kBatteryPluginTrustedTag                   = 54;
const unsigned kBatteryLow0TrustedTag                     = 55;
const unsigned kBatteryLow1TrustedTag                     = 56;
const unsigned kRecoveryModeTrustedTag                    = 57;
const unsigned kNeedServiceTrustedTag                     = 58;
const unsigned kRestoreLogoTrustedTag                     = 59;
const unsigned kRestoreDeviceTreeTrustedTag               = 60;
const unsigned kRestoreKernelCacheTrustedTag              = 61;
const unsigned kRestoreRamDiskTrustedTag                  = 62;
const unsigned kBbSNUMTag                                 = 63;
const unsigned kBbChipIDTag                               = 64;
const unsigned kBbProductionModeTag                       = 65;
const unsigned kFlashPSIBuildStringTag                    = 66;
const unsigned kModemStackDigestTag                       = 67;
const unsigned kBbNonceTag                                = 68;
const unsigned kBbPriorTicketIdTag                        = 69;
const unsigned kRamPSIBuildStringTag                      = 70;
const unsigned kHostFlashPSITag                           = 71;
const unsigned kEBLDigestTag                              = 72;
const unsigned kStaticEEPDigestTag                        = 73;
const unsigned kBbApBindingDigestTag                      = 74;
const unsigned kBbServerNonceTag                          = 75;
const unsigned kRamPSIPartialDigestTag                    = 76;
const unsigned kFlashPSIPartialDigestTag                  = 77;
const unsigned kBatteryCharging0DigestTag                 = 78;
const unsigned kBatteryCharging1DigestTag                 = 79;
const unsigned kBatteryFullDigestTag                      = 80;
const unsigned kBatteryCharging0PartialDigestTag          = 81;
const unsigned kBatteryCharging1PartialDigestTag          = 82;
const unsigned kBatteryFullPartialDigestTag               = 83;
const unsigned kBatteryCharging0TrustedTag                = 84;
const unsigned kBatteryCharging1TrustedTag                = 85;
const unsigned kBatteryFullTrustedTag                     = 86;
const unsigned kUniqueBuildIDTag                          = 87;
const unsigned kBbGoldCertIdTag                           = 88;
const unsigned kBbSkeyIdTag                               = 89;
const unsigned kBasebandFirmwareFlashPSIVersionTag        = 90;
const unsigned kBasebandFirmwareModemStackDigestTag       = 91;
const unsigned kBasebandFirmwareRamPSIVersionTag          = 92;
const unsigned kBasebandFirmwareEBLDigestTag              = 93;
const unsigned kBasebandFirmwareFlashPSISecPackDigestTag  = 94;
const unsigned kBasebandFirmwareModemStackSecPackDigestTag= 95;
const unsigned kBasebandFirmwareFlashPSIDigestTag         = 96;
const unsigned kBasebandFirmwareRamPSIPartialDigestTag    = 97;
const unsigned kBasebandFirmwareFlashPSIPartialDigestTag  = 98;
const unsigned kBbJtagEnableTag                           = 99;


#endif /* APPLEMOBILEPERSONALIZEDTICKET_H */
