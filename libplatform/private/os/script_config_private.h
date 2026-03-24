/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#ifndef __OS_SCRIPT_CONFIG_PRIVATE_H__
#define __OS_SCRIPT_CONFIG_PRIVATE_H__

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_param.h>

#define OS_SCRIPT_CONFIG_SPI_VERSION 20250808

__BEGIN_DECLS

#pragma mark - Private SPI for Scripting Engine restrictions

#define OS_SCRIPT_CONFIG_STORAGE_SIZE PAGE_MAX_SIZE

/*!
 * @var os_script_config_storage
 *
 * @discussion
 * A region reserved for usage by scripting engines. This will be disabled if the
 * OS_SECURITY_CONFIG_SCRIPT_RESTRICTIONS policy applies to the process and
 * any accesses to the region will result in a crash.
 *
 */
__SPI_AVAILABLE(macos(26.4), ios(26.4), tvos(26.4), watchos(26.4), visionos(26.4))
extern uint8_t os_script_config_storage[OS_SCRIPT_CONFIG_STORAGE_SIZE];

__END_DECLS

#endif // __OS_SCRIPT_CONFIG_PRIVATE_H__
