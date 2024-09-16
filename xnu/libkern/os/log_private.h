/*
 * Copyright (c) 2015-2024 Apple Inc. All rights reserved.
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

#ifndef __os_log_private_h
#define __os_log_private_h

#include <os/log.h>
#include <firehose/tracepoint_private.h>
#include <sys/queue.h>

#define OS_LOG_XNU_SUBSYSTEM  "com.apple.xnu"
#define OS_LOG_SUBSYSTEM      "com.apple.xnu.oslog"
#define OS_LOG_MAX_SIZE_ORDER 10 // Maximum log size order (1024 bytes)

__BEGIN_DECLS

/*!
 * @function os_log_with_args
 *
 * @abstract
 * os_log variant that supports va_list args.
 *
 * @discussion
 * os_log variant that supports va_list args.  This SPI should only be used
 * to shim legacy logging systems through os_log.
 *
 * @param oslog
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param type
 * Pass one of the following message types.
 *   OS_LOG_TYPE_DEFAULT
 *   OS_LOG_TYPE_DEBUG
 *   OS_LOG_TYPE_INFO
 *   OS_LOG_TYPE_ERROR
 *   OS_LOG_TYPE_FAULT
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded.  Supports all standard printf types in addition to %@
 * and %m (objects and errno respectively).
 *
 * @param args
 * A va_list containing the values for the format string.
 *
 * @param ret_addr
 * Pass the __builtin_return_address(0) of the function that created the
 * va_list from variadic arguments.  The caller must be the same binary
 * that generated the message and provided the format string.
 */
void
os_log_with_args(os_log_t oslog, os_log_type_t type, const char *format, va_list args, void *ret_addr)
__osloglike(3, 0);

/*
 * A private interface allowing to emit already encoded log messages.
 */
bool os_log_encoded_metadata(firehose_tracepoint_id_u, uint64_t, const void *, size_t);
bool os_log_encoded_signpost(firehose_stream_t, firehose_tracepoint_id_u, uint64_t, const void *, size_t, size_t);
bool os_log_encoded_log(firehose_stream_t, firehose_tracepoint_id_u, uint64_t, const void *, size_t, size_t);

__END_DECLS

#endif // __os_log_private_h
