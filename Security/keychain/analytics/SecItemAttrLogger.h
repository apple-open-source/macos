/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

#ifndef _SECITEMATTRLOGGER_H_
#define _SECITEMATTRLOGGER_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecTask.h>
#import "ipc/securityd_client.h"
#include <stdint.h>

__BEGIN_DECLS

// Report attributes telemetry for two dictionaries (e.g., query and attributes in SecItemUpdate)
void reportQueryAccessStructure(
    CFDictionaryRef dict1,
    CFDictionaryRef dict2,
    const char *operation,
    SecurityClient *client,
    CFStringRef accessGroup,
    uint64_t start_time,
    uint64_t end_time,
    int result_count,
    size_t result_size,
    CFErrorRef error);

__END_DECLS

#ifdef __OBJC__
// Attribute logger - encodes which kSec* attributes and values are present in dictionaries
// Format: Compressed encoding with only present keys
//
// Encoding structure:
//   - Keys without values: "key_index;" (e.g., "13;")
//   - Keys with values: "key_index,value_index;" (e.g., "12,1;")
//   - Error values: "key_index,error_constant;" (e.g., "108,-2;" for KEY_INVALID_VALUE)
//     Error constants: -2 (KEY_INVALID_VALUE), -3 (KEY_INVALID_VALUE_TYPE)
//
// Example encoding:
//   "0,0;2;12,1;13;88;"
//   - Key 0: present with value 0
//   - Key 2: present (doesn't track values)
//   - Key 12: present with value 1
//   - Key 13: present (doesn't track values)
//   - Key 88: present (doesn't track values)

@interface SecItemAttrLoggerInfo : NSObject

// Encoding version
@property (nonatomic, readonly) NSInteger encodingVersion;

// Actual encoding
@property (nonatomic, readonly) NSString *encoding;

// Number of keys tracked i.e. kSec* attributes
@property (nonatomic, readonly) NSUInteger numKeys;

// Create encoding from a query dictionary
+ (instancetype)loggerInfoWithDictionary:(NSDictionary *)dictionary;

// Print encoding info to system logs
+ (void)printEncodingInfo:(SecItemAttrLoggerInfo *)loggerInfo;

@end

#endif /* __OBJC__ */

#endif /* _SECITEMATTRLOGGER_H_ */
