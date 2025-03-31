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

#include "os/apt_private.h"
#include "os/internal.h"

// A stub implementation is needed for unsupported configs.
#define NEED_STUB 1


#ifdef NEED_STUB
void os_apt_msg_async_task_running_4swift(__unused uint64_t task_id) {}
void os_apt_msg_async_task_waiting_on_4swift(__unused uint64_t task_id) {}
#endif // NEED_STUB
