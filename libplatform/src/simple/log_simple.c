/*
 * Copyright (c) 2020 Apple Computer, Inc. All rights reserved.
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

#include <os/log_simple_private.h>
#include <_simple.h>
#include <errno.h>
#include <platform/string.h>
#include <platform/compat.h>
#include <mach/mach_time.h>
#include <sys/proc_info.h>
#include <libproc.h>
#include <pthread/private.h>
#include <mach-o/loader.h>
#include <sys/syslog.h>
#include <os/overflow.h>

#if !TARGET_OS_DRIVERKIT

static void
_os_log_simple_uuid_copy(uuid_t dest, const uuid_t src) {
	memmove(dest, src, sizeof(uuid_t));
}

uint64_t
os_log_simple_now(void)
{
	// If we want to support translated processes, then we'll need to copy
	// the rescaling logic from _os_trace_mach_continuous_time
	return mach_continuous_time();
}

uint8_t
os_log_simple_type_from_asl(int level)
{
	if (level < 0) level = 0;
	if (level > 7) level = 7;

	static uint8_t _level2ostype[] = {
		[LOG_EMERG] = OS_LOG_SIMPLE_TYPE_ERROR,
		[LOG_ALERT] = OS_LOG_SIMPLE_TYPE_ERROR,
		[LOG_CRIT] = OS_LOG_SIMPLE_TYPE_ERROR,
		[LOG_ERR] = OS_LOG_SIMPLE_TYPE_ERROR,
		[LOG_WARNING] = OS_LOG_SIMPLE_TYPE_DEFAULT,
		[LOG_NOTICE] = OS_LOG_SIMPLE_TYPE_DEFAULT,
		[LOG_INFO] = OS_LOG_SIMPLE_TYPE_INFO,
		[LOG_DEBUG] = OS_LOG_SIMPLE_TYPE_DEBUG,
	};
	return _level2ostype[level];
}

static void
_os_log_simple_set_proc(os_log_simple_payload_t *payload)
{
	pid_t pid = getpid();
	payload->pid = pid;

	struct proc_uniqidentifierinfo pinfo;
	if (proc_pidinfo(pid, PROC_PIDUNIQIDENTIFIERINFO, 0, &pinfo, sizeof(pinfo))
			!= sizeof(pinfo)) {
		// Leave fields unset on error
		return;
	}
	payload->unique_pid = pinfo.p_uniqueid;
	payload->pid_version = pinfo.p_idversion;

	_os_log_simple_uuid_copy(payload->process_uuid, pinfo.p_uuid);
}

static void
_os_log_simple_set_offset(os_log_simple_payload_t *payload,
		uint64_t absolute_offset, const struct mach_header *sender_mh,
		uint64_t dsc_load_addr)
{
	uint64_t offset = 0;
	if (sender_mh->flags & MH_DYLIB_IN_CACHE) {
		offset = absolute_offset - dsc_load_addr;
	} else {
		offset = absolute_offset - (uint64_t)(uintptr_t)sender_mh;
	}

	payload->relative_offset = offset;
}

void
__os_log_simple_offset(const struct mach_header *sender_mh,
		const uuid_t sender_uuid, const uuid_t dsc_uuid,
		uintptr_t dsc_load_addr, uint64_t absolute_offset,
		uint8_t type, const char *subsystem, const char *message)
{
	os_log_simple_payload_t payload = { 0 };
	payload.type = type;
	payload.subsystem = subsystem;
	payload.message = message;

	payload.timestamp = os_log_simple_now();

	_os_log_simple_set_proc(&payload);

#if ! defined(VARIANT_DYLD)
	payload.tid = _pthread_threadid_self_np_direct();
#endif // ! defined(VARIANT_DYLD)

	// Legacy simple_asl shim does not use the dyld macro wrappers
	// so we can't correctly resolve sender/offset
	if (sender_mh != NULL) {
		_os_log_simple_set_offset(&payload, absolute_offset, sender_mh,
				dsc_load_addr);
		_os_log_simple_uuid_copy(payload.sender_uuid, sender_uuid);
	} else {
		// Fallback path, use the process uuid as the sender
		_os_log_simple_uuid_copy(payload.sender_uuid, payload.process_uuid);
	}

	_os_log_simple_uuid_copy(payload.dsc_uuid, dsc_uuid);
	_os_log_simple_send(&payload);
}

void
_os_log_simple(const struct mach_header *sender_mh, uuid_t sender_uuid,
		uuid_t dsc_uuid, uintptr_t dsc_load_addr, uint8_t type,
		const char *subsystem, const char *fmt, ...)
{
	_SIMPLE_STRING message = _simple_salloc();
	if (message == NULL) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	_simple_vesprintf(message, NULL, fmt, ap);
	va_end(ap);

	uint64_t offset = (uint64_t)(uintptr_t)__builtin_return_address(0);

	__os_log_simple_offset(sender_mh, sender_uuid, dsc_uuid, dsc_load_addr,
			offset, type, subsystem, _simple_string(message));
	_simple_sfree(message);
}

void
_os_log_simple_shim(uint8_t type, const char *subsystem, const char *message)
{
	__os_log_simple_offset(NULL, UUID_NULL, UUID_NULL, 0, 0, type,
			subsystem, message);
}

extern int
_simple_asl_get_fd(void); // From asl.c

extern ssize_t
__sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);

typedef struct {
	uint16_t message_size; // includes null terminator
	uint16_t subsystem_size; // Set to 0 if there is no subsystem
	uint8_t type;
	uint64_t timestamp;
	uint64_t pid;
	uint64_t unique_pid;
	uint64_t pid_version;
	uint64_t tid;
	uint64_t relative_offset;
	uuid_t sender_uuid;
	uuid_t process_uuid;
	uuid_t dsc_uuid;
	char data[];
	// message = data [null-terminated]
	// subsystem = data + message_size [null terminated]
} os_log_simple_wire_t;

#define WIRE_BUFFER_SIZE 2048

static bool
_os_log_simple_is_disabled(void)
{
	// launchd and other early boot clients may emit os_log_simple messages
	// before logd comes up and sets the OS_TRACE_MODE_OFF bit. Thus we need
	// to check if the logs are disabled for each message and cannot cache
	// the result.
	uint32_t atm_config = *(uint32_t *)_COMM_PAGE_ATM_DIAGNOSTIC_CONFIG;
	if (atm_config & 0x0400) { /* OS_TRACE_MODE_OFF */
		return true;
	}
	return false;
}

int
_os_log_simple_send(os_log_simple_payload_t *payload)
{
	int asl_fd = _simple_asl_get_fd();
	if (asl_fd < 0) {
		return EBADF;
	}

	if (_os_log_simple_is_disabled()) {
		return 0;
	}

	size_t subsystem_size = 0;
	size_t message_size = strlen(payload->message) + 1;

	// Subsystem is optional
	if (payload->subsystem) {
		subsystem_size = strlen(payload->subsystem) + 1;
	}

	size_t send_size = 0;
	if (os_add3_overflow(sizeof(os_log_simple_wire_t), message_size, subsystem_size, &send_size)) {
		return EOVERFLOW;
	}

	if (send_size > WIRE_BUFFER_SIZE) {
		return E2BIG;
	}

	char buffer[WIRE_BUFFER_SIZE];
	os_log_simple_wire_t *wire = (os_log_simple_wire_t *)buffer;

	// Since WIRE_BUFFER_SIZE << 2**16, we can safely down-cast to uint16_t
	wire->message_size = (uint16_t)message_size;
	wire->subsystem_size = (uint16_t)subsystem_size;

	wire->type = payload->type;
	wire->timestamp = payload->timestamp;
	wire->pid = payload->pid;
	wire->unique_pid = payload->unique_pid;
	wire->pid_version = payload->pid_version;
	wire->tid = payload->tid;
	wire->relative_offset = payload->relative_offset;

	_os_log_simple_uuid_copy(wire->sender_uuid, payload->sender_uuid);
	_os_log_simple_uuid_copy(wire->process_uuid, payload->process_uuid);
	_os_log_simple_uuid_copy(wire->dsc_uuid, payload->dsc_uuid);

	char *wire_message = wire->data;
	strlcpy(wire_message, payload->message, message_size);

	if (subsystem_size > 0) {
		char *wire_subsystem = wire->data + message_size;
		strlcpy(wire_subsystem, payload->subsystem, subsystem_size);
	}

	ssize_t bytes = __sendto(asl_fd, buffer, send_size, 0, NULL, 0);
	int error = 0;
	if (bytes < 0) {
		error = errno;
	} else if (bytes < send_size) {
		error = EMSGSIZE;
	}

	return error;
}

int
_os_log_simple_parse_type(os_log_simple_payload_t *payload, os_log_simple_wire_t *wire)
{
	switch (wire->type) {
	case OS_LOG_SIMPLE_TYPE_DEFAULT:
	case OS_LOG_SIMPLE_TYPE_INFO:
	case OS_LOG_SIMPLE_TYPE_DEBUG:
	case OS_LOG_SIMPLE_TYPE_ERROR:
		payload->type = wire->type;
		return 0;
	default:
		return EINVAL;
	}
}

int
_os_log_simple_parse_subsystem(os_log_simple_payload_t *payload, os_log_simple_wire_t *wire)
{
	if (wire->subsystem_size == 0) {
		// No subsystem
		payload->subsystem = NULL;
	} else {
		char *wire_subsystem = wire->data + wire->message_size;
		// subsystem_size includes space for the nul-terminator
		if (wire_subsystem[wire->subsystem_size - 1] != '\0') {
			return EINVAL;
		}
		// wire is an alias for the client provided buffer; return a
		// pointer within that buffer to the client to avoid allocating memory
		payload->subsystem = wire_subsystem;
	}
	return 0;
}

int
_os_log_simple_parse_message(os_log_simple_payload_t *payload, os_log_simple_wire_t *wire)
{
	if (wire->message_size == 0) {
		return EINVAL; // message is mandatory
	}

	char *wire_message = wire->data;
	// message_size includes space for the nul-terminator
	if (wire_message[wire->message_size - 1] != '\0') {
		return EINVAL;
	}
	// wire is an alias for the client provided buffer; return a
	// pointer within that buffer to the client to avoid allocating memory
	payload->message = wire_message;
	return 0;
}

int
_os_log_simple_parse_timestamp(os_log_simple_payload_t *payload, os_log_simple_wire_t *wire)
{
	payload->timestamp = wire->timestamp;
	if (payload->timestamp == 0 || payload->timestamp > os_log_simple_now()) {
		// Forbid unset timestamps or time from the future
		return EINVAL;
	}
	return 0;
}

int
_os_log_simple_parse_identifiers(os_log_simple_payload_t *payload, os_log_simple_wire_t *wire)
{
	payload->pid = wire->pid;
	payload->unique_pid = wire->unique_pid;
	payload->pid_version = wire->pid_version;
	payload->tid = wire->tid;

	if (payload->pid == 0) {
		return EINVAL;
	}

	// If we encounter an error trying to call proc_pidinfo, we may not have a
	// valid unique_pid or pid_version, so we don't validate these fields
	return 0;
}

int
_os_log_simple_parse(const char *buffer, size_t length, os_log_simple_payload_t *payload_out)
{
	if (length < sizeof(os_log_simple_wire_t)) {
		return EBADMSG;
	}

	os_log_simple_wire_t *wire = (os_log_simple_wire_t *)buffer;

	size_t needed_size = 0;
	if (os_add3_overflow(sizeof(os_log_simple_wire_t), wire->message_size,
			wire->subsystem_size, &needed_size)) {
		return EOVERFLOW;
	}

	if (length < needed_size) {
		return E2BIG;
	}

	int error = _os_log_simple_parse_type(payload_out, wire);
	if (error) {
		return error;
	}

	error = _os_log_simple_parse_subsystem(payload_out, wire);
	if (error) {
		return error;
	}

	error = _os_log_simple_parse_message(payload_out, wire);
	if (error) {
		return error;
	}

	error = _os_log_simple_parse_timestamp(payload_out, wire);
	if (error) {
		return error;
	}

	error = _os_log_simple_parse_identifiers(payload_out, wire);
	if (error) {
		return error;
	}

	payload_out->relative_offset = wire->relative_offset;

	// Nothing to validate for uuids
	_os_log_simple_uuid_copy(payload_out->sender_uuid, wire->sender_uuid);
	_os_log_simple_uuid_copy(payload_out->process_uuid, wire->process_uuid);
	_os_log_simple_uuid_copy(payload_out->dsc_uuid, wire->dsc_uuid);

	return 0;
}

#endif // !TARGET_OS_DRIVERKIT
