/*
 * Copyright (c) 2015-2023 Apple Inc. All rights reserved.
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

#ifndef __os_log_h
#define __os_log_h

#include <os/object.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_attribute(not_tail_called)
#define OS_LOG_NOTAILCALL __attribute__((not_tail_called))
#define OS_LOG_NOTAILCALL_MARKER
#else
#define OS_LOG_NOTAILCALL
#define OS_LOG_NOTAILCALL_MARKER __asm__("")
#endif

__BEGIN_DECLS

extern void *__dso_handle;

#ifdef XNU_KERNEL_PRIVATE
extern bool startup_serial_logging_active;
extern uint64_t startup_serial_num_procs;
#endif /* XNU_KERNEL_PRIVATE */

#ifdef KERNEL
#define OS_LOG_BUFFER_MAX_SIZE 256
#else
#define OS_LOG_BUFFER_MAX_SIZE 1024
#endif

// The OS_LOG_BUFFER_MAX_SIZE limit includes the metadata that
// must be included in the os_log firehose buffer
#define OS_LOG_DATA_MAX_SIZE (OS_LOG_BUFFER_MAX_SIZE - 16)

OS_ALWAYS_INLINE
static inline void
    _os_log_verify_format_str(__unused const char *msg, ...)
__osloglike(1, 2);

OS_ALWAYS_INLINE
static inline void
_os_log_verify_format_str(__unused const char *msg, ...)                                       /* placeholder */
{
}

#if OS_OBJECT_USE_OBJC
OS_OBJECT_DECL(os_log);
#else
typedef struct os_log_s *os_log_t;
#endif /* OS_OBJECT_USE_OBJC */

/*!
 * @const OS_LOG_DISABLED
 *
 * @discussion
 * Use this to disable a specific log message.
 */
#define OS_LOG_DISABLED NULL

/*!
 * @const OS_LOG_DEFAULT
 *
 * @discussion
 * Use this to log a message in accordance with current system settings.
 */
#define OS_LOG_DEFAULT OS_OBJECT_GLOBAL_OBJECT(os_log_t, _os_log_default)
OS_EXPORT
struct os_log_s _os_log_default;

/*!
 * @enum os_log_type_t
 *
 * @discussion
 * Supported log message types.
 *
 * @constant OS_LOG_TYPE_DEFAULT
 * Equivalent type for "os_log()" messages, i.e., default messages that are always
 * captured to memory or disk.
 *
 * @constant OS_LOG_TYPE_INFO
 * Equivalent type for "os_log_info()" messages, i.e., Additional informational messages.
 *
 * @constant OS_LOG_TYPE_DEBUG
 * Equivalent type for "os_log_debug()" messages, i.e., Debug messages.
 *
 * @constant OS_LOG_TYPE_ERROR
 * Equivalent type for "os_log_error()" messages, i.e., local process error messages.
 *
 * @constant OS_LOG_TYPE_FAULT
 * Equivalent type for "os_log_fault()" messages, i.e., a system error that involves
 * potentially more than one process, usually used by daemons and services.
 */
OS_ENUM(os_log_type, uint8_t,
    OS_LOG_TYPE_DEFAULT = 0x00,
    OS_LOG_TYPE_INFO    = 0x01,
    OS_LOG_TYPE_DEBUG   = 0x02,
    OS_LOG_TYPE_ERROR   = 0x10,
    OS_LOG_TYPE_FAULT   = 0x11);

/*!
 * @function os_log_create
 *
 * @abstract
 * Creates a log object to be used with other log related functions.
 *
 * @discussion
 * Creates a log object to be used with other log related functions. The log
 * object serves two purposes: (1) tag related messages by subsystem and
 * category name for easy filtering, and (2) control logging system behavior for
 * messages.
 *
 * @param subsystem
 * The identifier of the given subsystem should be in reverse DNS form (i.e.,
 * com.company.mysubsystem). This string must be a constant string, not
 * dynamically generated.
 *
 * @param category
 * The category within the given subsystem that specifies the settings for the
 * log object. This string must be a constant string, not dynamically generated.
 *
 * @result
 * Returns an os_log_t value to be passed to other os_log API calls. This should
 * be called once at log initialization and rely on system to detect changes to
 * settings.
 *
 * A value will always be returned to allow for dynamic enablement.
 */
OS_EXPORT OS_NOTHROW OS_WARN_RESULT OS_OBJECT_RETURNS_RETAINED
os_log_t
os_log_create(const char *subsystem, const char *category);

/*!
 * @function os_log_info_enabled
 *
 * @abstract
 * Returns if additional information log messages are enabled for a particular
 * log object.
 *
 * @discussion
 * Returns if additional information log messages are enabled for a particular
 * log object.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @result
 * Returns ‘true’ if additional information log messages are enabled.
 */
OS_EXPORT OS_NOTHROW OS_WARN_RESULT
bool
os_log_info_enabled(os_log_t log);

/*!
 * @function os_log_debug_enabled
 *
 * @abstract
 * Returns if debug log messages are enabled for a particular log object.
 *
 * @discussion
 * Returns if debug log messages are enabled for a particular log object.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @result
 * Returns ‘true’ if debug log messages are enabled.
 */
OS_EXPORT OS_NOTHROW OS_WARN_RESULT
bool
os_log_debug_enabled(os_log_t log);

/*!
 * @function os_log
 *
 * @abstract
 * Insert a log message into the Unified Logging and Tracing system.
 *
 * @discussion
 * Insert a log message into the Unified Logging and Tracing system in
 * accordance with the preferences specified by the provided log object.
 * These messages cannot be disabled and therefore always captured either
 * to memory or disk.
 *
 * When an os_activity_id_t is present, the log message will also be scoped by
 * that identifier.  Activities provide granular filtering of log messages
 * across threads and processes.
 *
 * There is a physical cap of 256 bytes per entry for dynamic content,
 * i.e., %s and %@, that can be written to the persistence store.  As such,
 * all content exceeding the limit will be truncated before written to disk.
 * Live streams will continue to show the full content.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded.  This string must be a constant string, not dynamically
 * generated.  Supports all standard printf types and %@ (objects).
 */
#define os_log(log, format, ...) \
    os_log_with_type(log, OS_LOG_TYPE_DEFAULT, format, ##__VA_ARGS__)

/*!
 * @function os_log_info
 *
 * @abstract
 * Insert a development log message into the Unified Logging and Tracing system.
 *
 * @discussion
 * Insert a log message into the Unified Logging and Tracing system in
 * accordance with the preferences specified by the provided log object.
 *
 * When an os_activity_id_t is present, the log message will also be scoped by
 * that identifier.  Activities provide granular filtering of log messages
 * across threads and processes.
 *
 * There is a physical cap of 256 bytes per entry for dynamic content,
 * i.e., %s and %@, that can be written to the persistence store.  As such,
 * all content exceeding the limit will be truncated before written to disk.
 * Live streams will continue to show the full content.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded.  This string must be a constant string, not dynamically
 * generated.  Supports all standard printf types and %@ (objects).
 */
#define os_log_info(log, format, ...) \
    os_log_with_type(log, OS_LOG_TYPE_INFO, format, ##__VA_ARGS__)

/*!
 * @function os_log_debug
 *
 * @abstract
 * Insert a debug log message into the Unified Logging and Tracing system.
 *
 * @discussion
 * Insert a debug log message into the Unified Logging and Tracing system in
 * accordance with the preferences specified by the provided log object.
 *
 * When an os_activity_id_t is present, the log message will also be scoped by
 * that identifier.  Activities provide granular filtering of log messages
 * across threads and processes.
 *
 * There is a physical cap of 256 bytes per entry for dynamic content,
 * i.e., %s and %@, that can be written to the persistence store.  As such,
 * all content exceeding the limit will be truncated before written to disk.
 * Live streams will continue to show the full content.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded.  This string must be a constant string, not dynamically
 * generated.  Supports all standard printf types and %@ (objects).
 */
#define os_log_debug(log, format, ...) \
    os_log_with_type(log, OS_LOG_TYPE_DEBUG, format, ##__VA_ARGS__)

/*!
 * @function os_log_error
 *
 * @abstract
 * Insert an error log message into the Unified Logging and Tracing system.
 *
 * @discussion
 * Insert an error log message into the Unified Logging and Tracing system.
 *
 * When an os_activity_id_t is present, the log message will also be scoped by
 * that identifier.  Activities provide granular filtering of log messages
 * across threads and processes.
 *
 * There is a physical cap of 256 bytes per entry for dynamic content,
 * i.e., %s and %@, that can be written to the persistence store.  As such,
 * all content exceeding the limit will be truncated before written to disk.
 * Live streams will continue to show the full content.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded.  This string must be a constant string, not dynamically
 * generated.  Supports all standard printf types and %@ (objects).
 */
#define os_log_error(log, format, ...) \
    os_log_with_type(log, OS_LOG_TYPE_ERROR, format, ##__VA_ARGS__)

/*!
 * @function os_log_fault
 *
 * @abstract
 * Insert a fault log message into the Unified Logging and Tracing system.
 *
 * @discussion
 * Log a fault message issue into the Unified Logging and Tracing system
 * signifying a multi-process (i.e., system error) related issue, either
 * due to interaction via IPC or some other.  Faults will gather information
 * from the entire process chain and record it for later inspection.
 *
 * When an os_activity_id_t is present, the log message will also be scoped by
 * that identifier.  Activities provide granular filtering of log messages
 * across threads and processes.
 *
 * There is a physical cap of 256 bytes per entry for dynamic content,
 * i.e., %s and %@, that can be written to the persistence store.  As such,
 * all content exceeding the limit will be truncated before written to disk.
 * Live streams will continue to show the full content.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded.  This string must be a constant string, not dynamically
 * generated.  Supports all standard printf types and %@ (objects).
 */
#define os_log_fault(log, format, ...) \
    os_log_with_type(log, OS_LOG_TYPE_FAULT, format, ##__VA_ARGS__)

/*!
 * @function os_log_with_type
 *
 * @abstract
 * Log a message using a specific type.
 *
 * @discussion
 * Will log a message with the provided os_log_type_t.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param type
 * Pass a valid type from os_log_type_t.
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded.  This string must be a constant string, not dynamically
 * generated.  Supports all standard printf types and %@ (objects).
 */
#define os_log_with_type(log, type, format, ...) __extension__({                            \
    _Static_assert(__builtin_constant_p(format), "format string must be constant");         \
    __attribute__((section("__TEXT,__os_log"))) static const char _os_log_fmt[] = format;   \
    if (0) {                                                                                \
	_os_log_verify_format_str(format, ##__VA_ARGS__);                                   \
    } else {                                                                                  \
	_os_log_internal(&__dso_handle, log, type, _os_log_fmt, ##__VA_ARGS__);             \
    }                                                                                       \
    __asm__(""); /* avoid tailcall */                                                       \
})

/*!
 * @function os_log_at_time
 *
 * @abstract
 * Log a message using a specific type and a timestamp.
 *
 * @discussion
 * Will log a message with the provided os_log_type_t and a timestamp
 * signifying a moment of a log message creation.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param type
 * Pass a valid type from os_log_type_t.
 *
 * @param ts
 * Pass a uint64_t value (timestamp) of mach continuous time clock.
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded. This string must be a constant string, not dynamically
 * generated. Supports all standard printf types.
 */
#define os_log_at_time(log, type, ts, format, ...) __extension__({                          \
    _Static_assert(__builtin_constant_p(format), "format string must be constant");         \
    __attribute__((section("__TEXT,__os_log"))) static const char _os_log_fmt[] = format;   \
    if (0) {                                                                                \
	_os_log_verify_format_str(format, ##__VA_ARGS__);                                   \
    } else {                                                                                  \
	_os_log_at_time(&__dso_handle, log, type, ts, _os_log_fmt, ##__VA_ARGS__);          \
    }                                                                                       \
    __asm__(""); /* avoid tailcall */                                                       \
})

/*!
 * @function os_log_driverKit
 *
 * @abstract
 * Log a message using a specific type. This variant should be called only from dexts.
 *
 * @discussion
 * Will log a message with the provided os_log_type_t.
 *
 * @param log
 * Pass OS_LOG_DEFAULT or a log object previously created with os_log_create.
 *
 * @param type
 * Pass a valid type from os_log_type_t.
 *
 * @param format
 * A format string to generate a human-readable log message when the log
 * line is decoded.  This string must be a constant string, not dynamically
 * generated.  Supports all standard printf types and %@ (objects).
 *
 * @result
 * Returns EPERM if the caller is not a driverKit process, 0 in case of success.
 */
#define os_log_driverKit(out, log, type, format, ...) __extension__({                            \
    _Static_assert(__builtin_constant_p(format), "format string must be constant");         \
    __attribute__((section("__TEXT,__os_log"))) static const char _os_log_fmt[] = format;   \
    if (0) {                                                                                \
	_os_log_verify_format_str(format, ##__VA_ARGS__);                                   \
    } else {                                                                                  \
	(*(out)) = _os_log_internal_driverKit(&__dso_handle, log, type, _os_log_fmt, ##__VA_ARGS__);                 \
    }                                                                                       \
    __asm__(""); /* avoid tailcall */                                                       \
})

/*!
 * @function os_log_coprocessor
 *
 * @abstract
 * IOP logging function, intended for use by RTBuddy for coprocessor os log
 * functionality only.
 */
bool
os_log_coprocessor(void *buff, uint64_t buff_len, os_log_type_t type,
    const char *uuid, uint64_t timestamp, uint32_t offset, bool stream_log);

/*!
 * @function os_log_coprocessor_register
 *
 * @abstract
 * IOP metadata registration, intended for use by RTBuddy for coprocessor os log
 * functionality only. Will be removed after all user code will be updated to
 * use os_log_coprocessor_register_with_type.
 */
void
os_log_coprocessor_register(const char *uuid, const char *file_path, bool copy);

typedef enum {
	os_log_coproc_register_memory,
	os_log_coproc_register_harvest_fs_ftab,
} os_log_coproc_reg_t;

/*!
 * @function os_log_coprocessor_register_with_type
 *
 * @abstract
 * IOP metadata registration, intended for use by RTBuddy for coprocessor os log
 * functionality only.
 */
void
os_log_coprocessor_register_with_type(const char *uuid, const char *file_path, os_log_coproc_reg_t register_type);

#ifdef XNU_KERNEL_PRIVATE
#define os_log_with_startup_serial_and_type(log, type, format, ...) __extension__({ \
    if (startup_serial_logging_active) { printf(format, ##__VA_ARGS__); }           \
    else { os_log_with_type(log, type, format, ##__VA_ARGS__); }                    \
})
#define os_log_with_startup_serial(log, format, ...) \
    os_log_with_startup_serial_and_type(log, OS_LOG_TYPE_DEFAULT, format, ##__VA_ARGS__)
#define os_log_info_with_startup_serial(log, format, ...) \
    os_log_with_startup_serial_and_type(log, OS_LOG_TYPE_INFO, format, ##__VA_ARGS__)
#define os_log_debug_with_startup_serial(log, format, ...) \
    os_log_with_startup_serial_and_type(log, OS_LOG_TYPE_DEBUG, format, ##__VA_ARGS__)
#define os_log_error_with_startup_serial(log, format, ...) \
    os_log_with_startup_serial_and_type(log, OS_LOG_TYPE_ERROR, format, ##__VA_ARGS__)
#define os_log_fault_with_startup_serial(log, format, ...) \
    os_log_with_startup_serial_and_type(log, OS_LOG_TYPE_FAULT, format, ##__VA_ARGS__)
#endif /* XNU_KERNEL_PRIVATE */

/*!
 * @function _os_log_internal
 *
 * @abstract
 * Internal function used by macros.
 */
OS_EXPORT OS_NOTHROW
void
_os_log_internal(void *dso, os_log_t log, os_log_type_t type, const char *message, ...)
__osloglike(4, 5);

/*!
 * @function _os_log_internal_driverKit
 *
 * @abstract
 * Internal function used by macros.
 */
OS_EXPORT OS_NOTHROW
int
_os_log_internal_driverKit(void *dso, os_log_t log, os_log_type_t type, const char *message, ...)
__osloglike(4, 5);

/*!
 * @function _os_log_internal_props
 *
 * @abstract
 * Internal function used by macros.
 */
OS_EXPORT OS_NOTHROW
void
_os_log_at_time(void *dso, os_log_t log, os_log_type_t type, uint64_t ts, const char *message, ...)
__osloglike(5, 6);

__END_DECLS

#endif /* __os_log_h */
