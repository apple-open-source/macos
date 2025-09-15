/*
 * Copyright (c) 2023 Apple Computer, Inc. All rights reserved.
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

#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/port.h>
#include <mach/mk_timer.h>
#include <mach/notify.h>

#include <kern/assert.h>
#include <kern/exc_guard.h>
#include <kern/ipc_kobject.h>
#include <kern/kern_types.h>
#include <kern/mach_filter.h>
#include <kern/task.h>
#include <kern/ux_handler.h> /* is_ux_handler_port() */

#include <vm/vm_map_xnu.h> /* current_map() */
#include <vm/vm_protos.h> /* current_proc() */

#include <ipc/ipc_policy.h>
#include <ipc/ipc_service_port.h>
#include <ipc/port.h>

#if CONFIG_CSR
#include <sys/csr.h>
#endif
#include <sys/codesign.h>
#include <sys/proc_ro.h>
#include <sys/reason.h>

#include <libkern/coreanalytics/coreanalytics.h>

extern bool proc_is_simulated(struct proc *);
extern char *proc_name_address(struct proc *p);
extern int  exit_with_guard_exception(
	struct proc            *p,
	mach_exception_data_type_t code,
	mach_exception_data_type_t subcode);

#pragma mark policy tunables

extern const vm_size_t  ipc_kmsg_max_vm_space;

#if IPC_HAS_LEGACY_MACH_MSG_TRAP
#if DEVELOPMENT || DEBUG
static TUNABLE(bool, allow_legacy_mach_msg, "allow_legacy_mach_msg", false);
#endif /* DEVELOPMENT || DEBUG */
#endif /* IPC_HAS_LEGACY_MACH_MSG_TRAP */

/* a boot-arg to enable/disable OOL port array restrictions */
#if XNU_TARGET_OS_XR
TUNABLE(bool, ool_port_array_enforced, "ool_port_array_enforced", false);
#else
TUNABLE(bool, ool_port_array_enforced, "ool_port_array_enforced", true);
#endif /* XNU_TARGET_OS_XR */

/* Note: Consider Developer Mode when changing the default. */
TUNABLE(ipc_control_port_options_t, ipc_control_port_options,
    "ipc_control_port_options",
    ICP_OPTIONS_IMMOVABLE_1P_HARD |
    ICP_OPTIONS_PINNED_1P_HARD |
#if !XNU_TARGET_OS_OSX
    ICP_OPTIONS_IMMOVABLE_3P_HARD |
#endif
    ICP_OPTIONS_PINNED_3P_SOFT);

TUNABLE(bool, service_port_defense_enabled, "-service_port_defense_enabled", true);

/* The bootarg to disable ALL ipc policy violation telemetry */
TUNABLE(bool, ipcpv_telemetry_enabled, "-ipcpv_telemetry_enabled", true);

/* boot-arg for provisional reply port enforcement */
#if XNU_TARGET_OS_OSX || XNU_TARGET_OS_BRIDGE
TUNABLE(bool, prp_enforcement_enabled, "-prp_enforcement_enabled", false);
#else
TUNABLE(bool, prp_enforcement_enabled, "-prp_enforcement_enabled", true);
#endif /* XNU_TARGET_OS_OSX || XNU_TARGET_OS_BRIDGE */

/*
 * bootargs for reply port semantics on bootstrap ports
 */
TUNABLE(bool, bootstrap_port_telemetry_enabled, "-bootstrap_port_telemetry_enabled", true);
TUNABLE(bool, bootstrap_port_enforcement_enabled, "-bootstrap_port_enforcement_enabled", true);

/* Enables reply port/voucher/persona debugging code */
TUNABLE(bool, enforce_strict_reply, "-enforce_strict_reply", false);

#pragma mark policy options

ipc_space_policy_t
ipc_policy_for_task(task_t task)
{
#if XNU_TARGET_OS_OSX
	struct proc *proc = get_bsdtask_info(task);
#endif /* XNU_TARGET_OS_OSX */
	ipc_space_policy_t policy = IPC_SPACE_POLICY_DEFAULT;
	uint32_t ro_flags;

	if (task == kernel_task) {
		return policy | IPC_SPACE_POLICY_KERNEL;
	}

	ro_flags = task_ro_flags_get(task);
	if (ro_flags & TFRO_PLATFORM) {
		policy |= IPC_SPACE_POLICY_PLATFORM;
		policy |= IPC_POLICY_ENHANCED_V2;
	}

	if (task_get_platform_restrictions_version(task) >= 2) {
		policy |= IPC_POLICY_ENHANCED_V2;
	} else if (task_get_platform_restrictions_version(task) == 1) {
		policy |= IPC_POLICY_ENHANCED_V1;
#if XNU_TARGET_OS_OSX
	} else if (proc && csproc_hardened_runtime(proc)) {
		policy |= IPC_POLICY_ENHANCED_V0;
#endif /* XNU_TARGET_OS_OSX */
	}

#if XNU_TARGET_OS_OSX
	if (task_opted_out_mach_hardening(task)) {
		policy |= IPC_SPACE_POLICY_OPTED_OUT;
	}
#endif /* XNU_TARGET_OS_OSX */

	/*
	 * policy modifiers
	 */
#if XNU_TARGET_OS_OSX
	if (proc && proc_is_simulated(proc)) {
		policy |= IPC_SPACE_POLICY_SIMULATED;
	}
#endif
#if CONFIG_ROSETTA
	if (task_is_translated(task)) {
		policy |= IPC_SPACE_POLICY_TRANSLATED;
	}
#endif

	return policy;
}


inline ipc_space_policy_t
ipc_convert_msg_options_to_space(mach_msg_option64_t opts)
{
	return opts >> MACH64_POLICY_SHIFT;
}

mach_msg_option64_t
ipc_current_msg_options(
	task_t                  task,
	mach_msg_option64_t     opts)
{
	uint32_t ro_flags = task_ro_flags_get(task);

	/*
	 * Step 1: convert to kernel flags
	 * - clear any kernel only flags
	 * - convert MACH_SEND_FILTER_NONFATAL which is aliased to the
	 *   MACH_SEND_ALWAYS kernel flag into MACH64_POLICY_FILTER_NON_FATAL.
	 */
	opts &= MACH64_MSG_OPTION_USER;

	if (opts & MACH64_SEND_FILTER_NONFATAL) {
		/*
		 */
		opts &= ~MACH64_SEND_FILTER_NONFATAL;
		opts |= MACH64_POLICY_FILTER_NON_FATAL;
	}
	if (ro_flags & TFRO_FILTER_MSG) {
		opts |= MACH64_POLICY_FILTER_MSG;
	}

	/*
	 * Step 2: derive policy flags from the current context
	 */
	{
		/*
		 * mach_msg_option64_t can't use IPC_SPACE_POLICY_BASE(),
		 * check using this MACH64_POLICY_SHIFT is legitimate.
		 */
#define verify_policy_enum(name) \
	static_assert(IPC_SPACE_POLICY_ ## name == \
	    MACH64_POLICY_ ## name >> MACH64_POLICY_SHIFT)

		verify_policy_enum(DEFAULT);
		verify_policy_enum(ENHANCED);
		verify_policy_enum(PLATFORM);
		verify_policy_enum(KERNEL);
		verify_policy_enum(SIMULATED);
		verify_policy_enum(TRANSLATED);
		verify_policy_enum(OPTED_OUT);
		verify_policy_enum(ENHANCED_V0);
		verify_policy_enum(ENHANCED_V1);
		verify_policy_enum(ENHANCED_V2);
		verify_policy_enum(ENHANCED_VERSION_MASK);
		verify_policy_enum(MASK);

#undef verify_policy_enum
	}

	opts |= (uint64_t)ipc_space_policy(task->itk_space) << MACH64_POLICY_SHIFT;

	return opts;
}

mach_msg_return_t
ipc_preflight_msg_option64(mach_msg_option64_t opts)
{
	bool success = true;

	if ((opts & MACH64_SEND_MSG) && (opts & MACH64_MACH_MSG2)) {
		mach_msg_option64_t cfi = opts & MACH64_MSG_OPTION_CFI_MASK;

#if !XNU_TARGET_OS_OSX
		cfi &= ~MACH64_SEND_ANY;
#endif
		/* mach_msg2() calls must have exactly _one_ of these set */
		if (cfi == 0 || (cfi & (cfi - 1)) != 0) {
			success = false;
		}

		/* vector calls are only supported for message queues */
		if ((opts & (MACH64_SEND_MQ_CALL | MACH64_SEND_ANY)) == 0 &&
		    (opts & MACH64_MSG_VECTOR)) {
			success = false;
		}
	}

	if (success) {
		return MACH_MSG_SUCCESS;
	}

	mach_port_guard_exception(0, opts, kGUARD_EXC_INVALID_OPTIONS);
	if (opts & MACH64_MACH_MSG2) {
		return MACH_SEND_INVALID_OPTIONS;
	}
	return KERN_NOT_SUPPORTED;
}

#pragma mark helpers

bool
ipc_should_apply_policy(
	const ipc_space_policy_t current_policy,
	const ipc_space_policy_t requested_level)
{
	/* Do not apply security policies on these binaries to avoid bincompat regression */
	if ((current_policy & IPC_SPACE_POLICY_SIMULATED) ||
	    (current_policy & IPC_SPACE_POLICY_OPTED_OUT) ||
	    (current_policy & IPC_SPACE_POLICY_TRANSLATED)) {
		return false;
	}

	/* Check versioning for applying platform restrictions policy */
	if (requested_level & current_policy & IPC_SPACE_POLICY_ENHANCED) {
		/* Platform is always opted into platform restrictions */
		if (current_policy & IPC_SPACE_POLICY_PLATFORM) {
			return true;
		}

		const ipc_space_policy_t requested_version = requested_level & IPC_SPACE_POLICY_ENHANCED_VERSION_MASK;
		const ipc_space_policy_t current_es_version = current_policy & IPC_SPACE_POLICY_ENHANCED_VERSION_MASK;
		assert(requested_version != 0);
		return requested_version <= current_es_version;
	}
	return current_policy & requested_level;
}

#pragma mark legacy trap policies
#if IPC_HAS_LEGACY_MACH_MSG_TRAP

CA_EVENT(mach_msg_trap_event,
    CA_INT, msgh_id,
    CA_INT, sw_platform,
    CA_INT, sdk,
    CA_STATIC_STRING(CA_TEAMID_MAX_LEN), team_id,
    CA_STATIC_STRING(CA_SIGNINGID_MAX_LEN), signing_id,
    CA_STATIC_STRING(CA_PROCNAME_LEN), proc_name);

static void
mach_msg_legacy_send_analytics(
	mach_msg_id_t           msgh_id,
	uint32_t                platform,
	uint32_t                sdk)
{
	char *proc_name = proc_name_address(current_proc());
	const char *team_id = csproc_get_teamid(current_proc());
	const char *signing_id = csproc_get_identity(current_proc());

	ca_event_t ca_event = CA_EVENT_ALLOCATE(mach_msg_trap_event);
	CA_EVENT_TYPE(mach_msg_trap_event) * msg_event = ca_event->data;

	msg_event->msgh_id = msgh_id;
	msg_event->sw_platform = platform;
	msg_event->sdk = sdk;

	if (proc_name) {
		strlcpy(msg_event->proc_name, proc_name, CA_PROCNAME_LEN);
	}

	if (team_id) {
		strlcpy(msg_event->team_id, team_id, CA_TEAMID_MAX_LEN);
	}

	if (signing_id) {
		strlcpy(msg_event->signing_id, signing_id, CA_SIGNINGID_MAX_LEN);
	}

	CA_EVENT_SEND(ca_event);
}

static bool
ipc_policy_allow_legacy_mach_msg_trap_for_platform(
	mach_msg_id_t           msgid)
{
	struct proc_ro *pro = current_thread_ro()->tro_proc_ro;
	uint32_t platform = pro->p_platform_data.p_platform;
	uint32_t sdk = pro->p_platform_data.p_sdk;
	uint32_t sdk_major = sdk >> 16;

	/*
	 * Special rules, due to unfortunate bincompat reasons,
	 * allow for a hardcoded list of MIG calls to XNU to go through
	 * for macOS apps linked against an SDK older than 12.x.
	 */
	switch (platform) {
	case PLATFORM_MACOS:
		if (sdk == 0 || sdk_major > 12) {
			return false;
		}
		break;
	default:
		/* disallow for any non-macOS for platform */
		return false;
	}

	switch (msgid) {
	case 0xd4a: /* task_threads */
	case 0xd4d: /* task_info */
	case 0xe13: /* thread_get_state */
	case 0x12c4: /* mach_vm_read */
	case 0x12c8: /* mach_vm_read_overwrite */
		mach_msg_legacy_send_analytics(msgid, platform, sdk);
		return true;
	default:
		return false;
	}
}


mach_msg_return_t
ipc_policy_allow_legacy_send_trap(
	mach_msg_id_t           msgid,
	mach_msg_option64_t     opts)
{
	/* equivalent to ENHANCED_V0 */
	if ((opts & MACH64_POLICY_ENHANCED) == 0) {
#if __x86_64__
		if (current_map()->max_offset <= VM_MAX_ADDRESS) {
			/*
			 * Legacy mach_msg_trap() is the only
			 * available thing for 32-bit tasks
			 */
			return MACH_MSG_SUCCESS;
		}
#endif /* __x86_64__ */
#if CONFIG_ROSETTA
		if (opts & MACH64_POLICY_TRANSLATED) {
			/*
			 * Similarly, on Rosetta, allow mach_msg_trap()
			 * as those apps likely can't be fixed anymore
			 */
			return MACH_MSG_SUCCESS;
		}
#endif
#if DEVELOPMENT || DEBUG
		if (allow_legacy_mach_msg) {
			/* Honor boot-arg */
			return MACH_MSG_SUCCESS;
		}
#endif /* DEVELOPMENT || DEBUG */
		if (ipc_policy_allow_legacy_mach_msg_trap_for_platform(msgid)) {
			return MACH_MSG_SUCCESS;
		}
	}

	mach_port_guard_exception(msgid, opts, kGUARD_EXC_INVALID_OPTIONS);
	/*
	 * this should be MACH_SEND_INVALID_OPTIONS,
	 * but this is a new mach_msg2 error only.
	 */
	return KERN_NOT_SUPPORTED;
}


#endif /* IPC_HAS_LEGACY_MACH_MSG_TRAP */
#pragma mark ipc policy telemetry

/*
 * As CA framework replies on successfully allocating zalloc memory,
 * we maintain a small buffer that gets flushed when full. This helps us avoid taking spinlocks when working with CA.
 */
#define IPC_POLICY_VIOLATIONS_RB_SIZE         2

/*
 * Stripped down version of service port's string name. This is to avoid overwhelming CA's dynamic memory allocation.
 */
#define CA_MACH_SERVICE_PORT_NAME_LEN         86

struct ipc_policy_violations_rb_entry {
	char proc_name[CA_PROCNAME_LEN];
	char service_name[CA_MACH_SERVICE_PORT_NAME_LEN];
	char team_id[CA_TEAMID_MAX_LEN];
	char signing_id[CA_SIGNINGID_MAX_LEN];
	ipc_policy_violation_id_t violation_id;
	int  sw_platform;
	int  aux_data;
	int  sdk;
};
struct ipc_policy_violations_rb_entry ipc_policy_violations_rb[IPC_POLICY_VIOLATIONS_RB_SIZE];
static uint8_t ipc_policy_violations_rb_index = 0;

#if DEBUG || DEVELOPMENT
/* sysctl debug.ipcpv_telemetry_count */
_Atomic unsigned int ipcpv_telemetry_count = 0;
#endif

LCK_GRP_DECLARE(ipc_telemetry_lock_grp, "ipc_telemetry_lock_grp");
LCK_TICKET_DECLARE(ipc_telemetry_lock, &ipc_telemetry_lock_grp);

/*
 * Telemetry: report back the process name violating ipc policy. Note that this event can be used to report
 * any type of ipc violation through a ipc_policy_violation_id_t. It is named reply_port_semantics_violations
 * because we are reusing an existing event.
 */
CA_EVENT(reply_port_semantics_violations,
    CA_STATIC_STRING(CA_PROCNAME_LEN), proc_name,
    CA_STATIC_STRING(CA_MACH_SERVICE_PORT_NAME_LEN), service_name,
    CA_STATIC_STRING(CA_TEAMID_MAX_LEN), team_id,
    CA_STATIC_STRING(CA_SIGNINGID_MAX_LEN), signing_id,
    CA_INT, reply_port_semantics_violation,
    CA_INT, msgh_id); /* for aux_data, keeping the legacy name msgh_id to avoid CA shenanigan */

static void
send_telemetry(
	const struct ipc_policy_violations_rb_entry *entry)
{
	ca_event_t ca_event = CA_EVENT_ALLOCATE_FLAGS(reply_port_semantics_violations, Z_NOWAIT);
	if (ca_event) {
		CA_EVENT_TYPE(reply_port_semantics_violations) * event = ca_event->data;

		strlcpy(event->service_name, entry->service_name, CA_MACH_SERVICE_PORT_NAME_LEN);
		strlcpy(event->proc_name, entry->proc_name, CA_PROCNAME_LEN);
		strlcpy(event->team_id, entry->team_id, CA_TEAMID_MAX_LEN);
		strlcpy(event->signing_id, entry->signing_id, CA_SIGNINGID_MAX_LEN);
		event->reply_port_semantics_violation = entry->violation_id;
		event->msgh_id = entry->aux_data;

		CA_EVENT_SEND(ca_event);
	}
}

/* Routine: flush_ipc_policy_violations_telemetry
 * Conditions:
 *              Assumes ipc_policy_type is valid
 *              Assumes ipc telemetry lock is held.
 *              Unlocks it before returning.
 */
static void
flush_ipc_policy_violations_telemetry(void)
{
	struct ipc_policy_violations_rb_entry local_rb[IPC_POLICY_VIOLATIONS_RB_SIZE];
	uint8_t local_rb_index = 0;

	if (__improbable(ipc_policy_violations_rb_index > IPC_POLICY_VIOLATIONS_RB_SIZE)) {
		panic("Invalid ipc policy violation buffer index %d > %d",
		    ipc_policy_violations_rb_index, IPC_POLICY_VIOLATIONS_RB_SIZE);
	}

	/*
	 * We operate on local copy of telemetry buffer because CA framework relies on successfully
	 * allocating zalloc memory. It can not do that if we are accessing the shared buffer
	 * with spin locks held.
	 */
	while (local_rb_index != ipc_policy_violations_rb_index) {
		local_rb[local_rb_index] = ipc_policy_violations_rb[local_rb_index];
		local_rb_index++;
	}

	lck_ticket_unlock(&ipc_telemetry_lock);

	while (local_rb_index > 0) {
		struct ipc_policy_violations_rb_entry *entry = &local_rb[--local_rb_index];
		send_telemetry(entry);
	}

	/*
	 * Finally call out the buffer as empty. This is also a sort of rate limiting mechanisms for the events.
	 * Events will get dropped until the buffer is not fully flushed.
	 */
	lck_ticket_lock(&ipc_telemetry_lock, &ipc_telemetry_lock_grp);
	ipc_policy_violations_rb_index = 0;
}

void
ipc_stash_policy_violations_telemetry(
	ipc_policy_violation_id_t    violation_id,
	ipc_port_t                   service_port,
	int                          aux_data)
{
	if (!ipcpv_telemetry_enabled) {
		return;
	}

	struct ipc_policy_violations_rb_entry *entry;
	char *service_name = (char *) "unknown";
	task_t task = current_task_early();
	int pid = -1;

#if CONFIG_SERVICE_PORT_INFO
	if (IP_VALID(service_port)) {
		/*
		 * dest_port lock must be held to avoid race condition
		 * when accessing ip_splabel rdar://139066947
		 */
		struct mach_service_port_info sp_info;
		ipc_object_label_t label = ip_mq_lock_label_get(service_port);
		if (io_state_active(label.io_state) && ip_is_any_service_port_type(label.io_type)) {
			ipc_service_port_label_get_info(label.iol_service, &sp_info);
			service_name = sp_info.mspi_string_name;
		}
		ip_mq_unlock_label_put(service_port, &label);
	}
#endif /* CONFIG_SERVICE_PORT_INFO */

	if (task) {
		pid = task_pid(task);
	}

	if (task) {
		struct proc_ro *pro = current_thread_ro()->tro_proc_ro;
		uint32_t platform = pro->p_platform_data.p_platform;
		uint32_t sdk = pro->p_platform_data.p_sdk;
		char *proc_name = (char *) "unknown";
#ifdef MACH_BSD
		proc_name = proc_name_address(get_bsdtask_info(task));
#endif /* MACH_BSD */
		const char *team_id = csproc_get_identity(current_proc());
		const char *signing_id = csproc_get_teamid(current_proc());

		lck_ticket_lock(&ipc_telemetry_lock, &ipc_telemetry_lock_grp);

		if (ipc_policy_violations_rb_index >= IPC_POLICY_VIOLATIONS_RB_SIZE) {
			/* Dropping the event since buffer is full. */
			lck_ticket_unlock(&ipc_telemetry_lock);
			return;
		}
		entry = &ipc_policy_violations_rb[ipc_policy_violations_rb_index++];
		strlcpy(entry->proc_name, proc_name, CA_PROCNAME_LEN);

		strlcpy(entry->service_name, service_name, CA_MACH_SERVICE_PORT_NAME_LEN);
		entry->violation_id = violation_id;

		if (team_id) {
			strlcpy(entry->team_id, team_id, CA_TEAMID_MAX_LEN);
		}

		if (signing_id) {
			strlcpy(entry->signing_id, signing_id, CA_SIGNINGID_MAX_LEN);
		}
		entry->aux_data = aux_data;
		entry->sw_platform = platform;
		entry->sdk = sdk;
	}

	if (ipc_policy_violations_rb_index == IPC_POLICY_VIOLATIONS_RB_SIZE) {
		flush_ipc_policy_violations_telemetry();
	}

	lck_ticket_unlock(&ipc_telemetry_lock);
}

#if DEBUG || DEVELOPMENT
void
ipc_inc_telemetry_count(void)
{
	unsigned int count = os_atomic_load(&ipcpv_telemetry_count, relaxed);
	if (!os_add_overflow(count, 1, &count)) {
		os_atomic_store(&ipcpv_telemetry_count, count, relaxed);
	}
}
#endif /* DEBUG || DEVELOPMENT */

/*!
 * @brief
 * Checks that this message conforms to reply port policies, which are:
 * 1. IOT_REPLY_PORT's must be make-send-once disposition
 * 2. You must use an IOT_REPLY_PORT (or weak variant) if the dest_port requires it
 *
 * @param reply_port    the message local/reply port
 * @param dest_port     the message remote/dest port
 *
 * @returns
 * - true  if there is a violation in the security policy for this mach msg
 * - false otherwise
 */
static mach_msg_return_t
ipc_validate_local_port(
	mach_port_t         reply_port,
	mach_port_t         dest_port,
	mach_msg_option64_t opts)
{
	assert(IP_VALID(dest_port));
	/* An empty reply port, or an inactive reply port / dest port violates nothing */
	if (!IP_VALID(reply_port) || !ip_active(reply_port) || !ip_active(dest_port)) {
		return MACH_MSG_SUCCESS;
	}

	if (ip_is_reply_port(reply_port)) {
		return MACH_MSG_SUCCESS;
	}

	ipc_space_policy_t pol = ipc_convert_msg_options_to_space(opts);
	/* skip translated and simulated process */
	if (!ipc_should_apply_policy((pol), IPC_SPACE_POLICY_DEFAULT)) {
		return MACH_MSG_SUCCESS;
	}

	/* kobject enforcement */
	if (ip_is_kobject(dest_port) &&
	    ipc_should_apply_policy(pol, IPC_POLICY_ENHANCED_V1)) {
		mach_port_guard_exception(ip_get_receiver_name(dest_port), 0, kGUARD_EXC_KOBJECT_REPLY_PORT_SEMANTICS);
		return MACH_SEND_INVALID_REPLY;
	}

	if (!ipc_policy(dest_port)->pol_enforce_reply_semantics || ip_is_provisional_reply_port(reply_port)) {
		return MACH_MSG_SUCCESS;
	}

	/* bootstrap port defense */
	if (ip_is_bootstrap_port(dest_port) && ipc_should_apply_policy(pol, IPC_POLICY_ENHANCED_V2)) {
		if (bootstrap_port_telemetry_enabled &&
		    !ipc_space_has_telemetry_type(current_space(), IS_HAS_BOOTSTRAP_PORT_TELEMETRY)) {
			ipc_stash_policy_violations_telemetry(IPCPV_BOOTSTRAP_PORT, dest_port, 0);
		}
		if (bootstrap_port_enforcement_enabled) {
			mach_port_guard_exception(ip_get_receiver_name(dest_port), 1, kGUARD_EXC_REQUIRE_REPLY_PORT_SEMANTICS);
			return MACH_SEND_INVALID_REPLY;
		}
	}

	/* regular enforcement */
	if (!ip_is_bootstrap_port(dest_port)) {
		if (ip_type(dest_port) == IOT_SERVICE_PORT) {
			ipc_stash_policy_violations_telemetry(IPCPV_REPLY_PORT_SEMANTICS_OPTOUT, dest_port, 0);
		}
		mach_port_guard_exception(ip_get_receiver_name(dest_port), 0, kGUARD_EXC_REQUIRE_REPLY_PORT_SEMANTICS);
		return MACH_SEND_INVALID_REPLY;
	}

	return MACH_MSG_SUCCESS;
}

#pragma mark MACH_SEND_MSG policies

mach_msg_return_t
ipc_validate_kmsg_header_schema_from_user(
	mach_msg_user_header_t *hdr __unused,
	mach_msg_size_t         dsc_count,
	mach_msg_option64_t     opts)
{
	if (opts & MACH64_SEND_KOBJECT_CALL) {
		if (dsc_count > IPC_KOBJECT_DESC_MAX) {
			return MACH_SEND_TOO_LARGE;
		}
	}

	return MACH_MSG_SUCCESS;
}

mach_msg_return_t
ipc_validate_kmsg_schema_from_user(
	mach_msg_header_t      *kdata,
	mach_msg_send_uctx_t   *send_uctx,
	mach_msg_option64_t     opts __unused)
{
	mach_msg_kbase_t *kbase = NULL;
	vm_size_t vm_size;

	if (kdata->msgh_bits & MACH_MSGH_BITS_COMPLEX) {
		kbase = mach_msg_header_to_kbase(kdata);
	}

	if (send_uctx->send_dsc_port_count > IPC_KMSG_MAX_OOL_PORT_COUNT) {
		return MACH_SEND_TOO_LARGE;
	}

	if (os_add_overflow(send_uctx->send_dsc_vm_size,
	    send_uctx->send_dsc_port_count * sizeof(mach_port_t), &vm_size)) {
		return MACH_SEND_TOO_LARGE;
	}
	if (vm_size > ipc_kmsg_max_vm_space) {
		return MACH_MSG_VM_KERNEL;
	}

	return MACH_MSG_SUCCESS;
}

static mach_msg_return_t
ipc_filter_kmsg_header_from_user(
	mach_msg_header_t      *hdr,
	mach_port_t             dport,
	mach_msg_option64_t     opts)
{
	static const uint32_t MACH_BOOTSTRAP_PORT_MSG_ID_MASK = ((1u << 24) - 1);

	mach_msg_filter_id fid = 0;
	ipc_object_label_t dlabel;
	mach_msg_id_t msg_id = hdr->msgh_id;
	struct ipc_conn_port_label *sblabel = NULL;

	dlabel = ip_mq_lock_label_get(dport);

	if (io_state_active(dlabel.io_state) && dlabel.io_filtered) {
		switch (dlabel.io_type) {
		case IOT_SERVICE_PORT:
		case IOT_WEAK_SERVICE_PORT:
			/*
			 * Mask the top byte for messages sent to launchd's bootstrap port.
			 * Filter any messages with domain 0 (as they correspond to MIG
			 * based messages)
			 */
			if (dlabel.iol_service->ispl_bootstrap_port) {
				if ((msg_id & ~MACH_BOOTSTRAP_PORT_MSG_ID_MASK) == 0) {
					ip_mq_unlock_label_put(dport, &dlabel);
					goto filtered_msg;
				}
				msg_id = msg_id & MACH_BOOTSTRAP_PORT_MSG_ID_MASK;
			}

			sblabel = dlabel.iol_service->ispl_sblabel;
			break;

		case IOT_CONNECTION_PORT:
			/* Connection ports can also have send-side message filters */
			sblabel = dlabel.iol_connection;
			break;

		default:
			break;
		}
	}
	if (sblabel) {
		mach_msg_filter_retain_sblabel_callback(sblabel);
	}

	ip_mq_unlock_label_put(dport, &dlabel);

	if (sblabel && !mach_msg_fetch_filter_policy(sblabel, msg_id, &fid)) {
		goto filtered_msg;
	}
	return MACH_MSG_SUCCESS;

filtered_msg:
	if ((opts & MACH64_POLICY_FILTER_NON_FATAL) == 0) {
		mach_port_name_t dest_name = CAST_MACH_PORT_TO_NAME(hdr->msgh_remote_port);

		mach_port_guard_exception(dest_name, hdr->msgh_id,
		    kGUARD_EXC_MSG_FILTERED);
	}
	return MACH_SEND_MSG_FILTERED;
}

static bool
ipc_policy_allow_send_only_kobject_calls(void)
{
	struct proc_ro *pro = current_thread_ro()->tro_proc_ro;
	uint32_t sdk = pro->p_platform_data.p_sdk;
	uint32_t sdk_major = sdk >> 16;

	switch (pro->p_platform_data.p_platform) {
	case PLATFORM_IOS:
	case PLATFORM_MACCATALYST:
	case PLATFORM_TVOS:
		if (sdk == 0 || sdk_major > 17) {
			return false;
		}
		return true;
	case PLATFORM_MACOS:
		if (sdk == 0 || sdk_major > 14) {
			return false;
		}
		return true;
	case PLATFORM_WATCHOS:
		if (sdk == 0 || sdk_major > 10) {
			return false;
		}
		return true;
	default:
		return false;
	}
}

static mach_msg_return_t
ipc_validate_kmsg_dest_from_user(
	mach_msg_header_t      *hdr,
	ipc_port_t              port,
	mach_msg_option64_t     opts)
{
	/*
	 * This is a _user_ message via mach_msg2_trap()。
	 *
	 * To curb kobject port/message queue confusion and improve control flow
	 * integrity, mach_msg2_trap() invocations mandate the use of either
	 * MACH64_SEND_KOBJECT_CALL or MACH64_SEND_MQ_CALL and that the flag
	 * matches the underlying port type. (unless the call is from a simulator,
	 * since old simulators keep using mach_msg() in all cases indiscriminatingly.)
	 *
	 * Since:
	 *     (1) We make sure to always pass either MACH64_SEND_MQ_CALL or
	 *         MACH64_SEND_KOBJECT_CALL bit at all sites outside simulators
	 *         (checked by mach_msg2_trap());
	 *     (2) We checked in mach_msg2_trap() that _exactly_ one of the three bits is set.
	 *
	 * CFI check cannot be bypassed by simply setting MACH64_SEND_ANY.
	 */
#if XNU_TARGET_OS_OSX
	if (opts & MACH64_SEND_ANY) {
		return MACH_MSG_SUCCESS;
	}
#endif /* XNU_TARGET_OS_OSX */

	natural_t otype = ip_type(port);
	if (otype == IOT_TIMER_PORT) {
#if XNU_TARGET_OS_OSX
		if (__improbable(opts & MACH64_POLICY_ENHANCED)) {
			return MACH_SEND_INVALID_OPTIONS;
		}
		/*
		 * For bincompat, let's still allow user messages to timer port, but
		 * force MACH64_SEND_MQ_CALL flag for memory segregation.
		 */
		if (__improbable(!(opts & MACH64_SEND_MQ_CALL))) {
			return MACH_SEND_INVALID_OPTIONS;
		}
#else
		return MACH_SEND_INVALID_OPTIONS;
#endif
	} else if (io_is_kobject_type(otype)) {
		if (otype == IKOT_UEXT_OBJECT) {
			if (__improbable(!(opts & MACH64_SEND_DK_CALL))) {
				return MACH_SEND_INVALID_OPTIONS;
			}
		} else {
			/* Otherwise, caller must set MACH64_SEND_KOBJECT_CALL. */
			if (__improbable(!(opts & MACH64_SEND_KOBJECT_CALL))) {
				return MACH_SEND_INVALID_OPTIONS;
			}

			/* kobject calls must be a combined send/receive */
			if (__improbable((opts & MACH64_RCV_MSG) == 0)) {
				if ((opts & MACH64_POLICY_ENHANCED) ||
				    IP_VALID(hdr->msgh_local_port) ||
				    !ipc_policy_allow_send_only_kobject_calls()) {
					return MACH_SEND_INVALID_OPTIONS;
				}
			}
		}
#if CONFIG_CSR
	} else if (csr_check(CSR_ALLOW_KERNEL_DEBUGGER) == 0) {
		/*
		 * Allow MACH64_SEND_KOBJECT_CALL flag to message queues
		 * when SIP is off (for Mach-on-Mach emulation).
		 */
#endif /* CONFIG_CSR */
	} else {
		/* If destination is a message queue, caller must set MACH64_SEND_MQ_CALL */
		if (__improbable(!(opts & MACH64_SEND_MQ_CALL))) {
			return MACH_SEND_INVALID_OPTIONS;
		}
	}

	return MACH_MSG_SUCCESS;
}

mach_msg_return_t
ipc_validate_kmsg_header_from_user(
	mach_msg_header_t      *hdr,
	mach_msg_send_uctx_t   *send_uctx,
	mach_msg_option64_t     opts)
{
	ipc_port_t dest_port = hdr->msgh_remote_port;
	ipc_port_t reply_port = hdr->msgh_local_port;
	mach_msg_return_t mr = MACH_MSG_SUCCESS;
	ipc_space_policy_t current_policy;

	if (opts & MACH64_MACH_MSG2) {
		mr = ipc_validate_kmsg_dest_from_user(hdr, dest_port, opts);
		if (mr != MACH_MSG_SUCCESS) {
			goto out;
		}
	}

	/*
	 * For enhanced v2 binaries, enforce two OOL port array restrictions:
	 *     - the receive right has to be of a type that explicitly
	 *       allows receiving that descriptor
	 *     - there could be no more than ONE single array in a kmsg
	 */
	current_policy = ipc_convert_msg_options_to_space(opts);
	if (ool_port_array_enforced &&
	    send_uctx->send_dsc_port_arrays_count &&
	    ipc_should_apply_policy(current_policy, IPC_POLICY_ENHANCED_V2)) {
		if (!ip_is_port_array_allowed(dest_port)) {
			mach_port_guard_exception(current_policy,
			    MPG_PAYLOAD(MPG_FLAGS_INVALID_OPTIONS_OOL_RIGHT,
			    ip_type(dest_port)),
			    kGUARD_EXC_DESCRIPTOR_VIOLATION);

			return MACH_SEND_INVALID_OPTIONS;
		}

		if (send_uctx->send_dsc_port_arrays_count > 1) {
			mach_port_guard_exception(current_policy,
			    MPG_PAYLOAD(MPG_FLAGS_INVALID_OPTIONS_OOL_ARRAYS,
			    send_uctx->send_dsc_port_arrays_count),
			    kGUARD_EXC_DESCRIPTOR_VIOLATION);

			return MACH_SEND_INVALID_OPTIONS;
		}
	}

	/*
	 * Ensure that the reply field follows our security policies,
	 * including IOT_REPLY_PORT requirements
	 */
	mr = ipc_validate_local_port(reply_port, dest_port, opts);
	if (mr != MACH_MSG_SUCCESS) {
		goto out;
	}

	/*
	 * Evaluate message filtering if the sender is filtered.
	 */
	if ((opts & MACH64_POLICY_FILTER_MSG) &&
	    mach_msg_filter_at_least(MACH_MSG_FILTER_CALLBACKS_VERSION_1) &&
	    ip_to_object(dest_port)->io_filtered) {
		mr = ipc_filter_kmsg_header_from_user(hdr, dest_port, opts);
		if (mr != MACH_MSG_SUCCESS) {
			goto out;
		}
	}

out:
	if (mr == MACH_SEND_INVALID_OPTIONS) {
		mach_port_guard_exception(0, opts, kGUARD_EXC_INVALID_OPTIONS);
	}
	return mr;
}

#pragma mark receive immovability

bool
ipc_move_receive_allowed(
	ipc_space_t             space,
	ipc_port_t              port,
	mach_port_name_t        name)
{
	ipc_space_policy_t policy = ipc_space_policy(space);
	/*
	 * Check for service port before immovability so the task crash
	 * with reason kGUARD_EXC_SERVICE_PORT_VIOLATION_FATAL
	 */
	if (service_port_defense_enabled &&
	    ip_type(port) == IOT_SERVICE_PORT &&
	    !task_is_initproc(space->is_task)) {
		mach_port_guard_exception(IPCPV_MOVE_SERVICE_PORT, name,
		    kGUARD_EXC_SERVICE_PORT_VIOLATION_FATAL);
		return false;
	}

	if (ip_type(port) == IOT_PROVISIONAL_REPLY_PORT &&
	    ipc_should_apply_policy(policy, IPC_POLICY_ENHANCED_V2) &&
	    !ipc_space_has_telemetry_type(space, IS_HAS_MOVE_PRP_TELEMETRY)) {
		mach_port_guard_exception(name, 0, kGUARD_EXC_MOVE_PROVISIONAL_REPLY_PORT);
	}

	if (ip_is_immovable_receive(port)) {
		mach_port_guard_exception(name, 0, kGUARD_EXC_IMMOVABLE);
		return false;
	}

	return true;
}

#pragma mark send immovability


bool
ipc_should_mark_immovable_send(
	task_t task,
	ipc_port_t port,
	ipc_object_label_t label)
{
	/*
	 * some entitled processes are allowed to get movable control ports
	 * see `task_set_ctrl_port_default` - also all control ports are movable
	 * before/after the space becomes inactive. They will be made movable before
	 * the `task` is able to run code in userspace in `task_wait_to_return`
	 */
	if ((!task_is_immovable(task) ||
	    !is_active(task->itk_space)) &&
	    ip_is_tt_control_port_type(label.io_type)) {
		return false;
	}

	/* tasks get their own thread control port as immovable */
	if (label.io_type == IKOT_THREAD_CONTROL) {
		thread_t thread = ipc_kobject_get_raw(port, IKOT_THREAD_CONTROL);
		if (thread != THREAD_NULL && task == get_threadtask(thread)) {
			return true;
		}
	}

	/* tasks get their own task control port as immovable */
	if (task->itk_task_ports[TASK_FLAVOR_CONTROL] == port) {
		return true;
	}

	/* special cases are handled, check the default policy */
	if (!ipc_policy(label)->pol_movable_send) {
		return true;
	}

	return false;
}

/* requires: nothing locked, port is valid */
static bool
ip_is_currently_immovable_send(ipc_port_t port)
{
	ipc_object_label_t label = ipc_port_lock_label_get(port);
	if (task_is_immovable(current_task()) &&
	    (ip_is_tt_control_port_type(label.io_type))) {
		/* most tasks cannot move their control ports */
		ip_mq_unlock_label_put(port, &label);
		return true;
	}

	bool is_always_immovable_send = !ipc_policy(label)->pol_movable_send;
	ip_mq_unlock_label_put(port, &label);
	return is_always_immovable_send;
}

bool
ipc_can_stash_naked_send(ipc_port_t port)
{
	return !IP_VALID(port) || !ip_is_currently_immovable_send(port);
}

#pragma mark entry init

void
ipc_entry_init(
	ipc_space_t         space,
	ipc_object_t        object,
	mach_port_type_t    type,
	ipc_entry_t         entry,
	mach_port_urefs_t   urefs,
	mach_port_name_t    name)
{
	/* object type can be deadname, port, or a portset */
	assert((type & MACH_PORT_TYPE_ALL_RIGHTS) == type);
	assert(type != MACH_PORT_TYPE_NONE);
	assert(urefs <= MACH_PORT_UREFS_MAX);
	assert(entry);

	if (object && (type & MACH_PORT_TYPE_SEND_RIGHTS)) {
		ipc_port_t port = ip_object_to_port(object);
		ipc_object_label_t label = ip_label_get(port);

		if (ipc_should_mark_immovable_send(space->is_task, port, label)) {
			entry->ie_bits |= IE_BITS_IMMOVABLE_SEND;
		}
		io_label_set_and_put(&port->ip_object, &label);
	}
	entry->ie_object = object;
	entry->ie_bits |= type | urefs;
	ipc_entry_modified(space, name, entry);
}

#pragma mark policy guard violations

void
mach_port_guard_exception(uint32_t target, uint64_t payload, unsigned reason)
{
	mach_exception_code_t code = 0;
	EXC_GUARD_ENCODE_TYPE(code, GUARD_TYPE_MACH_PORT);
	EXC_GUARD_ENCODE_FLAVOR(code, reason);
	EXC_GUARD_ENCODE_TARGET(code, target);
	mach_exception_subcode_t subcode = (uint64_t)payload;
	thread_t t = current_thread();
	bool fatal = FALSE;

	if (reason <= MAX_OPTIONAL_kGUARD_EXC_CODE &&
	    (get_threadtask(t)->task_exc_guard & TASK_EXC_GUARD_MP_FATAL)) {
		fatal = true;
	} else if (reason <= MAX_FATAL_kGUARD_EXC_CODE) {
		fatal = true;
	}
	thread_guard_violation(t, code, subcode, fatal);
}

void
mach_port_guard_exception_immovable(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_t             port,
	mach_msg_type_name_t    disp,
	__assert_only ipc_entry_t entry)
{
	if (space == current_space()) {
		assert(entry->ie_bits & IE_BITS_IMMOVABLE_SEND);
		assert(entry->ie_port == port);

		boolean_t hard = task_get_control_port_options(current_task()) & TASK_CONTROL_PORT_IMMOVABLE_HARD;
		uint64_t payload = MPG_PAYLOAD(MPG_FLAGS_NONE, ip_type(port), disp);

		if (ip_is_tt_control_port(port)) {
			assert(task_is_immovable(current_task()));
			mach_port_guard_exception(name, payload,
			    hard ? kGUARD_EXC_IMMOVABLE : kGUARD_EXC_IMMOVABLE_NON_FATAL);
		} else {
			/* always fatal exception for non-control port violation */
			mach_port_guard_exception(name, payload, kGUARD_EXC_IMMOVABLE);
		}
	}
}

void
mach_port_guard_exception_pinned(
	ipc_space_t             space,
	mach_port_name_t        name,
	uint64_t                payload)
{
	ipc_space_policy_t policy = ipc_space_policy(space);
	int guard;

	if (space != current_space()) {
		guard = kGUARD_EXC_NONE;
	} else if (policy &
	    (IPC_SPACE_POLICY_TRANSLATED | IPC_SPACE_POLICY_SIMULATED)) {
		guard = kGUARD_EXC_NONE;
	} else if (ipc_should_apply_policy(policy, IPC_POLICY_ENHANCED_V1)) {
		if (ipc_control_port_options & ICP_OPTIONS_PINNED_1P_HARD) {
			guard = kGUARD_EXC_MOD_REFS;
		} else if (ipc_control_port_options & ICP_OPTIONS_PINNED_1P_SOFT) {
			guard = kGUARD_EXC_MOD_REFS_NON_FATAL;
		} else {
			guard = kGUARD_EXC_NONE;
		}
	} else {
		if (ipc_control_port_options & ICP_OPTIONS_PINNED_3P_HARD) {
			guard = kGUARD_EXC_MOD_REFS;
		} else if (ipc_control_port_options & ICP_OPTIONS_PINNED_3P_SOFT) {
			guard = kGUARD_EXC_MOD_REFS_NON_FATAL;
		} else {
			guard = kGUARD_EXC_NONE;
		}
	}

	if (guard != kGUARD_EXC_NONE) {
		mach_port_guard_exception(name, payload, guard);
	}
}

/*
 *	Routine:	mach_port_guard_ast
 *	Purpose:
 *		Raises an exception for mach port guard violation.
 *	Conditions:
 *		None.
 *	Returns:
 *		None.
 */

void
mach_port_guard_ast(
	thread_t                t,
	mach_exception_data_type_t code,
	mach_exception_data_type_t subcode)
{
	unsigned int reason = EXC_GUARD_DECODE_GUARD_FLAVOR(code);
	task_t task = get_threadtask(t);
	unsigned int behavior = task->task_exc_guard;
	bool fatal = true;

	assert(task == current_task());
	assert(task != kernel_task);

	if (reason <= MAX_FATAL_kGUARD_EXC_CODE) {
		/*
		 * Fatal Mach port guards - always delivered synchronously if dev mode is on.
		 * Check if anyone has registered for Synchronous EXC_GUARD, if yes then,
		 * deliver it synchronously and then kill the process, else kill the process
		 * and deliver the exception via EXC_CORPSE_NOTIFY.
		 */

		int flags = PX_DEBUG_NO_HONOR;
		exception_info_t info = {
			.os_reason = OS_REASON_GUARD,
			.exception_type = EXC_GUARD,
			.mx_code = code,
			.mx_subcode = subcode,
		};

		if (task_exception_notify(EXC_GUARD, code, subcode, fatal) == KERN_SUCCESS) {
			flags |= PX_PSIGNAL;
		}
		exit_with_mach_exception(get_bsdtask_info(task), info, flags);
	} else {
		/*
		 * Mach port guards controlled by task settings.
		 */

		/* Is delivery enabled */
		if ((behavior & TASK_EXC_GUARD_MP_DELIVER) == 0) {
			return;
		}

		/* If only once, make sure we're that once */
		while (behavior & TASK_EXC_GUARD_MP_ONCE) {
			uint32_t new_behavior = behavior & ~TASK_EXC_GUARD_MP_DELIVER;

			if (os_atomic_cmpxchg(&task->task_exc_guard,
			    behavior, new_behavior, relaxed)) {
				break;
			}
			behavior = task->task_exc_guard;
			if ((behavior & TASK_EXC_GUARD_MP_DELIVER) == 0) {
				return;
			}
		}
		fatal = (task->task_exc_guard & TASK_EXC_GUARD_MP_FATAL)
		    && (reason <= MAX_OPTIONAL_kGUARD_EXC_CODE);
		kern_return_t sync_exception_result;
		sync_exception_result = task_exception_notify(EXC_GUARD, code, subcode, fatal);

		if (task->task_exc_guard & TASK_EXC_GUARD_MP_FATAL) {
			if (reason > MAX_OPTIONAL_kGUARD_EXC_CODE) {
				/* generate a simulated crash if not handled synchronously */
				if (sync_exception_result != KERN_SUCCESS) {
					task_violated_guard(code, subcode, NULL, TRUE);
				}
			} else {
				/*
				 * Only generate crash report if synchronous EXC_GUARD wasn't handled,
				 * but it has to die regardless.
				 */

				int flags = PX_DEBUG_NO_HONOR;
				exception_info_t info = {
					.os_reason = OS_REASON_GUARD,
					.exception_type = EXC_GUARD,
					.mx_code = code,
					.mx_subcode = subcode
				};

				if (sync_exception_result == KERN_SUCCESS) {
					flags |= PX_PSIGNAL;
				}

				exit_with_mach_exception(get_bsdtask_info(task), info, flags);
			}
		} else if (task->task_exc_guard & TASK_EXC_GUARD_MP_CORPSE) {
			/* Raise exception via corpse fork if not handled synchronously */
			if (sync_exception_result != KERN_SUCCESS) {
				task_violated_guard(code, subcode, NULL, TRUE);
			}
		}
	}
}

#pragma mark notification policies

static bool
ipc_allow_service_port_register_pd(
	ipc_port_t              service_port,
	ipc_port_t              notify_port,
	uint64_t                *payload)
{
	/* boot-arg disables this security policy */
	if (!service_port_defense_enabled || !IP_VALID(notify_port)) {
		return true;
	}
	/* enforce this policy only on service port types */
	if (ip_is_any_service_port(service_port)) {
		/* Only launchd should be able to register for port destroyed notification on a service port. */
		if (!task_is_initproc(current_task())) {
			*payload = MPG_FLAGS_KERN_FAILURE_TASK;
			return false;
		}
		/* notify_port needs to be immovable */
		if (!ip_is_immovable_receive(notify_port)) {
			*payload = MPG_FLAGS_KERN_FAILURE_NOTIFY_TYPE;
			return false;
		}
		/* notify_port should be owned by launchd */
		if (!task_is_initproc(notify_port->ip_receiver->is_task)) {
			*payload = MPG_FLAGS_KERN_FAILURE_NOTIFY_RECV;
			return false;
		}
	}
	return true;
}

kern_return_t
ipc_allow_register_pd_notification(
	ipc_port_t              pd_port,
	ipc_port_t              notify_port)
{
	uint64_t payload;

	/*
	 * you cannot register for port destroyed notifications
	 * on an immovable receive right (which includes kobjects),
	 * or a (special) reply port or any other port that explicitly disallows them.
	 */
	release_assert(ip_in_a_space(pd_port));
	if (ip_is_immovable_receive(pd_port) ||
	    !ipc_policy(pd_port)->pol_notif_port_destroy) {
		mach_port_guard_exception(ip_type(pd_port), MACH_NOTIFY_PORT_DESTROYED, kGUARD_EXC_INVALID_NOTIFICATION_REQ);
		return KERN_INVALID_RIGHT;
	}

	/* Stronger pd enforcement for service ports */
	if (!ipc_allow_service_port_register_pd(pd_port, notify_port, &payload)) {
		mach_port_guard_exception(0, payload, kGUARD_EXC_KERN_FAILURE);
		return KERN_INVALID_RIGHT;
	}

	/* Allow only one registration of this notification */
	if (ipc_port_has_prdrequest(pd_port)) {
		mach_port_guard_exception(0, MPG_FLAGS_KERN_FAILURE_MULTI_NOTI, kGUARD_EXC_KERN_FAILURE);
		return KERN_FAILURE;
	}

	return KERN_SUCCESS;
}


#pragma mark policy array

__dead2
static void
no_kobject_no_senders(
	ipc_port_t              port,
	mach_port_mscount_t     mscount __unused)
{
	panic("unexpected call to no_senders for object %p, type %d",
	    port, ip_type(port));
}

__dead2
static void
no_label_free(ipc_object_label_t label)
{
	panic("unexpected call to label_free for object type %d, label %p",
	    label.io_type, label.iol_pointer);
}

/*
 * Denotes a policy which safe value is the argument to PENDING(),
 * but is currently not default and pending validation/prep work.
 */
#define PENDING(value)          value

__security_const_late
struct ipc_object_policy ipc_policy_array[IOT_UNKNOWN] = {
	[IOT_PORT_SET] = {
		.pol_name               = "port set",
		.pol_movability         = IPC_MOVE_POLICY_NEVER,
		.pol_movable_send       = false,
	},
	[IOT_PORT] = {
		.pol_name               = "port",
		.pol_movability         = IPC_MOVE_POLICY_ALWAYS,
		.pol_movable_send       = true,
		.pol_notif_dead_name    = true,
		.pol_notif_no_senders   = true,
		.pol_notif_port_destroy = true,
	},
	[IOT_SERVICE_PORT] = {
		.pol_name               = "service port",
		.pol_movability         = PENDING(IPC_MOVE_POLICY_ONCE_OR_AFTER_PD),
		.pol_movable_send       = true,
		.pol_label_free         = ipc_service_port_label_dealloc,
		.pol_enforce_reply_semantics = PENDING(true), /* pending on service port defense cleanup */
		.pol_notif_dead_name    = true,
		.pol_notif_no_senders   = true,
		.pol_notif_port_destroy = true,
	},
	[IOT_WEAK_SERVICE_PORT] = {
		.pol_name               = "weak service port",
		.pol_movability         = IPC_MOVE_POLICY_ALWAYS,
		.pol_movable_send       = true,
		.pol_label_free         = ipc_service_port_label_dealloc,
		.pol_notif_dead_name    = true,
		.pol_notif_no_senders   = true,
		.pol_notif_port_destroy = true,
	},
	[IOT_CONNECTION_PORT] = {
		.pol_name               = "connection port",
		.pol_movability         = IPC_MOVE_POLICY_ONCE,
		.pol_label_free         = ipc_connection_port_label_dealloc,
		.pol_enforce_reply_semantics = true,
		.pol_notif_dead_name    = true,
		.pol_notif_no_senders   = true,
		.pol_notif_port_destroy = true,
	},
	[IOT_CONNECTION_PORT_WITH_PORT_ARRAY] = {
		.pol_name               = "conn port with ool port array",
		.pol_movability         = IPC_MOVE_POLICY_NEVER,
		.pol_movable_send       = true,
		.pol_construct_entitlement = MACH_PORT_CONNECTION_PORT_WITH_PORT_ARRAY,
		.pol_notif_dead_name    = true,
		.pol_notif_no_senders   = true,
	},
	[IOT_EXCEPTION_PORT] = {
		.pol_name               = "exception port",
		.pol_movability         = IPC_MOVE_POLICY_NEVER,
		.pol_movable_send       = true,
		.pol_notif_dead_name    = true,
		.pol_notif_no_senders   = true,
	},
	[IOT_TIMER_PORT] = {
		.pol_name               = "timer port",
		.pol_movability         = IPC_MOVE_POLICY_NEVER,
		.pol_movable_send       = true,
		.pol_label_free         = mk_timer_port_label_dealloc,
		.pol_notif_dead_name    = true,
		.pol_notif_no_senders   = true,
	},
	[IOT_REPLY_PORT] = {
		.pol_name               = "reply port",
		.pol_movability         = IPC_MOVE_POLICY_NEVER,
		.pol_notif_dead_name    = true,
	},
	[IOT_SPECIAL_REPLY_PORT] = {
		.pol_name               = "special reply port",
		/*
		 * General use of a special reply port as a receive right
		 * can cause type confusion in the importance code.
		 */
		.pol_movability         = IPC_MOVE_POLICY_NEVER,
		.pol_notif_dead_name    = true,
	},
	[IOT_PROVISIONAL_REPLY_PORT] = {
		.pol_name               = "provisional reply port",
		.pol_movability         = IPC_MOVE_POLICY_ALWAYS,
		.pol_movable_send       = true,
		.pol_construct_entitlement = MACH_PORT_PROVISIONAL_REPLY_ENTITLEMENT,
		.pol_notif_dead_name    = true,
		.pol_notif_no_senders   = true,
		.pol_notif_port_destroy = true,
	},

	[__IKOT_FIRST ... IOT_UNKNOWN - 1] = {
		.pol_movability         = IPC_MOVE_POLICY_NEVER,
		.pol_notif_dead_name    = true,
	},
};

__startup_func
static void
ipc_policy_update_from_tunables(void)
{
	if (!service_port_defense_enabled) {
		ipc_policy_array[IOT_SERVICE_PORT].pol_movability =
		    IPC_MOVE_POLICY_ALWAYS;
	}
}
STARTUP(TUNABLES, STARTUP_RANK_LAST, ipc_policy_update_from_tunables);

/*
 * Ensure new port types that requires a construction entitlement
 * are marked as immovable.
 */
__startup_func
static void
ipc_policy_construct_entitlement_hardening(void)
{
	/* No need to check kobjects because they are always immovable */
	for (ipc_object_type_t i = 0; i < __IKOT_FIRST; i++) {
		/*
		 * IOT_PROVISIONAL_REPLY_PORT is an exception as it used to be
		 * movable. For process opted for enhanced security V2,
		 * kGUARD_EXC_MOVE_PROVISIONAL_REPLY_PORT will be thrown when a
		 * provisional reply port is being moved.
		 */
		if (i == IOT_PROVISIONAL_REPLY_PORT) {
			continue;
		}
		if (ipc_policy_array[i].pol_construct_entitlement) {
			assert(ipc_policy_array[i].pol_movability == IPC_MOVE_POLICY_NEVER);
		}
	}
}
STARTUP(TUNABLES, STARTUP_RANK_LAST, ipc_policy_construct_entitlement_hardening);

__startup_func
void
ipc_kobject_register_startup(ipc_kobject_ops_t ops)
{
	struct ipc_object_policy *pol = &ipc_policy_array[ops->iko_op_type];

	if (pol->pol_name) {
		panic("trying to register kobject(%d) twice", ops->iko_op_type);
	}

	/*
	 * Always make sure kobject ports have immovable receive rights.
	 *
	 * They use the ip_kobject field of the ipc_port structure,
	 * which is unioned with ip_imp_task.
	 *
	 * Thus, general use of a kobject port as a receive right can
	 * cause type confusion in the importance code.
	 */
	ipc_release_assert(pol->pol_movability == IPC_MOVE_POLICY_NEVER);
	if (ops->iko_op_no_senders) {
		pol->pol_notif_no_senders = true;
	}

	pol->pol_name               = ops->iko_op_name;
	pol->pol_kobject_stable     = ops->iko_op_stable;
	pol->pol_kobject_permanent  = ops->iko_op_permanent;
	pol->pol_kobject_no_senders = ops->iko_op_no_senders;
	pol->pol_label_free         = ops->iko_op_label_free;
	pol->pol_movable_send       = ops->iko_op_movable_send;
}

__startup_func
static void
ipc_policy_set_defaults(void)
{
	/*
	 * Check that implicit init to 0 picks the right "values"
	 * for all properties.
	 */
	static_assert(IPC_MOVE_POLICY_NEVER == 0);

	for (uint32_t i = 0; i < IOT_UNKNOWN; i++) {
		struct ipc_object_policy *pol = &ipc_policy_array[i];

		if (!pol->pol_kobject_no_senders) {
			pol->pol_kobject_no_senders = no_kobject_no_senders;
		}
		if (!pol->pol_label_free) {
			pol->pol_label_free = no_label_free;
		}
	}
}
STARTUP(MACH_IPC, STARTUP_RANK_LAST, ipc_policy_set_defaults);

#pragma mark exception port policy

bool
ipc_is_valid_exception_port(
	task_t task,
	ipc_port_t port)
{
	if (task == TASK_NULL && is_ux_handler_port(port)) {
		return true;
	}

	if (ip_is_exception_port(port)) {
		return true;
	}

	/*
	 * rdar://77996387
	 * Avoid exposing immovable ports send rights (kobjects) to `get_exception_ports`,
	 * but exception ports to still be set.
	 */
	if (!ipc_can_stash_naked_send(port)) {
		return false;
	}

	if (ip_is_immovable_receive(port)) {
		/*
		 * rdar://153108740
		 * Temporarily allow service ports until telemetry is clean.
		 */
		if (ip_type(port) == IOT_SERVICE_PORT) {
			return true;
		}
		return false;
	}

	return true;
}
