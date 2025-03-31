/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef __OS_APT_PRIVATE__
#define __OS_APT_PRIVATE__

#include <Availability.h>
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <os/base.h>

#define OS_APT_SPI_VERSION 20241023

#define OS_APT_MSG_AVAILABILITY \
		__SPI_AVAILABLE(macos(15.4), ios(18.4), tvos(18.4), watchos(11.4), visionos(2.4))

__BEGIN_DECLS

#pragma mark - Message namespaces and types

OS_APT_MSG_AVAILABILITY
OS_ENUM(apt_msg_namespace, uint8_t,
  apt_msg_ns_rsvd = 0,
  apt_msg_ns_swift = 1,
);

OS_APT_MSG_AVAILABILITY
OS_ENUM(apt_namespace_swift, uint8_t,
  apt_msg_ty_swift_rsvd = 0,
  apt_msg_ty_swift_task_running = 1,
  apt_msg_ty_swift_task_waiting_on = 2,
);

#pragma mark - Private SPI for Swift Concurrency runtime

/*!
 * @function os_apt_msg_async_task_running_4swift
 *
 * @abstract
 * Indicate that a Swift async task is running.
 *
 * @param task_id
 * ID of the Swift async task that is running.
 */
OS_APT_MSG_AVAILABILITY
OS_EXPORT OS_NOTHROW
void os_apt_msg_async_task_running_4swift(uint64_t task_id);

/*!
 * @function os_apt_msg_async_task_waiting_on_4swift
 *
 * @abstract
 * Indicate that the current Swift async task is waiting on a result from another task.
 *
 * @param task_id
 * Task ID of the task that the current Swift async task is waiting on.
 */
OS_APT_MSG_AVAILABILITY
OS_EXPORT OS_NOTHROW
void os_apt_msg_async_task_waiting_on_4swift(uint64_t task_id);

__END_DECLS

#endif // __OS_APT_PRIVATE__
