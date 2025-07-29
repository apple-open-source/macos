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

#ifndef __OS_SECURITY_CONFIG_PRIVATE__
#define __OS_SECURITY_CONFIG_PRIVATE__

#include <AppleFeatures/AppleFeatures.h>
#include <os/base.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <mach/mach_types.h>

__BEGIN_DECLS

/*!
 * @enum os_security_config_t
 *
 * @discussion
 * Supported security configurations that a process/task can have.
 *
 * @constant OS_SECURITY_CONFIG_NONE
 * No security config
 *
 * @constant OS_SECURITY_CONFIG_TPRO
 * Trusted Path Read Only
 *
 * @constant OS_SECURITY_CONFIG_HARDENED_HEAP
 * Hardened Heap
 */
__SPI_AVAILABLE(macos(15.6), ios(18.6), tvos(18.6), watchos(11.6), visionos(2.6), driverkit(24.6))
OS_OPTIONS(os_security_config, uint64_t,
  OS_SECURITY_CONFIG_NONE = 0x0,
  OS_SECURITY_CONFIG_HARDENED_HEAP = 0x1,
  OS_SECURITY_CONFIG_TPRO = 0x2,
);

/*!
 * @function os_security_config_get
 *
 * @abstract
 * Provide the security configuration value for the current process.
 *
 * @result
 * The value for the security configuration in the current process.
 */
__SPI_AVAILABLE(macos(15.6), ios(18.6), tvos(18.6), watchos(11.6), visionos(2.6), driverkit(24.6))
OS_EXPORT OS_NOTHROW
os_security_config_t
os_security_config_get(void);

/*!
 * @function os_security_config_get_for_proc
 *
 * @abstract
 * Provide the security configuration value for the target process.
 *
 * @param pid
 * Process identifier for the target process.
 *
 * @param config
 * Pointer to a os_security_config_t to which this function will write the
 * result of the operation, if completed successfully.
 *
 * @result
 * Returns -1 if the operation could not be completed, 0 otherwise.
 */
__SPI_AVAILABLE(macos(15.6), ios(18.6), tvos(18.6), watchos(11.6), visionos(2.6), driverkit(24.6))
OS_EXPORT OS_NOTHROW OS_NONNULL_ALL
int
os_security_config_get_for_proc(pid_t pid, os_security_config_t *config);

/*!
 * @function os_security_config_get_for_task
 *
 * @abstract
 * Provide the security configuration value for the target task.
 *
 * @param task
 * The target task.
 *
 * @param config
 * Pointer to a os_security_config_t to which this function will write the
 * result of the operation, if completed successfully.
 *
 * @result
 * Returns -1 if the operation could not be completed, 0 otherwise.
 */
__SPI_AVAILABLE(macos(15.6), ios(18.6), tvos(18.6), watchos(11.6), visionos(2.6), driverkit(24.6))
OS_EXPORT OS_NOTHROW OS_NONNULL_ALL
int
os_security_config_get_for_task(task_t task, os_security_config_t *config);

__END_DECLS

#endif // __OS_SECURITY_CONFIG_PRIVATE__
