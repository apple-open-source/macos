/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _REASON_H_
#define _REASON_H_

#include <stdint.h>

__BEGIN_DECLS

#ifdef KERNEL_PRIVATE

#include <kern/kern_cdata.h>

#ifdef XNU_KERNEL_PRIVATE
#include <os/refcnt.h>
#include <kern/locks.h>

typedef struct os_reason {
	decl_lck_mtx_data(, osr_lock);
	os_refcnt_t                     osr_refcount;
	uint32_t                        osr_namespace;
	uint64_t                        osr_code;
	uint64_t                        osr_flags;
	uint32_t                        osr_bufsize;
	struct kcdata_descriptor        osr_kcd_descriptor;
	char                            *osr_kcd_buf;
} *os_reason_t;

#define OS_REASON_NULL ((os_reason_t) NULL)

/* We only include 800 bytes of the exit reason description to not blow through the panic buffer */
#define LAUNCHD_PANIC_REASON_STRING_MAXLEN "800"

void os_reason_init(void);

os_reason_t build_userspace_exit_reason(uint32_t reason_namespace, uint64_t reason_code, user_addr_t payload, uint32_t payload_size,
    user_addr_t reason_string, uint64_t reason_flags);
char *exit_reason_get_string_desc(os_reason_t exit_reason);

/* The blocking allocation is currently not exported to KEXTs */
int os_reason_alloc_buffer(os_reason_t cur_reason, uint32_t osr_bufsize);

typedef struct _exception_info {
	int os_reason;
	int signal;
	exception_type_t exception_type;
	mach_exception_data_type_t mx_code;
	mach_exception_data_type_t mx_subcode;
	struct kt_info {
		int kt_subsys;
		uint32_t kt_error;
	} kt_info;
} exception_info_t;

#define PX_FLAGS_NONE           0
/* think twice about userspace debugging experience before using PX_DEBUG_NO_HONOR */
#define PX_DEBUG_NO_HONOR       (1 << 0) /* force exit even when debugging */
#define PX_KTRIAGE              (1 << 1) /* leave a ktriage record */
#define PX_PSIGNAL              (1 << 2) /* send sig instead of forced exit */
#define PX_NO_EXCEPTION_UTHREAD (1 << 3) /* do not set bsdthread exception */

int exit_with_mach_exception(struct proc *p, exception_info_t exception, uint32_t flags);
#if CONFIG_EXCLAVES
int exit_with_exclave_exception(struct proc *p, exception_info_t exception, uint32_t flags);
#endif

#else /* XNU_KERNEL_PRIVATE */

typedef void * os_reason_t;

#endif /* XNU_KERNEL_PRIVATE */

os_reason_t os_reason_create(uint32_t osr_namespace, uint64_t osr_code);
int os_reason_alloc_buffer_noblock(os_reason_t cur_reason, uint32_t osr_bufsize);
struct kcdata_descriptor * os_reason_get_kcdata_descriptor(os_reason_t cur_reason);
void os_reason_ref(os_reason_t cur_reason);
void os_reason_free(os_reason_t cur_reason);
void os_reason_set_flags(os_reason_t cur_reason, uint64_t flags);
void os_reason_set_description_data(os_reason_t cur_reason, uint32_t type, void *reason_data, uint32_t reason_data_len);
#endif /* KERNEL_PRIVATE */

/*
 * Reason namespaces.
 */
#define OS_REASON_INVALID       0
#define OS_REASON_JETSAM        1
#define OS_REASON_SIGNAL        2
#define OS_REASON_CODESIGNING   3
#define OS_REASON_HANGTRACER    4
#define OS_REASON_TEST          5
#define OS_REASON_DYLD          6
#define OS_REASON_LIBXPC        7
#define OS_REASON_OBJC          8
#define OS_REASON_EXEC          9
#define OS_REASON_SPRINGBOARD   10
#define OS_REASON_TCC           11
#define OS_REASON_REPORTCRASH   12
#define OS_REASON_COREANIMATION 13
#define OS_REASON_AGGREGATED    14
#define OS_REASON_RUNNINGBOARD  15
#define OS_REASON_ASSERTIOND    OS_REASON_RUNNINGBOARD  /* old name */
#define OS_REASON_SKYWALK       16
#define OS_REASON_SETTINGS      17
#define OS_REASON_LIBSYSTEM     18
#define OS_REASON_FOUNDATION    19
#define OS_REASON_WATCHDOG      20
#define OS_REASON_METAL         21
#define OS_REASON_WATCHKIT      22
#define OS_REASON_GUARD         23
#define OS_REASON_ANALYTICS     24
#define OS_REASON_SANDBOX       25
#define OS_REASON_SECURITY      26
#define OS_REASON_ENDPOINTSECURITY      27
#define OS_REASON_PAC_EXCEPTION 28
#define OS_REASON_BLUETOOTH_CHIP 29
#define OS_REASON_PORT_SPACE    30
#define OS_REASON_WEBKIT        31
#define OS_REASON_BACKLIGHTSERVICES 32
#define OS_REASON_MEDIA 33
#define OS_REASON_ROSETTA 34
#define OS_REASON_LIBIGNITION 35
#define OS_REASON_BOOTMOUNT 36


#define OS_REASON_REALITYKIT 38
#define OS_REASON_AUDIO      39
#define OS_REASON_WAKEBOARD  40
#define OS_REASON_CORERC     41

/*
 * Update whenever new OS_REASON namespaces are added.
 */
#define OS_REASON_MAX_VALID_NAMESPACE OS_REASON_CORERC

#define OS_REASON_BUFFER_MAX_SIZE 5120

#define OS_REASON_FLAG_NO_CRASH_REPORT          0x1    /* Don't create a crash report */
#define OS_REASON_FLAG_GENERATE_CRASH_REPORT    0x2    /* Create a crash report - the default for userspace requests */
#define OS_REASON_FLAG_FROM_USERSPACE           0x4    /* Reason created from a userspace syscall */
#define OS_REASON_FLAG_FAILED_DATA_COPYIN       0x8    /* We failed to copyin data from userspace */
#define OS_REASON_FLAG_PAYLOAD_TRUNCATED        0x10   /* The payload was truncated because it was longer than allowed */
#define OS_REASON_FLAG_BAD_PARAMS               0x20   /* Invalid parameters were passed involved with creating this reason */
#define OS_REASON_FLAG_CONSISTENT_FAILURE       0x40   /* Whatever caused this reason to be created will happen again */
#define OS_REASON_FLAG_ONE_TIME_FAILURE         0x80   /* Whatever caused this reason to be created was a one time issue */
#define OS_REASON_FLAG_NO_CRASHED_TID           0x100  /* Don't include the TID that processed the exit in the crash report */
#define OS_REASON_FLAG_ABORT                    0x200  /* Reason created from abort_* rather than terminate_* */
#define OS_REASON_FLAG_SHAREDREGION_FAULT       0x400  /* Fault happened within the shared cache region */
#define OS_REASON_FLAG_CAPTURE_LOGS             0x800  /* The report generated for this reason should capture logs */
#define OS_REASON_FLAG_SECURITY_SENSITIVE       0x1000 /* Mark as security sensitive for priority treatment */

/*
 * Set of flags that are allowed to be passed from userspace
 */
#define OS_REASON_FLAG_MASK_ALLOWED_FROM_USER (OS_REASON_FLAG_CONSISTENT_FAILURE | OS_REASON_FLAG_ONE_TIME_FAILURE | OS_REASON_FLAG_NO_CRASH_REPORT | OS_REASON_FLAG_ABORT | OS_REASON_FLAG_CAPTURE_LOGS | OS_REASON_FLAG_SECURITY_SENSITIVE)

/*
 * Macros to encode the exit reason namespace and first 32 bits of code in exception code
 * which is used by Report Crash as a hint. It should be only used as a hint since it
 * loses higher 32 bits of exit reason code.
 */
#define ENCODE_OSR_NAMESPACE_TO_MACH_EXCEPTION_CODE(code, osr_namespace) \
	(code) = (code) | (((osr_namespace) & ((uint64_t)UINT32_MAX)) << 32)
#define ENCODE_OSR_CODE_TO_MACH_EXCEPTION_CODE(code, osr_code) \
	(code) = (code) | ((osr_code) & ((uint64_t)UINT32_MAX))

#ifndef KERNEL
/*
 * abort_with_reason: Used to exit the current process and pass along
 *                    specific information about why it is being terminated.
 *
 * Inputs:              args->reason_namespace - OS_REASON namespace specified for the reason
 *                      args->reason_code - code in the specified namespace for the reason
 *                      args->reason_string - additional string formatted information about the request
 *                      args->reason_flags - options requested for how the process should be terminated (see OS_REASON_FLAG_* above).
 *
 * Outputs:             Does not return.
 */
void abort_with_reason(uint32_t reason_namespace, uint64_t reason_code, const char *reason_string, uint64_t reason_flags)
__attribute__((noreturn, cold));

/*
 * abort_with_payload: Used to exit the current process and pass along
 *                     specific information about why it is being terminated. The payload pointer
 *                     should point to structured data that can be interpreted by the consumer of
 *                     exit reason information.
 *
 * Inputs:              args->reason_namespace - OS_REASON namespace specified for the reason
 *                      args->reason_code - code in the specified namespace for the reason
 *                      args->payload - pointer to payload structure in user space
 *                      args->payload_size - length of payload buffer (this will be truncated to EXIT_REASON_PAYLOAD_MAX_LEN)
 *                      args->reason_string - additional string formatted information about the request
 *                      args->reason_flags - options requested for how the process should be terminated (see OS_REASON_FLAG_* above).
 *
 * Outputs:             Does not return.
 */
void abort_with_payload(uint32_t reason_namespace, uint64_t reason_code, void *payload, uint32_t payload_size, const char *reason_string,
    uint64_t reason_flags) __attribute__((noreturn, cold));

/*
 * terminate_with_reason: Used to terminate a specific process and pass along
 *                        specific information about why it is being terminated.
 *
 * Inputs:              args->pid - the PID of the process to be terminated
 *                      args->reason_namespace - OS_REASON namespace specified for the reason
 *                      args->reason_code - code in the specified namespace for the reason
 *                      args->reason_string - additional string formatted information about the request
 *                      args->reason_flags - options requested for how the process should be terminated (see OS_REASON_FLAG_* above)
 *
 * Outputs:             returns -1 and sets errno to EINVAL if the PID requested is the same as that of the calling process, invalid or the namespace provided is invalid.
 *                      returns -1 and sets errno to ESRCH if we couldn't find a live process with the requested PID
 *                      returns -1 and sets errno to EPERM if the caller is not privileged enough to kill the process with the requested PID
 *                      returns 0 otherwise
 */
int terminate_with_reason(int pid, uint32_t reason_namespace, uint64_t reason_code, const char *reason_string, uint64_t reason_flags);

/*
 * terminate_with_payload: Used to terminate a specific process and pass along
 *                         specific information about why it is being terminated. The payload pointer
 *                         should point to structured data that can be interpreted by the consumer of
 *                         exit reason information.
 *
 * Inputs:              args->pid - the PID of the process to be terminated.
 *                      args->reason_namespace - OS_REASON namespace specified for the reason
 *                      args->reason_code - code in the specified namespace for the reason
 *                      args->payload - pointer to payload structure in user space
 *                      args->payload_size - length of payload buffer (this will be truncated to EXIT_REASON_PAYLOAD_MAX_LEN)
 *                      args->reason_string - additional string formatted information about the request
 *                      args->reason_flags - options requested for how the process should be terminated (see OS_REASON_FLAG_* above)
 *
 * Outputs:             returns -1 and sets errno to EINVAL if the PID requested is the same as that of the calling process, is invalid or the namespace provided is invalid.
 *                      returns -1 and sets errno to ESRCH if we couldn't find a live process with the requested PID
 *                      returns -1 and sets errno to EPERM if the caller is not privileged enough to kill the process with the requested PID
 *                      returns 0 otherwise
 */
int terminate_with_payload(int pid, uint32_t reason_namespace, uint64_t reason_code, void *payload, uint32_t payload_size,
    const char *reason_string, uint64_t reason_flags);
#endif /* KERNEL */

/*
 * codesigning exit reasons
 */
#define CODESIGNING_EXIT_REASON_TASKGATED_INVALID_SIG           1
#define CODESIGNING_EXIT_REASON_INVALID_PAGE                    2
#define CODESIGNING_EXIT_REASON_TASK_ACCESS_PORT                3
#define CODESIGNING_EXIT_REASON_LAUNCH_CONSTRAINT_VIOLATION     4
/*
 * exec path specific exit reasons
 */
#define EXEC_EXIT_REASON_BAD_MACHO          1
#define EXEC_EXIT_REASON_SUGID_FAILURE      2
#define EXEC_EXIT_REASON_ACTV_THREADSTATE   3
#define EXEC_EXIT_REASON_STACK_ALLOC        4
#define EXEC_EXIT_REASON_APPLE_STRING_INIT  5
#define EXEC_EXIT_REASON_COPYOUT_STRINGS    6
#define EXEC_EXIT_REASON_COPYOUT_DYNLINKER  7
#define EXEC_EXIT_REASON_SECURITY_POLICY    8
#define EXEC_EXIT_REASON_TASKGATED_OTHER    9
#define EXEC_EXIT_REASON_FAIRPLAY_DECRYPT   10
#define EXEC_EXIT_REASON_DECRYPT            11
#define EXEC_EXIT_REASON_UPX                12
#define EXEC_EXIT_REASON_NO32EXEC           13
#define EXEC_EXIT_REASON_WRONG_PLATFORM     14
#define EXEC_EXIT_REASON_MAIN_FD_ALLOC      15
#define EXEC_EXIT_REASON_COPYOUT_ROSETTA    16
#define EXEC_EXIT_REASON_SET_DYLD_INFO      17
#define EXEC_EXIT_REASON_MACHINE_THREAD     18
#define EXEC_EXIT_REASON_BAD_PSATTR         19
/*
 * guard reasons
 */
#define GUARD_REASON_VNODE       1
#define GUARD_REASON_VIRT_MEMORY 2
#define GUARD_REASON_MACH_PORT   3
#define GUARD_REASON_EXCLAVES    4
#define GUARD_REASON_JIT         5

__END_DECLS

#endif /* _REASON_H_ */
